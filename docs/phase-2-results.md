# Resultados de fase 2 — Integración nativa temprana con Engine

**Fecha:** 2026-10-19. **Estado:** fase 2 cerrada localmente; T015–T018 completadas.
Gate 2 aprobado con FX-01 desde Engine en Release y ASAN. La fase 3 no se ha
iniciado. Este informe no acredita el MVP completo ni la ejecución remota de CI.

## Entrega y alcance

| Tarea | Implementación y comprobación disponible |
| --- | --- |
| T015 | Paquete instalado `Tessera::core`, ABI C 1.0, metadatos, controles de plataforma/CRT y consumer externo. Smoke aprobado en dev, Release y diagnósticos. |
| T016 | Módulo privado `TesseraBackend` de Engine: negociación, ownership de handles, controles completos, originales prestados, capturas, eventos y resultados con contexto. Contratos aprobados en Release y ASAN. |
| T017 | Selección explícita `tessera_native` en `AytherSession`, driver público `NativeEmulation` y `native_frame()`. No hay cambio silencioso a libretro ante errores. |
| T018 | FX-01 desde la fachada real de Engine, comparación con CLI autónoma y oráculos independientes; 67 comprobaciones de fachada y matriz Release aprobadas. |

Engine consume el paquete instalado mediante su API pública C; sus nuevos encabezados
C++ nativos no incluyen tipos de Tessera ni libretro. El checkout fijado se utiliza sólo
para dependencias de pruebas y fixtures. La opción `AYTHER_ENABLE_TESSERA=OFF`
conserva el proveedor existente y stubs que responden `unsupported_capability`.

La ruta nativa se conduce por `native_emulation()`, con entrada completa y
frame/época explícitos. Las llamadas legacy `step()` y `set_input()` se rechazan
sin avance. `native_frame()` publica originales RGBA8 y PCM S16LE con su reloj,
frecuencia y canales, y vacía la vista cuando no hay originales actuales.
FX-01 produce 16×16 píxeles y 800 pares estéreo a 48 kHz por frame; no se envían
al dispositivo legacy de 44,1 kHz. El respaldo se identifica como
`pack_not_integrated`; no se simulan sustituciones ni observación legacy.

Los contratos cubren entradas negativas sin avance, EOF, presupuesto y
recuperación de capturas, dos cursores independientes y pérdidas con 4096
eventos, retención tras destrucción del propietario, y cancelación por reset o
cierre con cola saturada. La identidad y causa de la cancelación permanecen
fuera de la cola. Las capturas históricas conservan referencias independientes;
los préstamos originales vencen ante avance/reset/cierre exitosos.

## Regresiones cerradas antes del hito

Los tests escritos antes de implementar registraron los fallos de paquete y de
símbolos inicialmente ausentes. La fachada real detectó después un rechazo
legacy no observable y préstamos caducados tras reset/cierre directo; ambos se
corrigieron y sus regresiones pasan. Se rechazan también observadores legacy
de PCM. El consumer de Engine preserva su prefijo al resolver Tessera, y el
build ASAN conserva `/WX` usando el forwarding correcto de clang-cl.

## Validación local confirmada

| Configuración o comprobación | Resultado |
| --- | --- |
| Núcleo `windows-dev`, incluida la prueba del paquete instalado | 10/10 CTest aprobadas. |
| Núcleo `windows-release`, incluida la prueba del paquete instalado | 10/10 CTest aprobadas. |
| Núcleo `windows-diagnostics`, incluida la prueba ASAN negativa esperada y el paquete | 11/11 CTest aprobadas; 190,22 s. |
| Smoke del paquete Tessera fuera del árbol, dev / Release / diagnósticos | 1/1 en cada configuración; incluido en los conteos anteriores. |
| Engine Release, tres contratos nativos y replay | 4/4 CTest aprobadas; 17,80 s. |
| Engine ASAN, resultado final de las cuatro pruebas nativas | 4/4 CTest aprobadas; 166,61 s. |
| Regresión Engine sin GPU, incluido el proveedor legacy y la integración nativa | 219/219 CTest aprobadas; 124,66 s. Se excluyeron las 25 pruebas etiquetadas GPU. |
| Consumer externo del paquete instalado de Engine | Configuración, 14 pasos de build, smoke de enlace con plazo de 5 s, cierre de símbolos del renderer y rechazo de versión 0.2 aprobados. |

Los conteos tienen alcances distintos; no se suman como una suite del MVP.
Los logs del núcleo son [dev](../build/evidence/phase2-core-dev-final.log),
[Release](../build/evidence/phase2-core-release.log) y
[ASAN](../build/evidence/phase2-core-asan.log). El log de la ruta Engine es
[Release](../build/evidence/phase2-engine-release-green.log) y
[ASAN](../build/evidence/phase2-engine-asan-test.log). También se conservan la
[regresión completa](../build/evidence/phase2-engine-regression-tests.log), el
[consumer instalado](../build/evidence/phase2-installed-engine-consumer.log) y la
[compilación OFF sin imports Tessera](../build/evidence/phase2-native-disabled.log). La evidencia bajo
`build/` es local y regenerable, ajena a los entregables versionados.

El [resumen de Engine Release](../../AYTHER-Engine/build/windows-release-engine-vpx/tests/tessera-evidence/tessera-native-summary.json)
registra `passed=true`, cero divergencias, doce ejecuciones Engine de 3000
frames y cuatro baselines de CLI de 3000: **36.000 + 12.000 frames**. Con el
proceso de self-tests son 17 instancias identificadas por PID y creación UTC.
La misma matriz ASAN completó otros 36.000 frames Engine y 12.000 autónomos
con cero divergencias. La repetición Release está incluida también en las
219 pruebas finales; no se suma otra vez como cobertura distinta.

| Perfil | Traza | Observación Engine | Procesos Engine |
| --- | --- | --- | --- |
| Mínimo | Neutral y B en 301…600 | Desactivada | Dos por traza. |
| Completo | Neutral y B en 301…600 | Desactivada | Dos por traza. |
| Completo | Neutral y B en 301…600 | Dos lectores | Dos por traza. |

Cada frame contrasta estado, vídeo y audio canónicos con fórmulas cerradas
independientes, además de CLI y repeticiones. El oráculo no abre otra sesión de
Tessera para calcular lo esperado. Los dos lectores del runner son lógicos y
secuenciales (`concurrent=false`); la retención multihilo pertenece a las pruebas
de contrato. Los hashes de estado incorporan build/configuración y sólo se
comparan entre ejecuciones con identidades compatibles.

## Costes locales de la ruta sintética

Primera repetición Release de la traza acción (evidencia conservada en
[resumen inicial](../build/evidence/phase2-engine-release-initial-summary.json)): 3000 muestras por etapa,
microsegundos de reloj monotónico, sin descartar calentamiento. Preparación,
metadatos y escritura JSON quedan fuera. Cada celda muestra p50 / p95 / p99.

| Perfil / observación | Avance público | Consultas, encoding y hashes | Lectura de observación | Oráculo funcional |
| --- | --- | --- | --- | --- |
| Mínimo / desactivada | 3,6 / 4,4 / 6,8 | 21,6 / 25,9 / 38,1 | 0,0 / 0,1 / 0,1 | 29,3 / 35,9 / 51,7 |
| Completo / desactivada | 3,8 / 5,8 / 7,8 | 22,6 / 34,6 / 46,0 | 0,0 / 0,1 / 0,1 | 30,4 / 48,6 / 65,7 |
| Completo / dos lectores | 10,0 / 14,3 / 21,0 | 22,7 / 31,5 / 48,3 | 26,1 / 33,8 / 58,4 | 31,1 / 45,2 / 66,9 |

El cero representa el camino vacío y la resolución del reloj. Estos tramos
mantienen separado el trabajo del harness. No establecen FPS, sobrecoste neto
aceptado, rendimiento de Mega Drive, ni CPU/GPU, presentación o audio de Runtime;
la aceptación completa de rendimiento corresponde a T032.

## Entorno, procedencia y publicación

Se conserva Windows x64, clang-cl, herramientas MSVC 14.51 y CRT `/MD`.
El paquete de diagnóstico exige el mismo compilador y versión, y copia su
runtime ASAN local para las pruebas. `_DISABLE_STL_ANNOTATION` mantiene
desactivadas las anotaciones de contenedores STL; las comprobaciones de
direcciones siguen activas en Engine C++ y Tessera. La biblioteca Rust y las
dependencias precompiladas no están instrumentadas por esta configuración.
Es cobertura diagnóstica local con ese alcance.

La base de Tessera es `4ae7f55`; el commit local de T015 es
`dfef7223af0928f89fa0db97bb754cc64a417aed`. El commit de Engine es
`eaed3cf46b33329b8c2ef881c648058162c2c7e6` en `feat/tessera-native-session`.
La integración CI fija ese SHA de Tessera en una entrada
Windows del job nativo existente. El build de la dependencia prepara sólo core
y CLI; las suites propias de Tessera permanecen en su check existente. La
validación Python del YAML conserva diez jobs y la matriz previa; no demuestra una ejecución de Actions. No hubo push,
PR ni publicación coordinada; el pin local debe publicarse antes del consumo
remoto, dentro de T035.

Se trabajó con una compilación a la vez, `Jobs=1`, como máximo dos frentes de
edición y reutilización de cachés. Se mantiene la estrategia de mínimos PR y
publicación coordinada, sin nuevas matrices ni publicaciones por tarea.

El paquete sintético de Tessera es MIT y conserva avisos de sus dependencias;
los oráculos adaptados mantienen procedencia y permiso. El qualifier ClownMDEmu
AGPL de Gate 0 no forma parte de este paquete. Este informe no acredita todavía
cumplimiento de distribución pública del futuro producto; T034 debe verificar
el conjunto realmente entregado y sus obligaciones.

## Fronteras que permanecen pendientes

- T019: backend Mega Drive de producto y ejecución de la demo copiada; reconocer
  su formato no equivale a disponer de una factory real integrada.
- T020: completar FX-02 de audio activo y sus expectativas independientes;
  la semilla sonora y la demo copiada no lo cierran por sí solas.
- T024: checkpoints y continuaciones autónomas/vía Engine; hoy se rechazan con
  `unsupported_capability`, sin prometer restauración de producto.
- T028: FX-03 con `.ay` y recurso gráfico reales, precedencia y píxeles
  esperados; T029 añade composición, respaldo e invalidación con paquete real.
- T030: consumo Runtime, presentación y audio de dispositivo; T032 consolida
  mediciones completas y T034–T036 distribución, CI real y aceptación final.

Referencias: [tareas](../specs/001-mvp-core/tasks.md),
[contrato nativo](../specs/001-mvp-core/contracts/native-api.md),
[consumo Engine](../../AYTHER-Engine/docs/TESSERA_NATIVE.md) y
[resultados de fase 1](phase-1-results.md).
