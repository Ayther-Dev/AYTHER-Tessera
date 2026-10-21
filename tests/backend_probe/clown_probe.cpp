#include <clownmdemu.h>
#include <nlohmann/json.hpp>
#include <picosha2.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Json = nlohmann::json;
using Bytes = std::vector<std::uint8_t>;
void u16(Bytes& bytes, const std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}
std::string digest(const Bytes& bytes) { return picosha2::hash256_hex_string(bytes); }
struct Probe {
    std::unique_ptr<ClownMDEmu> machine = std::make_unique<ClownMDEmu>();
    ClownMDEmu_Callbacks callbacks{};
    std::vector<cc_u16l> cartridge;
    std::array<std::uint16_t, 256> palette{};
    Bytes pixels = Bytes(1024 * 1024 * 4);
    Bytes psg, fm;
    std::vector<cc_s16l> scratch = std::vector<cc_s16l>(32768);
    std::uint32_t width{}, height{}, scanlines{};
    std::uint64_t input_reads{}, psg_frames{}, fm_frames{};
    bool pressed{};

    static Probe& self(void* data) { return *static_cast<Probe*>(data); }
    Probe(const std::filesystem::path& rom) {
        psg.reserve(32768); fm.reserve(32768);
        std::ifstream input(rom, std::ios::binary);
        if (!input) throw std::runtime_error("ROM_OPEN_FAILED");
        const Bytes bytes{std::istreambuf_iterator<char>{input}, {}};
        if (bytes.size() != 32768) throw std::runtime_error("ROM_SIZE_INVALID");
        cartridge.reserve(bytes.size() / 2);
        for (std::size_t i = 0; i < bytes.size(); i += 2)
            cartridge.push_back(static_cast<cc_u16l>((bytes[i] << 8) | bytes[i + 1]));
        callbacks.user_data = this;
        callbacks.colour_updated = [](void* data, const cc_u16f index, const cc_u16f colour) {
            if (index < self(data).palette.size()) self(data).palette[index] = static_cast<std::uint16_t>(colour);
        };
        callbacks.scanline_rendered = [](void* data, const cc_u16f line, const cc_u8l* row,
            cc_u16f, cc_u16f, const cc_u16f w, const cc_u16f h) {
            auto& p = self(data);
            if (w > 1024 || h > 1024 || line >= h) return;
            p.width = w; p.height = h; ++p.scanlines;
            for (std::size_t x = 0; x < w; ++x) {
                const auto colour = p.palette[row[x]];
                const auto offset = (static_cast<std::size_t>(line) * w + x) * 4;
                p.pixels[offset] = static_cast<std::uint8_t>((colour & 15) * 17);
                p.pixels[offset + 1] = static_cast<std::uint8_t>(((colour >> 4) & 15) * 17);
                p.pixels[offset + 2] = static_cast<std::uint8_t>(((colour >> 8) & 15) * 17);
                p.pixels[offset + 3] = 255;
            }
        };
        callbacks.input_requested = [](void* data, const cc_u8f player, const ClownMDEmu_Button button) -> cc_bool {
            auto& p = self(data); ++p.input_reads;
            return player == 0 && button == CLOWNMDEMU_BUTTON_B && p.pressed;
        };
        callbacks.psg_audio_to_be_generated = [](void* data, ClownMDEmu* m, const std::size_t count,
            void (*generate)(ClownMDEmu*, cc_s16l*, std::size_t)) {
            auto& p = self(data);
            if (count > p.scratch.size()) throw std::runtime_error("PSG_BATCH_TOO_LARGE");
            std::fill_n(p.scratch.data(), count, 0); // Upstream adds each voice to the caller's buffer.
            generate(m, p.scratch.data(), count);
            for (std::size_t i = 0; i < count; ++i) u16(p.psg, static_cast<std::uint16_t>(p.scratch[i]));
            p.psg_frames += count;
        };
        callbacks.fm_audio_to_be_generated = [](void* data, ClownMDEmu* m, const std::size_t count,
            void (*generate)(ClownMDEmu*, cc_s16l*, std::size_t)) {
            auto& p = self(data);
            if (count * 2 > p.scratch.size()) throw std::runtime_error("FM_BATCH_TOO_LARGE");
            std::fill_n(p.scratch.data(), count * 2, 0);
            generate(m, p.scratch.data(), count);
            for (std::size_t i = 0; i < count * 2; ++i) u16(p.fm, static_cast<std::uint16_t>(p.scratch[i]));
            p.fm_frames += count;
        };
        // Iterate drains these generators even with the CD add-on disabled.
        const auto discardStereo = [](void* data, ClownMDEmu* m, const std::size_t count,
            void (*generate)(ClownMDEmu*, cc_s16l*, std::size_t)) {
            auto& p = self(data);
            if (count > p.scratch.size() / 2) throw std::runtime_error("CD_BATCH_TOO_LARGE");
            std::fill_n(p.scratch.data(), count * 2, 0);
            generate(m, p.scratch.data(), count);
        };
        callbacks.pcm_audio_to_be_generated = discardStereo;
        callbacks.cdda_audio_to_be_generated = discardStereo;
        callbacks.cd_audio_read = [](void*, cc_s16l* samples, const std::size_t count) -> std::size_t {
            std::fill_n(samples, count * 2, 0);
            return count;
        };
        ClownMDEmu_InitialConfiguration config{};
        config.general.region = CLOWNMDEMU_REGION_OVERSEAS;
        config.general.tv_standard = CLOWNMDEMU_TV_STANDARD_NTSC;
        config.general.cd_add_on_enabled = cc_false;
        config.controller_manager.protocol = CONTROLLER_MANAGER_PROTOCOL_STANDARD;
        ClownMDEmu_Initialise(machine.get(), &config, &callbacks);
        ClownMDEmu_SetCartridge(machine.get(), cartridge.data(), static_cast<cc_u32f>(cartridge.size()));
        ClownMDEmu_HardReset(machine.get(), cc_true, cc_false);
    }
    Bytes observable() const {
        Bytes result; result.reserve(132000);
        for (const auto word : machine->state.m68k.ram) u16(result, word);
        result.insert(result.end(), std::begin(machine->vdp.state.vram), std::end(machine->vdp.state.vram));
        for (const auto word : machine->vdp.state.cram) u16(result, word);
        return result; // Deliberately partial: not a machine checkpoint or canonical full state.
    }
};
Json run(const std::filesystem::path& directory, const std::filesystem::path& traceName) {
    std::ifstream traceInput(directory / traceName);
    if (!traceInput) throw std::runtime_error("TRACE_OPEN_FAILED");
    const auto trace = Json::parse(traceInput);
    std::ifstream expectedInput(directory / "expected.json");
    const auto expected = Json::parse(expectedInput);
    const auto& frames = trace.at("frames");
    if (frames.size() != 60 && frames.size() != 3000) throw std::runtime_error("TRACE_LENGTH_INVALID");
    Probe p(directory / "program.md");
    auto records = Json::array();
    bool oracles = true, audio = true, copies = true;
    std::string videoDigests, psgDigests, fmDigests, observableDigests;
    std::uint64_t frameNumber{};
    for (const auto& frame : frames) {
        ++frameNumber;
        if (frame.at("frame").get<std::uint64_t>() != frameNumber) throw std::runtime_error("TRACE_ORDER_INVALID");
        p.pressed = frame.at("ports").at(0).at("controls").at("b").get<bool>();
        p.psg.clear(); p.fm.clear(); p.psg_frames = p.fm_frames = 0; p.scanlines = 0;
        ClownMDEmu_Iterate(p.machine.get());
        const auto observed = p.observable();
        const auto observedHash = digest(observed);
        const auto video = Bytes{p.pixels.begin(), p.pixels.begin() + p.width * p.height * 4};
        const auto videoHash = digest(video), psgHash = digest(p.psg), fmHash = digest(p.fm);
        auto readerOne = observed, readerTwo = observed, presentation = video;
        std::fill(presentation.begin(), presentation.end(), 0);
        copies = copies && readerOne == readerTwo && observedHash == digest(p.observable()) && videoHash == digest(video);
        const auto inputByte = static_cast<std::uint32_t>(p.machine->state.m68k.ram[0] >> 8);
        const auto fault = p.machine->state.m68k.ram[1];
        const auto marker = static_cast<std::uint32_t>((p.machine->vdp.state.vram[0xFFFC] << 8) | p.machine->vdp.state.vram[0xFFFD]);
        const auto colour = p.machine->vdp.state.cram[0];
        const auto center = (static_cast<std::size_t>(p.height / 2) * p.width + p.width / 2) * 4;
        const bool videoOracle = p.width > 0 && p.height > 0 &&
            video[center] == (p.pressed ? 255 : 0) && video[center + 1] == 0 &&
            video[center + 2] == (p.pressed ? 0 : 255) && video[center + 3] == 255;
        const auto& oracle = expected.at(p.pressed ? "pressed" : "neutral");
        const bool central = (frames.size() == 60)
            ? ((frameNumber >= 5 && frameNumber <= 15) || (frameNumber >= 25 && frameNumber <= 35) || (frameNumber >= 45 && frameNumber <= 55))
            : ((frameNumber >= 100 && frameNumber <= 200) || (frameNumber >= 350 && frameNumber <= 550) || (frameNumber >= 650 && frameNumber <= 850));
        std::int32_t minimum = 32767, maximum = -32768;
        for (std::size_t i = 0; i + 1 < p.psg.size(); i += 2) {
            const auto raw = static_cast<std::uint16_t>(p.psg[i] | (p.psg[i + 1] << 8));
            const auto value = raw >= 32768 ? static_cast<std::int32_t>(raw) - 65536 : raw;
            minimum = std::min(minimum, value); maximum = std::max(maximum, value);
        }
        if (central) {
            oracles = oracles && videoOracle && p.scanlines > 0 && inputByte == oracle.at("ram_input_byte") && fault == 0 &&
                marker == oracle.at("vram_marker_word") && colour == oracle.at("cram_colour_zero") &&
                p.machine->psg.state.tones[0].countdown_master == 256 &&
                p.machine->psg.state.tones[0].attenuation == (p.pressed ? 0 : 15);
            audio = audio && !p.psg.empty() && (p.pressed ? maximum - minimum > 16 : maximum <= 2 && minimum >= -2);
        }
        records.push_back({{"frame", frameNumber}, {"input", inputByte}, {"fault", fault}, {"vram", marker},
            {"cram", colour}, {"psg_min", minimum}, {"psg_max", maximum}, {"psg_samples", p.psg_frames},
            {"width", p.width}, {"height", p.height}, {"scanlines", p.scanlines},
            {"center_rgba", p.width && p.height ? Json::array({video[center], video[center+1], video[center+2], video[center+3]}) : Json::array()},
            {"observable_sha256", observedHash}, {"video_sha256", videoHash}, {"psg_sha256", psgHash}, {"fm_sha256", fmHash}});
        observableDigests += observedHash; videoDigests += videoHash; psgDigests += psgHash; fmDigests += fmHash;
    }
    // Exercise upstream's backup omission directly, without writing native structs to disk.
    auto backup = std::make_unique<ClownMDEmu_StateBackup>();
    auto& controller = p.machine->controller_manager.state.ea_multitap.controllers[0];
    controller.countdown = 9876;
    ClownMDEmu_SaveState(p.machine.get(), backup.get());
    controller.countdown = 1234;
    ClownMDEmu_LoadState(p.machine.get(), backup.get());
    const bool controllerRestored = controller.countdown == 9876;
    return {{"backend", "ClownMDEmu"}, {"revision", "88ef45a6585556e2247dd26b6c937d6b9fb4a12d"},
        {"trace", traceName.string()}, {"completed_frames", frameNumber}, {"oracle_pass", oracles}, {"audio_pass", audio},
        {"copy_readers_pass", copies}, {"input_reads", p.input_reads}, {"records", std::move(records)},
        {"aggregate", {{"observable", picosha2::hash256_hex_string(observableDigests)},
            {"video", picosha2::hash256_hex_string(videoDigests)}, {"psg", picosha2::hash256_hex_string(psgDigests)}, {"fm", picosha2::hash256_hex_string(fmDigests)}}},
        {"audio_stage", "Separate native-rate S16LE PSG mono and FM stereo; low-pass filters enabled; no final canonical stereo mixer/resampler"},
        {"psg_rate", CLOWNMDEMU_PSG_SAMPLE_RATE_NTSC}, {"fm_rate", CLOWNMDEMU_FM_SAMPLE_RATE_NTSC},
        {"upstream_backup_controller_restored", controllerRestored}, {"upstream_backup_controller_expected", 9876},
        {"upstream_backup_controller_actual", controller.countdown},
        {"full_machine_state_proven", false}, {"explicit_cross_process_checkpoint_proven", false},
        {"temporal_journal_proven", false}, {"gate0_pass", false},
        {"limitations", {"Observable hash covers RAM/VRAM/CRAM only", "Copies do not prove a temporal observation journal or native transactions", "Upstream backup omits controller manager", "Complete explicit field codec and native PCM mixing remain unbuilt"}}};
}
}
int main(const int argc, char* argv[]) {
    if (argc != 4) { std::cerr << "Uso: tessera_clown_probe <fixtures> <traza.json> <resultado.json>\n"; return 2; }
    try {
        const auto result = run(argv[1], argv[2]);
        std::ofstream output(argv[3], std::ios::binary);
        output << result.dump(2) << '\n';
        if (!output) throw std::runtime_error("REPORT_WRITE_FAILED");
        std::cout << "CLOWN_PROBE_FINISHED frames=" << result.at("completed_frames") << " gate0_pass=false\n";
        return result.at("oracle_pass").get<bool>() && result.at("audio_pass").get<bool>() && result.at("copy_readers_pass").get<bool>() ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << "CLOWN_PROBE_FAILED: " << error.what() << '\n'; return 1; }
}
