// Digest of everything a frame of the native Mega Drive produces or leaves
// behind (video, audio, RAM, VDP memories, clocks): the comparison unit of the
// savestate and determinism tests.
#pragma once
#include "md/system.hpp"

#include <cstdint>

namespace tessera::testsupport {

struct Fnv {
  std::uint64_t h = 14695981039346656037ULL;
  void add(const void *data, std::size_t n) {
    const auto *p = static_cast<const std::uint8_t *>(data);
    for (std::size_t i = 0; i < n; ++i) {
      h ^= p[i];
      h *= 1099511628211ULL;
    }
  }
  template <class T> void add(const T &v) { add(&v, sizeof(v)); }
};

inline std::uint64_t digest(const tessera::md::System &system) {
  Fnv f;
  const auto ram = system.work_ram();
  f.add(ram.data(), ram.size());
  const auto z = system.z80_ram();
  f.add(z.data(), z.size());
  f.add(system.vdp().vram().data(), system.vdp().vram().size());
  f.add(system.vdp().cram().data(), system.vdp().cram().size() * 2);
  f.add(system.vdp().vsram().data(), system.vdp().vsram().size() * 2);
  const auto &frame = system.renderer().frame();
  f.add(frame.data(), frame.size() * sizeof(frame[0]));
  for (const auto &w : system.audio_writes())
    f.add(w);
  f.add(system.audio_writes_dropped());
  for (const auto &s : system.ym().samples())
    f.add(s);
  const auto psg = system.psg().samples();
  f.add(psg.data(), psg.size() * sizeof(psg[0]));
  const auto audio = system.audio();
  f.add(audio.data(), audio.size() * sizeof(audio[0]));
  f.add(system.cpu().getClock());
  f.add(system.z80().cycles());
  return f.h;
}

} // namespace tessera::testsupport
