// TP-110.2 (RF-4.1): the native Mega Drive backend through the public API.
//  - a checkpoint exported at frame K and restored (in the same session or in a
//    new one) goes on exactly like the original;
//  - a checkpoint of another codec (`TSCLOWN1`/Clown machine record, or the
//    synthetic one), another ROM or a corrupted one is refused with the
//    documented status, and the session stays as it was.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "domain/wire.hpp"
#include "persistence/state_format.hpp"
#include "support/md_testrom.hpp"
#include <tessera/tessera.h>

#include <array>
#include <cstring>
#include <string_view>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;

template <class T> T initial() {
  T value{};
  value.header = {sizeof(T), 1, 0};
  return value;
}

struct Native {
  tessera_context_v1 context = initial<tessera_context_v1>();
  tessera_session handle{};
  Bytes rom;
  std::uint64_t epoch = 0;

  explicit Native(Bytes content) : rom(std::move(content)) {
    std::array<tessera_backend_info_v1, 4> backends{
        initial<tessera_backend_info_v1>(), initial<tessera_backend_info_v1>(),
        initial<tessera_backend_info_v1>(), initial<tessera_backend_info_v1>()};
    std::uint32_t required{};
    REQUIRE(tessera_backends_enumerate_v1(backends.data(), backends.size(),
                                          &required, &context) == TESSERA_OK);
    const tessera_backend_info_v1 *native = nullptr;
    for (std::uint32_t i = 0; i < required && i < backends.size(); ++i)
      if (std::string_view(backends[i].id) == "megadrive")
        native = &backends[i];
    REQUIRE(native != nullptr);
    const Bytes memory(65536, 0);
    const auto configuration =
        tessera::wire::configuration("megadrive", native->build_id, memory);
    auto create = initial<tessera_session_create_info_v1>();
    const auto id = tessera::wire::text("megadrive");
    create.backend_id = {id.data(), id.size()};
    create.content = {rom.data(), rom.size()};
    create.config = {configuration.data(), configuration.size()};
    create.initial_persistent_memory = {memory.data(), memory.size()};
    create.system_override = TESSERA_SYSTEM_MEGA_DRIVE;
    REQUIRE(tessera_session_create_v1(&create, &handle, &context) ==
            TESSERA_OK);
    epoch = info().frame_id.epoch;
  }
  ~Native() {
    if (handle != 0)
      tessera_session_close_v1(handle, &context);
  }
  Native(const Native &) = delete;
  Native &operator=(const Native &) = delete;

  tessera_session_info_v1 info() {
    auto result = initial<tessera_session_info_v1>();
    REQUIRE(tessera_session_query_v1(handle, &result, &context) == TESSERA_OK);
    return result;
  }
  /// Steps one frame with a moving pad on both ports.
  void step() {
    const auto now = info();
    std::array<tessera_input_assignment_v1, 24> assignments{};
    for (std::uint16_t port = 0; port < 2; ++port)
      for (std::uint16_t control = 0; control < 12; ++control) {
        const auto key = static_cast<unsigned>(now.frame_id.frame * 7 +
                                               control * 3 + port);
        // up/down and left/right never together: those are the first four
        const bool pressed = control < 4 ? (control % 2 == 0 && key % 5 == 0)
                                         : (key % 4 == 0);
        assignments[port * 12 + control] = {port, control, pressed ? 1 : 0};
      }
    auto input = initial<tessera_input_frame_v1>();
    input.epoch = now.frame_id.epoch;
    input.frame = now.frame_id.frame + 1;
    input.assignments = assignments.data();
    input.count = assignments.size();
    REQUIRE(tessera_session_step_v1(handle, &input, &context) == TESSERA_OK);
  }
  void step(unsigned frames) {
    for (unsigned i = 0; i < frames; ++i)
      step();
  }
  Bytes checkpoint() {
    std::uint64_t required{};
    REQUIRE(tessera_checkpoint_export_v1(handle, {nullptr, 0}, &required,
                                         &context) == TESSERA_BUFFER_TOO_SMALL);
    Bytes out(static_cast<std::size_t>(required));
    REQUIRE(tessera_checkpoint_export_v1(handle, {out.data(), out.size()},
                                         &required, &context) == TESSERA_OK);
    return out;
  }
  tessera_status restore(const Bytes &checkpoint) {
    return tessera_checkpoint_restore_v1(
        handle, {checkpoint.data(), checkpoint.size()}, {nullptr, 0}, &context);
  }
  Bytes original() {
    auto frame = initial<tessera_original_frame_v1>();
    REQUIRE(tessera_original_frame_query_v1(handle, &frame, &context) ==
            TESSERA_OK);
    Bytes out(frame.pixels.data, frame.pixels.data + frame.pixels.bytes);
    out.insert(out.end(), frame.pcm.data, frame.pcm.data + frame.pcm.bytes);
    return out;
  }
};

Bytes rom_a() { return tessera::testrom::pad_frames_rom(); }
Bytes rom_b() { return tessera::testrom::fm_psg_sweep_rom(5); }
} // namespace

TEST_CASE("a native checkpoint resumes exactly, in place and in a new session") {
  Native source(rom_a());
  source.step(20);
  const auto at_k = source.checkpoint();
  source.step(10);
  const auto expected_state = source.checkpoint();
  const auto expected_frame = source.original();

  // In the same session: go back to K and run again.
  REQUIRE(source.restore(at_k) == TESSERA_OK);
  CHECK(source.info().frame_id.frame == 20);
  CHECK(source.info().frame_id.epoch > source.epoch);
  source.step(10);
  CHECK(source.checkpoint() == expected_state);
  CHECK(source.original() == expected_frame);

  // In a new session of the same ROM.
  Native target(rom_a());
  target.step(3);
  REQUIRE(target.restore(at_k) == TESSERA_OK);
  CHECK(target.info().frame_id.frame == 20);
  target.step(10);
  CHECK(target.checkpoint() == expected_state);
  CHECK(target.original() == expected_frame);
}

TEST_CASE("another ROM, another codec or damage is refused and changes nothing") {
  Native source(rom_a());
  source.step(12);
  const auto checkpoint = source.checkpoint();

  Native other_rom(rom_b());
  other_rom.step(5);
  const auto before = other_rom.checkpoint();
  const auto epoch = other_rom.info().frame_id.epoch;
  CHECK(other_rom.restore(checkpoint) == TESSERA_CHECKPOINT_INCOMPATIBLE);
  CHECK(other_rom.info().frame_id.epoch == epoch);
  CHECK(other_rom.checkpoint() == before);

  Native victim(rom_a());
  victim.step(5);
  const auto victim_before = victim.checkpoint();
  auto corrupt = checkpoint;
  corrupt.back() ^= 1;
  CHECK(victim.restore(corrupt) == TESSERA_CHECKPOINT_INVALID);
  const Bytes truncated(checkpoint.begin(), checkpoint.begin() + checkpoint.size() / 2);
  CHECK(victim.restore(truncated) != TESSERA_OK);
  CHECK(victim.restore({}) != TESSERA_OK);
  CHECK(victim.checkpoint() == victim_before);
  CHECK(victim.info().frame_id.frame == 5);
  victim.step(); // and it still runs
}

TEST_CASE("the Clown and synthetic codecs are refused, the native one accepted") {
  using namespace tessera;
  const Bytes sha(32, 0x11), build(32, 0x22), config(32, 0x33);
  const auto make = [&](std::string_view codec, std::string_view machine_tag) {
    InputState held{};
    Bytes b = wire::text("TSSTATE1");
    wire::field(b, 1, 6, wire::text(codec));
    wire::scalar(b, 2, 1, 1);
    wire::scalar(b, 3, 1, 0);
    wire::field(b, 4, 5, sha);
    wire::field(b, 5, 5, build);
    wire::field(b, 6, 5, config);
    wire::scalar(b, 7, 3, 0);
    wire::scalar(b, 8, 3, 0);
    wire::field(b, 9, 8, wire::inputs(held, kPadsInputLayout));
    Bytes machine;
    wire::field(machine, 1, TESSERA_ENCODING_BYTES, wire::text(machine_tag));
    wire::field(b, 65536, 11, machine);
    return b;
  };
  const auto decode = [&](const Bytes &state, std::string_view backend) {
    return persistence::decode_state(state, backend, sha, build, config,
                                     896040);
  };
  // The native codec decodes for the native backend.
  CHECK_NOTHROW(decode(make("tessera.megadrive.native", "TMDS"), "megadrive"));
  // A Clown record (`TSCLOWN1`) or a synthetic one is another contract: -20.
  for (const std::string_view codec :
       {"tessera.megadrive.clown", "tessera.synthetic"}) {
    CAPTURE(codec);
    try {
      decode(make(codec, "TSCLOWN1"), "megadrive");
      FAIL("expected a refusal");
    } catch (const Failure &failure) {
      CHECK(failure.code == TESSERA_CHECKPOINT_INCOMPATIBLE);
    }
  }
  // And the reverse: a native record is refused by the Clown backend.
  try {
    decode(make("tessera.megadrive.native", "TMDS"), "clownmdemu");
    FAIL("expected a refusal");
  } catch (const Failure &failure) {
    CHECK(failure.code == TESSERA_CHECKPOINT_INCOMPATIBLE);
  }
  // A machine record that is not a native one never reaches the machine.
  Native session(rom_a());
  session.step(2);
  const auto before = session.checkpoint();
  Bytes clown_record = wire::text("TSCLOWN1");
  clown_record.resize(4096, 0);
  CHECK(session.restore(clown_record) != TESSERA_OK);
  CHECK(session.checkpoint() == before);
}
