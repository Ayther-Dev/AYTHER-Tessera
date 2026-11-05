// TP-108.3 (RF-2.4): two pads in the API. A system that takes two ports of
// twelve controls is validated, serialized and bound to traces with its own
// layout, and the legacy one-port layout of the other backends is untouched.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "domain/wire.hpp"
#include "md/input.hpp"
#include "session/session.hpp"
#include "support/fx01.hpp"
#include "support/md_testrom.hpp"
#include <doctest/doctest.h>

using namespace tessera;

namespace {
/// A system that only declares the layout of the pads; every other member is
/// inert.
class TwoPads final : public IEmulatedSystem {
public:
  std::string_view backend_id() const noexcept override { return "two-pads"; }
  std::uint64_t capabilities() const noexcept override { return 0; }
  std::uint64_t frame() const noexcept override { return 0; }
  std::uint64_t tick() const noexcept override { return 0; }
  const InputState &latched_inputs() const noexcept override { return input_; }
  InputLayout input_layout() const noexcept override {
    return kPadsInputLayout;
  }
  const Bytes &pixels() const noexcept override { return bytes_; }
  const Bytes &pcm() const noexcept override { return bytes_; }
  bool original_available() const noexcept override { return false; }
  std::uint32_t system() const noexcept override { return 0; }
  std::uint32_t frame_width() const noexcept override { return 0; }
  std::uint32_t frame_height() const noexcept override { return 0; }
  std::uint64_t frame_ticks() const noexcept override { return 1; }
  std::uint64_t tick_hz() const noexcept override { return 1; }
  void step(const InputState &) override {}
  Bytes machine_record() const override { return {}; }
  void restore_record(std::span<const std::uint8_t>, std::uint64_t,
                      const InputState &) override {}
  std::vector<RegionData> regions(std::uint64_t) const override { return {}; }

private:
  InputState input_{};
  Bytes bytes_;
};

std::vector<tessera_input_assignment_v1> all(std::uint16_t ports,
                                             std::uint16_t controls) {
  std::vector<tessera_input_assignment_v1> out;
  for (std::uint16_t p = 0; p < ports; ++p)
    for (std::uint16_t c = 0; c < controls; ++c)
      out.push_back({p, c, 0});
  return out;
}

std::unique_ptr<Session> make_session() {
  const auto b = fx01::backend(false);
  const auto pm = fx01::memory();
  const auto rom = fx01::content(1);
  const auto cfg = fx01::config(b, pm);
  return std::make_unique<Session>(b.id, rom, cfg, pm,
                                   wire::View(b.build_id, 32));
}

tessera_status status_of(Session &s,
                         const std::vector<tessera_input_assignment_v1> &a) {
  try {
    (void)s.validate_inputs(a);
  } catch (const Failure &f) {
    return f.code;
  }
  return TESSERA_OK;
}
} // namespace

TEST_CASE("legacy layout: one port of eight, port 1 and control 8 rejected") {
  auto s = make_session();
  CHECK(s->machine->input_layout() == kLegacyInputLayout);
  CHECK(status_of(*s, all(1, 8)) == TESSERA_OK);
  CHECK(status_of(*s, all(2, 12)) != TESSERA_OK);
  auto bad = all(1, 8);
  bad[0].port = 1;
  CHECK(status_of(*s, bad) != TESSERA_OK);
}

TEST_CASE("two pads: 24 assignments complete, ports 0 and 1, controls 0-11") {
  auto s = make_session();
  s->machine = std::make_unique<TwoPads>();
  auto a = all(2, 12);
  CHECK(s->validate_inputs(a) == InputState{});
  a[12 + 11].value = 1; // port 1, mode
  a[5].value = 1;       // port 0, C
  const auto pressed = s->validate_inputs(a);
  CHECK(pressed[5] == 1);
  CHECK(pressed[23] == 1);

  CHECK(status_of(*s, all(1, 8)) == TESSERA_INPUT_INCOMPLETE);
  auto missing = all(2, 12);
  missing.pop_back();
  CHECK(status_of(*s, missing) == TESSERA_INPUT_INCOMPLETE);
  auto bad = all(2, 12);
  bad[0].port = 2;
  CHECK(status_of(*s, bad) == TESSERA_INVALID_ARGUMENT);
  bad = all(2, 12);
  bad[3].control = 12;
  CHECK(status_of(*s, bad) == TESSERA_INVALID_ARGUMENT);
}

TEST_CASE("two pads: a repeated assignment is duplicate or contradictory") {
  auto s = make_session();
  s->machine = std::make_unique<TwoPads>();
  auto a = all(2, 12);
  a.push_back({1, 4, 0});
  CHECK(status_of(*s, a) == TESSERA_INPUT_DUPLICATE);
  a.back().value = 1;
  CHECK(status_of(*s, a) == TESSERA_INPUT_CONTRADICTORY);
}

TEST_CASE("input record: 24 entries for two pads, 8 for the legacy layout") {
  InputState pads{};
  pads[1] = 1;
  pads[12 + 7] = 1;
  CHECK(wire::elements(wire::inputs(pads, kPadsInputLayout), 64).size() == 24);
  CHECK(wire::elements(wire::inputs(InputState{}), 64).size() == 8);
}

TEST_CASE("a trace of two pads is told apart from a legacy one") {
  auto make = [](InputLayout layout) {
    Bytes trace = wire::text("TSTRACE1");
    wire::field(trace, 1, TESSERA_ENCODING_BYTES, Bytes(32, 1));
    wire::field(trace, 2, TESSERA_ENCODING_BYTES, Bytes(32, 2));
    wire::scalar(trace, 3, TESSERA_ENCODING_U64, 2);
    std::vector<Bytes> frames;
    for (std::uint64_t f = 1; f <= 2; ++f) {
      InputState in{};
      in[layout.count() - 1] = f == 2;
      Bytes item;
      wire::scalar(item, 1, TESSERA_ENCODING_U64, f);
      wire::field(item, 2, TESSERA_ENCODING_ARRAY, wire::inputs(in, layout));
      frames.push_back(std::move(item));
    }
    wire::field(trace, 4, TESSERA_ENCODING_ARRAY, wire::array(frames));
    return trace;
  };
  const auto pads = wire::trace(make(kPadsInputLayout));
  CHECK(pads.layout == kPadsInputLayout);
  CHECK(pads.frames[1][23] == 1);
  CHECK(wire::trace(make(kLegacyInputLayout)).layout == kLegacyInputLayout);
}

TEST_CASE("pad masks follow the bit order of the machine") {
  InputState in{};
  in[0] = 1;       // port 0 up
  in[4] = 1;       // B
  in[11] = 1;      // mode
  in[12 + 3] = 1;  // port 1 right
  in[12 + 10] = 1; // Z
  CHECK(md::pad_mask(in, 0) == (md::kPadUp | md::kPadB | md::kPadMode));
  CHECK(md::pad_mask(in, 1) == (md::kPadRight | md::kPadZ));
  in[1] = 1; // up with down
  CHECK(!md::pad_mask(in, 0));
  CHECK(md::pad_mask(in, 1).has_value());
  in = {};
  in[12 + 2] = in[12 + 3] = 1; // left with right on port 1
  CHECK(!md::pad_mask(in, 1));
}

TEST_CASE("two pads reach the ports of the machine") {
  auto config = md::MachineConfig::for_region(md::Region::usa);
  config.pads = {md::PadType::six_button, md::PadType::three_button};
  auto run = [&](const InputState &in) {
    md::System system(tessera::testrom::pad_latch_rom(3), config);
    system.reset();
    CHECK(md::apply_input(system, in));
    for (int i = 0; i < 20; ++i)
      system.run_frame();
    const auto ram = system.work_ram();
    return std::vector<std::uint8_t>(ram.begin(), ram.begin() + 24);
  };
  InputState idle{}, a_only{}, both{};
  a_only[6] = 1; // port 0: A
  both[6] = 1;
  both[12 + 5] = 1; // port 1: C
  CHECK(run(a_only) != run(idle));
  // The same input twice gives the same bytes.
  CHECK(run(both) == run(both));
  md::System system(tessera::testrom::pad_latch_rom(3), config);
  InputState contradictory{};
  contradictory[1] = contradictory[0] = 1;
  CHECK_FALSE(md::apply_input(system, contradictory));
}
