#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace tessera::phase0 {

// Private Gate 0 prototype, not the Tessera product ABI.
// Native FM/PSG generators and their upstream filter states remain in Clown.
// This class consumes every generated sample; it never substitutes a chip.
class MediaError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct AudioConfig {
    std::uint32_t output_rate{48000};
    std::int32_t fm_gain_q15{32768};
    std::int32_t psg_gain_q15{32768};
    bool operator==(const AudioConfig&) const = default;
};

struct NativeFrameCounts {
    std::uint32_t fm_frames{};
    std::uint32_t psg_frames{};
};

struct AudioFrameInfo {
    std::uint64_t frame{};
    std::uint32_t output_frames{};
    std::uint32_t fm_frames{};
    std::uint32_t psg_frames{};
    std::uint64_t output_phase_before{};
    std::uint64_t output_phase_after{};
};

// S16LE stereo, left then right. Integer Q15 gains use round-to-nearest with
// ties away from zero, followed by saturation to [-32768,32767].
// The causal reconstruction kernel is zero-order hold. Input timestamps follow
// the pinned core's frame-local clocks, including its discarded sub-clock ticks.
// Output cadence follows REAL 895780 master ticks per Iterate, not a rounded
// promise of 1001/60000 seconds. The rational remainder is checkpointed.
class CanonicalAudio {
public:
    using Bytes = std::vector<std::uint8_t>;
    static constexpr std::uint32_t master_clock_hz = 53693175;
    static constexpr std::uint32_t master_ticks_per_frame = 895780;
    static constexpr std::uint32_t m68k_cycles_per_frame = master_ticks_per_frame / 7;
    static constexpr std::uint32_t fm_master_divisor = 1008;
    static constexpr std::uint32_t psg_master_divisor = 240;
    static constexpr std::uint64_t max_frames = 10000000;

    explicit CanonicalAudio(AudioConfig config = {});
    void push_fm(std::span<const std::int16_t> interleaved_stereo);
    void push_psg(std::span<const std::int16_t> mono);
    [[nodiscard]] Bytes finish_frame();

    // Complete future-affecting media state, including partially pushed input
    // queues, held samples and both clock phases. Returned PCM belongs to caller;
    // there is no hidden pending output queue. The enclosing checkpoint must
    // include this block in its integrity hash. An internal SHA-256 also rejects
    // corruption of pending samples. No object bytes are persisted.
    [[nodiscard]] Bytes encode_state() const;
    void decode_state(std::span<const std::uint8_t> bytes);
    void reset();

    [[nodiscard]] NativeFrameCounts expected_native_frames() const noexcept;
    [[nodiscard]] const AudioConfig& config() const noexcept { return config_; }
    [[nodiscard]] std::uint64_t frames_completed() const noexcept { return completed_frames_; }
    [[nodiscard]] std::uint64_t output_frames_completed() const noexcept { return output_frames_; }
    [[nodiscard]] std::uint64_t output_phase() const noexcept { return output_phase_; }
    [[nodiscard]] std::uint32_t fm_cycle_remainder() const noexcept { return fm_cycle_remainder_; }
    [[nodiscard]] const AudioFrameInfo& last_frame_info() const noexcept { return last_frame_info_; }

private:
    AudioConfig config_;
    std::uint64_t completed_frames_{};
    std::uint64_t output_frames_{};
    std::uint64_t output_phase_{}; // numerator remainder modulo master_clock_hz.
    std::uint32_t fm_cycle_remainder_{}; // native FM leftover cycles modulo 144.
    std::int16_t held_fm_left_{};
    std::int16_t held_fm_right_{};
    std::int16_t held_psg_{};
    std::vector<std::int16_t> pending_fm_;
    std::vector<std::int16_t> pending_psg_;
    AudioFrameInfo last_frame_info_; // Diagnostic only; never influences output.
};

} // namespace tessera::phase0

