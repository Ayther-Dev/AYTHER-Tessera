#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/fx01.hpp"
#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <map>
#include <thread>
#include <utility>

namespace {
struct Field {
  std::uint32_t encoding{};
  fx01::Bytes bytes;
};
using Fields = std::map<std::uint32_t, Field>;
Fields fields(fx01::View bytes) {
  Fields result;
  std::size_t offset{};
  std::uint32_t previous{};
  while (offset < bytes.size()) {
    REQUIRE(bytes.size() - offset >= 16);
    const auto tag = static_cast<std::uint32_t>(fx01::read(bytes, offset, 4));
    const auto encoding =
        static_cast<std::uint32_t>(fx01::read(bytes, offset + 4, 4));
    const auto length = fx01::read(bytes, offset + 8, 8);
    offset += 16;
    REQUIRE(tag > previous);
    REQUIRE(encoding >= 1);
    REQUIRE(encoding <= 11);
    REQUIRE(length <= bytes.size() - offset);
    const auto data = bytes.subspan(offset, static_cast<std::size_t>(length));
    result.emplace(tag, Field{encoding, fx01::Bytes(data.begin(), data.end())});
    offset += static_cast<std::size_t>(length);
    previous = tag;
  }
  return result;
}
const Field &field(const Fields &record, std::uint32_t tag,
                   std::uint32_t encoding) {
  const auto found = record.find(tag);
  REQUIRE(found != record.end());
  REQUIRE(found->second.encoding == encoding);
  return found->second;
}
std::uint64_t number(const Fields &record, std::uint32_t tag,
                     std::uint32_t encoding = TESSERA_ENCODING_U64) {
  const auto &value = field(record, tag, encoding);
  const unsigned width = encoding == TESSERA_ENCODING_U32     ? 4
                         : encoding == TESSERA_ENCODING_BOOL8 ? 1
                                                              : 8;
  REQUIRE(value.bytes.size() == width);
  return fx01::read(value.bytes, 0, width);
}
std::vector<Fields> records(const Field &input) {
  REQUIRE(input.encoding == TESSERA_ENCODING_ARRAY);
  REQUIRE(input.bytes.size() >= 8);
  const auto count = fx01::read(input.bytes, 0, 8);
  REQUIRE(count <= 4096);
  std::vector<Fields> result;
  std::size_t offset = 8;
  for (std::uint64_t index = 0; index < count; ++index) {
    REQUIRE(input.bytes.size() - offset >= 8);
    const auto size = fx01::read(input.bytes, offset, 8);
    offset += 8;
    REQUIRE(size <= input.bytes.size() - offset);
    result.push_back(
        fields(fx01::View(input.bytes)
                   .subspan(offset, static_cast<std::size_t>(size))));
    offset += static_cast<std::size_t>(size);
  }
  REQUIRE(offset == input.bytes.size());
  return result;
}
struct Snapshot {
  tessera_snapshot handle{};
  explicit Snapshot(tessera_snapshot value = 0) : handle(value) {}
  Snapshot(const Snapshot &) = delete;
  Snapshot &operator=(const Snapshot &) = delete;
  Snapshot(Snapshot &&other) noexcept
      : handle(std::exchange(other.handle, 0)) {}
  ~Snapshot() {
    if (handle) {
      auto context = fx01::init<tessera_context_v1>();
      tessera_snapshot_release_v1(handle, &context);
    }
  }
};
Snapshot acquire(fx01::Session &session) {
  tessera_snapshot handle{};
  fx01::expect(
      tessera_snapshot_acquire_v1(session.handle, &handle, &session.context));
  REQUIRE(handle != 0);
  return Snapshot{handle};
}
tessera_snapshot_info_v1 snapshot_info(tessera_snapshot snapshot) {
  auto context = fx01::init<tessera_context_v1>();
  auto info = fx01::init<tessera_snapshot_info_v1>();
  fx01::expect(tessera_snapshot_query_v1(snapshot, &info, &context));
  return info;
}
struct Region {
  tessera_snapshot_region_v1 info;
  fx01::Bytes bytes;
  std::uint32_t index;
};
std::map<std::uint32_t, Region> regions(tessera_snapshot snapshot) {
  const auto info = snapshot_info(snapshot);
  std::map<std::uint32_t, Region> result;
  for (std::uint32_t index = 0; index < info.region_count; ++index) {
    auto context = fx01::init<tessera_context_v1>();
    auto region = fx01::init<tessera_snapshot_region_v1>();
    fx01::expect(
        tessera_snapshot_region_query_v1(snapshot, index, &region, &context));
    REQUIRE(region.data.data != nullptr);
    REQUIRE(region.data.bytes > 0);
    CHECK(region.byte_count == region.data.bytes);
    REQUIRE(result
                .emplace(region.kind, Region{region,
                                             fx01::Bytes(region.data.data,
                                                         region.data.data +
                                                             region.data.bytes),
                                             index})
                .second);
  }
  return result;
}
struct Batch {
  fx01::Bytes wire;
  std::vector<Fields> events;
  Fields losses;
  std::uint64_t next{};
};
Batch read_events(fx01::Session &session, std::uint64_t cursor = 0) {
  std::uint64_t required{}, next = UINT64_C(0xABCDEF);
  REQUIRE(tessera_events_read_v1(session.handle, cursor, {nullptr, 0},
                                 &required, &next,
                                 &session.context) == TESSERA_BUFFER_TOO_SMALL);
  CHECK(next == UINT64_C(0xABCDEF));
  REQUIRE(required > 1);
  REQUIRE(required <= 64 * 1024 * 1024);
  fx01::Bytes small(static_cast<std::size_t>(required - 1), 0xA5);
  auto repeated = required;
  CHECK(tessera_events_read_v1(session.handle, cursor,
                               {small.data(), small.size()}, &repeated, &next,
                               &session.context) == TESSERA_BUFFER_TOO_SMALL);
  CHECK(repeated == required);
  CHECK(next == UINT64_C(0xABCDEF));
  CHECK(std::all_of(small.begin(), small.end(),
                    [](auto value) { return value == 0xA5; }));
  Batch batch;
  batch.wire.resize(static_cast<std::size_t>(required));
  fx01::expect(tessera_events_read_v1(
      session.handle, cursor, {batch.wire.data(), batch.wire.size()}, &required,
      &batch.next, &session.context));
  const auto parsed = fields(batch.wire);
  REQUIRE(parsed.size() == 3);
  batch.events = records(field(parsed, 1, TESSERA_ENCODING_ARRAY));
  CHECK(number(parsed, 2) == batch.next);
  batch.losses = fields(field(parsed, 3, TESSERA_ENCODING_RECORD).bytes);
  REQUIRE(batch.losses.size() == 5);
  return batch;
}
Fields read_losses(fx01::Session &session) {
  std::uint64_t required{};
  REQUIRE(tessera_losses_query_v1(session.handle, {nullptr, 0}, &required,
                                  &session.context) ==
          TESSERA_BUFFER_TOO_SMALL);
  REQUIRE(required > 0);
  REQUIRE(required <= 1024);
  fx01::Bytes bytes(static_cast<std::size_t>(required));
  fx01::expect(tessera_losses_query_v1(session.handle,
                                       {bytes.data(), bytes.size()}, &required,
                                       &session.context));
  auto result = fields(bytes);
  REQUIRE(result.size() == 5);
  return result;
}
std::pair<fx01::Bytes, fx01::Bytes> originals(fx01::Session &session) {
  const auto frame = session.original();
  return {{frame.pixels.data, frame.pixels.data + frame.pixels.bytes},
          {frame.pcm.data, frame.pcm.data + frame.pcm.bytes}};
}
} // namespace

TEST_CASE("A complete capture exposes the five independently specified "
          "coherent regions") {
  fx01::Session session;
  fx01::expect(session.observe(11));
  fx01::expect(session.step(32));
  const auto state = session.state();
  const auto original = originals(session);
  const auto live = session.info();
  auto retained = acquire(session);
  const auto info = snapshot_info(retained.handle);
  const auto captured = regions(retained.handle);
  CHECK(info.frame_id.session == live.frame_id.session);
  CHECK(info.frame_id.epoch == live.frame_id.epoch);
  CHECK(info.frame_id.frame == 1);
  CHECK(info.generation == live.generation);
  CHECK(info.logical_tick == 1000);
  CHECK(info.region_mask == 31);
  CHECK(info.complete_mask == 31);
  REQUIRE(info.region_count == 5);
  REQUIRE(captured.size() == 5);
  for (std::uint32_t kind = 1; kind <= 5; ++kind)
    REQUIRE(captured.contains(kind));
  const auto &memory = captured.at(1);
  CHECK(memory.info.encoding == TESSERA_ENCODING_BYTES);
  REQUIRE(memory.bytes.size() == 16);
  CHECK(memory.bytes[0] == 4);
  CHECK(memory.bytes[5] == 90);
  CHECK(memory.bytes[13] == 226);
  const auto &palette = captured.at(2);
  CHECK(palette.info.encoding == TESSERA_ENCODING_BYTES);
  REQUIRE(palette.bytes.size() == 256 * 4);
  CHECK(palette.info.element_bytes == 4);
  CHECK(palette.info.element_count == 256);
  CHECK(palette.bytes[0] == 32);
  CHECK(palette.bytes[1] == 40);
  CHECK(palette.bytes[2] == 5);
  CHECK(palette.bytes[3] == 255);
  const auto &sprite = captured.at(3);
  REQUIRE(sprite.bytes.size() == 8);
  CHECK(fx01::read(sprite.bytes, 0, 2) == 0);
  CHECK(fx01::read(sprite.bytes, 2, 2) == 4);
  CHECK(fx01::read(sprite.bytes, 4, 2) == 255);
  CHECK(fx01::read(sprite.bytes, 6, 2) == 1);
  const auto geometry = fields(captured.at(4).bytes);
  CHECK(captured.at(4).info.encoding == TESSERA_ENCODING_RECORD);
  CHECK(number(geometry, 1, TESSERA_ENCODING_U32) == 16);
  CHECK(number(geometry, 2, TESSERA_ENCODING_U32) == 16);
  CHECK(number(geometry, 3, TESSERA_ENCODING_U32) == 1);
  CHECK(number(geometry, 4, TESSERA_ENCODING_U32) == 1);
  const auto temporal = fields(captured.at(5).bytes);
  CHECK(captured.at(5).info.encoding == TESSERA_ENCODING_RECORD);
  CHECK(number(temporal, 1) == 1);
  CHECK(number(temporal, 2) == 0);
  CHECK(number(temporal, 3) == 1000);
  CHECK(number(temporal, 4, TESSERA_ENCODING_BOOL8) == 1);
  std::uint64_t bytes{};
  for (const auto &[kind, region] : captured) {
    (void)kind;
    bytes += region.bytes.size();
  }
  CHECK(info.retained_bytes >= bytes);
  CHECK(info.retained_bytes <= 8 * 1024 * 1024);
  CHECK(session.state() == state);
  CHECK(originals(session) == original);
  fx01::expect(session.step());
  const auto historical = regions(retained.handle);
  CHECK(historical.at(1).bytes == memory.bytes);
  CHECK(snapshot_info(retained.handle).frame_id.frame == 1);
}

TEST_CASE("Two threads own independent references and historical captures "
          "survive reset and close") {
  fx01::Session session;
  fx01::expect(session.observe());
  fx01::expect(session.step(32));
  auto retained = acquire(session);
  const auto saved = regions(retained.handle);
  const auto identity = snapshot_info(retained.handle);
  fx01::expect(tessera_session_reset_v1(session.handle, &session.context));
  CHECK(session.info().frame_id.epoch == identity.frame_id.epoch + 1);
  CHECK(regions(retained.handle).at(1).bytes == saved.at(1).bytes);
  fx01::expect(tessera_session_close_v1(session.handle, &session.context));
  session.handle = 0;
  CHECK(regions(retained.handle).at(1).bytes == saved.at(1).bytes);
  auto context = fx01::init<tessera_context_v1>();
  fx01::expect(tessera_snapshot_retain_v1(retained.handle, &context));
  const auto token = std::exchange(retained.handle, 0);
  std::atomic<bool> success{true};
  const auto worker = [&] {
    auto local = fx01::init<tessera_context_v1>();
    for (unsigned repetition = 0; repetition < 100; ++repetition) {
      auto info = fx01::init<tessera_snapshot_info_v1>();
      auto region = fx01::init<tessera_snapshot_region_v1>();
      if (tessera_snapshot_query_v1(token, &info, &local) != TESSERA_OK ||
          tessera_snapshot_region_query_v1(token, saved.at(1).index, &region,
                                           &local) != TESSERA_OK) {
        success = false;
        break;
      }
      if (info.frame_id.epoch != identity.frame_id.epoch ||
          info.generation != identity.generation ||
          region.data.bytes != saved.at(1).bytes.size() ||
          region.data.data == nullptr ||
          !std::equal(saved.at(1).bytes.begin(), saved.at(1).bytes.end(),
                      region.data.data)) {
        success = false;
        break;
      }
    }
    if (tessera_snapshot_release_v1(token, &local) != TESSERA_OK)
      success = false;
  };
  {
    std::jthread first(worker), second(worker);
  }
  CHECK(success.load());
  auto dead = fx01::init<tessera_snapshot_info_v1>();
  CHECK(tessera_snapshot_query_v1(token, &dead, &context) ==
        TESSERA_INVALID_HANDLE);
  CHECK(tessera_snapshot_release_v1(token, &context) == TESSERA_INVALID_HANDLE);
}

TEST_CASE("Snapshot errors and the eight-object budget preserve machine "
          "originals and retained bytes") {
  fx01::Session session;
  tessera_snapshot absent = 123;
  CHECK(tessera_snapshot_acquire_v1(session.handle, &absent,
                                    &session.context) == TESSERA_WRONG_STATE);
  CHECK(absent == 123);
  fx01::expect(session.observe());
  fx01::expect(session.step());
  auto limits = fx01::init<tessera_limits_v1>();
  fx01::expect(
      tessera_limits_query_v1(session.handle, &limits, &session.context));
  REQUIRE(limits.retained_count == 8);
  std::vector<Snapshot> held;
  held.reserve(limits.retained_count);
  std::uint64_t retained_bytes{};
  for (std::uint32_t index = 0; index < limits.retained_count; ++index) {
    held.push_back(acquire(session));
    retained_bytes += snapshot_info(held.back().handle).retained_bytes;
    if (index + 1 < limits.retained_count)
      fx01::expect(session.step());
  }
  CHECK(retained_bytes <= limits.retained_bytes);
  const auto saved = regions(held[0].handle).at(1).bytes;
  // A ninth distinct frame must still advance and preserve original outputs;
  // its optional capture can fail while all eight older objects are retained.
  fx01::expect(session.step());
  const auto state = session.state();
  const auto original = originals(session);
  CHECK(
      tessera_snapshot_acquire_v1(session.handle, &absent, &session.context) ==
      TESSERA_BUDGET_EXCEEDED);
  CHECK(absent == 123);
  CHECK(session.state() == state);
  CHECK(originals(session) == original);
  CHECK(regions(held[0].handle).at(1).bytes == saved);
  const auto losses = read_losses(session);
  CHECK(number(losses, 1) >= 1);
  CHECK((number(losses, 4, TESSERA_ENCODING_U32) & UINT32_C(0x7FFFFFFF)) == 2);
  CHECK(number(losses, 2) == 0);
  CHECK(number(losses, 3) == 0);
  auto info = fx01::init<tessera_snapshot_info_v1>();
  info.header.size = 79;
  const auto before = info;
  CHECK(tessera_snapshot_query_v1(held[0].handle, &info, &session.context) ==
        TESSERA_ABI_INCOMPATIBLE);
  CHECK(std::memcmp(&info, &before, sizeof(info)) == 0);
  auto region = fx01::init<tessera_snapshot_region_v1>();
  const auto region_before = region;
  CHECK(tessera_snapshot_region_query_v1(held[0].handle, 5, &region,
                                         &session.context) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(std::memcmp(&region, &region_before, sizeof(region)) == 0);
  const auto expired = held.back().handle;
  held.pop_back();
  fx01::expect(session.step());
  auto replacement = acquire(session);
  CHECK(replacement.handle != expired);
  info = fx01::init<tessera_snapshot_info_v1>();
  CHECK(tessera_snapshot_query_v1(expired, &info, &session.context) ==
        TESSERA_INVALID_HANDLE);
}

TEST_CASE("Control groups reject atomically and apply only at the next valid "
          "frame without interference") {
  fx01::Session observed, reference;
  fx01::expect(observed.step());
  fx01::expect(reference.step());
  const auto before = observed.state();
  const auto reject = [&](std::uint64_t id,
                          std::span<const tessera_control_item_v1> items) {
    CHECK(observed.submit(id, items) == TESSERA_INVALID_ARGUMENT);
    CHECK(observed.state() == before);
    CHECK(observed.info().pending_transaction == 0);
  };
  const std::array<tessera_control_item_v1, 2> valid{
      {{1, 1, 3}, {2, 1, TESSERA_EVENT_BACKEND}}};
  reject(0, valid);
  reject(10, {});
  for (const auto bad : std::array<tessera_control_item_v1, 4>{
           {{1, 1, 32}, {1, 99, 1}, {3, 1, 0}, {2, 1, 32}}}) {
    const std::array<tessera_control_item_v1, 2> items{{valid[0], bad}};
    reject(10, items);
  }
  const std::array<tessera_control_item_v1, 2> duplicate{
      {{1, 1, 1}, {1, 1, 1}}};
  reject(10, duplicate);
  fx01::expect(observed.submit(77, valid));
  CHECK(observed.info().pending_transaction == 77);
  CHECK(observed.submit(78, valid) == TESSERA_CONTROL_PENDING);
  CHECK(observed.info().pending_transaction == 77);
  fx01::expect(tessera_session_pause_v1(observed.handle, &observed.context));
  {
    auto snapshot = acquire(observed);
    CHECK(snapshot_info(snapshot.handle).region_mask == 0);
  }
  auto assignments = fx01::inputs();
  auto invalid = fx01::init<tessera_input_frame_v1>();
  const auto identity = observed.info().frame_id;
  invalid.epoch = identity.epoch;
  invalid.frame = 3;
  invalid.assignments = assignments.data();
  invalid.count = 8;
  CHECK(tessera_session_step_v1(observed.handle, &invalid, &observed.context) ==
        TESSERA_INPUT_OUT_OF_ORDER);
  CHECK(observed.state() == before);
  CHECK(observed.info().pending_transaction == 77);
  fx01::expect(observed.step(32));
  const auto confirmation = observed.context;
  fx01::expect(reference.step(32));
  CHECK((confirmation.flags & TESSERA_CONTEXT_CONTROLS_APPLIED) != 0);
  CHECK(confirmation.transaction == 77);
  CHECK(confirmation.frame_id.epoch == identity.epoch);
  CHECK(confirmation.frame_id.frame == 2);
  CHECK(observed.info().pending_transaction == 0);
  CHECK(observed.state() == reference.state());
  CHECK(originals(observed) == originals(reference));
  {
    auto snapshot = acquire(observed);
    const auto info = snapshot_info(snapshot.handle);
    CHECK(info.region_mask == 3);
    CHECK(info.complete_mask == 3);
    CHECK(info.region_count == 2);
  }
  const std::array<tessera_control_item_v1, 2> disable{{{1, 1, 0}, {2, 1, 0}}};
  fx01::expect(observed.submit(79, disable));
  fx01::expect(observed.step());
  fx01::expect(reference.step());
  {
    auto snapshot = acquire(observed);
    CHECK(snapshot_info(snapshot.handle).region_mask == 0);
    CHECK(snapshot_info(snapshot.handle).region_count == 0);
  }
  CHECK(observed.state() == reference.state());
  CHECK(originals(observed) == originals(reference));
}

TEST_CASE("Readers own their cursors and short event buffers publish no bytes "
          "or cursor advance") {
  fx01::Session session;
  fx01::expect(session.observe(7));
  fx01::expect(session.step());
  fx01::expect(session.step(32));
  {
    auto snapshot = acquire(session);
  }
  const auto state = session.state();
  const auto original = originals(session);
  const auto identity = session.info().frame_id;
  const auto reader1 = read_events(session), reader2 = read_events(session);
  CHECK(reader1.wire == reader2.wire);
  REQUIRE(!reader1.events.empty());
  std::uint64_t previous{};
  for (const auto &event : reader1.events) {
    REQUIRE(event.size() == 8);
    const auto sequence = number(event, 1);
    CHECK(sequence > previous);
    previous = sequence;
    CHECK(number(event, 2) == identity.session);
    CHECK(number(event, 3) == identity.epoch);
    CHECK(number(event, 4) <= identity.frame);
    CHECK(number(event, 5) <= 2000);
    const auto code = number(event, 6, TESSERA_ENCODING_U32);
    CHECK(code >= 1);
    CHECK(code <= 5);
    CHECK(field(event, 7, TESSERA_ENCODING_BYTES).bytes.size() <= 256);
    CHECK(number(event, 8, TESSERA_ENCODING_U32) >= 1);
    CHECK(number(event, 8, TESSERA_ENCODING_U32) <= 11);
  }
  CHECK(read_events(session, reader1.next).events.empty());
  CHECK(number(reader1.losses, 1) == 0);
  CHECK(number(read_losses(session), 1) == 0);
  CHECK(session.state() == state);
  CHECK(originals(session) == original);
  std::array<std::uint8_t, 256> untouched;
  untouched.fill(0xA5);
  std::uint64_t required{}, next = 123;
  CHECK(tessera_events_read_v1(
            session.handle, std::numeric_limits<std::uint64_t>::max(),
            {untouched.data(), untouched.size()}, &required, &next,
            &session.context) == TESSERA_INVALID_ARGUMENT);
  CHECK(next == 123);
  CHECK(std::all_of(untouched.begin(), untouched.end(),
                    [](auto value) { return value == 0xA5; }));
}

TEST_CASE("A saturated 4096-event journal reports its exact gap independently "
          "and still cancels controls") {
  for (const bool close_session : std::array<bool, 2>{false, true}) {
    fx01::Session session;
    fx01::expect(session.observe(1));
    for (unsigned frame = 0; frame < 4200; ++frame)
      fx01::expect(session.step());
    const auto baseline = session.state();
    const auto original = originals(session);
    const auto identity = session.info().frame_id;
    const auto batch = read_events(session);
    REQUIRE(batch.events.size() == 4096);
    const auto first = number(batch.events.front(), 1),
               last = number(batch.events.back(), 1);
    CHECK(first > 1);
    CHECK(last - first + 1 == 4096);
    for (std::size_t index = 1; index < batch.events.size(); ++index)
      CHECK(number(batch.events[index], 1) ==
            number(batch.events[index - 1], 1) + 1);
    const auto loss = read_losses(session);
    const auto count = number(loss, 1), begin = number(loss, 2),
               end = number(loss, 3);
    CHECK(count > 0);
    CHECK(begin > 0);
    CHECK(end >= begin);
    CHECK(end < first);
    CHECK(count == end - begin + 1);
    const auto cause = number(loss, 4, TESSERA_ENCODING_U32);
    CHECK((cause & UINT32_C(0x80000000)) != 0);
    CHECK((cause & UINT32_C(0x7FFFFFFF)) == 1);
    CHECK(number(batch.losses, 1) == count);
    CHECK(number(batch.losses, 2) == begin);
    CHECK(number(batch.losses, 3) == end);
    CHECK(read_events(session).wire == batch.wire);
    CHECK(session.state() == baseline);
    CHECK(originals(session) == original);
    fx01::expect(session.observe(901));
    if (close_session) {
      fx01::expect(tessera_session_close_v1(session.handle, &session.context));
      session.handle = 0;
      CHECK(session.context.transaction == 901);
      CHECK(session.context.cancel_reason == TESSERA_CANCEL_CLOSE);
      CHECK((session.context.flags & TESSERA_CONTEXT_CONTROL_CANCELLED) != 0);
    } else {
      fx01::expect(tessera_session_reset_v1(session.handle, &session.context));
      const auto cancelled = session.context;
      CHECK(cancelled.transaction == 901);
      CHECK(cancelled.cancel_reason == TESSERA_CANCEL_RESET);
      CHECK((cancelled.flags & TESSERA_CONTEXT_CONTROL_CANCELLED) != 0);
      const auto now = session.info();
      CHECK(now.frame_id.epoch == identity.epoch + 1);
      CHECK(now.frame_id.frame == 0);
      CHECK(now.pending_transaction == 0);
      const auto reset_state = session.state();
      const std::array<tessera_control_item_v1, 1> item{{{1, 1, 31}}};
      auto old = fx01::init<tessera_control_transaction_v1>();
      old.epoch = identity.epoch;
      old.frame = identity.frame + 1;
      old.id = 902;
      old.items = item.data();
      old.count = 1;
      CHECK(tessera_controls_submit_v1(
                session.handle, &old, &session.context) == TESSERA_STALE_EPOCH);
      CHECK(session.state() == reset_state);
      CHECK(session.info().pending_transaction == 0);
      fx01::expect(session.step());
      auto neutral = acquire(session);
      CHECK(snapshot_info(neutral.handle).region_mask == 0);
    }
  }
}

TEST_CASE("Repeated acquisitions share a bounded snapshot token and count "
          "independent references") {
  fx01::Session session;
  fx01::expect(session.observe());
  fx01::expect(session.step());
  auto first = acquire(session);
  std::vector<Snapshot> references;
  for (unsigned index = 0; index < 1000; ++index) {
    references.push_back(acquire(session));
    REQUIRE(references.back().handle == first.handle);
  }
  references.clear();
  CHECK(snapshot_info(first.handle).frame_id.frame == 1);
}
