#pragma once
#include "domain/wire.hpp"
#include <cstddef>
#include <memory>
#ifndef TESSERA_CLOWN_REFERENCE
#define TESSERA_CLOWN_REFERENCE 0
#endif
#ifndef TESSERA_MEGADRIVE
#define TESSERA_MEGADRIVE 0
#endif
namespace tessera {
// The distributed core registers the native Mega Drive core (megadrive, first
// and the default for Mega Drive content, TP-112.1) and the two synthetic test
// backends. The ClownMDEmu reference backend (AGPL-3.0) is present only in the
// local reference build (TESSERA_BUILD_CLOWN_REFERENCE), after megadrive, and
// is selected only by its id.
inline constexpr std::size_t kRegistrationCount =
    2 + (TESSERA_CLOWN_REFERENCE ? 1 : 0) + (TESSERA_MEGADRIVE ? 1 : 0);
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
