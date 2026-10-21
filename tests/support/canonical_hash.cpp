#include "support/canonical_hash.hpp"
#include <algorithm>
namespace independent_oracle {
Digests frame(bool complete, bool action, std::uint64_t f) {
  // Closed forms derived from the model, without executing a backend.
  const auto count =
      action && f > 300 ? std::min<std::uint64_t>(f - 300, 300) : 0;
  const unsigned mask = action && f >= 301 && f <= 600 ? 32 : 0;
  const auto accumulator = static_cast<std::uint32_t>(7 + f + 32 * count);
  auto memory = fx01::memory();
  for (std::size_t j = 0; j < 16; ++j)
    memory[j] =
        static_cast<std::uint8_t>(memory[j] + f + (j % 8 == 5 ? count : 0));
  const auto phase = static_cast<std::uint32_t>((7 + 800 * f) % 256);
  const auto backend = fx01::backend(complete);
  const auto rom = fx01::content(complete ? 2 : 1);
  const auto config = fx01::config(backend, fx01::memory());
  auto state = fx01::str("TSSTATE1");
  fx01::field(state, 1, 6, fx01::str("tessera.synthetic"));
  fx01::scalar(state, 2, 1, 1);
  fx01::scalar(state, 3, 1, 0);
  fx01::field(state, 4, 5, fx01::sha(rom));
  fx01::field(state, 5, 5, backend.build_id);
  fx01::field(state, 6, 5, fx01::sha(config));
  fx01::scalar(state, 7, 3, f);
  fx01::scalar(state, 8, 3, 1000 * f);
  fx01::field(state, 9, 8, fx01::input_wire(mask));
  fx01::Bytes machine;
  fx01::scalar(machine, 1, 1, accumulator);
  fx01::field(machine, 2, 5, memory);
  fx01::scalar(machine, 3, 1, phase);
  fx01::field(state, 65536, 11, machine);
  auto video = fx01::str("TSVIDEO1");
  for (auto value : {16U, 16U, 1U})
    fx01::put(video, value, 4);
  for (unsigned y = 0; y < 16; ++y)
    for (unsigned x = 0; x < 16; ++x) {
      const bool sprite = x == (mask & 7) && y == ((mask >> 3) & 7);
      const auto px = sprite ? 15 : x, py = sprite ? 15 : y;
      video.push_back(static_cast<std::uint8_t>(px + mask));
      video.push_back(static_cast<std::uint8_t>(py + accumulator));
      video.push_back(static_cast<std::uint8_t>(memory[(px + py) % 16] + f));
      video.push_back(255);
    }
  auto audio = fx01::str("TSAUDIO1");
  fx01::put(audio, 48000, 4);
  fx01::put(audio, 2, 4);
  fx01::put(audio, 800, 8);
  const auto previous = (7 + 800 * (f ? f - 1 : 0)) % 256;
  const auto amplitude = 1000 + 4 * static_cast<std::int32_t>(mask);
  for (unsigned n = 0; n < 800; ++n) {
    const auto left = (previous + n) % 256 < 128 ? amplitude : -amplitude;
    fx01::put(audio, static_cast<std::uint16_t>(left), 2);
    fx01::put(audio, static_cast<std::uint16_t>(-left), 2);
  }
  return {fx01::hex(state), fx01::hex(video), fx01::hex(audio)};
}
} // namespace independent_oracle
