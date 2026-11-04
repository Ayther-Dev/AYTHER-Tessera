# Contrato de observación de Tessera

Spec 008, RF-3 (TP-006.7). Borrador pendiente de aprobación del responsable.

Tessera expone la ABI 1.10 de gpgx `ayther-abi-1.10-r4` por su API C 1.1 (`observation-api.md`). Las constantes y los layouts los genera `tools/gen_observation_header.py` y los comprueba `contract.observation_abi`.

## Regiones, suscripciones, capacidades y fase

| Id | Región | Bytes | Suscripción que la activa | Capacidad | Fase en que Tessera la sirve |
|---|---|---|---|---|---|
| 1 | VRAM | 65536 | `VDP_MEMORY` | `REGION_READ` | 1 |
| 2 | CRAM | 128 | `VDP_MEMORY` | `REGION_READ` | 1 |
| 3 | VDP_REGS | 32 | `VDP_MEMORY` | `REGION_READ` | 1 |
| 4 | VSRAM | 128 | `VDP_MEMORY` | `REGION_READ` | 1 |
| 5 | LAYER_MASK | 1 | `RENDER_CONTROLS` | `CONTROL_WRITE` | 1 |
| 6 a 9 | Supresión | 16, 512, 3072, 1 | — | — | Fuera del contrato (PQ-1) |
| 10 | LAYER_DIM | 1 | `RENDER_CONTROLS` | `CONTROL_WRITE` | 1 |
| 11, 12 | AUDIO_WRITES y su cuenta | 65536, 4 | `AUDIO_WRITES` | `AUDIO_WRITES_V1` | 1 |
| 13, 14 | PARSED_SPRITES y su cuenta | 1280, 1 | `SPRITE_CAPTURE` | `PARSED_SPRITES_V1` | 1 |
| 15 | AUDIO_MUTE | 4 | `RENDER_CONTROLS` | `CONTROL_WRITE` | 1 |
| 16 | RASTER_FALLBACK_REASONS | 4 | `RASTER_TRACKING` | `RASTER_FALLBACK_V1` | 1 |
| 17 | ATTRIBUTION | 71680 | `ATTRIBUTION` | `ATTRIBUTION_V1` | 1 |
| 18 | SYSTEM | 44 | siempre | `SYSTEM_V1` | 1 |
| 19 a 21 | LINE_REGS, LINE_CRAM, LINE_CELLS | 7192, 152, 59160 | `LINE_STATE`, `LINE_CRAM`, `LINE_CELLS` | `LINE_STATE_V1` | 1 |
| 22 | RASTER_JOURNAL | 2064 | `RASTER_TRACKING` | `OBSERVABILITY_V1` | 1 |
| 23 | FRAME_HASH | 56 | `FRAME_HASH` | `OBSERVABILITY_V1` | 1 |
| 24 | PALETTE | 512 | siempre | `OBSERVABILITY_V1` | 1 |
| 25 | SPRITE_OUTCOME | 80 | `SPRITE_CAPTURE` | `SPRITE_OUTCOME_V1` | 1 |
| 26 | Z80_RAM | 8192 | `VDP_MEMORY` | `REGION_READ` | 1 |

Los tamaños se midieron por caja negra sobre gpgx r4 (`src/observation/region_table.hpp`). La columna de suscripciones y capacidades sigue las de la ABI y se revisa contra gpgx en TP-101.
