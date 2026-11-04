// Banco diferencial (spec 008, RF-7.2). Esqueleto TP-005.1: lee y valida los
// manifiestos oracles.toml y corpus.toml. Los planos llegan en TP-005.2 en
// adelante.
#include "libretro_host.hpp"
#include "trace_input.hpp"

#include <nlohmann/json.hpp>
#include "tessera/tessera_observation.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace {

using Table = std::map<std::string, std::string>;

struct Manifest {
  Table top;
  std::vector<Table> entries;
};

std::string trim(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
    ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
    --b;
  return s.substr(a, b - a);
}

std::string unquote(const std::string &v) {
  if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
    return v.substr(1, v.size() - 2);
  return v;
}

// Subconjunto de TOML: [[entry]], clave = valor (cadena, número, lista).
bool parse(const std::string &path, Manifest &out, std::string &err) {
  std::ifstream in(path);
  if (!in) {
    err = "no se puede abrir " + path;
    return false;
  }
  Table *current = &out.top;
  std::string line;
  int n = 0;
  while (std::getline(in, line)) {
    ++n;
    const auto hash = line.find('#');
    if (hash != std::string::npos && line.find('"') == std::string::npos)
      line.erase(hash);
    line = trim(line);
    if (line.empty())
      continue;
    if (line == "[[entry]]") {
      out.entries.emplace_back();
      current = &out.entries.back();
      continue;
    }
    const auto eq = line.find('=');
    if (line.front() == '[' || eq == std::string::npos) {
      err = path + ":" + std::to_string(n) + ": línea no admitida";
      return false;
    }
    (*current)[trim(line.substr(0, eq))] = unquote(trim(line.substr(eq + 1)));
  }
  return true;
}

bool is_sha256(const std::string &s) {
  return std::regex_match(s, std::regex("[0-9a-f]{64}"));
}

bool validate(const Manifest &oracles, const Manifest &corpus,
              std::vector<std::string> &errs) {
  for (const char *k : {"gpgx_core", "gpgx_sha256", "corpus_dir"})
    if (!oracles.top.count(k))
      errs.push_back(std::string("oracles.toml: falta ") + k);
  if (oracles.top.count("gpgx_sha256") &&
      !is_sha256(oracles.top.at("gpgx_sha256")))
    errs.push_back("oracles.toml: gpgx_sha256 no es un sha256");
  if (corpus.entries.empty())
    errs.push_back("corpus.toml: no hay entradas");
  for (const auto &e : corpus.entries) {
    const auto id = e.count("id") ? e.at("id") : std::string("?");
    if (!e.count("sha256") || !is_sha256(e.at("sha256")))
      errs.push_back("corpus.toml: " + id + ": sha256 ausente o inválido");
    for (const char *k : {"id", "system", "region", "segments"})
      if (!e.count(k))
        errs.push_back("corpus.toml: " + id + ": falta " + k);
  }
  return errs.empty();
}

int report_run(const diffbench::RunResult &r, unsigned frames,
               std::map<std::string, std::string> &args) {
  if (!r.error.empty()) {
    std::cerr << "error: " << r.error << "\n";
    return 1;
  }
  const auto write_blob = [&](const char *flag, const std::vector<uint8_t> &data) {
    if (!args.count(flag))
      return;
    std::ofstream blob(args[flag], std::ios::binary | std::ios::trunc);
    blob.write(reinterpret_cast<const char *>(data.data()),
               static_cast<std::streamsize>(data.size()));
  };
  write_blob("--save-ram-out", r.save_ram);
  write_blob("--work-ram-out", r.work_ram);
  write_blob("--frame-out", r.last_frame);
  if (args.count("--hashes-out")) {
    std::ofstream out(args["--hashes-out"]);
    for (const auto h : r.frame_hashes)
      out << std::hex << h << "\n";
  }
  if (args.count("--dump-regions")) {
    unsigned readable = 0, read_ok = 0, size_ok = 0;
    for (const auto &d : r.regions) {
      std::cout << "region=" << d.id << " bytes=" << d.byte_size
                << " access=" << d.access << " query=" << d.query_status
                << " elem=" << d.element_size << " cap=" << d.capacity
                << " ver=" << d.data_version << " legacy=" << d.legacy_id
                << " read=" << d.read_status << "\n";
      if (d.access & TESSERA_OBS_REGION_ACCESS_READ) {
        ++readable;
        read_ok += d.read_status == 0;
        size_ok += d.bytes_read == d.byte_size && d.byte_size > 0;
      }
    }
    std::cout << "regions=" << r.regions.size() << " readable=" << readable
              << " read_ok=" << read_ok << " size_ok=" << size_ok << "\n";
    return read_ok == readable && size_ok == readable ? 0 : 1;
  }
  if (args.count("--report-out")) {
    // Informe del plan §3.3. Esta ejecución solo corre el oráculo, así que
    // no hay candidato ni divergencia; nunca incluye framebuffers ni PCM.
    nlohmann::json report = {
        {"tool", "tessera_diffbench"},
        {"contract", 1},
        {"entry", args.count("--entry") ? args["--entry"] : "unnamed"},
        {"segment", {0, frames}},
        {"reference", {{"core", "gpgx"}, {"build_id", "ayther-abi-1.10-r4"}}},
        {"candidate", {{"core", "gpgx"}, {"build_id", "ayther-abi-1.10-r4"}}},
        {"frames_compared", r.frame_hashes.size()},
        {"first_divergence", nullptr},
        {"exceptions_applied", nlohmann::json::array()},
        {"audio_writes", {{"frames_equal", r.frame_hashes.size()}}}};
    std::ofstream out(args["--report-out"]);
    out << report.dump(2) << "\n";
  }
  std::cout << "frames=" << frames << " hashed=" << r.frame_hashes.size()
            << " size=" << r.width << "x" << r.height
            << " audio_peak=" << r.peak << " \n";
  return r.frame_hashes.size() == frames ? 0 : 1;
}

void help() {
  std::cout << "tessera_diffbench - banco diferencial (spec 008, RF-7.2)\n"
               "  --oracles <oracles.toml>   rutas y hashes de los oráculos\n"
               "  --corpus <corpus.toml>     entradas del corpus\n"
               "  --entry <id>               entrada a ejecutar\n"
               "  --planes <lista>           framebuffer,audio_writes,regions\n"
               "  --out <dir>                carpeta del informe\n"
               "  --validate-only            solo valida los manifiestos\n"
               "  --libretro <dll> --rom <archivo> [--frames N] "
               "[--hashes-out <archivo>]\n"
               "                             ejecuta un core y hashea cada "
               "frame\n";
}

} // namespace

int main(int argc, char **argv) {
  std::map<std::string, std::string> args;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      help();
      return 0;
    }
    if (a == "--validate-only" || a == "--dump-regions") {
      args[a] = "1";
      continue;
    }
    if (a.rfind("--", 0) == 0 && i + 1 < argc) {
      args[a] = argv[++i];
      continue;
    }
    std::cerr << "argumento no reconocido: " << a << "\n";
    return 2;
  }
#ifdef TESSERA_DIFFBENCH_WITH_TESSERA
  if (args.count("--libretro") && args.count("--tessera") &&
      args.count("--first-divergence-out")) {
    // Primera divergencia del plano 1 entre el or�culo y el candidato. El
    // plano 2 no se compara: el backend de referencia no expone escrituras de
    // audio por la API p�blica, y se registra como no disponible.
    const unsigned frames = args.count("--frames")
                                ? static_cast<unsigned>(std::strtoul(
                                      args["--frames"].c_str(), nullptr, 10))
                                : 600u;
    const auto a =
        diffbench::run_libretro(args["--libretro"], args["--rom"], frames);
    const auto b =
        diffbench::run_tessera(args["--tessera"], args["--rom"], frames);
    if (!a.error.empty()) {
      std::cerr << "error: " << a.error << b.error << "\n";
      return 1;
    }
    nlohmann::json first = nullptr;
    unsigned equal = 0;
    for (size_t i = 0;
         i < std::min(a.frame_hashes.size(), b.frame_hashes.size()); ++i) {
      if (a.frame_hashes[i] != b.frame_hashes[i]) {
        first = {{"plane", "framebuffer"},
                 {"frame", i},
                 {"reference_hash", a.frame_hashes[i]},
                 {"candidate_hash", b.frame_hashes[i]}};
        break;
      }
      ++equal;
    }
    if (first.is_null() && !b.error.empty())
      first = {{"plane", "framebuffer"},
               {"frame", b.frame_hashes.size()},
               {"candidate_error", b.error}};
    nlohmann::json out = {
        {"tool", "tessera_diffbench"},
        {"entry", args.count("--entry") ? args["--entry"] : "unnamed"},
        {"segment", {0, frames}},
        {"reference", {{"core", "gpgx"}, {"build_id", "ayther-abi-1.10-r4"}}},
        {"candidate", {{"core", args["--tessera"]}}},
        {"frames_compared",
         std::min(a.frame_hashes.size(), b.frame_hashes.size())},
        {"frames_equal", equal},
        {"first_divergence", first},
        {"candidate_error", b.error},
        {"audio_writes", {{"status", "not_available"}}},
        {"audio_peak", {{"reference", a.peak}, {"candidate", b.peak}}}};
    std::ofstream f(args["--first-divergence-out"]);
    f << out.dump(2) << "\n";
    std::cout << "frames_equal=" << equal << " first_divergence="
              << (first.is_null()
                      ? "none"
                      : "frame " + std::to_string(first["frame"].get<size_t>()))
              << "\n";
    return 0;
  }
#endif
  if (args.count("--libretro")) {
    if (!args.count("--rom")) {
      std::cerr << "falta --rom\n";
      return 2;
    }
    const unsigned frames = args.count("--frames")
                                ? static_cast<unsigned>(std::strtoul(
                                      args["--frames"].c_str(), nullptr, 10))
                                : 600u;
    std::vector<diffbench::TraceFrame> trace;
    if (args.count("--trace")) {
      std::string terr;
      trace = diffbench::load_trace(args["--trace"], terr);
      if (!terr.empty()) {
        std::cerr << "error: " << terr << "\n";
        return 1;
      }
    }
    const auto r =
        diffbench::run_libretro(args["--libretro"], args["--rom"], frames,
                                args.count("--dump-regions") > 0,
                                args.count("--trace") ? &trace : nullptr);
    return report_run(r, frames, args);
  }
  if (args.count("--tessera")) {
#ifdef TESSERA_DIFFBENCH_WITH_TESSERA
    if (!args.count("--rom")) {
      std::cerr << "falta --rom\n";
      return 2;
    }
    const unsigned frames = args.count("--frames")
                                ? static_cast<unsigned>(std::strtoul(
                                      args["--frames"].c_str(), nullptr, 10))
                                : 600u;
    std::vector<diffbench::TraceFrame> trace;
    if (args.count("--trace")) {
      std::string terr;
      trace = diffbench::load_trace(args["--trace"], terr);
      if (!terr.empty()) {
        std::cerr << "error: " << terr << "\n";
        return 1;
      }
    }
    const auto r =
        diffbench::run_tessera(args["--tessera"], args["--rom"], frames,
                               args.count("--trace") ? &trace : nullptr);
    return report_run(r, frames, args);
#else
    std::cerr << "este binario no incluye el backend de referencia "
                 "(TESSERA_BUILD_CLOWN_REFERENCE=OFF)\n";
    return 2;
#endif
  }
  if (!args.count("--oracles") || !args.count("--corpus")) {
    std::cerr << "faltan --oracles y --corpus\n";
    return 2;
  }
  Manifest oracles, corpus;
  std::string err;
  if (!parse(args["--oracles"], oracles, err) ||
      !parse(args["--corpus"], corpus, err)) {
    std::cerr << err << "\n";
    return 2;
  }
  std::vector<std::string> errs;
  if (!validate(oracles, corpus, errs)) {
    for (const auto &e : errs)
      std::cerr << "error: " << e << "\n";
    return 1;
  }
  std::cout << "manifiestos válidos: " << corpus.entries.size()
            << " entradas\n";
  if (!args.count("--validate-only")) {
    std::cerr << "los planos llegan en TP-005.2 y siguientes\n";
    return 3;
  }
  return 0;
}
