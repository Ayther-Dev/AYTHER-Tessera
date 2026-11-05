// Entradas por frame del banco diferencial (spec 008, D-7, TP-005.4).
// Lee una traza TSTRACE1 (binaria) o su JSON canónico y la reduce a ocho
// controles por frame del puerto 0: arriba, abajo, izquierda, derecha, A, B,
// C y start, en el orden de la API.
#pragma once
#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "domain/wire.hpp"

namespace diffbench {

using FrameInput = std::array<int32_t, 8>;

inline std::vector<FrameInput> load_trace(const std::string &path,
                                          std::string &error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "no se puede abrir la traza " + path;
    return {};
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
  std::vector<FrameInput> frames;
  try {
    if (bytes.size() >= 8 &&
        std::string(bytes.begin(), bytes.begin() + 8) == "TSTRACE1") {
      const auto t = tessera::wire::trace(bytes);
      for (const auto &f : t.frames) {
        FrameInput row{};
        for (size_t i = 0; i < 8; ++i)
          row[i] = f[i];
        frames.push_back(row);
      }
    } else {
      const auto j = nlohmann::json::parse(bytes.begin(), bytes.end());
      for (const auto &f : j.at("frames")) {
        FrameInput row{};
        for (const auto &a : f.at("assignments"))
          if (a.at("port").get<int>() == 0 && a.at("control").get<int>() < 8)
            row[a.at("control").get<size_t>()] = a.at("value").get<int32_t>();
        frames.push_back(row);
      }
    }
  } catch (const std::exception &e) {
    error = std::string("traza inválida: ") + e.what();
    return {};
  }
  return frames;
}

} // namespace diffbench
