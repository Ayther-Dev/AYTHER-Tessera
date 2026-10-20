# Informe de aceptación — MVP

**Corte local:** 2026-10-20. **Resultado:** aceptación final **pendiente**.
La fase 5 añadió presentación Vulkan, salida SDL al dispositivo y una línea
base comparable de tres capas. La ampliación local añadió asignaciones,
copias explícitas, eventos, respaldos, etapas CPU de presentación y una
comparación progresiva FX-03 con hashes por frame. Los checks de código de
las tres revisiones están verdes y los dos paquetes de software se publicaron
y consumieron desde URL pública. El perfil físico GPU/audio en CI sigue
pendiente. Un test omitido no cuenta como aprobado.

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
| RF-09 | `unit.state_format`, `integration.state_store`, `integration.checkpoint` y replay de continuaciones; matriz T033 revalidada en `windows-dev` y ASan | Local verificado; CI final pendiente |
| RF-10 | `contract.snapshot`, `integration.megadrive_observation`, `integration.robustness`; benchmark: 361 eventos retenidos y 0 pérdidas | Local verificado; CI final pendiente |
| RF-11 | `unit.system_detector`, Engine provider, Runtime proveedor explícito | Local verificado |
| RF-12 | Engine→Tessera y Runtime→Engine→Tessera por API nativa; Runtime 233/233 CTest locales con paquetes públicos | Local verificado; CI física pendiente |
| RF-13 | [Línea base](../../docs/performance-baseline.md): asignaciones C++ por frame, copias explícitas, eventos, respaldos, memoria, entrega, etapas CPU, espera de imagen y trabajo GPU separados; comparación progresiva FX-03 con hashes originales por etapa y composición 180/180. Una de seis repeticiones tuvo 337 muestras faltantes y espera GPU elevada; la presentación efectiva física no expone timestamps. | Local parcial; incidente y CI pendiente |
| RC-01 | Stack fijado, `legal/DEPENDENCIES.csv`, builds locales | Local verificado; revisión final pendiente |
| RC-02 | Núcleo y suite sin Engine/UI/GPU: CTest 16/16 breves | Local verificado |
| RC-03 | Contratos, unidad, replay y robustez en CI de código; GPU/audio locales 233/233 Runtime | Pendiente del perfil físico CI |
| RC-04 | Código propio nuevo en inglés, mensajes/documentos en español; fuentes externas intactas con idioma, avisos y procedencia | Local verificado en T034 |
| RC-05 | ZIP públicos Tessera/Engine, fuente y avisos, hashes/atestaciones, consumidor externo y Runtime desde prefijos públicos | Local verificado |
| CA-01 | FX-01/02/03 presentes y con manifiestos, fuentes/generadores y hashes | Local verificado; entrega pendiente |
| CA-02 | Replay completo de fases 1/3/4 y oráculos independientes, dos procesos | Local verificado; CI final pendiente |
| CA-03 | Checkpoints 500/1.500/2.500, continuación K+1 y errores E/S de fase 3; consolidación T033 6/6 local y 4/4 ASan | Local verificado; CI pendiente |
| CA-04 | Dos lectores, retención, observación activada/inactiva y misma salida | Local verificado |
| CA-05 | FX-03 con precedencia/solapamientos Engine; Runtime Vulkan con readback y respaldo | Local verificado; CI GPU pendiente |
| CA-06 | FX-02 oráculos de silencio→PSG→silencio y Runtime audio activo 180 frames | Local verificado |
| CA-07 | Negativos de estados/pack/controles/EOF en fases 1–4 y `integration.robustness`; Runtime añadió entrada inválida, control fuera de orden y pack ausente sin avanzar frame. Matriz T033 cerrada localmente (6/6 y 4/4 ASan); falta CI | Pendiente |
| CA-08 | Ambos paquetes de software publicados y verificados desde URL pública; checks de código verdes y Runtime 233/233 local | Pendiente del perfil físico CI |
| CA-09 | Detección/override/ambigüedad y despacho registrados en pruebas de fases 1–3 | Local verificado |
| CA-10 | Pruebas Engine y Runtime locales, GPU FX-03 y audio FX-02; mismo backend nativo | Local verificado; CI final pendiente |
| CA-11 | Dos series de 3×180 frames progresivos con hashes conservados, composición real y presupuestos locales derivados de la base. Un episodio de 337 muestras faltantes y p99 de entrega 33,463 ms activó avisos; la causa y el perfil físico en CI siguen pendientes. | Pendiente |

**Fixtures:** las fuentes Engine/Lab copiadas permanecen intactas. La ROM
canónica y FX-03 se generan desde fuente MPL bajo `build/`; FX-02 se genera
desde fuente MIT. Los SHA-256 coinciden con los manifiestos. ROM, pack y
registro de confianza de Engine, y la ROM FX-03 derivada, se excluyeron de las
ramas publicables (Tessera desde `1f9389d` y Runtime desde `91b85da`, ambas
basadas en `main`). `git rev-list --objects main..HEAD` no devuelve esas rutas.
Runtime consume las rutas generadas. Las ramas históricas quedan solo locales.
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
`summary.json` local); benchmark progresivo FX-03 en
`build/evidence/phase5-progressive-fx03/summary.json` (3×180 frames, 30 de
calentamiento, 180 composiciones por serie, hashes por frame conservados) y
FX-02 sin pack en `build/evidence/phase5-progressive-fx02/summary.json`.
La regresión `ayther.integration.tessera_session_test` confirmó la política
durante replay por traza (1/1); Engine controls/presentation (2/2) y Runtime
options/headless/audio/GPU (4/4) pasaron tras la corrección. Revisión de código de la medida en
`tests/performance/benchmark.cpp`; binarios y revisiones en la línea base.
La consolidación T033 pasó 6/6 en `build/windows-dev`, 4/4 con AddressSanitizer
en `build/windows-diagnostics` y el caso de paquetes mutados de Engine (1/1).
`ctest --test-dir build/windows-dev --output-on-failure -R
'^package\.installed_consumer$'` pasó 1/1: instaló en prefijo aislado,
reubicó el paquete y compiló consumidores C11/C++20 fuera del árbol.

**Diez grupos de QA preservados:** reinicio y estado inicial; frames/traza/EOF;
entradas completas y rechazos; transacción pendiente y cancelaciones; captura
inmutable/retención; checkpoint y publicación atómica; detección por contenido;
procedencia/precedencia gráfica y respaldo; fixture de audio independiente;
métricas con punto/origen/cobertura y alcance de idioma externo. Los casos
negativos se asignan a las suites citadas arriba y a T031 donde continúan
abiertos.

**Gate de cierre:** ejecutar T030–T035 sin omisiones, publicar la cadena de
artefactos verificables Tessera→Engine→Runtime, actualizar el lock real y
obtener checks obligatorios de los tres PR mínimos. Después repetir esta matriz
con SHAs de commit, URLs de CI y resultados sin pendientes. Hasta entonces
T036 no se marca completada y el MVP no se declara aceptado.

**Corte de revisión 2026-10-20:** hay un PR borrador por repositorio:
[Tessera #1](https://github.com/Ayther-Dev/AYTHER-Tessera/pull/1),
[Engine #48](https://github.com/Ayther-Dev/AYTHER-Engine/pull/48) y
[Runtime #55](https://github.com/Ayther-Dev/AYTHER-Runtime/pull/55).
El [Core checks de Tessera](https://github.com/Ayther-Dev/AYTHER-Tessera/actions/runs/39434356326)
pasó sobre `2549a0b`; Engine fija esa revisión. Runtime pasó 230/230 CTest
locales tras recompilar con el commit `a1fc482`. Los checks de Runtime
y Engine `3deb4c2` pasaron, incluido el [Required CI gate](https://github.com/Ayther-Dev/AYTHER-Engine/actions/runs/39440336838).
La cobertura C++ remota dio 82,39 % total y 79,42 % en líneas cambiadas,
sin reducir umbrales. Falta paquete Engine publicado con su lock Runtime
actualizado, runner GPU/audio físico y la verificación del asset final en T035. Ninguno de estos avances
equivale a aceptación CA-08 o CA-11.

La [comparación progresiva](../../docs/performance-baseline.md) usó FX-03 en
3×180 frames, 30 de calentamiento por serie: Engine mínimo p50 `step`
1.474,5/1.482,8/1.501,3 µs; con observación completa
3.011,4/3.106,8/2.962,5 µs; la composición añadió p50
85,2/89,7/84,4 µs en `engine_frame`. Runtime con dispositivos midió
`engine_step` 3.131,2/3.196,5/3.231,2 µs, `engine_frame`
132,9/130,8/126,5 µs y `runtime_device` 288/282/272 µs. Los hashes
originales coincidieron en las rutas comparables y no hubo pérdidas de eventos
ni solicitudes SDL sin datos; la presentación efectiva y los cortes acústicos
físicos no están medidos. CA-11 permanece pendiente.

**Evidencia adicional sobre candidatos locales extraídos (2026-10-20):**
Tessera `d074cb4` y Engine `3deb4c2` generaron ZIP reproducibles y libres de
ROM/pack/trust/generador; Engine activó Tessera nativa, pasó su replay de
36.000 frames y un consumidor externo compiló y ejecutó. Runtime `a1fc482`
consumió ambos prefijos y pasó 230/230 CTest. En el dispositivo local, FX-03
presentó 180 frames (p95 de entrega 18,105 ms) y FX-02 entregó 287.128
muestras activas; SDL informó cero muestras faltantes en ambas pasadas.
Los hashes SHA-256 de los ZIP, el origen de las series y sus límites están en
[`docs/release-checklist.md`](../../docs/release-checklist.md) y
[`docs/performance-baseline.md`](../../docs/performance-baseline.md).
El test de dispositivo valida readback y callbacks locales; no aporta
timestamps efectivos de presentación ni observación acústica física. Faltan
revisión de distribución, paquetes publicados, lock Runtime nativo y CI del
candidato; CA-08/CA-11 y T035–T036 siguen abiertos.

La [serie progresiva del candidato](../../docs/performance-baseline.md) repitió
FX-03 con el paquete Release actual en 3×180 frames y hashes comparables
coincidentes. Una traza regenerada con ese mismo build probó EOF en Runtime:
al solicitar 181 frames se emitieron exactamente 180, y la desactivación
programada en el frame 181 no produjo aplicación ni respaldo espurio. Esta
evidencia amplía T030/T032 localmente; falta su perfil nativo en CI y el lock
publicado, por lo que no cierra T030–T032 ni CA-10/CA-11.

**Corte de validación física y distribución (2026-10-20):** Runtime volvió a
pasar 3/3 pruebas de ruta nativa con los dispositivos GPU/audio de este PC
contra los prefijos extraídos de Tessera y Engine. Los dos ZIP reproducibles
actuales y sus SHA-256 constan en la revisión de distribución; todavía son
candidatos locales. Engine y Tessera aprobaron sus checks de PR en `fb3d9a8`
y `9e4f4c9`, respectivamente. Un
runner temporal de organización se retiró sin ejecutar jobs porque no había
un grupo autorizado para los tres repositorios públicos. Por tanto, el perfil
GPU/audio en CI, los assets, el lock y la aceptación siguen pendientes.

La línea base ya incluye presupuestos de aviso locales para la siguiente
validación, calculados con el máximo de las tres repeticiones y 25 % de margen
redondeado. Se limitan al hardware y perfil medidos; no sustituyen el perfil
de dispositivo en CI que quedó pendiente por decisión del usuario.

La revisión técnica de distribución T034 pasó localmente con Tessera `de825e5`
y Engine `fb3d9a8`: metadato de fuente limpia, paquete reproducible,
consumidor instalado y consumidor externo de los ZIP extraídos. Runtime pasó
3/3 pruebas nativas con este par exacto. La publicación, procedencia remota,
lock y aceptación T035–T036 continúan pendientes.

La comparación progresiva del par revisado se repitió dos veces (6×180 frames).
En una repetición apareció un episodio SDL de 337 muestras faltantes durante
7,021 ms y p99 de entrega 33,463 ms, acompañado por p99 de espera GPU al
adquirir imagen de 36,267 ms; las otras cinco repeticiones no lo reprodujeron.
Se conservan ambas series en la línea base. Los presupuestos locales de aviso
detectaron el caso y CA-11 no se declara cerrado.

El borrador de Tessera `v0.1.0-mvp.2` pasó su workflow Release y ofrece un ZIP
SHA-256 `40ae6447b3a5dd7ca4bd4aaed2fccc436118fd53bec6c97deeb6c2593bed5a8f`
con checksum y atestación verificados, fuente limpia y sin fixtures excluidos.
Su consumidor externo pasó con Engine local. En ese punto todavía faltaban el
asset Engine final, los locks nativos y la aceptación de toda la cadena.

El borrador Engine `native-v0.1.0-rc.22` también pasó su workflow completo;
el ZIP SHA-256 `4f17c34cde082b39a21c4a5216d77bc53e161df896487f1f9e50ab8a3fc1bdf2`
y su checksum fueron descargados y atestados. Ambos ZIP borrador juntos
pasaron el consumidor externo y el build completo de Runtime con 233/233
CTest secuenciales, incluidos FX-03, presentación GPU y audio. Runtime fijó
las dos identidades en locks nativos
verificados y subió el PR #55. Faltan los checks de esas ramas, publicar solo
los paquetes de software autorizados y consumir sus URL públicas. La prueba
física en CI y el incidente de regularidad/audio descrito arriba mantienen
CA-08 y CA-11 abiertos.

Los dos prereleases de software se publicaron con solo ZIP y checksum. Las
descargas sin credenciales desde las URL canónicas de los locks conservaron
los hashes indicados y pasaron manifiestos, atestaciones y verificadores de
paquete. El consumidor externo y Runtime configurado desde esos prefijos
públicos pasaron; Runtime repitió 233/233 CTest secuenciales, con GPU/audio
local. Los checks de código de Engine y Runtime quedaron verdes; falta
revalidar el commit documental final de Tessera. La matriz física de CI
continúa pendiente, por lo que CA-08 y el cierre del MVP no se declaran
aprobados.

La comparación progresiva se repitió con los **paquetes públicos exactos**:
dos series de 3×180 frames, 30 de calentamiento, mismo contenido/configuración/
traza y hashes estables dentro de cada modo. En las seis repeticiones hubo
180 composiciones, 361 eventos sin pérdidas y cero demanda adicional de audio
no disponible. La entrega tuvo p99 18,261–19,141 ms y máximo hasta 24,681 ms;
`runtime_device` p99 llegó a 2,506 ms en una repetición. La línea base ahora
separa esos resultados de los límites del binario anterior. El episodio previo
de 337 muestras y la falta de CI física siguen sin resolver, por lo que
T032/CA-11 permanecen abiertos.

La prueba FX-02 con audio activo, sobre los mismos paquetes públicos, añadió
dos series de 3×180 frames con B mantenido: 287.128 muestras activas por
repetición, hashes estables, cero pérdidas de eventos y cero demanda faltante
en el stream SDL. La entrega dio p99 18,103–18,929 ms y máximo hasta
27,683 ms. Este indicador del callback no mide cortes audibles del dispositivo;
el perfil físico en CI sigue pendiente por decisión del usuario.
