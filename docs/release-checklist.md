# Coordinación de CI y entrega del MVP

Estado al 2026-10-20: tres PR borradores abiertos, uno por repositorio:
[Tessera #1](https://github.com/Ayther-Dev/AYTHER-Tessera/pull/1),
[Engine #48](https://github.com/Ayther-Dev/AYTHER-Engine/pull/48) y
[Runtime #55](https://github.com/Ayther-Dev/AYTHER-Runtime/pull/55).
No se crearon tags ni releases por tarea.

El usuario confirmó que no hay permiso verificable para publicar la ROM/pack
Engine/Lab ni la ROM FX-03 derivada. Se generan en `build/` con
`tools/generate-native-fixtures.ps1`. Las tres ramas publicadas se auditaron con
`git rev-list --objects main..HEAD`: no incluyen ROM, pack, trust ni generador
compilado. Ningún binario generado se sube como artifact de Actions.

| Repositorio | Validación local de este corte | CI remota exigida antes de T036 |
| --- | --- | --- |
| Tessera | 16/16 CTest breves con ROM generada, consumidor instalado, replay largo y comparación progresiva FX-03/FX-02 | `Core checks` aprobado en [run 39451047154](https://github.com/Ayther-Dev/AYTHER-Tessera/actions/runs/39451047154), commit `5704605` |
| Engine | Build/instalación y pruebas nativas locales; replay Release de 18.000 frames y 4.500 continuaciones sin divergencias | Todos los checks del [PR #48](https://github.com/Ayther-Dev/AYTHER-Engine/pull/48) aprobados; 225/225 CTest Windows y 4/4 perfiles nativos; pin Tessera `2549a0b` |
| Runtime | Build MSVC y 230/230 CTest locales; FX-03 progresivo 180/180 frames por repetición y etapa `engine_frame` aislada | Todos los checks del [PR #55](https://github.com/Ayther-Dev/AYTHER-Runtime/pull/55) aprobados; lock rc.19 todavía sin ruta nativa |

La rama `main` de Runtime protege estos contextos: `Windows / RelWithDebInfo`,
`Linux / RelWithDebInfo`, `Linux / ASan-UBSan-Fuzz`,
`Security / Dependency Review`, `Security / Secret Scan`,
`Analyze (actions)`, `Analyze (c-cpp)` y `Windows / QA`.
Las ramas `main` de Tessera y Engine no devuelven protección de rama en la API;
sus workflows declarados siguen siendo parte del gate del MVP.

El paquete Runtime publicado continúa fijado al Engine `v0.1.0-rc.19`, que no
exporta esta ruta nativa. El workflow de Engine ahora fija Tessera `5704605`
en el PR #48; sus nuevos checks están en curso. No cambiar locks
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
Restan los presupuestos sustentados por las series, el paquete Engine publicado
con la ruta nativa, el lock Runtime actualizado, su validación con los paquetes
publicados y la prueba GPU/audio en un runner físico. El inventario y los avisos de ClownMDEmu
ya reflejan la integración nativa; T034 aún exige verificar la clausura real
de la distribución combinada y sus avisos. T034–T036 y
CA-08/CA-11 permanecen abiertos.

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
Runtime `a1fc482` configuró y enlazó con los dos paquetes extraídos y pasó
230/230 CTest, incluidos los tres perfiles Tessera. Engine pasó 247/247 CTest
en una pasada posterior a alinear CLI y biblioteca Tessera. En la primera, el
único fallo se debió a `AYTHER_TESSERA_CLI` cacheado desde una revisión vieja.
La validación de dispositivo local de FX-02 entregó 287.128 muestras
activas y cero faltantes SDL; FX-03 presentó 180 frames sin errores Vulkan.
Timestamps efectivos de presentación y cortes acústicos físicos siguen sin
observación. Son candidatos locales, no assets publicados: faltan revisión de
distribución, publicación autorizada de paquetes, lock real y CI sobre ese lock.
