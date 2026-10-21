#include "clown_media.hpp"

#include <nlohmann/json.hpp>
#include <picosha2.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using tessera::phase0::AudioConfig;
using tessera::phase0::CanonicalAudio;
using tessera::phase0::MediaError;
using Bytes = CanonicalAudio::Bytes;

struct Checks {
    std::size_t count{};

    void require(const bool condition, const std::string_view name) {
        ++count;
        if (!condition) throw std::runtime_error("Prueba de audio fallida: " + std::string{name});
    }
};

struct NativeInput {
    std::vector<std::int16_t> fm;
    std::vector<std::int16_t> psg;
};

NativeInput constant_input(const CanonicalAudio& audio, const std::int16_t left,
                           const std::int16_t right, const std::int16_t mono) {
    const auto counts = audio.expected_native_frames();
    NativeInput input{std::vector<std::int16_t>(static_cast<std::size_t>(counts.fm_frames) * 2),
                      std::vector<std::int16_t>(counts.psg_frames, mono)};
    for (std::size_t index = 0; index < input.fm.size(); index += 2) {
        input.fm[index] = left;
        input.fm[index + 1] = right;
    }
    return input;
}

NativeInput patterned_input(const CanonicalAudio& audio, const std::uint32_t frame) {
    auto input = constant_input(audio, 0, 0, 0);
    for (std::size_t index = 0; index < input.fm.size() / 2; ++index) {
        const auto value = static_cast<std::int16_t>(
            static_cast<std::int32_t>((frame * 29 + index * 7) % 2001) - 1000);
        input.fm[index * 2] = value;
        input.fm[index * 2 + 1] = static_cast<std::int16_t>(-value);
    }
    for (std::size_t index = 0; index < input.psg.size(); ++index) {
        input.psg[index] = static_cast<std::int16_t>(
            static_cast<std::int32_t>((frame * 41 + index * 11) % 3001) - 1500);
    }
    return input;
}

void push_all(CanonicalAudio& audio, const NativeInput& input) {
    audio.push_fm(input.fm);
    audio.push_psg(input.psg);
}

void push_parts(CanonicalAudio& audio, const std::span<const std::int16_t> fm,
                const std::span<const std::int16_t> psg) {
    // FM fragments always contain complete stereo frames; source order differs.
    for (std::size_t offset = 0; offset < psg.size();) {
        const auto size = std::min<std::size_t>(31, psg.size() - offset);
        audio.push_psg(psg.subspan(offset, size));
        offset += size;
    }
    for (std::size_t offset = 0; offset < fm.size();) {
        const auto size = std::min<std::size_t>(14, fm.size() - offset);
        audio.push_fm(fm.subspan(offset, size));
        offset += size;
    }
}

std::int16_t sample_at(const Bytes& pcm, const std::size_t frame, const std::size_t channel) {
    const auto offset = frame * 4 + channel * 2;
    const auto bits = static_cast<std::uint16_t>(pcm.at(offset)) |
                      (static_cast<std::uint16_t>(pcm.at(offset + 1)) << 8);
    const auto signed_value = bits <= 0x7FFF ? static_cast<std::int32_t>(bits)
                                            : static_cast<std::int32_t>(bits) - 0x10000;
    return static_cast<std::int16_t>(signed_value);
}

void expect_stereo(Checks& checks, const Bytes& pcm, const std::int16_t left,
                   const std::int16_t right, const std::string_view name) {
    checks.require(!pcm.empty() && pcm.size() % 4 == 0, std::string{name} + ".shape");
    bool correct = true;
    for (std::size_t frame = 0; frame < pcm.size() / 4; ++frame) {
        correct = correct && sample_at(pcm, frame, 0) == left && sample_at(pcm, frame, 1) == right;
    }
    checks.require(correct, name);
}

template<class Action>
void expect_rejected(Checks& checks, CanonicalAudio& destination, Action&& action,
                     const std::string_view error, const std::string_view name) {
    const auto before = destination.encode_state();
    std::string received;
    try {
        action();
    } catch (const MediaError& failure) {
        received = failure.what();
    }
    checks.require(received == error, std::string{name} + ".error");
    checks.require(destination.encode_state() == before, std::string{name} + ".unchanged");
}

void put_u32(Bytes& bytes, const std::size_t offset, const std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes.at(offset + index) = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void put_u64(Bytes& bytes, const std::size_t offset, const std::uint64_t value) {
    put_u32(bytes, offset, static_cast<std::uint32_t>(value));
    put_u32(bytes, offset + 4, static_cast<std::uint32_t>(value >> 32));
}

void resign(Bytes& bytes) {
    if (bytes.size() < 64) throw std::runtime_error("Estado de prueba demasiado corto para SHA-256");
    bytes.resize(bytes.size() - 64);
    const auto digest = picosha2::hash256_hex_string(bytes);
    bytes.insert(bytes.end(), digest.begin(), digest.end());
}

} // namespace

// Private qualifier entry point: no product ABI, filesystem, wall clock or core.
nlohmann::json run_media_tests() {
    Checks checks;
    auto cases = nlohmann::json::array();
    auto rates = nlohmann::json::array();
    const auto record = [&](const std::string_view name, auto&& action) {
        const auto before = checks.count;
        action();
        cases.push_back({{"name", std::string{name}}, {"checks", checks.count - before}});
    };

    record("constant_unity_s16le", [&] {
        CanonicalAudio audio;
        push_all(audio, constant_input(audio, 1000, -1000, 2000));
        const auto pcm = audio.finish_frame();
        expect_stereo(checks, pcm, 3000, 1000, "constant_unity");
        checks.require(pcm[0] == 0xB8 && pcm[1] == 0x0B && pcm[2] == 0xE8 && pcm[3] == 0x03,
                       "constant_unity.little_endian");
    });

    record("psg_known_transition", [&] {
        CanonicalAudio audio;
        auto input = constant_input(audio, 0, 0, 1000);
        std::fill_n(input.psg.begin(), 100, 0);
        push_all(audio, input);
        const auto pcm = audio.finish_frame();
        std::size_t first_change = pcm.size() / 4;
        bool correct = true;
        for (std::size_t frame = 0; frame < pcm.size() / 4; ++frame) {
            const std::int16_t expected = frame < 21 ? 0 : 1000;
            correct = correct && sample_at(pcm, frame, 0) == expected &&
                                  sample_at(pcm, frame, 1) == expected;
            if (sample_at(pcm, frame, 0) != 0 && first_change == pcm.size() / 4) first_change = frame;
        }
        // Native PSG sample 100 is timestamped at 101*240 ticks, not at zero.
        checks.require(first_change == 21, "psg_transition.first_output_index_21");
        checks.require(correct, "psg_transition.entire_window");
    });

    record("48000_first_five_counts", [&] {
        CanonicalAudio audio;
        constexpr std::array<std::size_t, 5> golden{800, 801, 801, 801, 800};
        for (const auto count : golden) {
            push_all(audio, constant_input(audio, 0, 0, 0));
            checks.require(audio.finish_frame().size() == count * 4, "48000_golden_count");
        }
        checks.require(audio.output_frames_completed() == 4003, "48000_golden_total");
    });

    record("rational_cadence_44100_48000_96000", [&] {
        for (const auto rate : std::array<std::uint32_t, 3>{44100, 48000, 96000}) {
            CanonicalAudio audio{AudioConfig{rate}};
            std::uint64_t previous_total{};
            std::uint64_t previous_phase{};
            for (std::uint64_t frame = 1; frame <= 60; ++frame) {
                const auto native = audio.expected_native_frames();
                checks.require(native.fm_frames == (frame % 3 == 1 ? 888U : 889U) &&
                                   native.psg_frames == 3732, "native_cadence");
                push_all(audio, constant_input(audio, 0, 0, 0));
                const auto pcm = audio.finish_frame();
                const auto clock = frame * 895780ULL * rate;
                const auto total = clock / 53693175ULL;
                const auto phase = clock % 53693175ULL;
                checks.require(pcm.size() == (total - previous_total) * 4, "rational_frame_length");
                checks.require(std::all_of(pcm.begin(), pcm.end(), [](const auto value) { return value == 0; }),
                               "zero_native_is_zero_pcm");
                checks.require(audio.output_frames_completed() == total && audio.output_phase() == phase &&
                                   audio.last_frame_info().output_phase_before == previous_phase &&
                                   audio.last_frame_info().output_phase_after == phase &&
                                   audio.frames_completed() == frame, "rational_phase_and_counters");
                previous_total = total;
                previous_phase = phase;
            }
            checks.require(audio.fm_cycle_remainder() == 0, "native_fm_phase_after_60_frames");
            rates.push_back({{"rate", rate}, {"frames", 60},
                             {"output_frames", previous_total}, {"phase", previous_phase}});
        }
    });

    record("q15_rounding_saturation_and_stereo", [&] {
        const auto verify = [&](const AudioConfig config, const std::int16_t fm_left,
                                const std::int16_t fm_right, const std::int16_t psg,
                                const std::int16_t left, const std::int16_t right) {
            CanonicalAudio audio{config};
            push_all(audio, constant_input(audio, fm_left, fm_right, psg));
            expect_stereo(checks, audio.finish_frame(), left, right, "q15_golden");
        };
        verify({48000, 32768, 32768}, 32767, 32767, 32767, 32767, 32767);
        verify({48000, 32768, 32768}, -32768, -32768, -32768, -32768, -32768);
        verify({48000, 16384, 0}, 1, -1, 0, 1, -1);
        verify({48000, 16384, 0}, 3, -3, 0, 2, -2);
        verify({48000, 0, 4096}, 0, 0, 8, 1, 1);
        verify({48000, 0, 4096}, 0, 0, 4, 1, 1);
        verify({48000, 0, 4096}, 0, 0, -4, -1, -1);
        verify({48000, 0, 0}, -32768, 32767, 32767, 0, 0);
    });

    record("initial_hold_and_cross_frame_carry", [&] {
        CanonicalAudio audio{AudioConfig{384000}};
        push_all(audio, constant_input(audio, 0, 0, 123));
        const auto first = audio.finish_frame();
        checks.require(sample_at(first, 0, 0) == 0 && sample_at(first, 0, 1) == 0,
                       "hold_before_first_native_sample");
        checks.require(sample_at(first, 1, 0) == 123 && sample_at(first, 1, 1) == 123,
                       "hold_after_first_psg_sample");
        CanonicalAudio restored{AudioConfig{384000}};
        restored.decode_state(audio.encode_state());
        const auto input = constant_input(audio, 0, 0, 456);
        push_all(audio, input);
        push_all(restored, input);
        const auto next = audio.finish_frame();
        checks.require(sample_at(next, 0, 0) == 123 && sample_at(next, 0, 1) == 123,
                       "previous_frame_last_native_sample_is_held");
        checks.require(restored.finish_frame() == next && restored.encode_state() == audio.encode_state(),
                       "hold_and_output_phase_survive_checkpoint");
    });

    record("push_partition_and_order", [&] {
        CanonicalAudio whole;
        CanonicalAudio fragmented;
        for (std::uint32_t frame = 1; frame <= 12; ++frame) {
            const auto input = patterned_input(whole, frame);
            push_all(whole, input);
            push_parts(fragmented, input.fm, input.psg);
            fragmented.push_fm(std::span<const std::int16_t>{});
            fragmented.push_psg(std::span<const std::int16_t>{});
            checks.require(whole.encode_state() == fragmented.encode_state(), "partition.pending_state");
            checks.require(whole.finish_frame() == fragmented.finish_frame(), "partition.pcm");
            checks.require(whole.encode_state() == fragmented.encode_state(), "partition.completed_state");
        }
    });

    CanonicalAudio source;
    for (std::uint32_t frame = 1; frame <= 5; ++frame) {
        push_all(source, patterned_input(source, frame));
        static_cast<void>(source.finish_frame());
    }
    const auto next_input = patterned_input(source, 6);
    const auto fm = std::span<const std::int16_t>{next_input.fm};
    const auto psg = std::span<const std::int16_t>{next_input.psg};
    source.push_fm(fm.first(22));
    source.push_psg(psg.first(37));
    const auto partial_state = source.encode_state();

    record("partial_queue_checkpoint_and_continuation", [&] {
        CanonicalAudio restored;
        push_all(restored, constant_input(restored, -123, 456, -789));
        static_cast<void>(restored.finish_frame());
        restored.decode_state(partial_state);
        checks.require(restored.encode_state() == partial_state, "partial_queue.exact_roundtrip");
        source.push_fm(fm.subspan(22));
        source.push_psg(psg.subspan(37));
        push_parts(restored, fm.subspan(22), psg.subspan(37));
        checks.require(source.finish_frame() == restored.finish_frame(), "partial_queue.first_pcm");
        checks.require(source.encode_state() == restored.encode_state(), "partial_queue.first_state");
        for (std::uint32_t frame = 7; frame <= 18; ++frame) {
            const auto input = patterned_input(source, frame);
            push_all(source, input);
            push_parts(restored, input.fm, input.psg);
            checks.require(source.finish_frame() == restored.finish_frame(), "checkpoint.continuation_pcm");
            checks.require(source.encode_state() == restored.encode_state(), "checkpoint.continuation_state");
        }
        restored.reset();
        checks.require(restored.encode_state() == CanonicalAudio{}.encode_state(), "reset.initial_state");
    });

    record("codec_integrity_and_semantic_rejection", [&] {
        CanonicalAudio destination;
        push_all(destination, constant_input(destination, 321, -654, 987));
        static_cast<void>(destination.finish_frame());
        destination.push_psg(std::array<std::int16_t, 2>{123, -456});
        const auto reject = [&](const Bytes& state, const std::string_view error, const std::string_view name) {
            expect_rejected(checks, destination, [&] { destination.decode_state(state); }, error, name);
        };
        auto state = partial_state;
        state.at(86) ^= 1; // First pending sample, not just a header mutation.
        reject(state, "MEDIA_STATE_INTEGRITY_MISMATCH", "corrupt_pending_sample");
        state = partial_state;
        state.resize(12);
        reject(state, "MEDIA_STATE_TRUNCATED", "short_state");
        state = partial_state;
        state.pop_back();
        reject(state, "MEDIA_STATE_INTEGRITY_MISMATCH", "truncated_digest");

        const auto altered = [&](const std::size_t offset, const std::uint32_t value,
                                  const std::string_view error, const std::string_view name) {
            auto candidate = partial_state;
            put_u32(candidate, offset, value);
            resign(candidate); // Semantic checks must reject a valid integrity digest.
            reject(candidate, error, name);
        };
        // TSCAUD01 v1 field offsets are explicit wire fields, never object layout.
        altered(8, 2, "MEDIA_STATE_VERSION_UNSUPPORTED", "unsupported_version");
        altered(12, 44100, "MEDIA_STATE_CONFIG_MISMATCH", "different_output_rate");
        altered(12, 0, "MEDIA_CONFIG_INVALID", "invalid_output_rate");
        altered(16, 32769, "MEDIA_STATE_GAIN_INVALID", "invalid_gain");
        altered(20, 4096, "MEDIA_STATE_CONFIG_MISMATCH", "different_gain");
        altered(24, 53693176, "MEDIA_STATE_CLOCK_PROFILE_MISMATCH", "different_master_clock");
        altered(68, 144, "MEDIA_STATE_COUNTER_INVALID", "fm_phase_out_of_range");
        altered(68, 1, "MEDIA_STATE_CLOCK_PHASE_INVALID", "fm_phase_inconsistent_with_frames");
        altered(78, 1, "MEDIA_STATE_QUEUE_COUNT_INVALID", "incomplete_stereo_queue");
        altered(82, 3733, "MEDIA_STATE_QUEUE_COUNT_INVALID", "psg_queue_overflow");

        state = partial_state;
        state.at(60) ^= 1;
        resign(state);
        reject(state, "MEDIA_STATE_CLOCK_PHASE_INVALID", "output_phase_inconsistent_with_frames");
        state = partial_state;
        state.at(52) ^= 1;
        resign(state);
        reject(state, "MEDIA_STATE_CLOCK_PHASE_INVALID", "output_count_inconsistent_with_frames");
        state = partial_state;
        put_u64(state, 44, CanonicalAudio::max_frames + 1);
        resign(state);
        reject(state, "MEDIA_STATE_COUNTER_INVALID", "frame_count_out_of_range");
        state = partial_state;
        state.erase(state.end() - 65); // Remove one queue byte, then repair only integrity.
        resign(state);
        reject(state, "MEDIA_STATE_SIZE_INVALID", "truncated_payload_with_valid_digest");
        state = partial_state;
        state.insert(state.end() - 64, 0);
        resign(state);
        reject(state, "MEDIA_STATE_SIZE_INVALID", "trailing_payload_with_valid_digest");
        state = CanonicalAudio{}.encode_state();
        state.at(72) = 1;
        resign(state);
        reject(state, "MEDIA_STATE_INITIAL_HOLD_INVALID", "nonzero_hold_before_initial_sample");
        state = partial_state;
        state.at(0) ^= 1;
        resign(state);
        reject(state, "MEDIA_STATE_VERSION_UNSUPPORTED", "different_magic");
    });

    record("invalid_input_has_no_partial_effect", [&] {
        CanonicalAudio audio;
        expect_rejected(checks, audio, [&] { static_cast<void>(audio.finish_frame()); },
                        "MEDIA_NATIVE_FRAME_INCOMPLETE", "missing_native_input");
        expect_rejected(checks, audio, [&] { audio.push_fm(std::array<std::int16_t, 1>{1}); },
                        "MEDIA_FM_INPUT_COUNT_INVALID", "odd_stereo_input");
        auto excess = constant_input(audio, 0, 0, 0);
        excess.fm.push_back(1);
        excess.fm.push_back(2);
        expect_rejected(checks, audio, [&] { audio.push_fm(excess.fm); },
                        "MEDIA_FM_INPUT_COUNT_INVALID", "fm_overflow");
        excess.psg.push_back(1);
        expect_rejected(checks, audio, [&] { audio.push_psg(excess.psg); },
                        "MEDIA_PSG_INPUT_COUNT_INVALID", "psg_overflow");
        push_all(audio, constant_input(audio, 0, 0, 0));
        expect_rejected(checks, audio, [&] { audio.push_fm(std::array<std::int16_t, 2>{1, 2}); },
                        "MEDIA_FM_INPUT_COUNT_INVALID", "fm_overflow_after_full_push");
        expect_rejected(checks, audio, [&] { audio.push_psg(std::array<std::int16_t, 1>{1}); },
                        "MEDIA_PSG_INPUT_COUNT_INVALID", "psg_overflow_after_full_push");
    });

    record("invalid_config", [&] {
        for (const auto config : std::array<AudioConfig, 6>{
                 AudioConfig{0}, AudioConfig{384001}, AudioConfig{48000, -1, 0},
                 AudioConfig{48000, 32769, 0}, AudioConfig{48000, 0, -1},
                 AudioConfig{48000, 0, 32769}}) {
            std::string error;
            try {
                const CanonicalAudio invalid{config};
                static_cast<void>(invalid);
            } catch (const MediaError& failure) {
                error = failure.what();
            }
            checks.require(error == "MEDIA_CONFIG_INVALID", "invalid_config_rejected");
        }
    });

    return {{"status", "passed"}, {"checks", checks.count}, {"cases", std::move(cases)},
            {"rates", std::move(rates)}, {"format", "S16LE stereo"},
            {"reconstruction", "causal zero-order hold; integer Q15"},
            {"master_clock_hz", 53693175}, {"master_ticks_per_frame", 895780},
            {"scope", "media component only; no backend or product integration claim"}};
}
