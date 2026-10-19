#include "systems/system_registry.hpp"
#include "backends/synthetic/synthetic_backend.hpp"
#include "systems/system_detector.hpp"
namespace tessera {
std::span<const SystemRegistration> SystemRegistry::registrations() {
  static const std::array<SystemRegistration, 2> entries{
      {{"synthetic.minimal", TESSERA_BACKEND_TEST, TESSERA_SYSTEM_NONE, 0x100F,
        make_synthetic_backend},
       {"synthetic.complete", TESSERA_BACKEND_TEST, TESSERA_SYSTEM_NONE, 0x1DFF,
        make_synthetic_backend}}};
  return entries;
}
std::unique_ptr<IEmulatedSystem> SystemRegistry::create(std::string_view id,
                                                        wire::View content,
                                                        wire::View memory) {
  for (const auto &entry : registrations())
    if (entry.id == id)
      return entry.factory(id, content, memory);
  throw Failure{TESSERA_IMPLEMENTATION_UNAVAILABLE, TESSERA_PHASE_CREATE};
}
bool SystemRegistry::initial_reset_fault(std::string_view id,
                                         wire::View content) {
  for (const auto &entry : registrations())
    if (entry.id == id)
      return (parse_synthetic_content(content).flags & 1) != 0;
  return false;
}
std::string SystemRegistry::resolve(std::string_view requested,
                                    wire::View content,
                                    std::uint32_t system_override) {
  require(system_override <= TESSERA_SYSTEM_MEGA_DRIVE);
  for (const auto &entry : registrations())
    if (entry.id == requested) {
      require(system_override == entry.system, TESSERA_INVALID_CONTENT);
      return std::string(entry.id);
    }
  if (requested.empty()) {
    const auto detection = SystemDetector::detect(content, system_override);
    require(detection.status == TESSERA_OK, detection.status);
  }
  throw Failure{TESSERA_IMPLEMENTATION_UNAVAILABLE, TESSERA_PHASE_CREATE};
}
} // namespace tessera
