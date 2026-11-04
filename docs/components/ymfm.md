# ymfm (YM2612/YM3438 y YM2413)

Spec 008, DA-4, RF-3.7, RNF-4. Verificado el 2026-11-03.

## Licencia
BSD-3-Clause (`LICENSE`, copyright 2021 Aaron Giles). Exige conservar el aviso en fuente y binario. Sin copyleft.

## Procedencia
`github.com/aaronsgiles/ymfm`, commit `81aec25ccbb98f4873a255f7551ac4dadac59b4a` (main), directorio `src/`: `ymfm_fm.*`, `ymfm_opn.*`, `ymfm_opl.*` (YM2413 es la variante OPLL en `ymfm_opl`), `ymfm_ssg.*`, `ymfm.h`.

## Estado completo
`ym2612::save_restore(ymfm_saved_state&)` existe, igual que en los demás chips. Basta para el savestate, con la comprobación del plan (RF-4.1) de que dos ejecuciones desde el mismo estado dan la misma salida.

## Temporización
La interfaz `ymfm_interface` lleva los temporizadores y la ocupación; `generate()` produce muestras a la frecuencia nativa del chip. El orden y la segmentación de las escrituras se registran en la capa de Tessera, no en ymfm (RF-3.5).

## Mute por canal
No hay un mute público. `ym2612::generate` ya suma cada canal por separado con la máscara `1 << chan` en `m_fm.output(...)`, así que el mute (RF-3.7) se logra con un parche mínimo: filtrar `chan` con una máscara propia antes de sumar, más el DAC. Es un parche local de pocas líneas y se anota como modificación MIT-compatible en el aviso (la BSD-3 permite modificar). Pendiente de implementar en la fase 1; no es bloqueante.
