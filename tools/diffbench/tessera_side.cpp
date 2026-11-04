// Lado Tessera del banco diferencial (spec 008, TP-005.5): ejecuta un backend
// por la API pública y hashea cada frame con el mismo formato que el oráculo.
#include "cli/public_client.hpp"
#include "domain/wire.hpp"
#include "libretro_host.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace diffbench {

RunResult run_tessera(const std::string &backend_id,
                      const std::string &rom_path, unsigned frames,
                      const std::vector<TraceFrame> *trace) {
  RunResult result;
  std::ifstream in(rom_path, std::ios::binary);
  if (!in) {
    result.error = "no se puede abrir la ROM " + rom_path;
    return result;
  }
  const tessera::cli::Bytes rom((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
  try {
    using namespace tessera::cli;
    const auto backend = find_backend(backend_id);
    const Bytes memory(65536);
    const auto config = tessera::wire::configuration(
        backend_id, std::span<const std::uint8_t>(backend.build_id), memory);
    PublicSession session(backend_id, rom, config, memory);
    for (unsigned i = 0; i < frames; ++i) {
      std::uint32_t mask = 0;
      if (trace && i < trace->size())
        for (unsigned c = 0; c < 8; ++c)
          mask |= static_cast<std::uint32_t>((*trace)[i][c] != 0) << c;
      session.step(digital_inputs(mask));
      const auto frame = session.original();
      ensure(frame.video_format == TESSERA_VIDEO_RGBA8,
             TESSERA_BACKEND_FAILURE);
      uint64_t hash = fnv_seed;
      for (std::uint32_t y = 0; y < frame.height; ++y) {
        const auto *row =
            frame.pixels.data + std::uint64_t{frame.row_bytes} * y;
        for (std::uint32_t x = 0; x < frame.width; ++x)
          hash = fnv_rgb(hash, row[x * 4], row[x * 4 + 1], row[x * 4 + 2]);
      }
      result.width = frame.width;
      result.height = frame.height;
      result.frame_hashes.push_back(hash);
      const auto *pcm = reinterpret_cast<const std::int16_t *>(frame.pcm.data);
      for (std::uint64_t k = 0; pcm && k < frame.pcm_frames * 2; ++k)
        result.peak = std::max(result.peak, std::abs(int{pcm[k]}));
    }
  } catch (const tessera::cli::ClientError &e) {
    result.error = "API pública: " + e.text;
  } catch (const std::exception &e) {
    result.error = e.what();
  }
  return result;
}

} // namespace diffbench
