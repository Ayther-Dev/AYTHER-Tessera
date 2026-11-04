// Banco diferencial (spec 008, RF-7.2). Esqueleto TP-005.1: lee y valida los
// manifiestos oracles.toml y corpus.toml. Los planos llegan en TP-005.2 en
// adelante.
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
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

void help() {
  std::cout << "tessera_diffbench - banco diferencial (spec 008, RF-7.2)\n"
               "  --oracles <oracles.toml>   rutas y hashes de los oráculos\n"
               "  --corpus <corpus.toml>     entradas del corpus\n"
               "  --entry <id>               entrada a ejecutar\n"
               "  --planes <lista>           framebuffer,audio_writes,regions\n"
               "  --out <dir>                carpeta del informe\n"
               "  --validate-only            solo valida los manifiestos\n";
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
    if (a == "--validate-only") {
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
