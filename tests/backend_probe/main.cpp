#include <nlohmann/json.hpp>
#include <picosha2.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Json = nlohmann::json;
using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t rom_size = 32 * 1024;
constexpr std::size_t entry_offset = 0x200;
constexpr std::uint32_t initial_stack = 0x00FFFF00;
constexpr std::uint32_t ram_input = 0x00FF0000;
constexpr std::uint32_t ram_fault = 0x00FF0002;
constexpr std::uint32_t pad_data = 0x00A10003;
constexpr std::uint32_t pad_control = 0x00A10009;
constexpr std::uint32_t vdp_data = 0x00C00000;
constexpr std::uint32_t vdp_control = 0x00C00004;
constexpr std::uint32_t psg_port = 0x00C00011;
constexpr std::uint16_t neutral_marker = 0x1357;
constexpr std::uint16_t active_marker = 0x2468;
constexpr std::uint16_t neutral_colour = 0x0E00;
constexpr std::uint16_t active_colour = 0x000E;

class FixtureError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void put_word(Bytes& bytes, const std::size_t offset, const std::uint16_t value) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw FixtureError("ROM_WRITE_OUT_OF_RANGE");
    }
    bytes[offset] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 1] = static_cast<std::uint8_t>(value & 0xFF);
}

void put_long(Bytes& bytes, const std::size_t offset, const std::uint32_t value) {
    put_word(bytes, offset, static_cast<std::uint16_t>(value >> 16));
    put_word(bytes, offset + 2, static_cast<std::uint16_t>(value & 0xFFFF));
}

void put_ascii(Bytes& bytes, const std::size_t offset, const std::size_t width,
               const std::string_view text) {
    if (offset > bytes.size() || width > bytes.size() - offset || text.size() > width) {
        throw FixtureError("ROM_TEXT_OUT_OF_RANGE");
    }
    std::fill_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), width,
                static_cast<std::uint8_t>(' '));
    for (std::size_t index = 0; index < text.size(); ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(text[index]);
    }
}

class RomEmitter {
public:
    RomEmitter() : bytes_(rom_size, 0) {}

    [[nodiscard]] std::size_t position() const noexcept { return position_; }

    void word(const std::uint16_t value) {
        put_word(bytes_, position_, value);
        position_ += 2;
    }

    void long_word(const std::uint32_t value) {
        word(static_cast<std::uint16_t>(value >> 16));
        word(static_cast<std::uint16_t>(value & 0xFFFF));
    }

    void move_byte(const std::uint8_t value, const std::uint32_t address) {
        word(0x13FC); // MOVE.B #imm,(abs.L).
        word(value);
        long_word(address);
    }

    void move_word(const std::uint16_t value, const std::uint32_t address) {
        word(0x33FC); // MOVE.W #imm,(abs.L).
        word(value);
        long_word(address);
    }

    void move_long(const std::uint32_t value, const std::uint32_t address) {
        word(0x23FC); // MOVE.L #imm,(abs.L).
        long_word(value);
        long_word(address);
    }

    [[nodiscard]] std::size_t branch(const std::uint16_t opcode) {
        const auto instruction = position_;
        word(opcode);
        word(0);
        return instruction;
    }

    void resolve_branch(const std::size_t instruction, const std::size_t target) {
        // Word branch displacements are relative to the extension word.
        const auto displacement = static_cast<std::ptrdiff_t>(target) -
                                  static_cast<std::ptrdiff_t>(instruction + 2);
        if (displacement < std::numeric_limits<std::int16_t>::min() ||
            displacement > std::numeric_limits<std::int16_t>::max() ||
            displacement % 2 != 0) {
            throw FixtureError("ROM_BRANCH_OUT_OF_RANGE");
        }
        put_word(bytes_, instruction + 2,
                 static_cast<std::uint16_t>(static_cast<std::int16_t>(displacement)));
    }

    void write_oracles(const bool active) {
        move_long(0x7FFC0003, vdp_control); // VRAM write at byte address 0xFFFC.
        move_word(active ? active_marker : neutral_marker, vdp_data);
        move_long(0xC0000000, vdp_control); // CRAM write at colour index zero.
        move_word(active ? active_colour : neutral_colour, vdp_data);
        move_byte(active ? 0x90 : 0x9F, psg_port);
    }

    [[nodiscard]] Bytes finish(const std::size_t fault_handler) {
        put_long(bytes_, 0, initial_stack);
        put_long(bytes_, 4, static_cast<std::uint32_t>(entry_offset));
        for (std::size_t index = 2; index < 64; ++index) {
            put_long(bytes_, index * 4, static_cast<std::uint32_t>(fault_handler));
        }
        put_ascii(bytes_, 0x100, 16, "SEGA MEGA DRIVE");
        put_ascii(bytes_, 0x110, 16, "(C)TESSERA 2026");
        put_ascii(bytes_, 0x120, 48, "TESSERA PHASE 0 INPUT AND PSG PROBE");
        put_ascii(bytes_, 0x150, 48, "TESSERA PHASE 0 INPUT AND PSG PROBE");
        put_ascii(bytes_, 0x180, 14, "GM TESSERA-00");
        put_ascii(bytes_, 0x190, 16, "J");
        put_long(bytes_, 0x1A0, 0);
        put_long(bytes_, 0x1A4, static_cast<std::uint32_t>(rom_size - 1));
        put_long(bytes_, 0x1A8, 0x00FF0000);
        put_long(bytes_, 0x1AC, 0x00FFFFFF);
        put_ascii(bytes_, 0x1F0, 16, "U");

        std::uint32_t checksum = 0;
        for (std::size_t offset = entry_offset; offset < bytes_.size(); offset += 2) {
            checksum += (static_cast<std::uint32_t>(bytes_[offset]) << 8) |
                        bytes_[offset + 1];
        }
        put_word(bytes_, 0x18E, static_cast<std::uint16_t>(checksum & 0xFFFF));
        return std::move(bytes_);
    }

private:
    Bytes bytes_;
    std::size_t position_{entry_offset};
};

[[nodiscard]] Bytes make_rom() {
    RomEmitter emitter;
    emitter.word(0x46FC); // MOVE.W #0x2700,SR: supervisor mode, interrupts masked.
    emitter.word(0x2700);
    emitter.move_long(0x53454741, 0x00A14000); // TMSS signature.
    emitter.move_word(0, ram_fault);
    emitter.move_byte(0x40, pad_control); // TH is an output; other pins are inputs.
    emitter.move_byte(0x40, pad_data); // TH high exposes button B in bit four.

    // Power-on VRAM is zero in the two pinned candidates. The marker lies outside
    // the configured name tables, sprite table, and full-screen scroll words.
    constexpr std::array<std::uint16_t, 14> registers{
        0x8004, 0x8144, 0x8230, 0x8334, 0x8407, 0x8578, 0x8700,
        0x8B00, 0x8C81, 0x8D3F, 0x8F02, 0x9001, 0x9100, 0x9200
    };
    for (const auto value : registers) {
        emitter.move_word(value, vdp_control);
    }
    constexpr std::array<std::uint8_t, 6> psg_initialisation{0x9F, 0xBF, 0xDF, 0xFF, 0x80, 0x10};
    for (const auto command : psg_initialisation) {
        emitter.move_byte(command, psg_port);
    }

    const auto loop = emitter.position();
    emitter.word(0x1039); // MOVE.B (abs.L),D0.
    emitter.long_word(pad_data);
    emitter.word(0x0200); // ANDI.B #0x10,D0.
    emitter.word(0x0010);
    emitter.word(0x13C0); // MOVE.B D0,(abs.L); preserves the relevant zero result.
    emitter.long_word(ram_input);
    const auto pressed_branch = emitter.branch(0x6700); // BEQ.W.
    emitter.write_oracles(false);
    const auto neutral_loop = emitter.branch(0x6000); // BRA.W.
    emitter.resolve_branch(neutral_loop, loop);
    const auto pressed = emitter.position();
    emitter.resolve_branch(pressed_branch, pressed);
    emitter.write_oracles(true);
    const auto active_loop = emitter.branch(0x6000);
    emitter.resolve_branch(active_loop, loop);

    const auto fault_handler = emitter.position();
    emitter.move_word(0xDEAD, ram_fault);
    emitter.word(0x60FE); // BRA.S to this instruction after reporting an exception.
    return emitter.finish(fault_handler);
}

[[nodiscard]] Json controls(const bool pressed) {
    return Json{{"up", false}, {"down", false}, {"left", false}, {"right", false},
                {"a", false}, {"b", pressed}, {"c", false}, {"start", false}};
}

[[nodiscard]] Json make_trace(const std::uint32_t count, const bool action) {
    const bool short_trace = count == 60;
    const std::uint32_t first_pressed = short_trace ? 21 : 301;
    const std::uint32_t last_pressed = short_trace ? 40 : 600;
    Json frames = Json::array();
    for (std::uint32_t frame = 1; frame <= count; ++frame) {
        const bool pressed = action && frame >= first_pressed && frame <= last_pressed;
        frames.push_back(Json{
            {"frame", frame},
            {"ports", Json::array({Json{{"port", 0}, {"controls", controls(pressed)}}})}
        });
    }
    return Json{
        {"trace_version", 1},
        {"format_scope", "phase0_probe_private_not_native_api_abi"},
        {"system", "mega_drive"},
        {"controller_profile", "one_three_button_pad"},
        {"frames", std::move(frames)}
    };
}

[[nodiscard]] Json oracle(const bool active) {
    return Json{
        {"ram_input_byte", active ? 0 : 0x10},
        {"ram_fault_word", 0},
        {"vram_marker_word", active ? active_marker : neutral_marker},
        {"cram_colour_zero", active ? active_colour : neutral_colour},
        {"psg_tone_zero_period", 0x100},
        {"psg_tone_zero_attenuation", active ? 0 : 15},
        {"audio", active ? "active_periodic_signal" : "silent_after_filter_transient"}
    };
}

[[nodiscard]] Json make_expectations() {
    return Json{
        {"oracle_version", 1},
        {"basis", "generator_source_before_emulator_execution"},
        {"evidence", "declared_expectations_not_execution_results"},
        {"addresses", Json{
            {"ram_input_byte", ram_input}, {"ram_fault_word", ram_fault},
            {"vram_marker_byte_address", 0xFFFC}, {"cram_colour_index", 0}
        }},
        {"neutral", oracle(false)},
        {"pressed", oracle(true)},
        {"short", Json{
            {"frames", 60}, {"pressed_first", 21}, {"pressed_last", 40},
            {"sample_frames", Json::array({10, 30, 50})},
            {"central_windows", Json::array({
                Json{{"first", 5}, {"last", 15}, {"state", "neutral"}},
                Json{{"first", 25}, {"last", 35}, {"state", "pressed"}},
                Json{{"first", 45}, {"last", 55}, {"state", "neutral"}}
            })}
        }},
        {"full", Json{
            {"frames", 3000}, {"pressed_first", 301}, {"pressed_last", 600},
            {"sample_frames", Json::array({150, 450, 750})},
            {"central_windows", Json::array({
                Json{{"first", 100}, {"last", 200}, {"state", "neutral"}},
                Json{{"first", 350}, {"last", 550}, {"state", "pressed"}},
                Json{{"first", 650}, {"last", 850}, {"state", "neutral"}}
            })}
        }},
        {"control_traces", "neutral_in_every_frame"},
        {"video_rule", "cram_oracles_exact_rgb_change_checked_per_backend"},
        {"audio_rule", "generate_samples_in_all_windows_active_varies_control_is_silent_allow_filter_transients"},
        {"audio_stage", "identify_actual_backend_stream_format_and_filters_not_assumed_48khz_mix"},
        {"initial_state_precondition", "pinned_backend_power_on_vram_cram_vsram_zero"},
        {"limits", Json::array({
            "no_cross_backend_hash_equivalence",
            "no_native_api_or_engine_acceptance_evidence",
            "no_runtime_audio_substitution",
            "no_graphics_replacement_package"
        })}
    };
}

void write_bytes(const std::filesystem::path& path, const std::span<const std::uint8_t> bytes) {
    std::ofstream stream;
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.open(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    stream.close();
}

void generate(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    Json artifacts = Json::array();
    const auto write_artifact = [&](const std::string& name, const Bytes& bytes) {
        write_bytes(directory / name, bytes);
        artifacts.push_back(Json{{"path", name}, {"size_bytes", bytes.size()},
                                  {"sha256", picosha2::hash256_hex_string(bytes)}});
    };
    const auto write_json = [&](const std::string& name, const Json& value) {
        const auto text = value.dump(2) + '\n';
        write_artifact(name, Bytes{text.begin(), text.end()});
    };

    write_artifact("program.md", make_rom());
    write_json("short-control.json", make_trace(60, false));
    write_json("short-action.json", make_trace(60, true));
    write_json("full-control.json", make_trace(3000, false));
    write_json("full-action.json", make_trace(3000, true));
    write_json("expected.json", make_expectations());
    const Json manifest{
        {"manifest_version", 1},
        {"fixture_id", "tessera-phase0-input-psg-v1"},
        {"generator", "tests/backend_probe/main.cpp"},
        {"provenance", "original_local_opcode_emitter_no_external_fixture_source"},
        {"hash_algorithm", "sha256"},
        {"hash_scope", "generated_artifacts_except_manifest"},
        {"execution_results", "not_recorded_in_fixture"},
        {"artifacts", std::move(artifacts)}
    };
    const auto text = manifest.dump(2) + '\n';
    write_bytes(directory / "manifest.json", Bytes{text.begin(), text.end()});
}

} // namespace

int main(const int argc, char* argv[]) {
    if (argc != 3 || std::string_view{argv[1]} != "--generate") {
        std::cerr << "Uso: tessera_backend_probe --generate <directorio>\n";
        return 2;
    }
    try {
        generate(std::filesystem::path{argv[2]});
        std::cout << "FIXTURE_GENERATED: archivos generados en " << argv[2] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FIXTURE_GENERATION_FAILED: no se pudo generar el fixture: "
                  << error.what() << '\n';
        return 1;
    }
}
