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
No hace falta parchear ymfm: `ym2612::m_fm`, `m_dac_enable` y `m_dac_data` son `protected`. `MutableChip` (`src/md/ym.cpp`) deriva de `ym2612` y repite la suma de `generate` (canales de 9 bits con la discontinuidad del DAC), saltando los canales silenciados (aportan la discontinuidad del silencio). Sin mute llama al `generate` original. El mute afecta solo a la muestra mezclada, no al estado del chip (TP-107.5); el chip entrega además la muestra sin mute para que el savestate no dependa de la máscara.

## Integración (TP-107.1)
Vendorizado en `cmake/dependencies.lock.json` (`ymfm`, caché `third_party/cache/ymfm-81aec25/src`, SHA-256 verificado al configurar por `cmake/Ymfm.cmake`). Se compilan `ymfm_opn.cpp`, `ymfm_adpcm.cpp` y `ymfm_ssg.cpp` (el header de OPN los arrastra aunque la YM2612 no los use). `src/md/ym.{hpp,cpp}` es el adaptador (`Ym2612`): reloj del chip = maestro/7, una muestra cada 144 relojes (1008 ciclos maestros), temporizadores armados por `ymfm_set_timer` con vencimiento en relojes del chip. Los accesos declaran el ciclo maestro en el que ocurren; la ocupación (bit 7 del estado) la mide `System` en el bus, no ymfm. Prueba: `unit.md_ym`.

El silencio de la YM2612 no es cero: ymfm modela el desplazamiento constante del DAC (discontinuidad de 9 bits), así que «silencio» es un flujo plano.
