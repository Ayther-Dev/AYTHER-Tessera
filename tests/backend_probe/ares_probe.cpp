#include <nlohmann/json.hpp>
#include <picosha2.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// All standard-library headers precede the upstream register macro.
#include <md/md.hpp>
#undef register

namespace {

using Json = nlohmann::json;
using Bytes = std::vector<std::uint8_t>;
constexpr std::string_view backend_commit = "0aafd85789215e84e1e43415c07d4c88461b7899";
constexpr std::size_t max_file_size = 8 * 1024 * 1024;
constexpr double silence_epsilon = 0.000001;

class ProbeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] Bytes read_bytes(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    if (size > max_file_size) throw ProbeError("INPUT_FILE_TOO_LARGE");
    Bytes bytes(static_cast<std::size_t>(size));
    std::ifstream stream;
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.open(path, std::ios::binary);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

[[nodiscard]] Json read_json(const std::filesystem::path& path) {
    const auto bytes = read_bytes(path);
    return Json::parse(bytes);
}

void write_result(const std::filesystem::path& path, const Json& value) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream stream;
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.open(path, std::ios::binary | std::ios::trunc);
    stream << value.dump(2) << '\n';
    stream.close();
}

void append_u16(Bytes& bytes, const std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void append_u32(Bytes& bytes, const std::uint32_t value) {
    append_u16(bytes, static_cast<std::uint16_t>(value >> 16));
    append_u16(bytes, static_cast<std::uint16_t>(value & 0xFFFF));
}

void append_u64(Bytes& bytes, const std::uint64_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value >> 32));
    append_u32(bytes, static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
}

void append_text(Bytes& bytes, const std::string_view text) {
    append_u32(bytes, static_cast<std::uint32_t>(text.size()));
    bytes.insert(bytes.end(), text.begin(), text.end());
}

void append_hash(std::string& sequence, const std::uint32_t frame, const std::string& hash) {
    sequence += std::to_string(frame) + ':' + hash + '\n';
}

struct SignalStats {
    std::uint64_t samples{};
    double minimum{std::numeric_limits<double>::infinity()};
    double maximum{-std::numeric_limits<double>::infinity()};
    double peak_absolute{};

    void add(const double value) {
        ++samples;
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
        peak_absolute = std::max(peak_absolute, std::abs(value));
    }

    [[nodiscard]] double range() const noexcept { return samples == 0 ? 0.0 : maximum - minimum; }
};

struct AudioBlock {
    std::uint32_t channels{};
    double source_frequency{};
    double output_frequency{};
    std::vector<double> samples;
};

struct VideoFrame {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint32_t> pixels;
};

struct Observable {
    Bytes bytes;
    std::uint8_t input{};
    std::uint16_t fault{};
    std::uint16_t marker{};
    std::uint16_t colour{};
};

class Harness final : public ares::Platform {
public:
    explicit Harness(const Bytes& rom) {
        if (ares::platform != nullptr) throw ProbeError("ARES_GLOBAL_INSTANCE_ALREADY_ACTIVE");
        system_pak_ = std::make_shared<nall::vfs::directory>();
        cartridge_pak_ = std::make_shared<nall::vfs::directory>();
        cartridge_pak_->setAttribute("title", "Tessera Gate 0 input and PSG probe");
        cartridge_pak_->setAttribute("region", "NTSC-U");
        cartridge_pak_->setAttribute("bootable", true);
        if (!cartridge_pak_->append("program.rom", std::span<const std::uint8_t>{rom})) {
            throw ProbeError("ROM_PAK_CREATION_FAILED");
        }
        ares::platform = this;
        try {
            ares::MegaDrive::option("TMSS", "false");
            ares::setRunAhead(false);
            if (!ares::MegaDrive::load(node_, "[Sega] Mega Drive (NTSC-U)")) {
                throw ProbeError("ARES_SYSTEM_LOAD_FAILED");
            }
            if (!ares::MegaDrive::cartridgeSlot.port->allocate()) {
                throw ProbeError("ARES_CARTRIDGE_ALLOCATION_FAILED");
            }
            ares::MegaDrive::cartridgeSlot.port->connect();
            if (!ares::MegaDrive::controllerPort1.port->allocate("Control Pad")) {
                throw ProbeError("ARES_CONTROLLER_ALLOCATION_FAILED");
            }
            node_->power(false);
        } catch (...) {
            if (node_) node_->unload();
            ares::platform = nullptr;
            throw;
        }
    }

    ~Harness() {
        if (node_) node_->unload();
        ares::platform = nullptr;
    }

    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;
    Harness(Harness&&) = delete;
    Harness& operator=(Harness&&) = delete;

    auto pak(ares::Node::Object object) -> std::shared_ptr<nall::vfs::directory> override {
        if (object->name() == "Mega Drive") return system_pak_;
        if (object->name() == "Mega Drive Cartridge") return cartridge_pak_;
        return {};
    }

    auto input(ares::Node::Input::Input object) -> void override {
        if (auto button = std::dynamic_pointer_cast<ares::Core::Input::Button>(object)) {
            button->setValue(button->name() == "B" && pressed_);
        }
    }

    auto audio(ares::Node::Audio::Stream stream) -> void override {
        // Exceptions must not unwind across libco coroutine stacks.
        try {
            const auto channels = stream->channels();
            if (channels == 0 || channels > 8) {
                callback_error_ = "ARES_AUDIO_CHANNEL_COUNT_INVALID";
                return;
            }
            const auto stream_name = stream->name();
            const std::string name{stream_name.data(), stream_name.size()};
            auto& block = audio_[name];
            block.channels = channels;
            block.source_frequency = stream->frequency();
            block.output_frequency = stream->resamplerFrequency();
            std::vector<double> samples(channels);
            while (stream->pending()) {
                if (stream->read(samples.data()) != channels) {
                    callback_error_ = "ARES_AUDIO_READ_SIZE_INVALID";
                    return;
                }
                for (const auto sample : samples) {
                    if (!std::isfinite(sample)) {
                        callback_error_ = "ARES_AUDIO_SAMPLE_NOT_FINITE";
                        return;
                    }
                    block.samples.push_back(sample);
                }
            }
        } catch (const std::exception& error) {
            callback_error_ = std::string{"ARES_AUDIO_CALLBACK_FAILED: "} + error.what();
        }
    }

    auto video(ares::Node::Video::Screen, const std::uint32_t* data,
               const std::uint32_t pitch_bytes, const std::uint32_t width,
               const std::uint32_t height) -> void override {
        std::lock_guard lock(video_mutex_);
        try {
            if (data == nullptr || width == 0 || height == 0 ||
                width > 4096 || height > 4096 ||
                pitch_bytes % sizeof(std::uint32_t) != 0 ||
                pitch_bytes / sizeof(std::uint32_t) < width) {
                video_error_ = "ARES_VIDEO_GEOMETRY_INVALID";
            } else {
                video_.width = width;
                video_.height = height;
                video_.pixels.resize(static_cast<std::size_t>(width) * height);
                const auto pitch_words = pitch_bytes / sizeof(std::uint32_t);
                for (std::uint32_t row = 0; row < height; ++row) {
                    std::copy_n(data + static_cast<std::size_t>(row) * pitch_words, width,
                                video_.pixels.begin() + static_cast<std::ptrdiff_t>(row) * width);
                }
                ++delivered_frames_;
            }
        } catch (const std::exception& error) {
            video_error_ = std::string{"ARES_VIDEO_CALLBACK_FAILED: "} + error.what();
        }
        video_ready_.notify_one();
    }

    void step(const bool pressed) {
        pressed_ = pressed;
        audio_.clear();
        std::uint64_t target{};
        {
            std::lock_guard lock(video_mutex_);
            target = delivered_frames_ + 1;
        }
        node_->run();
        if (!callback_error_.empty()) throw ProbeError(callback_error_);
        std::unique_lock lock(video_mutex_);
        if (!video_ready_.wait_for(lock, std::chrono::seconds{5}, [&] {
                return delivered_frames_ >= target || !video_error_.empty();
            })) {
            throw ProbeError("ARES_VIDEO_DELIVERY_TIMEOUT");
        }
        if (!video_error_.empty()) throw ProbeError(video_error_);
        if (delivered_frames_ != target) throw ProbeError("ARES_VIDEO_DELIVERY_COUNT_MISMATCH");
    }

    [[nodiscard]] VideoFrame video_frame() const {
        std::lock_guard lock(video_mutex_);
        return video_;
    }

    [[nodiscard]] const std::map<std::string, AudioBlock>& audio_blocks() const noexcept {
        return audio_;
    }

    [[nodiscard]] Observable observable() const {
        const auto& cpu = ares::MegaDrive::cpu;
        const auto& vdp = ares::MegaDrive::vdp;
        const auto& ram = cpu.debugger.memory.ram;
        const auto& vram = vdp.debugger.memory.vram;
        const auto& cram = vdp.debugger.memory.cram;
        const auto& vsram = vdp.debugger.memory.vsram;
        if (!ram || !vram || !cram || !vsram || ram->size() != 0x10000 ||
            vram->size() != 0x10000 || cram->size() != 128 || vsram->size() != 80) {
            throw ProbeError("ARES_OBSERVABLE_REGION_SIZE_UNEXPECTED");
        }
        const auto read_word = [](const ares::Node::Debugger::Memory& region,
                                  const std::uint32_t address) -> std::uint16_t {
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(region->read(address)) << 8) |
                 static_cast<std::uint16_t>(region->read(address + 1)));
        };
        // The public CRAM debugger exposes the packed nine-bit internal colour.
        // Reconstruct the VDP bus representation used by the independent oracle.
        const auto cram_bus_word = [](const std::uint16_t packed) -> std::uint16_t {
            return static_cast<std::uint16_t>(((packed & 0x007) << 1) |
                ((packed & 0x038) << 2) | ((packed & 0x1C0) << 3));
        };
        Observable result;
        result.bytes.reserve(0x20000 + 256);
        append_text(result.bytes, "ARES_OBSERVABLE_MEMORY_V1");
        // Logical bus words are encoded explicitly; no object memory is hashed.
        for (std::uint32_t address = 0; address < 0x10000; ++address) {
            result.bytes.push_back(static_cast<std::uint8_t>(ram->read(address)));
        }
        for (std::uint32_t address = 0; address < 0x10000; ++address) {
            result.bytes.push_back(static_cast<std::uint8_t>(vram->read(address)));
        }
        for (std::uint32_t index = 0; index < 64; ++index) {
            append_u16(result.bytes, cram_bus_word(read_word(cram, index * 2)));
        }
        for (std::uint32_t address = 0; address < 80; ++address) {
            result.bytes.push_back(static_cast<std::uint8_t>(vsram->read(address)));
        }
        result.input = static_cast<std::uint8_t>(ram->read(0));
        result.fault = read_word(ram, 2);
        result.marker = read_word(vram, 0xFFFC);
        result.colour = cram_bus_word(read_word(cram, 0));
        return result;
    }

private:
    std::shared_ptr<nall::vfs::directory> system_pak_;
    std::shared_ptr<nall::vfs::directory> cartridge_pak_;
    ares::Node::System node_;
    bool pressed_{};
    std::map<std::string, AudioBlock> audio_;
    std::string callback_error_;
    mutable std::mutex video_mutex_;
    std::condition_variable video_ready_;
    VideoFrame video_;
    std::uint64_t delivered_frames_{};
    std::string video_error_;
};

[[nodiscard]] std::string video_hash(const VideoFrame& frame) {
    Bytes bytes;
    bytes.reserve(8 + frame.pixels.size() * 4);
    append_u32(bytes, frame.width);
    append_u32(bytes, frame.height);
    for (const auto pixel : frame.pixels) append_u32(bytes, pixel);
    return picosha2::hash256_hex_string(bytes);
}

[[nodiscard]] std::string audio_hash(const std::map<std::string, AudioBlock>& blocks) {
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
    Bytes bytes;
    append_text(bytes, "ARES_AUDIO_STREAMS_V1");
    append_u32(bytes, static_cast<std::uint32_t>(blocks.size()));
    for (const auto& [name, block] : blocks) {
        append_text(bytes, name);
        append_u32(bytes, block.channels);
        append_u64(bytes, std::bit_cast<std::uint64_t>(block.source_frequency));
        append_u64(bytes, std::bit_cast<std::uint64_t>(block.output_frequency));
        append_u64(bytes, block.samples.size());
        for (const auto sample : block.samples) append_u64(bytes, std::bit_cast<std::uint64_t>(sample));
    }
    return picosha2::hash256_hex_string(bytes);
}

void validate_trace(const Json& trace, const bool short_trace, const bool action) {
    if (trace.at("trace_version") != 1 ||
        trace.at("format_scope") != "phase0_probe_private_not_native_api_abi" ||
        trace.at("system") != "mega_drive" ||
        trace.at("controller_profile") != "one_three_button_pad") {
        throw ProbeError("TRACE_PROFILE_UNSUPPORTED");
    }
    const auto count = short_trace ? 60U : 3000U;
    const auto& frames = trace.at("frames");
    if (!frames.is_array() || frames.size() != count) throw ProbeError("TRACE_FRAME_COUNT_INVALID");
    constexpr std::array<std::string_view, 8> names{"up", "down", "left", "right", "a", "b", "c", "start"};
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto& frame = frames.at(index);
        if (frame.at("frame") != index + 1) throw ProbeError("TRACE_FRAME_SEQUENCE_INVALID");
        const auto& ports = frame.at("ports");
        if (!ports.is_array() || ports.size() != 1 || ports.at(0).at("port") != 0) {
            throw ProbeError("TRACE_PORT_SET_INVALID");
        }
        const auto& controls = ports.at(0).at("controls");
        if (!controls.is_object() || controls.size() != names.size()) {
            throw ProbeError("TRACE_CONTROL_SET_INVALID");
        }
        const bool expected_b = action && index + 1 >= (short_trace ? 21U : 301U) &&
                                index + 1 <= (short_trace ? 40U : 600U);
        for (const auto name : names) {
            const auto& value = controls.at(std::string{name});
            if (!value.is_boolean()) throw ProbeError("TRACE_CONTROL_VALUE_INVALID");
            if (value.get<bool>() != (name == "b" && expected_b)) {
                throw ProbeError("TRACE_FIXTURE_INPUT_MISMATCH");
            }
        }
    }
}

void validate_manifest(const std::filesystem::path& directory) {
    const auto manifest = read_json(directory / "manifest.json");
    if (manifest.at("manifest_version") != 1 ||
        manifest.at("fixture_id") != "tessera-phase0-input-psg-v1") {
        throw ProbeError("FIXTURE_MANIFEST_UNSUPPORTED");
    }
    for (const auto& artifact : manifest.at("artifacts")) {
        const auto name = artifact.at("path").get<std::string>();
        const auto path = std::filesystem::path{name};
        if (path.is_absolute() || path.filename() != path || name == "manifest.json") {
            throw ProbeError("FIXTURE_MANIFEST_PATH_INVALID");
        }
        const auto bytes = read_bytes(directory / path);
        if (artifact.at("size_bytes") != bytes.size() ||
            artifact.at("sha256") != picosha2::hash256_hex_string(bytes)) {
            throw ProbeError("FIXTURE_INTEGRITY_MISMATCH");
        }
    }
}

[[nodiscard]] Json capability_gaps() {
    return Json{
        {"gate0_eligible", false},
        {"selection_status", "not_selected"},
        {"checkpoint", "unsupported_by_probe"},
        {"raw_coroutine_stack_serializer", "not_used"},
        {"synchronized_serializer", "not_evaluated"},
        {"missing_checkpoint_coverage", Json::array({
            "audio_stream_filters_resampler_history_and_pending_samples",
            "control_pad_select_and_direction_latches",
            "scheduler_safe_point_export_may_advance_emulation"
        })},
        {"full_machine_canonical_state", "not_implemented"},
        {"observation_invariance", "not_tested"},
        {"native_tessera_api_integration", "not_tested"},
        {"engine_runtime_integration", "not_tested"}
    };
}

void run_probe(const std::filesystem::path& directory, const std::string& trace_name, Json& result) {
    const bool short_trace = trace_name == "short-action.json" || trace_name == "short-control.json";
    const bool action = trace_name == "short-action.json" || trace_name == "full-action.json";
    if (!short_trace && trace_name != "full-action.json" && trace_name != "full-control.json") {
        throw ProbeError("TRACE_NAME_UNSUPPORTED");
    }
    validate_manifest(directory);
    const auto rom = read_bytes(directory / "program.md");
    if (rom.size() != 32768) throw ProbeError("ROM_SIZE_UNEXPECTED");
    const auto trace = read_json(directory / trace_name);
    const auto expected = read_json(directory / "expected.json");
    if (expected.at("oracle_version") != 1 ||
        expected.at("basis") != "generator_source_before_emulator_execution") {
        throw ProbeError("ORACLE_PROFILE_UNSUPPORTED");
    }
    validate_trace(trace, short_trace, action);
    result["rom_sha256"] = picosha2::hash256_hex_string(rom);
    result["trace_sha256"] = picosha2::hash256_hex_string(read_bytes(directory / trace_name));
    result["oracle_sha256"] = picosha2::hash256_hex_string(read_bytes(directory / "expected.json"));
    result["frames_requested"] = trace.at("frames").size();
    result["frames_completed"] = 0;
    result["frame_records"] = Json::array();
    result["oracle_failures"] = Json::array();
    auto& failures = result["oracle_failures"];
    const auto& windows = expected.at(short_trace ? "short" : "full").at("central_windows");
    std::vector<SignalStats> psg_windows(windows.size());
    std::vector<bool> window_active(windows.size(), false);
    for (std::size_t index = 0; index < windows.size(); ++index) {
        window_active[index] = action && windows.at(index).at("state") == "pressed";
    }
    const auto check = [&](const bool valid, const std::uint32_t frame, const std::string& code,
                           const std::uint32_t wanted, const std::uint32_t actual) {
        if (!valid && failures.size() < 32) {
            failures.push_back(Json{{"frame", frame}, {"code", code}, {"expected", wanted}, {"actual", actual}});
        }
    };
    std::string state_sequence, video_sequence, audio_sequence;
    Json stream_metadata = Json::object();
    Harness harness{rom};
    for (const auto& input : trace.at("frames")) {
        const auto frame = input.at("frame").get<std::uint32_t>();
        const bool pressed = input.at("ports").at(0).at("controls").at("b").get<bool>();
        harness.step(pressed);
        const auto observation = harness.observable();
        const auto video = harness.video_frame();
        const auto& audio = harness.audio_blocks();
        const auto state_digest = picosha2::hash256_hex_string(observation.bytes);
        const auto video_digest = video_hash(video);
        const auto audio_digest = audio_hash(audio);
        append_hash(state_sequence, frame, state_digest);
        append_hash(video_sequence, frame, video_digest);
        append_hash(audio_sequence, frame, audio_digest);
        const auto center = video.pixels.at(static_cast<std::size_t>(video.height / 2) * video.width + video.width / 2);
        std::uint64_t audio_samples{};
        for (const auto& [name, block] : audio) {
            audio_samples += block.samples.size();
            stream_metadata[name] = Json{
                {"channels", block.channels}, {"source_frequency_hz", block.source_frequency},
                {"output_frequency_hz", block.output_frequency}
            };
        }
        result["frame_records"].push_back(Json{
            {"frame", frame}, {"observable_state_sha256", state_digest},
            {"video_sha256", video_digest}, {"audio_sha256", audio_digest},
            {"audio_scalar_samples", audio_samples}, {"center_argb8888", center}
        });
        result["frames_completed"] = frame;
        check(observation.fault == 0, frame, "ROM_EXCEPTION_MARKER", 0, observation.fault);
        for (std::size_t index = 0; index < windows.size(); ++index) {
            const auto first = windows.at(index).at("first").get<std::uint32_t>();
            const auto last = windows.at(index).at("last").get<std::uint32_t>();
            if (frame < first || frame > last) continue;
            const auto& oracle = expected.at(window_active[index] ? "pressed" : "neutral");
            check(observation.input == oracle.at("ram_input_byte").get<std::uint32_t>(),
                  frame, "RAM_INPUT_ORACLE", oracle.at("ram_input_byte").get<std::uint32_t>(), observation.input);
            check(observation.marker == oracle.at("vram_marker_word").get<std::uint32_t>(),
                  frame, "VRAM_MARKER_ORACLE", oracle.at("vram_marker_word").get<std::uint32_t>(), observation.marker);
            check(observation.colour == oracle.at("cram_colour_zero").get<std::uint32_t>(),
                  frame, "CRAM_COLOUR_ORACLE", oracle.at("cram_colour_zero").get<std::uint32_t>(), observation.colour);
            const auto red = (center >> 16) & 0xFF;
            const auto blue = center & 0xFF;
            check(window_active[index] ? red > blue : blue > red,
                  frame, "VIDEO_CENTRE_COLOUR_ORACLE", window_active[index] ? 1U : 0U, red > blue ? 1U : 0U);
            if (const auto stream = audio.find("PSG"); stream != audio.end()) {
                for (const auto sample : stream->second.samples) psg_windows[index].add(sample);
            }
        }
    }
    result["audio_windows"] = Json::array();
    for (std::size_t index = 0; index < windows.size(); ++index) {
        const auto& stats = psg_windows[index];
        const bool passed = stats.samples > 0 &&
            (window_active[index] ? stats.range() > silence_epsilon : stats.peak_absolute <= silence_epsilon);
        result["audio_windows"].push_back(Json{
            {"first", windows.at(index).at("first")}, {"last", windows.at(index).at("last")},
            {"expected", window_active[index] ? "active" : "silent"},
            {"scalar_samples", stats.samples}, {"peak_to_peak", stats.range()},
            {"peak_absolute", stats.peak_absolute},
            {"silence_epsilon", silence_epsilon}, {"passed", passed}
        });
        if (!passed && failures.size() < 32) {
            failures.push_back(Json{{"code", "PSG_WINDOW_ORACLE"}, {"window", index}});
        }
    }
    result["hashes"] = Json{
        {"observable_state_sha256", picosha2::hash256_hex_string(state_sequence)},
        {"video_sha256", picosha2::hash256_hex_string(video_sequence)},
        {"audio_sha256", picosha2::hash256_hex_string(audio_sequence)},
        {"aggregate_definition", "sha256 of frame:per_frame_hex_digest LF records in frame order"}
    };
    result["audio_streams"] = std::move(stream_metadata);
    result["video_geometry"] = Json{
        {"width", harness.video_frame().width}, {"height", harness.video_frame().height},
        {"format", "argb8888_word_values_u32be_for_hash"}
    };
    result["status"] = failures.empty() ? "passed_partial_probe" : "failed";
    result["oracles_passed"] = failures.empty();
}

[[nodiscard]] Json initial_result(const std::string& trace) {
    return Json{
        {"result_version", 1}, {"backend", "ares-v148"}, {"backend_commit", backend_commit},
        {"trace", trace}, {"status", "not_run"}, {"frames_completed", 0},
        {"configuration", Json{
            {"system", "[Sega] Mega Drive (NTSC-U)"}, {"tmss", false},
            {"controller", "Control Pad"}, {"run_ahead", false},
            {"backend_asan_instrumentation", false},
#ifdef PROFILE_ACCURACY
            {"profile_define", "PROFILE_ACCURACY"},
#else
            {"profile_define", "PROFILE_PERFORMANCE"},
#endif
            {"vdp_implementation", "upstream_forces_accuracy_with_if_0"}
        }},
        {"state_scope", "PARTIAL_OBSERVABLE_MEMORY_ONLY"},
        {"state_format", "tag_then_64KiB_68k_RAM_64KiB_active_VRAM_64_CRAM_bus_words_40_VSRAM_words_all_u16be"},
        {"state_excludes", Json::array({"cpu_registers", "scheduler", "audio_chip_state", "audio_dsp_and_queues", "controller_protocol_state"})},
        {"video_stage", "original_ares_Screen_callback_after_native_palette_conversion_no_Tessera_or_Engine_presentation"},
        {"audio_format", "separate_streams_interleaved_ieee754_binary64_be_for_hash_no_mixing"},
        {"audio_stage", "Stream_read_after_upstream_filters_and_resampler"},
        {"capabilities", capability_gaps()}
    };
}

} // namespace

int main(const int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Uso: tessera_ares_probe <directorio-fixture> <traza-json> <resultado-json>\n";
        return 2;
    }
    auto result = initial_result(argv[2]);
    int exit_code = 0;
    try {
        run_probe(std::filesystem::path{argv[1]}, argv[2], result);
        if (result.at("status") == "failed") exit_code = 1;
    } catch (const std::exception& error) {
        result["status"] = "failed";
        result["error"] = Json{{"code", "ARES_PROBE_FAILED"}, {"diagnostic", error.what()}};
        exit_code = 1;
    }
    try {
        write_result(std::filesystem::path{argv[3]}, result);
    } catch (const std::exception& error) {
        std::cerr << "ARES_RESULT_WRITE_FAILED: no se pudo guardar el resultado: " << error.what() << '\n';
        return 1;
    }
    std::cout << "ARES_PROBE_FINISHED: resultado guardado en " << argv[3]
              << "; candidato sin seleccionar.\n";
    return exit_code;
}
