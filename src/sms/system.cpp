#include "sms/system.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace tessera::sms {
namespace {

constexpr std::uint32_t kSmsStateMagic = 0x54534D53U; // "TSMS"
constexpr std::uint32_t kSmsStateVersion = 1U;
constexpr std::uint32_t kSmsStateEnd = 0x454E4421U;

std::uint32_t crc32(std::span<const std::uint8_t> data) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (std::uint8_t b : data) {
    crc ^= b;
    for (int i = 0; i < 8; ++i)
      crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
  }
  return ~crc;
}

} // namespace

System::System(std::vector<std::uint8_t> rom, ConsoleModel model, bool pal,
               bool japanese, SmsMapper mapper)
    : rom_(std::move(rom)), model_(model), pal_(pal), japanese_(japanese),
      mapper_(model == ConsoleModel::sg1000 && rom_.size() <= 0xC000U
                  ? SmsMapper::plain
                  : mapper),
      rom_crc32_(crc32(rom_)), vdp_(model), z80_(*this) {
  reset();
}

void System::reset() {
  ram_.fill(0);
  vdp_.reset();
  z80_.reset();
  psg_.reset();
  ym2413_.reset();
  sega_regs_ = {0, 0, 1, 2};
  cm_banks_ = {0, 1, 2};
  korean_bank_ = 2;
  mem_ctrl_ = 0;
  io_ctrl_ = 0xFF;
  gg_stereo_ = 0xFF;
  prev_pause_ = false;
  frame_ = 0;
  master_ = 0;
  audio_out_.clear();
}

void System::set_pad(unsigned port, std::uint16_t pressed) noexcept {
  if (port >= 2)
    return;
  pads_[port] = pressed;
  if (port == 0 && model_ == ConsoleModel::sms) {
    const bool pause_now = (pressed & kSmsPauseOrStart) != 0;
    z80_.set_nmi(pause_now);
    prev_pause_ = pause_now;
  }
}

void System::set_audio_mute(std::uint32_t mask) noexcept {
  audio_mute_ = mask & kAudioMuteBits;
  const std::uint16_t fm_mask = static_cast<std::uint16_t>(
      (audio_mute_ & 0x1FFU) | (((audio_mute_ >> 13) & 0x1FU) << 9));
  ym2413_.set_mute(fm_mask);
  psg_.set_mute(static_cast<std::uint8_t>((audio_mute_ >> 9) & 0x0FU));
}

std::uint8_t System::read_rom_mapped(std::uint16_t addr) const noexcept {
  if (rom_.empty())
    return 0xFF;
  if (mapper_ == SmsMapper::plain) {
    return addr < rom_.size() ? rom_[addr] : 0xFF;
  }
  if (mapper_ == SmsMapper::sega) {
    if (addr < 0x0400U)
      return rom_[addr % rom_.size()];
    const std::size_t slot = addr >> 14; // 0, 1, 2
    const std::size_t bank = sega_regs_[slot + 1];
    const std::size_t off = bank * 0x4000U + (addr & 0x3FFFU);
    return rom_[off % rom_.size()];
  }
  if (mapper_ == SmsMapper::codemasters) {
    const std::size_t slot = addr >> 14;
    const std::size_t bank = cm_banks_[slot];
    const std::size_t off = bank * 0x4000U + (addr & 0x3FFFU);
    return rom_[off % rom_.size()];
  }
  // Korean mapper
  if (addr < 0x8000U)
    return rom_[addr % rom_.size()];
  const std::size_t off =
      std::size_t{korean_bank_} * 0x4000U + (addr & 0x3FFFU);
  return rom_[off % rom_.size()];
}

void System::write_mapper(std::uint16_t addr, std::uint8_t value) noexcept {
  if (mapper_ == SmsMapper::sega && addr >= 0xFFFCU) {
    sega_regs_[addr - 0xFFFCU] = value;
  } else if (mapper_ == SmsMapper::codemasters) {
    if (addr == 0x0000U)
      cm_banks_[0] = value;
    else if (addr == 0x4000U)
      cm_banks_[1] = value;
    else if (addr == 0x8000U)
      cm_banks_[2] = value;
  } else if (mapper_ == SmsMapper::korean && addr == 0xA000U) {
    korean_bank_ = value;
  }
}

std::uint8_t System::read(std::uint16_t addr, unsigned &wait) {
  wait = 0;
  if (addr < 0xC000U) {
    if (mapper_ == SmsMapper::sega && addr >= 0x8000U &&
        (sega_regs_[0] & 0x08U) != 0) {
      const std::size_t bank_off = (sega_regs_[0] & 0x04U) ? 0x4000U : 0x0000U;
      return sram_[bank_off + (addr & 0x3FFFU)];
    }
    return read_rom_mapped(addr);
  }
  if (model_ == ConsoleModel::sg1000)
    return ram_[addr & 0x03FFU];
  return ram_[addr & 0x1FFFU];
}

void System::write(std::uint16_t addr, std::uint8_t value, unsigned &wait) {
  wait = 0;
  if (addr < 0xC000U) {
    if (mapper_ == SmsMapper::sega && addr >= 0x8000U &&
        (sega_regs_[0] & 0x08U) != 0) {
      const std::size_t bank_off = (sega_regs_[0] & 0x04U) ? 0x4000U : 0x0000U;
      sram_[bank_off + (addr & 0x3FFFU)] = value;
      sram_dirty_ = true;
      return;
    }
    write_mapper(addr, value);
    return;
  }
  if (model_ == ConsoleModel::sg1000) {
    ram_[addr & 0x03FFU] = value;
    return;
  }
  ram_[addr & 0x1FFFU] = value;
  write_mapper(addr, value);
}

std::uint8_t System::in(std::uint16_t port) {
  const std::uint8_t p = static_cast<std::uint8_t>(port & 0xFFU);
  if (model_ == ConsoleModel::game_gear && p == 0x00U) {
    std::uint8_t v = 0xFFU;
    if ((pads_[0] & kSmsPauseOrStart) != 0)
      v &= ~0x80U;
    if (!japanese_)
      v |= 0x40U;
    else
      v &= ~0x40U;
    return v;
  }
  if (p == 0x7EU)
    return vdp_.v_counter();
  if (p == 0x7FU)
    return vdp_.h_counter();
  if (p == 0xBEU)
    return vdp_.read_data();
  if (p == 0xBFU)
    return vdp_.read_control();
  if (p == 0xF2U && model_ == ConsoleModel::sms)
    return ym2413_.read_detect();

  if (p == 0xDCU || p == 0xC0U) {
    std::uint8_t v = 0xFFU;
    if (pads_[0] & kSmsUp)
      v &= ~0x01U;
    if (pads_[0] & kSmsDown)
      v &= ~0x02U;
    if (pads_[0] & kSmsLeft)
      v &= ~0x04U;
    if (pads_[0] & kSmsRight)
      v &= ~0x08U;
    if (pads_[0] & kSmsButton1)
      v &= ~0x10U;
    if (pads_[0] & kSmsButton2)
      v &= ~0x20U;
    if (pads_[1] & kSmsUp)
      v &= ~0x40U;
    if (pads_[1] & kSmsDown)
      v &= ~0x80U;
    return v;
  }
  if (p == 0xDDU || p == 0xC1U) {
    std::uint8_t v = 0xFFU;
    if (pads_[1] & kSmsLeft)
      v &= ~0x01U;
    if (pads_[1] & kSmsRight)
      v &= ~0x02U;
    if (pads_[1] & kSmsButton1)
      v &= ~0x04U;
    if (pads_[1] & kSmsButton2)
      v &= ~0x08U;
    return v;
  }
  return 0xFFU;
}

void System::out(std::uint16_t port, std::uint8_t value) {
  const std::uint8_t p = static_cast<std::uint8_t>(port & 0xFFU);
  if (model_ == ConsoleModel::game_gear && p == 0x06U) {
    gg_stereo_ = value;
    return;
  }
  if (p == 0x3EU) {
    mem_ctrl_ = value;
  } else if (p == 0x3FU) {
    io_ctrl_ = value;
  } else if (p == 0x7EU || p == 0x7FU) {
    psg_.write(value, master_);
  } else if (p == 0xBEU) {
    vdp_.write_data(value);
  } else if (p == 0xBFU) {
    vdp_.write_control(value);
  } else if (p == 0xF0U && model_ == ConsoleModel::sms) {
    ym2413_.write_addr(value);
  } else if (p == 0xF1U && model_ == ConsoleModel::sms) {
    ym2413_.write_data(value);
  } else if (p == 0xF2U && model_ == ConsoleModel::sms) {
    ym2413_.write_detect(value);
  }
}

void System::run_frame() {
  const int total_lines = pal_ ? 313 : 262;
  const int z80_cycles_per_line = 228;
  const int samples_per_frame = pal_ ? (48000 / 50) : (48000 / 60);

  for (int line = 0; line < total_lines; ++line) {
    const std::uint8_t vc = line <= 0xDA ? static_cast<std::uint8_t>(line)
                                         : static_cast<std::uint8_t>(line - 6);
    vdp_.set_counters(vc, 0);
    if (line < 192)
      vdp_.render_line(line);
    if (line == 192)
      vdp_.end_active_display();

    z80_.set_int(vdp_.irq_pending());
    for (int t = 0; t < z80_cycles_per_line; ++t)
      z80_.tick();
    master_ += static_cast<std::uint64_t>(z80_cycles_per_line) * 15ULL;
  }

  psg_.run_to(master_);
  audio_out_.clear();
  audio_out_.reserve(static_cast<std::size_t>(samples_per_frame) * 2U);
  for (int i = 0; i < samples_per_frame; ++i) {
    std::int32_t l = 0;
    std::int32_t r = 0;
    if (model_ == ConsoleModel::sms)
      ym2413_.step_sample(l, r);
    auto clamp16 = [](std::int32_t v) -> std::int16_t {
      return static_cast<std::int16_t>(std::clamp(v, -32768, 32767));
    };
    audio_out_.push_back(clamp16(l));
    audio_out_.push_back(clamp16(r));
  }
  psg_.clear_samples();
  ++frame_;
}

bool System::load_srm(std::span<const std::uint8_t> file) {
  if (file.size() > sram_.size())
    return false;
  sram_.fill(0xFF);
  std::copy(file.begin(), file.end(), sram_.begin());
  sram_dirty_ = false;
  return true;
}

std::vector<std::uint8_t> System::to_srm() const {
  return std::vector<std::uint8_t>(sram_.begin(), sram_.end());
}

bool System::save_srm_atomic(const std::string &path) {
  const std::string tmp = path + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out)
      return false;
    out.write(reinterpret_cast<const char *>(sram_.data()),
              static_cast<std::streamsize>(sram_.size()));
    out.flush();
    if (!out) {
      out.close();
      std::error_code ignored;
      std::filesystem::remove(tmp, ignored);
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return false;
  }
  sram_dirty_ = false;
  return true;
}

std::vector<std::uint8_t> System::save_state() const {
  std::vector<std::uint8_t> out;
  md::StateWriter w(out);
  w.u32(kSmsStateMagic);
  w.u32(kSmsStateVersion);
  w.u32(rom_crc32_);
  w.u8(static_cast<std::uint8_t>(model_));
  w.u8(static_cast<std::uint8_t>(mapper_));
  w.u64(frame_);
  w.u64(master_);
  w.bytes(ram_);
  w.bytes(sram_);
  w.boolean(sram_dirty_);
  w.array(sega_regs_);
  w.array(cm_banks_);
  w.u8(korean_bank_);
  w.u8(mem_ctrl_);
  w.u8(io_ctrl_);
  w.u8(gg_stereo_);
  w.boolean(prev_pause_);
  vdp_.save(w);
  z80_.save(w);
  psg_.save(w);
  ym2413_.save(w);
  w.u32(kSmsStateEnd);
  return out;
}

bool System::load_state(std::span<const std::uint8_t> state) {
  const auto backup = save_state();
  auto fail = [&]() {
    (void)load_state(backup);
    return false;
  };
  md::StateReader r(state);
  if (r.u32() != kSmsStateMagic || r.u32() != kSmsStateVersion)
    return false;
  if (r.u32() != rom_crc32_)
    return false;
  if (r.u8() != static_cast<std::uint8_t>(model_))
    return false;
  if (r.u8() != static_cast<std::uint8_t>(mapper_))
    return false;
  frame_ = r.u64();
  master_ = r.u64();
  r.bytes(ram_);
  r.bytes(sram_);
  sram_dirty_ = r.boolean();
  r.array(sega_regs_);
  r.array(cm_banks_);
  korean_bank_ = r.u8();
  mem_ctrl_ = r.u8();
  io_ctrl_ = r.u8();
  gg_stereo_ = r.u8();
  prev_pause_ = r.boolean();
  vdp_.load(r);
  z80_.load(r);
  psg_.load(r);
  ym2413_.load(r);
  if (r.u32() != kSmsStateEnd || !r.finished())
    return fail();
  return true;
}

} // namespace tessera::sms
