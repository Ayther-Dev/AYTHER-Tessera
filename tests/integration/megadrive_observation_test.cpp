#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "domain/wire.hpp"
#include <tessera/tessera.h>

#include <array>
#include <atomic>
#include <fstream>
#include <iterator>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace {
template <class T> T initial() {
  T value{};
  value.header = {sizeof(T), 1, 0};
  return value;
}
using Bytes = std::vector<std::uint8_t>;
Bytes rom() {
  std::ifstream file(TESSERA_ORIGINAL_ROM, std::ios::binary);
  REQUIRE(file.good());
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
struct Driver {
  Bytes content = rom();
  Bytes memory = Bytes(65536);
  Bytes configuration;
  tessera_session session{};
  tessera_context_v1 context = initial<tessera_context_v1>();
  std::uint64_t epoch{};
  Driver() {
    std::array<tessera_backend_info_v1, 3> backends{
        initial<tessera_backend_info_v1>(),
        initial<tessera_backend_info_v1>(),
        initial<tessera_backend_info_v1>()};
    std::uint32_t count{};
    REQUIRE(tessera_backends_enumerate_v1(backends.data(), backends.size(),
                                          &count, &context) == TESSERA_OK);
    configuration = tessera::wire::configuration(
        "clownmdemu", backends[0].build_id, memory);
    auto create = initial<tessera_session_create_info_v1>();
    create.content = {content.data(), content.size()};
    create.config = {configuration.data(), configuration.size()};
    create.initial_persistent_memory = {memory.data(), memory.size()};
    create.system_override = TESSERA_SYSTEM_MEGA_DRIVE;
    REQUIRE(tessera_session_create_v1(&create, &session, &context) == TESSERA_OK);
    auto info = initial<tessera_session_info_v1>();
    REQUIRE(tessera_session_query_v1(session, &info, &context) == TESSERA_OK);
    epoch = info.frame_id.epoch;
    CHECK((info.capabilities & TESSERA_CAP_TEMPORAL_OBSERVATION) != 0);
  }
  ~Driver() {
    if (session)
      tessera_session_close_v1(session, &context);
  }
  void observe(std::uint64_t frame, std::uint64_t mask = 31) {
    std::array<tessera_control_item_v1, 2> items{{
        {TESSERA_CONTROL_OBSERVATION_REGION_MASK, TESSERA_VALUE_MASK64, mask},
        {TESSERA_CONTROL_OBSERVATION_EVENT_MASK, TESSERA_VALUE_MASK64, 31}}};
    auto controls = initial<tessera_control_transaction_v1>();
    controls.epoch = epoch;
    controls.frame = frame;
    controls.id = frame;
    controls.items = items.data();
    controls.count = items.size();
    REQUIRE(tessera_controls_submit_v1(session, &controls, &context) == TESSERA_OK);
  }
  void step(std::uint64_t frame) {
    std::array<tessera_input_assignment_v1, 8> assignments{};
    for (std::uint16_t i = 0; i < assignments.size(); ++i)
      assignments[i] = {0, i, 0};
    auto input = initial<tessera_input_frame_v1>();
    input.epoch = epoch;
    input.frame = frame;
    input.assignments = assignments.data();
    input.count = assignments.size();
    REQUIRE(tessera_session_step_v1(session, &input, &context) == TESSERA_OK);
  }
  Bytes state() {
    std::uint64_t required{};
    REQUIRE(tessera_state_export_canonical_v1(session, {nullptr, 0},
                                               &required, &context) ==
            TESSERA_BUFFER_TOO_SMALL);
    Bytes data(static_cast<std::size_t>(required));
    REQUIRE(tessera_state_export_canonical_v1(
                session, {data.data(), data.size()}, &required,
                &context) == TESSERA_OK);
    return data;
  }
  std::pair<Bytes, Bytes> original() {
    auto frame = initial<tessera_original_frame_v1>();
    REQUIRE(tessera_original_frame_query_v1(session, &frame, &context) ==
            TESSERA_OK);
    return {{frame.pixels.data, frame.pixels.data + frame.pixels.bytes},
            {frame.pcm.data, frame.pcm.data + frame.pcm.bytes}};
  }
};
struct Retained {
  tessera_snapshot id{};
  Retained() = default;
  explicit Retained(Driver &driver) {
    REQUIRE(tessera_snapshot_acquire_v1(driver.session, &id, &driver.context) ==
            TESSERA_OK);
  }
  Retained(const Retained &) = delete;
  Retained &operator=(const Retained &) = delete;
  ~Retained() {
    if (id) {
      auto context = initial<tessera_context_v1>();
      tessera_snapshot_release_v1(id, &context);
    }
  }
  tessera_snapshot_info_v1 info() const {
    auto context = initial<tessera_context_v1>();
    auto value = initial<tessera_snapshot_info_v1>();
    REQUIRE(tessera_snapshot_query_v1(id, &value, &context) == TESSERA_OK);
    return value;
  }
};
} // namespace

TEST_CASE("Mega Drive scanline evidence reconstructs the exact original") {
  Driver observed, plain;
  observed.observe(1);
  observed.step(1);
  plain.step(1);
  CHECK(observed.state() == plain.state());
  CHECK(observed.original() == plain.original());
  Retained first(observed);
  auto info = first.info();
  CHECK(info.region_mask == 31);
  CHECK(info.complete_mask == 31);
  CHECK(info.region_count == 5);
  CHECK(info.generation >= 1);
  auto second_context = initial<tessera_context_v1>();
  REQUIRE(tessera_snapshot_retain_v1(first.id, &second_context) == TESSERA_OK);
  std::atomic<bool> other_reader_ok{false};
  std::thread other_reader([&] {
    auto context = initial<tessera_context_v1>();
    auto value = initial<tessera_snapshot_info_v1>();
    const auto read =
        tessera_snapshot_query_v1(first.id, &value, &context);
    other_reader_ok = read == TESSERA_OK && value.frame_id.frame == 1;
    tessera_snapshot_release_v1(first.id, &context);
  });
  CHECK(first.info().frame_id.frame == 1);
  other_reader.join();
  CHECK(other_reader_ok.load());

  auto region = initial<tessera_snapshot_region_v1>();
  REQUIRE(tessera_snapshot_region_query_v1(first.id, 4, &region,
                                           &second_context) == TESSERA_OK);
  CHECK(region.kind == 5);
  CHECK(region.flags == 3);
  const auto fields = tessera::wire::record(
      {region.data.data, static_cast<std::size_t>(region.data.bytes)});
  REQUIRE(fields.size() == 8);
  CHECK(tessera::wire::read(
      tessera::wire::get(fields, 0, 1, TESSERA_ENCODING_U32), 0, 4) == 2);
  const auto width = tessera::wire::read(
      tessera::wire::get(fields, 1, 2, TESSERA_ENCODING_U32), 0, 4);
  const auto height = tessera::wire::read(
      tessera::wire::get(fields, 2, 3, TESSERA_ENCODING_U32), 0, 4);
  CHECK(width == 320);
  CHECK(height == 224);
  const auto rows = tessera::wire::get(fields, 4, 5, TESSERA_ENCODING_BYTES);
  REQUIRE(rows.size() == height * (width + 512));
  CHECK(tessera::wire::read(
      tessera::wire::get(fields, 5, 6, TESSERA_ENCODING_U32), 0, 4) == 1);
  const auto cells = tessera::wire::get(fields, 6, 7, TESSERA_ENCODING_BYTES);
  REQUIRE(cells.size() >= 4 + 16 + 256);
  CHECK(tessera::wire::read(cells, 0, 4) >= 1);
  const auto sources = tessera::wire::get(fields, 7, 8, TESSERA_ENCODING_BYTES);
  REQUIRE(sources.size() == width * height * 8);
  const auto plane = sources.subspan((36 * width + 36) * 8, 8);
  CHECK(plane[4] == 1);
  CHECK(plane[5] == 255);
  const auto sprite = sources.subspan((67 * width + 103) * 8, 8);
  CHECK(sprite[4] == 4);
  CHECK(sprite[5] == 0);
  auto original = initial<tessera_original_frame_v1>();
  REQUIRE(tessera_original_frame_query_v1(observed.session, &original,
                                          &observed.context) == TESSERA_OK);
  bool exact = true;
  for (std::size_t y = 0; y < height; ++y)
    for (std::size_t x = 0; x < width; ++x) {
      const auto row = rows.subspan(y * (width + 512));
      const auto index = row[512 + x];
      const auto colour = tessera::wire::read(row, index * 2, 2);
      const auto offset = (y * width + x) * 4;
      exact = exact && original.pixels.data[offset] == (colour & 15) * 17 &&
              original.pixels.data[offset + 1] ==
                  ((colour >> 4) & 15) * 17 &&
              original.pixels.data[offset + 2] ==
                  ((colour >> 8) & 15) * 17 &&
              original.pixels.data[offset + 3] == 255;
    }
  CHECK(exact);
  observed.step(2);
  plain.step(2);
  CHECK(observed.state() == plain.state());
  CHECK(observed.original() == plain.original());
  CHECK(first.info().frame_id.frame == 1);
}

TEST_CASE("Retained readers cause a bounded loss and the next full capture recovers") {
  Driver driver;
  driver.observe(1);
  std::vector<std::unique_ptr<Retained>> readers;
  for (std::uint64_t frame = 1; frame <= 8; ++frame) {
    driver.step(frame);
    readers.push_back(std::make_unique<Retained>(driver));
    CHECK(readers.back()->info().complete_mask == 31);
  }
  driver.step(9);
  tessera_snapshot lost{};
  CHECK(tessera_snapshot_acquire_v1(driver.session, &lost, &driver.context) ==
        TESSERA_BUDGET_EXCEEDED);
  std::uint64_t required{};
  REQUIRE(tessera_losses_query_v1(driver.session, {nullptr, 0}, &required,
                                  &driver.context) == TESSERA_BUFFER_TOO_SMALL);
  Bytes losses(static_cast<std::size_t>(required));
  REQUIRE(tessera_losses_query_v1(
              driver.session, {losses.data(), losses.size()}, &required,
              &driver.context) == TESSERA_OK);
  const auto loss_fields = tessera::wire::record(losses);
  CHECK(tessera::wire::read(
            tessera::wire::get(loss_fields, 0, 1, TESSERA_ENCODING_U64),
            0, 8) > 0);
  readers.clear();
  driver.step(10);
  Retained recovered(driver);
  CHECK(recovered.info().frame_id.frame == 10);
  CHECK(recovered.info().complete_mask == 31);
}
