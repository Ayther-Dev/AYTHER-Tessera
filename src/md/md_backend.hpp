// Núcleo Mega Drive propio de Tessera (spec 008). Solo se compila con
// TESSERA_BUILD_MEGADRIVE. TP-101.1 aporta el esqueleto registrado; el
// emulador llega en TP-101.2 y siguientes.
#pragma once
#include "domain/wire.hpp"
#include "systems/emulated_system.hpp"

#include <memory>
#include <string_view>

namespace tessera::md {

// Fábrica registrada como backend `megadrive`. Hasta que exista la máquina
// (TP-101.2+) informa de que la implementación no está disponible.
std::unique_ptr<IEmulatedSystem>
make_backend(std::string_view id, wire::View content, wire::View memory);

} // namespace tessera::md
