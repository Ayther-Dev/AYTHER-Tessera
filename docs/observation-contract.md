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

## Estado por línea y diario raster (TP-203.1, TP-204.1–204.3, medido sobre gpgx r4)

Regiones 16 (RASTER_FALLBACK_REASONS), 19 (LINE_REGS), 20 (LINE_CRAM), 21 (LINE_CELLS) y 22 (RASTER_JOURNAL), con `RASTER_TRACKING`, `LINE_STATE`, `LINE_CRAM` y `LINE_CELLS`. Ajustado por caja negra con `raster_bands` 0–33 (escrituras desde la HINT cada 8 líneas o cada línea), byte a byte:

- LINE_REGS, LINE_CRAM y LINE_CELLS: cabecera de 24 bytes (tamaño, tamaño de entrada, `count`, `flags`, frame) y una entrada por línea visible. LINE_CRAM se reduce a una entrada con `flags` 1 si ninguna escritura cambió la CRAM en una línea visible. En entrelazado modo 2 y con scroll vertical de 2 celdas solo hay cabecera (`count` 0). El tamaño varía y la consulta de la región informa el vigente.
- Los campos de LINE_REGS son los del comienzo de la línea, salvo las bases de los planos y de la ventana (vigentes al dibujar). El scroll vertical de A y B conserva 10 bits (Phantasy Star IV escribe 0x738 y la región dice 0x338). El recorte de ventana se fija al escribir el registro 17 con columnas distintas de 0 y se conserva. Una línea con la pantalla apagada al comenzar y al dibujar conserva la entrada anterior: las tablas viven entre frames.
- LINE_CELLS (TP-204.2, `modes_sweep` 0–31 y `raster_bands` 0–33): por línea, 21 columnas de 2 celdas para los planos A y B y la ventana; cada `uint32` lleva la palabra de la tabla de nombres de la celda izquierda en la mitad baja y la de la derecha en la alta. Los planos empiezan en el par `((-hscroll) & 0x3FF) >> 4` de la fila de celdas `(línea + vscroll) >> 3`; `shift` = `hscroll & 15`; `row` = `((línea + vscroll) & 7) << 3`; columnas = ancho en pares + 1 si `shift` ≠ 0 (el plano B cuenta con el ancho del frame, el A con el de la línea). Una ventana horizontal descuenta sus pares del plano A; a su derecha y con `shift` ≠ 0, la primera columna de A repite la segunda. Una línea cubierta por la ventana vertical deja el plano A vacío. La ventana lista sus pares desde la fila `línea >> 3`, con `row` = `(línea & 7) << 3`. Lo que queda tras cada cuenta vale 0.
- La altura del frame (224/240) se fija al comienzo del frame (primera línea del blanking) y rige la imagen, el contador de HINT y el diario.
- RASTER_JOURNAL: hasta 256 eventos `(v_counter, razón, dirección, dato)` de escrituras que cambian un valor y se ven por primera vez en una línea visible. Razón 1 registro (no el 10, el 15 ni los de DMA), 2 CRAM (dirección = índice de entrada), 4 VSRAM, 8 tabla de scroll H. El resto de la VRAM pone 0x40 en RASTER_FALLBACK_REASONS, el cambio de ancho a media pantalla o el entrelazado modo 2 ponen 0x20, el diario lleno 0x80 y suma `dropped`. Con la pantalla apagada (registro 1, bit 6) la línea es el backdrop y solo cuenta lo que lo cambia: el registro 7, su entrada de CRAM y el propio bit de pantalla. Lo demás no es evento ni pone razón (ni 0x40 ni 0x20); medido en los frames 1–4 de las 74 ROM de prueba mientras cargan la escena (TP-205.3, que lo destapó: el diario nativo se llenaba al cargar).
- Modo 4 no tiene estado por línea. Nada de esto va en el savestate.

## Atribución por píxel (TP-204.4, medida sobre gpgx r4)

Región 17 (ATTRIBUTION), con `ATTRIBUTION`: un byte por píxel del frame dibujado (ancho × alto). Ajustada byte a byte con `raster_bands` 0–33, `modes_sweep` 0–31, `sprite_limits` y `palette_sweep`, y con el corpus.

- Bits 7-6, capa ganadora de los planos: 0 fondo, 1 B, 2 A, 3 ventana (la línea tiene ventana y la columna cae en el recorte del registro 17). Gana la celda de A con prioridad, luego B con prioridad, luego A, luego B; un píxel transparente de ambos planos es fondo.
- Bit 5, prioridad de la celda ganadora; bits 4-3, su línea de paleta (0 en el fondo).
- Bits 2-1, sombra (1) solo en píxeles de plano con sombra/brillo activo y ninguna de las dos celdas con prioridad. Tessera nunca marca brillo (2) y ningún caso medido lo presenta en gpgx.
- Bit 0, sprite: el píxel visible es de un sprite, o un sprite chocó en esa columna sin sombra/brillo activo.
- Una línea con la pantalla apagada al dibujar pero encendida al comenzar se atribuye con los planos. Las líneas no dibujadas conservan los bytes del frame anterior: la tabla solo se borra al cambiar de tamaño.
- Hasta que una línea pasa por la mezcla desde el arranque, la consulta informa 0 bytes y la lectura de 0 bytes (con o sin buffer) responde por la suscripción. Modo 4 no tiene atribución (0 bytes).
- Corpus sin entrada y con la línea de tiempo de cada juego: idéntico salvo dos excepciones aprobadas (EXC-005 y EXC-006 de `docs/parity-exceptions.md`). Phantasy Star IV en el frame 480, 3 píxeles de la línea 91 (x 242–244), A/paleta 1 en gpgx y fondo en Tessera, con imagen idéntica: la escritura de VRAM cae a mitad de línea. VDP FIFO Testing en todos los puntos: el texto de resultados sale con otra paleta porque la máquina ya diverge (framebuffer declarado en `compare_corpus.py`).

## Recomposición del frame (TP-205.1, medida sobre gpgx r4)

`tessera_observation_recompose_v1` (en gpgx, `recompose_frame`) vuelve a dibujar el último frame con el estado del VDP al terminarlo, sin cambios raster: cada línea usa los registros, la CRAM, la VSRAM, la tabla de scroll y la VRAM finales. Trabaja sobre copias y no cambia la máquina. Ajustada píxel a píxel con `raster_bands` 0–33, `modes_sweep` 0–31, `palette_sweep` 0–8, `mode4_scene` 1–11 y `sprite_limits`, y con el corpus en 5 puntos (6 ROM idénticas; VDP FIFO Testing hereda la divergencia de máquina ya declarada).

- Necesita la suscripción `RECOMPOSITION` (bit 5), activa desde el frame siguiente a pedirla; sin ella, `TESSERA_NOT_SUBSCRIBED` (gpgx -9).
- Devuelve RGB565 del tamaño del frame emitido (el ancho de la línea 0 y el alto fijado al comenzar el frame), no del que dicen los registros al final (`raster_bands` 16). Mode 4 se recompone igual.
- Los `flags` no cambian nada: ningún bit altera el estado ni los píxeles.
- Entrelazado modo 2: rechazo (gpgx `RC_INTERLACE2`, -21), en Tessera `TESSERA_WRONG_STATE`. Capacidad menor que el frame: rechazo (gpgx `RC_INVALID_PARAMS`, -23), en Tessera `TESSERA_INVALID_ARGUMENT`. Traducción decidida por el responsable (2026-11-08): Tessera conserva sus propios códigos y es el banco quien normaliza los de gpgx (`compare_recompose.py`).
- En el frame en que el ancho pasa de H40 a H32, las columnas 256–319 no coinciden con gpgx: es la misma pregunta abierta de `raster_bands` 7.

## Controles de render y recomposición multicapa (TP-205.2, medida sobre gpgx r4)

`tessera_observation_write_control_v1` escribe LAYER_MASK (región 5) y LAYER_DIM (región 10); `tessera_observation_recompose_multilayer_v1` (en gpgx, `recompose_multilayer`) devuelve cinco capas RGB565: plano A, plano B, ventana, sprites y compuesto. Ajustado píxel a píxel con `raster_bands` 0–33, `modes_sweep` 0–33, `palette_sweep` 0–8, `mode4_scene` 1–11 y `sprite_limits` (las 16 máscaras con dim 0 y cuatro con dim 1), y con el corpus en 5 puntos (6 ROM idénticas; VDP FIFO Testing hereda su divergencia de máquina declarada).

- Controles. Valores aceptados: LAYER_MASK 0–0x0F (bits A 1, B 2, ventana 4, sprites 8), LAYER_DIM 0 o 1; otro valor, otro desplazamiento o más de un byte: `TESSERA_INVALID_ARGUMENT` (gpgx -1 y -4). Una escritura aceptada avanza la generación en 1; una rechazada no. Antes de escribir se leen 0xFF y 0. Se leen tal como se escribieron, con o sin suscripción, pero solo actúan con `RENDER_CONTROLS` activa. AUDIO_MUTE (15) no se sirve todavía (`UNSUPPORTED_CAPABILITY`, TP-209). Los valores escritos van en el registro de máquina del savestate (RF-3.9).
- LAYER_MASK actúa sobre el frame emitido y sobre `recompose_frame`, no sobre la multicapa. Una capa oculta no dibuja. En una línea que la ventana cubre entera, la ventana se ve con ella y el plano B visibles. Con ella y el plano A, pero sin B, se ve el plano A de la última línea dibujada sin ventana completa (una línea del frame anterior si la ventana está arriba), salvo en sombra/resalte con los sprites visibles, donde no se ve nada (`modes_sweep` 8, 9, 12, 26, 27, 31, 32, 33). Si la ventana cubre parte de la línea y está la A sin la ventana, la A desborda la ventana según su scroll fino. En sombra/resalte sin sprites, lo que saldría a brillo normal sale negro si gana un píxel opaco con prioridad y, si no, resaltado. En Mode 4, el bit 0 es el fondo (oculto: entrada 0 de la CRAM) y el bit 3 los sprites. En las líneas en blanco de la columna izquierda, el backdrop sale a brillo normal también en sombra/resalte (`sprite_limits` 225).
- LAYER_DIM deja cada canal RGB565 en un cuarto (`>> 2`), después de la máscara. En el frame emitido respeta los píxeles de sprite: los que gana un sprite, los de un sprite que choca con otro bajo un plano con prioridad (la misma regla de ATTRIBUTION) y, en Mode 4, cualquier sprite opaco, también bajo la columna izquierda en blanco. `recompose_frame` y la multicapa oscurecen todo, salvo en cada línea las columnas que un sprite ganó en la última línea del frame emitido (`sprite_limits` 0, 200, 223).
- Multicapa. Necesita `RECOMPOSITION`. Mode 4: rechazo (gpgx `RC_NOT_MODE5`, -20); entrelazado modo 2 (-21); capacidad menor que el frame (-23). Los rechazos se traducen como en `recompose_frame`. Sin `RENDER_CONTROLS`, A, B y ventana son los planos juntos sin sprites, y sprites y compuesto el frame entero. Con ella, cada capa sola sobre el backdrop; el compuesto muestra todas. Ninguna capa tiene la columna izquierda en blanco (`modes_sweep` 7 y 29).
- Con `RASTER_TRACKING`, la multicapa reproduce el diario raster sobre el estado final: cada evento rige desde su línea `v_counter`, y antes de su primer evento un valor es el final. En una línea con sombra/resalte que el estado final no tiene, los sprites se pierden como si estuvieran ocultos (`raster_bands` 8). Un diario que perdió eventos rechaza la recomposición (gpgx `RC_JOURNAL_OVERFLOW`, -24; `raster_bands` 9 y 10): en Tessera `TESSERA_WRONG_STATE` (decisión del responsable, 2026-11-08). `recompose_frame` no reproduce el diario. En `raster_bands` 7 las columnas 256–319 de las bandas H32 difieren: limitación conocida aceptada por el responsable (2026-11-08), como en `recompose_frame`.
- Banco: `tessera_diffbench --poke` (varias escrituras separadas por coma) y `--multilayer-out`; `tessera_md_run --controls`, `--isolate-layers`, `--replay-journal` y `--multilayer-out`; `compare_controls.py` y `compare_multilayer.py`.

## Estadísticas de recomposición (TP-205.3, medidas sobre gpgx r4)

`tessera_observation_recompose_stats_v1` (en gpgx, `get_recompose_stats`) devuelve 48 bytes: `struct_size` 48, `single_calls`, `single_hits`, `multilayer_calls`, `multilayer_hits` y `controls_fingerprint`. No exige suscripción. Ajustado con 16 guiones de llamadas sobre `modes_sweep`, `mode4_scene` y `raster_bands` (`tests/fixtures/diffbench/recompose-stats.txt`), que corren igual en gpgx (`compare_recompose_stats.py`) y en Tessera por la API pública (`integration.md_subscriptions`).

- Solo cuentan las llamadas aceptadas: ni `NOT_SUBSCRIBED`, ni Mode 4, ni entrelazado modo 2, ni la capacidad corta.
- Un acierto es la misma recomposición pedida otra vez en el mismo frame con los mismos `flags` y los mismos controles. Hay una entrada por tipo (simple y multicapa, independientes): volver a un valor anterior es un fallo; reescribir el mismo valor no. Los `flags` cuentan aunque no cambien la imagen.
- Una restauración vacía las entradas, pero los contadores siguen. En gpgx un `retro_reset` no las vacía; el reset de sesión de Tessera es un encendido (máquina nueva) y empieza de cero.
- `out_size` menor de 48: `TESSERA_BUFFER_TOO_SMALL` (gpgx -3, el mismo valor); mayor: se aceptan 48.
- `controls_fingerprint`: FNV-1a de 64 bits (base 0xcbf29ce484222325, primo 0x100000001b3) sobre `RENDER_CONTROLS` activa (0/1), LAYER_MASK y LAYER_DIM tal como se escribieron, un byte que gpgx pone a 1 cuando se escribe la supresión por tile de plano y las tablas de supresión de sprites (16 bytes), tiles (512) y tiles de plano (3 072). Tessera rechaza esas escrituras (PQ-1), así que esa parte vale ceros. Sin nada escrito y sin suscripción: 0x343178de6c99aa60; con `RENDER_CONTROLS`: 0x92b24ce1ac3a0531.
- Las suscripciones son del consumidor, no de la máquina: sobreviven a una restauración, como en gpgx a `retro_unserialize`.
- Banco: `tessera_diffbench --stats-script` y `--stats-out`.

## Delta de frame (TP-205.4, medido sobre gpgx r4)

`tessera_observation_frame_delta_poll_v1` (`poll_frame_delta`) y `tessera_observation_frame_delta_since_v1` (`frame_delta_since`) devuelven `tessera_obs_frame_delta_v1`, 2080 bytes: `struct_size` 2080, `delta_version` 1, `frame_generation`, cuatro contadores y `dirty_patterns[2048]`. Ninguna exige suscripción. Ajustado con 20 guiones sobre una ROM propia que escribe la VRAM desde una tabla por frame (`delta_probe`, `tests/support/md_delta_testrom.hpp`), `raster_bands`, `fm_psg_sweep` y `modes_sweep` (`tests/fixtures/diffbench/frame-delta.txt`), que corren igual en gpgx y en Tessera por la API pública.

- `frame_generation` cuenta los frames ejecutados desde el encendido (1 tras el primero). Una restauración no la cambia, aunque el estado sea de un frame anterior: es del consumidor, no de la máquina.
- `dirty_patterns`: un byte por patrón de 32 bytes de la VRAM; el bit n indica que cambió algún byte de su fila n (4 bytes). Por el puerto de datos y por DMA desde el 68000, reescribir el mismo valor no marca nada; el relleno y la copia de VRAM marcan todo lo que tocan, cambie o no. CRAM, VSRAM y los registros no marcan nada.
- Se acumula del final de un frame al final del siguiente y se congela ahí: leerlo no lo consume y dos lecturas dan lo mismo.
- Se acumula mientras está activa `VDP_MEMORY` o `RASTER_TRACKING`; si no, el delta va vacío. El primer frame tras el encendido, el primero en que vuelve a estar activa y el siguiente a una restauración dan todo sucio (0xFF), porque antes no se siguió nada.
- Contadores del frame: `raster_event_count` y `raster_events_dropped` (el diario raster, con `RASTER_TRACKING`), `parsed_sprite_count` (con `SPRITE_CAPTURE`) y `audio_write_count` (con `AUDIO_WRITES`); 0 si su suscripción no está activa.
- Una restauración marca todo sucio también en el delta ya congelado, y deja sus contadores a 0.
- `frame_delta_since(g)`: OR de los deltas de las generaciones posteriores a `g`, con la generación y los contadores del último frame. Con `g` igual o posterior a la generación actual, el del último frame. Se guardan 8 frames: si `g` queda a más de 8 generaciones, todo sucio y `TESSERA_STALE_EPOCH` (gpgx `AYTHER_STATUS_DELTA_HISTORY_LOST`, -10; Tessera no tiene ese código y lo traduce, como los demás). La salida se rellena aunque el estado no sea `TESSERA_OK`.
- `out_size` menor de 2080: `TESSERA_BUFFER_TOO_SMALL` (gpgx -3) y la salida no se toca; mayor: se escriben 2080.
- Excepción declarada EXC-007 (`docs/parity-exceptions.md`): cinco guiones en los que la CPU de Tessera escribe la VRAM a un ritmo algo distinto que gpgx (2554 palabras por frame con la pantalla encendida, frente a 2550; una palabra al final de cargar la escena con la pantalla apagada). Cambian las filas sucias junto al límite de un frame y el recuento de sprites del frame que reescribe la tabla de sprites. No es una regla del delta. Están en `frame-delta-timing.txt`; el test nativo los ejecuta y muestra la diferencia sin fallar la suite.
- Banco: operaciones `d[out_size]`, `D<generación>[,out_size]` (`D-<n>` cuenta hacia atrás desde la última lectura), `S` y `L` (guardar y restaurar más tarde) en `--stats-script`.

## Hash del frame (TP-206.1, medido sobre gpgx r4)

La región 23 (`FRAME_HASH`, 56 bytes) exige la suscripción `FRAME_HASH` (0x800); sin ella, `TESSERA_NOT_SUBSCRIBED` (gpgx -9). Ajustada con los 34 `modes_sweep`, `raster_bands`, `sprite_limits`, `fm_psg_sweep`, `fm_psg_z80`, `mode4_scene`, `delta_probe` y FX-02 (`tests/fixtures/diffbench/frame-hash*.txt`), que corren igual en gpgx y en Tessera por la API pública.

- Disposición: `layout_version` (u32, 1), `struct_size` (u32, 56) y seis u64 little endian: `frame_index`, `video`, `audio`, `vram`, `cram`, `vsram`.
- Algoritmo: FNV-1a de 64 bits con primo 0x100000001b3 y base 1469598103934665603 (0x14650FB0739D0383). No es la base estándar de FNV-1a (0xCBF29CE484222325): es la que usa gpgx, y cada campo parte de ella.
- `frame_index`: índice desde 0 del último frame ejecutado. Es monótono y no cambia al restaurar.
- `video`: los píxeles emitidos en RGB565 little endian, fila a fila, `ancho × 2` bytes por fila sin el relleno del pitch. La ventana es la geometría del frame **anterior** (en el primer frame, la propia): gpgx hashea antes de actualizar su viewport. Medido al crecer (de 256×192 a 256×224 en el frame 1 de `modes_sweep`): el frame nuevo entra solo en la ventana vieja.
- `audio`: las muestras int16 estéreo intercaladas del frame, tal como se entregan.
- `vram`, `cram`, `vsram`: los bytes crudos de las regiones 1 (64 KB), 2 (128 bytes) y 4 (128 bytes), como se leen.
- Se calcula al final de cada frame con la suscripción activa. Restaurar un estado, cargar otro o hacer un reset no lo recalcula: la región conserva el valor anterior hasta el final del frame siguiente.
- `audio` no es comparable con gpgx: Tessera mezcla a 48 kHz (801 muestras estéreo por frame en NTSC) y gpgx a 44,1 kHz (736 en el estado estable medido). Es la misma fórmula sobre otras muestras, y el PCM es solo informativo (plan §5.2). Excepción declarada EXC-008; los tests comparan los otros cinco campos byte a byte.
- Primeros frames tras el encendido: mientras la ROM prepara el VDP con la pantalla dibujándose, la CPU de Tessera llega a cada escritura algo antes que gpgx (la causa de EXC-007). Una línea o una entrada de CRAM cae un paso antes, y cambian `video`, `vram` o `cram` en los frames 0–1. `raster_bands` 7 arrastra además la limitación conocida de líneas H32 en un frame H40. Excepción declarada EXC-007 (ampliada al plano `frame_hash`). Están en `frame-hash-timing.txt`; el test nativo los ejecuta y muestra la diferencia sin fallar la suite.
- Banco: operación `h` en `--stats-script` (`H` añade los hashes que calcula el propio host, para diagnóstico). Un caso cuya ROM es `fx02` usa la ROM FX-02 indicada con `--fx02`.

## Controles en Mode 4 (TP-211.1, medido sobre gpgx r4)

gpgx r4 anuncia las capacidades 0xEFFFF: **sin** el bit 16 (`MODE4_CONTROLS`). Según la ABI, ese bit dice que la supresión de sprites, el peel y la supresión por celda de plano funcionan también en Mode 4; Tessera no sirve esos controles en ningún modo (regiones 6–9, PQ-1). Por eso Tessera tampoco lo anuncia: paridad con r4 (decisión del propietario, 2026-11-08). La API aún no publica la máscara de capacidades de observación; queda pendiente fuera de TP-211.1.

- LAYER_MASK y LAYER_DIM en Mode 4: gpgx acepta los 16 valores de la máscara (estado 0, también con los bits B y ventana, que no tienen referente; r4 no devuelve `UNSUPPORTED_MODE`). Bit 0 = el fondo, bit 3 = los sprites (TP-205.2). Se escriben y actúan igual que en Mode 5: desde el frame siguiente y solo con `RENDER_CONTROLS` activa.
- Ajustado con `tests/fixtures/diffbench/mode4-controls.txt`: `mode4_scene` 1, 2, 4, 5, 6, 8, 9, 10 y 11 recorren las 16 máscaras, LAYER_DIM y sus combinaciones; un guion sin `RENDER_CONTROLS` que la activa a mitad; y `mode4_pbc`. El efecto se lee en el vídeo de FRAME_HASH del frame siguiente, junto con VRAM, CRAM y VSRAM, desde el frame 1 (el 0 es el de EXC-007).
- `mode4_pbc` vuelve a Mode 5 (registro 1 = 0x44). Al salir de Mode 4 gpgx reparte la VRAM (palabra w → 2w) **sin mover los bytes en su búfer**: una palabra que Mode 4 guardaba con el byte bajo primero queda con el alto primero en Mode 5. Medido en la región VRAM; corregido en `Vdp` (TP-211.1).

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
