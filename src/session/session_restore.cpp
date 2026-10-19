#include "session/session.hpp"

#include "persistence/state_format.hpp"

#include <algorithm>

namespace tessera {
namespace {
bool equal(wire::View left, wire::View right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin());
}
} // namespace

Bytes Session::checkpoint() const {
  require(machine->capabilities() & TESSERA_CAP_CHECKPOINT,
          TESSERA_UNSUPPORTED_CAPABILITY, TESSERA_PHASE_READ);
  return persistence::encode_checkpoint(state(), replay_raw,
                                        replay ? replay_next : 0, initial_sha);
}

void Session::restore(wire::View encoded, wire::View trace_bytes,
                      tessera_context_v1 &context) {
  require(epoch < UINT64_MAX && generation < UINT64_MAX,
          TESSERA_BUDGET_EXCEEDED, TESSERA_PHASE_RESTORE);
  persistence::CheckpointData checkpoint;
  persistence::ParsedState state_data;
  checkpoint = persistence::decode_checkpoint(encoded);
  state_data = persistence::decode_state(
      checkpoint.state, backend, content_sha, build, config_sha,
      machine->frame_ticks());
  require(equal(checkpoint.initial_sha, initial_sha),
          TESSERA_CHECKPOINT_INCOMPATIBLE, TESSERA_PHASE_RESTORE);

  std::optional<wire::Trace> next_trace;
  Bytes next_raw;
  if (checkpoint.trace_length != 0) {
    require(!trace_bytes.empty(), TESSERA_INVALID_TRACE,
            TESSERA_PHASE_RESTORE);
    auto candidate = wire::trace(trace_bytes);
    require(candidate.frames.size() == checkpoint.trace_length &&
                equal(wire::sha(trace_bytes), checkpoint.trace_sha) &&
                equal(candidate.content_sha, content_sha) &&
                equal(candidate.config_sha, config_sha),
            TESSERA_CHECKPOINT_INCOMPATIBLE, TESSERA_PHASE_RESTORE);
    require(state_data.completed_frames < candidate.frames.size() + 1 &&
                checkpoint.next_replay_frame ==
                    state_data.completed_frames + 1,
            TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);
    const InputState expected =
        state_data.completed_frames
            ? candidate.frames[static_cast<std::size_t>(
                  state_data.completed_frames - 1)]
            : InputState{};
    require(state_data.inputs == expected, TESSERA_CHECKPOINT_INVALID,
            TESSERA_PHASE_RESTORE);
    next_raw.assign(trace_bytes.begin(), trace_bytes.end());
    next_trace = std::move(candidate);
  } else {
    require(trace_bytes.empty(), TESSERA_CHECKPOINT_INCOMPATIBLE,
            TESSERA_PHASE_RESTORE);
  }

  auto candidate = SystemRegistry::create(backend, content, memory);
  candidate->restore_record(state_data.machine, state_data.completed_frames,
                            state_data.inputs);
  require(candidate->tick() == state_data.logical_tick &&
              candidate->machine_record() == state_data.machine,
          TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_RESTORE);

  // All parsing, allocation and backend reconstruction precede this commit.
  cancel(context, TESSERA_CANCEL_RESTORE);
  machine.swap(candidate);
  replay.swap(next_trace);
  replay_raw.swap(next_raw);
  replay_next = replay ? checkpoint.next_replay_frame : 1;
  ++epoch;
  ++generation;
  latest.reset();
  if (!(context.flags & TESSERA_CONTEXT_CONTROL_CANCELLED))
    context.frame_id = identity();
}

} // namespace tessera
