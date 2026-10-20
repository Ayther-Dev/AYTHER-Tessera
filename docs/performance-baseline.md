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
Runtime, `engine_step` rodea avance/replay y entrega de frame de Engine;
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

No se instrumentaron aún asignaciones por frame, bytes efectivamente copiados,
volumen/pérdidas de eventos del recorrido Engine, cantidad de respaldos,
tiempo GPU puro separado de espera de swapchain ni cortes exactos del
dispositivo. La comparación es una línea base local y no establece umbrales de
aceptación ni una mejora frente a otro emulador. CA-11 sigue pendiente hasta
cubrir esas métricas y confirmar la CI del perfil declarado.

Para repetir localmente, ejecutar `tools/generate-native-fixtures.ps1` con el
build de Tessera y `make_test_pack.exe` de Engine, y luego ejecutar
`build/windows-dev/tests/tessera_performance_benchmark.exe` con
CLI de Tessera, Runtime local, ROM, directorio de salida y `--devices`.
La ROM FX-02 está en `build/windows-dev/native-fixtures/fx02/program.md`;
ninguna ROM o pack generados se incluyen en Git o artifacts de CI.
Requiere los paquetes locales de Engine y Tessera instalados antes de
configurar Runtime.
