#include "md/md_backend.hpp"

#include "domain/wire.hpp"

namespace tessera::md {

std::unique_ptr<IEmulatedSystem> make_backend(std::string_view, wire::View,
                                              wire::View) {
  throw Failure{TESSERA_IMPLEMENTATION_UNAVAILABLE, TESSERA_PHASE_CREATE};
}

} // namespace tessera::md
