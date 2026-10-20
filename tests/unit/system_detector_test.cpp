#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/fx01.hpp"
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace {
void put_be32(fx01::Bytes &bytes, std::size_t at, std::uint32_t value) {
  for (unsigned index = 0; index < 4; ++index)
    bytes.at(at + index) = static_cast<std::uint8_t>(value >> (24 - index * 8));
}
fx01::Bytes raw_md() {
  fx01::Bytes bytes(32768, 0);
  put_be32(bytes, 0, 0x00FF1000);
  put_be32(bytes, 4, 0x00000200);
  const std::string signature = "SEGA MEGA DRIVE  ";
  std::copy(signature.begin(), signature.end(), bytes.begin() + 0x100);
  put_be32(bytes, 0x1A0, 0);
  put_be32(bytes, 0x1A4, static_cast<std::uint32_t>(bytes.size() - 1));
  put_be32(bytes, 0x1A8, 0x00FF0000);
  put_be32(bytes, 0x1AC, 0x00FFFFFF);
  bytes[0x1F0] = 'U';
  bytes[0x1F1] = 'S';
  bytes[0x1F2] = 'A';
  bytes[0x200] = 0x60;
  bytes[0x201] = 0xFE;
  return bytes;
}
struct Detection {
  tessera_status status{};
  tessera_detection_v1 result = fx01::init<tessera_detection_v1>();
  std::array<tessera_candidate_v1, 4> candidates{};
  std::uint32_t required{};
  tessera_context_v1 context = fx01::init<tessera_context_v1>();
};
Detection detect(fx01::View bytes,
                 std::uint32_t override_system = TESSERA_SYSTEM_NONE) {
  Detection found;
  found.status =
      tessera_detect_v1(fx01::view(bytes), override_system, &found.result,
                        found.candidates.data(),
                        static_cast<std::uint32_t>(found.candidates.size()),
                        &found.required, &found.context);
  return found;
}
void recognized(const Detection &found, fx01::View bytes, std::uint64_t offset,
                std::uint64_t payload_bytes) {
  REQUIRE(found.status == TESSERA_OK);
  CHECK(found.context.code == TESSERA_OK);
  CHECK(found.result.outcome == TESSERA_DETECTION_RECOGNIZED);
  CHECK(found.result.system == TESSERA_SYSTEM_MEGA_DRIVE);
  CHECK(found.result.evidence != 0);
  CHECK(found.result.evidence_offset == offset);
  CHECK(found.result.evidence_bytes == payload_bytes);
  CHECK(found.required == 1);
  CHECK(found.result.candidate_count == 1);
  CHECK(found.candidates[0].system == TESSERA_SYSTEM_MEGA_DRIVE);
  CHECK(found.candidates[0].evidence != 0);
  const auto digest = fx01::sha(bytes);
  CHECK(std::equal(digest.begin(), digest.end(),
                   std::begin(found.result.content_sha256)));
}
} // namespace

TEST_CASE("MD recognition uses bytes rather than a name and distinguishes RAW "
          "from COPIER512") {
  const auto raw = raw_md();
  recognized(detect(raw), raw, 0, raw.size());
  // The public detector receives no filename, extension or directory ordering.
  const auto renamed_copy = raw;
  const auto first = detect(raw), renamed = detect(renamed_copy);
  CHECK(first.result.evidence == renamed.result.evidence);
  CHECK(std::equal(std::begin(first.result.content_sha256),
                   std::end(first.result.content_sha256),
                   std::begin(renamed.result.content_sha256)));
  fx01::Bytes copier(512, 0xA5);
  fx01::append(copier, raw);
  recognized(detect(copier), copier, 512, raw.size());
  recognized(detect(raw, TESSERA_SYSTEM_MEGA_DRIVE), raw, 0, raw.size());
}

TEST_CASE("Two valid MD interpretations remain ambiguous even with a "
          "same-system override") {
  const auto payload = raw_md();
  fx01::Bytes ambiguous(512, 0);
  fx01::append(ambiguous, payload);
  const auto header = raw_md();
  std::copy_n(header.begin(), 512, ambiguous.begin());
  put_be32(ambiguous, 0x1A4, static_cast<std::uint32_t>(ambiguous.size() - 1));
  for (const auto override_system : std::array<std::uint32_t, 2>{
           TESSERA_SYSTEM_NONE, TESSERA_SYSTEM_MEGA_DRIVE}) {
    const auto found = detect(ambiguous, override_system);
    CHECK(found.status == TESSERA_AMBIGUOUS_CONTENT);
    CHECK(found.result.outcome == TESSERA_DETECTION_AMBIGUOUS);
    CHECK(found.required >= 2);
    CHECK(found.result.candidate_count == found.required);
    REQUIRE(found.required <= found.candidates.size());
    for (std::size_t index = 0; index < found.required; ++index) {
      CHECK(found.candidates[index].system == TESSERA_SYSTEM_MEGA_DRIVE);
      CHECK(found.candidates[index].evidence != 0);
      if (index)
        CHECK(found.candidates[index - 1].system <=
              found.candidates[index].system);
    }
  }
}

TEST_CASE("Malformed MD structure differs from unknown bytes and override "
          "cannot invent evidence") {
  auto truncated = raw_md();
  truncated.resize(400);
  auto odd_reset = raw_md();
  put_be32(odd_reset, 4, 0x201);
  auto outside_reset = raw_md();
  put_be32(outside_reset, 4, 0x10000);
  auto beyond_declared_rom = raw_md();
  put_be32(beyond_declared_rom, 0x1A4, 0x20000);
  for (const auto &bytes : std::array<fx01::Bytes, 4>{
           truncated, odd_reset, outside_reset, beyond_declared_rom}) {
    const auto found = detect(bytes);
    CHECK(found.status == TESSERA_INVALID_CONTENT);
    CHECK(found.result.outcome == TESSERA_DETECTION_INVALID);
    CHECK(found.context.code == TESSERA_INVALID_CONTENT);
  }
  const fx01::Bytes unknown(1024, 0x3C);
  for (const auto override_system : std::array<std::uint32_t, 2>{
           TESSERA_SYSTEM_NONE, TESSERA_SYSTEM_MEGA_DRIVE}) {
    const auto found = detect(unknown, override_system);
    CHECK(found.status == TESSERA_UNKNOWN_CONTENT);
    CHECK(found.result.outcome == TESSERA_DETECTION_UNKNOWN);
    CHECK(found.required == 0);
    CHECK(found.result.candidate_count == 0);
  }
  CHECK(detect(raw_md(), 99).status == TESSERA_INVALID_ARGUMENT);
}

TEST_CASE("Candidate capacity is queried without a partial result and without "
          "changing a live session") {
  fx01::Session live;
  fx01::expect(live.step(32));
  const auto state = live.state();
  const auto rom = raw_md();
  auto context = fx01::init<tessera_context_v1>();
  auto out = fx01::init<tessera_detection_v1>();
  out.evidence = 0x12345678;
  const auto before = out;
  std::uint32_t required{};
  CHECK(tessera_detect_v1(fx01::view(rom), 0, &out, nullptr, 0, &required,
                          &context) == TESSERA_BUFFER_TOO_SMALL);
  CHECK(required == 1);
  CHECK(std::memcmp(&out, &before, sizeof(out)) == 0);
  std::array<tessera_candidate_v1, 1> candidate{};
  fx01::expect(tessera_detect_v1(fx01::view(rom), 0, &out, candidate.data(), 1,
                                 &required, &context));
  CHECK(out.system == TESSERA_SYSTEM_MEGA_DRIVE);
  CHECK(live.state() == state);
}

TEST_CASE("Registry exposes the real Mega Drive strategy and keeps synthetic strategies isolated") {
  fx01::Session minimal(false), complete;
  CHECK(minimal.info().system == TESSERA_SYSTEM_NONE);
  CHECK(complete.info().system == TESSERA_SYSTEM_NONE);
  CHECK(minimal.info().capabilities == UINT64_C(0x120F));
  CHECK(complete.info().capabilities == UINT64_C(0x1FFF));
  const auto before = complete.state();
  const auto rom = raw_md();
  auto request = fx01::init<tessera_session_create_info_v1>();
  request.content = fx01::view(rom);
  request.system_override = TESSERA_SYSTEM_MEGA_DRIVE;
  tessera_session untouched = UINT64_C(0x123456);
  CHECK(tessera_session_create_v1(&request, &untouched, &complete.context) ==
        TESSERA_UNSUPPORTED_CAPABILITY);
  CHECK(untouched == UINT64_C(0x123456));
  CHECK(complete.state() == before);
  const auto unknown_id = fx01::str("not.installed");
  request.backend_id = fx01::view(unknown_id);
  request.content = fx01::view(complete.rom);
  request.config = fx01::view(complete.cfg);
  request.initial_persistent_memory = fx01::view(complete.pm);
  request.system_override = TESSERA_SYSTEM_NONE;
  CHECK(tessera_session_create_v1(&request, &untouched, &complete.context) ==
        TESSERA_IMPLEMENTATION_UNAVAILABLE);
  CHECK(untouched == UINT64_C(0x123456));
  CHECK(complete.state() == before);
}

TEST_CASE("An invalid synthetic-looking prefix does not hide a valid COPIER512 "
          "interpretation") {
  const auto raw = raw_md();
  fx01::Bytes bytes(512, 0xA5);
  const auto magic = fx01::str("TSSYN001");
  std::copy(magic.begin(), magic.end(), bytes.begin());
  fx01::append(bytes, raw);
  recognized(detect(bytes), bytes, 512, raw.size());
}
