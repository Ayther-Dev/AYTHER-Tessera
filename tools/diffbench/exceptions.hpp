// Lista de excepciones de paridad (spec 008, D-3, TP-005.9).
// Lee las filas de docs/parity-exceptions.md (plan §3.4).
#pragma once
#include <cstdint>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace diffbench {

struct Exception {
  std::string id, plane;
  uint64_t first_frame = 0, last_frame = 0;
};

namespace detail {
inline std::string trim_cell(const std::string &s) {
  const auto a = s.find_first_not_of(" \t\r");
  const auto b = s.find_last_not_of(" \t\r");
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
} // namespace detail

// Cada fila `| EXC-nnn | plano | alcance | descripción | afecta | aprobación
// |`. El alcance contiene un rango de frames `a–b` (o `a-b`, o un solo frame).
// `afecta a identidad` solo puede ser «No» (RF-10.5) y toda excepción lleva
// aprobación; si no, `error` explica el defecto y la lista queda vacía.
inline std::vector<Exception> parse_exceptions(const std::string &text,
                                               std::string &error) {
  std::vector<Exception> out;
  std::istringstream in(text);
  std::string line;
  const std::regex range(R"((\d+)\s*(?:–|-)\s*(\d+))");
  const std::regex single(R"((\d+))");
  while (std::getline(in, line)) {
    if (line.rfind("| EXC-", 0) != 0)
      continue;
    std::vector<std::string> cells;
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, '|'))
      cells.push_back(detail::trim_cell(cell));
    // cells[0] queda vacío antes de la primera barra.
    if (cells.size() < 7) {
      error = "fila mal formada: " + line;
      return {};
    }
    Exception e;
    e.id = cells[1];
    e.plane = cells[2];
    if (cells[5] != "No") {
      error = e.id + ": una divergencia que afecta a la identidad es un "
                     "defecto, no una excepción (RF-10.5)";
      return {};
    }
    if (cells[6].empty()) {
      error = e.id + ": falta la aprobación";
      return {};
    }
    std::smatch m;
    if (std::regex_search(cells[3], m, range)) {
      e.first_frame = std::stoull(m[1]);
      e.last_frame = std::stoull(m[2]);
    } else if (std::regex_search(cells[3], m, single)) {
      e.first_frame = e.last_frame = std::stoull(m[1]);
    } else {
      error = e.id + ": el alcance no indica frames";
      return {};
    }
    out.push_back(e);
  }
  return out;
}

// Id de la excepción que cubre la divergencia, o cadena vacía.
inline std::string find_exception(const std::vector<Exception> &list,
                                  const std::string &plane, uint64_t frame) {
  for (const auto &e : list)
    if (e.plane == plane && frame >= e.first_frame && frame <= e.last_frame)
      return e.id;
  return {};
}

} // namespace diffbench
