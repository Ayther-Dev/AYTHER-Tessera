# Guía de validación del MVP

**Estado:** fases 0–4 completadas localmente al 2026-10-19; Gate 0 seleccionó
ClownMDEmu, Gate 2 acredita FX-01 desde Engine y Gate 3 acredita Mega Drive,
FX-02 y checkpoints entre procesos. Consultar los resultados y límites de
[fase 3](../../docs/phase-3-results.md), [fase 2](../../docs/phase-2-results.md),
[fase 1](../../docs/phase-1-results.md) y [fase 0](../../docs/phase-0-results.md).
FX-03 y la presentación Engine–Tessera con `.ay` están verificadas localmente.
La fase 5 validó localmente la ruta Runtime de GPU y audio con dispositivos de
prueba y ejecutó la comparación progresiva FX-03. Tessera y Runtime tienen
sus checks actuales aprobados; Engine revalida el pin nuevo de Tessera en el
PR borrador. Faltan el paquete nativo versionado, su lock y la
prueba física GPU/audio en runner.

## Comprobaciones disponibles hoy

Desde PowerShell, leer la característica activa y comprobar su identidad:

```powershell
$repoRoot = 'C:\ayther\AYTHER-Tessera'
$featureFile = Join-Path $repoRoot '.specify\feature.json'
$feature = Get-Content -LiteralPath $featureFile -Raw | ConvertFrom-Json
if ($feature.feature_directory -ne 'specs/001-mvp-core') {
    throw 'La característica activa no es 001-mvp-core.'
}
Get-Item -LiteralPath (Join-Path $repoRoot $feature.feature_directory)
```

Comprobar las fuentes incluidas en `SHA256SUMS`; las ROM y packs se generan bajo
`build/` y se verifican con `tools/generate-native-fixtures.ps1`:

```powershell
$fixtureRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'tests\fixtures\ayther-engine-synthetic'))
$fixturePrefix = $fixtureRoot + [IO.Path]::DirectorySeparatorChar
foreach ($entry in Get-Content -LiteralPath (Join-Path $fixtureRoot 'SHA256SUMS')) {
    if ($entry -notmatch '^([0-9a-fA-F]{64}) \*(.+)$') { throw 'Entrada de hash inválida.' }
    $expectedHash = $Matches[1]
    $filePath = [IO.Path]::GetFullPath((Join-Path $fixtureRoot $Matches[2]))
    if ($Matches[2] -in @('ayther_synthetic.md', 'test_pack.ay', 'trust.toml')) { continue }
    if (-not $filePath.StartsWith($fixturePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Ruta fuera del fixture.'
    }
    if ((Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash -ne $expectedHash) {
        throw "Hash incorrecto: $filePath"
    }
}
'Integridad de archivos verificada.'
```

Esta comprobación acredita identidad de archivos; no prueba ejecución ni firma
del pack. La ROM canónica no activa sonido y su imagen puede permanecer estable.
El pack existente contiene texto de prueba, sin sustitución gráfica real.

## Compilación disponible

- Entorno local comprobado: Windows x64, `clang-cl` 22.1.6, MSVC v145 14.51.36231,
  CMake 4.3.3 y Ninja 1.13.2. El [inventario](../../docs/development-environment.md)
  identifica recursos, SDK y límites de ejecución.
- El wrapper selecciona anfitrión/destino x64 y CRT `/MD`. Requiere caché de
  dependencias válida; sus versiones/hashes están en
  [`cmake/dependencies.lock.json`](../../cmake/dependencies.lock.json).
- Ejecutar desde la raíz con una compilación activa. Para candidatos usar
  `-Jobs 1`; los benchmarks posteriores deben quedar aislados de compilaciones.

Núcleo nativo sin Engine/GPU, con ClownMDEmu fijado y tests sintéticos:

```powershell
./tools/bootstrap-dependencies.ps1 -Offline
./tools/check.ps1 -Preset windows-dev -Jobs 1
./tools/check.ps1 -Preset windows-release -Jobs 1
./tools/check.ps1 -Preset windows-diagnostics -Jobs 1
```

La preparación verifica archivos y no descarga implícitamente. Si falta caché,
la descarga explícita es `./tools/bootstrap-dependencies.ps1 -Download`.
`windows-diagnostics` incluye un caso negativo de acceso fuera del heap: el test
solo pasa cuando AddressSanitizer detecta el fallo esperado. Sus anotaciones de
contenedores STL están desactivadas; las comprobaciones de direcciones siguen activas.

Las suites `contract.native_api`, `unit.synthetic_backend`, `unit.session`,
`unit.system_detector`, `contract.snapshot`, `integration.cli`,
`integration.replay`, `integration.megadrive_session`, `unit.state_format`,
`integration.state_store`, `integration.checkpoint` e
`integration.megadrive_replay` se ejecutan por CTest. El replay FX-01 crea doce procesos nuevos de
3.000 frames cada uno y contrasta los tres hashes con el modelo independiente.
Los informes y `summary.json` quedan en `build/evidence/phase1-<config>-<ASAN>`;
se regeneran con el build_id de cada configuración. No comparar estados de
builds/configuraciones distintos: el estado incluye su identidad.

Para generar contenido, memoria, configuración y trazas binaria/JSON sin
descargas, usar la [CLI](../../docs/cli.md):

```powershell
./build/windows-dev/src/tessera.exe demo --backend synthetic.complete --scenario action --frames 3000 --observer two-readers --fixture-dir ./build/fx01 --report ./build/fx01-report.json
```

## Probes de candidatos y enlace SDK

Los probes son ejecutables de evaluación de fase 0, separados de la API pública.
Con la caché ya disponible, verificarla y ejecutar ambos candidatos:

```powershell
./tools/bootstrap-backends.ps1
$cache = Join-Path (Get-Location) 'third_party/backends'
$clownPin = 'clownmdemu-core-88ef45a6585556e2247dd26b6c937d6b9fb4a12d'
$aresPin = 'ares-0aafd85789215e84e1e43415c07d4c88461b7899'
./tools/check.ps1 -Preset windows-dev -Jobs 1 -CMakeArgs @(
    '-DTESSERA_BUILD_BACKEND_PROBES=ON',
    '-DTESSERA_PROBE_CLOWN=ON', '-DTESSERA_PROBE_ARES=ON',
    "-DTESSERA_CLOWN_SOURCE=$cache/$clownPin/$clownPin",
    "-DTESSERA_ARES_SOURCE=$cache/$aresPin/$aresPin"
)
```

Si faltan archivos de candidatos, `./tools/bootstrap-backends.ps1 -Download`
descarga las revisiones fijadas explícitamente. Los tests generan el fixture
en `build/windows-dev/tests/backend_probe/fixtures` y resultados separados en
`build/windows-dev/tests/backend_probe/results`; usan las trazas cortas de 60 frames.
No escribirlos sobre la [copia canónica](../../tests/fixtures/ayther-engine-synthetic/README.md).
La [demo propia](../../tests/fixtures/backend-probe/README.md) tiene oráculos
derivados de su fuente y es solo una semilla de FX-02. Ares no tiene cobertura ASAN.

Tras completar esas pruebas y preparar el SDK con el comando de enlace de abajo,
ejecutar la matriz completa de Gate 0 con los dos prefijos exportados:

```powershell
pwsh -NoProfile -File ./tools/check-backends.ps1 -SkipBuild
```

El runner compara ambos candidatos en trazas cortas y exige a ClownMDEmu
dos procesos de 3.000 frames para control, acción y ROM copiada; seis
restauraciones en procesos nuevos de 500 frames y comparación por frame de
estado completo, vídeo y PCM. Comprueba también observación apagada, dos
lectores y saturación, oráculos independientes y respaldo original íntegro.
Verifica fuentes y codec sin descargas y repite el enlace SDK incluso con
`-SkipBuild`. Sin ese flag compila y ejecuta primero los checks de candidatos.
Requiere el SDK previamente configurado, Python 3.10 o posterior y clang para
la auditoría manual reproducible; Python no es dependencia de CMake ni del núcleo.

Cada ejecución conserva informes con identidad de binario, fuentes, ROM,
configuración, traza y PID en un directorio nuevo bajo `build/evidence/backends`.
El resumen es `build/evidence/backend-gate.json`: **salida 0** solo cuando todas
sus pruebas pasan; **salida 1** ante fallo, sin selección. El respaldo probado
no acredita reconstrucción temporal, FX-03 ni integración de producto.

Después de esa matriz, repetir las regresiones negativas del CLI privado:

```powershell
$gate = Get-Content build/evidence/backend-gate.json -Raw | ConvertFrom-Json
if (-not $gate.gate0_pass) { throw 'Gate 0 no está cerrado.' }
$baseline = $gate.runs | Where-Object name -EQ 'own-action-process1'
$evidence = Split-Path (Join-Path (Get-Location) $baseline.report)
./tests/backend_probe/verify_cli.ps1 `
    -Probe ./build/windows-dev/tests/backend_probe/tessera_clown_gate_probe.exe `
    -FixtureDirectory (Join-Path $evidence 'fixtures') `
    -CheckpointFile (Join-Path $evidence 'checkpoints-own/500.tsc') `
    -EvidenceDirectory (Join-Path $evidence ('cli-negatives-' + [Guid]::NewGuid().ToString('N')))
```

Los cuatro casos deben fallar con su código esperado y sin generar un informe
de éxito: overflow desde un checkpoint, cantidad negativa, más frames que los
restantes y modo de observación desconocido. El runner también prueba tres
regresiones de identidad: PID reciclado, misma instancia y PIDs distintos.

La suite diagnóstica ampliada incluye 8 tests con ClownMDEmu y sus fuentes
C instrumentadas, más el probe C++; ares estaba desactivado. Para repetir ese
alcance, utilizar el preset `windows-diagnostics` con las opciones anteriores,
pero `-DTESSERA_PROBE_ARES=OFF`. No atribuir cobertura ASAN a ares.

Para repetir el enlace mínimo con el SDK comprobado:

```powershell
$env:AYTHER_ENGINE_PREFIX = 'C:/ayther/AYTHER-Engine/install/windows-release-engine-vpx'
$env:AYTHER_ENGINE_DEPENDENCY_PREFIX = 'C:/ayther/AYTHER-Engine/build/windows-release-engine-vpx/vcpkg_installed/x64-windows'
./tools/check.ps1 -Preset windows-engine-integration -Jobs 1
```

Este prefijo usa Core ABI 8/SDK 0.1.0, Release y `/MD`. El enlace de símbolo real
y POD C/C++ no demuestra Engine→Tessera. `windows-runtime-integration` rechaza
la configuración porque la integración de producto aún no existe; no es un check aprobado.

## Escenarios de validación tras implementar

1. **Núcleo y CLI:** ejecutar sin interfaz gráfica mediante la API C nativa;
   comprobar carga, pausa, avance de un frame, reinicio, entradas y cierre.
2. **Detección y despacho:** reconocer Mega Drive por contenido, incluso con la
   ROM renombrada; rechazar contenido inválido con extensión engañosa, truncados, ambigüedad y selección
   explícita incompatible. Registrar sistema e implementación seleccionados.
3. **FX-01:** perfiles mínimo y completo del backend sintético, capacidades
   ausentes y una entrada con cambio de estado conocido; el control sin entrada
   debe producir el resultado distinto previsto.
4. **Replay:** dos procesos independientes por demo, 3.000 frames y cero
   divergencias de estado emulado, vídeo original y audio original canónicos.
5. **Estados:** guardar después de completar K=500, 1.500 y 2.500; restaurar cada
   checkpoint en otro proceso y comparar K+1…K+500 con la ejecución continua.
   Verificar corrupción, incompatibilidad y guardado interrumpido sin carga parcial.
6. **Observación:** dos lectores reciben capturas idénticas e inmutables;
   observar conserva hashes originales. Saturación, lectores lentos y cierre
   producen resultados definidos; reinicio y restauración invalidan épocas previas.
7. **Engine → Tessera:** repetir los contratos por la integración nativa de Engine;
   verificar que esta ruta consume directamente la API de Tessera.
8. **FX-02:** ejecutar la variante completada con oráculos independientes de estado observable,
   vídeo y audio/entradas por API autónoma y Engine; verificar efecto conocido
   frente a control, ventanas de audio activo y transiciones. El silencio constante falla.
9. **FX-03:** incorporar un paquete `.ay` con imágenes reales, regla sobre recurso
   conocido e identidad correcta de ROM; comparar el resultado visual esperado
   mediante Engine y conservar hashes originales. Verificar precedencia de reglas
   coincidentes/solapadas, respaldo e invalidación.
10. **Runtime → Engine → Tessera:** validar presentación y audio en el entorno
    declarado; separar esta evidencia de las pruebas del núcleo sin GPU.

## Medición de rendimiento y evidencia de cierre

Medir por etapas emulación, vídeo/audio originales, observación, capturas, hashing
y composición; comparar Tessera autónoma y Engine con el mismo backend y traza.
Comparar libretro únicamente cuando permita aislar la misma implementación.

Documentar calentamiento, repeticiones, hardware, versiones y configuración.
Registrar p50/p95/p99, CPU por etapa, GPU donde corresponda, memoria máxima,
asignaciones y bytes copiados por frame, pérdidas de eventos, respaldos,
intervalos de entrega al presentador en milisegundos con reloj monotónico y
p50/p95/p99/máximo. Separar timestamps efectivos cuando existan e identificar
origen/cobertura. Contar episodios de cortes de audio, muestras faltantes y
duración total/máxima; el silencio esperado es audio válido. Comparar observación
desactivada, activada y sustitución activa. Fijar presupuestos después de medir la línea base.

Conservar informes, hashes y trazabilidad a requisitos; ejecutar unidad,
contratos, replay, regresiones y diagnósticos compatibles en CI. El cierre exige
FX-01, FX-02 y FX-03 completos y distribución verificada en entorno limpio.
Los resultados de fases 0–3 no sustituyen FX-03 ni las pruebas de Runtime.

## Paquete instalado e integración nativa de fase 2

Tessera se instala fuera del árbol de fuentes. La prueba `package.installed_consumer`
instala en un prefijo temporal, lo reubica y compila un consumidor C11/C++20 sin
incluir fuentes privadas; también rechaza versión 0.2, CRT `/MT`, destino ARM64
y herramientas MSVC anteriores a 14.51. El paquete de diagnóstico conserva el
compilador exacto y copia el runtime ASAN local para las pruebas.

```powershell
./tools/check.ps1 -Preset windows-release -Jobs 1
cmake --install build/windows-release --prefix install/windows-release
```

El proveedor de Engine se habilita explícitamente; no cambia los consumidores
legacy y no sustituye errores por otro backend. Desde una Developer Shell x64
con el entorno de dependencias ya preparado de Engine:

```powershell
cmake -S C:/ayther/AYTHER-Engine -B C:/ayther/AYTHER-Engine/build/windows-release-engine-vpx `
  -DAYTHER_ENABLE_TESSERA=ON `
  -DTessera_DIR=C:/ayther/AYTHER-Tessera/install/windows-release/lib/cmake/Tessera `
  -DAYTHER_TESSERA_TEST_SOURCE_DIR=C:/ayther/AYTHER-Tessera `
  -DVCPKG_MANIFEST_INSTALL=OFF
cmake --build C:/ayther/AYTHER-Engine/build/windows-release-engine-vpx --parallel 1
ctest --test-dir C:/ayther/AYTHER-Engine/build/windows-release-engine-vpx `
  -R tessera --parallel 1 --output-on-failure
```

`VCPKG_MANIFEST_INSTALL=OFF` presupone el caché auditado local: un entorno sin
esas dependencias falla en lugar de descargarlas implícitamente. El checkout de
Tessera aporta únicamente headers fijados para las pruebas y los fixtures de
la CLI; el producto enlaza `Tessera::core` instalado. Engine documenta el uso
público en `docs/TESSERA_NATIVE.md`. Los resultados definitivos de cada fase
se registran antes de abrir la siguiente; esta receta no acredita su ejecución.

## Repetir Gate 3 localmente

Preparar solo la fuente fijada de ClownMDEmu, construir con un proceso activo y
ejecutar primero las pruebas breves. El replay largo genera FX-02, valida sus
hashes y publica una carpeta de evidencia nueva bajo `build/evidence`:

```powershell
./tools/bootstrap-backends.ps1 -SelectedOnly
./tools/check.ps1 -Preset windows-dev -Stage Build -Jobs 1
ctest --test-dir build/windows-dev -E '^integration.megadrive_replay$' --output-on-failure -j 1
ctest --test-dir build/windows-dev -R '^integration.megadrive_replay$' --output-on-failure -j 1
$evidence = Get-ChildItem build/evidence/phase3-RelWithDebInfo-OFF -Directory |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$summary = Get-Content (Join-Path $evidence.FullName 'summary.json') -Raw | ConvertFrom-Json
if ($summary.divergences -ne 0 -or $summary.processes -ne 15) { throw 'Gate 3 autónomo falló.' }
cmake --install build/windows-dev --prefix install/windows-dev
```

Desde la Developer Shell x64 de Engine, apuntar su build local sin ASan al
paquete y a **esa misma** evidencia; los hashes de estado incluyen el build_id,
por lo que no se mezclan perfiles dev/Release/ASan:

```powershell
.\tools\generate-native-fixtures.ps1 -TesseraBuild .\build\windows-dev `
  -EnginePackTool C:\ayther\AYTHER-Engine\build\windows-native\bin\make_test_pack.exe
```

Ese comando se ejecuta desde Tessera tras construir ambos generadores. No
publica las ROM, los packs ni el registro de confianza.

```powershell
cmake -S C:/ayther/AYTHER-Engine -B C:/ayther/AYTHER-Engine/build/windows-native `
  -DAYTHER_ENABLE_TESSERA=ON `
  -DTessera_DIR=C:/ayther/AYTHER-Tessera/install/windows-dev/lib/cmake/Tessera `
  -DAYTHER_TESSERA_TEST_SOURCE_DIR=C:/ayther/AYTHER-Tessera `
  -DAYTHER_TESSERA_TEST_FIXTURE_DIR=C:/ayther/AYTHER-Tessera/build/windows-dev/native-fixtures `
  -DAYTHER_TESSERA_PHASE3_EVIDENCE_DIR=$($evidence.FullName)
cmake --build C:/ayther/AYTHER-Engine/build/windows-native `
  --target ayther_tessera_checkpoint_test --parallel 1
ctest --test-dir C:/ayther/AYTHER-Engine/build/windows-native `
  -R '^ayther.integration.tessera_checkpoint$' --output-on-failure -j 1
```

La última prueba usa 12 procesos Engine y compara 13.500 frames con los hashes
autónomos y los oráculos de FX-02. Los negativos de formato, guardado,
interrupción y restauración están en `unit.state_format`,
`integration.state_store`, `integration.checkpoint` e
`integration.megadrive_session`.

## Perfil local de Runtime y rendimiento de fase 5

Instalar Tessera y Engine desde las revisiones locales validadas, configurar
Runtime con ambos prefijos, `AYTHER_ENABLE_TESSERA_NATIVE=ON` y
`AYTHER_ENABLE_GPU_TESTS=ON`, y pasar
`AYTHER_TESSERA_TEST_FIXTURE_DIR=C:/ayther/AYTHER-Tessera/build/windows-dev/native-fixtures`.
El segundo interruptor exige ventana, Vulkan y
dispositivo de audio reales; no cuenta una prueba omitida. Después:

```powershell
ctest --test-dir C:/ayther/AYTHER-Runtime/out/build/tessera-local `
  -C RelWithDebInfo --output-on-failure `
  -R '^(runtime_options|tessera_runtime_.*)$'
build/windows-dev/tests/tessera_audio_fixture_generator.exe `
  --generate build/phase5-fx02
build/windows-dev/tests/tessera_performance_benchmark.exe `
  build/windows-dev/src/tessera.exe `
  C:/ayther/AYTHER-Runtime/out/build/tessera-local/bin/RelWithDebInfo/ayther_runtime.exe `
  build/phase5-fx02/program.md build/evidence/phase5-benchmark --devices
```

Para la comparación progresiva local de FX-03, con las mismas opciones de
build y paquetes instalados, ejecutar:

```powershell
build/windows-dev/tests/tessera_performance_benchmark.exe `
  build/windows-dev/src/tessera.exe `
  C:/ayther/AYTHER-Runtime/out/build/tessera-local/bin/RelWithDebInfo/ayther_runtime.exe `
  build/windows-dev/native-fixtures/fx03/fx03_rom.md `
  build/evidence/phase5-progressive-fx03 --devices `
  --pack build/windows-dev/native-fixtures/fx03/fx03.ay `
  build/windows-dev/native-fixtures/fx03/trust.toml
```

El runner exige composición real y compara hashes originales por frame entre
las etapas de Engine y Runtime; `docs/performance-baseline.md` documenta el
alcance y los límites de las mediciones. ROM y pack permanecen solo en local.

El último comando se ejecuta desde `C:/ayther/AYTHER-Tessera`, sin otras
compilaciones o pruebas paralelas. Genera una traza y configuración compartidas,
tres repeticiones de 180 frames por modo y 30 frames de calentamiento; el
informe JSON queda bajo `build/evidence`. `docs/performance-baseline.md` define
el punto y límite de cada serie. La validación GPU obtiene un readback del
renderer además de comprobar la entrega al swapchain; los timestamps efectivos
de presentación no están disponibles. Las demandas SDL de audio son un
indicador de falta de datos en el stream, no una medición de salida audible.

El perfil compatible con el lock Engine rc.19 conserva
`AYTHER_ENABLE_TESSERA_NATIVE=OFF`. Para el cierre del MVP deben ejecutarse
los checks CI de las revisiones publicadas y el perfil nativo contra la cadena
de paquetes finales; `docs/release-checklist.md` registra esos gates.

Para repetir la comparación del candidato Release con los ZIP locales ya
extraídos y el Runtime enlazado a ambos prefijos, ejecutar:

```powershell
build/windows-release/tests/tessera_performance_benchmark.exe `
  build/candidates/d074/unpacked/tessera-0.1.0-windows-x86_64/bin/tessera.exe `
  C:/ayther/AYTHER-Runtime/out/build/tessera-local/bin/RelWithDebInfo/ayther_runtime.exe `
  build/windows-dev/native-fixtures/fx03/fx03_rom.md `
  build/evidence/phase5-progressive-fx03-packaged-d074 --devices `
  --pack build/windows-dev/native-fixtures/fx03/fx03.ay `
  build/windows-dev/native-fixtures/fx03/trust.toml
```

La traza binaria nueva se escribe en ese directorio de evidencia. Para
verificar EOF sin un frame extra, usarla con `--frames 181` y comprobar que
Runtime informa `AYTHER_NATIVE_EOF frame=180` y
`AYTHER_NATIVE_DONE frames=180`. Con el pack, `--native-deactivate-at 181`
debe conservar 180 composiciones y cero respaldos. Esta comprobación solo
sirve si la traza y el binario comparten build_id; una traza de otra revisión
se rechaza antes de avanzar. No publicar la traza generada en Git ni como
artifact de CI.
