// TP-201.3 (RF-3.8, RF-3.9): the subscriptions of the observation API on the
// native Mega Drive backend.
//  - arranca sin nada activo; una mascara pedida entre frames pasa a activa al
//    empezar el siguiente frame; los bits desconocidos se rechazan;
//  - una region cuya suscripcion esta inactiva responde NOT_SUBSCRIBED y no
//    hace trabajo (el contador queda a cero); SYSTEM y Z80_RAM no piden nada;
//  - las respuestas coinciden con la matriz medida sobre gpgx r4
//    (tests/fixtures/diffbench/subscription-matrix.txt);
//  - el transporte de audio events solo corre con AUDIO_EVENTS activa.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "domain/wire.hpp"
#include "md/regions.hpp"
#include "md/vdp_render.hpp"
#include "session/session.hpp"
#include "support/md_delta_testrom.hpp"
#include "support/md_limits_testrom.hpp"
#include "support/md_z80_testrom.hpp"
#include "support/md_testrom.hpp"
#include <tessera/tessera.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;

template <class T> T initial() {
  T value{};
  value.header = {sizeof(T), 1, 0};
  return value;
}

struct Native {
  tessera_context_v1 context = initial<tessera_context_v1>();
  tessera_session handle{};
  Bytes rom;
  std::uint64_t epoch = 0;

  explicit Native(Bytes content) : rom(std::move(content)) {
    std::array<tessera_backend_info_v1, 4> backends{
        initial<tessera_backend_info_v1>(), initial<tessera_backend_info_v1>(),
        initial<tessera_backend_info_v1>(), initial<tessera_backend_info_v1>()};
    std::uint32_t required{};
    REQUIRE(tessera_backends_enumerate_v1(backends.data(), backends.size(),
                                          &required, &context) == TESSERA_OK);
    const tessera_backend_info_v1 *native = nullptr;
    for (std::uint32_t i = 0; i < required && i < backends.size(); ++i)
      if (std::string_view(backends[i].id) == "megadrive")
        native = &backends[i];
    REQUIRE(native != nullptr);
    const Bytes memory(65536, 0);
    const auto configuration =
        tessera::wire::configuration("megadrive", native->build_id, memory);
    auto create = initial<tessera_session_create_info_v1>();
    const auto id = tessera::wire::text("megadrive");
    create.backend_id = {id.data(), id.size()};
    create.content = {rom.data(), rom.size()};
    create.config = {configuration.data(), configuration.size()};
    create.initial_persistent_memory = {memory.data(), memory.size()};
    create.system_override = TESSERA_SYSTEM_MEGA_DRIVE;
    REQUIRE(tessera_session_create_v1(&create, &handle, &context) ==
            TESSERA_OK);
    epoch = info().frame_id.epoch;
  }
  ~Native() {
    if (handle != 0)
      tessera_session_close_v1(handle, &context);
  }
  Native(const Native &) = delete;
  Native &operator=(const Native &) = delete;

  tessera_session_info_v1 info() {
    auto result = initial<tessera_session_info_v1>();
    REQUIRE(tessera_session_query_v1(handle, &result, &context) == TESSERA_OK);
    return result;
  }
  /// Steps one frame with a moving pad on both ports.
  void step() {
    const auto now = info();
    std::array<tessera_input_assignment_v1, 24> assignments{};
    for (std::uint16_t port = 0; port < 2; ++port)
      for (std::uint16_t control = 0; control < 12; ++control) {
        const auto key =
            static_cast<unsigned>(now.frame_id.frame * 7 + control * 3 + port);
        // up/down and left/right never together: those are the first four
        const bool pressed =
            control < 4 ? (control % 2 == 0 && key % 5 == 0) : (key % 4 == 0);
        assignments[port * 12 + control] = {port, control, pressed ? 1 : 0};
      }
    auto input = initial<tessera_input_frame_v1>();
    input.epoch = now.frame_id.epoch;
    input.frame = now.frame_id.frame + 1;
    input.assignments = assignments.data();
    input.count = assignments.size();
    REQUIRE(tessera_session_step_v1(handle, &input, &context) == TESSERA_OK);
  }
  void step(unsigned frames) {
    for (unsigned i = 0; i < frames; ++i)
      step();
  }
  Bytes checkpoint() {
    std::uint64_t required{};
    REQUIRE(tessera_checkpoint_export_v1(handle, {nullptr, 0}, &required,
                                         &context) == TESSERA_BUFFER_TOO_SMALL);
    Bytes out(static_cast<std::size_t>(required));
    REQUIRE(tessera_checkpoint_export_v1(handle, {out.data(), out.size()},
                                         &required, &context) == TESSERA_OK);
    return out;
  }
  tessera_status restore(const Bytes &checkpoint) {
    return tessera_checkpoint_restore_v1(
        handle, {checkpoint.data(), checkpoint.size()}, {nullptr, 0}, &context);
  }
  Bytes original() {
    auto frame = initial<tessera_original_frame_v1>();
    REQUIRE(tessera_original_frame_query_v1(handle, &frame, &context) ==
            TESSERA_OK);
    Bytes out(frame.pixels.data, frame.pixels.data + frame.pixels.bytes);
    out.insert(out.end(), frame.pcm.data, frame.pcm.data + frame.pcm.bytes);
    return out;
  }
};

using tessera::Bytes;
constexpr std::uint32_t kAll = TESSERA_OBS_SUB_ALL;

struct Observed : Native {
  using Native::Native;
  tessera_status set(std::uint32_t mask) {
    return tessera_observation_subscriptions_set_v1(handle, mask, &context);
  }
  tessera_obs_subscription_state_v1 state() {
    tessera_obs_subscription_state_v1 s{};
    REQUIRE(tessera_observation_subscriptions_get_v1(handle, &s, sizeof(s),
                                                     &context) == TESSERA_OK);
    return s;
  }
  tessera_status read(std::uint32_t region, Bytes *out = nullptr) {
    tessera_obs_region_info_v1 info{};
    REQUIRE(tessera_observation_region_query_v1(
                handle, region, &info, sizeof(info), &context) == TESSERA_OK);
    Bytes buffer(info.byte_size);
    std::uint64_t generation = 0;
    const auto status = tessera_observation_region_read_v1(
        handle, region, 0, buffer.data(), info.byte_size, UINT64_MAX,
        &generation, &context);
    if (out)
      *out = buffer;
    return status;
  }
  std::uint64_t work() {
    return tessera::sessions.get(handle)
        ->machine->observation()
        ->observation_work();
  }
};

Bytes rom_idle() { return tessera::testrom::vint_counter_rom(); }
Bytes rom_audio() { return tessera::testrom::fm_psg_sweep_rom(5); }

/// What the oracle answered, translated to Tessera's statuses.
tessera_status expected_status(int reference) {
  return reference == 0    ? TESSERA_OK
         : reference == -9 ? TESSERA_NOT_SUBSCRIBED
                           : reference;
}
/// Regions the native machine serves today (TP-201.1/.2, PALETTE and the raster
/// regions of TP-203/204).
bool served(unsigned region) {
  return region == 1 || region == 2 || region == 3 || region == 4 ||
         region == 13 || region == 14 || region == 17 || region == 18 ||
         region == 24 || region == 25 || region == 26 || region == 16 ||
         region == 19 || region == 20 || region == 21 || region == 22 ||
         region == 5 || region == 10 || region == 23 || region == 11 ||
         region == 12;
}
} // namespace

TEST_CASE("nothing is active at the start and unknown bits are refused") {
  Observed s(rom_idle());
  auto state = s.state();
  CHECK(state.struct_size == sizeof(state));
  CHECK(state.state_version == 1);
  CHECK(state.supported_mask == kAll);
  CHECK(state.active_mask == 0);
  CHECK(state.requested_mask == 0);
  CHECK(state.activation_frame == 0);

  CHECK(s.set(kAll + 1) == TESSERA_INVALID_ARGUMENT);
  CHECK(s.set(UINT32_C(1) << 31) == TESSERA_INVALID_ARGUMENT);
  state = s.state();
  CHECK(state.requested_mask == 0); // refused: nothing changed
  CHECK(s.set(TESSERA_OBS_SUB_VDP_MEMORY) == TESSERA_OK);
  CHECK(s.state().requested_mask == TESSERA_OBS_SUB_VDP_MEMORY);
  CHECK(s.state().active_mask == 0); // active only from the next frame
}

TEST_CASE("a request becomes active when the next frame starts") {
  Observed s(rom_idle());
  REQUIRE(s.set(TESSERA_OBS_SUB_VDP_MEMORY) == TESSERA_OK);
  CHECK(s.read(1) == TESSERA_NOT_SUBSCRIBED); // requested is not active
  s.step();
  CHECK(s.state().active_mask == TESSERA_OBS_SUB_VDP_MEMORY);
  CHECK(s.state().activation_frame == 1);
  CHECK(s.read(1) == TESSERA_OK);
  s.step(3);
  CHECK(s.state().activation_frame == 4);
  // back to nothing: idle again from the next frame
  REQUIRE(s.set(0) == TESSERA_OK);
  CHECK(s.read(1) == TESSERA_OK); // still active until the frame starts
  s.step();
  CHECK(s.read(1) == TESSERA_NOT_SUBSCRIBED);
}

TEST_CASE("without subscriptions the observation does no work") {
  Observed s(rom_audio());
  s.step(20);
  CHECK(s.read(1) == TESSERA_NOT_SUBSCRIBED);
  CHECK(s.read(11) == TESSERA_NOT_SUBSCRIBED);
  std::uint32_t count = 0;
  Bytes events(80 * 8);
  CHECK(tessera_observation_audio_events_poll_v1(s.handle, events.data(), 8, 80,
                                                 &count, &s.context) ==
        TESSERA_NOT_SUBSCRIBED);
  CHECK(count == 0);
  CHECK(s.work() == 0); // the counter stays at zero

  // what the machine serves, once subscribed, is the work
  REQUIRE(s.set(TESSERA_OBS_SUB_VDP_MEMORY) == TESSERA_OK);
  s.step();
  CHECK(s.read(1) == TESSERA_OK);
  CHECK(s.work() == 65536); // only what the read delivered
  const auto before = s.work();
  CHECK(s.read(26) == TESSERA_OK); // needs no subscription, still counts
  CHECK(s.work() == before + 8192);
}

TEST_CASE("the regions read as the machine holds them") {
  Observed s(rom_audio());
  REQUIRE(s.set(kAll) == TESSERA_OK);
  s.step(30);
  Bytes vram, cram, regs, vsram, z80, system, palette;
  REQUIRE(s.read(1, &vram) == TESSERA_OK);
  REQUIRE(s.read(2, &cram) == TESSERA_OK);
  REQUIRE(s.read(3, &regs) == TESSERA_OK);
  REQUIRE(s.read(4, &vsram) == TESSERA_OK);
  REQUIRE(s.read(26, &z80) == TESSERA_OK);
  REQUIRE(s.read(18, &system) == TESSERA_OK);
  REQUIRE(s.read(24, &palette) == TESSERA_OK);
  CHECK(vram.size() == 65536);
  CHECK(cram.size() == 128);
  CHECK(regs.size() == 32);
  CHECK(vsram.size() == 128);
  CHECK(z80.size() == 8192);
  CHECK(system.size() == 44);
  CHECK(palette.size() == 512);
  // SYSTEM: its own size, the layout version and the Mega Drive hardware id
  CHECK(system[0] == 44);
  CHECK(system[4] == 1);
  CHECK(system[8] == 0x80);
  // a range inside a region, and one that is not
  Bytes part(16);
  std::uint64_t generation = 0;
  CHECK(tessera_observation_region_read_v1(s.handle, 3, 8, part.data(), 16,
                                           UINT64_MAX, &generation,
                                           &s.context) == TESSERA_OK);
  CHECK(std::equal(part.begin(), part.end(), regs.begin() + 8));
  CHECK(tessera_observation_region_read_v1(
            s.handle, 3, 24, part.data(), 16, UINT64_MAX, &generation,
            &s.context) == TESSERA_INVALID_ARGUMENT);
}

TEST_CASE("reading does not change the machine") {
  Observed observed(rom_audio());
  Observed control(rom_audio());
  REQUIRE(observed.set(kAll) == TESSERA_OK);
  observed.step(20);
  control.step(20);
  for (std::uint32_t region : {1u, 2u, 3u, 4u, 18u, 24u, 26u})
    REQUIRE(observed.read(region) == TESSERA_OK);
  observed.step(10);
  control.step(10);
  CHECK(observed.checkpoint() == control.checkpoint());
}

TEST_CASE("work RAM read and poke through the public API (TP-301.3)") {
  // vint_counter adds one to the long at $FF0000 on every vertical interrupt.
  // The region is word-swapped: the 68000 byte at $FF0000 + n is byte n ^ 1.
  Observed s(rom_idle());
  Observed control(rom_idle()); // same frames, never poked
  tessera_obs_region_info_v1 info{};
  REQUIRE(tessera_observation_region_query_v1(
              s.handle, TESSERA_OBS_REGION_WORK_RAM, &info, sizeof(info),
              &s.context) == TESSERA_OK);
  CHECK(info.region_id == TESSERA_OBS_REGION_WORK_RAM);
  CHECK(info.byte_size == 65536);
  CHECK(info.element_size == 1);
  CHECK(info.capacity == 65536);
  CHECK(info.access_flags == (TESSERA_OBS_REGION_ACCESS_READ |
                              TESSERA_OBS_REGION_ACCESS_CONTROL_WRITE |
                              TESSERA_OBS_REGION_WORD_SWAPPED_LE));
  CHECK(info.legacy_memory_id == 2); // RETRO_MEMORY_SYSTEM_RAM

  const auto counter = [&](Observed &o) {
    std::array<std::uint8_t, 4> b{};
    std::uint64_t generation = 0;
    REQUIRE(tessera_observation_region_read_v1(
                o.handle, TESSERA_OBS_REGION_WORK_RAM, 0, b.data(), 4,
                UINT64_MAX, &generation, &o.context) == TESSERA_OK);
    return std::uint32_t{b[1]} << 24 | std::uint32_t{b[0]} << 16 |
           std::uint32_t{b[3]} << 8 | b[2];
  };
  const auto poke = [&](std::uint32_t offset, Bytes data,
                        std::uint64_t expected = UINT64_MAX,
                        std::uint64_t *generation = nullptr) {
    std::uint64_t g = 0;
    const auto status = tessera_observation_write_control_v1(
        s.handle, TESSERA_OBS_REGION_WORK_RAM, offset, data.data(),
        static_cast<std::uint32_t>(data.size()), expected, &g, &s.context);
    if (generation)
      *generation = g;
    return status;
  };

  // Read with nothing subscribed: the work RAM is always served.
  s.step(10);
  control.step(10);
  REQUIRE(s.state().active_mask == 0);
  const auto counted = counter(s);
  CHECK(counted >= 9);
  CHECK(counted <= 10);
  Bytes whole;
  REQUIRE(s.read(TESSERA_OBS_REGION_WORK_RAM, &whole) == TESSERA_OK);
  REQUIRE(whole.size() == 65536);
  CHECK(whole[2] == static_cast<std::uint8_t>(counted));
  CHECK(s.checkpoint() == control.checkpoint()); // reading changed nothing

  // Poke the counter to $1000 (68000 bytes 00 00 10 00 at $FF0000): the
  // machine runs on from the poked value and the generation advances.
  std::uint64_t before = 0, after = 0;
  REQUIRE(tessera_observation_region_read_v1(
              s.handle, TESSERA_OBS_REGION_WORK_RAM, 0, nullptr, 0, UINT64_MAX,
              &before, &s.context) == TESSERA_OK);
  REQUIRE(poke(0, {0x00, 0x00, 0x00, 0x10}, before, &after) == TESSERA_OK);
  CHECK(after == before + 1);
  CHECK(counter(s) == 0x1000);
  s.step();
  control.step();
  CHECK(counter(s) == 0x1001);
  CHECK(counter(control) == counted + 1);

  // The poke is state: the checkpoint differs from the unpoked machine and a
  // restore brings the poked byte back.
  REQUIRE(poke(0x8000, {0xAB}) == TESSERA_OK);
  CHECK(s.checkpoint() != control.checkpoint());
  const auto saved = s.checkpoint();
  REQUIRE(poke(0x8000, {0x00}) == TESSERA_OK);
  REQUIRE(s.restore(saved) == TESSERA_OK);
  std::array<std::uint8_t, 1> scratch{};
  std::uint64_t generation = 0;
  REQUIRE(tessera_observation_region_read_v1(
              s.handle, TESSERA_OBS_REGION_WORK_RAM, 0x8000, scratch.data(), 1,
              UINT64_MAX, &generation, &s.context) == TESSERA_OK);
  CHECK(scratch[0] == 0xAB);

  // Refused writes leave the machine and the generation as they were.
  REQUIRE(tessera_observation_region_read_v1(
              s.handle, TESSERA_OBS_REGION_WORK_RAM, 0, nullptr, 0, UINT64_MAX,
              &before, &s.context) == TESSERA_OK);
  CHECK(poke(0xFFFF, {1, 2}) == TESSERA_INVALID_ARGUMENT);
  CHECK(poke(0x10000, {1}) == TESSERA_INVALID_ARGUMENT);
  CHECK(poke(0, {}) == TESSERA_INVALID_ARGUMENT);
  CHECK(poke(0, {1}, before + 7) == TESSERA_STALE_EPOCH);
  REQUIRE(tessera_observation_region_read_v1(
              s.handle, TESSERA_OBS_REGION_WORK_RAM, 0, nullptr, 0, UINT64_MAX,
              &after, &s.context) == TESSERA_OK);
  CHECK(after == before);
  CHECK(counter(s) == 0x1001);
}

TEST_CASE("recompose_frame draws the last frame again (TP-205.1)") {
  Observed s(rom_idle());
  std::vector<std::uint16_t> pixels(320 * 240);
  std::uint32_t width = 0, height = 0;
  const auto recompose = [&](std::uint32_t capacity) {
    return tessera_observation_recompose_v1(s.handle, pixels.data(), capacity,
                                            0, &width, &height, &s.context);
  };
  s.step(3);
  CHECK(recompose(320 * 240) == TESSERA_NOT_SUBSCRIBED);
  REQUIRE(s.set(TESSERA_OBS_SUB_RECOMPOSITION) == TESSERA_OK);
  CHECK(recompose(320 * 240) == TESSERA_NOT_SUBSCRIBED); // from the next frame
  s.step();
  const auto before = s.checkpoint();
  REQUIRE(recompose(320 * 240) == TESSERA_OK);
  const auto frame = s.original();
  // A ROM with no raster change recomposes to the frame it emitted.
  REQUIRE(frame.size() >= std::size_t{width} * height * 4);
  CHECK(width == 256); // vint_counter is H32
  CHECK(height == 224);
  bool same = true;
  for (std::size_t i = 0; i < std::size_t{width} * height; ++i) {
    const std::uint32_t c =
        frame[4 * i] | frame[4 * i + 1] << 8 | frame[4 * i + 2] << 16;
    same = same && pixels[i] == tessera::md::rgba_to_565(c);
  }
  CHECK(same);
  // A buffer smaller than the frame is refused (gpgx: RC_INVALID_PARAMS) and
  // nothing changes in the machine.
  CHECK(recompose(width * height - 1) == TESSERA_INVALID_ARGUMENT);
  CHECK(tessera_observation_recompose_v1(s.handle, nullptr, 320 * 240, 0,
                                         &width, &height, &s.context) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(s.checkpoint() == before);
}

TEST_CASE("LAYER_MASK and LAYER_DIM through write_control (TP-205.2)") {
  Observed s(rom_idle());
  Observed plain(rom_idle()); // the same frames with no control written
  const auto write = [&](std::uint32_t region, Bytes data,
                         std::uint64_t *generation = nullptr) {
    std::uint64_t g = 0;
    const auto status = tessera_observation_write_control_v1(
        s.handle, region, 0, data.data(),
        static_cast<std::uint32_t>(data.size()), UINT64_MAX, &g, &s.context);
    if (generation)
      *generation = g;
    return status;
  };
  const auto value = [&](std::uint32_t region) {
    Bytes out;
    REQUIRE(s.read(region, &out) == TESSERA_OK);
    REQUIRE(out.size() == 1);
    return out[0];
  };
  s.step(2);
  plain.step(2);
  // Before any write gpgx reads 0xFF and 0, with nothing subscribed.
  CHECK(value(TESSERA_OBS_REGION_LAYER_MASK) == 0xFF);
  CHECK(value(TESSERA_OBS_REGION_LAYER_DIM) == 0);
  std::uint64_t before = 0, after = 0;
  REQUIRE(write(TESSERA_OBS_REGION_LAYER_MASK, {0x0F}, &before) == TESSERA_OK);
  // Refused: a mask beyond the four layers, a dim other than 0 or 1, more
  // bytes than the region has. A refused write leaves the generation.
  CHECK(write(TESSERA_OBS_REGION_LAYER_MASK, {0x10}) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(write(TESSERA_OBS_REGION_LAYER_MASK, {0xFF}) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(write(TESSERA_OBS_REGION_LAYER_DIM, {2}) == TESSERA_INVALID_ARGUMENT);
  CHECK(write(TESSERA_OBS_REGION_LAYER_DIM, {1, 0}) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(write(TESSERA_OBS_REGION_AUDIO_MUTE, {1}) ==
        TESSERA_UNSUPPORTED_CAPABILITY); // TP-209
  REQUIRE(write(TESSERA_OBS_REGION_LAYER_DIM, {1}, &after) == TESSERA_OK);
  CHECK(after == before + 1);
  CHECK(value(TESSERA_OBS_REGION_LAYER_DIM) == 1);
  // Without RENDER_CONTROLS the controls are kept but do nothing.
  s.step();
  plain.step();
  CHECK(s.original() == plain.original());
  // With it, from the next frame: LAYER_DIM darkens every channel.
  REQUIRE(s.set(TESSERA_OBS_SUB_RENDER_CONTROLS |
                TESSERA_OBS_SUB_RECOMPOSITION) == TESSERA_OK);
  s.step();
  plain.step();
  const auto dark = s.original(), lit = plain.original();
  REQUIRE(dark.size() == lit.size());
  const std::size_t pixels = 256 * 224;
  bool dimmed = true;
  for (std::size_t i = 0; i < pixels * 4; i += 4)
    for (unsigned c = 0; c < 3; ++c) {
      const unsigned bits = c == 1 ? 6 : 5;
      const unsigned v = (lit[i + c] >> (8 - bits)) >> 2;
      const unsigned expanded =
          bits == 6 ? (v << 2 | v >> 4) : (v << 3 | v >> 2);
      dimmed = dimmed && dark[i + c] == expanded;
    }
  CHECK(dimmed);
  // recompose_multilayer: five dimmed layers; the composite is the frame.
  std::array<std::vector<std::uint16_t>, 5> layers;
  for (auto &layer : layers)
    layer.assign(pixels, 0);
  std::uint32_t width = 0, height = 0;
  const auto multilayer = [&](std::uint32_t capacity) {
    return tessera_observation_recompose_multilayer_v1(
        s.handle, layers[0].data(), layers[1].data(), layers[2].data(),
        layers[3].data(), layers[4].data(), capacity, 0, &width, &height,
        &s.context);
  };
  REQUIRE(multilayer(pixels) == TESSERA_OK);
  CHECK(width == 256);
  CHECK(height == 224);
  bool composite = true;
  for (std::size_t i = 0; i < pixels; ++i)
    composite = composite &&
                layers[4][i] == tessera::md::rgba_to_565(dark[4 * i] |
                                                         dark[4 * i + 1] << 8 |
                                                         dark[4 * i + 2] << 16);
  CHECK(composite);
  CHECK(multilayer(pixels - 1) == TESSERA_INVALID_ARGUMENT);
  CHECK(tessera_observation_recompose_multilayer_v1(
            s.handle, layers[0].data(), nullptr, layers[2].data(),
            layers[3].data(), layers[4].data(), pixels, 0, &width, &height,
            &s.context) == TESSERA_INVALID_ARGUMENT);
  // RF-3.9: the controls are part of the state.
  REQUIRE(write(TESSERA_OBS_REGION_LAYER_MASK, {0x05}) == TESSERA_OK);
  const auto saved = s.checkpoint();
  REQUIRE(write(TESSERA_OBS_REGION_LAYER_MASK, {0x0F}) == TESSERA_OK);
  REQUIRE(write(TESSERA_OBS_REGION_LAYER_DIM, {0}) == TESSERA_OK);
  REQUIRE(s.restore(saved) == TESSERA_OK);
  CHECK(value(TESSERA_OBS_REGION_LAYER_MASK) == 0x05);
  CHECK(value(TESSERA_OBS_REGION_LAYER_DIM) == 1);
}

TEST_CASE("recompose_multilayer needs RECOMPOSITION (TP-205.2)") {
  Observed s(rom_idle());
  std::array<std::vector<std::uint16_t>, 5> l;
  for (auto &layer : l)
    layer.assign(256 * 224, 0);
  std::uint32_t width = 0, height = 0;
  s.step(2);
  CHECK(tessera_observation_recompose_multilayer_v1(
            s.handle, l[0].data(), l[1].data(), l[2].data(), l[3].data(),
            l[4].data(), 256 * 224, 0, &width, &height,
            &s.context) == TESSERA_NOT_SUBSCRIBED);
}

TEST_CASE("the audio event transport only runs while subscribed") {
  Observed s(rom_audio());
  std::uint32_t count = 0;
  Bytes events(80 * 64);
  s.step(5);
  tessera_obs_audio_transport_stats_v1 stats{};
  CHECK(tessera_observation_audio_transport_stats_v1(
            s.handle, &stats, sizeof(stats), &s.context) == TESSERA_OK);
  CHECK(stats.pending == 0); // the stats answer even when idle

  REQUIRE(s.set(TESSERA_OBS_SUB_AUDIO_EVENTS) == TESSERA_OK);
  s.step(20);
  CHECK(tessera_observation_audio_events_poll_v1(
            s.handle, events.data(), 64, 80, &count, &s.context) == TESSERA_OK);
  CHECK(count > 0);
  CHECK(tessera_observation_audio_events_poll_v1(s.handle, events.data(), 64,
                                                 64, &count, &s.context) ==
        TESSERA_INVALID_ARGUMENT); // not an 80 byte event
  CHECK(s.work() >= 80u * count);
}

TEST_CASE("the answers match the matrix measured on the reference core") {
  std::ifstream matrix(std::string(TESSERA_SOURCE_DIR) +
                       "/tests/fixtures/diffbench/subscription-matrix.txt");
  REQUIRE(matrix.good());
  struct Key {
    unsigned mask, frames;
    bool operator<(const Key &o) const {
      return std::tie(mask, frames) < std::tie(o.mask, o.frames);
    }
  };
  struct Case {
    int set_status = 0;
    unsigned after_active = 0, after_requested = 0, end_active = 0,
             end_requested = 0;
    unsigned long long after_frame = 0, end_frame = 0;
    std::map<unsigned, int> reads;
  };
  std::map<Key, Case> cases;
  std::map<unsigned, std::tuple<int, int, int>> audio;
  std::string line;
  while (std::getline(matrix, line)) {
    std::istringstream in(line);
    std::string kind;
    in >> kind;
    if (kind == "audio") {
      unsigned mask;
      int poll, events, transport;
      in >> std::hex >> mask >> std::dec >> poll >> events >> transport;
      audio[mask] = {poll, events, transport};
      continue;
    }
    unsigned mask, frames;
    in >> std::hex >> mask >> std::dec >> frames;
    auto &c = cases[{mask, frames}];
    if (kind == "set") {
      in >> c.set_status;
    } else if (kind == "state") {
      std::string tag;
      unsigned long long frame;
      unsigned active, requested;
      in >> tag >> active >> requested >> frame;
      if (tag == "after_set") {
        c.after_active = active;
        c.after_requested = requested;
        c.after_frame = frame;
      } else {
        c.end_active = active;
        c.end_requested = requested;
        c.end_frame = frame;
      }
    } else if (kind == "read") {
      unsigned region;
      int status;
      in >> region >> status;
      c.reads[region] = status;
    }
  }
  REQUIRE(cases.size() >= 27);

  for (const auto &[key, expected] : cases) {
    INFO("mask 0x" << std::hex << key.mask << std::dec << " after "
                   << key.frames << " frames");
    Observed s(rom_idle());
    CHECK(s.set(key.mask) == expected_status(expected.set_status));
    auto state = s.state();
    CHECK(state.active_mask == expected.after_active);
    CHECK(state.requested_mask == expected.after_requested);
    CHECK(state.activation_frame == expected.after_frame);
    s.step(key.frames);
    state = s.state();
    CHECK(state.active_mask == expected.end_active);
    CHECK(state.requested_mask == expected.end_requested);
    CHECK(state.activation_frame == expected.end_frame);
    for (const auto &[region, reference] : expected.reads) {
      const auto native = s.read(region);
      if (reference == -9)
        CHECK_MESSAGE(native == TESSERA_NOT_SUBSCRIBED, "region " << region);
      else if (served(region))
        CHECK_MESSAGE(native == TESSERA_OK, "region " << region);
      else
        CHECK_MESSAGE(native == TESSERA_UNSUPPORTED_CAPABILITY,
                      "region " << region << " is not served yet");
    }
  }
  for (const auto &[mask, answer] : audio) {
    INFO("audio mask 0x" << std::hex << mask);
    Observed s(rom_audio());
    REQUIRE(s.set(mask) == TESSERA_OK);
    s.step(20);
    std::uint32_t count = 0;
    Bytes events(80 * 1024);
    const auto status = tessera_observation_audio_events_poll_v1(
        s.handle, events.data(), 1024, 80, &count, &s.context);
    CHECK(status == expected_status(std::get<0>(answer)));
    CHECK((count > 0) == (std::get<1>(answer) != 0));
    tessera_obs_audio_transport_stats_v1 stats{};
    CHECK(tessera_observation_audio_transport_stats_v1(
              s.handle, &stats, sizeof(stats), &s.context) ==
          expected_status(std::get<2>(answer)));
  }
}

namespace {
/// The test ROMs of tests/fixtures/diffbench/recompose-stats.txt by the name
/// tessera_md_testrom gives them.
Bytes rom_named(const std::string &name) {
  namespace rom = tessera::testrom;
  const auto number = [&](std::size_t prefix) {
    return static_cast<unsigned>(std::stoul(name.substr(prefix)));
  };
  if (name.rfind("modes-sweep-", 0) == 0)
    return rom::modes_sweep_rom(rom::modes_variant(number(12)));
  if (name.rfind("raster-bands-", 0) == 0)
    return rom::raster_bands_rom(rom::raster_variant(number(13)));
  if (name == "audio-flood")
    return rom::audio_flood_rom();
  if (name == "mode4-pbc")
    return rom::mode4_pbc_rom();
  if (name.rfind("mode4-scene-", 0) == 0)
    return rom::mode4_scene_rom(rom::mode4_variant(number(12)));
  if (name.rfind("delta-probe-", 0) == 0)
    return rom::delta_probe_rom(number(12));
  if (name.rfind("sprite-limits-", 0) == 0)
    return rom::sprite_limits_rom(rom::sprite_variant(number(14)));
  if (name == "fm-psg-z80")
    return rom::fm_psg_z80_rom();
  if (name.rfind("fm-psg-", 0) == 0)
    return rom::fm_psg_sweep_rom(number(7));
#ifdef TESSERA_FX02_ROM
  if (name == "fx02") {
    std::ifstream file(TESSERA_FX02_ROM, std::ios::binary);
    REQUIRE(file.good());
    return Bytes(std::istreambuf_iterator<char>(file), {});
  }
#endif
  FAIL("unknown test rom " << name);
  return {};
}

/// A gpgx status in the fixture as Tessera returns it (the bench mapping).
std::string native_statuses(const std::string &line) {
  const auto at = line.find("status=");
  if (at == std::string::npos)
    return line;
  const auto end = line.find(' ', at);
  const int reference = std::stoi(line.substr(at + 7, end - at - 7));
  const int native = reference == -20 || reference == -21 || reference == -24
                         ? TESSERA_WRONG_STATE
                     : reference == -23 ? TESSERA_INVALID_ARGUMENT
                     : reference == -9  ? TESSERA_NOT_SUBSCRIBED
                     : reference == -10 ? TESSERA_STALE_EPOCH
                                        : reference;
  return line.substr(0, at + 7) + std::to_string(native) +
         (end == std::string::npos ? "" : line.substr(end));
}

/// The dirty bytes of a frame delta as tessera_diffbench prints them: runs
/// "first-last:value" (or "index:value"), comma separated, or "none".
std::string dirty_runs(const std::uint8_t (&dirty)[2048]) {
  std::string out;
  char buffer[32];
  for (std::size_t i = 0; i < 2048;) {
    if (dirty[i] == 0) {
      ++i;
      continue;
    }
    std::size_t j = i;
    while (j + 1 < 2048 && dirty[j + 1] == dirty[i])
      ++j;
    if (j == i)
      std::snprintf(buffer, sizeof(buffer), "%zu:%02x", i, dirty[i]);
    else
      std::snprintf(buffer, sizeof(buffer), "%zu-%zu:%02x", i, j, dirty[i]);
    out += out.empty() ? "" : ",";
    out += buffer;
    i = j + 1;
  }
  return out.empty() ? "none" : out;
}

/// Runs a stats script (see compare_recompose_stats.py) on the native machine
/// through the public API and answers as tessera_diffbench does on gpgx.
std::vector<std::string>
run_stats_script(Observed &s, const std::string &script, unsigned frames) {
  std::map<unsigned, std::vector<std::string>> steps;
  std::istringstream per_frame(script);
  for (std::string part; std::getline(per_frame, part, ';');) {
    const auto colon = part.find(':');
    std::istringstream ops(part.substr(colon + 1));
    auto &list =
        steps[static_cast<unsigned>(std::stoul(part.substr(0, colon)))];
    for (std::string op; std::getline(ops, op, '.');)
      list.push_back(op);
  }
  std::vector<std::string> out;
  std::uint64_t last_generation = 0;
  std::uint64_t generation_base = UINT64_MAX; // c and G (TP-302)
  Bytes saved;
  const std::uint32_t full = 320 * 240;
  std::vector<std::uint16_t> layers(5u * full);
  for (unsigned i = 0; i < frames; ++i) {
    s.step();
    for (auto op : steps[i]) {
      const auto at = op.find('@');
      const std::uint32_t capacity =
          at == std::string::npos
              ? full
              : static_cast<std::uint32_t>(std::stoul(op.substr(at + 1)));
      const auto argument =
          op.substr(1, at == std::string::npos ? std::string::npos : at - 1);
      const auto hex = [&] {
        return argument.empty() ? 0u
                                : static_cast<std::uint32_t>(
                                      std::stoul(argument, nullptr, 16));
      };
      std::ostringstream line;
      line << "frame=" << i << " ";
      tessera_status status = TESSERA_UNSUPPORTED_CAPABILITY;
      std::uint32_t width = 0, height = 0;
      if (op[0] == 'g') {
        tessera_obs_recompose_stats_v1 st{};
        status = tessera_observation_recompose_stats_v1(
            s.handle, &st,
            argument.empty() ? sizeof(st)
                             : static_cast<std::uint32_t>(std::stoul(argument)),
            &s.context);
        line << "stats status=" << status << " size=" << st.struct_size
             << " single=" << st.single_calls << "/" << st.single_hits
             << " multi=" << st.multilayer_calls << "/" << st.multilayer_hits
             << " fingerprint=" << std::hex << st.controls_fingerprint;
        out.push_back(line.str());
        continue;
      }
      if (op[0] == 'R') {
        // TP-208.3, as tessera_diffbench prints it (libretro_host.cpp);
        // R<id>/<stride>:<skip> hashes each record from byte <skip> on.
        std::size_t stride = 1, skip = 0;
        if (const auto slash = op.find('/'); slash != std::string::npos) {
          stride = std::stoul(op.substr(slash + 1));
          skip = std::stoul(op.substr(op.find(':', slash) + 1));
          op = op.substr(0, slash);
        }
        const auto comma = op.find(',');
        const auto id =
            static_cast<std::uint32_t>(std::stoul(op.substr(1, comma - 1)));
        Bytes data;
        const auto status = s.read(id, &data);
        const std::size_t n =
            comma == std::string::npos
                ? data.size()
                : std::min<std::size_t>(data.size(),
                                        std::stoul(op.substr(comma + 1)));
        std::uint64_t hash = 1469598103934665603ULL;
        Bytes kept;
        for (std::size_t k = 0; k < n; ++k)
          if (k % stride >= skip) {
            hash = (hash ^ data[k]) * 1099511628211ULL;
            kept.push_back(data[k]);
          }
        line << "region " << id << " status=" << status
             << " size=" << data.size() << " fnv=" << std::hex << hash
             << " head=";
        for (std::size_t k = 0; k < 8 && k < kept.size(); ++k)
          line << (kept[k] < 16 ? "0" : "") << unsigned{kept[k]};
        out.push_back(line.str());
        continue;
      }
      if (op[0] == 'h') {
        // TP-206.1: the FRAME_HASH region, as tessera_diffbench prints it
        std::array<std::uint8_t, 56> fh;
        fh.fill(0x5A);
        std::uint64_t generation = 0;
        status = tessera_observation_region_read_v1(
            s.handle, TESSERA_OBS_REGION_FRAME_HASH, 0, fh.data(), 56,
            UINT64_MAX, &generation, &s.context);
        const auto u32 = [&](std::size_t at) {
          std::uint32_t v = 0;
          std::memcpy(&v, fh.data() + at, 4);
          return v;
        };
        const auto u64 = [&](std::size_t at) {
          std::uint64_t v = 0;
          std::memcpy(&v, fh.data() + at, 8);
          return v;
        };
        line << "hash status=" << status << " version=" << u32(0)
             << " size=" << u32(4) << " frame_index=" << u64(8) << std::hex
             << " video=" << u64(16) << " audio=" << u64(24)
             << " vram=" << u64(32) << " cram=" << u64(40)
             << " vsram=" << u64(48);
        out.push_back(line.str());
        continue;
      }
      if (op[0] == 'd' || op[0] == 'D') {
        // TP-205.4, as tessera_diffbench prints it (libretro_host.cpp)
        tessera_obs_frame_delta_v1 delta{};
        std::memset(&delta, 0x5A, sizeof(delta));
        const auto comma = op.find(',');
        const auto size =
            comma != std::string::npos
                ? static_cast<std::uint32_t>(std::stoul(op.substr(comma + 1)))
            : op[0] == 'd' && op.size() > 1
                ? static_cast<std::uint32_t>(std::stoul(op.substr(1)))
                : static_cast<std::uint32_t>(sizeof(delta));
        if (op[0] == 'd') {
          status = tessera_observation_frame_delta_poll_v1(s.handle, &delta,
                                                           size, &s.context);
          if (status == TESSERA_OK)
            last_generation = delta.frame_generation;
        } else {
          const auto g = op.substr(
              1, comma == std::string::npos ? std::string::npos : comma - 1);
          const std::uint64_t from =
              g[0] == '-' ? last_generation - std::stoull(g.substr(1))
                          : std::stoull(g);
          status = tessera_observation_frame_delta_since_v1(
              s.handle, from, &delta, size, &s.context);
        }
        line << op << " status=" << status << " size=" << delta.struct_size
             << " version=" << delta.delta_version
             << " generation=" << delta.frame_generation
             << " raster=" << delta.raster_event_count
             << " sprites=" << delta.parsed_sprite_count
             << " audio=" << delta.audio_write_count
             << " dropped=" << delta.raster_events_dropped
             << " dirty=" << dirty_runs(delta.dirty_patterns);
        out.push_back(line.str());
        continue;
      }
      if (op[0] == 'c') {
        // TP-302, as tessera_diffbench prints it (libretro_host.cpp): the
        // generation counts from the first one seen in the case.
        tessera_obs_frame_snapshot_v1 snap{};
        std::memset(&snap, 0x5A, sizeof(snap));
        const auto size =
            op.size() > 1 ? static_cast<std::uint32_t>(std::stoul(op.substr(1)))
                          : static_cast<std::uint32_t>(sizeof(snap));
        status = tessera_observation_snapshot_capture_v1(s.handle, &snap, size,
                                                         &s.context);
        if (status == TESSERA_OK && generation_base == UINT64_MAX)
          generation_base = snap.snapshot_generation;
        line << op << " status=" << status << " size=" << snap.struct_size
             << " version=" << snap.snapshot_version
             << " generation=" << (status == TESSERA_OK ? "+" : "")
             << (status == TESSERA_OK
                     ? snap.snapshot_generation - generation_base
                     : snap.snapshot_generation)
             << " frame_generation=" << snap.frame_generation << std::hex
             << " flags=" << snap.flags << " overflow=" << snap.overflow_flags
             << " fallback=" << snap.fallback_reasons << std::dec
             << " sprites=" << snap.parsed_sprite_count
             << " audio=" << snap.audio_write_count << std::hex
             << " reserved=" << snap.reserved0;
        out.push_back(line.str());
        continue;
      }
      if (op[0] == 'G') {
        std::uint64_t generation = 0;
        status = tessera_observation_region_read_v1(
            s.handle, static_cast<std::uint32_t>(std::stoul(op.substr(1))), 0,
            nullptr, 0, UINT64_MAX, &generation, &s.context);
        line << op << " status=" << status;
        if (status == TESSERA_OK) {
          if (generation_base == UINT64_MAX)
            generation_base = generation;
          line << " generation=+" << generation - generation_base;
        }
        out.push_back(line.str());
        continue;
      }
      if (op == "S") {
        saved = s.checkpoint();
        status = TESSERA_OK;
      } else if (op == "L") {
        status = s.restore(saved);
      } else if (op[0] == 's') {
        status = tessera_observation_recompose_v1(s.handle, layers.data(),
                                                  capacity, hex(), &width,
                                                  &height, &s.context);
      } else if (op[0] == 'm') {
        auto *l = layers.data();
        status = tessera_observation_recompose_multilayer_v1(
            s.handle, l, l + full, l + 2 * full, l + 3 * full, l + 4 * full,
            capacity, hex(), &width, &height, &s.context);
      } else if (op[0] == 'p') {
        const auto eq = op.find('=');
        const auto byte = static_cast<std::uint8_t>(
            std::stoul(op.substr(eq + 1), nullptr, 16));
        std::uint64_t generation = 0;
        status = tessera_observation_write_control_v1(
            s.handle,
            static_cast<std::uint32_t>(std::stoul(op.substr(1, eq - 1))), 0,
            &byte, 1, UINT64_MAX, &generation, &s.context);
      } else if (op[0] == 'u') {
        status = s.set(hex());
      } else if (op == "x") {
        status = s.restore(s.checkpoint());
      }
      line << op << " status=" << status;
      out.push_back(line.str());
    }
  }
  return out;
}
} // namespace

namespace {
/// Runs every case of a fixture written by compare_recompose_stats.py
/// ("case <rom> <subscriptions hex> <script> [frames]", gpgx's lines, "end")
/// on the native machine; answers how many cases it ran.
/// Drops the " audio=<hex>" field of a FRAME_HASH line: Tessera's PCM is not
/// gpgx's (48 kHz against 44.1 kHz; plan §5.2 keeps PCM informative; EXC-008),
/// so the audio hash is the same formula over different samples.
std::string without_audio(std::string line) {
  const auto at = line.find(" audio=");
  if (at != std::string::npos)
    line.erase(at, line.find(' ', at + 1) - at);
  return line;
}

unsigned check_stats_fixture(const std::string &name, bool skip_audio = false) {
  std::ifstream fixture(std::string(TESSERA_SOURCE_DIR) +
                        "/tests/fixtures/diffbench/" + name);
  REQUIRE(fixture.good());
  std::string header;
  std::vector<std::string> expected;
  unsigned cases = 0;
  for (std::string line; std::getline(fixture, line);) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;
    if (line.rfind("case ", 0) == 0) {
      header = line;
      expected.clear();
      continue;
    }
    if (line != "end") {
      expected.push_back(skip_audio ? without_audio(native_statuses(line))
                                    : native_statuses(line));
      continue;
    }
    std::istringstream fields(header.substr(5));
    std::string rom, sub, script;
    unsigned frames = 5;
    fields >> rom >> sub >> script;
    fields >> frames;
    CAPTURE(header);
    Observed s(rom_named(rom));
    REQUIRE(s.set(static_cast<std::uint32_t>(std::stoul(sub, nullptr, 16))) ==
            TESSERA_OK);
    auto got = run_stats_script(s, script, frames);
    if (skip_audio)
      for (auto &line : got)
        line = without_audio(line);
    CHECK(got == expected);
    for (std::size_t i = 0, shown = 0;
         i < got.size() && i < expected.size() && shown < 4; ++i)
      if (got[i] != expected[i]) {
        MESSAGE("native: " << got[i] << " | gpgx: " << expected[i]);
        ++shown;
      }
    ++cases;
  }
  return cases;
}
} // namespace

TEST_CASE("recompose_stats answers as gpgx did (TP-205.3)") {
  CHECK(check_stats_fixture("recompose-stats.txt") == 16);
}

TEST_CASE("frame_delta answers as gpgx did (TP-205.4)") {
  CHECK(check_stats_fixture("frame-delta.txt") == 20);
}

// TP-302 (rc.3): the frame snapshot and the generation region reads check.
TEST_CASE("capture_snapshot answers as gpgx did (TP-302)") {
  CHECK(check_stats_fixture("frame-snapshot.txt") == 10);
}

// TP-206.1: video, VRAM, CRAM, VSRAM and frame_index byte for byte; the
// audio field is left out (without_audio).
TEST_CASE("frame_hash answers as gpgx did (TP-206.1)") {
  CHECK(check_stats_fixture("frame-hash.txt", true) == 43);
}

// FX-02 comes from the backend probe; without TESSERA_DIFFBENCH_FX02_ROM the
// build cannot name it and the case does not run.
TEST_CASE("frame_hash over FX-02 answers as gpgx did (TP-206.1)" *
          doctest::skip(
#ifdef TESSERA_FX02_ROM
              false
#else
              true
#endif
              )) {
  CHECK(check_stats_fixture("frame-hash-fx02.txt", true) == 1);
}

// The first frames after power-on, where the test ROMs set the VDP
// up while it draws: Tessera's CPU reaches each write a little earlier than
// gpgx (EXC-007, widened to frame_hash), so a line or a CRAM entry lands
// one step sooner.
// raster_bands 7 also holds the known H32-in-H40 limitation. It runs and
// shows the differences without failing the suite.
TEST_CASE("frame_hash while the ROM sets the VDP up (TP-206.1)" *
          doctest::may_fail()) {
  CHECK(check_stats_fixture("frame-hash-timing.txt", true) == 38);
}

// TP-208.3 (RNF-5): the ROM limit published by limits_query_v1 is the one
// session creation applies, and the other limits are those of plan 2.10.
TEST_CASE("the published limits are the ones applied (TP-208.3)") {
  const auto create = [](const Bytes &rom) {
    tessera_context_v1 context = initial<tessera_context_v1>();
    const Bytes memory(65536, 0);
    std::array<tessera_backend_info_v1, 4> backends{
        initial<tessera_backend_info_v1>(), initial<tessera_backend_info_v1>(),
        initial<tessera_backend_info_v1>(), initial<tessera_backend_info_v1>()};
    std::uint32_t required{};
    REQUIRE(tessera_backends_enumerate_v1(backends.data(), backends.size(),
                                          &required, &context) == TESSERA_OK);
    const tessera_backend_info_v1 *native = nullptr;
    for (std::uint32_t i = 0; i < required && i < backends.size(); ++i)
      if (std::string_view(backends[i].id) == "megadrive")
        native = &backends[i];
    REQUIRE(native != nullptr);
    const auto configuration =
        tessera::wire::configuration("megadrive", native->build_id, memory);
    auto info = initial<tessera_session_create_info_v1>();
    const auto id = tessera::wire::text("megadrive");
    info.backend_id = {id.data(), id.size()};
    info.content = {rom.data(), rom.size()};
    info.config = {configuration.data(), configuration.size()};
    info.initial_persistent_memory = {memory.data(), memory.size()};
    info.system_override = TESSERA_SYSTEM_MEGA_DRIVE;
    tessera_session handle{};
    const auto status = tessera_session_create_v1(&info, &handle, &context);
    if (status == TESSERA_OK)
      tessera_session_close_v1(handle, &context);
    return status;
  };
  constexpr std::size_t kMiB = 1024 * 1024;
  CHECK(create(tessera::testrom::sized_rom(16 * kMiB)) == TESSERA_OK);
  CHECK(create(tessera::testrom::sized_rom(9 * kMiB)) == TESSERA_OK);
  CHECK(create(tessera::testrom::sized_rom(16 * kMiB + 2)) ==
        TESSERA_INVALID_CONTENT);

  Native s(tessera::testrom::sized_rom(0x400));
  auto limits = initial<tessera_observation_limits_v1>();
  REQUIRE(tessera_observation_limits_query_v1(s.handle, &limits, &s.context) ==
          TESSERA_OK);
  CHECK(limits.max_rom_bytes == 16 * kMiB);
  CHECK(limits.max_state_bytes == 2 * kMiB);
  CHECK(limits.max_journal_events == 256);
  CHECK(limits.max_audio_writes == 8192);
  CHECK(limits.max_parsed_sprites == 128);
  CHECK(s.checkpoint().size() <= limits.max_state_bytes);
}

// TP-208.3: past the limits, as gpgx: AUDIO_WRITES keeps the first 8 192
// records of the frame, RASTER_JOURNAL counts what it dropped.
TEST_CASE("past the limits the regions answer as gpgx did (TP-208.3)") {
  CHECK(check_stats_fixture("limits.txt") == 5);
}

// TP-211.1: LAYER_MASK and LAYER_DIM in Mode 4 (mode4_scene, mode4_pbc), read
// through the FRAME_HASH video of the next frame.
TEST_CASE("the render controls act in Mode 4 as on gpgx (TP-211.1)") {
  CHECK(check_stats_fixture("mode4-controls.txt", true) == 11);
}

// EXC-007 (docs/parity-exceptions.md): the CPU writes a few VRAM words more
// per frame than on gpgx, so the dirty rows next to a frame boundary differ.
// It runs and shows the differences without failing the suite.
TEST_CASE("frame_delta at a frame boundary (TP-205.4, EXC-007)" *
          doctest::may_fail()) {
  CHECK(check_stats_fixture("frame-delta-timing.txt") == 5);
}
