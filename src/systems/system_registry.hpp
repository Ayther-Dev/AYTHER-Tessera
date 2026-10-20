#pragma once
#include "domain/wire.hpp"
#include <memory>
namespace tessera {
struct SystemRegistration {
  std::string_view id;
  std::uint32_t kind, system;
  std::uint64_t capabilities;
  std::unique_ptr<IEmulatedSystem> (*factory)(std::string_view, wire::View,
                                              wire::View);
};
class SystemRegistry {
public:
  [[nodiscard]] static std::span<const SystemRegistration> registrations();
  [[nodiscard]] static std::unique_ptr<IEmulatedSystem>
  create(std::string_view id, wire::View content, wire::View memory);
  [[nodiscard]] static bool initial_reset_fault(std::string_view id,
                                                wire::View content);
  [[nodiscard]] static std::string resolve(std::string_view requested,
                                           wire::View content,
                                           std::uint32_t system_override);
};
} // namespace tessera
