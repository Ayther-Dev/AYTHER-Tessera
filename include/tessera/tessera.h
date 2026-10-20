#ifndef TESSERA_TESSERA_H
#define TESSERA_TESSERA_H

#include <stdint.h>

/* The initial supported binary platform is Windows x64, little endian, MSVC
   ABI. All layouts use natural alignment. Never apply packing around this
   header. */
#if defined(_MSC_VER)
#define TESSERA_CALL __cdecl
#else
#define TESSERA_CALL
#endif

typedef int32_t tessera_status;
typedef uint64_t tessera_session;
typedef uint64_t tessera_snapshot;

/* Numeric constants do not introduce enum storage into the C ABI. */
#define TESSERA_ABI_MAJOR UINT16_C(1)
#define TESSERA_ABI_MINOR UINT16_C(0)
#define TESSERA_OK INT32_C(0)
#define TESSERA_END_OF_TRACE INT32_C(1)
#define TESSERA_INVALID_ARGUMENT (-INT32_C(1))
#define TESSERA_ABI_INCOMPATIBLE (-INT32_C(2))
#define TESSERA_BUFFER_TOO_SMALL (-INT32_C(3))
#define TESSERA_INVALID_HANDLE (-INT32_C(4))
#define TESSERA_WRONG_STATE (-INT32_C(5))
#define TESSERA_UNSUPPORTED_CAPABILITY (-INT32_C(6))
#define TESSERA_UNKNOWN_CONTENT (-INT32_C(7))
#define TESSERA_AMBIGUOUS_CONTENT (-INT32_C(8))
#define TESSERA_INVALID_CONTENT (-INT32_C(9))
#define TESSERA_IMPLEMENTATION_UNAVAILABLE (-INT32_C(10))
#define TESSERA_STALE_EPOCH (-INT32_C(11))
#define TESSERA_INPUT_LATE (-INT32_C(12))
#define TESSERA_INPUT_OUT_OF_ORDER (-INT32_C(13))
#define TESSERA_INPUT_DUPLICATE (-INT32_C(14))
#define TESSERA_INPUT_INCOMPLETE (-INT32_C(15))
#define TESSERA_INPUT_CONTRADICTORY (-INT32_C(16))
#define TESSERA_INVALID_TRACE (-INT32_C(17))
#define TESSERA_BUDGET_EXCEEDED (-INT32_C(18))
#define TESSERA_CONTROL_PENDING (-INT32_C(19))
#define TESSERA_CHECKPOINT_INCOMPATIBLE (-INT32_C(20))
#define TESSERA_CHECKPOINT_INVALID (-INT32_C(21))
#define TESSERA_IO_NO_SPACE (-INT32_C(22))
#define TESSERA_IO_ACCESS_DENIED (-INT32_C(23))
#define TESSERA_IO_PUBLISH_FAILED (-INT32_C(24))
#define TESSERA_IO_OTHER (-INT32_C(25))
#define TESSERA_OUT_OF_MEMORY (-INT32_C(26))
#define TESSERA_BACKEND_FAILURE (-INT32_C(27))
#define TESSERA_PHASE_NONE UINT32_C(0)
#define TESSERA_PHASE_VALIDATE UINT32_C(1)
#define TESSERA_PHASE_DETECT UINT32_C(2)
#define TESSERA_PHASE_CREATE UINT32_C(3)
#define TESSERA_PHASE_STEP UINT32_C(4)
#define TESSERA_PHASE_RESET UINT32_C(5)
#define TESSERA_PHASE_READ UINT32_C(6)
#define TESSERA_PHASE_WRITE UINT32_C(7)
#define TESSERA_PHASE_PUBLISH UINT32_C(8)
#define TESSERA_PHASE_RESTORE UINT32_C(9)
#define TESSERA_PHASE_CLOSE UINT32_C(10)
#define TESSERA_OS_NONE UINT32_C(0)
#define TESSERA_OS_WIN32 UINT32_C(1)
#define TESSERA_CANCEL_NONE UINT32_C(0)
#define TESSERA_CANCEL_RESET UINT32_C(1)
#define TESSERA_CANCEL_RESTORE UINT32_C(2)
#define TESSERA_CANCEL_CLOSE UINT32_C(3)
#define TESSERA_CONTEXT_CONTROLS_APPLIED UINT32_C(1)
#define TESSERA_CONTEXT_CONTROL_CANCELLED UINT32_C(2)
#define TESSERA_SESSION_PAUSED UINT32_C(1)
#define TESSERA_SESSION_RUNNING UINT32_C(2)
#define TESSERA_SESSION_CLOSED UINT32_C(3)
#define TESSERA_SYSTEM_NONE UINT32_C(0)
#define TESSERA_SYSTEM_MEGA_DRIVE UINT32_C(1)
#define TESSERA_BACKEND_PRODUCT UINT32_C(1)
#define TESSERA_BACKEND_TEST UINT32_C(2)
#define TESSERA_DETECTION_RECOGNIZED UINT32_C(1)
#define TESSERA_DETECTION_UNKNOWN UINT32_C(2)
#define TESSERA_DETECTION_AMBIGUOUS UINT32_C(3)
#define TESSERA_DETECTION_INVALID UINT32_C(4)
#define TESSERA_CAP_FRAME UINT64_C(1)
#define TESSERA_CAP_INPUT UINT64_C(2)
#define TESSERA_CAP_VIDEO UINT64_C(4)
#define TESSERA_CAP_PCM UINT64_C(8)
#define TESSERA_CAP_MEMORY UINT64_C(16)
#define TESSERA_CAP_PALETTE UINT64_C(32)
#define TESSERA_CAP_SPRITES UINT64_C(64)
#define TESSERA_CAP_GEOMETRY UINT64_C(128)
#define TESSERA_CAP_EVENTS UINT64_C(256)
#define TESSERA_CAP_CHECKPOINT UINT64_C(512)
#define TESSERA_CAP_SAFE_CONTROLS UINT64_C(1024)
#define TESSERA_CAP_TEMPORAL_OBSERVATION UINT64_C(2048)
#define TESSERA_CAP_CANONICAL_STATE UINT64_C(4096)
#define TESSERA_REGION_MEMORY UINT64_C(1)
#define TESSERA_REGION_PALETTE UINT64_C(2)
#define TESSERA_REGION_SPRITES UINT64_C(4)
#define TESSERA_REGION_GEOMETRY UINT64_C(8)
#define TESSERA_REGION_TEMPORAL UINT64_C(16)
#define TESSERA_EVENT_LIFECYCLE UINT64_C(1)
#define TESSERA_EVENT_CONTROLS UINT64_C(2)
#define TESSERA_EVENT_BACKEND UINT64_C(4)
#define TESSERA_EVENT_CAPTURES UINT64_C(8)
#define TESSERA_EVENT_LOSSES UINT64_C(16)
#define TESSERA_CONTROL_OBSERVATION_REGION_MASK UINT32_C(1)
#define TESSERA_CONTROL_OBSERVATION_EVENT_MASK UINT32_C(2)
#define TESSERA_VALUE_MASK64 UINT32_C(1)
#define TESSERA_VIDEO_RGBA8 UINT32_C(1)
#define TESSERA_ENCODING_U32 UINT32_C(1)
#define TESSERA_ENCODING_I32 UINT32_C(2)
#define TESSERA_ENCODING_U64 UINT32_C(3)
#define TESSERA_ENCODING_I64 UINT32_C(4)
#define TESSERA_ENCODING_BYTES UINT32_C(5)
#define TESSERA_ENCODING_UTF8 UINT32_C(6)
#define TESSERA_ENCODING_BOOL8 UINT32_C(7)
#define TESSERA_ENCODING_ARRAY UINT32_C(8)
#define TESSERA_ENCODING_F32 UINT32_C(9)
#define TESSERA_ENCODING_F64 UINT32_C(10)
#define TESSERA_ENCODING_RECORD UINT32_C(11)

/* ABI 1.0: 8 bytes, natural alignment 4; no packing. */
typedef struct tessera_header_v1 {
  uint32_t size;
  uint16_t major;
  uint16_t minor;
} tessera_header_v1;

/* ABI 1.0: 16 bytes, natural alignment 8; no packing. */
typedef struct tessera_bytes_v1 {
  const uint8_t *data;
  uint64_t bytes;
} tessera_bytes_v1;

/* ABI 1.0: 16 bytes, natural alignment 8; no packing. */
typedef struct tessera_buffer_v1 {
  uint8_t *data;
  uint64_t capacity;
} tessera_buffer_v1;

/* ABI 1.0: 24 bytes, natural alignment 8; no packing. */
typedef struct tessera_frame_id_v1 {
  uint64_t session;
  uint64_t epoch;
  uint64_t frame;
} tessera_frame_id_v1;

/* ABI 1.0: 64 bytes, natural alignment 8; no packing. */
typedef struct tessera_context_v1 {
  tessera_header_v1 header;
  tessera_status code;
  uint32_t phase;
  uint32_t os_domain;
  uint32_t os_code;
  tessera_frame_id_v1 frame_id;
  uint64_t transaction;
  uint32_t cancel_reason;
  uint32_t flags;
} tessera_context_v1;

/* ABI 1.0: 32 bytes, natural alignment 8; no packing. */
typedef struct tessera_api_info_v1 {
  tessera_header_v1 header;
  uint64_t capabilities;
  uint16_t major;
  uint16_t minor;
  uint32_t pointer_bytes;
  uint32_t max_alignment;
  uint32_t reserved;
} tessera_api_info_v1;

/* ABI 1.0: 96 bytes, natural alignment 8; no packing. */
typedef struct tessera_limits_v1 {
  tessera_header_v1 header;
  uint64_t content_bytes;
  uint64_t checkpoint_bytes;
  uint64_t retained_bytes;
  uint64_t trace_bytes;
  uint32_t video_width;
  uint32_t video_height;
  uint32_t pcm_frames;
  uint32_t event_count;
  uint32_t event_payload_bytes;
  uint32_t input_count;
  uint32_t control_count;
  uint32_t retained_count;
  uint32_t trace_frames;
  uint32_t ports;
  uint32_t controls_per_port;
  uint32_t reserved;
  uint64_t snapshot_bytes;
} tessera_limits_v1;

/* ABI 1.0: 152 bytes, natural alignment 8; no packing. */
typedef struct tessera_backend_info_v1 {
  tessera_header_v1 header;
  uint32_t kind;
  uint32_t system;
  uint64_t capabilities;
  char id[64];
  char version[32];
  uint8_t build_id[32];
} tessera_backend_info_v1;

/* ABI 1.0: 80 bytes, natural alignment 8; no packing. */
typedef struct tessera_detection_v1 {
  tessera_header_v1 header;
  uint32_t outcome;
  uint32_t system;
  uint64_t evidence;
  uint64_t evidence_offset;
  uint64_t evidence_bytes;
  uint8_t content_sha256[32];
  uint32_t candidate_count;
  uint32_t reserved;
} tessera_detection_v1;

/* ABI 1.0: 16 bytes, natural alignment 8; no packing. */
typedef struct tessera_candidate_v1 {
  uint32_t system;
  uint32_t evidence_class;
  uint64_t evidence;
} tessera_candidate_v1;

/* ABI 1.0: 80 bytes, natural alignment 8; no packing. */
typedef struct tessera_session_create_info_v1 {
  tessera_header_v1 header;
  tessera_bytes_v1 backend_id;
  tessera_bytes_v1 content;
  tessera_bytes_v1 config;
  tessera_bytes_v1 initial_persistent_memory;
  uint32_t system_override;
  uint32_t flags;
} tessera_session_create_info_v1;

/* ABI 1.0: 160 bytes, natural alignment 8; no packing. */
typedef struct tessera_session_info_v1 {
  tessera_header_v1 header;
  tessera_frame_id_v1 frame_id;
  uint64_t generation;
  uint64_t logical_tick;
  uint64_t tick_hz_num;
  uint64_t tick_hz_den;
  uint64_t capabilities;
  uint32_t state;
  uint32_t system;
  uint8_t content_sha256[32];
  uint64_t pending_transaction;
  uint64_t next_replay_frame;
  uint8_t backend_build_id[32];
} tessera_session_info_v1;

/* ABI 1.0: 8 bytes, natural alignment 4; no packing. */
typedef struct tessera_input_assignment_v1 {
  uint16_t port;
  uint16_t control;
  int32_t value;
} tessera_input_assignment_v1;

/* ABI 1.0: 40 bytes, natural alignment 8; no packing. */
typedef struct tessera_input_frame_v1 {
  tessera_header_v1 header;
  uint64_t epoch;
  uint64_t frame;
  const tessera_input_assignment_v1 *assignments;
  uint32_t count;
  uint32_t reserved;
} tessera_input_frame_v1;

/* ABI 1.0: 16 bytes, natural alignment 8; no packing. */
typedef struct tessera_control_item_v1 {
  uint32_t id;
  uint32_t value_type;
  uint64_t value;
} tessera_control_item_v1;

/* ABI 1.0: 48 bytes, natural alignment 8; no packing. */
typedef struct tessera_control_transaction_v1 {
  tessera_header_v1 header;
  uint64_t epoch;
  uint64_t frame;
  uint64_t id;
  const tessera_control_item_v1 *items;
  uint32_t count;
  uint32_t reserved;
} tessera_control_transaction_v1;

/* ABI 1.0: 120 bytes, natural alignment 8; no packing. */
typedef struct tessera_original_frame_v1 {
  tessera_header_v1 header;
  tessera_frame_id_v1 frame_id;
  uint32_t width;
  uint32_t height;
  uint32_t row_bytes;
  uint32_t video_format;
  uint32_t aspect_num;
  uint32_t aspect_den;
  tessera_bytes_v1 pixels;
  tessera_bytes_v1 pcm;
  uint32_t sample_rate;
  uint32_t channels;
  uint64_t pcm_frames;
  uint64_t tick_begin;
  uint64_t tick_end;
} tessera_original_frame_v1;

/* ABI 1.0: 80 bytes, natural alignment 8; no packing. */
typedef struct tessera_snapshot_info_v1 {
  tessera_header_v1 header;
  tessera_frame_id_v1 frame_id;
  uint64_t generation;
  uint64_t region_mask;
  uint64_t complete_mask;
  uint64_t retained_bytes;
  uint64_t logical_tick;
  uint32_t region_count;
  uint32_t reserved;
} tessera_snapshot_info_v1;

/* ABI 1.0: 64 bytes, natural alignment 8; no packing. */
typedef struct tessera_snapshot_region_v1 {
  tessera_header_v1 header;
  uint32_t kind;
  uint32_t encoding;
  uint64_t address;
  uint64_t byte_count;
  uint32_t element_bytes;
  uint32_t element_count;
  tessera_bytes_v1 data;
  uint32_t bank;
  uint32_t flags;
} tessera_snapshot_region_v1;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize every extensible header with its actual size and ABI 1.0; zero all
   reserved fields. Compatible future tails are neither read nor overwritten.
   ctx is required except in status_format, which formats a const context.
   No C++ exception crosses these declarations. */
tessera_status TESSERA_CALL tessera_api_query_v1(uint16_t major, uint16_t minor,
                                                 tessera_api_info_v1 *out,
                                                 tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_limits_query_v1(tessera_session session,
                                                    tessera_limits_v1 *out,
                                                    tessera_context_v1 *ctx);
tessera_status TESSERA_CALL
tessera_backends_enumerate_v1(tessera_backend_info_v1 *out, uint32_t capacity,
                              uint32_t *required, tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_detect_v1(
    tessera_bytes_v1 content, uint32_t system_override,
    tessera_detection_v1 *out, tessera_candidate_v1 *candidates,
    uint32_t capacity, uint32_t *required, tessera_context_v1 *ctx);

/* Creation copies caller inputs; it never takes ownership of caller buffers.
   The owner serializes session operations. Zero is never a valid handle.
   Each successful step completes exactly the next frame and leaves pause.
   Reset returns to the recorded initial state in a new epoch. */
tessera_status TESSERA_CALL
tessera_session_create_v1(const tessera_session_create_info_v1 *in,
                          tessera_session *out, tessera_context_v1 *ctx);
tessera_status TESSERA_CALL
tessera_session_query_v1(tessera_session session, tessera_session_info_v1 *out,
                         tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_session_pause_v1(tessera_session session,
                                                     tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_session_step_v1(
    tessera_session session, const tessera_input_frame_v1 *in,
    tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_session_reset_v1(tessera_session session,
                                                     tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_session_close_v1(tessera_session session,
                                                     tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_trace_validate_v1(tessera_bytes_v1 trace,
                                                      tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_session_bind_trace_v1(
    tessera_session session, tessera_bytes_v1 trace, tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_session_replay_next_v1(
    tessera_session session, tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_controls_submit_v1(
    tessera_session session, const tessera_control_transaction_v1 *in,
    tessera_context_v1 *ctx);

/* Original views expire at the next successful step/reset/restore/close.
   Retained snapshot references keep historical bytes until the last release,
   even after the session closes. Only snapshot read/retain/release operations
   may run on independent valid references from other threads. */
tessera_status TESSERA_CALL tessera_original_frame_query_v1(
    tessera_session session, tessera_original_frame_v1 *out,
    tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_snapshot_acquire_v1(
    tessera_session session, tessera_snapshot *out, tessera_context_v1 *ctx);
tessera_status TESSERA_CALL
tessera_snapshot_retain_v1(tessera_snapshot snapshot, tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_snapshot_query_v1(
    tessera_snapshot snapshot, tessera_snapshot_info_v1 *out,
    tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_snapshot_region_query_v1(
    tessera_snapshot snapshot, uint32_t index, tessera_snapshot_region_v1 *out,
    tessera_context_v1 *ctx);
tessera_status TESSERA_CALL
tessera_snapshot_release_v1(tessera_snapshot snapshot, tessera_context_v1 *ctx);

/* Output buffers belong to the caller. NULL/zero queries the required length;
   BUFFER_TOO_SMALL writes no payload and advances no event cursor. required
   counts payload bytes without an implicit NUL terminator. Inputs and outputs
   must not overlap. Export does not apply or cancel pending controls.
   Restore validates a candidate before replacing the session and opening an
   epoch; failed restore preserves the session and its pending transaction. */
tessera_status TESSERA_CALL tessera_events_read_v1(
    tessera_session session, uint64_t cursor, tessera_buffer_v1 out,
    uint64_t *required, uint64_t *next_cursor, tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_losses_query_v1(tessera_session session,
                                                    tessera_buffer_v1 out,
                                                    uint64_t *required,
                                                    tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_state_export_canonical_v1(
    tessera_session session, tessera_buffer_v1 out, uint64_t *required,
    tessera_context_v1 *ctx);
tessera_status TESSERA_CALL
tessera_checkpoint_export_v1(tessera_session session, tessera_buffer_v1 out,
                             uint64_t *required, tessera_context_v1 *ctx);
tessera_status TESSERA_CALL tessera_checkpoint_restore_v1(
    tessera_session session, tessera_bytes_v1 checkpoint,
    tessera_bytes_v1 trace, tessera_context_v1 *ctx);
tessera_status TESSERA_CALL
tessera_status_format_v1(const tessera_context_v1 *context,
                         tessera_buffer_v1 out, uint64_t *required);

#ifdef __cplusplus
}
#endif

#endif /* TESSERA_TESSERA_H */
