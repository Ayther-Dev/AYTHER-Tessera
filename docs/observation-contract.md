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

## Unidad del campo `cycle` de AUDIO_WRITES (TP-011.2)
El campo `cycle` de cada registro de `AUDIO_WRITES` está en ciclos master (53 693 175 Hz NTSC, 53 203 424 Hz PAL) contados desde el inicio del frame. Medido en gpgx r4 por caja negra (evidencia: `specs/008-AYTHER-tessera-parity/evidence/g0/cycle-base.json`). La secuencia (chip, addr, data) debe coincidir siempre; el ciclo de las escrituras de la 68000 se compara con tolerancia de 200 ciclos master, el de las del Z80 de forma exacta salvo el desfase de arranque.

## Sensibilidades de timing que fijan una identidad (TP-011.3, RF-10.5)

Lo que sigue es lo que, si Tessera lo hiciera distinto de gpgx, cambiaría un identificador o la cobertura y dejaría de ser portable un pack (DA-7). Cada fila cita la región de observación que lo fija; el comportamiento exacto se averigua por caja negra con el banco diferencial, nunca leyendo código de gpgx.

| Clase | Sensibilidad | Región que la fija | Estado en Tessera |
|---|---|---|---|
| sprite, pose | Orden de recorrido de la SAT (cadena de enlaces, 80 ranuras): el orden de captura entra en `pose_key` y no se ordena | 13 y 14 (PARSED_SPRITES y su cuenta) | ROM sintética `sprite_limits` en paridad de framebuffer; la región PARSED_SPRITES se sirve en la fase de observación |
| sprite | Límites por línea (sprites y píxeles por línea según H32/H40) y qué ranura queda fuera | 25 (SPRITE_OUTCOME), 21 (LINE_CELLS) | `sprite_limits` y `modes_sweep` en paridad de framebuffer; el resultado por ranura queda para la fase de observación |
| animation group | Frames en que cada hash aparece en cada ranura (ventana de 64 frames): depende del orden y del frame en que se atribuye cada sprite | 13, 14 | igual que la fila de la SAT |
| plane_tile, panorama | Bandas raster: líneas en que cambian los registros del VDP o la CRAM dentro del frame (HINT, escrituras a media pantalla) y razones por las que cae a la vía de reserva | 22 (RASTER_JOURNAL), 16 (RASTER_FALLBACK_REASONS), 19 (LINE_REGS) | `hint_counter` y `modes_sweep` en paridad; el diario raster se sirve en la fase de observación |
| palette_variant | La línea de paleta debe quedar estable 30 frames antes de firmarse: un cambio de CRAM en otra línea o fuera de orden desplaza la firma | 2 (CRAM), 24 (PALETTE), 20 (LINE_CRAM) | CRAM idéntica byte a byte en el corpus de framebuffer |
| audio_event fm | Orden y segmentación de las escrituras (registro de dirección, key-on/key-off por canal): la firma no usa el ciclo, sí la secuencia | 11 y 12 (AUDIO_WRITES y su cuenta) | secuencia (chip, addr, data) idéntica donde los frames coinciden; ver abajo la divergencia de ciclo |
| audio_event fm, psg | Frame al que cae cada escritura: el cierre del evento es `max(frame-1, inicio)`, así que una escritura que cruza el límite de frame cambia la segmentación | 11, 12 (campo `cycle`, unidad en la sección anterior) | divergencia conocida en el corpus (Sonic 1, frames 540 y 600: una escritura en el frame vecino); TP-107.7 sigue abierta |
| audio_event dac | Bloque de apertura de `0x2A` y frames de silencio que lo cierran | 11, 12 | pendiente de corpus con DAC |

Dos consecuencias para el trabajo de paridad:

- La divergencia de ciclo en AUDIO_WRITES no es cosmética: solo es inocua mientras la escritura caiga en el mismo frame que en gpgx. Por eso TP-107.7 exige cero divergencias sin declarar y no acepta una tolerancia general.
- Ninguna de estas diferencias puede declararse como excepción D-3 (RF-10.5); si aparece una, se corrige el núcleo o se sube la versión del contrato de identidad.
