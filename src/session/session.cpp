#include "session/session.hpp"
#include "api/call.hpp"
#include <algorithm>

namespace tessera {
using namespace wire;
Session::Session(std::string id, View rom, View cfg, View pm, View build_id)
    : backend(std::move(id)), content(rom.begin(), rom.end()),
      config(cfg.begin(), cfg.end()), memory(pm.begin(), pm.end()),
      content_sha(sha(rom)), config_sha(sha(cfg)),
      build(build_id.begin(), build_id.end()) {
  machine = SystemRegistry::create(backend, content, memory);
  require(config == configuration(backend, build, memory),
          TESSERA_INVALID_ARGUMENT, TESSERA_PHASE_CREATE);
  reset_fault = SystemRegistry::initial_reset_fault(backend, content);
  initial_sha = sha(state());
}
tessera_frame_id_v1 Session::identity() const {
  return {handle, epoch, machine->frame()};
}
tessera_session_info_v1 Session::info() const {
  auto v = api::initialized<tessera_session_info_v1>();
  v.frame_id = identity();
  v.generation = generation;
  v.logical_tick = machine->tick();
  v.tick_hz_num = machine->tick_hz();
  v.tick_hz_den = 1;
  v.capabilities = machine->capabilities();
  v.state = TESSERA_SESSION_PAUSED;
  v.system = machine->system();
  std::copy(content_sha.begin(), content_sha.end(), v.content_sha256);
  std::copy(build.begin(), build.end(), v.backend_build_id);
  v.pending_transaction = pending ? pending->id : 0;
  v.next_replay_frame = replay_next;
  return v;
}
Bytes Session::state() const {
  Bytes b = text("TSSTATE1");
  field(b, 1, 6, text(machine->system() ? "tessera.megadrive.clown" : "tessera.synthetic"));
  scalar(b, 2, 1, 1);
  scalar(b, 3, 1, 0);
  field(b, 4, 5, content_sha);
  field(b, 5, 5, build);
  field(b, 6, 5, config_sha);
  scalar(b, 7, 3, machine->frame());
  scalar(b, 8, 3, machine->tick());
  field(b, 9, 8, inputs(machine->latched_inputs()));
  field(b, 65536, 11, machine->machine_record());
  return b;
}
void Session::validate_target(std::uint64_t e, std::uint64_t f) const {
  require(e == epoch, TESSERA_STALE_EPOCH);
  require(f > machine->frame(), TESSERA_INPUT_LATE);
  require(machine->frame() < UINT64_MAX / 1000 && f == machine->frame() + 1,
          TESSERA_INPUT_OUT_OF_ORDER);
}
InputState Session::validate_inputs(
    std::span<const tessera_input_assignment_v1> assignments) const {
  InputState state{};
  std::array<bool, 8> seen{};
  for (const auto &a : assignments) {
    require(a.port == 0 && a.control < 8 && a.value >= 0 && a.value <= 1);
    if (seen[a.control])
      throw Failure{state[a.control] == a.value ? TESSERA_INPUT_DUPLICATE
                                                : TESSERA_INPUT_CONTRADICTORY};
    seen[a.control] = true;
    state[a.control] = a.value;
  }
  require(std::all_of(seen.begin(), seen.end(), [](bool b) { return b; }),
          TESSERA_INPUT_INCOMPLETE);
  return state;
}
void Session::step(const InputState &input, tessera_context_v1 &ctx) {
  // The backend prepares output buffers before committing its machine state.
  machine->prepare_observation(pending ? pending->regions : region_mask);
  machine->step(input);
  ++generation;
  if (pending) {
    region_mask = pending->regions;
    event_mask = pending->events;
    ctx.transaction = pending->id;
    ctx.flags = TESSERA_CONTEXT_CONTROLS_APPLIED;
    pending.reset();
    emit(2);
  }
  ctx.frame_id = identity();
  emit(3);
  capture();
}
void Session::reset(tessera_context_v1 &ctx) {
  require(epoch < UINT64_MAX && generation < UINT64_MAX,
          TESSERA_BUDGET_EXCEEDED, TESSERA_PHASE_RESET);
  if (reset_fault) {
    reset_fault = false;
    throw Failure{TESSERA_BACKEND_FAILURE, TESSERA_PHASE_RESET};
  }
  auto candidate = SystemRegistry::create(backend, content, memory);
  cancel(ctx, TESSERA_CANCEL_RESET);
  machine = std::move(candidate);
  ++epoch;
  ++generation;
  region_mask = event_mask = 0;
  latest.reset();
  replay_next = 1;
}
} // namespace tessera
