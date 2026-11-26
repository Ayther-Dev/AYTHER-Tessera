// Backend for the Mega Drive integration tests (TP-112.2, DA-1). CTest sets
// TESSERA_TEST_BACKEND per registration: the native core by default and
// clownmdemu for the reference variant.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace tessera_test {
inline std::string backend_under_test() {
  const char *value = std::getenv("TESSERA_TEST_BACKEND");
  return value != nullptr && *value != '\0' ? value : "megadrive";
}
inline bool native_under_test() { return backend_under_test() == "megadrive"; }
// Input layout of the backend: the native core takes two pads of 12 controls
// (TP-108.3), ClownMDEmu one pad of 8.
inline std::uint16_t pad_ports() { return native_under_test() ? 2 : 1; }
inline std::uint16_t pad_controls() { return native_under_test() ? 12 : 8; }
template <class Assignment> std::vector<Assignment> neutral_input() {
  std::vector<Assignment> out;
  for (std::uint16_t port = 0; port < pad_ports(); ++port)
    for (std::uint16_t control = 0; control < pad_controls(); ++control)
      out.push_back({port, control, 0});
  return out;
}
} // namespace tessera_test
