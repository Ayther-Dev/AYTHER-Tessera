# Informe de aceptación — MVP

**Corte local:** 2026-10-20. **Resultado:** aceptación final **pendiente**.
La fase 5 añadió presentación Vulkan, salida SDL al dispositivo y una línea
base comparable de tres capas. Los checks remotos de las revisiones finales,
la distribución coordinada y todas las métricas de RF-13/CA-11 aún no tienen
evidencia suficiente. Un test omitido no cuenta como aprobado.

| ID | Evidencia ejecutada o pendiente | Estado |
| --- | --- | --- |
| RF-01 | `integration.cli`, `integration.replay`, Engine nativo y Runtime headless; replay/EOF en benchmark | Local verificado; CI pendiente |
| RF-02 | Fase 3/4: 15 procesos, 18.000 frames, 4.500 continuados y oráculos FX-02 | Local verificado; CI final pendiente |
| RF-03 | `contract.native_api`, `integration.megadrive_session`, Engine backend/provider | Local verificado |
| RF-04 | `contract.snapshot`, `integration.megadrive_observation` | Local verificado |
| RF-05 | Engine `tessera_presentation_test` y Runtime `tessera_runtime_gpu_audio`; readback 1.224/0/0/1.224 píxeles magenta | Local verificado; CI GPU pendiente |
| RF-06 | `contract.snapshot`, Engine controls/presentation, Runtime confirmación 1/2/3 | Local verificado; negativos Runtime adicionales pendientes |
| RF-07 | Engine controls/presentation y checkpoint; respaldo/rearme locales | Local verificado; CI final pendiente |
| RF-08 | Engine pack/presentation: manifiesto, derechos, hashes, rutas y límites | Local verificado |
| RF-09 | `unit.state_format`, `integration.state_store`, `integration.checkpoint` y replay de continuaciones | Local verificado; revisión final T033 pendiente |
| RF-10 | `contract.snapshot`, `integration.megadrive_observation`, `integration.robustness` | Local verificado; volumen/pérdidas en benchmark pendientes |
| RF-11 | `unit.system_detector`, Engine provider, Runtime proveedor explícito | Local verificado |
| RF-12 | Engine→Tessera y Runtime→Engine→Tessera por API nativa; CTest Runtime 4/4 | Local verificado; CI encadenada pendiente |
| RF-13 | [Línea base](../../docs/performance-baseline.md): 3 repeticiones, p50/p95/p99, memoria y entrega; faltan asignaciones/copias/eventos/respaldo y GPU/espera separados | Pendiente |
| RC-01 | Stack fijado, `legal/DEPENDENCIES.csv`, builds locales | Local verificado; revisión final pendiente |
| RC-02 | Núcleo y suite sin Engine/UI/GPU: CTest 16/16 breves | Local verificado |
| RC-03 | Contratos, unidad, replay, robustez y GPU/audio locales; Actions de estas revisiones no ejecutado | Pendiente |
| RC-04 | Código nuevo en inglés, mensajes/documentos en español; copia externa conservada; revisión de idioma final pendiente | Pendiente |
| RC-05 | `package.installed_consumer` pasó; paquetes Engine/Runtime y avisos de todas las copias pendientes | Pendiente |
| CA-01 | FX-01/02/03 presentes y con manifiestos, fuentes/generadores y hashes | Local verificado; entrega pendiente |
| CA-02 | Replay completo de fases 1/3/4 y oráculos independientes, dos procesos | Local verificado; CI final pendiente |
| CA-03 | Checkpoints 500/1.500/2.500, continuación K+1 y errores E/S de fase 3 | Local verificado; consolidación T033 pendiente |
| CA-04 | Dos lectores, retención, observación activada/inactiva y misma salida | Local verificado |
| CA-05 | FX-03 con precedencia/solapamientos Engine; Runtime Vulkan con readback y respaldo | Local verificado; CI GPU pendiente |
| CA-06 | FX-02 oráculos de silencio→PSG→silencio y Runtime audio activo 180 frames | Local verificado |
| CA-07 | Negativos de estados/pack/controles/EOF en fases 1–4 y `integration.robustness`; Runtime añadió entrada inválida, control fuera de orden y pack ausente sin avanzar frame. Matriz final T033 abierta | Pendiente |
| CA-08 | Consumidor Tessera limpio pasó; CI y cadena de paquetes Engine→Runtime sin publicar | Pendiente |
| CA-09 | Detección/override/ambigüedad y despacho registrados en pruebas de fases 1–3 | Local verificado |
| CA-10 | Pruebas Engine y Runtime locales, GPU FX-03 y audio FX-02; mismo backend nativo | Local verificado; CI final pendiente |
| CA-11 | Línea base parcial con tres repeticiones y puntos/origen declarados; faltan métricas exactas y CI | Pendiente |

**Fixtures:** las fuentes Engine/Lab copiadas permanecen intactas. La ROM
canónica y FX-03 se generan desde fuente MPL bajo `build/`; FX-02 se genera
desde fuente MIT. Los SHA-256 coinciden con los manifiestos. ROM, pack y
registro de confianza de Engine, y la ROM FX-03 derivada, se excluyeron del
índice Git; Runtime consume las rutas generadas. Falta limpiar esas copias de
la historia de las ramas antes de cualquier push o PR.
`tessera_runtime_gpu_audio` verificó cuatro frames de readback (pack activo,
desactivado y reactivado), 90 entregas reguladas y 89 intervalos. La GPU fue
NVIDIA GeForce RTX 3060 Laptop GPU, controlador 32.0.16.1656. El test
`tessera_runtime_active_audio` validó PCM no silencioso; el ensayo manual de
300 frames registró `source_peak=1022` y 478.628 muestras activas. Los
contadores SDL de demanda no prueban por sí solos ausencia de cortes audibles.

**Comandos comprobados:** `ctest --test-dir build/windows-dev --output-on-failure
-E '^integration.megadrive_replay$' -j 2` (16/16); replay largo repetido el
2026-10-20 con ROM generada en `build/` (15 procesos, 18.000 frames completos,
4.500 continuados, cero divergencias; evidencia local
`build/evidence/phase3-RelWithDebInfo-OFF/t021-44556-404038921/summary.json`);
`ctest --test-dir C:/ayther/AYTHER-Engine/build/windows-native -C RelWithDebInfo
-R '^ayther\\.integration\\.tessera_(backend_test|provider_test|pack_test|presentation_test|controls_test)$'`
(5/5); `ctest --test-dir C:/ayther/AYTHER-Runtime/out/build/tessera-local
-C RelWithDebInfo -R '^(runtime_options|tessera_runtime_.*)$'`
(4/4); `tessera_performance_benchmark.exe` con `--devices` (3 repeticiones,
`summary.json` local). Revisión de código de la medida en
`tests/performance/benchmark.cpp`; binarios y revisiones en la línea base.

**Diez grupos de QA preservados:** reinicio y estado inicial; frames/traza/EOF;
entradas completas y rechazos; transacción pendiente y cancelaciones; captura
inmutable/retención; checkpoint y publicación atómica; detección por contenido;
procedencia/precedencia gráfica y respaldo; fixture de audio independiente;
métricas con punto/origen/cobertura y alcance de idioma externo. Los casos
negativos se asignan a las suites citadas arriba y a T033/T031 donde continúan
abiertos.

**Gate de cierre:** ejecutar T030–T035 sin omisiones, limpiar de la historia
publicable los binarios excluidos por decisión del usuario, publicar la cadena de
artefactos verificables Tessera→Engine→Runtime, actualizar el lock real y
obtener checks obligatorios de los tres PR mínimos. Después repetir esta matriz
con SHAs de commit, URLs de CI y resultados sin pendientes. Hasta entonces
T036 no se marca completada y el MVP no se declara aceptado.
