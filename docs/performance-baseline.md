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
frames; el campo `missing_samples` suma **solicitudes estimadas de relleno**,
no mide interrupciones audibles ni muestras realmente perdidas por el hardware.
La prueba adicional FX-02 con B pulsado entregó PCM
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
| Tessera sin observación | 1750,3 / 1712,9 / 1769,5 | — | 26964 / 26638 / 27511 |
| Tessera con snapshot/eventos | 3141,1 / 3198,2 / 3086,4 | — | 25892 / 26192 / 26158 |
| Engine, máscara de observación 0 tras frame 1 | 1474,5 / 1482,8 / 1501,3 | 0,7 / 0,7 / 0,7 | 584 / 587 / 585 |
| Engine, máscara 31 | 3011,4 / 3106,8 / 2962,5 | 1,4 / 1,4 / 1,5 | 622 / 632 / 629 |
| Engine, máscara 31 y pack | 2973,5 / 3035,3 / 2927,0 | 85,2 / 89,7 / 84,4 | 637 / 632 / 632 |
| Runtime, pack y dispositivos | 3131,2 / 3196,5 / 3231,2 | 132,9 / 130,8 / 126,5 | 609 / 612 / 614 |

Los p95/p99 de `step` o `engine_step` (µs), por repetición, se conservaron
en el mismo `summary.json`:

| Modo | p95, repeticiones 1/2/3 | p99, repeticiones 1/2/3 |
| --- | ---: | ---: |
| Tessera sin observación | 2409,3 / 2148,5 / 2566,7 | 3170,9 / 2828,0 / 3707,8 |
| Tessera con snapshot/eventos | 3963,4 / 5577,6 / 3646,2 | 8424,0 / 8231,7 / 7639,6 |
| Engine, máscara 0 | 1778,9 / 1829,5 / 1808,3 | 2166,3 / 2310,9 / 1982,1 |
| Engine, máscara 31 | 3464,8 / 3579,5 / 3742,5 | 3693,2 / 3752,6 / 4317,8 |
| Engine, máscara 31 y pack | 3404,1 / 3357,2 / 3308,4 | 3521,2 / 3664,1 / 3453,4 |
| Runtime, pack y dispositivos | 3539,4 / 3597,5 / 3623,4 | 4008,6 / 3788,7 / 3780,2 |

Para la composición `engine_frame`, p95/p99 fueron
141,7/202,6; 151,9/184,2; 156,0/277,0 µs. Con dispositivos,
`engine_frame` dio 176,8/237,8; 175,5/253,9; 160,4/183,8 µs, y
`runtime_device` 431,8/800,4; 345,2/660,9; 384,4/569,1 µs.
La dispersión de p99, especialmente en Tessera autónoma con observación,
desaconseja presentar un presupuesto estricto a partir de solo tres series.
Los demás tramos instrumentados conservan en el JSON las tres series completas.
Como envolvente entre repeticiones, `canonical_hashes` p95/p99 fue
35.446–37.357/41.131–46.042 µs sin observación y
33.222–35.600/37.321–40.107 µs con ella; `observation` p95/p99 con
capturas fue 572–767/627–1.490 µs. `runtime_hash` p95/p99 quedó en
717–733/783–938 µs con Engine mínimo, 728–815/876–999 µs con
observación, 710–732/803–985 µs con composición y 716–730/801–909 µs
con Runtime. `gpu_execute` p95/p99 dio 307–310/311–454 µs;
`gpu_image_wait` 0,2/0,2–8,4 µs. Los rangos son mínimos y máximos de
percentiles de cada repetición, no percentiles calculados sobre muestras
agregadas; no se suman tramos solapados.

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

### Comparación progresiva del candidato empaquetado

Se regeneró `build/evidence/phase5-progressive-fx03-packaged-d074/summary.json`
con CLI Tessera Release `d074cb4`, Runtime `a1fc482` enlazado a los ZIP
extraídos y la ROM FX-03 de SHA-256
`92478b869fc61dcd7a8d457b52ac6e08661c27956c774babf4768f32497d1077`.
Son tres repeticiones secuenciales de 180 frames, con 30 de calentamiento
excluidos. Cada celda contiene p50/p95/p99 de avance, en µs, para las
repeticiones 1/2/3. La comparación con la serie anterior no atribuye la
diferencia a la integración: cambió la configuración de build de Tessera.

| Modo | Repetición 1 | Repetición 2 | Repetición 3 |
| --- | ---: | ---: | ---: |
| Tessera sin observación | 1048/1307/1768 | 993/1321/1634 | 1086/1275/1381 |
| Tessera con observación | 2422/2865/3002 | 2105/2600/3165 | 2434/2869/3283 |
| Engine mínimo | 893/1123/1478 | 914/1055/1141 | 809/1032/1156 |
| Engine con observación | 2296/2808/3095 | 2245/2913/2983 | 2239/2527/2624 |
| Engine con composición | 2252/2901/3489 | 2087/2679/2958 | 2520/3008/3220 |
| Runtime con dispositivos | 2447/2805/2955 | 2560/3147/3377 | 2189/2577/2640 |

Las demás etapas instrumentadas se resumen como mínimo–máximo entre los
percentiles de las tres repeticiones, en µs. Cada etapa conserva sus 150
muestras posteriores al calentamiento en los informes de evidencia locales.

| Modo y etapa | p50 | p95 | p99 |
| --- | ---: | ---: | ---: |
| Tessera mínima, hash canónico | 24.199–25.873 | 27.036–28.712 | 27.498–30.561 |
| Tessera observada, hash canónico | 22.296–25.506 | 26.813–28.457 | 28.229–29.341 |
| Tessera observada, capturas/eventos | 375–426 | 560–613 | 630–715 |
| Engine mínimo, hash Runtime | 524–610 | 642–695 | 725–741 |
| Engine observado, hash Runtime | 599–626 | 757–766 | 971–1.180 |
| Engine compuesto, hash Runtime | 557–667 | 702–809 | 873–942 |
| Engine compuesto, `engine_frame` | 73–89 | 139–160 | 164–278 |
| Runtime, hash | 555–606 | 690–818 | 727–901 |
| Runtime, `engine_frame` | 113–126 | 163–232 | 210–309 |
| Runtime, `runtime_device` | 289–320 | 411–629 | 765–940 |
| Runtime, `gpu_execute` | 86,112–97,152 | 636,480–952,896 | 642,464–956,928 |
| Runtime, `gpu_image_wait` | 0,128 | 0,256–0,576 | 0,288–3,808 |

La tabla expresa rangos de percentiles por repetición, no percentiles de
muestras agregadas. Los hashes canónicos de Tessera y los hashes FNV de Runtime
son trabajos diferentes; tampoco se suman etapas CPU/GPU solapadas. El benchmark comprobó que
los 180 hashes de vídeo original y PCM de Engine coinciden entre los modos
mínimo, observado, compuesto y Runtime; los 180 hashes presentados de la
composición coinciden con Runtime en las tres repeticiones. Cada ejecución
conservó 180 composiciones, 361 eventos emitidos/retenidos, cero pérdidas y
cero episodios/muestras faltantes del stream SDL por repetición; FX-03 mantiene
silencio previsto. La entrega al presentador, en ms y con 179 intervalos y
cero errores Vulkan por repetición, fue:

| Repetición | p50 | p95 | p99 | Máximo | Callbacks SDL |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 16,688 | 17,906 | 18,203 | 18,406 | 295 |
| 2 | 16,725 | 17,730 | 18,205 | 18,448 | 295 |
| 3 | 16,619 | 17,908 | 18,223 | 18,589 | 296 |

Los timestamps efectivos de presentación continúan indisponibles.

### Presupuestos locales derivados para la siguiente validación

Los siguientes límites de aviso se derivan de la mayor observación entre las
tres repeticiones del candidato `d074cb4` en este PC, con aproximadamente 25 %
de margen y redondeo hacia arriba. Se aplican solo a una repetición del mismo
perfil, hardware, toolchain, contenido y ventana de 150 frames estables; una
medición en otro equipo requiere su propia línea base. No se suman etapas
solapadas. El margen no convierte tres series en garantía estadística.

| Serie | Mayor valor observado | Límite de aviso local |
| --- | ---: | ---: |
| Tessera observada, avance p99 | 3,283 ms | 4,2 ms |
| Engine observado, avance p99 | 3,095 ms | 3,9 ms |
| Engine compuesto, `engine_frame` p99 | 0,278 ms | 0,35 ms |
| Runtime, avance p99 | 3,377 ms | 4,3 ms |
| Runtime, `runtime_device` p99 | 0,940 ms | 1,2 ms |
| Runtime, `gpu_execute` p99 | 0,957 ms | 1,2 ms |
| Entrega al presentador p99 | 18,223 ms | 22,8 ms |
| Intervalo máximo de entrega | 18,589 ms | 23,3 ms |
| Memoria máxima de Runtime | 281,4 MiB | 352 MiB |

La pérdida de eventos y las muestras faltantes del stream SDL tuvieron base
cero; una observación positiva exige investigar el evento, sin tolerancia
porcentual calculada a partir de cero. Los 180 respaldos fueron la consecuencia
prevista del fixture, no un objetivo de reducción. La presentación efectiva y
la salida acústica física no aportan una serie observable en este hardware;
estos límites se refieren a entrega, comandos GPU y demanda del stream.

**Comprobación con el ZIP revisado `de825e5` y Engine `fb3d9a8`:** se ejecutaron
dos series independientes de 3×180 frames (30 de calentamiento), ambas con
hashes originales y presentados equivalentes entre etapas, 180 composiciones,
361 eventos y cero pérdidas por repetición. Los informes locales están en
`build/evidence/phase5-progressive-fx03-de825/summary.json` y
`build/evidence/phase5-progressive-fx03-de825-repeat/summary.json`. La primera
serie registró p99 de entrega 18,289/33,463/18,865 ms; en la segunda,
18,470/18,254/18,536 ms. En la repetición central de la primera serie hubo
un intervalo máximo de 49,140 ms y un callback SDL que pidió 337 muestras
adicionales (7,021 ms a 48 kHz). El p99 CPU de `gpu_acquire_cpu` subió allí a
36.267,3 µs, frente a 57,9 y 85,0 µs en las otras dos repeticiones. En la
segunda serie, las seis cifras comparables de audio y entrega no reprodujeron
el incidente: tres repeticiones con cero demanda adicional y p99 de adquisición
de 93,3/66,2/82,0 µs. Son medidas agregadas de la misma repetición, sin
marcas temporales que prueben simultaneidad. `gpu_acquire_cpu` engloba
`vkWaitForFences`, `vkAcquireNextImageKHR` y el inicio del comando; no permite
atribuir el pico a una de esas llamadas. El
límite local de aviso de entrega y el de audio se activaron: no se descarta el
episodio por la repetición favorable ni se extrapola una tasa de fallos desde
seis observaciones.
El callback SDL precede a la obtención de datos y `additional_amount` expresa
bytes que el stream quisiera añadir, con posible sobreestimación por buffering
o remuestreo; no es un contador de PCM efectivamente perdido. El campo
`missing_samples` del informe conserva su nombre heredado, pero aquí solo
representa **demanda adicional estimada**. FX-03 generó silencio previsto
(`source_peak=0`, `active_samples=0`), por lo que tampoco es evidencia de un
corte acústico audible. Queda por caracterizar la cola de presentación de esa
repetición y ejecutar el perfil físico de aceptación en CI. T032 no puede
usar este contador como oráculo definitivo de cortes: requiere una medición
que distinga la solicitud de relleno de PCM no entregado a tiempo. El informe
actual tampoco identifica el frame o instante del callback positivo.
El pico de memoria por proceso fue 16,3–16,9 MiB en Tessera mínima,
21,6–21,7 MiB con observación, 22,0–22,1 MiB en Engine mínimo,
24,1–24,4 MiB con composición y 242,7–281,4 MiB en Runtime con dispositivos.
Los contadores de `new` del ejecutable dieron 2 solicitudes/frame en Engine
mínimo, 196 con observación, 205 con composición y ~210 con Runtime. El CLI
Release de esta serie no incorporó el contador de asignaciones: sus campos
vacíos no significan cero asignaciones y se usa para esa cobertura la serie
instrumentada anterior. Tampoco se atribuye a `new` la memoria interna de SDL,
Vulkan o bibliotecas con otro asignador.
La traza nueva,
SHA-256 `5743f98501e277590f703a33bbe2426aca3a1bcda463918ffec9f24cc7ee9c28`,
permitió además pedir 181 frames al Runtime: emitió `AYTHER_NATIVE_EOF frame=180`
y `AYTHER_NATIVE_DONE frames=180`. Con pack activo, un control de desactivación
solicitado para el frame 181 no produjo frame ni respaldo adicional
(`composed_frames=180`, `fallback_frames=0`). Ninguna traza o fixture generado
se publica en Git ni en artifacts de CI.

### Comparación con los paquetes públicos exactos

Se repitió la comparación FX-03 en dos series de 3×180 frames, con 30 frames
de calentamiento por repetición. Evidencia local ignorada por Git:
`build/evidence/phase5-progressive-public-pair-linked/summary.json` y
`build/evidence/phase5-progressive-public-pair-repeat/summary.json`. El primer
intento con un runner enlazado al core de desarrollo fue rechazado por identidad
de backend incompatible. El runner usado en ambas series se enlazó con el
**mismo paquete público Tessera** que consumen el CLI y Engine; no se mezclaron
IDs de builds diferentes. Paquetes ZIP: Tessera `v0.1.0-mvp.2`
SHA-256 `40ae6447b3a5dd7ca4bd4aaed2fccc436118fd53bec6c97deeb6c2593bed5a8f`
y Engine `native-v0.1.0-rc.22`
SHA-256 `4f17c34cde082b39a21c4a5216d77bc53e161df896487f1f9e50ab8a3fc1bdf2`.
Binarios: CLI `6f7609c22390bab4b844d276de4e95c0650ea7b7c79110b80685506c60ac9bf8`,
Runtime `808f6f2e6742260c6727e065900da50dc767db6ce563211d1b7d662e66bad895`,
runner `39f17bf45bc42a5cd3842ef228e439c4b666fcf769d8b9e468d77f5ddffcd677`.
ROM `92478b869fc61dcd7a8d457b52ac6e08661c27956c774babf4768f32497d1077`,
pack `669550f27ed28b2897265a15828987a968335dc6e2d69e4a83f3252928199abf`,
configuración `349cf804ed83fad41e838ce385b919b097065deaa91a8af6c152240f60de5e61`
y traza `844147cc2e378444b6a75e683670b549fa97f2e132f627730f0f337d58b7c276`
fueron idénticos entre las dos series. El host Windows usó MSVC 19.51.36256
para Runtime/runner, core Tessera Clang 20.1.8, NVIDIA GeForce RTX 3060
Laptop GPU con controlador 32.0.16.1656 y dispositivo de salida SDL local.

| Etapa, unidad ms por frame estable | p50 entre seis repeticiones | p95 | p99 |
| --- | ---: | ---: | ---: |
| Tessera sin observación, avance | 1,002–1,052 | 1,271–1,637 | 2,519–5,133 |
| Tessera con observación, avance | 2,326–2,444 | 2,988–4,234 | 3,403–7,037 |
| Engine mínimo, avance | 0,783–0,952 | 0,968–1,124 | 1,068–1,710 |
| Engine observado, avance | 2,160–2,444 | 2,560–2,883 | 2,701–3,346 |
| Engine con pack, composición | 0,065–0,074 | 0,105–0,140 | 0,124–0,277 |
| Runtime, avance | 1,764–2,230 | 2,247–2,664 | 2,540–2,978 |
| Runtime, entrega CPU a dispositivo | 0,247–0,273 | 0,323–1,376 | 0,345–2,506 |
| GPU, ejecución por timestamps | 0,086–0,127 | 0,339–0,958 | 0,636–0,961 |
| CPU, adquisición de imagen | 0,022–0,029 | 0,035–1,172 | 0,044–2,242 |

La entrega monotónica al presentador dio p50 16,602–16,743 ms, p95
17,797–18,545 ms, p99 18,261–19,141 ms y máximo 18,363–24,681 ms;
dos máximos superaron 24 ms, sin elevar el p99 por encima de 20 ms. Runtime
usó 248,3–248,8 MiB de memoria máxima por proceso. Cada repetición completó
180 composiciones y 361 eventos sin pérdidas, con hashes originales y
presentados estables dentro de cada modo; observación directa no cambió sus
hashes y la presentación Runtime coincidió con la composición Engine. SDL
reportó 0 episodios y 0 muestras adicionales demandadas sin disponibilidad
en las seis repeticiones. El silencio de FX-03 es el contenido esperado; FX-02
cubre audio activo por separado. La serie de presentación efectiva sigue
`unavailable`; el callback SDL mide una aproximación de falta de datos del
stream, no cortes audibles del hardware.

Los límites de aviso de `d074cb4` anteriores pertenecen a ese binario y
perfil. En la pareja pública, los p99 de observación directa, entrega CPU y
dos máximos de intervalo mostraron colas distintas; no se interpretan como
regresión causal frente a `d074cb4` ni se oculta la demanda SDL adicional
estimada de 337 muestras en FX-03. Para una siguiente validación del **mismo par público**,
un margen provisional de aproximadamente 25 % sobre los mayores valores
medidos da avisos de 8,8 ms para p99 de Tessera observada, 4,2 ms para Engine
observado, 3,2 ms para `runtime_device`, 24,0 ms para p99 de entrega y 30,9 ms
para el intervalo máximo. Cualquier muestra adicional demandada por SDL se
investiga por tener base cero. Son avisos locales, no garantías estadísticas
ni sustitutos del perfil físico en CI.

### FX-02 con audio activo en los mismos paquetes públicos

El runner `tests/performance/benchmark.cpp` admite `--press-b` para generar
una traza completa de 180 frames con el control B mantenido. Su valor por
defecto sigue siendo una entrada neutral. Se ejecutaron dos series adicionales
de 3×180 frames con `--devices --press-b` y sin pack, excluyendo los primeros
30 frames de cada repetición. Evidencia local:
`build/evidence/phase5-progressive-fx02-public-active/summary.json` y
`build/evidence/phase5-progressive-fx02-public-active-repeat/summary.json`.
ROM FX-02 SHA-256 `5296c4fa331fd8cb47663fc7ce3b0abef03364221b309a65f2966c6a8642225c`,
configuración `349cf804ed83fad41e838ce385b919b097065deaa91a8af6c152240f60de5e61`,
traza `fb97aa4998c424297405e8903cd2e1e00e73384239ee219fa4bbcf477925113c`
y runner recompilado `c1328a35080ebd69c37aad16074b31d97723ed9d3bd6afffb373d7230d70649b`.
Los ZIP, Runtime, GPU y toolchain son los identificados en la sección anterior.

Las seis repeticiones produjeron `source_peak=1022` y 287.128 muestras
activas cada una, con hashes estables por modo, cero pérdidas de eventos y
**cero episodios/muestras adicionales demandadas sin disponibilidad** en el
stream SDL. La entrega dio p99 18,103–18,929 ms y máximo 18,511–27,683 ms;
dos máximos superaron 25 ms sin producir demanda faltante según el callback.
`runtime_device` p99 fue 0,656–1,426 ms y `gpu_execute` p99
0,324–0,489 ms. Se conserva el máximo de 27,683 ms como variación real de
entrega, sin equipararlo a una interrupción acústica. Este perfil tampoco
mide el reloj de salida ni sustituye la validación física en CI.

Una tercera serie FX-02 de 3×180 frames comprobó el nombre explícito del
indicador en el JSON (`measurement=sdl_callback_refill_request_estimate`) con
el Runtime de SHA-256 `808f6f2e6742260c6727e065900da50dc767db6ce563211d1b7d662e66bad895`
y el runner recompilado SHA-256
`2261e8bf577f6eea3482a1871e4504bb35232bec7f5f35596162922b0819faf4`.
Evidencia ignorada por Git:
`build/evidence/phase5-fx02-refill-semantics-exact/summary.json`. Conservó
configuración y traza de las dos series anteriores. Sus tres repeticiones
tuvieron 287.128 muestras PCM activas cada una, cero solicitud adicional
registrada, p99 de entrega 18,120–19,030 ms y máximos 18,540–31,680 ms.
El máximo aislado de 31,680 ms sin solicitud adicional refuerza que un
intervalo largo de entrega y el indicador SDL no son la misma medición.
