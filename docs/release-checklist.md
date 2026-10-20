# Coordinación de CI y entrega del MVP

Estado al 2026-10-20: preparación local; **sin PR, push nuevo, tag, release ni
Actions de las revisiones finales**. Objetivo aprobado: tres PR coordinados,
uno por Tessera, Engine y Runtime. No abrir un PR por tarea.

El usuario confirmó que no hay permiso verificable para publicar la ROM/pack
Engine/Lab ni la ROM FX-03 derivada. Se generan en `build/` con
`tools/generate-native-fixtures.ps1`. Las ramas locales publicables de Tessera
(`1f9389d`) y Runtime (`91b85da`) parten de `main` con commits netos sin los
binarios: `git rev-list --objects main..HEAD` no muestra ROM, pack ni trust
generados. Las ramas previas se conservan solo como respaldo local y no deben
subirse. Ningún binario generado se subirá como artifact de Actions.

| Repositorio | Validación local de este corte | CI remota exigida antes de T036 |
| --- | --- | --- |
| Tessera | 16/16 CTest breves con ROM generada, consumidor instalado y replay largo de fase 4; comparación progresiva FX-03 y FX-02 sin pack completadas localmente | Job `Core checks` del PR, hashes de commit y suite sin omisiones |
| Engine | Build/instalación local y 5/5 pruebas nativas con ROM/pack generados; regresión de replay por traza 1/1 y controls/presentation 2/2 tras la corrección | Checks nativos del PR con Tessera de la revisión final, no la revisión antigua fijada `dfef722` |
| Runtime | Build MSVC local; 4/4 CTest de proveedor, FX-02 audio y FX-03 Vulkan con rutas generadas; FX-03 progresivo compuso 180/180 frames por repetición y separó `engine_frame`. Perfil con lock rc.19 y ruta nativa desactivada: build y `runtime_options` 1/1 sin descargas | Ocho checks protegidos de `main`; además recorrido nativo headless con Engine/Tessera finales y GPU/audio en runner con dispositivo real |

La rama `main` de Runtime protege estos contextos: `Windows / RelWithDebInfo`,
`Linux / RelWithDebInfo`, `Linux / ASan-UBSan-Fuzz`,
`Security / Dependency Review`, `Security / Secret Scan`,
`Analyze (actions)`, `Analyze (c-cpp)` y `Windows / QA`.
Las ramas `main` de Tessera y Engine no devuelven protección de rama en la API;
sus workflows declarados siguen siendo parte del gate del MVP. Ninguno de los
tres repositorios mostró runs de Actions en las ramas de trabajo consultadas.

El paquete Runtime publicado continúa fijado al Engine `v0.1.0-rc.19`, que no
exporta esta ruta nativa. El workflow de Engine instala Tessera desde el commit
antiguo `dfef722`; ese pin tampoco acredita el FX-03 final. No cambiar locks
por un artefacto inexistente. Para cerrar T035 hay que: consolidar y publicar
la revisión de Tessera con SHA de fuentes y paquete; validar y publicar Engine
contra ese SHA; actualizar el lock Runtime con versión, URL, checksum y
procedencia reales; ejecutar sus ocho checks protegidos y el perfil GPU/audio
declarado. Un runner sin GPU/audio no convierte una omisión en aprobación. La
ruta nativa de Runtime es una opción de build explícita mientras el lock
anterior mantenga la CI del producto actual; el perfil de aceptación la activa.

La prevalidación local utiliza prefijos instalados y cachés verificadas, una
compilación activa y benchmarks aislados. No se requieren PR adicionales,
matrices nuevas por tarea, cron ni ejecuciones manuales para validar cambios
intermedios. Antes de cada push, registrar `git diff --check`, CTest pertinente,
hashes de binarios/fixtures y el resultado de `acceptance-report.md`.

**Bloqueadores actuales:** el perfil local ya separa asignaciones C++, copias
explícitas, eventos/pérdidas, respaldos, espera de imagen y trabajo GPU, pero
no mide cortes acústicos físicos ni presentación efectiva del dispositivo.
Restan los negativos Runtime de entradas y pendientes, la revisión final de
derechos/idioma, presupuestos sustentados por las series, artefactos publicados
y CI remota encadenada. T036 y CA-08/CA-11 permanecen abiertos.
