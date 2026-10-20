# Coordinación de CI y entrega del MVP

Estado al 2026-10-20: tres PR borradores abiertos, uno por repositorio:
[Tessera #1](https://github.com/Ayther-Dev/AYTHER-Tessera/pull/1),
[Engine #48](https://github.com/Ayther-Dev/AYTHER-Engine/pull/48) y
[Runtime #55](https://github.com/Ayther-Dev/AYTHER-Runtime/pull/55).
Se publicaron solo dos prereleases de software, Tessera mvp.2 y Engine rc.22,
tras revisar distribución y fijar los locks de Runtime. No se abrió otro PR.

El usuario autorizó el 2026-10-20 publicar únicamente los paquetes de software
Tessera y Engine nativo tras cerrar la revisión de distribución y actualizar el
lock de Runtime. La [revisión técnica local T034](distribution-review.md) quedó
conforme; los ZIP públicos descargados coinciden con los locks, manifiestos y
atestaciones. El gate GPU/audio físico en CI sigue pendiente por decisión del
usuario.

El usuario confirmó que no hay permiso verificable para publicar la ROM/pack
Engine/Lab ni la ROM FX-03 derivada. Se generan en `build/` con
`tools/generate-native-fixtures.ps1`. Las tres ramas publicadas se auditaron con
`git rev-list --objects main..HEAD`: no incluyen ROM, pack, trust ni generador
compilado. Ningún binario generado se sube como artifact de Actions.

| Repositorio | Validación local de este corte | CI remota exigida antes de T036 |
| --- | --- | --- |
| Tessera | ZIP público mvp.2, consumidor C/C++, replay y comparación progresiva FX-03/FX-02 | `Core checks` y workflow de release mvp.2 aprobados; revalidación del commit documental final pendiente |
| Engine | ZIP público rc.22, 44 checks de payload y consumidor externo de la pareja | CI completa del PR #48 y workflow nativo rc.22 aprobados; pin Tessera `c69b9a3` |
| Runtime | Build con los dos prefijos públicos y 233/233 CTest locales, incluidos FX-03, GPU y audio | Todos los checks del PR #55 aprobados; locks nativos reales junto al lock rc.19 predeterminado |

La rama `main` de Runtime protege estos contextos: `Windows / RelWithDebInfo`,
`Linux / RelWithDebInfo`, `Linux / ASan-UBSan-Fuzz`,
`Security / Dependency Review`, `Security / Secret Scan`,
`Analyze (actions)`, `Analyze (c-cpp)` y `Windows / QA`.
Las ramas `main` de Tessera y Engine no devuelven protección de rama en la API;
sus workflows declarados siguen siendo parte del gate del MVP.

El paquete Runtime publicado continúa fijado al Engine `v0.1.0-rc.19` para la
CI del producto actual. Los locks nativos adicionales fijan Engine rc.22 y
Tessera mvp.2; la ruta nativa de Runtime se activa explícitamente y pasó
233/233 CTest locales con ambos paquetes públicos. Los checks protegidos de
Runtime están verdes. El perfil GPU/audio físico en CI continúa pendiente;
una ejecución local o un runner sin dispositivos no lo satisface.

La prevalidación local utiliza prefijos instalados y cachés verificadas, una
compilación activa y benchmarks aislados. No se requieren PR adicionales,
matrices nuevas por tarea, cron ni ejecuciones manuales para validar cambios
intermedios. Antes de cada push, registrar `git diff --check`, CTest pertinente,
hashes de binarios/fixtures y el resultado de `acceptance-report.md`.

**Bloqueadores actuales:** el perfil local separa asignaciones C++, copias,
eventos/pérdidas, respaldos, espera de imagen y trabajo GPU; no ofrece
timestamps efectivos del dispositivo. Una repetición presentó 337 muestras
faltantes y un p99 de entrega de 33,463 ms, por encima del aviso local; se
conserva el incidente para T032/CA-11. La prueba GPU/audio física en CI sigue
pendiente por decisión del usuario. T031/T032/T035/T036 y CA-08/CA-11 no se
declaran cerrados.

La revisión Engine `3deb4c2` pasó el
[CI completo 39440336838](https://github.com/Ayther-Dev/AYTHER-Engine/actions/runs/39440336838):
replay Tessera de 18.000 frames y 4.500 continuaciones sin divergencias,
225/225 CTest nativos Windows, consumidor externo, ASan+UBSan, 4/4 tests
nativos instrumentados y `Required CI gate`. La cobertura C++ remota combinó
Linux CPU/GPU y Windows Engine→Tessera: 53.886/65.403 (82,39 %) total y
3.007/3.786 (79,42 %) en líneas cambiadas, con mínimos 50 % y 70 % intactos.
El workflow de release existente todavía construye Engine sin Tessera; T035
requiere un candidato nativo verificable y su lock real antes de declarar
entrega completa. Runtime no tiene runner autohospedado registrado (consulta
GitHub de 2026-10-20), por lo que el perfil GPU/audio físico en CI sigue pendiente.
El [CI completo de Engine 39451144667](https://github.com/Ayther-Dev/AYTHER-Engine/actions/runs/39451144667)
ya terminó en verde, incluido el consumidor Windows del paquete, sobre `6bbe484`.
El usuario indicó usar los dispositivos locales; aún faltan registro,
configuración y ejecución del perfil de aceptación en CI. Se ejecutó de nuevo
`ctest --test-dir C:/ayther/AYTHER-Runtime/out/build/tessera-local
-C RelWithDebInfo --output-on-failure
-R '^tessera_runtime_(native_headless|active_audio|gpu_audio)$'`:
3/3 pruebas aprobadas en 8,14 s con la GPU y el audio de este PC. Se comprobó
que la cuenta puede solicitar un token de registro sin mostrarlo, pero no hay
runner registrado ni workflow GPU/audio activo en CI; el resultado local no
sustituye ese gate.

**Candidato local extraído (2026-10-20):** Tessera `d074cb4` instaló C11/C++20,
CLI, licencias, inventario y avisos; dos ZIP idénticos SHA-256
`42b8870ab8d83744c895f6d5cef5ab3d98b36b5bd74c135c8d0520b1af12b907`
(27 entradas). Engine `3deb4c2` se recompiló con `AYTHER_ENABLE_TESSERA=ON`
contra ese paquete, pasó 44 comprobaciones de payload y 12 replays nativos
con 36.000 frames sin divergencias. Dos ZIP Engine idénticos SHA-256
`5bd460bfe713ac5b3b86b9ebf3caef564ee5f9930dd8d33c69d47f2880808c3e`
(122 entradas) se extrajeron junto con Tessera; un consumidor externo compiló
y ejecutó. Ninguno de los ZIP lleva ROM, pack, trust ni generador compilado.
Los hashes del aviso, inventario, lock y licencia principal del ZIP de Tessera
coinciden con las fuentes (4/4); las 10 licencias externas instaladas también
son idénticas byte por byte a las fuentes. El metadato del paquete conserva
`distribution_review: T034-pending` hasta cerrar la revisión conjunta de
distribución, sin inferir derechos sobre los fixtures generados.
La revisión siguiente agrega `SOURCE_AVAILABILITY.md` al prefijo instalado y
registra SHA de commit y estado limpio en `package-metadata.json`. Configuración
Release y consumidor C11/C++20 pasaron; el último candidato ZIP de SHA
`42b8870a…` antecede a este cambio y debe regenerarse antes de publicarse.

**Candidato con oferta de fuentes (2026-10-20):** Tessera `488f8aa` se configuró
con árbol limpio y produjo dos ZIP byte a byte idénticos,
`tessera-v0.1.0-mvp.1-windows-x86_64.zip`, SHA-256
`27c070dc2bb89682eb70bb22e61d45e4ea69feb8a952fad6d1c3306651509ff9`.
Las 28 entradas incluyen el manifiesto de fuentes y licencias, sin ROM, pack,
trust ni generador compilado. `package.installed_consumer` pasó 1/1 y verifica
el commit, el lock de backends y el parche. El replay Release produjo 18.000
frames completos y 4.500 continuaciones en 15 procesos sin divergencias.
Engine `fb3d9a8`, compilado Release/VPX con ese prefijo, pasó 222/223 CTest
en la primera ejecución: el único fallo leyó checkpoints de una revisión
anterior. Tras generar el corpus Release compatible, el test restante pasó
1/1 con 12 workers y 13.500 frames; la unión cubre 223/223 tests de ese build.
Su prefijo instalado superó 44 comprobaciones de payload y dos ZIP idénticos,
`ayther-engine-vpx-v0.1.0-rc.21-windows-x86_64.zip`, SHA-256
`9ae983e6ed015aa133701ba154de3f8e087879ff410222f3c91b2997408acfcb`,
contienen 122 entradas, aviso AGPL actualizado y ningún fixture excluido.
Un consumidor externo se configuró, compiló y ejecutó desde ambos ZIP extraídos.
Runtime se reconstruyó contra esos prefijos (rutas `Ayther_DIR`/`Tessera_DIR`
comprobadas) y aprobó 3/3 pruebas nativas, incluida GPU y audio físicos.
Estas identidades siguen siendo candidatas locales: aún no hay tag, asset,
checksum publicado, atestación ni lock Runtime nativo.
Runtime `a1fc482` configuró y enlazó con los dos paquetes extraídos y pasó
230/230 CTest, incluidos los tres perfiles Tessera. Engine pasó 247/247 CTest
en una pasada posterior a alinear CLI y biblioteca Tessera. En la primera, el
único fallo se debió a `AYTHER_TESSERA_CLI` cacheado desde una revisión vieja.
La validación de dispositivo local de FX-02 entregó 287.128 muestras
activas y cero faltantes SDL; FX-03 presentó 180 frames sin errores Vulkan.
Timestamps efectivos de presentación y cortes acústicos físicos siguen sin
observación. Son candidatos locales, no assets publicados: faltan publicación
autorizada de paquetes, lock real y CI sobre ese lock.

**Corte local posterior (2026-10-20):** el candidato limpio de Tessera en
`9e4f4c9` produjo dos ZIP reproducibles con SHA-256
`9960c1d285019de450ac11919893f0f34517a2e88298718a8cfa8bc0b5ff0d8c`
(28 entradas), oferta de fuentes en español y consumidor instalado aprobado.
El candidato Engine en `fb3d9a8` produjo dos ZIP idénticos con SHA-256
`9ae983e6ed015aa133701ba154de3f8e087879ff410222f3c91b2997408acfcb`
(122 entradas); su aviso AGPL ya está corregido. Los ZIP excluyen los cuatro
tipos de binarios no autorizados. Engine Release cubrió 223/223 casos entre la
pasada inicial y la repetición del checkpoint con corpus compatible; Runtime
pasó 3/3 pruebas de ruta nativa con GPU y audio físicos locales. El PR de Engine
y el de Runtime están en verde; el check de Tessera para `9e4f4c9` también
terminó en verde. Siguen pendientes la verificación final de los assets
publicados, el lock nativo y el perfil de dispositivo en CI.

Para intentar el perfil físico en CI se registró temporalmente el runner de
organización `ayther-mvp-arnion`. Los repositorios Tessera, Engine y Runtime son
públicos; la revisión automática rechazó habilitar jobs públicos en un grupo
propuesto. El runner se detuvo y se eliminó del registro de la organización sin
ejecutar jobs. Las pruebas locales conservan su evidencia, pero no sustituyen
el gate de CI aprobado. El usuario decidió mantener la validación GPU/audio
en los dispositivos locales y dejar ese gate de CI pendiente.

**Revisión técnica local T034 completada:** el commit `de825e5` instaló el
metadato `T034-local-verified` desde árbol limpio. El consumidor instalado
C11/C++20 pasó 1/1; dos ZIP idénticos de Tessera tienen SHA-256
`b20d79bed826ae3c766a45e262492acf4f345d9118a10297da2068c2b27c5215`.
El consumidor externo compiló y ejecutó con ese ZIP y el ZIP Engine
`9ae983e6…` extraídos. Runtime se reconfiguró contra ese par exacto y pasó
3/3 pruebas nativas, incluida GPU/audio físico. Los ZIP siguen siendo
candidatos locales: faltan tags, assets, atestaciones, lock nativo y consumo
de la entrega publicada. La decisión de dejar CI de dispositivo pendiente se
mantiene.

**Borrador Tessera `v0.1.0-mvp.2`:** el run
[`39468937984`](https://github.com/Ayther-Dev/AYTHER-Tessera/actions/runs/39468937984)
pasó build, CTest Release, instalación, empaquetado doble y atestación. El
primer tag `v0.1.0-mvp.1` falló antes de compilar por la comprobación de
release inexistente en PowerShell; no generó assets y no se reutilizó. El
borrador mvp.2 contiene solo `tessera-v0.1.0-mvp.2-windows-x86_64.zip`
(SHA-256 `40ae6447b3a5dd7ca4bd4aaed2fccc436118fd53bec6c97deeb6c2593bed5a8f`)
y `CHECKSUMS.sha256`
(SHA-256 `639ea54a7b3dd9def7c50130ba9b440f2b29a63478fc689f9a57749e3cba6cd6`).
Se descargaron y contrastaron ambos; `gh attestation verify` aceptó únicamente
el workflow `native-release.yml`, el tag mvp.2 y runner alojado. El ZIP tiene
28 entradas, fuente limpia `c69b9a3`, metadato `T034-local-verified` y cero
ROM/pack/trust/generador. Un consumidor externo compiló y ejecutó usando el
ZIP del borrador con el paquete Engine local. El borrador todavía no constituye
publicación final ni lock Runtime.

**Borrador Engine `native-v0.1.0-rc.22` y pareja fijada:** el primer tag
`native-v0.1.0-rc.21` falló antes de compilar por leer un `$LASTEXITCODE`
heredado después del script de versión; no creó assets. El run
[`39474881427`](https://github.com/Ayther-Dev/AYTHER-Engine/actions/runs/39474881427)
del tag nuevo pasó replay, CTest Release, 44 checks de payload, empaquetado
repetible y atestación. El borrador tiene solo
`ayther-engine-vpx-native-v0.1.0-rc.22-windows-x86_64.zip`
(SHA-256 `4f17c34cde082b39a21c4a5216d77bc53e161df896487f1f9e50ab8a3fc1bdf2`)
y `CHECKSUMS.sha256`
(SHA-256 `0da7437f81029aeb0265b8e2fb718303e1b8cf30bc5fb92766228f7a48e14f4b`).
Se descargaron, contrastaron y verificaron con SLSA para el workflow y tag
exactos; el ZIP contiene 122 entradas y ninguno de los binarios de fixture
excluidos. El consumidor externo compiló y ejecutó con los dos ZIP borrador;
Runtime pasó su build completo y 233/233 CTest secuenciales sobre esa pareja,
incluidas las pruebas de FX-03, GPU y audio.
Runtime confirmó y subió ambos locks nativos exactos en el PR #55, mientras
rc.19 permanece como predeterminado. Los borradores aún requieren checks de
ramas, publicación autorizada y validación desde URL pública. El gate físico
GPU/audio en CI permanece pendiente.

**Publicación de software verificada:** los checks de los tres PR borradores
terminaron en verde; los releases de
[Tessera `v0.1.0-mvp.2`](https://github.com/Ayther-Dev/AYTHER-Tessera/releases/tag/v0.1.0-mvp.2)
y [Engine `native-v0.1.0-rc.22`](https://github.com/Ayther-Dev/AYTHER-Engine/releases/tag/native-v0.1.0-rc.22)
son prereleases públicos, cada uno con exactamente un ZIP de software y
`CHECKSUMS.sha256`. Las URL canónicas del lock se descargaron sin credenciales
de release; hashes, manifiestos, atestaciones, revisión/fuente y exclusiones
coincidieron. El consumidor externo compiló y ejecutó con ambos prefijos
públicos. Runtime reconfigurado contra ellos pasó build completo y 233/233
CTest secuenciales, incluidos FX-03, GPU y audio locales. Se conservaron
fuera de publicación ROM, pack, trust y generador compilado. T031/T036 y el
gate físico GPU/audio de CI siguen pendientes por la decisión del usuario;
T032 conserva el incidente de regularidad/audio descrito en la línea base.

**Rendimiento del par público:** el runner enlazado al core instalado ejecutó
dos series de 3×180 frames con 30 de calentamiento. Los hashes por modo
permanecieron estables, los 361 eventos no tuvieron pérdidas y las seis
repeticiones dieron cero demanda adicional de audio no disponible. La entrega
al presentador tuvo p99 18,261–19,141 ms y máximo hasta 24,681 ms; una
repetición tuvo `runtime_device` p99 de 2,506 ms. La
[línea base](performance-baseline.md) conserva tanto esa variación como el
episodio anterior de 337 muestras; T032 y CA-11 siguen abiertos.

**Audio activo FX-02:** dos series adicionales de 3×180 frames con B mantenido
produjeron 287.128 muestras activas por repetición y cero demanda faltante
según SDL. La entrega tuvo p99 18,103–18,929 ms y máximo hasta 27,683 ms.
Estos resultados locales no sustituyen el perfil GPU/audio físico de CI.
