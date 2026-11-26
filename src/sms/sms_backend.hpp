// Native SMS / Game Gear / SG-1000 backend and ObservationSource
// (spec 008, Phase 6 / TP-601.5).
#pragma once
#include "domain/wire.hpp"
#include "systems/emulated_system.hpp"

#include <memory>
#include <string_view>

namespace tessera::sms {

std::unique_ptr<IEmulatedSystem>
make_backend(std::string_view id, wire::View content, wire::View memory);

} // namespace tessera::sms
