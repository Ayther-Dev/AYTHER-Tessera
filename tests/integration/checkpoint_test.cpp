#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "support/fx01.hpp"

#include <array>

namespace {
fx01::Bytes export_checkpoint(fx01::Session &session) {
  std::uint64_t required{};
  CHECK(tessera_checkpoint_export_v1(session.handle, {nullptr, 0}, &required,
                                     &session.context) ==
        TESSERA_BUFFER_TOO_SMALL);
  fx01::Bytes result(static_cast<std::size_t>(required));
  fx01::expect(tessera_checkpoint_export_v1(
      session.handle, {result.data(), result.size()}, &required,
      &session.context));
  return result;
}
tessera_status restore(fx01::Session &session, fx01::View checkpoint,
                       fx01::View trace) {
  return tessera_checkpoint_restore_v1(session.handle, fx01::view(checkpoint),
                                       fx01::view(trace), &session.context);
}
struct Held {
  tessera_snapshot handle{};
  fx01::Session &owner;
  explicit Held(fx01::Session &session) : owner(session) {
    fx01::expect(tessera_snapshot_acquire_v1(owner.handle, &handle,
                                            &owner.context));
  }
  ~Held() {
    if (handle) tessera_snapshot_release_v1(handle, &owner.context);
  }
  tessera_snapshot_info_v1 info() const {
    auto result = fx01::init<tessera_snapshot_info_v1>();
    fx01::expect(tessera_snapshot_query_v1(handle, &result, &owner.context));
    return result;
  }
};
} // namespace

TEST_CASE("FX-01 restore commits only after validation and resumes at K+1") {
  fx01::Session source;
  const std::array<unsigned, 4> inputs{0, 32, 0, 64};
  const auto trace = fx01::trace(source.rom, source.cfg, inputs);
  fx01::expect(tessera_session_bind_trace_v1(source.handle, fx01::view(trace),
                                             &source.context));
  fx01::expect(source.observe(70));
  fx01::expect(tessera_session_replay_next_v1(source.handle, &source.context));
  fx01::expect(tessera_session_replay_next_v1(source.handle, &source.context));
  const auto after_k = source.info();
  CHECK(after_k.frame_id.frame == 2);
  CHECK(after_k.next_replay_frame == 3);
  const auto encoded = export_checkpoint(source);
  fx01::expect(tessera_session_replay_next_v1(source.handle, &source.context));
  const auto next_state = source.state();
  const auto next_video = source.original();
  const fx01::Bytes next_pixels(next_video.pixels.data,
                                next_video.pixels.data + next_video.pixels.bytes);
  const fx01::Bytes next_pcm(next_video.pcm.data,
                             next_video.pcm.data + next_video.pcm.bytes);

  fx01::Session target;
  fx01::expect(target.step(64));
  fx01::expect(target.observe(81));
  Held retained(target);
  const auto historical = retained.info();
  const auto before = target.info();
  const auto before_state = target.state();
  const auto exported_pending = export_checkpoint(target);
  CHECK_FALSE(exported_pending.empty());
  CHECK(target.info().pending_transaction == 81);

  auto corrupt = encoded;
  corrupt.back() ^= 1;
  CHECK(restore(target, corrupt, trace) == TESSERA_CHECKPOINT_INVALID);
  CHECK(target.info().frame_id.epoch == before.frame_id.epoch);
  CHECK(target.info().pending_transaction == 81);
  CHECK(target.state() == before_state);
  CHECK(retained.info().frame_id.frame == historical.frame_id.frame);

  auto wrong_trace = trace;
  wrong_trace.back() ^= 1;
  CHECK(restore(target, encoded, wrong_trace) != TESSERA_OK);
  CHECK(target.info().pending_transaction == 81);
  CHECK(target.state() == before_state);

  fx01::expect(restore(target, encoded, trace));
  const auto cancelled = target.context;
  CHECK(cancelled.transaction == 81);
  CHECK(cancelled.cancel_reason == TESSERA_CANCEL_RESTORE);
  CHECK((cancelled.flags & TESSERA_CONTEXT_CONTROL_CANCELLED) != 0);
  const auto restored = target.info();
  CHECK(restored.frame_id.epoch == before.frame_id.epoch + 1);
  CHECK(restored.frame_id.frame == 2);
  CHECK(restored.next_replay_frame == 3);
  CHECK(restored.pending_transaction == 0);
  CHECK(retained.info().frame_id.frame == historical.frame_id.frame);
  auto unavailable = fx01::init<tessera_original_frame_v1>();
  CHECK(tessera_original_frame_query_v1(target.handle, &unavailable,
                                         &target.context) ==
        TESSERA_WRONG_STATE);
  fx01::expect(tessera_session_replay_next_v1(target.handle,
                                              &target.context));
  CHECK(target.info().frame_id.frame == 3);
  CHECK(target.state() == next_state);
  const auto actual = target.original();
  CHECK(fx01::Bytes(actual.pixels.data,
                    actual.pixels.data + actual.pixels.bytes) == next_pixels);
  CHECK(fx01::Bytes(actual.pcm.data, actual.pcm.data + actual.pcm.bytes) ==
        next_pcm);
  const auto epoch_after_replay = target.info().frame_id.epoch;
  fx01::expect(tessera_session_reset_v1(target.handle, &target.context));
  const auto reset = target.info();
  CHECK(reset.frame_id.epoch == epoch_after_replay + 1);
  CHECK(reset.frame_id.frame == 0);
  CHECK(reset.next_replay_frame == 1);
  fx01::Session fresh;
  CHECK(target.state() == fresh.state());
  CHECK(tessera_original_frame_query_v1(target.handle, &unavailable,
                                         &target.context) ==
        TESSERA_WRONG_STATE);
}

TEST_CASE("Restore cancellation survives a saturated event queue") {
  fx01::Session session;
  fx01::expect(session.observe(91));
  fx01::expect(session.step());
  const auto encoded = export_checkpoint(session);
  for (unsigned frame = 2; frame <= 2200; ++frame)
    fx01::expect(session.step());
  fx01::expect(session.observe(92));
  const auto before = session.info();
  CHECK(before.pending_transaction == 92);
  auto corrupt = encoded;
  corrupt.back() ^= 1;
  CHECK(restore(session, corrupt, {}) == TESSERA_CHECKPOINT_INVALID);
  CHECK(session.info().pending_transaction == 92);
  fx01::expect(restore(session, encoded, {}));
  CHECK(session.context.transaction == 92);
  CHECK(session.context.cancel_reason == TESSERA_CANCEL_RESTORE);
  CHECK((session.context.flags & TESSERA_CONTEXT_CONTROL_CANCELLED) != 0);
  const auto restored = session.info();
  CHECK(restored.frame_id.epoch == before.frame_id.epoch + 1);
  CHECK(restored.frame_id.frame == 1);
  CHECK(restored.pending_transaction == 0);
}
