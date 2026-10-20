#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <tessera/tessera.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

static_assert(sizeof(void *) == 8);
static_assert(std::is_same_v<tessera_status, std::int32_t>);
static_assert(std::is_same_v<tessera_session, std::uint64_t>);
static_assert(std::is_same_v<tessera_snapshot, std::uint64_t>);
#define ABI_TYPE(type, bytes, alignment)                                       \
  static_assert(sizeof(type) == bytes && alignof(type) == alignment);          \
  static_assert(std::is_standard_layout_v<type> &&                             \
                std::is_trivially_copyable_v<type>)
#define ABI_OFFSET(type, member, offset)                                       \
  static_assert(offsetof(type, member) == offset)

ABI_TYPE(tessera_header_v1, 8, 4);
ABI_OFFSET(tessera_header_v1, size, 0);
ABI_OFFSET(tessera_header_v1, major, 4);
ABI_OFFSET(tessera_header_v1, minor, 6);

ABI_TYPE(tessera_bytes_v1, 16, 8);
ABI_OFFSET(tessera_bytes_v1, data, 0);
ABI_OFFSET(tessera_bytes_v1, bytes, 8);

ABI_TYPE(tessera_buffer_v1, 16, 8);
ABI_OFFSET(tessera_buffer_v1, data, 0);
ABI_OFFSET(tessera_buffer_v1, capacity, 8);

ABI_TYPE(tessera_frame_id_v1, 24, 8);
ABI_OFFSET(tessera_frame_id_v1, session, 0);
ABI_OFFSET(tessera_frame_id_v1, epoch, 8);
ABI_OFFSET(tessera_frame_id_v1, frame, 16);

ABI_TYPE(tessera_context_v1, 64, 8);
ABI_OFFSET(tessera_context_v1, header, 0);
ABI_OFFSET(tessera_context_v1, code, 8);
ABI_OFFSET(tessera_context_v1, phase, 12);
ABI_OFFSET(tessera_context_v1, os_domain, 16);
ABI_OFFSET(tessera_context_v1, os_code, 20);
ABI_OFFSET(tessera_context_v1, frame_id, 24);
ABI_OFFSET(tessera_context_v1, transaction, 48);
ABI_OFFSET(tessera_context_v1, cancel_reason, 56);
ABI_OFFSET(tessera_context_v1, flags, 60);

ABI_TYPE(tessera_api_info_v1, 32, 8);
ABI_OFFSET(tessera_api_info_v1, header, 0);
ABI_OFFSET(tessera_api_info_v1, capabilities, 8);
ABI_OFFSET(tessera_api_info_v1, major, 16);
ABI_OFFSET(tessera_api_info_v1, minor, 18);
ABI_OFFSET(tessera_api_info_v1, pointer_bytes, 20);
ABI_OFFSET(tessera_api_info_v1, max_alignment, 24);
ABI_OFFSET(tessera_api_info_v1, reserved, 28);

ABI_TYPE(tessera_limits_v1, 96, 8);
ABI_OFFSET(tessera_limits_v1, header, 0);
ABI_OFFSET(tessera_limits_v1, content_bytes, 8);
ABI_OFFSET(tessera_limits_v1, checkpoint_bytes, 16);
ABI_OFFSET(tessera_limits_v1, retained_bytes, 24);
ABI_OFFSET(tessera_limits_v1, trace_bytes, 32);
ABI_OFFSET(tessera_limits_v1, video_width, 40);
ABI_OFFSET(tessera_limits_v1, video_height, 44);
ABI_OFFSET(tessera_limits_v1, pcm_frames, 48);
ABI_OFFSET(tessera_limits_v1, event_count, 52);
ABI_OFFSET(tessera_limits_v1, event_payload_bytes, 56);
ABI_OFFSET(tessera_limits_v1, input_count, 60);
ABI_OFFSET(tessera_limits_v1, control_count, 64);
ABI_OFFSET(tessera_limits_v1, retained_count, 68);
ABI_OFFSET(tessera_limits_v1, trace_frames, 72);
ABI_OFFSET(tessera_limits_v1, ports, 76);
ABI_OFFSET(tessera_limits_v1, controls_per_port, 80);
ABI_OFFSET(tessera_limits_v1, reserved, 84);
ABI_OFFSET(tessera_limits_v1, snapshot_bytes, 88);

ABI_TYPE(tessera_backend_info_v1, 152, 8);
ABI_OFFSET(tessera_backend_info_v1, header, 0);
ABI_OFFSET(tessera_backend_info_v1, kind, 8);
ABI_OFFSET(tessera_backend_info_v1, system, 12);
ABI_OFFSET(tessera_backend_info_v1, capabilities, 16);
ABI_OFFSET(tessera_backend_info_v1, id, 24);
ABI_OFFSET(tessera_backend_info_v1, version, 88);
ABI_OFFSET(tessera_backend_info_v1, build_id, 120);

ABI_TYPE(tessera_detection_v1, 80, 8);
ABI_OFFSET(tessera_detection_v1, header, 0);
ABI_OFFSET(tessera_detection_v1, outcome, 8);
ABI_OFFSET(tessera_detection_v1, system, 12);
ABI_OFFSET(tessera_detection_v1, evidence, 16);
ABI_OFFSET(tessera_detection_v1, evidence_offset, 24);
ABI_OFFSET(tessera_detection_v1, evidence_bytes, 32);
ABI_OFFSET(tessera_detection_v1, content_sha256, 40);
ABI_OFFSET(tessera_detection_v1, candidate_count, 72);
ABI_OFFSET(tessera_detection_v1, reserved, 76);

ABI_TYPE(tessera_candidate_v1, 16, 8);
ABI_OFFSET(tessera_candidate_v1, system, 0);
ABI_OFFSET(tessera_candidate_v1, evidence_class, 4);
ABI_OFFSET(tessera_candidate_v1, evidence, 8);

ABI_TYPE(tessera_session_create_info_v1, 80, 8);
ABI_OFFSET(tessera_session_create_info_v1, header, 0);
ABI_OFFSET(tessera_session_create_info_v1, backend_id, 8);
ABI_OFFSET(tessera_session_create_info_v1, content, 24);
ABI_OFFSET(tessera_session_create_info_v1, config, 40);
ABI_OFFSET(tessera_session_create_info_v1, initial_persistent_memory, 56);
ABI_OFFSET(tessera_session_create_info_v1, system_override, 72);
ABI_OFFSET(tessera_session_create_info_v1, flags, 76);

ABI_TYPE(tessera_session_info_v1, 160, 8);
ABI_OFFSET(tessera_session_info_v1, header, 0);
ABI_OFFSET(tessera_session_info_v1, frame_id, 8);
ABI_OFFSET(tessera_session_info_v1, generation, 32);
ABI_OFFSET(tessera_session_info_v1, logical_tick, 40);
ABI_OFFSET(tessera_session_info_v1, tick_hz_num, 48);
ABI_OFFSET(tessera_session_info_v1, tick_hz_den, 56);
ABI_OFFSET(tessera_session_info_v1, capabilities, 64);
ABI_OFFSET(tessera_session_info_v1, state, 72);
ABI_OFFSET(tessera_session_info_v1, system, 76);
ABI_OFFSET(tessera_session_info_v1, content_sha256, 80);
ABI_OFFSET(tessera_session_info_v1, pending_transaction, 112);
ABI_OFFSET(tessera_session_info_v1, next_replay_frame, 120);
ABI_OFFSET(tessera_session_info_v1, backend_build_id, 128);

ABI_TYPE(tessera_input_assignment_v1, 8, 4);
ABI_OFFSET(tessera_input_assignment_v1, port, 0);
ABI_OFFSET(tessera_input_assignment_v1, control, 2);
ABI_OFFSET(tessera_input_assignment_v1, value, 4);

ABI_TYPE(tessera_input_frame_v1, 40, 8);
ABI_OFFSET(tessera_input_frame_v1, header, 0);
ABI_OFFSET(tessera_input_frame_v1, epoch, 8);
ABI_OFFSET(tessera_input_frame_v1, frame, 16);
ABI_OFFSET(tessera_input_frame_v1, assignments, 24);
ABI_OFFSET(tessera_input_frame_v1, count, 32);
ABI_OFFSET(tessera_input_frame_v1, reserved, 36);

ABI_TYPE(tessera_control_item_v1, 16, 8);
ABI_OFFSET(tessera_control_item_v1, id, 0);
ABI_OFFSET(tessera_control_item_v1, value_type, 4);
ABI_OFFSET(tessera_control_item_v1, value, 8);

ABI_TYPE(tessera_control_transaction_v1, 48, 8);
ABI_OFFSET(tessera_control_transaction_v1, header, 0);
ABI_OFFSET(tessera_control_transaction_v1, epoch, 8);
ABI_OFFSET(tessera_control_transaction_v1, frame, 16);
ABI_OFFSET(tessera_control_transaction_v1, id, 24);
ABI_OFFSET(tessera_control_transaction_v1, items, 32);
ABI_OFFSET(tessera_control_transaction_v1, count, 40);
ABI_OFFSET(tessera_control_transaction_v1, reserved, 44);

ABI_TYPE(tessera_original_frame_v1, 120, 8);
ABI_OFFSET(tessera_original_frame_v1, header, 0);
ABI_OFFSET(tessera_original_frame_v1, frame_id, 8);
ABI_OFFSET(tessera_original_frame_v1, width, 32);
ABI_OFFSET(tessera_original_frame_v1, height, 36);
ABI_OFFSET(tessera_original_frame_v1, row_bytes, 40);
ABI_OFFSET(tessera_original_frame_v1, video_format, 44);
ABI_OFFSET(tessera_original_frame_v1, aspect_num, 48);
ABI_OFFSET(tessera_original_frame_v1, aspect_den, 52);
ABI_OFFSET(tessera_original_frame_v1, pixels, 56);
ABI_OFFSET(tessera_original_frame_v1, pcm, 72);
ABI_OFFSET(tessera_original_frame_v1, sample_rate, 88);
ABI_OFFSET(tessera_original_frame_v1, channels, 92);
ABI_OFFSET(tessera_original_frame_v1, pcm_frames, 96);
ABI_OFFSET(tessera_original_frame_v1, tick_begin, 104);
ABI_OFFSET(tessera_original_frame_v1, tick_end, 112);

ABI_TYPE(tessera_snapshot_info_v1, 80, 8);
ABI_OFFSET(tessera_snapshot_info_v1, header, 0);
ABI_OFFSET(tessera_snapshot_info_v1, frame_id, 8);
ABI_OFFSET(tessera_snapshot_info_v1, generation, 32);
ABI_OFFSET(tessera_snapshot_info_v1, region_mask, 40);
ABI_OFFSET(tessera_snapshot_info_v1, complete_mask, 48);
ABI_OFFSET(tessera_snapshot_info_v1, retained_bytes, 56);
ABI_OFFSET(tessera_snapshot_info_v1, logical_tick, 64);
ABI_OFFSET(tessera_snapshot_info_v1, region_count, 72);
ABI_OFFSET(tessera_snapshot_info_v1, reserved, 76);

ABI_TYPE(tessera_snapshot_region_v1, 64, 8);
ABI_OFFSET(tessera_snapshot_region_v1, header, 0);
ABI_OFFSET(tessera_snapshot_region_v1, kind, 8);
ABI_OFFSET(tessera_snapshot_region_v1, encoding, 12);
ABI_OFFSET(tessera_snapshot_region_v1, address, 16);
ABI_OFFSET(tessera_snapshot_region_v1, byte_count, 24);
ABI_OFFSET(tessera_snapshot_region_v1, element_bytes, 32);
ABI_OFFSET(tessera_snapshot_region_v1, element_count, 36);
ABI_OFFSET(tessera_snapshot_region_v1, data, 40);
ABI_OFFSET(tessera_snapshot_region_v1, bank, 56);
ABI_OFFSET(tessera_snapshot_region_v1, flags, 60);

static_assert(TESSERA_ABI_MAJOR == 1);
static_assert(TESSERA_ABI_MINOR == 0);
static_assert(TESSERA_OK == 0);
static_assert(TESSERA_END_OF_TRACE == 1);
static_assert(TESSERA_INVALID_ARGUMENT == -1);
static_assert(TESSERA_ABI_INCOMPATIBLE == -2);
static_assert(TESSERA_BUFFER_TOO_SMALL == -3);
static_assert(TESSERA_INVALID_HANDLE == -4);
static_assert(TESSERA_WRONG_STATE == -5);
static_assert(TESSERA_UNSUPPORTED_CAPABILITY == -6);
static_assert(TESSERA_UNKNOWN_CONTENT == -7);
static_assert(TESSERA_AMBIGUOUS_CONTENT == -8);
static_assert(TESSERA_INVALID_CONTENT == -9);
static_assert(TESSERA_IMPLEMENTATION_UNAVAILABLE == -10);
static_assert(TESSERA_STALE_EPOCH == -11);
static_assert(TESSERA_INPUT_LATE == -12);
static_assert(TESSERA_INPUT_OUT_OF_ORDER == -13);
static_assert(TESSERA_INPUT_DUPLICATE == -14);
static_assert(TESSERA_INPUT_INCOMPLETE == -15);
static_assert(TESSERA_INPUT_CONTRADICTORY == -16);
static_assert(TESSERA_INVALID_TRACE == -17);
static_assert(TESSERA_BUDGET_EXCEEDED == -18);
static_assert(TESSERA_CONTROL_PENDING == -19);
static_assert(TESSERA_CHECKPOINT_INCOMPATIBLE == -20);
static_assert(TESSERA_CHECKPOINT_INVALID == -21);
static_assert(TESSERA_IO_NO_SPACE == -22);
static_assert(TESSERA_IO_ACCESS_DENIED == -23);
static_assert(TESSERA_IO_PUBLISH_FAILED == -24);
static_assert(TESSERA_IO_OTHER == -25);
static_assert(TESSERA_OUT_OF_MEMORY == -26);
static_assert(TESSERA_BACKEND_FAILURE == -27);
static_assert(TESSERA_PHASE_NONE == 0);
static_assert(TESSERA_PHASE_VALIDATE == 1);
static_assert(TESSERA_PHASE_DETECT == 2);
static_assert(TESSERA_PHASE_CREATE == 3);
static_assert(TESSERA_PHASE_STEP == 4);
static_assert(TESSERA_PHASE_RESET == 5);
static_assert(TESSERA_PHASE_READ == 6);
static_assert(TESSERA_PHASE_WRITE == 7);
static_assert(TESSERA_PHASE_PUBLISH == 8);
static_assert(TESSERA_PHASE_RESTORE == 9);
static_assert(TESSERA_PHASE_CLOSE == 10);
static_assert(TESSERA_OS_NONE == 0);
static_assert(TESSERA_OS_WIN32 == 1);
static_assert(TESSERA_CANCEL_NONE == 0);
static_assert(TESSERA_CANCEL_RESET == 1);
static_assert(TESSERA_CANCEL_RESTORE == 2);
static_assert(TESSERA_CANCEL_CLOSE == 3);
static_assert(TESSERA_CONTEXT_CONTROLS_APPLIED == 1);
static_assert(TESSERA_CONTEXT_CONTROL_CANCELLED == 2);
static_assert(TESSERA_SESSION_PAUSED == 1);
static_assert(TESSERA_SESSION_RUNNING == 2);
static_assert(TESSERA_SESSION_CLOSED == 3);
static_assert(TESSERA_SYSTEM_NONE == 0);
static_assert(TESSERA_SYSTEM_MEGA_DRIVE == 1);
static_assert(TESSERA_BACKEND_PRODUCT == 1);
static_assert(TESSERA_BACKEND_TEST == 2);
static_assert(TESSERA_DETECTION_RECOGNIZED == 1);
static_assert(TESSERA_DETECTION_UNKNOWN == 2);
static_assert(TESSERA_DETECTION_AMBIGUOUS == 3);
static_assert(TESSERA_DETECTION_INVALID == 4);
static_assert(TESSERA_CAP_FRAME == 1);
static_assert(TESSERA_CAP_INPUT == 2);
static_assert(TESSERA_CAP_VIDEO == 4);
static_assert(TESSERA_CAP_PCM == 8);
static_assert(TESSERA_CAP_MEMORY == 16);
static_assert(TESSERA_CAP_PALETTE == 32);
static_assert(TESSERA_CAP_SPRITES == 64);
static_assert(TESSERA_CAP_GEOMETRY == 128);
static_assert(TESSERA_CAP_EVENTS == 256);
static_assert(TESSERA_CAP_CHECKPOINT == 512);
static_assert(TESSERA_CAP_SAFE_CONTROLS == 1024);
static_assert(TESSERA_CAP_TEMPORAL_OBSERVATION == 2048);
static_assert(TESSERA_CAP_CANONICAL_STATE == 4096);
static_assert(TESSERA_REGION_MEMORY == 1);
static_assert(TESSERA_REGION_PALETTE == 2);
static_assert(TESSERA_REGION_SPRITES == 4);
static_assert(TESSERA_REGION_GEOMETRY == 8);
static_assert(TESSERA_REGION_TEMPORAL == 16);
static_assert(TESSERA_EVENT_LIFECYCLE == 1);
static_assert(TESSERA_EVENT_CONTROLS == 2);
static_assert(TESSERA_EVENT_BACKEND == 4);
static_assert(TESSERA_EVENT_CAPTURES == 8);
static_assert(TESSERA_EVENT_LOSSES == 16);
static_assert(TESSERA_CONTROL_OBSERVATION_REGION_MASK == 1);
static_assert(TESSERA_CONTROL_OBSERVATION_EVENT_MASK == 2);
static_assert(TESSERA_VALUE_MASK64 == 1);
static_assert(TESSERA_VIDEO_RGBA8 == 1);
static_assert(TESSERA_ENCODING_U32 == 1);
static_assert(TESSERA_ENCODING_I32 == 2);
static_assert(TESSERA_ENCODING_U64 == 3);
static_assert(TESSERA_ENCODING_I64 == 4);
static_assert(TESSERA_ENCODING_BYTES == 5);
static_assert(TESSERA_ENCODING_UTF8 == 6);
static_assert(TESSERA_ENCODING_BOOL8 == 7);
static_assert(TESSERA_ENCODING_ARRAY == 8);
static_assert(TESSERA_ENCODING_F32 == 9);
static_assert(TESSERA_ENCODING_F64 == 10);
static_assert(TESSERA_ENCODING_RECORD == 11);

static_assert(std::is_same_v<decltype(&tessera_api_query_v1),
                             tessera_status(TESSERA_CALL *)(
                                 uint16_t, uint16_t, tessera_api_info_v1 *,
                                 tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_limits_query_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_limits_v1 *,
                                 tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_backends_enumerate_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_backend_info_v1 *, uint32_t,
                                 uint32_t *, tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_detect_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_bytes_v1, uint32_t,
                                 tessera_detection_v1 *, tessera_candidate_v1 *,
                                 uint32_t, uint32_t *, tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_session_create_v1),
                             tessera_status(TESSERA_CALL *)(
                                 const tessera_session_create_info_v1 *,
                                 tessera_session *, tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_session_query_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_session_info_v1 *,
                                 tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_session_pause_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_context_v1 *)>);
static_assert(
    std::is_same_v<decltype(&tessera_session_step_v1),
                   tessera_status(TESSERA_CALL *)(
                       tessera_session, const tessera_input_frame_v1 *,
                       tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_session_reset_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_session_close_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_trace_validate_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_bytes_v1, tessera_context_v1 *)>);
static_assert(std::is_same_v<
              decltype(&tessera_session_bind_trace_v1),
              tessera_status(TESSERA_CALL *)(tessera_session, tessera_bytes_v1,
                                             tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_session_replay_next_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_context_v1 *)>);
static_assert(
    std::is_same_v<decltype(&tessera_controls_submit_v1),
                   tessera_status(TESSERA_CALL *)(
                       tessera_session, const tessera_control_transaction_v1 *,
                       tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_original_frame_query_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_original_frame_v1 *,
                                 tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_snapshot_acquire_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_snapshot *,
                                 tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_snapshot_retain_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_snapshot, tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_snapshot_query_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_snapshot, tessera_snapshot_info_v1 *,
                                 tessera_context_v1 *)>);
static_assert(
    std::is_same_v<decltype(&tessera_snapshot_region_query_v1),
                   tessera_status(TESSERA_CALL *)(tessera_snapshot, uint32_t,
                                                  tessera_snapshot_region_v1 *,
                                                  tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_snapshot_release_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_snapshot, tessera_context_v1 *)>);
static_assert(
    std::is_same_v<decltype(&tessera_events_read_v1),
                   tessera_status(TESSERA_CALL *)(
                       tessera_session, uint64_t, tessera_buffer_v1, uint64_t *,
                       uint64_t *, tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_losses_query_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_buffer_v1, uint64_t *,
                                 tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_state_export_canonical_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_buffer_v1, uint64_t *,
                                 tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_checkpoint_export_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_buffer_v1, uint64_t *,
                                 tessera_context_v1 *)>);
static_assert(std::is_same_v<decltype(&tessera_checkpoint_restore_v1),
                             tessera_status(TESSERA_CALL *)(
                                 tessera_session, tessera_bytes_v1,
                                 tessera_bytes_v1, tessera_context_v1 *)>);
static_assert(std::is_same_v<
              decltype(&tessera_status_format_v1),
              tessera_status(TESSERA_CALL *)(const tessera_context_v1 *,
                                             tessera_buffer_v1, uint64_t *)>);

extern "C" {
tessera_status TESSERA_CALL tessera_test_c_query(tessera_api_info_v1 *,
                                                 tessera_context_v1 *);
tessera_status TESSERA_CALL tessera_test_c_enumerate(tessera_backend_info_v1 *,
                                                     std::uint32_t,
                                                     std::uint32_t *,
                                                     tessera_context_v1 *);
tessera_status TESSERA_CALL tessera_test_c_format(const tessera_context_v1 *,
                                                  tessera_buffer_v1,
                                                  std::uint64_t *);
}

namespace {

template <class T> T initialized() {
  T value{};
  value.header = {static_cast<std::uint32_t>(sizeof(T)), TESSERA_ABI_MAJOR,
                  TESSERA_ABI_MINOR};
  return value;
}
template <class T> auto object_bytes(const T &value) {
  std::array<std::uint8_t, sizeof(T)> result{};
  std::memcpy(result.data(), &value, sizeof(T));
  return result;
}
template <std::size_t N> struct Guarded {
  std::array<std::uint8_t, N + 16> bytes;
  Guarded() { bytes.fill(0xA5); }
  std::uint8_t *data() { return bytes.data() + 8; }
  bool guards_intact() const {
    return std::all_of(bytes.begin(), bytes.begin() + 8,
                       [](auto v) { return v == 0xA5; }) &&
           std::all_of(bytes.end() - 8, bytes.end(),
                       [](auto v) { return v == 0xA5; });
  }
};
std::string ascii_lower(std::string value) {
  for (auto &c : value)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + 'a' - 'A');
  return value;
}
bool valid_utf8(std::string_view text) {
  std::uint32_t scalar{}, minimum{};
  unsigned remaining{};
  for (const unsigned char c : text) {
    if (remaining) {
      if ((c & 0xC0) != 0x80)
        return false;
      scalar = (scalar << 6) | (c & 0x3F);
      if (--remaining == 0 && (scalar < minimum || scalar > 0x10FFFF ||
                               (scalar >= 0xD800 && scalar <= 0xDFFF)))
        return false;
    } else if (c <= 0x7F) {
      if (c == 0)
        return false;
    } else if (c >= 0xC2 && c <= 0xDF) {
      scalar = c & 0x1F;
      minimum = 0x80;
      remaining = 1;
    } else if (c >= 0xE0 && c <= 0xEF) {
      scalar = c & 0x0F;
      minimum = 0x800;
      remaining = 2;
    } else if (c >= 0xF0 && c <= 0xF4) {
      scalar = c & 7;
      minimum = 0x10000;
      remaining = 3;
    } else
      return false;
  }
  return remaining == 0;
}
std::string format_context(const tessera_context_v1 &context) {
  const auto before = object_bytes(context);
  std::uint64_t required = 0;
  REQUIRE(tessera_status_format_v1(&context, {nullptr, 0}, &required) ==
          TESSERA_BUFFER_TOO_SMALL);
  REQUIRE(required > 0);
  REQUIRE(required <= 4096);
  std::vector<std::uint8_t> storage(static_cast<std::size_t>(required) + 16,
                                    0xA5);
  const tessera_buffer_v1 output{storage.data() + 8, required};
  auto repeated = required;
  REQUIRE(tessera_test_c_format(&context, output, &repeated) == TESSERA_OK);
  CHECK(repeated == required);
  CHECK(std::all_of(storage.begin(), storage.begin() + 8,
                    [](auto v) { return v == 0xA5; }));
  CHECK(std::all_of(storage.end() - 8, storage.end(),
                    [](auto v) { return v == 0xA5; }));
  CHECK(object_bytes(context) == before);
  std::string result(reinterpret_cast<const char *>(output.data),
                     static_cast<std::size_t>(required));
  CHECK(valid_utf8(result));
  return result;
}
template <std::size_t N> std::string fixed_id(const char (&value)[N]) {
  const auto end = std::find(std::begin(value), std::end(value), '\0');
  REQUIRE(end != std::end(value));
  CHECK(end != std::begin(value));
  CHECK(std::all_of(end, std::end(value),
                    [](const char c) { return c == '\0'; }));
  CHECK(std::all_of(std::begin(value), end,
                    [](const unsigned char c) { return c < 128; }));
  return {std::begin(value), end};
}
} // namespace

TEST_CASE("ABI 1.0 negotiates from both C and C++ translation units") {
  auto api = initialized<tessera_api_info_v1>();
  auto context = initialized<tessera_context_v1>();
  CHECK_NOTHROW(tessera_test_c_query(&api, &context));
  REQUIRE(tessera_test_c_query(&api, &context) == TESSERA_OK);
  CHECK(context.code == TESSERA_OK);
  CHECK(context.flags == 0);
  CHECK(context.os_domain == TESSERA_OS_NONE);
  CHECK(context.cancel_reason == TESSERA_CANCEL_NONE);
  CHECK(api.major == 1);
  CHECK(api.minor == 0);
  CHECK(api.pointer_bytes == 8);
  CHECK(api.max_alignment == 8);
  CHECK(api.reserved == 0);
  CHECK((api.capabilities & ~UINT64_C(0x1FFF)) == 0);
  CHECK((api.capabilities & TESSERA_CAP_FRAME) != 0);
  auto cpp = initialized<tessera_api_info_v1>();
  REQUIRE(tessera_api_query_v1(1, 0, &cpp, &context) == TESSERA_OK);
  CHECK(cpp.capabilities == api.capabilities);
}

TEST_CASE("ABI rejects incompatible versions and insufficient prefixes without "
          "partial output") {
  for (const auto version :
       std::array<std::array<std::uint16_t, 2>, 3>{{{0, 0}, {2, 0}, {1, 1}}}) {
    auto api = initialized<tessera_api_info_v1>();
    api.capabilities = UINT64_C(0x123456789);
    const auto before = object_bytes(api);
    auto context = initialized<tessera_context_v1>();
    CHECK(tessera_api_query_v1(version[0], version[1], &api, &context) ==
          TESSERA_ABI_INCOMPATIBLE);
    CHECK(context.code == TESSERA_ABI_INCOMPATIBLE);
    CHECK(object_bytes(api) == before);
  }
  for (const auto size : std::array<std::uint32_t, 3>{0, 7, 31}) {
    auto api = initialized<tessera_api_info_v1>();
    api.header.size = size;
    const auto before = object_bytes(api);
    auto context = initialized<tessera_context_v1>();
    CHECK(tessera_api_query_v1(1, 0, &api, &context) ==
          TESSERA_ABI_INCOMPATIBLE);
    CHECK(object_bytes(api) == before);
  }
  auto api = initialized<tessera_api_info_v1>();
  api.header.major = 2;
  const auto before = object_bytes(api);
  auto context = initialized<tessera_context_v1>();
  CHECK(tessera_api_query_v1(1, 0, &api, &context) == TESSERA_ABI_INCOMPATIBLE);
  CHECK(object_bytes(api) == before);
}

TEST_CASE("Context validation precedes writes and compatible extensions "
          "preserve foreign tails") {
  auto api = initialized<tessera_api_info_v1>();
  const auto before = object_bytes(api);
  CHECK(tessera_api_query_v1(1, 0, &api, nullptr) == TESSERA_INVALID_ARGUMENT);
  CHECK(object_bytes(api) == before);
  auto context = initialized<tessera_context_v1>();
  CHECK(tessera_api_query_v1(1, 0, nullptr, &context) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(context.code == TESSERA_INVALID_ARGUMENT);
  for (const auto size : std::array<std::uint32_t, 3>{0, 7, 63}) {
    context = initialized<tessera_context_v1>();
    context.header.size = size;
    const auto contextBefore = object_bytes(context);
    CHECK(tessera_api_query_v1(1, 0, &api, &context) ==
          TESSERA_ABI_INCOMPATIBLE);
    CHECK(object_bytes(api) == before);
    CHECK(object_bytes(context) == contextBefore);
  }
  context = initialized<tessera_context_v1>();
  context.header.major = 2;
  const auto contextBefore = object_bytes(context);
  CHECK(tessera_api_query_v1(1, 0, &api, &context) == TESSERA_ABI_INCOMPATIBLE);
  CHECK(object_bytes(context) == contextBefore);
  struct ExtendedApi {
    tessera_api_info_v1 prefix;
    std::array<std::uint8_t, 24> tail;
  };
  struct ExtendedContext {
    tessera_context_v1 prefix;
    std::array<std::uint8_t, 24> tail;
  };
  ExtendedApi extended{initialized<tessera_api_info_v1>(), {}};
  ExtendedContext extendedContext{initialized<tessera_context_v1>(), {}};
  extended.tail.fill(0xA5);
  extendedContext.tail.fill(0x5A);
  extended.prefix.header.size = sizeof(extended);
  extendedContext.prefix.header.size = sizeof(extendedContext);
  REQUIRE(tessera_api_query_v1(1, 0, &extended.prefix,
                               &extendedContext.prefix) == TESSERA_OK);
  CHECK(extended.prefix.major == 1);
  CHECK(std::all_of(extended.tail.begin(), extended.tail.end(),
                    [](auto v) { return v == 0xA5; }));
  CHECK(std::all_of(extendedContext.tail.begin(), extendedContext.tail.end(),
                    [](auto v) { return v == 0x5A; }));
}

TEST_CASE(
    "Global limits are explicit budgets with their complete 96-byte prefix") {
  auto limits = initialized<tessera_limits_v1>();
  auto context = initialized<tessera_context_v1>();
  REQUIRE(tessera_limits_query_v1(0, &limits, &context) == TESSERA_OK);
  CHECK(limits.content_bytes == UINT64_C(64) * 1024 * 1024);
  CHECK(limits.checkpoint_bytes == UINT64_C(64) * 1024 * 1024);
  CHECK(limits.retained_bytes == UINT64_C(64) * 1024 * 1024);
  CHECK(limits.trace_bytes == UINT64_C(64) * 1024 * 1024);
  CHECK(limits.snapshot_bytes == UINT64_C(8) * 1024 * 1024);
  CHECK(limits.video_width == 1024);
  CHECK(limits.video_height == 1024);
  CHECK(limits.pcm_frames == 16384);
  CHECK(limits.event_count == 4096);
  CHECK(limits.event_payload_bytes == 256);
  CHECK(limits.input_count == 64);
  CHECK(limits.control_count == 64);
  CHECK(limits.retained_count == 8);
  CHECK(limits.trace_frames == 65536);
  CHECK(limits.ports == 2);
  CHECK(limits.controls_per_port == 32);
  CHECK(limits.reserved == 0);
  limits.header.size = 95;
  const auto before = object_bytes(limits);
  CHECK(tessera_limits_query_v1(0, &limits, &context) ==
        TESSERA_ABI_INCOMPATIBLE);
  CHECK(object_bytes(limits) == before);
  CHECK(tessera_limits_query_v1(0, nullptr, &context) ==
        TESSERA_INVALID_ARGUMENT);
}

TEST_CASE("Extended structures reject aliases in foreign tails before writing "
          "either object") {
  struct ExtendedContext {
    tessera_context_v1 prefix;
    tessera_api_info_v1 tail;
  };
  ExtendedContext storage{initialized<tessera_context_v1>(),
                          initialized<tessera_api_info_v1>()};
  storage.prefix.header.size = sizeof(storage);
  const auto before = object_bytes(storage);
  CHECK(tessera_api_query_v1(1, 0, &storage.tail, &storage.prefix) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(object_bytes(storage) == before);
}

TEST_CASE("Extended backend arrays honor each declared stride and preserve "
          "foreign tails") {
  struct ExtendedBackend {
    tessera_backend_info_v1 prefix;
    std::array<std::uint8_t, 8> tail;
  };
  std::array<ExtendedBackend, 3> values{};
  for (auto &value : values) {
    value.prefix = initialized<tessera_backend_info_v1>();
    value.prefix.header.size = sizeof(value);
    value.tail.fill(0xA5);
  }
  auto context = initialized<tessera_context_v1>();
  std::uint32_t required{};
  REQUIRE(tessera_backends_enumerate_v1(&values[0].prefix, 3, &required,
                                        &context) == TESSERA_OK);
  CHECK(required == 3);
  CHECK(fixed_id(values[0].prefix.id) == "clownmdemu");
  CHECK(fixed_id(values[1].prefix.id) == "synthetic.minimal");
  CHECK(fixed_id(values[2].prefix.id) == "synthetic.complete");
  for (const auto &value : values)
    CHECK(std::all_of(value.tail.begin(), value.tail.end(),
                      [](auto b) { return b == 0xA5; }));
}

TEST_CASE("Backend enumeration is all-or-nothing and copies test identities "
          "into caller storage") {
  auto context = initialized<tessera_context_v1>();
  std::uint32_t required = 0;
  REQUIRE(tessera_backends_enumerate_v1(nullptr, 0, &required, &context) ==
          TESSERA_BUFFER_TOO_SMALL);
  REQUIRE(required == 3);
  std::array<tessera_backend_info_v1, 3> backends{
      initialized<tessera_backend_info_v1>(),
      initialized<tessera_backend_info_v1>(),
      initialized<tessera_backend_info_v1>()};
  const auto before = object_bytes(backends);
  CHECK(tessera_backends_enumerate_v1(backends.data(), 1, &required,
                                      &context) == TESSERA_BUFFER_TOO_SMALL);
  CHECK(required == 3);
  CHECK(object_bytes(backends) == before);
  CHECK(tessera_backends_enumerate_v1(nullptr, 3, &required, &context) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(tessera_backends_enumerate_v1(backends.data(), 3, nullptr, &context) ==
        TESSERA_INVALID_ARGUMENT);
  backends[1].header.size = 151;
  const auto badPrefix = object_bytes(backends);
  CHECK(tessera_backends_enumerate_v1(backends.data(), 3, &required,
                                      &context) == TESSERA_ABI_INCOMPATIBLE);
  CHECK(object_bytes(backends) == badPrefix);
  backends[1] = initialized<tessera_backend_info_v1>();
  REQUIRE(tessera_test_c_enumerate(backends.data(), 3, &required, &context) ==
          TESSERA_OK);
  std::set<std::string> ids;
  for (const auto &backend : backends) {
    CHECK(backend.kind == (fixed_id(backend.id) == "clownmdemu" ?
                               TESSERA_BACKEND_PRODUCT : TESSERA_BACKEND_TEST));
    CHECK(backend.system == (fixed_id(backend.id) == "clownmdemu" ?
                                 TESSERA_SYSTEM_MEGA_DRIVE : TESSERA_SYSTEM_NONE));
    CHECK((backend.capabilities & ~UINT64_C(0x1FFF)) == 0);
    ids.insert(fixed_id(backend.id));
    CHECK(!fixed_id(backend.version).empty());
    CHECK(std::any_of(std::begin(backend.build_id), std::end(backend.build_id),
                      [](auto v) { return v != 0; }));
  }
  CHECK(ids ==
        std::set<std::string>{"clownmdemu", "synthetic.minimal", "synthetic.complete"});
  const std::string first = fixed_id(backends[0].id);
  backends[0].id[0] = '?'; // A caller copy cannot corrupt the registry.
  std::array<tessera_backend_info_v1, 3> repeated{
      initialized<tessera_backend_info_v1>(),
      initialized<tessera_backend_info_v1>(),
      initialized<tessera_backend_info_v1>()};
  REQUIRE(tessera_backends_enumerate_v1(repeated.data(), 3, &required,
                                        &context) == TESSERA_OK);
  CHECK(fixed_id(repeated[0].id) == first);
  // Capability absence and live-session lifetime cases are added by T009/T012.
}

TEST_CASE("All opaque-handle operations reject zero and fabricated tokens "
          "without payload writes") {
  for (const auto handle :
       std::array<std::uint64_t, 2>{0, UINT64_C(0xFEDCBA9876543210)}) {
    CAPTURE(handle);
    const auto check = [&](auto &&action) {
      auto context = initialized<tessera_context_v1>();
      CHECK_NOTHROW(action(&context));
      CHECK(action(&context) == TESSERA_INVALID_HANDLE);
      CHECK(context.code == TESSERA_INVALID_HANDLE);
    };
    auto info = initialized<tessera_session_info_v1>();
    const auto infoBefore = object_bytes(info);
    check([&](auto *c) { return tessera_session_query_v1(handle, &info, c); });
    CHECK(object_bytes(info) == infoBefore);
    check([&](auto *c) { return tessera_session_pause_v1(handle, c); });
    check([&](auto *c) { return tessera_session_reset_v1(handle, c); });
    check([&](auto *c) { return tessera_session_close_v1(handle, c); });
    std::array<tessera_input_assignment_v1, 8> assignments{};
    for (std::uint16_t i = 0; i < 8; ++i)
      assignments[i] = {0, i, 0};
    auto input = initialized<tessera_input_frame_v1>();
    input.epoch = 1;
    input.frame = 1;
    input.assignments = assignments.data();
    input.count = 8;
    check([&](auto *c) { return tessera_session_step_v1(handle, &input, c); });
    check([&](auto *c) {
      return tessera_session_bind_trace_v1(handle, {nullptr, 0}, c);
    });
    check([&](auto *c) { return tessera_session_replay_next_v1(handle, c); });
    const tessera_control_item_v1 item{TESSERA_CONTROL_OBSERVATION_REGION_MASK,
                                       TESSERA_VALUE_MASK64, 0};
    auto controls = initialized<tessera_control_transaction_v1>();
    controls.epoch = 1;
    controls.frame = 1;
    controls.id = 1;
    controls.items = &item;
    controls.count = 1;
    check([&](auto *c) {
      return tessera_controls_submit_v1(handle, &controls, c);
    });
    auto frame = initialized<tessera_original_frame_v1>();
    const auto frameBefore = object_bytes(frame);
    check([&](auto *c) {
      return tessera_original_frame_query_v1(handle, &frame, c);
    });
    CHECK(object_bytes(frame) == frameBefore);
    tessera_snapshot acquired = UINT64_C(0x1234);
    check([&](auto *c) {
      return tessera_snapshot_acquire_v1(handle, &acquired, c);
    });
    CHECK(acquired == UINT64_C(0x1234));
    check([&](auto *c) { return tessera_snapshot_retain_v1(handle, c); });
    auto snapshot = initialized<tessera_snapshot_info_v1>();
    const auto snapshotBefore = object_bytes(snapshot);
    check([&](auto *c) {
      return tessera_snapshot_query_v1(handle, &snapshot, c);
    });
    CHECK(object_bytes(snapshot) == snapshotBefore);
    auto region = initialized<tessera_snapshot_region_v1>();
    const auto regionBefore = object_bytes(region);
    check([&](auto *c) {
      return tessera_snapshot_region_query_v1(handle, 0, &region, c);
    });
    CHECK(object_bytes(region) == regionBefore);
    check([&](auto *c) { return tessera_snapshot_release_v1(handle, c); });
    Guarded<128> buffer;
    const auto bufferBefore = buffer.bytes;
    std::uint64_t required = 0, next = 0;
    const tessera_buffer_v1 output{buffer.data(), 128};
    check([&](auto *c) {
      return tessera_events_read_v1(handle, 0, output, &required, &next, c);
    });
    check([&](auto *c) {
      return tessera_losses_query_v1(handle, output, &required, c);
    });
    check([&](auto *c) {
      return tessera_state_export_canonical_v1(handle, output, &required, c);
    });
    check([&](auto *c) {
      return tessera_checkpoint_export_v1(handle, output, &required, c);
    });
    check([&](auto *c) {
      return tessera_checkpoint_restore_v1(handle, {nullptr, 0}, {nullptr, 0},
                                           c);
    });
    CHECK(buffer.bytes == bufferBefore);
    if (handle != 0) {
      auto limits = initialized<tessera_limits_v1>();
      const auto limitsBefore = object_bytes(limits);
      check(
          [&](auto *c) { return tessera_limits_query_v1(handle, &limits, c); });
      CHECK(object_bytes(limits) == limitsBefore);
    }
  }
}

TEST_CASE("Malformed byte views and missing create outputs are rejected before "
          "ownership transfer") {
  auto context = initialized<tessera_context_v1>();
  tessera_session session = UINT64_C(0x4567);
  CHECK(tessera_session_create_v1(nullptr, &session, &context) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(session == UINT64_C(0x4567));
  auto creation = initialized<tessera_session_create_info_v1>();
  creation.backend_id = {nullptr, 1};
  CHECK(tessera_session_create_v1(&creation, &session, &context) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(session == UINT64_C(0x4567));
  auto detection = initialized<tessera_detection_v1>();
  const auto before = object_bytes(detection);
  std::uint32_t required = 0;
  CHECK(tessera_detect_v1({nullptr, 1}, TESSERA_SYSTEM_NONE, &detection,
                          nullptr, 0, &required,
                          &context) == TESSERA_INVALID_ARGUMENT);
  CHECK(object_bytes(detection) == before);
  CHECK(tessera_trace_validate_v1({nullptr, 1}, &context) ==
        TESSERA_INVALID_ARGUMENT);
}

TEST_CASE("Every stable result has a Spanish UTF-8 message with exact "
          "caller-owned byte length") {
  struct MessageCase {
    tessera_status code;
    const char *keyword;
  };
  const std::array<MessageCase, 29> cases{
      {{TESSERA_OK, "complet"},
       {TESSERA_END_OF_TRACE, "traza"},
       {TESSERA_INVALID_ARGUMENT, "argument"},
       {TESSERA_ABI_INCOMPATIBLE, "abi"},
       {TESSERA_BUFFER_TOO_SMALL, "buffer"},
       {TESSERA_INVALID_HANDLE, "inv"},
       {TESSERA_WRONG_STATE, "estado"},
       {TESSERA_UNSUPPORTED_CAPABILITY, "capacidad"},
       {TESSERA_UNKNOWN_CONTENT, "contenido"},
       {TESSERA_AMBIGUOUS_CONTENT, "ambigu"},
       {TESSERA_INVALID_CONTENT, "contenido"},
       {TESSERA_IMPLEMENTATION_UNAVAILABLE, "implement"},
       {TESSERA_STALE_EPOCH, "poca"},
       {TESSERA_INPUT_LATE, "entrada"},
       {TESSERA_INPUT_OUT_OF_ORDER, "entrada"},
       {TESSERA_INPUT_DUPLICATE, "entrada"},
       {TESSERA_INPUT_INCOMPLETE, "entrada"},
       {TESSERA_INPUT_CONTRADICTORY, "entrada"},
       {TESSERA_INVALID_TRACE, "traza"},
       {TESSERA_BUDGET_EXCEEDED, "presupuesto"},
       {TESSERA_CONTROL_PENDING, "pendiente"},
       {TESSERA_CHECKPOINT_INCOMPATIBLE, "checkpoint"},
       {TESSERA_CHECKPOINT_INVALID, "checkpoint"},
       {TESSERA_IO_NO_SPACE, "espacio"},
       {TESSERA_IO_ACCESS_DENIED, "acceso"},
       {TESSERA_IO_PUBLISH_FAILED, "public"},
       {TESSERA_IO_OTHER, "e/s"},
       {TESSERA_OUT_OF_MEMORY, "memoria"},
       {TESSERA_BACKEND_FAILURE, "backend"}}};
  for (const auto &entry : cases) {
    CAPTURE(entry.code);
    auto context = initialized<tessera_context_v1>();
    context.code = entry.code;
    CHECK(ascii_lower(format_context(context)).find(entry.keyword) !=
          std::string::npos);
  }
}

TEST_CASE("Status formatting publishes no partial bytes and preserves input "
          "context") {
  auto context = initialized<tessera_context_v1>();
  context.code = TESSERA_IO_NO_SPACE;
  context.phase = TESSERA_PHASE_WRITE;
  context.os_domain = TESSERA_OS_WIN32;
  context.os_code = 112;
  const auto before = object_bytes(context);
  std::uint64_t required = 0;
  REQUIRE(tessera_status_format_v1(&context, {nullptr, 0}, &required) ==
          TESSERA_BUFFER_TOO_SMALL);
  REQUIRE(required > 1);
  REQUIRE(required <= 1024);
  Guarded<1024> buffer;
  const auto bytesBefore = buffer.bytes;
  auto repeated = required;
  CHECK(tessera_status_format_v1(&context, {buffer.data(), required - 1},
                                 &repeated) == TESSERA_BUFFER_TOO_SMALL);
  CHECK(repeated == required);
  CHECK(buffer.bytes == bytesBefore);
  CHECK(object_bytes(context) == before);
  CHECK(tessera_status_format_v1(&context, {nullptr, 1}, &repeated) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(tessera_status_format_v1(&context, {buffer.data(), 1024}, nullptr) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(tessera_status_format_v1(nullptr, {buffer.data(), 1024}, &repeated) ==
        TESSERA_INVALID_ARGUMENT);
  CHECK(buffer.bytes == bytesBefore);
  auto malformed = context;
  malformed.header.size = 63;
  CHECK(tessera_status_format_v1(&malformed, {buffer.data(), 1024},
                                 &repeated) == TESSERA_ABI_INCOMPATIBLE);
  CHECK(buffer.bytes == bytesBefore);
  CHECK(buffer.guards_intact());
  CHECK(object_bytes(context) == before);
}

TEST_CASE("Cancellation and I/O formatting retain their structured identities "
          "and Spanish context") {
  for (const auto reason : std::array<std::uint32_t, 3>{TESSERA_CANCEL_RESET,
                                                        TESSERA_CANCEL_RESTORE,
                                                        TESSERA_CANCEL_CLOSE}) {
    auto context = initialized<tessera_context_v1>();
    context.code = TESSERA_OK;
    context.flags = TESSERA_CONTEXT_CONTROL_CANCELLED;
    context.cancel_reason = reason;
    context.transaction = 147;
    context.frame_id = {5, 3, 17};
    context.phase = reason == TESSERA_CANCEL_RESET     ? TESSERA_PHASE_RESET
                    : reason == TESSERA_CANCEL_RESTORE ? TESSERA_PHASE_RESTORE
                                                       : TESSERA_PHASE_CLOSE;
    const auto text = ascii_lower(format_context(context));
    CHECK(text.find("147") != std::string::npos);
    CHECK(text.find("cancel") != std::string::npos);
  }
  for (const auto phase : std::array<std::uint32_t, 3>{
           TESSERA_PHASE_READ, TESSERA_PHASE_WRITE, TESSERA_PHASE_PUBLISH}) {
    auto context = initialized<tessera_context_v1>();
    context.code = TESSERA_IO_ACCESS_DENIED;
    context.phase = phase;
    context.os_domain = TESSERA_OS_WIN32;
    context.os_code = 5;
    const auto text = ascii_lower(format_context(context));
    CHECK(text.find("acceso") != std::string::npos);
    CHECK(text.find('5') != std::string::npos);
  }
}
