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
| 24 | PALETTE | 512 | `VDP_MEMORY` | `OBSERVABILITY_V1` | 1 |
| 25 | SPRITE_OUTCOME | 80 | `SPRITE_CAPTURE` | `SPRITE_OUTCOME_V1` | 1 |
| 26 | Z80_RAM | 8192 | siempre | `REGION_READ` | 1 |

Los tamaños se midieron por caja negra sobre gpgx r4 (`src/observation/region_table.hpp`). La columna de suscripciones y capacidades sigue las de la ABI y se revisa contra gpgx en TP-101.

## Semántica de las suscripciones (TP-201.3, medida sobre gpgx r4)

Medida por caja negra con `tools/diffbench/measure_subscriptions.py`; la matriz completa está en `tests/fixtures/diffbench/subscription-matrix.txt` y la comprueba `integration.md_subscriptions`.

- Al arrancar `supported = 0xFFF`, `active = requested = 0`. `set` con un bit desconocido devuelve `INVALID_ARGUMENT` y no cambia nada.
- La máscara pedida pasa a activa al empezar el siguiente frame. `activation_frame` es el número (base 1) del último frame empezado; un cambio pendiente lo fija en el frame siguiente; pedir 0 estando en 0 lo deja como está.
- Siempre legibles: LAYER_MASK, regiones 5-10, 15, 18 (SYSTEM) y 26 (Z80_RAM). PALETTE (24) cuelga de `VDP_MEMORY`.
- Puertas: VDP_MEMORY 1-4, 24; SPRITE_CAPTURE 13, 14, 25; RASTER_TRACKING 16, 22; AUDIO_WRITES 11, 12; ATTRIBUTION 17; LINE_STATE 19; LINE_CRAM 20; LINE_CELLS 21; FRAME_HASH 23. Región inactiva: `TESSERA_NOT_SUBSCRIBED` (-28; gpgx devuelve -9). Región suscrita que Tessera aún no sirve: `UNSUPPORTED_CAPABILITY`.
- El poll de audio events exige AUDIO_EVENTS activa (si no, `NOT_SUBSCRIBED`); las estadísticas del transporte responden siempre.
- Con una lectura en curso en otro hilo, las llamadas de observación devuelven `TESSERA_BUSY` (-29) mientras la sesión avanza.
- Contador de trabajo (`observation_work`, solo para pruebas y TP-208): bytes que la observación entregó. Sin suscripciones activas y sin lecturas queda en 0.
- Sin medir: momento exacto de la desactivación a mitad de partida y `activation_frame` tras cargar un savestate.

## Sprites analizados y su resultado (TP-202, medido sobre gpgx r4)

Regiones 13 (PARSED_SPRITES), 14 (cuenta) y 25 (SPRITE_OUTCOME), activas con `SPRITE_CAPTURE`. Modelo ajustado por caja negra sobre las 109 escenas de `sprite_limits` (byte a byte) y comprobado en el arranque de los juegos del corpus hasta el frame 600:

- Por cada línea activa se recorre la cadena de enlaces (64 pasos en H32, 80 en H40). Un sprite que cruza la línea entra en la lista de esa línea si caben menos de 16 (H32) o 20 (H40); si no cabe, su resultado recibe el bit 4 y no se captura por eso.
- PARSED_SPRITES lista cada sprite una sola vez por frame, en el orden de la primera línea en que entra en una lista y, dentro de la línea, el de la cadena. Entrada de 10 bytes: `yr` (campo Y de la SAT, 10 bits), `xr` (9 bits), `attr`, `w` y `h` en celdas, `sat_idx`, `chain_pos` (paso de la cadena). La cuenta es un byte. Pasada la cuenta, las ranuras conservan lo que dejaron frames anteriores.
- SPRITE_OUTCOME, un byte por posición de la SAT, acumulado en el frame: 1 entró en una lista; 2 dibujó alguna columna visible; 4 quedó fuera por el límite de la línea; 8 el presupuesto de píxeles (256 o 320) se agotó con él o antes; 16 enmascarado por un sprite con X = 0 anterior. Nunca aparece el bit de supresión (PQ-1).
- Modo 4 no está cubierto: gpgx lo analiza de otra forma y esas escenas no entran en la comparación de estas regiones.
- Fuera de la suscripción no se analiza nada. Los datos no van en el savestate.

## Estado por línea y diario raster (TP-203.1, TP-204.1, TP-204.3, medido sobre gpgx r4)

Regiones 16 (RASTER_FALLBACK_REASONS), 19 (LINE_REGS), 20 (LINE_CRAM) y 22 (RASTER_JOURNAL), con `RASTER_TRACKING`, `LINE_STATE` y `LINE_CRAM`. Ajustado por caja negra con `raster_bands` 0–33 (escrituras desde la HINT cada 8 líneas o cada línea), byte a byte:

- LINE_REGS y LINE_CRAM: cabecera de 24 bytes (tamaño, tamaño de entrada, `count`, `flags`, frame) y una entrada por línea visible. LINE_CRAM se reduce a una entrada con `flags` 1 si ninguna escritura cambió la CRAM en una línea visible. En entrelazado modo 2 y con scroll vertical de 2 celdas solo hay cabecera (`count` 0). El tamaño varía y la consulta de la región informa el vigente.
- Los campos de LINE_REGS son los del comienzo de la línea, salvo las bases de los planos y de la ventana (vigentes al dibujar). El recorte de ventana se fija al escribir el registro 17 con columnas distintas de 0 y se conserva. Una línea con la pantalla apagada al comenzar y al dibujar conserva la entrada anterior: las tablas viven entre frames.
- La altura del frame (224/240) se fija al comienzo del frame (primera línea del blanking) y rige la imagen, el contador de HINT y el diario.
- RASTER_JOURNAL: hasta 256 eventos `(v_counter, razón, dirección, dato)` de escrituras que cambian un valor y se ven por primera vez en una línea visible. Razón 1 registro (no el 10, el 15 ni los de DMA), 2 CRAM (dirección = índice de entrada), 4 VSRAM, 8 tabla de scroll H. El resto de la VRAM pone 0x40 en RASTER_FALLBACK_REASONS, el cambio de ancho a media pantalla o el entrelazado modo 2 ponen 0x20, el diario lleno 0x80 y suma `dropped`.
- Modo 4 no tiene estado por línea. Nada de esto va en el savestate.

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
