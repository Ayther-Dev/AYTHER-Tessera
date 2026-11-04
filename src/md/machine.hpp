// Native Mega Drive machine: configuration, time constants and scheduler
// (spec 008, TP-101.2; plan 1.3). The unit of time is the master clock cycle.
#pragma once
#include <array>
#include <cstdint>

namespace tessera::md {

enum class Region : std::uint8_t { japan, usa, europe };
enum class Standard : std::uint8_t { ntsc, pal };
enum class PadType : std::uint8_t { three_button, six_button };
enum class Mapper : std::uint8_t { none, ssf2 };
enum class DisplayWidth : std::uint8_t { h32, h40 };

// A line is 3420 master cycles in H32 and H40 alike.
inline constexpr std::uint32_t kCyclesPerLine = 3420;
inline constexpr std::uint32_t kDivider68k = 7;
inline constexpr std::uint32_t kDividerZ80 = 15;

constexpr std::uint32_t master_hz(Standard s) noexcept {
  return s == Standard::ntsc ? 53'693'175U : 53'203'424U;
}
constexpr std::uint32_t lines_per_frame(Standard s) noexcept {
  return s == Standard::ntsc ? 262U : 313U;
}
constexpr std::uint32_t cycles_per_frame(Standard s) noexcept {
  return lines_per_frame(s) * kCyclesPerLine;
}
// VDP dot clock divider: master/8 in H40, master/10 in H32.
constexpr std::uint32_t dot_divider(DisplayWidth w) noexcept {
  return w == DisplayWidth::h40 ? 8U : 10U;
}

struct MachineConfig {
  Region region = Region::usa;
  Standard standard = Standard::ntsc;
  std::array<PadType, 2> pads{PadType::three_button, PadType::three_button};
  bool tmss = false;
  Mapper mapper = Mapper::none;

  // Default configuration of a console of the given region: Europe is PAL,
  // Japan and the Americas are NTSC.
  static constexpr MachineConfig for_region(Region r) noexcept {
    MachineConfig config;
    config.region = r;
    config.standard = r == Region::europe ? Standard::pal : Standard::ntsc;
    return config;
  }
};

// Ticks of each CPU produced by advancing the master clock.
struct Ticks {
  std::uint64_t m68k = 0;
  std::uint64_t z80 = 0;
};

// Advances the master clock and derives line, frame and component ticks. The
// remainders of the dividers are carried between calls, so the total is always
// floor(master / divider) however the time is sliced.
class Scheduler {
public:
  explicit Scheduler(Standard standard) noexcept
      : frame_cycles_(cycles_per_frame(standard)) {}

  Ticks advance(std::uint64_t master_cycles) noexcept {
    const auto before = master_;
    master_ += master_cycles;
    return {master_ / kDivider68k - before / kDivider68k,
            master_ / kDividerZ80 - before / kDividerZ80};
  }

  std::uint64_t master() const noexcept { return master_; }
  std::uint64_t frame() const noexcept { return master_ / frame_cycles_; }
  std::uint32_t frame_cycle() const noexcept {
    return static_cast<std::uint32_t>(master_ % frame_cycles_);
  }
  std::uint32_t line() const noexcept { return frame_cycle() / kCyclesPerLine; }
  std::uint32_t line_cycle() const noexcept {
    return frame_cycle() % kCyclesPerLine;
  }

private:
  std::uint32_t frame_cycles_;
  std::uint64_t master_ = 0;
};

} // namespace tessera::md
