// Constantes del contrato de observación ABI 1.10 (spec 008, RF-3.0).
// GENERADO por tools/gen_observation_header.py desde ayther-abi-1.10-r4; no editar a mano.
#ifndef TESSERA_OBSERVATION_H
#define TESSERA_OBSERVATION_H

#include <stdint.h>

#define TESSERA_OBS_ABI_VERSION_1_0 UINT32_C(0x00010000)
#define TESSERA_OBS_ABI_VERSION_1_1 UINT32_C(0x00010001)
#define TESSERA_OBS_ABI_VERSION_1_2 UINT32_C(0x00010002)
#define TESSERA_OBS_ABI_VERSION_1_3 UINT32_C(0x00010003)
#define TESSERA_OBS_ABI_VERSION_1_4 UINT32_C(0x00010004)
#define TESSERA_OBS_ABI_VERSION_1_5 UINT32_C(0x00010005)
#define TESSERA_OBS_ABI_VERSION_1_6 UINT32_C(0x00010006)
#define TESSERA_OBS_ABI_VERSION_1_7 UINT32_C(0x00010007)
#define TESSERA_OBS_ABI_VERSION_1_8 UINT32_C(0x00010008)
#define TESSERA_OBS_ABI_VERSION_1_9 UINT32_C(0x00010009)
#define TESSERA_OBS_ABI_VERSION_1_10 UINT32_C(0x0001000A)
#define TESSERA_OBS_ABI_VERSION_LATEST TESSERA_OBS_ABI_VERSION_1_10
#define TESSERA_OBS_CAP_LEGACY_MEMORY (UINT64_C(1) << 0)
#define TESSERA_OBS_CAP_REGION_QUERY (UINT64_C(1) << 1)
#define TESSERA_OBS_CAP_REGION_READ (UINT64_C(1) << 2)
#define TESSERA_OBS_CAP_CONTROL_WRITE (UINT64_C(1) << 3)
#define TESSERA_OBS_CAP_FRAME_SNAPSHOT (UINT64_C(1) << 4)
#define TESSERA_OBS_CAP_PARSED_SPRITES_V1 (UINT64_C(1) << 5)
#define TESSERA_OBS_CAP_AUDIO_WRITES_V1 (UINT64_C(1) << 6)
#define TESSERA_OBS_CAP_RASTER_FALLBACK_V1 (UINT64_C(1) << 7)
#define TESSERA_OBS_CAP_RECOMPOSE_V1 (UINT64_C(1) << 8)
#define TESSERA_OBS_CAP_AUDIO_PROBE_V1 (UINT64_C(1) << 9)
#define TESSERA_OBS_CAP_SUBSCRIPTIONS_V1 (UINT64_C(1) << 10)
#define TESSERA_OBS_CAP_FRAME_DELTA_V1 (UINT64_C(1) << 11)
#define TESSERA_OBS_CAP_RECOMPOSE_STATS_V1 (UINT64_C(1) << 12)
#define TESSERA_OBS_CAP_ATTRIBUTION_V1 (UINT64_C(1) << 13)
#define TESSERA_OBS_CAP_FRAME_DELTA_SINCE_V1 (UINT64_C(1) << 14)
#define TESSERA_OBS_CAP_SYSTEM_V1 (UINT64_C(1) << 15)
#define TESSERA_OBS_CAP_MODE4_CONTROLS (UINT64_C(1) << 16)
#define TESSERA_OBS_CAP_LINE_STATE_V1 (UINT64_C(1) << 17)
#define TESSERA_OBS_CAP_OBSERVABILITY_V1 (UINT64_C(1) << 18)
#define TESSERA_OBS_CAP_SPRITE_OUTCOME_V1 (UINT64_C(1) << 19)
#define TESSERA_OBS_SUB_VDP_MEMORY (UINT32_C(1) << 0)
#define TESSERA_OBS_SUB_SPRITE_CAPTURE (UINT32_C(1) << 1)
#define TESSERA_OBS_SUB_RENDER_CONTROLS (UINT32_C(1) << 2)
#define TESSERA_OBS_SUB_RASTER_TRACKING (UINT32_C(1) << 3)
#define TESSERA_OBS_SUB_AUDIO_WRITES (UINT32_C(1) << 4)
#define TESSERA_OBS_SUB_RECOMPOSITION (UINT32_C(1) << 5)
#define TESSERA_OBS_SUB_AUDIO_EVENTS (UINT32_C(1) << 6)
#define TESSERA_OBS_SUB_ATTRIBUTION (UINT32_C(1) << 7)
#define TESSERA_OBS_SUB_LINE_STATE (UINT32_C(1) << 8)
#define TESSERA_OBS_SUB_LINE_CRAM (UINT32_C(1) << 9)
#define TESSERA_OBS_SUB_LINE_CELLS (UINT32_C(1) << 10)
#define TESSERA_OBS_SUB_FRAME_HASH (UINT32_C(1) << 11)
#define TESSERA_OBS_SUB_ALL UINT32_C(0xFFF)
#define TESSERA_OBS_REGION_ACCESS_READ (UINT32_C(1) << 0)
#define TESSERA_OBS_REGION_ACCESS_CONTROL_WRITE (UINT32_C(1) << 1)
#define TESSERA_OBS_REGION_WORD_SWAPPED_LE (UINT32_C(1) << 5)
#define TESSERA_OBS_LAYOUT_RAW_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_SPRITE_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_AUDIO_WRITE_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_AUDIO_EVENT_V1 UINT32_C(2)
#define TESSERA_OBS_LAYOUT_FRAME_DELTA_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_ATTRIBUTION_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_SYSTEM_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_LINE_REGS_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_LINE_CRAM_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_LINE_CELLS_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_JOURNAL_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_FRAME_HASH_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_PALETTE_V1 UINT32_C(1)
#define TESSERA_OBS_LAYOUT_SPR_OUTCOME_V1 UINT32_C(1)
#define TESSERA_OBS_LINE_CELL_COLUMNS 21
#define TESSERA_OBS_JOURNAL_MAX_EVENTS 256
#define TESSERA_OBS_PALETTE_ENTRIES 256
#define TESSERA_OBS_SPR_OUT_PARSED UINT8_C(0x01)
#define TESSERA_OBS_SPR_OUT_DRAWN UINT8_C(0x02)
#define TESSERA_OBS_SPR_OUT_DROP_LINE UINT8_C(0x04)
#define TESSERA_OBS_SPR_OUT_DROP_PIXEL UINT8_C(0x08)
#define TESSERA_OBS_SPR_OUT_MASKED_X0 UINT8_C(0x10)
#define TESSERA_OBS_SPR_OUT_SUPPRESSED UINT8_C(0x20)
#define TESSERA_OBS_AUDIO_TRANSPORT_CALLBACK_ACTIVE (UINT32_C(1) << 0)
#define TESSERA_OBS_AUDIO_TRANSPORT_OBSERVATION_ACTIVE (UINT32_C(1) << 1)

#define TESSERA_OBS_REGION_VRAM 1
#define TESSERA_OBS_REGION_CRAM 2
#define TESSERA_OBS_REGION_VDP_REGS 3
#define TESSERA_OBS_REGION_VSRAM 4
#define TESSERA_OBS_REGION_LAYER_MASK 5
#define TESSERA_OBS_REGION_SPRITE_SUPPRESS 6
#define TESSERA_OBS_REGION_TILE_SUPPRESS 7
#define TESSERA_OBS_REGION_PLANE_TILE_SUPPRESS 8
#define TESSERA_OBS_REGION_PLANE_SUPPRESS_ACTIVE 9
#define TESSERA_OBS_REGION_LAYER_DIM 10
#define TESSERA_OBS_REGION_AUDIO_WRITES 11
#define TESSERA_OBS_REGION_AUDIO_WRITE_COUNT 12
#define TESSERA_OBS_REGION_PARSED_SPRITES 13
#define TESSERA_OBS_REGION_PARSED_SPRITE_COUNT 14
#define TESSERA_OBS_REGION_AUDIO_MUTE 15
#define TESSERA_OBS_REGION_RASTER_FALLBACK_REASONS 16
#define TESSERA_OBS_REGION_ATTRIBUTION 17
#define TESSERA_OBS_REGION_SYSTEM 18
#define TESSERA_OBS_REGION_LINE_REGS 19
#define TESSERA_OBS_REGION_LINE_CRAM 20
#define TESSERA_OBS_REGION_LINE_CELLS 21
#define TESSERA_OBS_REGION_RASTER_JOURNAL 22
#define TESSERA_OBS_REGION_FRAME_HASH 23
#define TESSERA_OBS_REGION_PALETTE 24
#define TESSERA_OBS_REGION_SPRITE_OUTCOME 25
#define TESSERA_OBS_REGION_Z80_RAM 26
#define TESSERA_OBS_REGION_COUNT 27

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tessera_obs_line_regs_v1 {
    uint16_t xscroll_a;
    uint16_t xscroll_b;
    uint16_t yscroll_a;
    uint16_t yscroll_b;
    uint16_t ntab;
    uint16_t ntbb;
    uint16_t ntwb;
    uint16_t hscb;
    uint16_t satb;
    uint8_t reg1;
    uint8_t reg7;
    uint8_t reg11;
    uint8_t reg12;
    uint8_t reg13;
    uint8_t reg16;
    uint8_t reg17;
    uint8_t reg18;
    uint8_t clip_a_start;
    uint8_t clip_a_end;
    uint8_t clip_w_start;
    uint8_t clip_w_end;
    uint8_t flags;
    uint8_t reserved0;
} tessera_obs_line_regs_v1;

typedef struct tessera_obs_line_header_v1 {
    uint32_t struct_size;
    uint32_t entry_size;
    uint32_t lines;
    uint32_t flags;
    uint64_t frame_generation;
} tessera_obs_line_header_v1;

typedef struct tessera_obs_line_cells_v1 {
    uint32_t name_a[TESSERA_OBS_LINE_CELL_COLUMNS];
    uint32_t name_b[TESSERA_OBS_LINE_CELL_COLUMNS];
    uint32_t name_w[TESSERA_OBS_LINE_CELL_COLUMNS];
    uint8_t row_a;
    uint8_t row_b;
    uint8_t row_w;
    uint8_t shift_a;
    uint8_t shift_b;
    uint8_t cols_a;
    uint8_t cols_b;
    uint8_t cols_w;
    uint8_t reserved0;
} tessera_obs_line_cells_v1;

typedef struct tessera_obs_journal_event_v1 {
    uint16_t v_counter;
    uint16_t reason;
    uint16_t address;
    uint16_t data;
} tessera_obs_journal_event_v1;

typedef struct tessera_obs_journal_v1 {
    uint32_t layout_version;
    uint32_t struct_size;
    uint32_t count;
    uint32_t dropped;
    tessera_obs_journal_event_v1 events[TESSERA_OBS_JOURNAL_MAX_EVENTS];
} tessera_obs_journal_v1;

typedef struct tessera_obs_frame_hash_v1 {
    uint32_t layout_version;
    uint32_t struct_size;
    uint64_t frame_index;
    uint64_t video_hash;
    uint64_t audio_hash;
    uint64_t vram_hash;
    uint64_t cram_hash;
    uint64_t vsram_hash;
} tessera_obs_frame_hash_v1;

typedef struct tessera_obs_system_v1 {
    uint32_t struct_size;
    uint32_t layout_version;
    uint8_t system_hw;
    uint8_t region_pal;
    uint8_t vdp_mode;
    uint8_t interlace;
    uint8_t h40;
    uint8_t shadow_highlight;
    uint16_t lines_per_frame;
    uint16_t viewport_x;
    uint16_t viewport_y;
    uint16_t viewport_w;
    uint16_t viewport_h;
    uint32_t cpu_clock;
    uint32_t master_clock;
    uint8_t fm_core;
    uint8_t psg_present;
    uint8_t pcm_present;
    uint8_t flags;
    uint32_t rom_crc32;
    uint32_t rom_bytes;
} tessera_obs_system_v1;

typedef struct tessera_obs_sprite_v1 {
    uint16_t yr;
    uint16_t xr;
    uint16_t attr;
    uint8_t w;
    uint8_t h;
    uint8_t sat_idx;
    uint8_t chain_pos;
} tessera_obs_sprite_v1;

typedef struct tessera_obs_audio_write_v1 {
    uint32_t cycle;
    uint16_t addr;
    uint8_t data;
    uint8_t chip;
} tessera_obs_audio_write_v1;

typedef struct tessera_obs_audio_voice_v1 {
    uint8_t op_tl[4];
    uint8_t op_ar[4];
    uint8_t op_dr[4];
    uint8_t op_sr[4];
    uint8_t op_rr[4];
    uint8_t op_mul[4];
    uint8_t op_dt[4];
    uint8_t algorithm;
    uint8_t feedback;
    uint8_t ams;
    uint8_t fms;
    uint8_t pan;
    uint32_t block_fnum;
} tessera_obs_audio_voice_v1;

typedef struct tessera_obs_audio_transport_stats_v1 {
    uint32_t struct_size;
    uint32_t transport_version;
    uint32_t event_size;
    uint32_t capacity;
    uint32_t pending;
    uint32_t high_water_mark;
    uint32_t dropped_events;
    uint32_t flags;
} tessera_obs_audio_transport_stats_v1;

typedef struct tessera_obs_region_info_v1 {
    uint32_t struct_size;
    uint32_t region_id;
    uint32_t data_version;
    uint32_t element_size;
    uint32_t capacity;
    uint32_t byte_size;
    uint32_t access_flags;
    uint32_t legacy_memory_id;
} tessera_obs_region_info_v1;

typedef struct tessera_obs_frame_snapshot_v1 {
    uint32_t struct_size;
    uint32_t snapshot_version;
    uint64_t snapshot_generation;
    uint64_t frame_generation;
    uint32_t flags;
    uint32_t overflow_flags;
    uint32_t fallback_reasons;
    uint32_t parsed_sprite_count;
    uint32_t audio_write_count;
    uint32_t reserved0;
} tessera_obs_frame_snapshot_v1;

typedef struct tessera_obs_subscription_state_v1 {
    uint32_t struct_size;
    uint32_t state_version;
    uint32_t supported_mask;
    uint32_t active_mask;
    uint32_t requested_mask;
    uint32_t reserved0;
    uint64_t activation_frame;
} tessera_obs_subscription_state_v1;

typedef struct tessera_obs_frame_delta_v1 {
    uint32_t struct_size;
    uint32_t delta_version;
    uint64_t frame_generation;
    uint32_t raster_event_count;
    uint32_t parsed_sprite_count;
    uint32_t audio_write_count;
    uint32_t raster_events_dropped;
    uint8_t dirty_patterns[2048];
} tessera_obs_frame_delta_v1;

typedef struct tessera_obs_recompose_stats_v1 {
    uint32_t struct_size;
    uint32_t reserved0;
    uint64_t single_calls;
    uint64_t single_hits;
    uint64_t multilayer_calls;
    uint64_t multilayer_hits;
    uint64_t controls_fingerprint;
} tessera_obs_recompose_stats_v1;

#ifdef __cplusplus
}
#endif

#endif /* TESSERA_OBSERVATION_H */
