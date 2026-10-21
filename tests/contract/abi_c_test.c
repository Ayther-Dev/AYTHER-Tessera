#include <stddef.h>
#include <stdint.h>
#include <tessera/tessera.h>

_Static_assert(sizeof(void *) == 8, "ABI 1.0 requires 64-bit pointers.");
_Static_assert(sizeof(tessera_status) == 4, "Status has fixed signed width.");
_Static_assert(sizeof(tessera_session) == 8 && sizeof(tessera_snapshot) == 8,
               "Handles are 64-bit tokens.");
#define ABI_TYPE(type, bytes, alignment)                                       \
  _Static_assert(sizeof(type) == bytes, #type " size");                        \
  _Static_assert(_Alignof(type) == alignment, #type " alignment")
#define ABI_OFFSET(type, member, offset)                                       \
  _Static_assert(offsetof(type, member) == offset, #type "." #member)

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

_Static_assert(TESSERA_ABI_MAJOR == 1,
               "TESSERA_ABI_MAJOR has its ABI 1.0 value.");
_Static_assert(TESSERA_ABI_MINOR == 0,
               "TESSERA_ABI_MINOR has its ABI 1.0 value.");
_Static_assert(TESSERA_OK == 0, "TESSERA_OK has its ABI 1.0 value.");
_Static_assert(TESSERA_END_OF_TRACE == 1,
               "TESSERA_END_OF_TRACE has its ABI 1.0 value.");
_Static_assert(TESSERA_INVALID_ARGUMENT == -1,
               "TESSERA_INVALID_ARGUMENT has its ABI 1.0 value.");
_Static_assert(TESSERA_ABI_INCOMPATIBLE == -2,
               "TESSERA_ABI_INCOMPATIBLE has its ABI 1.0 value.");
_Static_assert(TESSERA_BUFFER_TOO_SMALL == -3,
               "TESSERA_BUFFER_TOO_SMALL has its ABI 1.0 value.");
_Static_assert(TESSERA_INVALID_HANDLE == -4,
               "TESSERA_INVALID_HANDLE has its ABI 1.0 value.");
_Static_assert(TESSERA_WRONG_STATE == -5,
               "TESSERA_WRONG_STATE has its ABI 1.0 value.");
_Static_assert(TESSERA_UNSUPPORTED_CAPABILITY == -6,
               "TESSERA_UNSUPPORTED_CAPABILITY has its ABI 1.0 value.");
_Static_assert(TESSERA_UNKNOWN_CONTENT == -7,
               "TESSERA_UNKNOWN_CONTENT has its ABI 1.0 value.");
_Static_assert(TESSERA_AMBIGUOUS_CONTENT == -8,
               "TESSERA_AMBIGUOUS_CONTENT has its ABI 1.0 value.");
_Static_assert(TESSERA_INVALID_CONTENT == -9,
               "TESSERA_INVALID_CONTENT has its ABI 1.0 value.");
_Static_assert(TESSERA_IMPLEMENTATION_UNAVAILABLE == -10,
               "TESSERA_IMPLEMENTATION_UNAVAILABLE has its ABI 1.0 value.");
_Static_assert(TESSERA_STALE_EPOCH == -11,
               "TESSERA_STALE_EPOCH has its ABI 1.0 value.");
_Static_assert(TESSERA_INPUT_LATE == -12,
               "TESSERA_INPUT_LATE has its ABI 1.0 value.");
_Static_assert(TESSERA_INPUT_OUT_OF_ORDER == -13,
               "TESSERA_INPUT_OUT_OF_ORDER has its ABI 1.0 value.");
_Static_assert(TESSERA_INPUT_DUPLICATE == -14,
               "TESSERA_INPUT_DUPLICATE has its ABI 1.0 value.");
_Static_assert(TESSERA_INPUT_INCOMPLETE == -15,
               "TESSERA_INPUT_INCOMPLETE has its ABI 1.0 value.");
_Static_assert(TESSERA_INPUT_CONTRADICTORY == -16,
               "TESSERA_INPUT_CONTRADICTORY has its ABI 1.0 value.");
_Static_assert(TESSERA_INVALID_TRACE == -17,
               "TESSERA_INVALID_TRACE has its ABI 1.0 value.");
_Static_assert(TESSERA_BUDGET_EXCEEDED == -18,
               "TESSERA_BUDGET_EXCEEDED has its ABI 1.0 value.");
_Static_assert(TESSERA_CONTROL_PENDING == -19,
               "TESSERA_CONTROL_PENDING has its ABI 1.0 value.");
_Static_assert(TESSERA_CHECKPOINT_INCOMPATIBLE == -20,
               "TESSERA_CHECKPOINT_INCOMPATIBLE has its ABI 1.0 value.");
_Static_assert(TESSERA_CHECKPOINT_INVALID == -21,
               "TESSERA_CHECKPOINT_INVALID has its ABI 1.0 value.");
_Static_assert(TESSERA_IO_NO_SPACE == -22,
               "TESSERA_IO_NO_SPACE has its ABI 1.0 value.");
_Static_assert(TESSERA_IO_ACCESS_DENIED == -23,
               "TESSERA_IO_ACCESS_DENIED has its ABI 1.0 value.");
_Static_assert(TESSERA_IO_PUBLISH_FAILED == -24,
               "TESSERA_IO_PUBLISH_FAILED has its ABI 1.0 value.");
_Static_assert(TESSERA_IO_OTHER == -25,
               "TESSERA_IO_OTHER has its ABI 1.0 value.");
_Static_assert(TESSERA_OUT_OF_MEMORY == -26,
               "TESSERA_OUT_OF_MEMORY has its ABI 1.0 value.");
_Static_assert(TESSERA_BACKEND_FAILURE == -27,
               "TESSERA_BACKEND_FAILURE has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_NONE == 0,
               "TESSERA_PHASE_NONE has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_VALIDATE == 1,
               "TESSERA_PHASE_VALIDATE has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_DETECT == 2,
               "TESSERA_PHASE_DETECT has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_CREATE == 3,
               "TESSERA_PHASE_CREATE has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_STEP == 4,
               "TESSERA_PHASE_STEP has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_RESET == 5,
               "TESSERA_PHASE_RESET has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_READ == 6,
               "TESSERA_PHASE_READ has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_WRITE == 7,
               "TESSERA_PHASE_WRITE has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_PUBLISH == 8,
               "TESSERA_PHASE_PUBLISH has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_RESTORE == 9,
               "TESSERA_PHASE_RESTORE has its ABI 1.0 value.");
_Static_assert(TESSERA_PHASE_CLOSE == 10,
               "TESSERA_PHASE_CLOSE has its ABI 1.0 value.");
_Static_assert(TESSERA_OS_NONE == 0, "TESSERA_OS_NONE has its ABI 1.0 value.");
_Static_assert(TESSERA_OS_WIN32 == 1,
               "TESSERA_OS_WIN32 has its ABI 1.0 value.");
_Static_assert(TESSERA_CANCEL_NONE == 0,
               "TESSERA_CANCEL_NONE has its ABI 1.0 value.");
_Static_assert(TESSERA_CANCEL_RESET == 1,
               "TESSERA_CANCEL_RESET has its ABI 1.0 value.");
_Static_assert(TESSERA_CANCEL_RESTORE == 2,
               "TESSERA_CANCEL_RESTORE has its ABI 1.0 value.");
_Static_assert(TESSERA_CANCEL_CLOSE == 3,
               "TESSERA_CANCEL_CLOSE has its ABI 1.0 value.");
_Static_assert(TESSERA_CONTEXT_CONTROLS_APPLIED == 1,
               "TESSERA_CONTEXT_CONTROLS_APPLIED has its ABI 1.0 value.");
_Static_assert(TESSERA_CONTEXT_CONTROL_CANCELLED == 2,
               "TESSERA_CONTEXT_CONTROL_CANCELLED has its ABI 1.0 value.");
_Static_assert(TESSERA_SESSION_PAUSED == 1,
               "TESSERA_SESSION_PAUSED has its ABI 1.0 value.");
_Static_assert(TESSERA_SESSION_RUNNING == 2,
               "TESSERA_SESSION_RUNNING has its ABI 1.0 value.");
_Static_assert(TESSERA_SESSION_CLOSED == 3,
               "TESSERA_SESSION_CLOSED has its ABI 1.0 value.");
_Static_assert(TESSERA_SYSTEM_NONE == 0,
               "TESSERA_SYSTEM_NONE has its ABI 1.0 value.");
_Static_assert(TESSERA_SYSTEM_MEGA_DRIVE == 1,
               "TESSERA_SYSTEM_MEGA_DRIVE has its ABI 1.0 value.");
_Static_assert(TESSERA_BACKEND_PRODUCT == 1,
               "TESSERA_BACKEND_PRODUCT has its ABI 1.0 value.");
_Static_assert(TESSERA_BACKEND_TEST == 2,
               "TESSERA_BACKEND_TEST has its ABI 1.0 value.");
_Static_assert(TESSERA_DETECTION_RECOGNIZED == 1,
               "TESSERA_DETECTION_RECOGNIZED has its ABI 1.0 value.");
_Static_assert(TESSERA_DETECTION_UNKNOWN == 2,
               "TESSERA_DETECTION_UNKNOWN has its ABI 1.0 value.");
_Static_assert(TESSERA_DETECTION_AMBIGUOUS == 3,
               "TESSERA_DETECTION_AMBIGUOUS has its ABI 1.0 value.");
_Static_assert(TESSERA_DETECTION_INVALID == 4,
               "TESSERA_DETECTION_INVALID has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_FRAME == 1,
               "TESSERA_CAP_FRAME has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_INPUT == 2,
               "TESSERA_CAP_INPUT has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_VIDEO == 4,
               "TESSERA_CAP_VIDEO has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_PCM == 8, "TESSERA_CAP_PCM has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_MEMORY == 16,
               "TESSERA_CAP_MEMORY has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_PALETTE == 32,
               "TESSERA_CAP_PALETTE has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_SPRITES == 64,
               "TESSERA_CAP_SPRITES has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_GEOMETRY == 128,
               "TESSERA_CAP_GEOMETRY has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_EVENTS == 256,
               "TESSERA_CAP_EVENTS has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_CHECKPOINT == 512,
               "TESSERA_CAP_CHECKPOINT has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_SAFE_CONTROLS == 1024,
               "TESSERA_CAP_SAFE_CONTROLS has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_TEMPORAL_OBSERVATION == 2048,
               "TESSERA_CAP_TEMPORAL_OBSERVATION has its ABI 1.0 value.");
_Static_assert(TESSERA_CAP_CANONICAL_STATE == 4096,
               "TESSERA_CAP_CANONICAL_STATE has its ABI 1.0 value.");
_Static_assert(TESSERA_REGION_MEMORY == 1,
               "TESSERA_REGION_MEMORY has its ABI 1.0 value.");
_Static_assert(TESSERA_REGION_PALETTE == 2,
               "TESSERA_REGION_PALETTE has its ABI 1.0 value.");
_Static_assert(TESSERA_REGION_SPRITES == 4,
               "TESSERA_REGION_SPRITES has its ABI 1.0 value.");
_Static_assert(TESSERA_REGION_GEOMETRY == 8,
               "TESSERA_REGION_GEOMETRY has its ABI 1.0 value.");
_Static_assert(TESSERA_REGION_TEMPORAL == 16,
               "TESSERA_REGION_TEMPORAL has its ABI 1.0 value.");
_Static_assert(TESSERA_EVENT_LIFECYCLE == 1,
               "TESSERA_EVENT_LIFECYCLE has its ABI 1.0 value.");
_Static_assert(TESSERA_EVENT_CONTROLS == 2,
               "TESSERA_EVENT_CONTROLS has its ABI 1.0 value.");
_Static_assert(TESSERA_EVENT_BACKEND == 4,
               "TESSERA_EVENT_BACKEND has its ABI 1.0 value.");
_Static_assert(TESSERA_EVENT_CAPTURES == 8,
               "TESSERA_EVENT_CAPTURES has its ABI 1.0 value.");
_Static_assert(TESSERA_EVENT_LOSSES == 16,
               "TESSERA_EVENT_LOSSES has its ABI 1.0 value.");
_Static_assert(
    TESSERA_CONTROL_OBSERVATION_REGION_MASK == 1,
    "TESSERA_CONTROL_OBSERVATION_REGION_MASK has its ABI 1.0 value.");
_Static_assert(TESSERA_CONTROL_OBSERVATION_EVENT_MASK == 2,
               "TESSERA_CONTROL_OBSERVATION_EVENT_MASK has its ABI 1.0 value.");
_Static_assert(TESSERA_VALUE_MASK64 == 1,
               "TESSERA_VALUE_MASK64 has its ABI 1.0 value.");
_Static_assert(TESSERA_VIDEO_RGBA8 == 1,
               "TESSERA_VIDEO_RGBA8 has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_U32 == 1,
               "TESSERA_ENCODING_U32 has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_I32 == 2,
               "TESSERA_ENCODING_I32 has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_U64 == 3,
               "TESSERA_ENCODING_U64 has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_I64 == 4,
               "TESSERA_ENCODING_I64 has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_BYTES == 5,
               "TESSERA_ENCODING_BYTES has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_UTF8 == 6,
               "TESSERA_ENCODING_UTF8 has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_BOOL8 == 7,
               "TESSERA_ENCODING_BOOL8 has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_ARRAY == 8,
               "TESSERA_ENCODING_ARRAY has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_F32 == 9,
               "TESSERA_ENCODING_F32 has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_F64 == 10,
               "TESSERA_ENCODING_F64 has its ABI 1.0 value.");
_Static_assert(TESSERA_ENCODING_RECORD == 11,
               "TESSERA_ENCODING_RECORD has its ABI 1.0 value.");

typedef tessera_status(TESSERA_CALL *abi_function_0)(uint16_t, uint16_t,
                                                     tessera_api_info_v1 *,
                                                     tessera_context_v1 *);
_Static_assert(_Generic(&tessera_api_query_v1, abi_function_0: 1, default: 0),
               "tessera_api_query_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_1)(tessera_session,
                                                     tessera_limits_v1 *,
                                                     tessera_context_v1 *);
_Static_assert(_Generic(&tessera_limits_query_v1,
                   abi_function_1: 1,
                   default: 0),
               "tessera_limits_query_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_2)(tessera_backend_info_v1 *,
                                                     uint32_t, uint32_t *,
                                                     tessera_context_v1 *);
_Static_assert(_Generic(&tessera_backends_enumerate_v1,
                   abi_function_2: 1,
                   default: 0),
               "tessera_backends_enumerate_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_3)(tessera_bytes_v1, uint32_t,
                                                     tessera_detection_v1 *,
                                                     tessera_candidate_v1 *,
                                                     uint32_t, uint32_t *,
                                                     tessera_context_v1 *);
_Static_assert(_Generic(&tessera_detect_v1, abi_function_3: 1, default: 0),
               "tessera_detect_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_4)(
    const tessera_session_create_info_v1 *, tessera_session *,
    tessera_context_v1 *);
_Static_assert(_Generic(&tessera_session_create_v1,
                   abi_function_4: 1,
                   default: 0),
               "tessera_session_create_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_5)(tessera_session,
                                                     tessera_session_info_v1 *,
                                                     tessera_context_v1 *);
_Static_assert(_Generic(&tessera_session_query_v1,
                   abi_function_5: 1,
                   default: 0),
               "tessera_session_query_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_6)(tessera_session,
                                                     tessera_context_v1 *);
_Static_assert(_Generic(&tessera_session_pause_v1,
                   abi_function_6: 1,
                   default: 0),
               "tessera_session_pause_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_7)(
    tessera_session, const tessera_input_frame_v1 *, tessera_context_v1 *);
_Static_assert(_Generic(&tessera_session_step_v1,
                   abi_function_7: 1,
                   default: 0),
               "tessera_session_step_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_8)(tessera_session,
                                                     tessera_context_v1 *);
_Static_assert(_Generic(&tessera_session_reset_v1,
                   abi_function_8: 1,
                   default: 0),
               "tessera_session_reset_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_9)(tessera_session,
                                                     tessera_context_v1 *);
_Static_assert(_Generic(&tessera_session_close_v1,
                   abi_function_9: 1,
                   default: 0),
               "tessera_session_close_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_10)(tessera_bytes_v1,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_trace_validate_v1,
                   abi_function_10: 1,
                   default: 0),
               "tessera_trace_validate_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_11)(tessera_session,
                                                      tessera_bytes_v1,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_session_bind_trace_v1,
                   abi_function_11: 1,
                   default: 0),
               "tessera_session_bind_trace_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_12)(tessera_session,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_session_replay_next_v1,
                   abi_function_12: 1,
                   default: 0),
               "tessera_session_replay_next_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_13)(
    tessera_session, const tessera_control_transaction_v1 *,
    tessera_context_v1 *);
_Static_assert(_Generic(&tessera_controls_submit_v1,
                   abi_function_13: 1,
                   default: 0),
               "tessera_controls_submit_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_14)(
    tessera_session, tessera_original_frame_v1 *, tessera_context_v1 *);
_Static_assert(_Generic(&tessera_original_frame_query_v1,
                   abi_function_14: 1,
                   default: 0),
               "tessera_original_frame_query_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_15)(tessera_session,
                                                      tessera_snapshot *,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_snapshot_acquire_v1,
                   abi_function_15: 1,
                   default: 0),
               "tessera_snapshot_acquire_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_16)(tessera_snapshot,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_snapshot_retain_v1,
                   abi_function_16: 1,
                   default: 0),
               "tessera_snapshot_retain_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_17)(
    tessera_snapshot, tessera_snapshot_info_v1 *, tessera_context_v1 *);
_Static_assert(_Generic(&tessera_snapshot_query_v1,
                   abi_function_17: 1,
                   default: 0),
               "tessera_snapshot_query_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_18)(
    tessera_snapshot, uint32_t, tessera_snapshot_region_v1 *,
    tessera_context_v1 *);
_Static_assert(_Generic(&tessera_snapshot_region_query_v1,
                   abi_function_18: 1,
                   default: 0),
               "tessera_snapshot_region_query_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_19)(tessera_snapshot,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_snapshot_release_v1,
                   abi_function_19: 1,
                   default: 0),
               "tessera_snapshot_release_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_20)(tessera_session, uint64_t,
                                                      tessera_buffer_v1,
                                                      uint64_t *, uint64_t *,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_events_read_v1,
                   abi_function_20: 1,
                   default: 0),
               "tessera_events_read_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_21)(tessera_session,
                                                      tessera_buffer_v1,
                                                      uint64_t *,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_losses_query_v1,
                   abi_function_21: 1,
                   default: 0),
               "tessera_losses_query_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_22)(tessera_session,
                                                      tessera_buffer_v1,
                                                      uint64_t *,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_state_export_canonical_v1,
                   abi_function_22: 1,
                   default: 0),
               "tessera_state_export_canonical_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_23)(tessera_session,
                                                      tessera_buffer_v1,
                                                      uint64_t *,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_checkpoint_export_v1,
                   abi_function_23: 1,
                   default: 0),
               "tessera_checkpoint_export_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_24)(tessera_session,
                                                      tessera_bytes_v1,
                                                      tessera_bytes_v1,
                                                      tessera_context_v1 *);
_Static_assert(_Generic(&tessera_checkpoint_restore_v1,
                   abi_function_24: 1,
                   default: 0),
               "tessera_checkpoint_restore_v1 signature");
typedef tessera_status(TESSERA_CALL *abi_function_25)(
    const tessera_context_v1 *, tessera_buffer_v1, uint64_t *);
_Static_assert(_Generic(&tessera_status_format_v1,
                   abi_function_25: 1,
                   default: 0),
               "tessera_status_format_v1 signature");

/* These wrappers exercise C compilation, by-value buffers and caller ownership.
   They do not claim a DLL allocator boundary or retained-session ownership. */
tessera_status TESSERA_CALL tessera_test_c_query(tessera_api_info_v1 *out,
                                                 tessera_context_v1 *ctx) {
  return tessera_api_query_v1(TESSERA_ABI_MAJOR, TESSERA_ABI_MINOR, out, ctx);
}
tessera_status TESSERA_CALL
tessera_test_c_enumerate(tessera_backend_info_v1 *out, uint32_t capacity,
                         uint32_t *required, tessera_context_v1 *ctx) {
  return tessera_backends_enumerate_v1(out, capacity, required, ctx);
}
tessera_status TESSERA_CALL tessera_test_c_format(const tessera_context_v1 *ctx,
                                                  tessera_buffer_v1 out,
                                                  uint64_t *required) {
  return tessera_status_format_v1(ctx, out, required);
}
