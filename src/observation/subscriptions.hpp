// Suscripciones de observación (spec 008, TP-201.3, RF-3.8/RF-3.9).
//
// Medido por caja negra sobre gpgx r4 (tests/fixtures/diffbench/
// subscription-matrix.json, tools/diffbench/measure_subscriptions.py):
//  - arranca con active = requested = 0; supported = TESSERA_OBS_SUB_ALL;
//  - una máscara pedida entre frames pasa a activa al empezar el siguiente
//    frame; los bits desconocidos se rechazan y no cambian nada;
//  - activation_frame es el número (desde 1) del último frame empezado; entre
//    una petición que cambia la máscara y ese frame vale el número del
//    siguiente frame; una petición de 0 sobre 0 no lo toca.
// Observar no altera el estado emulado: esta clase no entra en el savestate.
#pragma once
#include "tessera/tessera_observation.h"

#include <cstdint>

namespace tessera::observation {

struct SubscriptionSnapshot {
  std::uint32_t supported = TESSERA_OBS_SUB_ALL;
  std::uint32_t active = 0;
  std::uint32_t requested = 0;
  std::uint64_t activation_frame = 0;
};

class Subscriptions {
public:
  /// False (and nothing changes) if the mask has bits the contract lacks.
  bool request(std::uint32_t mask) noexcept {
    if ((mask & ~TESSERA_OBS_SUB_ALL) != 0)
      return false;
    if (mask != active_)
      activation_frame_ = frames_started_ + 1;
    requested_ = mask;
    return true;
  }
  /// The start of a frame: the requested mask becomes the active one.
  void begin_frame() noexcept {
    ++frames_started_;
    active_ = requested_;
    activation_frame_ = frames_started_;
  }
  [[nodiscard]] bool active(std::uint32_t bit) const noexcept {
    return (active_ & bit) != 0;
  }
  [[nodiscard]] SubscriptionSnapshot snapshot() const noexcept {
    return {TESSERA_OBS_SUB_ALL, active_, requested_, activation_frame_};
  }

private:
  std::uint32_t active_ = 0, requested_ = 0;
  std::uint64_t frames_started_ = 0, activation_frame_ = 0;
};

} // namespace tessera::observation
