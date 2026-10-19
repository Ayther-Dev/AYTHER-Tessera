#include "canonical_audio.hpp"
#include <picosha2.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>

namespace tessera::megadrive {
namespace {

constexpr std::uint32_t codec_version = 1;
constexpr std::array<std::uint8_t, 8> codec_magic{'T', 'S', 'C', 'A', 'U', 'D', '0', '1'};
constexpr std::uint32_t fm_cycles_per_sample = 144;
constexpr std::uint32_t m68k_master_divisor = 7;
constexpr std::int64_t q15_one = 32768;
constexpr std::uint32_t maximum_output_rate = 384000;
constexpr std::size_t fixed_codec_bytes = 86;
constexpr std::size_t digest_characters = 64;
constexpr std::size_t maximum_fm_scalars =
    2 * ((CanonicalAudio::m68k_cycles_per_frame + fm_cycles_per_sample - 1) / fm_cycles_per_sample);
constexpr std::size_t maximum_psg_samples =
    CanonicalAudio::master_ticks_per_frame / CanonicalAudio::psg_master_divisor;
constexpr std::size_t maximum_codec_bytes = fixed_codec_bytes + digest_characters +
    2 * (maximum_fm_scalars + maximum_psg_samples);
static_assert(CanonicalAudio::max_frames <= std::numeric_limits<std::uint64_t>::max() /
    (static_cast<std::uint64_t>(CanonicalAudio::master_ticks_per_frame) * maximum_output_rate));

void validate_config(const AudioConfig& config) {
    if (config.output_rate == 0 || config.output_rate > maximum_output_rate ||
        config.fm_gain_q15 < 0 || config.fm_gain_q15 > q15_one ||
        config.psg_gain_q15 < 0 || config.psg_gain_q15 > q15_one) {
        throw MediaError("MEDIA_CONFIG_INVALID");
    }
}

void append_u16(CanonicalAudio::Bytes& bytes, const std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(CanonicalAudio::Bytes& bytes, const std::uint32_t value) {
    append_u16(bytes, static_cast<std::uint16_t>(value & 0xFFFF));
    append_u16(bytes, static_cast<std::uint16_t>(value >> 16));
}

void append_u64(CanonicalAudio::Bytes& bytes, const std::uint64_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
    append_u32(bytes, static_cast<std::uint32_t>(value >> 32));
}

[[nodiscard]] std::int16_t mix(const std::int16_t fm, const std::int16_t psg,
                               const AudioConfig& config) {
    const std::int64_t weighted = static_cast<std::int64_t>(fm) * config.fm_gain_q15 +
                                  static_cast<std::int64_t>(psg) * config.psg_gain_q15;
    const auto rounded = weighted >= 0 ? (weighted + q15_one / 2) / q15_one
                                      : -((-weighted + q15_one / 2) / q15_one);
    return static_cast<std::int16_t>(std::clamp<std::int64_t>(rounded, -32768, 32767));
}

class Reader {
public:
    explicit Reader(const std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::span<const std::uint8_t> take(const std::size_t count) {
        if (count > bytes_.size() - position_) throw MediaError("MEDIA_STATE_TRUNCATED");
        const auto result = bytes_.subspan(position_, count);
        position_ += count;
        return result;
    }

    [[nodiscard]] std::uint16_t u16() {
        const auto data = take(2);
        return static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8));
    }

    [[nodiscard]] std::uint32_t u32() {
        const auto low = u16();
        const auto high = u16();
        return static_cast<std::uint32_t>(low) | (static_cast<std::uint32_t>(high) << 16);
    }

    [[nodiscard]] std::uint64_t u64() {
        const auto low = u32();
        const auto high = u32();
        return static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
    }

    [[nodiscard]] std::int16_t sample() {
        const auto bits = u16();
        const auto value = bits <= 0x7FFF ? static_cast<std::int32_t>(bits)
                                         : static_cast<std::int32_t>(bits) - 0x10000;
        return static_cast<std::int16_t>(value);
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - position_; }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_{};
};

} // namespace

CanonicalAudio::CanonicalAudio(const AudioConfig config) : config_(config) {
    validate_config(config_);
}

NativeFrameCounts CanonicalAudio::expected_native_frames() const noexcept {
    return NativeFrameCounts{
        (fm_cycle_remainder_ + m68k_cycles_per_frame) / fm_cycles_per_sample,
        master_ticks_per_frame / psg_master_divisor
    };
}

void CanonicalAudio::push_fm(const std::span<const std::int16_t> interleaved_stereo) {
    const auto maximum_scalars = static_cast<std::size_t>(expected_native_frames().fm_frames) * 2;
    if (interleaved_stereo.size() % 2 != 0 ||
        interleaved_stereo.size() > maximum_scalars - pending_fm_.size()) {
        throw MediaError("MEDIA_FM_INPUT_COUNT_INVALID");
    }
    pending_fm_.insert(pending_fm_.end(), interleaved_stereo.begin(), interleaved_stereo.end());
}

void CanonicalAudio::push_psg(const std::span<const std::int16_t> mono) {
    const auto maximum_samples = static_cast<std::size_t>(expected_native_frames().psg_frames);
    if (mono.size() > maximum_samples - pending_psg_.size()) {
        throw MediaError("MEDIA_PSG_INPUT_COUNT_INVALID");
    }
    pending_psg_.insert(pending_psg_.end(), mono.begin(), mono.end());
}

CanonicalAudio::Bytes CanonicalAudio::finish_frame() {
    const auto expected = expected_native_frames();
    if (pending_fm_.size() != static_cast<std::size_t>(expected.fm_frames) * 2 ||
        pending_psg_.size() != expected.psg_frames) {
        throw MediaError("MEDIA_NATIVE_FRAME_INCOMPLETE");
    }
    if (completed_frames_ >= max_frames) throw MediaError("MEDIA_FRAME_LIMIT_REACHED");

    const std::uint64_t phase_total = output_phase_ +
        static_cast<std::uint64_t>(master_ticks_per_frame) * config_.output_rate;
    const auto count = static_cast<std::uint32_t>(phase_total / master_clock_hz);
    const auto next_phase = phase_total % master_clock_hz;
    Bytes pcm;
    pcm.reserve(static_cast<std::size_t>(count) * 4);
    auto fm_left = held_fm_left_;
    auto fm_right = held_fm_right_;
    auto psg = held_psg_;
    std::size_t fm_index{};
    std::size_t psg_index{};
    const std::uint64_t first_fm_tick =
        static_cast<std::uint64_t>(fm_cycles_per_sample - fm_cycle_remainder_) * m68k_master_divisor;

    for (std::uint32_t output = 1; output <= count; ++output) {
        // Target offset in master ticks is target_numerator/output_rate.
        // Cross-products stay below 2^64 for every validated configuration.
        const std::uint64_t target_numerator =
            static_cast<std::uint64_t>(output) * master_clock_hz - output_phase_;
        while (fm_index < expected.fm_frames) {
            const std::uint64_t tick = first_fm_tick + fm_index * fm_master_divisor;
            if (tick * config_.output_rate > target_numerator) break;
            fm_left = pending_fm_[fm_index * 2];
            fm_right = pending_fm_[fm_index * 2 + 1];
            ++fm_index;
        }
        while (psg_index < expected.psg_frames) {
            const std::uint64_t tick = (psg_index + 1) * psg_master_divisor;
            if (tick * config_.output_rate > target_numerator) break;
            psg = pending_psg_[psg_index++];
        }
        append_u16(pcm, static_cast<std::uint16_t>(mix(fm_left, psg, config_)));
        append_u16(pcm, static_cast<std::uint16_t>(mix(fm_right, psg, config_)));
    }

    // Native samples after the last output timestamp still precede the frame
    // endpoint. Consume them into the hold used by the next frame: do not drop
    // a pending sample or manufacture a new one to satisfy nominal frame rate.
    fm_left = pending_fm_[pending_fm_.size() - 2];
    fm_right = pending_fm_.back();
    psg = pending_psg_.back();
    const auto next_fm_remainder =
        (fm_cycle_remainder_ + m68k_cycles_per_frame) % fm_cycles_per_sample;
    last_frame_info_ = AudioFrameInfo{
        completed_frames_ + 1, count, expected.fm_frames, expected.psg_frames,
        output_phase_, next_phase
    };
    held_fm_left_ = fm_left;
    held_fm_right_ = fm_right;
    held_psg_ = psg;
    fm_cycle_remainder_ = next_fm_remainder;
    output_phase_ = next_phase;
    output_frames_ += count;
    ++completed_frames_;
    pending_fm_.clear();
    pending_psg_.clear();
    return pcm;
}

CanonicalAudio::Bytes CanonicalAudio::encode_state() const {
    Bytes bytes;
    bytes.reserve(fixed_codec_bytes + digest_characters +
                  (pending_fm_.size() + pending_psg_.size()) * 2);
    bytes.insert(bytes.end(), codec_magic.begin(), codec_magic.end());
    append_u32(bytes, codec_version);
    append_u32(bytes, config_.output_rate);
    append_u32(bytes, static_cast<std::uint32_t>(config_.fm_gain_q15));
    append_u32(bytes, static_cast<std::uint32_t>(config_.psg_gain_q15));
    append_u32(bytes, master_clock_hz);
    append_u32(bytes, master_ticks_per_frame);
    append_u32(bytes, fm_master_divisor);
    append_u32(bytes, psg_master_divisor);
    append_u32(bytes, m68k_cycles_per_frame);
    append_u64(bytes, completed_frames_);
    append_u64(bytes, output_frames_);
    append_u64(bytes, output_phase_);
    append_u32(bytes, fm_cycle_remainder_);
    append_u16(bytes, static_cast<std::uint16_t>(held_fm_left_));
    append_u16(bytes, static_cast<std::uint16_t>(held_fm_right_));
    append_u16(bytes, static_cast<std::uint16_t>(held_psg_));
    append_u32(bytes, static_cast<std::uint32_t>(pending_fm_.size()));
    append_u32(bytes, static_cast<std::uint32_t>(pending_psg_.size()));
    for (const auto sample : pending_fm_) append_u16(bytes, static_cast<std::uint16_t>(sample));
    for (const auto sample : pending_psg_) append_u16(bytes, static_cast<std::uint16_t>(sample));
    const auto checksum = picosha2::hash256_hex_string(bytes);
    bytes.insert(bytes.end(), checksum.begin(), checksum.end());
    return bytes;
}

void CanonicalAudio::decode_state(const std::span<const std::uint8_t> bytes) {
    if (bytes.size() < fixed_codec_bytes + digest_characters) {
        throw MediaError("MEDIA_STATE_TRUNCATED");
    }
    if (bytes.size() > maximum_codec_bytes) throw MediaError("MEDIA_STATE_SIZE_INVALID");
    const auto payload = bytes.first(bytes.size() - digest_characters);
    const auto checksum = bytes.last(digest_characters);
    const auto expected_checksum = picosha2::hash256_hex_string(payload.begin(), payload.end());
    if (!std::equal(checksum.begin(), checksum.end(), expected_checksum.begin())) {
        throw MediaError("MEDIA_STATE_INTEGRITY_MISMATCH");
    }
    Reader reader{payload};
    const auto magic = reader.take(codec_magic.size());
    if (!std::equal(magic.begin(), magic.end(), codec_magic.begin()) ||
        reader.u32() != codec_version) {
        throw MediaError("MEDIA_STATE_VERSION_UNSUPPORTED");
    }
    const auto rate = reader.u32();
    const auto fm_gain = reader.u32();
    const auto psg_gain = reader.u32();
    if (fm_gain > q15_one || psg_gain > q15_one) throw MediaError("MEDIA_STATE_GAIN_INVALID");
    const AudioConfig decoded_config{
        rate, static_cast<std::int32_t>(fm_gain), static_cast<std::int32_t>(psg_gain)
    };
    validate_config(decoded_config);
    if (decoded_config != config_) throw MediaError("MEDIA_STATE_CONFIG_MISMATCH");
    if (reader.u32() != master_clock_hz || reader.u32() != master_ticks_per_frame ||
        reader.u32() != fm_master_divisor || reader.u32() != psg_master_divisor ||
        reader.u32() != m68k_cycles_per_frame) {
        throw MediaError("MEDIA_STATE_CLOCK_PROFILE_MISMATCH");
    }

    CanonicalAudio candidate{decoded_config};
    candidate.completed_frames_ = reader.u64();
    candidate.output_frames_ = reader.u64();
    candidate.output_phase_ = reader.u64();
    candidate.fm_cycle_remainder_ = reader.u32();
    candidate.held_fm_left_ = reader.sample();
    candidate.held_fm_right_ = reader.sample();
    candidate.held_psg_ = reader.sample();
    const auto fm_scalars = reader.u32();
    const auto psg_samples = reader.u32();
    if (candidate.completed_frames_ > max_frames ||
        candidate.output_phase_ >= master_clock_hz ||
        candidate.fm_cycle_remainder_ >= fm_cycles_per_sample) {
        throw MediaError("MEDIA_STATE_COUNTER_INVALID");
    }
    const auto expected = candidate.expected_native_frames();
    if (fm_scalars % 2 != 0 || fm_scalars > expected.fm_frames * 2 ||
        psg_samples > expected.psg_frames) {
        throw MediaError("MEDIA_STATE_QUEUE_COUNT_INVALID");
    }
    const std::uint64_t clock_product = candidate.completed_frames_ *
        master_ticks_per_frame * decoded_config.output_rate;
    if (candidate.output_frames_ != clock_product / master_clock_hz ||
        candidate.output_phase_ != clock_product % master_clock_hz ||
        candidate.fm_cycle_remainder_ !=
            (candidate.completed_frames_ * m68k_cycles_per_frame) % fm_cycles_per_sample) {
        throw MediaError("MEDIA_STATE_CLOCK_PHASE_INVALID");
    }
    const auto required_bytes = (static_cast<std::size_t>(fm_scalars) + psg_samples) * 2;
    if (reader.remaining() != required_bytes) throw MediaError("MEDIA_STATE_SIZE_INVALID");
    candidate.pending_fm_.reserve(fm_scalars);
    candidate.pending_psg_.reserve(psg_samples);
    for (std::uint32_t index = 0; index < fm_scalars; ++index) {
        candidate.pending_fm_.push_back(reader.sample());
    }
    for (std::uint32_t index = 0; index < psg_samples; ++index) {
        candidate.pending_psg_.push_back(reader.sample());
    }
    if (candidate.completed_frames_ == 0 &&
        (candidate.held_fm_left_ != 0 || candidate.held_fm_right_ != 0 || candidate.held_psg_ != 0)) {
        throw MediaError("MEDIA_STATE_INITIAL_HOLD_INVALID");
    }
    // Strong replacement guarantee: malformed state never changes this object.
    *this = std::move(candidate);
}

void CanonicalAudio::reset() {
    completed_frames_ = 0;
    output_frames_ = 0;
    output_phase_ = 0;
    fm_cycle_remainder_ = 0;
    held_fm_left_ = 0;
    held_fm_right_ = 0;
    held_psg_ = 0;
    pending_fm_.clear();
    pending_psg_.clear();
    last_frame_info_ = {};
}

} // namespace tessera::megadrive


