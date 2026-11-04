// TP-101.10 (RF-7.3): the synthetic `bus_sweep` ROM on the native machine
// leaves the same work RAM and the same SRAM as gpgx did (goldens taken from
// gpgx with tessera_diffbench and stored in tests/fixtures/md).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "md/system.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

using namespace tessera::md;

namespace {
std::uint64_t fnv1a(std::span<const std::uint8_t> bytes) {
  std::uint64_t h = 1469598103934665603ULL;
  for (const auto b : bytes) {
    h ^= b;
    h *= 1099511628211ULL;
  }
  return h;
}
std::string hex(std::uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
  return buf;
}
std::map<std::string, std::string> read_golden() {
  std::map<std::string, std::string> out;
  std::ifstream in(GOLDEN_FILE);
  std::string line;
  while (std::getline(in, line)) {
    const auto eq = line.find('=');
    if (eq != std::string::npos)
      out[line.substr(0, eq)] = line.substr(eq + 1);
  }
  return out;
}
std::vector<std::uint8_t> read_bytes(const char *path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
} // namespace

TEST_CASE("bus_sweep leaves gpgx's work RAM and SRAM") {
  const auto golden = read_golden();
  REQUIRE(golden.count("frames"));
  REQUIRE(golden.count("work_ram_fnv1a64"));
  REQUIRE(golden.count("sram_fnv1a64"));

  System system(tessera::testrom::bus_sweep_rom(),
                MachineConfig::for_region(Region::usa));
  system.reset();
  for (int i = 0; i < std::stoi(golden.at("frames")); ++i)
    system.run_frame();

  CHECK(hex(fnv1a(system.work_ram())) == golden.at("work_ram_fnv1a64"));
  const auto srm = system.sram().to_srm();
  CHECK(hex(fnv1a(srm)) == golden.at("sram_fnv1a64"));
  CHECK(srm == read_bytes(SRM_FIXTURE)); // byte for byte, not just the hash
}
