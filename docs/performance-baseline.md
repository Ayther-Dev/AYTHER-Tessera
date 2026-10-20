# Línea base local de rendimiento — MVP

Fecha: 2026-10-19. Evidencia reproducible: `build/evidence/phase5-benchmark-final/summary.json`
(archivo local ignorado por Git). Runner: `tests/performance/benchmark.cpp`.
Tres repeticiones secuenciales por modo, 180 frames por repetición y 30 de
calentamiento excluidos de los percentiles de ejecución. ROM FX-02 neutral de
32 KiB, SHA-256 `5296c4fa331fd8cb47663fc7ce3b0abef03364221b309a65f2966c6a8642225c`;
configuración y traza TLV únicas generadas por el runner y usadas en las tres
capas. El benchmark comprueba 180 frames, primera/última identidad y salidas
originales iguales entre Runtime sin ventana y con dispositivos. El replay y
los oráculos completos se prueban aparte; este runner no los sustituye.

Equipo: Intel Core i7-1280P (14 núcleos/20 hilos), 16 GiB RAM, Windows 11
10.0.26200. GPU: NVIDIA GeForce RTX 3060 Laptop GPU, controlador
32.0.16.1656, Vulkan; Intel Iris Xe también presente, controlador
30.0.101.2079. Tessera: Clang 22.1.6 + MSVC 14.51, `/MD`, `RelWithDebInfo`;
Engine y Runtime: MSVC 19.51/14.51, `/MD`, `RelWithDebInfo`.
Revisiones de partida: Tessera `bc3327e`, Engine `a661ad7`, Runtime `90e3b04`;
los cambios de fase 5 en Engine/Runtime aún eran locales al medir. Binarios:
Tessera CLI SHA-256 `c635a9c221db2ad400666d6cfc8b2306a758790bec786cf0aee63469c06f6fe`,
Engine instalado (`ayther_engine.lib`) `a7e87a1e97eb65c861cda12fb53a6d06945087aa0be017406a104b428f12b512`,
Runtime `f47df8a522a902d85e7678e9d6e1c2dbd73bcabbabf6ab84c2197f94c1ec14ab`.

| Modo / etapa | p50 por repetición | p95 por repetición | p99 por repetición | Pico de memoria de proceso |
| --- | --- | --- | --- | --- |
| Tessera sin observación, `replay_next` | 1,828 / 1,786 / 1,835 ms | 2,175 / 2,155 / 2,080 ms | 2,819 / 2,351 / 2,176 ms | 16,1–16,3 MiB |
| Tessera con observación, `replay_next` | 3,292 / 3,194 / 3,315 ms | 3,744 / 3,665 / 3,681 ms | 4,114 / 3,867 / 3,981 ms | 20,8–21,9 MiB |
| Engine nativo sin ventana, avance de sesión | 3,171 / 3,224 / 3,258 ms | 3,702 / 3,788 / 3,801 ms | 3,828 / 3,852 / 4,024 ms | 22,5–22,6 MiB |
| Runtime con Vulkan y audio, avance de Engine | 3,101 / 3,064 / 2,883 ms | 3,623 / 3,549 / 3,438 ms | 3,781 / 3,967 / 3,813 ms | 247,2–247,8 MiB |

Tessera mide `replay_next`, exportación/hash canónicos y lectura de
observación con `steady_clock` en microsegundos; el informe JSON conserva las
150 muestras posteriores al calentamiento por repetición. El modo observado
realizó 180 lecturas de snapshot y 180 lotes de eventos por repetición. En
Runtime, `engine_step` rodea solo avance/replay de Engine; `engine_frame`
rodea la adquisición del frame y la composición de Engine;
`runtime_hash` rodea hashes de vídeo original/presentado y PCM;
`runtime_device` rodea encolado SDL, render Vulkan y espera de `end_frame`.
Estas etapas no se suman con la serie de presentación: esta última mide
intervalos entre entregas, incluye el limitador de ~16,689 ms y puede solaparse
con trabajo de CPU/GPU. Carga de ROM/pack, configuración y apertura de
dispositivos quedan fuera de la etapa estable; guardado se valida por separado.

| Serie Runtime con dispositivos | Tres repeticiones |
| --- | --- |
| `runtime_device` p50 | 0,278 / 0,300 / 0,294 ms |
| Entrega al presentador p95 | 18,104 / 17,841 / 17,944 ms |
| Frames e intervalos | 180 y 179 por repetición |
| Validaciones Vulkan | 0 errores reportados |
| Audio de dispositivo | 144.143 frames fuente aproximadamente por repetición, silencio esperado; 0 episodios y 0 muestras faltantes observadas |

El punto de entrega es el retorno de `VkSwapchain::end_frame`, con reloj
monotónico. Los timestamps de presentación efectiva no están disponibles en
este backend y se informan separadamente como `unavailable`. El callback de
SDL observa demanda de bytes adicionales del stream tras precargar cuatro
frames; su contador de faltantes es una **aproximación de falta de datos en el
stream**, no una medición de interrupciones audibles ni de muestras realmente
perdidas por el hardware. La prueba adicional FX-02 con B pulsado entregó PCM
activo (`source_peak=1022`, `active_samples=478628` en 300 frames) y no
silencio constante. El ensayo FX-03 de GPU leyó el render offscreen de 960×720:
1.224 píxeles magenta con pack, cero sin pack y el mismo hash al reactivarlo;
el swapchain presentó los cuatro frames sin errores.

### Ampliación de métricas, 2026-10-20

Evidencia local: `build/evidence/phase5-benchmark-allocations-final/summary.json`,
`fx03-devices.log` y `fx02-active-audio-callback.log` en el mismo directorio;
repetición con el runner final en
`build/evidence/phase5-benchmark-config-verified-v2/summary.json`. El runner
final comprueba también igualdad de SHA-256 de configuración entre las tres
rutas (`077fe4f966c6eaa21d41de33cec512c4ddeb3a68ab274ff3cf3d9a2a9e4d3a14`).
La misma
ROM FX-02, configuración y traza de 180 frames se ejecutaron tres veces por
modo, con 30 de calentamiento. Esta corrida habilitó
`TESSERA_ALLOCATION_METRICS=ON` y `AYTHER_NATIVE_ALLOCATION_METRICS=ON`.
Ambos interruptores son opcionales y están desactivados por defecto. Los
tiempos de esta corrida incluyen la instrumentación; la tabla anterior
conserva la línea base temporal sin contador de asignaciones.

| Ruta | `new` por frame p50 / p95 / p99 | Bytes solicitados a `new` por frame p50 / p95 / p99 |
| --- | --- | --- |
| Tessera CLI sin observación | 736 / 736 / 736 | 29.732.198 / 29.732.198 / 29.732.198 |
| Tessera CLI con observación | 1.088 / 1.088 / 1.089 | 33.505.105 / 33.505.105 / 33.507.153 |
| Engine→Tessera sin ventana | 188 / 188 / 189 | 2.189.728 / 2.189.728 / 2.191.776 |
| Runtime con dispositivos | 193 / 194 / 196 | 2.190.328 / 2.191.352 / 2.194.632 |

Cada repetición produjo 150 muestras medidas y los mismos percentiles de
asignación. El contador global del ejecutable intercepta `operator new` y
`new[]`, incluidos tamaños alineados; informa llamadas y bytes solicitados,
no memoria viva ni asignaciones hechas mediante `malloc`, Vulkan, SDL o
bibliotecas cargadas con otro asignador. La ruta CLI incluye hashes canónicos,
extracción de estado y lectura de observación dentro del frame; la ruta
Engine/Runtime mide su propio recorrido y sus cifras no son equivalentes a
las del CLI. El contador puede alterar latencias y memoria máxima.

| Transferencia / observación | Resultado por repetición de 180 frames |
| --- | --- |
| Tessera con observación | 180 snapshots; 172.212.480 bytes de payload copiados al CLI (956.736/frame); 180 lotes, 361 registros, 98.824 bytes serializados, 0 pérdidas |
| Engine sin ventana | 51.609.600 bytes de vídeo prestados, no copiados; 361 eventos emitidos/retenidos, 66.604 bytes en consulta final, 0 pérdidas; 180 respaldos `no_pack` |
| Runtime con Vulkan/audio | 51.609.600 bytes transferidos al staging GPU (286.720/frame); 576.572 bytes PCM encolados al stream; mismos 361 eventos y 180 respaldos |
| FX-03 con pack y controles | 4 frames; 2 composiciones con 573.440 bytes de copia base; 2 respaldos `policy_unconfirmed`; 1.146.880 bytes al staging GPU; 6 eventos retenidos, 0 pérdidas |

Los bytes prestados son longitud de spans, no copias. `composed_copy` cuenta
la copia explícita de la imagen base cuando Engine compone sustituciones;
`gpu_staging` cuenta la transferencia de RGBA al staging del render.
No se atribuyen a estos contadores copias internas de Vulkan ni operaciones
de GPU. El volumen de eventos de Tessera suma lotes por frame; Engine consulta
un lote final, por eso sus bytes serializados difieren aunque ambos retengan
361 eventos.

Con dispositivos, los p50 de CPU por repetición fueron 19,6/20,6/16,5 µs
para espera/adquisición del swapchain, 84,5/78,8/78,1 µs para grabar comandos,
y 108,2/117,1/103,9 µs para submit/present. Son tiempos **CPU**; no miden
ejecución GPU pura. La entrega mantuvo p95 de 17,884/18,090/18,254 ms, con
180 frames, 179 intervalos y cero errores de validación por repetición.
Una corrida intermedia (`build/evidence/phase5-benchmark-final-verified/summary.json`)
tuvo p95 de 31,657 ms en su primera serie, frente a 17,900 y 18,008 ms
en las otras dos. La repetición final de identidad de configuración dio
17,831/18,244/17,787 ms. La variación queda visible y no sirve para fijar
un presupuesto a partir de una sola corrida.
La reproducción FX-02 activa entregó 144.143 frames fuente, pico PCM 1.022,
287.128 muestras activas y 295 llamadas al callback de SDL, sin episodios ni
solicitudes de datos no cubiertas durante 180 frames. Ese contador mide demanda no cubierta
en el stream; no acredita cortes audibles reales ni el reloj del hardware.

### Timestamps GPU y cobertura de audio, 2026-10-20

Evidencia: `build/evidence/phase5-benchmark-gpu-wait-split-retry/summary.json`.
Runtime creó un pool de consultas Vulkan cuando la familia gráfica declaró
`timestampValidBits=64`; la GPU NVIDIA informó `timestampPeriod=1 ns`. Un
timestamp `TOP_OF_PIPE`, otro en `COLOR_ATTACHMENT_OUTPUT` tras la compuerta
del semáforo de imagen y otro `BOTTOM_OF_PIPE` delimitan espera de imagen y
trabajo posterior del command buffer.
El resultado se lee tras la fence correspondiente y los dos últimos frames
se drenan fuera de las etapas temporizadas. Hubo 180 muestras por repetición,
0 errores de validación y la misma configuración/hash de ROM en las tres
rutas. Se registraron por separado espera/adquisición, grabación y submit/
present en CPU. El timestamp de presentación efectiva permanece no disponible.

| Repetición | GPU trabajo p50 / p95 / p99 / máximo (µs) | GPU espera imagen p50 / p95 / p99 / máximo (µs) | Entrega p95 (ms) |
| --- | --- | --- | --- |
| 1 | 86,112 / 474,592 / 485,728 / 503,136 | 0,128 / 0,256 / 6,368 / 8,672 | 17,799 |
| 2 | 85,376 / 635,392 / 640,192 / 640,928 | 0,128 / 0,256 / 0,256 / 0,256 | 18,330 |
| 3 | 125,088 / 954,432 / 956,928 / 974,944 | 0,128 / 0,608 / 0,608 / 0,608 | 18,118 |

Los p50 CPU de adquisición/grabación/submit fueron 32,1/92,6/145,6 µs,
21,7/76,5/106,1 µs y 21,5/76,2/82,5 µs. El tramo GPU total p50 fue
86,272/85,504/125,216 µs. Cada serie GPU tiene 180 muestras; los
percentiles excluyen los 30 frames de calentamiento.

`gpu_execute` mide del punto posterior a la espera de imagen al final del
command buffer; `gpu_image_wait` mide desde su inicio hasta ese punto, y
`gpu_command_span` conserva el total. Son tiempos de etapas Vulkan en el reloj
del dispositivo, no FPS ni tiempo acústico; el trabajo puede solaparse con
CPU y no se suman sus percentiles. Los altos percentiles de trabajo se
conservan y aún no justifican un presupuesto. Un intento previo falló en la
tercera repetición por `VK_ERROR_OUT_OF_DATE_KHR` al presentar el primer frame
(`build/evidence/phase5-benchmark-gpu-wait-split/devices-2.log`); el runner lo
rechazó y la repetición completa quedó registrada por separado.
El método sigue la [guía oficial de profiling de Vulkan](https://docs.vulkan.org/guide/latest/profiling.html).

En las tres repeticiones SDL llamó 295/295/296 veces al callback de audio,
con 0 episodios y 0 muestras demandadas sin disponibilidad en el stream.
Así queda medida la definición aprobada en el límite Runtime→SDL; el conteo
de bytes solicitado por SDL puede variar con buffering y remuestreo según
su [contrato de callback](https://wiki.libsdl.org/SDL3/SDL_AudioStreamCallback).
No existe en esta ruta un contador de cortes acústicos físicos del altavoz.
La aceptación final sigue pendiente de la CI y de la consolidación completa
de T032; no se afirma una mejora frente a otro emulador.

Para repetir localmente, ejecutar `tools/generate-native-fixtures.ps1` con el
build de Tessera y `make_test_pack.exe` de Engine, y luego ejecutar
`build/windows-dev/tests/tessera_performance_benchmark.exe` con
CLI de Tessera, Runtime local, ROM, directorio de salida y `--devices`.
Para incluir asignaciones, configurar Tessera con
`-DTESSERA_ALLOCATION_METRICS=ON` y Runtime con
`-DAYTHER_NATIVE_ALLOCATION_METRICS=ON`, instalar Tessera y reenlazar
Runtime contra ese mismo paquete antes del benchmark. El JSON conserva las
180 muestras de llamadas y bytes por repetición, incluidas las 30 iniciales.
La ROM FX-02 está en `build/windows-dev/native-fixtures/fx02/program.md`;
ninguna ROM o pack generados se incluyen en Git o artifacts de CI.
Requiere los paquetes locales de Engine y Tessera instalados antes de
configurar Runtime.

### Comparación progresiva FX-03, 2026-10-20

Evidencia local: `build/evidence/phase5-progressive-fx03/summary.json` (ignorada
por Git). Una ROM FX-03, configuración y traza neutral de 180 frames se usaron
en todos los modos, con tres repeticiones secuenciales y 30 frames de
calentamiento excluidos. SHA-256: ROM
`92478b869fc61dcd7a8d457b52ac6e08661c27956c774babf4768f32497d1077`,
configuración `077fe4f966c6eaa21d41de33cec512c4ddeb3a68ab274ff3cf3d9a2a9e4d3a14`,
traza `fdbab19a72919f981e0a36013e1761d7e3b55fb5bf5c454932ca8b0696ed6301`,
pack `669550f27ed28b2897265a15828987a968335dc6e2d69e4a83f3252928199abf`.
Mismo equipo y toolchain indicados arriba. Binarios SHA-256: CLI
`e1655b39fa5984364991d1b87c32cebda2464b1a5b0d2c269a77dc3ac338e450`,
Engine instalado
`eca7603dd309e196b72e24002dd83de1f9fef3cd7d5f3e016f38d75cd36a0258`,
Runtime `2e57617096c90356596fd0218b58e18a461119150b382c27ee2b03863ee18001`.

| Etapa | `step` p50, µs (tres series) | `engine_frame` p50, µs | Hash p50, µs |
| --- | ---: | ---: | ---: |
| Tessera sin observación | 1698,8 / 1613,6 / 1730,2 | — | 26785 / 25443 / 27344 |
| Tessera con snapshot/eventos | 3159,7 / 3076,3 / 3184,5 | — | 27224 / 24598 / 27385 |
| Engine, máscara de observación 0 tras frame 1 | 1474,5 / 1482,8 / 1501,3 | 0,7 / 0,7 / 0,7 | 584 / 587 / 585 |
| Engine, máscara 31 | 3011,4 / 3106,8 / 2962,5 | 1,4 / 1,4 / 1,5 | 622 / 632 / 629 |
| Engine, máscara 31 y pack | 2973,5 / 3035,3 / 2927,0 | 85,2 / 89,7 / 84,4 | 637 / 632 / 632 |
| Runtime, pack y dispositivos | 3131,2 / 3196,5 / 3231,2 | 132,9 / 130,8 / 126,5 | 609 / 612 / 614 |

`step` termina antes de `native_frame()`. Los 85–90 µs de composición p50
aparecen en `engine_frame`, no en `engine_step`; la entrega Vulkan/SDL añade
`runtime_device` p50 de 288/282/272 µs. El hash de Tessera exporta y digiere
estado, vídeo y audio canónicos; el de Runtime digiere vídeo original,
presentado y PCM mediante FNV. Por esa diferencia de trabajo y algoritmo,
sus tiempos de hash no son comparables entre sí. Runtime mantuvo el hash activo
en todas las etapas y lo midió aparte: la serie mínima aísla el costo de
avance con observación desactivada, pero no es una ejecución completa sin
hash. El control inicial se confirmó en el frame 1 y se desactivó en el 2,
dentro del calentamiento. Carga de ROM/pack, apertura de dispositivos, guardado
y pacing quedan fuera de los tramos CPU estables.

La comparación valida los 180 hashes canónicos de Tessera entre observación
apagada/encendida y los 180 hashes originales de vídeo/PCM de Engine entre
máscara 0, máscara 31 y composición; los 180 hashes original/presentado/PCM
de Runtime con dispositivos coinciden con Engine con pack. La configuración
coincide en todas las rutas. Los formatos SHA-256 canónico y FNV sobre bytes
originales son distintos; la comprobación cruzada directa de un digest único
entre CLI y Runtime queda fuera de esta serie. La composición se produjo en
180/180 frames por repetición, con 51.609.600 bytes de copia base; sin pack
hubo 180 respaldos. Observación completa: 180 snapshots, 361 eventos y cero
pérdidas por repetición; Engine retuvo los mismos 361 eventos. En Runtime se
transfirieron 51.609.600 bytes al staging GPU y 576.572 bytes PCM al stream.
Los contadores `new` fueron 2/frame en Engine mínimo, ~196/frame con capturas,
~205/frame con composición y ~210/frame con dispositivos; son asignaciones
solicitadas, no memoria viva. El pico de proceso pasó de ~22,1 MiB en Engine
mínimo a ~24,4 MiB con pack y ~244–283 MiB con dispositivos.

La GPU informó trabajo p50 86,112/85,888/86,272 µs, espera de imagen p50
0,128 µs y entrega p95 17,969/17,883/18,036 ms; hubo cero errores de
validación. Audio SDL informó cero episodios y cero muestras demandadas sin
disponibilidad. Estas series de GPU, espera, entrega y audio conservan los
límites de interpretación descritos arriba; no se suman percentiles ni se
deriva todavía un presupuesto contractual. Durante el primer ensayo, replay
por traza evitaba confirmar la política de Engine y produjo 180 respaldos
`policy_unconfirmed`; se corrigió mediante `AytherSession::native_replay_next`
y la serie publicada arriba es la repetición completa posterior a la
corrección. La comparación no acredita aún CI ni aceptación final.

### Candidatos empaquetados y dispositivo, 2026-10-20

Se repitió el recorrido sobre ZIP locales extraídos, sin rutas de biblioteca a
los árboles de fuente: Tessera `d074cb4` (SHA-256 del ZIP
`42b8870ab8d83744c895f6d5cef5ab3d98b36b5bd74c135c8d0520b1af12b907`),
Engine `3deb4c2` compilado con Tessera activa (ZIP
`5bd460bfe713ac5b3b86b9ebf3caef564ee5f9930dd8d33c69d47f2880808c3e`)
y Runtime `a1fc482` (ejecutable
`3361fe5ca12e514069e0960a13ddfd81c0a91c86086107d86ee06b9c48edc5b9`).
El consumidor externo de Engine compiló y ejecutó desde ambos prefijos
extraídos. Runtime pasó 230/230 CTest en esta configuración, incluidos los
tres tests Tessera. Ambos ZIP son reproducibles byte por byte en dos pasadas;
ninguno contiene ROM, pack, registro de confianza ni generador compilado.

En la NVIDIA GeForce RTX 3060 Laptop GPU, con 180 frames regulados y 30 de
calentamiento, FX-03 dio 179 intervalos de entrega: p50/p95/p99/máximo
16,708/18,105/18,593/19,059 ms; `gpu_execute` p50/p95/p99
95,648/464,288/467,808 µs; `runtime_device` p50/p95/p99
333,2/632,1/1.188,6 µs. Hubo cero errores Vulkan. SDL recibió 144.143
frames de origen, 296 callbacks y cero episodios o muestras faltantes, con
silencio previsto (`active_samples=0`). Con FX-02 y entrada activa, la misma
ruta dio entrega p50/p95/p99/máximo 16,734/18,002/18,587/18,779 ms,
`gpu_execute` 86,688/465,344/469,728 µs y `runtime_device`
323,7/686,6/956,8 µs. El stream recibió 144.143 frames de origen, pico PCM
1.022, 287.128 muestras activas, 296 callbacks y cero episodios o muestras
faltantes. Ambas pasadas obtuvieron 180 timestamps de comandos GPU con
`valid_bits=64` y `period_ns=1`; los timestamps efectivos de presentación
permanecen indisponibles. Estas cifras son una observación de una pasada por
fixture sobre paquetes locales: no sustituyen las tres repeticiones de la
comparación progresiva ni demuestran ausencia de cortes audibles físicos.
