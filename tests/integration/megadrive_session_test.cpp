#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "domain/wire.hpp"
#include <tessera/tessera.h>

#include <array>
#include <fstream>
#include <iterator>
#include <picosha2.h>

namespace {
template <class T> T initial() {
  T value{};
  value.header = {sizeof(T), 1, 0};
  return value;
}
std::vector<std::uint8_t> load(const char *path) {
  std::ifstream input(path, std::ios::binary);
  REQUIRE(input.good());
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}
} // namespace

TEST_CASE("The copied Mega Drive ROM executes through the native product backend") {
  const auto rom = load(TESSERA_ORIGINAL_ROM);
  REQUIRE(rom.size() == 32768);
  CHECK(picosha2::hash256_hex_string(rom.begin(), rom.end()) ==
        "c7666ec2a4d05ad3df8ce14648b5abd5bc4b0f9711c2edcaad228e127070c34f");
  auto context = initial<tessera_context_v1>();
  auto detection = initial<tessera_detection_v1>();
  std::array<tessera_candidate_v1, 2> candidates{};
  std::uint32_t required{};
  REQUIRE(tessera_detect_v1({rom.data(), rom.size()}, TESSERA_SYSTEM_MEGA_DRIVE,
                            &detection, candidates.data(), candidates.size(),
                            &required, &context) == TESSERA_OK);
  CHECK(detection.system == TESSERA_SYSTEM_MEGA_DRIVE);
  REQUIRE(required == 1);

  std::array<tessera_backend_info_v1, 3> backends{
      initial<tessera_backend_info_v1>(), initial<tessera_backend_info_v1>(),
      initial<tessera_backend_info_v1>()};
  REQUIRE(tessera_backends_enumerate_v1(backends.data(), backends.size(),
                                        &required, &context) == TESSERA_OK);
  REQUIRE(std::string_view(backends[0].id) == "clownmdemu");
  CHECK(backends[0].kind == TESSERA_BACKEND_PRODUCT);
  const std::vector<std::uint8_t> memory(65536, 0);
  const auto configuration = tessera::wire::configuration(
      "clownmdemu", backends[0].build_id, memory);
  auto create = initial<tessera_session_create_info_v1>();
  create.content = {rom.data(), rom.size()};
  create.config = {configuration.data(), configuration.size()};
  create.initial_persistent_memory = {memory.data(), memory.size()};
  create.system_override = TESSERA_SYSTEM_MEGA_DRIVE;
  tessera_session session{};
  REQUIRE(tessera_session_create_v1(&create, &session, &context) == TESSERA_OK);
  auto info = initial<tessera_session_info_v1>();
  REQUIRE(tessera_session_query_v1(session, &info, &context) == TESSERA_OK);
  CHECK(info.system == TESSERA_SYSTEM_MEGA_DRIVE);
  CHECK(info.tick_hz_num == 53693175);
  CHECK(info.frame_id.frame == 0);

  std::array<tessera_input_assignment_v1, 8> assignments{};
  for (std::uint16_t index = 0; index < assignments.size(); ++index)
    assignments[index] = {0, index, 0};
  auto input = initial<tessera_input_frame_v1>();
  input.epoch = info.frame_id.epoch;
  input.frame = 1;
  input.assignments = assignments.data();
  input.count = assignments.size();
  REQUIRE(tessera_session_step_v1(session, &input, &context) == TESSERA_OK);
  auto original = initial<tessera_original_frame_v1>();
  REQUIRE(tessera_original_frame_query_v1(session, &original, &context) ==
          TESSERA_OK);
  CHECK(original.frame_id.frame == 1);
  CHECK(original.width == 320);
  CHECK(original.height == 224);
  CHECK(original.pixels.bytes == 320 * 224 * 4);
  CHECK(original.video_format == TESSERA_VIDEO_RGBA8);
  CHECK(original.sample_rate == 48000);
  CHECK(original.pcm_frames > 0);
  CHECK(original.pcm.bytes == original.pcm_frames * 4);
  CHECK(original.tick_end - original.tick_begin == 895780);
  std::uint64_t state_bytes{};
  CHECK(tessera_state_export_canonical_v1(session, {nullptr, 0}, &state_bytes,
                                          &context) == TESSERA_BUFFER_TOO_SMALL);
  CHECK(state_bytes > 1000000);
  std::vector<std::uint8_t> before(static_cast<std::size_t>(state_bytes));
  REQUIRE(tessera_state_export_canonical_v1(
              session, {before.data(), before.size()}, &state_bytes,
              &context) == TESSERA_OK);
  std::uint64_t checkpoint_bytes{};
  REQUIRE(tessera_checkpoint_export_v1(session, {nullptr, 0},
                                       &checkpoint_bytes, &context) ==
          TESSERA_BUFFER_TOO_SMALL);
  std::vector<std::uint8_t> checkpoint(
      static_cast<std::size_t>(checkpoint_bytes));
  REQUIRE(tessera_checkpoint_export_v1(
              session, {checkpoint.data(), checkpoint.size()},
              &checkpoint_bytes, &context) == TESSERA_OK);
  const auto outer = tessera::wire::record(
      tessera::wire::View(checkpoint).subspan(64));
  const auto state = tessera::wire::get(outer, 0, 1, TESSERA_ENCODING_BYTES);
  const auto fields = tessera::wire::record(state.subspan(8));
  const auto machine = tessera::wire::get(
      fields, 9, 65536, TESSERA_ENCODING_RECORD);
  REQUIRE(machine.size() > 16);
  const auto machine_offset = static_cast<std::size_t>(
      machine.data() - checkpoint.data());
  checkpoint[machine_offset] = 0; // Invalid first nested machine-record tag.
  const auto digest = tessera::wire::sha(
      tessera::wire::View(checkpoint).subspan(64));
  std::copy(digest.begin(), digest.end(), checkpoint.begin() + 24);
  CHECK(tessera_checkpoint_restore_v1(
            session, {checkpoint.data(), checkpoint.size()}, {nullptr, 0},
            &context) == TESSERA_CHECKPOINT_INVALID);
  auto after = initial<tessera_session_info_v1>();
  REQUIRE(tessera_session_query_v1(session, &after, &context) == TESSERA_OK);
  CHECK(after.frame_id.frame == 1);
  CHECK(after.frame_id.epoch == info.frame_id.epoch);
  std::vector<std::uint8_t> unchanged(before.size());
  REQUIRE(tessera_state_export_canonical_v1(
              session, {unchanged.data(), unchanged.size()}, &state_bytes,
              &context) == TESSERA_OK);
  CHECK(unchanged == before);
  CHECK(tessera_session_close_v1(session, &context) == TESSERA_OK);
}
