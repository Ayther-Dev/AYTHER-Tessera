#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/fx01.hpp"
#include <doctest/doctest.h>

TEST_CASE("Session starts paused and advances exactly one complete frame") {
  fx01::Session s;
  auto i = s.info();
  CHECK(i.state == TESSERA_SESSION_PAUSED);
  CHECK(i.frame_id.frame == 0);
  CHECK(i.logical_tick == 0);
  CHECK(i.tick_hz_num == 60000);
  CHECK(i.tick_hz_den == 1);
  auto frame = fx01::init<tessera_original_frame_v1>();
  CHECK(tessera_original_frame_query_v1(s.handle, &frame, &s.context) ==
        TESSERA_WRONG_STATE);
  fx01::expect(tessera_session_pause_v1(s.handle, &s.context));
  CHECK(s.info().frame_id.frame == 0);
  fx01::expect(s.step(32));
  i = s.info();
  CHECK(i.frame_id.frame == 1);
  CHECK(i.logical_tick == 1000);
  CHECK(i.state == TESSERA_SESSION_PAUSED);
  const auto original = s.original();
  CHECK(original.width == 16);
  CHECK(original.height == 16);
  CHECK(original.pcm_frames == 800);
  const auto state = s.state();
  fx01::expect(tessera_session_pause_v1(s.handle, &s.context));
  CHECK(s.state() == state);
}
TEST_CASE("Invalid complete inputs preserve machine and pending transaction") {
  fx01::Session s;
  fx01::expect(s.observe(101));
  const auto before = s.state();
  const auto initial = s.info();
  auto valid = fx01::inputs();
  auto try_frame = [&](std::uint64_t epoch, std::uint64_t frame,
                       std::span<const tessera_input_assignment_v1> entries,
                       tessera_status expected) {
    auto in = fx01::init<tessera_input_frame_v1>();
    in.epoch = epoch;
    in.frame = frame;
    in.assignments = entries.data();
    in.count = static_cast<std::uint32_t>(entries.size());
    CHECK(tessera_session_step_v1(s.handle, &in, &s.context) == expected);
    CHECK(s.state() == before);
    CHECK(s.info().pending_transaction == 101);
    CHECK(s.info().generation == initial.generation);
  };
  try_frame(initial.frame_id.epoch + 1, 1, valid, TESSERA_STALE_EPOCH);
  try_frame(initial.frame_id.epoch, 0, valid, TESSERA_INPUT_LATE);
  try_frame(initial.frame_id.epoch, 2, valid, TESSERA_INPUT_OUT_OF_ORDER);
  try_frame(initial.frame_id.epoch, 1, {}, TESSERA_INPUT_INCOMPLETE);
  try_frame(initial.frame_id.epoch, 1, std::span(valid).first(7),
            TESSERA_INPUT_INCOMPLETE);
  auto bad = valid;
  bad[7] = bad[0];
  try_frame(initial.frame_id.epoch, 1, bad, TESSERA_INPUT_DUPLICATE);
  bad = valid;
  bad[7] = bad[0];
  bad[7].value = 1;
  try_frame(initial.frame_id.epoch, 1, bad, TESSERA_INPUT_CONTRADICTORY);
  bad = valid;
  bad[0].value = -1;
  try_frame(initial.frame_id.epoch, 1, bad, TESSERA_INVALID_ARGUMENT);
  bad = valid;
  bad[0].value = 2;
  try_frame(initial.frame_id.epoch, 1, bad, TESSERA_INVALID_ARGUMENT);
  bad = valid;
  bad[0].port = 1;
  try_frame(initial.frame_id.epoch, 1, bad, TESSERA_INVALID_ARGUMENT);
  bad = valid;
  bad[0].control = 8;
  try_frame(initial.frame_id.epoch, 1, bad, TESSERA_INVALID_ARGUMENT);
  // The digital map has no directional-exclusion policy.
  fx01::expect(s.step(15));
  CHECK(s.context.transaction == 101);
  CHECK((s.context.flags & TESSERA_CONTEXT_CONTROLS_APPLIED) != 0);
}
TEST_CASE("Reset reconstructs recorded bytes and neutral controls; caller "
          "buffers are copied") {
  fx01::Session s;
  const auto initial = s.state();
  const auto epoch = s.info().frame_id.epoch;
  std::fill(s.rom.begin(), s.rom.end(), 0);
  std::fill(s.pm.begin(), s.pm.end(), 0);
  std::fill(s.cfg.begin(), s.cfg.end(), 0);
  fx01::expect(s.observe(1));
  fx01::expect(s.step(32));
  fx01::expect(s.observe(2));
  fx01::expect(tessera_session_reset_v1(s.handle, &s.context));
  CHECK(s.context.transaction == 2);
  CHECK(s.context.cancel_reason == TESSERA_CANCEL_RESET);
  CHECK((s.context.flags & TESSERA_CONTEXT_CONTROL_CANCELLED) != 0);
  const auto now = s.info();
  CHECK(now.frame_id.epoch == epoch + 1);
  CHECK(now.frame_id.frame == 0);
  CHECK(now.logical_tick == 0);
  CHECK(now.state == TESSERA_SESSION_PAUSED);
  CHECK(now.pending_transaction == 0);
  CHECK(s.state() == initial);
  auto unavailable = fx01::init<tessera_original_frame_v1>();
  CHECK(tessera_original_frame_query_v1(s.handle, &unavailable, &s.context) ==
        TESSERA_WRONG_STATE);
  fx01::expect(s.step());
  tessera_snapshot snapshot{};
  fx01::expect(tessera_snapshot_acquire_v1(s.handle, &snapshot, &s.context));
  auto captured = fx01::init<tessera_snapshot_info_v1>();
  fx01::expect(tessera_snapshot_query_v1(snapshot, &captured, &s.context));
  CHECK(captured.region_mask == 0);
  fx01::expect(tessera_snapshot_release_v1(snapshot, &s.context));
}
TEST_CASE("Injected reset failure preserves all public state and the accepted "
          "transaction") {
  fx01::Session s(true, 1);
  fx01::expect(s.observe(11));
  fx01::expect(s.step(32));
  fx01::expect(s.observe(12));
  const auto state = s.state();
  const auto before = s.info();
  const auto frame = s.original();
  const fx01::Bytes video(frame.pixels.data,
                          frame.pixels.data + frame.pixels.bytes),
      pcm(frame.pcm.data, frame.pcm.data + frame.pcm.bytes);
  CHECK(tessera_session_reset_v1(s.handle, &s.context) ==
        TESSERA_BACKEND_FAILURE);
  CHECK(s.context.phase == TESSERA_PHASE_RESET);
  CHECK(s.context.transaction == 0);
  CHECK(s.state() == state);
  CHECK(fx01::Bytes(frame.pixels.data,
                    frame.pixels.data + frame.pixels.bytes) == video);
  CHECK(fx01::Bytes(frame.pcm.data, frame.pcm.data + frame.pcm.bytes) == pcm);
  const auto after = s.info();
  CHECK(after.frame_id.epoch == before.frame_id.epoch);
  CHECK(after.frame_id.frame == before.frame_id.frame);
  CHECK(after.generation == before.generation);
  CHECK(after.pending_transaction == 12);
  const auto same = s.original();
  CHECK(fx01::Bytes(same.pixels.data, same.pixels.data + same.pixels.bytes) ==
        video);
  CHECK(fx01::Bytes(same.pcm.data, same.pcm.data + same.pcm.bytes) == pcm);
  fx01::expect(tessera_session_reset_v1(s.handle, &s.context));
  CHECK(s.context.transaction == 12);
  CHECK(s.context.cancel_reason == TESSERA_CANCEL_RESET);
  CHECK(s.info().frame_id.frame == 0);
}
TEST_CASE("Whole trace is validated before bind; EOF preserves cursor and a "
          "pending transaction") {
  fx01::Session s;
  const std::array<unsigned, 2> masks{0, 32};
  const auto trace = fx01::trace(s.rom, s.cfg, masks);
  fx01::expect(tessera_trace_validate_v1(fx01::view(trace), &s.context));
  fx01::expect(
      tessera_session_bind_trace_v1(s.handle, fx01::view(trace), &s.context));
  auto broken = trace;
  broken.pop_back();
  const auto before = s.state();
  CHECK(tessera_session_bind_trace_v1(s.handle, fx01::view(broken),
                                      &s.context) == TESSERA_INVALID_TRACE);
  CHECK(s.state() == before);
  fx01::expect(tessera_session_replay_next_v1(s.handle, &s.context));
  fx01::expect(tessera_session_replay_next_v1(s.handle, &s.context));
  CHECK(s.info().frame_id.frame == 2);
  fx01::expect(s.observe(31));
  const auto end = s.state();
  const auto cursor = s.info().next_replay_frame;
  CHECK(tessera_session_replay_next_v1(s.handle, &s.context) ==
        TESSERA_END_OF_TRACE);
  CHECK(s.state() == end);
  CHECK(s.info().next_replay_frame == cursor);
  CHECK(s.info().pending_transaction == 31);
  fx01::expect(tessera_session_reset_v1(s.handle, &s.context));
  CHECK(s.info().next_replay_frame == 1);
  fx01::expect(tessera_session_replay_next_v1(s.handle, &s.context));
  CHECK(s.info().frame_id.frame == 1);
}
TEST_CASE("Capabilities absent in minimal and expired handles fail without "
          "mutation") {
  fx01::Session s(false);
  fx01::expect(s.step());
  const auto before = s.state();
  tessera_snapshot h = 17;
  CHECK(tessera_snapshot_acquire_v1(s.handle, &h, &s.context) ==
        TESSERA_UNSUPPORTED_CAPABILITY);
  CHECK(h == 17);
  const std::array<tessera_control_item_v1, 1> control{{{1, 1, 0}}};
  CHECK(s.submit(2, control) == TESSERA_UNSUPPORTED_CAPABILITY);
  std::uint64_t n{};
  CHECK(tessera_checkpoint_export_v1(s.handle, {nullptr, 0}, &n, &s.context) ==
        TESSERA_BUFFER_TOO_SMALL);
  CHECK(n > 64);
  CHECK(s.state() == before);
  const auto old = s.handle;
  fx01::expect(tessera_session_close_v1(s.handle, &s.context));
  s.handle = 0;
  CHECK(tessera_session_close_v1(old, &s.context) == TESSERA_INVALID_HANDLE);
  fx01::Session next;
  CHECK(next.handle != old);
  auto info = fx01::init<tessera_session_info_v1>();
  CHECK(tessera_session_query_v1(old, &info, &s.context) ==
        TESSERA_INVALID_HANDLE);
}

TEST_CASE("Failed creation validates profile memory and canonical "
          "configuration before publication") {
  fx01::Session live;
  fx01::expect(live.step(32));
  const auto previous = live.state();
  const auto info = fx01::backend();
  const auto id = fx01::str(info.id);
  const auto rom = fx01::content(2), pm = fx01::memory(),
             cfg = fx01::config(info, pm);
  auto creation = fx01::init<tessera_session_create_info_v1>();
  creation.backend_id = fx01::view(id);
  creation.content = fx01::view(rom);
  creation.config = fx01::view(cfg);
  creation.initial_persistent_memory = fx01::view(pm);
  auto altered = cfg;
  altered.back() ^= 1;
  creation.config = fx01::view(altered);
  tessera_session output = 123;
  CHECK(tessera_session_create_v1(&creation, &output, &live.context) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(output == 123);
  creation.config = fx01::view(cfg);
  creation.initial_persistent_memory = fx01::view(fx01::View(pm).first(15));
  CHECK(tessera_session_create_v1(&creation, &output, &live.context) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(output == 123);
  creation.initial_persistent_memory = fx01::view(pm);
  const auto wrong_profile = fx01::content(1);
  creation.content = fx01::view(wrong_profile);
  CHECK(tessera_session_create_v1(&creation, &output, &live.context) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(output == 123);
  CHECK(live.state() == previous);
}

TEST_CASE("Whole canonical traces reject duplicate contradictory missing "
          "reordered and incompatible input before binding") {
  fx01::Session session;
  const auto before = session.state();
  for (unsigned variant = 0; variant < 6; ++variant) {
    std::vector<fx01::Bytes> entries;
    for (const auto &assignment : fx01::inputs()) {
      fx01::Bytes e;
      fx01::scalar(e, 1, 1, assignment.port);
      fx01::scalar(e, 2, 1, assignment.control);
      fx01::scalar(e, 3, 2, assignment.value);
      entries.push_back(e);
    }
    if (variant == 0)
      entries[7] = entries[0];
    if (variant == 1) {
      entries[7] = entries[0];
      entries[7][entries[7].size() - 4] = 1;
    }
    if (variant == 2)
      entries.pop_back();
    if (variant == 3)
      std::swap(entries[0], entries[1]);
    fx01::Bytes frame;
    fx01::scalar(frame, 1, 3, variant == 4 ? 2 : 1);
    fx01::field(frame, 2, 8, fx01::array(entries));
    auto trace = fx01::str("TSTRACE1");
    auto content_sha = fx01::sha(session.rom);
    if (variant == 5)
      content_sha[0] ^= 1;
    fx01::field(trace, 1, 5, content_sha);
    fx01::field(trace, 2, 5, fx01::sha(session.cfg));
    fx01::scalar(trace, 3, 3, 1);
    fx01::field(trace, 4, 8, fx01::array({frame}));
    CHECK(tessera_session_bind_trace_v1(session.handle, fx01::view(trace),
                                        &session.context) ==
          TESSERA_INVALID_TRACE);
    CHECK(session.state() == before);
    CHECK(session.info().next_replay_frame == 1);
  }
}

TEST_CASE("Checkpoint export and invalid restore preserve the accepted transaction and "
          "borrowed original views") {
  fx01::Session session;
  fx01::expect(session.step(32));
  fx01::expect(session.observe(91));
  const auto before = session.state();
  const auto original = session.original();
  const fx01::Bytes pixels(original.pixels.data,
                           original.pixels.data + original.pixels.bytes);
  std::uint64_t required = 123;
  CHECK(tessera_checkpoint_export_v1(session.handle, {nullptr, 0}, &required,
                                     &session.context) ==
        TESSERA_BUFFER_TOO_SMALL);
  CHECK(required > 64);
  CHECK(session.context.phase == TESSERA_PHASE_READ);
  CHECK(tessera_checkpoint_restore_v1(session.handle, {nullptr, 0},
                                      {nullptr, 0}, &session.context) ==
        TESSERA_CHECKPOINT_INVALID);
  CHECK(session.context.phase == TESSERA_PHASE_RESTORE);
  CHECK(session.state() == before);
  CHECK(session.info().pending_transaction == 91);
  CHECK(fx01::Bytes(original.pixels.data,
                    original.pixels.data + original.pixels.bytes) == pixels);
}
