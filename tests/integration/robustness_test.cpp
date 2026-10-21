#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "support/fx01.hpp"

#include <array>

TEST_CASE("Invalid frames and consecutive controls preserve the next valid frame") {
  fx01::Session session;
  REQUIRE(session.observe(101) == TESSERA_OK);
  CHECK(session.observe(102) == TESSERA_CONTROL_PENDING);
  const auto before = session.state();
  const auto next = session.info().frame_id.frame + 1;
  auto complete = fx01::inputs(0);
  auto invalid = fx01::init<tessera_input_frame_v1>();
  invalid.epoch = session.info().frame_id.epoch;
  invalid.frame = next;
  invalid.assignments = complete.data();
  invalid.count = 7;
  CHECK(tessera_session_step_v1(session.handle, &invalid, &session.context) ==
        TESSERA_INPUT_INCOMPLETE);
  CHECK(session.info().frame_id.frame == next - 1);
  CHECK(session.info().pending_transaction == 101);
  CHECK(session.state() == before);
  invalid.count = 8;
  REQUIRE(tessera_session_step_v1(session.handle, &invalid, &session.context) ==
          TESSERA_OK);
  CHECK(session.context.transaction == 101);
  CHECK((session.context.flags & TESSERA_CONTEXT_CONTROLS_APPLIED) != 0);
  CHECK(session.info().frame_id.frame == next);
  CHECK(session.info().pending_transaction == 0);
}

TEST_CASE("Malformed replay never advances, and EOF remains stable") {
  fx01::Session session;
  const std::array<unsigned, 1> controls{0};
  const auto good = fx01::trace(session.rom, session.cfg, controls);
  auto bad = good;
  bad.pop_back();
  CHECK(tessera_session_bind_trace_v1(session.handle, fx01::view(bad),
                                      &session.context) == TESSERA_INVALID_TRACE);
  CHECK(session.info().frame_id.frame == 0);
  REQUIRE(tessera_session_bind_trace_v1(session.handle, fx01::view(good),
                                        &session.context) == TESSERA_OK);
  REQUIRE(tessera_session_replay_next_v1(session.handle, &session.context) ==
          TESSERA_OK);
  const auto after = session.state();
  CHECK(tessera_session_replay_next_v1(session.handle, &session.context) ==
        TESSERA_END_OF_TRACE);
  CHECK(tessera_session_replay_next_v1(session.handle, &session.context) ==
        TESSERA_END_OF_TRACE);
  CHECK(session.state() == after);
  CHECK(session.info().frame_id.frame == 1);
}

TEST_CASE("Retained capture survives a saturated producer and session close") {
  fx01::Session session;
  REQUIRE(session.observe(201) == TESSERA_OK);
  REQUIRE(session.step() == TESSERA_OK);
  tessera_snapshot held{};
  REQUIRE(tessera_snapshot_acquire_v1(session.handle, &held, &session.context) ==
          TESSERA_OK);
  auto historical = fx01::init<tessera_snapshot_info_v1>();
  REQUIRE(tessera_snapshot_query_v1(held, &historical, &session.context) ==
          TESSERA_OK);
  for (unsigned index = 0; index < 2200; ++index)
    REQUIRE(session.step() == TESSERA_OK);
  auto still_held = fx01::init<tessera_snapshot_info_v1>();
  REQUIRE(tessera_snapshot_query_v1(held, &still_held, &session.context) ==
          TESSERA_OK);
  CHECK(still_held.frame_id.frame == historical.frame_id.frame);
  REQUIRE(tessera_session_close_v1(session.handle, &session.context) == TESSERA_OK);
  session.handle = 0;
  auto after_close = fx01::init<tessera_snapshot_info_v1>();
  CHECK(tessera_snapshot_query_v1(held, &after_close, &session.context) ==
        TESSERA_OK);
  CHECK(after_close.frame_id.frame == historical.frame_id.frame);
  CHECK(tessera_snapshot_release_v1(held, &session.context) == TESSERA_OK);
}
