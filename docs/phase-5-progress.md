# Fase 5 — progreso local

Fecha: 2026-10-19. Estado: **en curso**; T030–T036 siguen abiertas hasta
acreditar sus criterios completos. No se abrió ningún PR ni se ejecutó GitHub
Actions. El lock de Runtime conserva la release publicada existente.

## T030 y T031 — primer recorrido Runtime

Se instaló localmente el Engine de `feat/tessera-native-session` desde su build
`windows-native` y se configuró Runtime contra ese prefijo y el paquete local
de Tessera. Runtime admite `--provider tessera --rom ... --pack ...
--trust-registry ... --frames N` sin un core libretro. Prepara una configuración
Mega Drive a partir de la ROM detectada, aplica estados completos de ocho
controles por frame y expone código de rechazo, identidad de frame,
confirmación de transacción, hashes de vídeo original/presentado y PCM. El
proveedor explícito rechaza opciones libretro. El modo es por ahora **sin
ventana ni salida de audio al dispositivo**.

En `C:/ayther/AYTHER-Runtime/out/build/tessera-local` pasaron `runtime_options`
(166 assertions) y `tessera_runtime_native_headless` con copia local de FX-03.
El recorrido de cuatro frames activó, desactivó y reactivó el pack: la imagen
presentada cambió con la política; vídeo original y PCM continuaron, y Engine
confirmó las tres transacciones. El runner directo imprimió hashes y estados
por frame. No hay evidencia todavía de GPU, regularidad de presentación ni
cortes de audio reales. El camino de traza/EOF está implementado, pero falta un
oráculo de Runtime para él y para rechazo de entradas inválidas durante una
política pendiente. La CI declarada aún consume el lock anterior.

## T033 y T034 — comprobaciones de Tessera

`integration.robustness` pasó en `windows-dev` y `windows-diagnostics`:
entradas incompletas conservan estado y pendiente, EOF repetido no avanza,
traza truncada se rechaza, capturas retenidas sobreviven productor saturado y
cierre. Se repitieron `unit.state_format`, `integration.state_store` e
`integration.checkpoint` con AddressSanitizer. Falta consolidar todos los casos
de agotamiento y fallos de E/S en la matriz de T033.

`package.installed_consumer` pasó: instaló, reubicó y consumió el paquete desde
C y C++, probó la CLI y rechazó versión, CRT, arquitectura y toolset
incompatibles. T034 sigue abierta hasta revisar derechos y checksums de todas
las copias de fixtures, completar la guía y validar el paquete final.

## Pendientes para T032, T035 y T036

- Instrumentar las etapas y comparar Tessera autónoma, Engine y Runtime con
  cargas idénticas y percentiles/identidades de binarios y contenido.
- Completar salida GPU y audio de Runtime, su test de integración real y CI.
- Publicar paquetes de Tessera y Engine con revisiones, checksums y procedencia
  verificables antes de actualizar el lock de Runtime.
- Cerrar la trazabilidad RF/RC/CA con pruebas ejecutadas; ningún criterio
  pendiente se declara aprobado.
