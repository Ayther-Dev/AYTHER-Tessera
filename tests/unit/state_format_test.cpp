#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "persistence/state_format.hpp"
#include "support/fx01.hpp"

namespace {
fx01::Bytes checkpoint(fx01::Session &session) {
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
} // namespace

TEST_CASE("Versioned checkpoint round trips explicit state and excludes pending controls") {
  fx01::Session session;
  fx01::expect(session.step());
  fx01::expect(session.observe(44));
  const auto before = session.state();
  const auto bytes = checkpoint(session);
  const auto parsed = tessera::persistence::decode_checkpoint(bytes);
  CHECK(parsed.state == before);
  CHECK(parsed.trace_length == 0);
  CHECK(parsed.next_replay_frame == 0);
  CHECK(parsed.trace_sha == fx01::Bytes(32, 0));
  CHECK(session.info().pending_transaction == 44);
  // The five documented fields contain no transaction or presentation record.
  const auto backend = fx01::backend();
  const auto state = tessera::persistence::decode_state(
      parsed.state, "synthetic.complete", fx01::sha(session.rom),
      backend.build_id, fx01::sha(session.cfg), 1000);
  CHECK(state.completed_frames == 1);
  CHECK(state.logical_tick == 1000);
  CHECK(state.machine.size() > 0);
}

TEST_CASE("Checkpoint rejects truncated, mutated and incompatible headers before state use") {
  fx01::Session session;
  fx01::expect(session.step());
  const auto before = session.state();
  auto bytes = checkpoint(session);
  auto reject = [&](const fx01::Bytes &candidate, tessera_status expected) {
    CHECK(tessera_checkpoint_restore_v1(
              session.handle, {candidate.data(), candidate.size()},
              {nullptr, 0}, &session.context) == expected);
    CHECK(session.state() == before);
  };
  auto damaged = bytes;
  damaged.back() ^= 1;
  reject(damaged, TESSERA_CHECKPOINT_INVALID);
  damaged = bytes;
  damaged[8] = 2;
  reject(damaged, TESSERA_CHECKPOINT_INVALID);
  damaged = bytes;
  damaged[60] = 1;
  reject(damaged, TESSERA_CHECKPOINT_INVALID);
  damaged = bytes;
  damaged.pop_back();
  reject(damaged, TESSERA_CHECKPOINT_INVALID);
  damaged = bytes;
  damaged[64] = 0; // The first TLV tag is invalid despite a valid outer SHA.
  const auto payload_hash = fx01::sha(
      fx01::View(damaged).subspan(64));
  std::copy(payload_hash.begin(), payload_hash.end(), damaged.begin() + 24);
  reject(damaged, TESSERA_CHECKPOINT_INVALID);
  damaged = bytes;
  damaged[88] = 0; // The nested canonical state tag is invalid.
  const auto nested_hash = fx01::sha(fx01::View(damaged).subspan(64));
  std::copy(nested_hash.begin(), nested_hash.end(), damaged.begin() + 24);
  reject(damaged, TESSERA_CHECKPOINT_INVALID);
  fx01::Session other(false);
  CHECK(tessera_checkpoint_restore_v1(
            other.handle, {bytes.data(), bytes.size()}, {nullptr, 0},
            &other.context) == TESSERA_CHECKPOINT_INCOMPATIBLE);
}
