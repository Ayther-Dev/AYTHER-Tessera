#include "systems/system_registry.hpp"
#include "backends/synthetic/synthetic_backend.hpp"
#include "backends/megadrive/megadrive_backend.hpp"
#include "systems/system_detector.hpp"
namespace tessera {
std::span<const SystemRegistration> SystemRegistry::registrations() {
  static const std::array<SystemRegistration, 3> entries{
      {{"clownmdemu", TESSERA_BACKEND_PRODUCT, TESSERA_SYSTEM_MEGA_DRIVE,
        0x1FFF, make_megadrive_backend},
       {"synthetic.minimal", TESSERA_BACKEND_TEST, TESSERA_SYSTEM_NONE, 0x120F,
        make_synthetic_backend},
       {"synthetic.complete", TESSERA_BACKEND_TEST, TESSERA_SYSTEM_NONE, 0x1FFF,
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
    if (entry.id == id && entry.kind == TESSERA_BACKEND_TEST)
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
      if (entry.kind == TESSERA_BACKEND_PRODUCT) {
        const auto detection = SystemDetector::detect(content, system_override);
        require(detection.status == TESSERA_OK &&
                    detection.info.system == entry.system,
                detection.status == TESSERA_OK ? TESSERA_INVALID_CONTENT
                                               : detection.status);
      }
      return std::string(entry.id);
    }
  if (requested.empty()) {
    const auto detection = SystemDetector::detect(content, system_override);
    require(detection.status == TESSERA_OK, detection.status);
    for (const auto &entry : registrations())
      if (entry.kind == TESSERA_BACKEND_PRODUCT &&
          entry.system == detection.info.system)
        return std::string(entry.id);
  }
  throw Failure{TESSERA_IMPLEMENTATION_UNAVAILABLE, TESSERA_PHASE_CREATE};
}
} // namespace tessera
