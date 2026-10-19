# Evaluación del backend — Gate 0

Fecha: 2026-10-19. **Gate 0 aprobado localmente; ClownMDEmu seleccionado para
la integración posterior.** La evaluación usa ejecutables privados de fase 0,
separados del núcleo y de la API de producto. La fase 1 no ha comenzado.

## Fuentes y reproducción

El lock versionado es [dependencies.lock.json](../cmake/dependencies.lock.json).
[bootstrap-backends.ps1](../tools/bootstrap-backends.ps1) verifica los ZIP y
cada archivo extraído; las descargas requieren `-Download`. Conserva timestamps
de fuentes intactas para reutilizar builds incrementales. Caché y binarios
permanecen fuera de Git.

| Candidato / componente | Revisión fijada |
| --- | --- |
| ares v148, incluidos nall/libco/sljit/qon del archivo | `0aafd85789215e84e1e43415c07d4c88461b7899` |
| ClownMDEmu | `88ef45a6585556e2247dd26b6c937d6b9fb4a12d` |
| clown68000 | `bb198386ae3ee0cd6d246ff917f9941b02b5be6a` |
| clownz80 | `aa4cae3127a5893ba7066bcc2d9e9ba8e88e1c3b` |
| clowncommon del core | `ecee31fc78ee1eed901431d070239c9c6430607c` |
| clowncommon de ambos CPU | `ddeff174c2121b40284883e0e1e3520f39d99227` |

Desde PowerShell en la raíz:

```powershell
./tools/bootstrap-dependencies.ps1 -Offline
./tools/bootstrap-backends.ps1
$env:AYTHER_ENGINE_PREFIX = 'C:/ayther/AYTHER-Engine/install/windows-release-engine-vpx'
$env:AYTHER_ENGINE_DEPENDENCY_PREFIX = 'C:/ayther/AYTHER-Engine/build/windows-release-engine-vpx/vcpkg_installed/x64-windows'
./tools/check.ps1 -Preset windows-engine-integration -Jobs 1
pwsh -NoProfile -File ./tools/check-backends.ps1
```

El runner configura, construye y comprueba los candidatos con un trabajo.
`-SkipBuild` reutiliza ejecutables locales existentes, manteniendo las
comprobaciones de identidad de binario, fuentes y runner. El informe regenerable
es [backend-gate.json](../build/evidence/backend-gate.json); la ejecución actual
se registra en [gate-matrix-final.log](../build/evidence/gate-matrix-final.log).
Cada instancia se identifica por PID y hora UTC de inicio; un PID reciclado
por Windows no se confunde con reutilizar la misma instancia.
El código 0 exige todas las pruebas del gate; el código 1 indica fallo de
validación. La matriz final terminó con código 0 e informe de esquema 2.

Se usa clang-cl 22.1.6, MSVC 14.51.36231, C++20/C11, `/MD` y Ninja.
El perfil contrastado es Mega Drive NTSC-U, mando de tres botones y CD
desactivado. No se declara soporte Mega-CD por incluir sus campos en el codec.

## Expectativas independientes

La [demo propia](../tests/fixtures/backend-probe/README.md) se generó antes del
contraste. Su ROM SHA-256 es
`5296c4fa331fd8cb47663fc7ce3b0abef03364221b309a65f2966c6a8642225c`.
Las trazas cortas contienen 60 frames completos y las largas 3.000; acción B
frente a control neutral. El generador determina RAM, VRAM, CRAM, color central,
periodo/atenuación del PSG y ventanas de actividad/silencio sin consultar los
resultados de candidatos. El silencio previsto es audio válido.

Los hashes contrastan procesos de la misma implementación, configuración y
traza; no se exige igualdad entre emuladores. El fixture copiado de Engine/Lab
añade regresión de replay y checkpoints, sin aportar un oráculo independiente
de actividad sonora. Ninguno de estos probes cierra FX-02 por API autónoma y
Engine; FX-01 y el paquete gráfico real FX-03 siguen pendientes.

## ClownMDEmu: máquina y persistencia explícita

El target headless enlaza el core y los intérpretes 68000/Z80 con
`CC_USE_C99_INTEGERS=ON`. Se conserva como regresión el fallo del backup de
upstream: guardar countdown del mando 9.876, cambiar a 1.234 y cargar deja
1.234. `SaveState/LoadState` omite `ControllerManager_State` y `mega_cd.cdd`.
Ese backup no se usa para persistencia ni hashes canónicos.

El [codec de evaluación](../tests/backend_probe/clown_state_codec.cpp) usa
12 raíces: `state`, `m68k`, `z80`, `vdp.state`, `fm.state`, `psg.state`,
`controller_manager.state`, `mega_cd.m68k`, `mega_cd.cdc`, `mega_cd.cdda.state`,
`mega_cd.pcm.state` y `mega_cd.cdd`. El [inventario generado](../tools/generate-clown-codec.audit.json)
describe 281 hojas y fija SHA-256 de 78 archivos fuente. Su identidad es
`3fffabc5b4e97e04506138221ce7f03f7c6576e6bafaea6ca18e9f0d35549cfc`.
El payload ocupa 1.131.929 bytes y su envoltura 188 bytes: versión, esquema,
revisión, longitud y checksum preceden a los campos little-endian explícitos.
No se persisten punteros, padding, pilas ni bytes de objetos C/C++.

La configuración y ROM se suministran de nuevo y se vinculan por hash; quedan
fuera del payload de máquina los callbacks, punteros de host y sus propietarios.
Una instancia privada recibe los campos, comprueba límites, relaciones y el
perfil sin CD, y solo después sustituye la máquina activa. Un rechazo conserva
máquina, época y transacción pendiente. Se prueban corrupción, truncado,
identidades ajenas y valores semánticos inválidos con checksum recalculado.

El qualifier [clown_gate.cpp](../tests/backend_probe/clown_gate.cpp) envuelve
máquina y media en `TSSTATE1`: TLV ordenado, etiquetas únicas y longitudes
explícitas, enteros little-endian, hashes de ROM/build/configuración, frame
completado, tick lógico y entrada completa latcheada. Excluye identidad de
sesión/época, observación, métricas, recursos HD y pendientes. `TSTRACE1` fija
la traza completa; `TSCHK001` guarda estado, identidad/cursor de replay e
identidad inicial. Exportar no aplica ni cancela controles. Restaurar K continúa
en K+1 y valida todo antes del intercambio; no neutraliza controles ya activos.

Esto es un contrato privado de viabilidad: no implementa todavía la API C
binaria de T007 ni el almacenamiento de producto con todos los fallos de E/S
de T023. El reinicio reconstruye el estado inicial registrado, incluida SRAM,
con frame cero, entrada neutral, nueva época y controles neutrales.

## Vídeo y continuidad sonora

El vídeo original canónico es `TSVIDEO1`, geometría y RGBA8 por filas activas,
sin padding y alfa 255. Los índices y nibbles de paleta de upstream se
convierten sin Engine ni composición HD. Tras restore no se ofrece como frame
actual una imagen vacía o antigua: vuelve a estar disponible al ejecutar el
siguiente frame. Las capturas históricas retenidas siguen legibles.

Los callbacks ejecutan siempre FM, PSG, PCM y CDDA; los buffers se inicializan
a cero antes de generar. El perfil exige PCM/CDDA silenciosos con CD desactivado.
Los filtros low-pass de upstream están activos y el efecto ladder de FM está
desactivado; estas opciones forman parte de la identidad de configuración.
El mezclador entero [clown_media.cpp](../tests/backend_probe/clown_media.cpp)
genera `TSAUDIO1`, PCM S16LE estéreo L/R a 48.000 Hz. Usa ganancias Q15 FM
32.768 y PSG 4.096, redondeo al más cercano con empates alejándose de cero,
saturación S16 y reconstrucción causal ZOH.

La cadencia sigue 895.780 ticks maestros reales por `Iterate` y reloj de
53.693.175 Hz. El contador FM recibe 127.968 ciclos por frame y conserva su
resto módulo 144; no se sustituye por una duración nominal de frame redondeada.
El estado explícito de media conserva fases racionales, contadores, muestras
retenidas y colas de entrada FM/PSG pendientes. No existe una cola de salida
oculta: el PCM entregado pertenece al llamador. Los filtros del core forman
parte del codec de máquina. Las pruebas de media cubren cadencia, ganancias,
saturación, división de callbacks, restauración y rechazos sin mutación.

## Observación y controles del qualifier

Los únicos controles seguros son máscaras de regiones `0xF` y de eventos
`0x1F`. Se validan tipos, bits, duplicados, sesión, época y próximo frame.
Hay una sola transacción pendiente; se confirma al comenzar el frame indicado.
Entrada inválida, pausa y EOF no avanzan ni aplican pendientes. Reset, restore
y cierre devuelven identidad/causa de cancelación fuera de la cola saturable;
un restore fallido conserva la pendiente. No se usan los flags de desactivación
PSG como silencio: esos flags alterarían la evolución del chip.

Las capturas inmutables identifican sesión, época y generación. Dos lectores
retienen la misma captura sin interferirse; el límite es ocho capturas de
hasta 8 MiB cada una. La cola de eventos admite 4.096 entradas, con cursores
independientes y contador de pérdidas fuera de cola; los huecos son detectables.
Saturar estos recursos no cambia los hashes de máquina, vídeo ni PCM.

**No se declara capacidad temporal completa.** El bit temporal `0x10` de la
máscara de regiones se rechaza. VRAM/CRAM/SAT finales no prueban todos los
efectos de un frame. Ante cobertura insuficiente, el qualifier presenta el
frame original completo con causa `TEMPORAL_COVERAGE_INCOMPLETE`, sin mezclar
una sustitución parcial. Esta prueba acredita el mecanismo de respaldo del
probe; T025/T029, Engine, precedencia/solapamientos y FX-03 siguen pendientes.

## ares

El target aislado incluye MD, sus procesadores/chips, nall, libco, sljit y qon;
excluye frontend, CHD e iconos sin referencias en MD. El harness define los
globals de `ares.cpp.in` y ordena `system.cpp` primero para conservar el
scheduler durante la destrucción de threads, sin modificar las fuentes.
Este commit fuerza el VDP de accuracy aunque se declare `PROFILE_PERFORMANCE`.

El probe copia vídeo según pitch desde el worker, con sincronización y timeout.
Drena audio desde `Stream::read`, tras filtros/remuestreo, separado por stream;
sus hashes f64 no son el PCM canónico de Tessera. Pasa los oráculos parciales
de memoria, color y actividad/silencio PSG de 60 frames y su repetición entre
procesos; no inspecciona periodo/atenuación internos del PSG.

`serialize(false)` copia pilas de corutina y queda descartado para persistencia.
`serialize(true)` sincroniza el scheduler sin neutralidad temporal demostrada.
Faltan select/latches privados del mando y estado completo de filtros,
remuestreo y muestras pendientes de Stream. No se ejecutan esos serializers
para atribuirles continuidad ni se declara un codec completo de ares.

## Matriz de decisión y límites

La matriz congelada completó seis baselines de 3.000 frames: dos
procesos por control propio, acción propia y control del fixture original.
Los hashes canónicos de estado/vídeo/PCM coincidieron en cada frame; acción y
control difirieron en la ventana activa y cumplieron los oráculos independientes.
Pasaron seis restores en procesos nuevos: ambos contenidos en K=500, 1.500
y 2.500, con 500 frames K+1..K+500 y los tres hashes coincidentes. Pasaron
también tres modos de observación de 60 frames desactivada/completa/saturada,
sin alterar esos hashes, y los probes parciales de ambos candidatos.

El [informe final](../build/evidence/backend-gate.json), esquema 2, registra
`gate0_pass: true`, `selected_backend: ClownMDEmu`, diez pruebas obligatorias
verdaderas y ningún error. Son 18.000 frames de baseline, 3.000 restaurados
y 180 de observación, además del contraste parcial. La cobertura temporal
sigue incompleta: la viabilidad se acredita con respaldo original íntegro,
sin anunciar una capacidad temporal inexistente. ares no se selecciona por
los impedimentos de persistencia y continuidad descritos arriba.

La suite final de desarrollo pasó 9/9 y la diagnóstica 8/8 sobre las fuentes
congeladas. El qualifier pasó 78 comprobaciones de contrato y 876 de media
en 11 grupos; la identidad de procesos tiene tres regresiones aprobadas.
Los cuatro casos negativos de invocación del probe también se rechazaron
sin generar un informe de éxito; no acreditan la CLI de producto de T013.
ASAN instrumenta el probe C++ y los cinco targets C de
ClownMDEmu; ares está desactivado en esa suite. Engine SDK 0.1.0/Core ABI 8
pasó su enlace mínimo 3/3. Estas pruebas tienen alcances distintos y no
demuestran la integración Engine→Tessera de T015–T018 ni el MVP completo.

Los tiempos por frame del qualifier separan emulación/PCM y codificación/hash.
Son datos locales de diagnóstico, sin calentamiento/repeticiones y comparación
Engine/Runtime suficientes para cerrar el benchmark T032.
La [primera ejecución de acción propia de la matriz final](../build/evidence/backends/gate0-20261019T195047554Z-7b7af288a5414b92a44f30fffc064acb/own-action-process1.json)
midió p50/p95/p99 de 1,3474/2,2909/4,7857 ms en emulación/PCM y
18,2605/25,7697/30,6805 ms en codificación/hash canónicos.
Incluye trabajo del harness de evaluación; no fija el coste del núcleo de
producto ni acredita una tasa de presentación. Capturas, respaldo y oráculos
quedan fuera de esas dos series; su suma no representa el coste total del frame.

El alcance construido y su tratamiento AGPL local constan en
[los avisos](../legal/THIRD_PARTY_NOTICES.md); las revisiones y licencias están
en [el inventario](../legal/DEPENDENCIES.csv). La selección técnica local no
acredita cumplimiento de una distribución futura. No se publican binarios,
no se ejecutó CI remota y no se ha iniciado la fase 1. T001–T006 quedan
completadas en este alcance de fase 0; las pruebas de producto siguen en sus
fases previstas.
