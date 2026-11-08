// TP-201.3 (RF-3.8, RF-3.9): the subscriptions of the observation API on the
// native Mega Drive backend.
//  - arranca sin nada activo; una mascara pedida entre frames pasa a activa al
//    empezar el siguiente frame; los bits desconocidos se rechazan;
//  - una region cuya suscripcion esta inactiva responde NOT_SUBSCRIBED y no
//    hace trabajo (el contador queda a cero); SYSTEM y Z80_RAM no piden nada;
//  - las respuestas coinciden con la matriz medida sobre gpgx r4
//    (tests/fixtures/diffbench/subscription-matrix.txt);
//  - el transporte de audio events solo corre con AUDIO_EVENTS activa.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "domain/wire.hpp"
#include "md/regions.hpp"
#include "md/vdp_render.hpp"
#include "session/session.hpp"
#include "support/md_testrom.hpp"
#include <tessera/tessera.h>

#include <array>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
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
        const auto key =
            static_cast<unsigned>(now.frame_id.frame * 7 + control * 3 + port);
        // up/down and left/right never together: those are the first four
        const bool pressed =
            control < 4 ? (control % 2 == 0 && key % 5 == 0) : (key % 4 == 0);
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

using tessera::Bytes;
constexpr std::uint32_t kAll = TESSERA_OBS_SUB_ALL;

struct Observed : Native {
  using Native::Native;
  tessera_status set(std::uint32_t mask) {
    return tessera_observation_subscriptions_set_v1(handle, mask, &context);
  }
  tessera_obs_subscription_state_v1 state() {
    tessera_obs_subscription_state_v1 s{};
    REQUIRE(tessera_observation_subscriptions_get_v1(handle, &s, sizeof(s),
                                                     &context) == TESSERA_OK);
    return s;
  }
  tessera_status read(std::uint32_t region, Bytes *out = nullptr) {
    tessera_obs_region_info_v1 info{};
    REQUIRE(tessera_observation_region_query_v1(
                handle, region, &info, sizeof(info), &context) == TESSERA_OK);
    Bytes buffer(info.byte_size);
    std::uint64_t generation = 0;
    const auto status = tessera_observation_region_read_v1(
        handle, region, 0, buffer.data(), info.byte_size, UINT64_MAX,
        &generation, &context);
    if (out)
      *out = buffer;
    return status;
  }
  std::uint64_t work() {
    return tessera::sessions.get(handle)
        ->machine->observation()
        ->observation_work();
  }
};

Bytes rom_idle() { return tessera::testrom::vint_counter_rom(); }
Bytes rom_audio() { return tessera::testrom::fm_psg_sweep_rom(5); }

/// What the oracle answered, translated to Tessera's statuses.
tessera_status expected_status(int reference) {
  return reference == 0    ? TESSERA_OK
         : reference == -9 ? TESSERA_NOT_SUBSCRIBED
                           : reference;
}
/// Regions the native machine serves today (TP-201.1/.2, PALETTE and the raster
/// regions of TP-203/204).
bool served(unsigned region) {
  return region == 1 || region == 2 || region == 3 || region == 4 ||
         region == 13 || region == 14 || region == 17 || region == 18 ||
         region == 24 || region == 25 || region == 26 || region == 16 ||
         region == 19 || region == 20 || region == 21 || region == 22 ||
         region == 5 || region == 10;
}
} // namespace

TEST_CASE("nothing is active at the start and unknown bits are refused") {
  Observed s(rom_idle());
  auto state = s.state();
  CHECK(state.struct_size == sizeof(state));
  CHECK(state.state_version == 1);
  CHECK(state.supported_mask == kAll);
  CHECK(state.active_mask == 0);
  CHECK(state.requested_mask == 0);
  CHECK(state.activation_frame == 0);

  CHECK(s.set(kAll + 1) == TESSERA_INVALID_ARGUMENT);
  CHECK(s.set(UINT32_C(1) << 31) == TESSERA_INVALID_ARGUMENT);
  state = s.state();
  CHECK(state.requested_mask == 0); // refused: nothing changed
  CHECK(s.set(TESSERA_OBS_SUB_VDP_MEMORY) == TESSERA_OK);
  CHECK(s.state().requested_mask == TESSERA_OBS_SUB_VDP_MEMORY);
  CHECK(s.state().active_mask == 0); // active only from the next frame
}

TEST_CASE("a request becomes active when the next frame starts") {
  Observed s(rom_idle());
  REQUIRE(s.set(TESSERA_OBS_SUB_VDP_MEMORY) == TESSERA_OK);
  CHECK(s.read(1) == TESSERA_NOT_SUBSCRIBED); // requested is not active
  s.step();
  CHECK(s.state().active_mask == TESSERA_OBS_SUB_VDP_MEMORY);
  CHECK(s.state().activation_frame == 1);
  CHECK(s.read(1) == TESSERA_OK);
  s.step(3);
  CHECK(s.state().activation_frame == 4);
  // back to nothing: idle again from the next frame
  REQUIRE(s.set(0) == TESSERA_OK);
  CHECK(s.read(1) == TESSERA_OK); // still active until the frame starts
  s.step();
  CHECK(s.read(1) == TESSERA_NOT_SUBSCRIBED);
}

TEST_CASE("without subscriptions the observation does no work") {
  Observed s(rom_audio());
  s.step(20);
  CHECK(s.read(1) == TESSERA_NOT_SUBSCRIBED);
  CHECK(s.read(11) == TESSERA_NOT_SUBSCRIBED);
  std::uint32_t count = 0;
  Bytes events(80 * 8);
  CHECK(tessera_observation_audio_events_poll_v1(s.handle, events.data(), 8, 80,
                                                 &count, &s.context) ==
        TESSERA_NOT_SUBSCRIBED);
  CHECK(count == 0);
  CHECK(s.work() == 0); // the counter stays at zero

  // what the machine serves, once subscribed, is the work
  REQUIRE(s.set(TESSERA_OBS_SUB_VDP_MEMORY) == TESSERA_OK);
  s.step();
  CHECK(s.read(1) == TESSERA_OK);
  CHECK(s.work() == 65536); // only what the read delivered
  const auto before = s.work();
  CHECK(s.read(26) == TESSERA_OK); // needs no subscription, still counts
  CHECK(s.work() == before + 8192);
}

TEST_CASE("the regions read as the machine holds them") {
  Observed s(rom_audio());
  REQUIRE(s.set(kAll) == TESSERA_OK);
  s.step(30);
  Bytes vram, cram, regs, vsram, z80, system, palette;
  REQUIRE(s.read(1, &vram) == TESSERA_OK);
  REQUIRE(s.read(2, &cram) == TESSERA_OK);
  REQUIRE(s.read(3, &regs) == TESSERA_OK);
  REQUIRE(s.read(4, &vsram) == TESSERA_OK);
  REQUIRE(s.read(26, &z80) == TESSERA_OK);
  REQUIRE(s.read(18, &system) == TESSERA_OK);
  REQUIRE(s.read(24, &palette) == TESSERA_OK);
  CHECK(vram.size() == 65536);
  CHECK(cram.size() == 128);
  CHECK(regs.size() == 32);
  CHECK(vsram.size() == 128);
  CHECK(z80.size() == 8192);
  CHECK(system.size() == 44);
  CHECK(palette.size() == 512);
  // SYSTEM: its own size, the layout version and the Mega Drive hardware id
  CHECK(system[0] == 44);
  CHECK(system[4] == 1);
  CHECK(system[8] == 0x80);
  // a range inside a region, and one that is not
  Bytes part(16);
  std::uint64_t generation = 0;
  CHECK(tessera_observation_region_read_v1(s.handle, 3, 8, part.data(), 16,
                                           UINT64_MAX, &generation,
                                           &s.context) == TESSERA_OK);
  CHECK(std::equal(part.begin(), part.end(), regs.begin() + 8));
  CHECK(tessera_observation_region_read_v1(
            s.handle, 3, 24, part.data(), 16, UINT64_MAX, &generation,
            &s.context) == TESSERA_INVALID_ARGUMENT);
}

TEST_CASE("reading does not change the machine") {
  Observed observed(rom_audio());
  Observed control(rom_audio());
  REQUIRE(observed.set(kAll) == TESSERA_OK);
  observed.step(20);
  control.step(20);
  for (std::uint32_t region : {1u, 2u, 3u, 4u, 18u, 24u, 26u})
    REQUIRE(observed.read(region) == TESSERA_OK);
  observed.step(10);
  control.step(10);
  CHECK(observed.checkpoint() == control.checkpoint());
}

TEST_CASE("recompose_frame draws the last frame again (TP-205.1)") {
  Observed s(rom_idle());
  std::vector<std::uint16_t> pixels(320 * 240);
  std::uint32_t width = 0, height = 0;
  const auto recompose = [&](std::uint32_t capacity) {
    return tessera_observation_recompose_v1(s.handle, pixels.data(), capacity,
                                            0, &width, &height, &s.context);
  };
  s.step(3);
  CHECK(recompose(320 * 240) == TESSERA_NOT_SUBSCRIBED);
  REQUIRE(s.set(TESSERA_OBS_SUB_RECOMPOSITION) == TESSERA_OK);
  CHECK(recompose(320 * 240) == TESSERA_NOT_SUBSCRIBED); // from the next frame
  s.step();
  const auto before = s.checkpoint();
  REQUIRE(recompose(320 * 240) == TESSERA_OK);
  const auto frame = s.original();
  // A ROM with no raster change recomposes to the frame it emitted.
  REQUIRE(frame.size() >= std::size_t{width} * height * 4);
  CHECK(width == 256); // vint_counter is H32
  CHECK(height == 224);
  bool same = true;
  for (std::size_t i = 0; i < std::size_t{width} * height; ++i) {
    const std::uint32_t c =
        frame[4 * i] | frame[4 * i + 1] << 8 | frame[4 * i + 2] << 16;
    same = same && pixels[i] == tessera::md::rgba_to_565(c);
  }
  CHECK(same);
  // A buffer smaller than the frame is refused (gpgx: RC_INVALID_PARAMS) and
  // nothing changes in the machine.
  CHECK(recompose(width * height - 1) == TESSERA_INVALID_ARGUMENT);
  CHECK(tessera_observation_recompose_v1(s.handle, nullptr, 320 * 240, 0,
                                         &width, &height, &s.context) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(s.checkpoint() == before);
}

TEST_CASE("LAYER_MASK and LAYER_DIM through write_control (TP-205.2)") {
  Observed s(rom_idle());
  Observed plain(rom_idle()); // the same frames with no control written
  const auto write = [&](std::uint32_t region, Bytes data,
                         std::uint64_t *generation = nullptr) {
    std::uint64_t g = 0;
    const auto status = tessera_observation_write_control_v1(
        s.handle, region, 0, data.data(),
        static_cast<std::uint32_t>(data.size()), UINT64_MAX, &g, &s.context);
    if (generation)
      *generation = g;
    return status;
  };
  const auto value = [&](std::uint32_t region) {
    Bytes out;
    REQUIRE(s.read(region, &out) == TESSERA_OK);
    REQUIRE(out.size() == 1);
    return out[0];
  };
  s.step(2);
  plain.step(2);
  // Before any write gpgx reads 0xFF and 0, with nothing subscribed.
  CHECK(value(TESSERA_OBS_REGION_LAYER_MASK) == 0xFF);
  CHECK(value(TESSERA_OBS_REGION_LAYER_DIM) == 0);
  std::uint64_t before = 0, after = 0;
  REQUIRE(write(TESSERA_OBS_REGION_LAYER_MASK, {0x0F}, &before) == TESSERA_OK);
  // Refused: a mask beyond the four layers, a dim other than 0 or 1, more
  // bytes than the region has. A refused write leaves the generation.
  CHECK(write(TESSERA_OBS_REGION_LAYER_MASK, {0x10}) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(write(TESSERA_OBS_REGION_LAYER_MASK, {0xFF}) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(write(TESSERA_OBS_REGION_LAYER_DIM, {2}) == TESSERA_INVALID_ARGUMENT);
  CHECK(write(TESSERA_OBS_REGION_LAYER_DIM, {1, 0}) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(write(TESSERA_OBS_REGION_AUDIO_MUTE, {1}) ==
        TESSERA_UNSUPPORTED_CAPABILITY); // TP-209
  REQUIRE(write(TESSERA_OBS_REGION_LAYER_DIM, {1}, &after) == TESSERA_OK);
  CHECK(after == before + 1);
  CHECK(value(TESSERA_OBS_REGION_LAYER_DIM) == 1);
  // Without RENDER_CONTROLS the controls are kept but do nothing.
  s.step();
  plain.step();
  CHECK(s.original() == plain.original());
  // With it, from the next frame: LAYER_DIM darkens every channel.
  REQUIRE(s.set(TESSERA_OBS_SUB_RENDER_CONTROLS |
                TESSERA_OBS_SUB_RECOMPOSITION) == TESSERA_OK);
  s.step();
  plain.step();
  const auto dark = s.original(), lit = plain.original();
  REQUIRE(dark.size() == lit.size());
  const std::size_t pixels = 256 * 224;
  bool dimmed = true;
  for (std::size_t i = 0; i < pixels * 4; i += 4)
    for (unsigned c = 0; c < 3; ++c) {
      const unsigned bits = c == 1 ? 6 : 5;
      const unsigned v = (lit[i + c] >> (8 - bits)) >> 2;
      const unsigned expanded =
          bits == 6 ? (v << 2 | v >> 4) : (v << 3 | v >> 2);
      dimmed = dimmed && dark[i + c] == expanded;
    }
  CHECK(dimmed);
  // recompose_multilayer: five dimmed layers; the composite is the frame.
  std::array<std::vector<std::uint16_t>, 5> layers;
  for (auto &layer : layers)
    layer.assign(pixels, 0);
  std::uint32_t width = 0, height = 0;
  const auto multilayer = [&](std::uint32_t capacity) {
    return tessera_observation_recompose_multilayer_v1(
        s.handle, layers[0].data(), layers[1].data(), layers[2].data(),
        layers[3].data(), layers[4].data(), capacity, 0, &width, &height,
        &s.context);
  };
  REQUIRE(multilayer(pixels) == TESSERA_OK);
  CHECK(width == 256);
  CHECK(height == 224);
  bool composite = true;
  for (std::size_t i = 0; i < pixels; ++i)
    composite = composite &&
                layers[4][i] == tessera::md::rgba_to_565(dark[4 * i] |
                                                         dark[4 * i + 1] << 8 |
                                                         dark[4 * i + 2] << 16);
  CHECK(composite);
  CHECK(multilayer(pixels - 1) == TESSERA_INVALID_ARGUMENT);
  CHECK(tessera_observation_recompose_multilayer_v1(
            s.handle, layers[0].data(), nullptr, layers[2].data(),
            layers[3].data(), layers[4].data(), pixels, 0, &width, &height,
            &s.context) == TESSERA_INVALID_ARGUMENT);
  // RF-3.9: the controls are part of the state.
  REQUIRE(write(TESSERA_OBS_REGION_LAYER_MASK, {0x05}) == TESSERA_OK);
  const auto saved = s.checkpoint();
  REQUIRE(write(TESSERA_OBS_REGION_LAYER_MASK, {0x0F}) == TESSERA_OK);
  REQUIRE(write(TESSERA_OBS_REGION_LAYER_DIM, {0}) == TESSERA_OK);
  REQUIRE(s.restore(saved) == TESSERA_OK);
  CHECK(value(TESSERA_OBS_REGION_LAYER_MASK) == 0x05);
  CHECK(value(TESSERA_OBS_REGION_LAYER_DIM) == 1);
}

TEST_CASE("recompose_multilayer needs RECOMPOSITION (TP-205.2)") {
  Observed s(rom_idle());
  std::array<std::vector<std::uint16_t>, 5> l;
  for (auto &layer : l)
    layer.assign(256 * 224, 0);
  std::uint32_t width = 0, height = 0;
  s.step(2);
  CHECK(tessera_observation_recompose_multilayer_v1(
            s.handle, l[0].data(), l[1].data(), l[2].data(), l[3].data(),
            l[4].data(), 256 * 224, 0, &width, &height,
            &s.context) == TESSERA_NOT_SUBSCRIBED);
}

TEST_CASE("the audio event transport only runs while subscribed") {
  Observed s(rom_audio());
  std::uint32_t count = 0;
  Bytes events(80 * 64);
  s.step(5);
  tessera_obs_audio_transport_stats_v1 stats{};
  CHECK(tessera_observation_audio_transport_stats_v1(
            s.handle, &stats, sizeof(stats), &s.context) == TESSERA_OK);
  CHECK(stats.pending == 0); // the stats answer even when idle

  REQUIRE(s.set(TESSERA_OBS_SUB_AUDIO_EVENTS) == TESSERA_OK);
  s.step(20);
  CHECK(tessera_observation_audio_events_poll_v1(
            s.handle, events.data(), 64, 80, &count, &s.context) == TESSERA_OK);
  CHECK(count > 0);
  CHECK(tessera_observation_audio_events_poll_v1(s.handle, events.data(), 64,
                                                 64, &count, &s.context) ==
        TESSERA_INVALID_ARGUMENT); // not an 80 byte event
  CHECK(s.work() >= 80u * count);
}

TEST_CASE("the answers match the matrix measured on the reference core") {
  std::ifstream matrix(std::string(TESSERA_SOURCE_DIR) +
                       "/tests/fixtures/diffbench/subscription-matrix.txt");
  REQUIRE(matrix.good());
  struct Key {
    unsigned mask, frames;
    bool operator<(const Key &o) const {
      return std::tie(mask, frames) < std::tie(o.mask, o.frames);
    }
  };
  struct Case {
    int set_status = 0;
    unsigned after_active = 0, after_requested = 0, end_active = 0,
             end_requested = 0;
    unsigned long long after_frame = 0, end_frame = 0;
    std::map<unsigned, int> reads;
  };
  std::map<Key, Case> cases;
  std::map<unsigned, std::tuple<int, int, int>> audio;
  std::string line;
  while (std::getline(matrix, line)) {
    std::istringstream in(line);
    std::string kind;
    in >> kind;
    if (kind == "audio") {
      unsigned mask;
      int poll, events, transport;
      in >> std::hex >> mask >> std::dec >> poll >> events >> transport;
      audio[mask] = {poll, events, transport};
      continue;
    }
    unsigned mask, frames;
    in >> std::hex >> mask >> std::dec >> frames;
    auto &c = cases[{mask, frames}];
    if (kind == "set") {
      in >> c.set_status;
    } else if (kind == "state") {
      std::string tag;
      unsigned long long frame;
      unsigned active, requested;
      in >> tag >> active >> requested >> frame;
      if (tag == "after_set") {
        c.after_active = active;
        c.after_requested = requested;
        c.after_frame = frame;
      } else {
        c.end_active = active;
        c.end_requested = requested;
        c.end_frame = frame;
      }
    } else if (kind == "read") {
      unsigned region;
      int status;
      in >> region >> status;
      c.reads[region] = status;
    }
  }
  REQUIRE(cases.size() >= 27);

  for (const auto &[key, expected] : cases) {
    INFO("mask 0x" << std::hex << key.mask << std::dec << " after "
                   << key.frames << " frames");
    Observed s(rom_idle());
    CHECK(s.set(key.mask) == expected_status(expected.set_status));
    auto state = s.state();
    CHECK(state.active_mask == expected.after_active);
    CHECK(state.requested_mask == expected.after_requested);
    CHECK(state.activation_frame == expected.after_frame);
    s.step(key.frames);
    state = s.state();
    CHECK(state.active_mask == expected.end_active);
    CHECK(state.requested_mask == expected.end_requested);
    CHECK(state.activation_frame == expected.end_frame);
    for (const auto &[region, reference] : expected.reads) {
      const auto native = s.read(region);
      if (reference == -9)
        CHECK_MESSAGE(native == TESSERA_NOT_SUBSCRIBED, "region " << region);
      else if (served(region))
        CHECK_MESSAGE(native == TESSERA_OK, "region " << region);
      else
        CHECK_MESSAGE(native == TESSERA_UNSUPPORTED_CAPABILITY,
                      "region " << region << " is not served yet");
    }
  }
  for (const auto &[mask, answer] : audio) {
    INFO("audio mask 0x" << std::hex << mask);
    Observed s(rom_audio());
    REQUIRE(s.set(mask) == TESSERA_OK);
    s.step(20);
    std::uint32_t count = 0;
    Bytes events(80 * 1024);
    const auto status = tessera_observation_audio_events_poll_v1(
        s.handle, events.data(), 1024, 80, &count, &s.context);
    CHECK(status == expected_status(std::get<0>(answer)));
    CHECK((count > 0) == (std::get<1>(answer) != 0));
    tessera_obs_audio_transport_stats_v1 stats{};
    CHECK(tessera_observation_audio_transport_stats_v1(
              s.handle, &stats, sizeof(stats), &s.context) ==
          expected_status(std::get<2>(answer)));
  }
}

namespace {
/// The test ROMs of tests/fixtures/diffbench/recompose-stats.txt by the name
/// tessera_md_testrom gives them.
Bytes rom_named(const std::string &name) {
  namespace rom = tessera::testrom;
  const auto number = [&](std::size_t prefix) {
    return static_cast<unsigned>(std::stoul(name.substr(prefix)));
  };
  if (name.rfind("modes-sweep-", 0) == 0)
    return rom::modes_sweep_rom(rom::modes_variant(number(12)));
  if (name.rfind("raster-bands-", 0) == 0)
    return rom::raster_bands_rom(rom::raster_variant(number(13)));
  if (name.rfind("mode4-scene-", 0) == 0)
    return rom::mode4_scene_rom(rom::mode4_variant(number(12)));
  FAIL("unknown test rom " << name);
  return {};
}

/// A gpgx status in the fixture as Tessera returns it (the bench mapping).
std::string native_statuses(const std::string &line) {
  const auto at = line.find("status=");
  if (at == std::string::npos)
    return line;
  const auto end = line.find(' ', at);
  const int reference = std::stoi(line.substr(at + 7, end - at - 7));
  const int native = reference == -20 || reference == -21 || reference == -24
                         ? TESSERA_WRONG_STATE
                     : reference == -23 ? TESSERA_INVALID_ARGUMENT
                     : reference == -9  ? TESSERA_NOT_SUBSCRIBED
                                        : reference;
  return line.substr(0, at + 7) + std::to_string(native) +
         (end == std::string::npos ? "" : line.substr(end));
}

/// Runs a stats script (see compare_recompose_stats.py) on the native machine
/// through the public API and answers as tessera_diffbench does on gpgx.
std::vector<std::string>
run_stats_script(Observed &s, const std::string &script, unsigned frames) {
  std::map<unsigned, std::vector<std::string>> steps;
  std::istringstream per_frame(script);
  for (std::string part; std::getline(per_frame, part, ';');) {
    const auto colon = part.find(':');
    std::istringstream ops(part.substr(colon + 1));
    auto &list =
        steps[static_cast<unsigned>(std::stoul(part.substr(0, colon)))];
    for (std::string op; std::getline(ops, op, '.');)
      list.push_back(op);
  }
  std::vector<std::string> out;
  const std::uint32_t full = 320 * 240;
  std::vector<std::uint16_t> layers(5u * full);
  for (unsigned i = 0; i < frames; ++i) {
    s.step();
    for (const auto &op : steps[i]) {
      const auto at = op.find('@');
      const std::uint32_t capacity =
          at == std::string::npos
              ? full
              : static_cast<std::uint32_t>(std::stoul(op.substr(at + 1)));
      const auto argument =
          op.substr(1, at == std::string::npos ? std::string::npos : at - 1);
      const auto hex = [&] {
        return argument.empty() ? 0u
                                : static_cast<std::uint32_t>(
                                      std::stoul(argument, nullptr, 16));
      };
      std::ostringstream line;
      line << "frame=" << i << " ";
      tessera_status status = TESSERA_UNSUPPORTED_CAPABILITY;
      std::uint32_t width = 0, height = 0;
      if (op[0] == 'g') {
        tessera_obs_recompose_stats_v1 st{};
        status = tessera_observation_recompose_stats_v1(
            s.handle, &st,
            argument.empty() ? sizeof(st)
                             : static_cast<std::uint32_t>(std::stoul(argument)),
            &s.context);
        line << "stats status=" << status << " size=" << st.struct_size
             << " single=" << st.single_calls << "/" << st.single_hits
             << " multi=" << st.multilayer_calls << "/" << st.multilayer_hits
             << " fingerprint=" << std::hex << st.controls_fingerprint;
        out.push_back(line.str());
        continue;
      }
      if (op[0] == 's') {
        status = tessera_observation_recompose_v1(s.handle, layers.data(),
                                                  capacity, hex(), &width,
                                                  &height, &s.context);
      } else if (op[0] == 'm') {
        auto *l = layers.data();
        status = tessera_observation_recompose_multilayer_v1(
            s.handle, l, l + full, l + 2 * full, l + 3 * full, l + 4 * full,
            capacity, hex(), &width, &height, &s.context);
      } else if (op[0] == 'p') {
        const auto eq = op.find('=');
        const auto byte = static_cast<std::uint8_t>(
            std::stoul(op.substr(eq + 1), nullptr, 16));
        std::uint64_t generation = 0;
        status = tessera_observation_write_control_v1(
            s.handle,
            static_cast<std::uint32_t>(std::stoul(op.substr(1, eq - 1))), 0,
            &byte, 1, UINT64_MAX, &generation, &s.context);
      } else if (op[0] == 'u') {
        status = s.set(hex());
      } else if (op == "x") {
        status = s.restore(s.checkpoint());
      }
      line << op << " status=" << status;
      out.push_back(line.str());
    }
  }
  return out;
}
} // namespace

TEST_CASE("recompose_stats answers as gpgx did (TP-205.3)") {
  std::ifstream fixture(std::string(TESSERA_SOURCE_DIR) +
                        "/tests/fixtures/diffbench/recompose-stats.txt");
  REQUIRE(fixture.good());
  std::string header;
  std::vector<std::string> expected;
  unsigned cases = 0;
  for (std::string line; std::getline(fixture, line);) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;
    if (line.rfind("case ", 0) == 0) {
      header = line;
      expected.clear();
      continue;
    }
    if (line != "end") {
      expected.push_back(native_statuses(line));
      continue;
    }
    std::istringstream fields(header.substr(5));
    std::string rom, sub, script;
    fields >> rom >> sub >> script;
    CAPTURE(header);
    Observed s(rom_named(rom));
    REQUIRE(s.set(static_cast<std::uint32_t>(std::stoul(sub, nullptr, 16))) ==
            TESSERA_OK);
    const auto got = run_stats_script(s, script, 5);
    CHECK(got == expected);
    for (std::size_t i = 0; i < got.size() && i < expected.size(); ++i)
      if (got[i] != expected[i]) {
        MESSAGE("native: " << got[i] << " | gpgx: " << expected[i]);
        break;
      }
    ++cases;
  }
  CHECK(cases == 16);
}
