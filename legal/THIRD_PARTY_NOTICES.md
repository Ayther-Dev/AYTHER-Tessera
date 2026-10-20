# Avisos de terceros y procedencia

Auditoría de Fase 0, 2026-10-19. Este registro distingue los headers auxiliares
fijados, las copias de referencia del fixture y los backends descargados para
evaluación. Descargar o registrar un candidato no lo selecciona ni acredita la
viabilidad técnica del Gate 0. Las licencias originales se conservan sin cambios.

El inventario verificable está en [DEPENDENCIES.csv](DEPENDENCIES.csv). Los hashes
SHA-256, tamaños, revisiones y URL de los archivos auxiliares y de evidencia están
en [dependencies.lock.json](../third_party/cache/dependencies.lock.json). Son hashes
de los bytes descargados; salvo el checksum publicado de json.hpp, no se afirma
que constituyan firmas o checksums publicados por los autores. Las rutas de
include del lock permiten consumir los headers locales sin descargas en el build.
El inventario registra material incorporado al workspace; no afirma que todos los
archivos se compilen o distribuyan en un producto.

## Dependencias auxiliares fijadas

**nlohmann/json 3.12.0**, commit `55f93686c01528224f448c19128836e7df245f72`.
Se incorporó únicamente el header amalgamado `single_include/nlohmann/json.hpp`
como [json.hpp](../third_party/cache/nlohmann-json-3.12.0/include/nlohmann/json.hpp).
Su SHA-256 coincide con el checksum de la [publicación oficial v3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0).
La finalidad prevista es leer y escribir los formatos JSON del núcleo y sus
herramientas; no se incorporan los tests, herramientas ni dependencias opcionales
del repositorio upstream. Copyright 2013-2025 Niels Lohmann; licencia principal
[MIT](../third_party/licenses/nlohmann-json-3.12.0/LICENSE.MIT).

El header conserva los avisos de Evan Nemerson, The Abseil Authors, Björn
Hoehrmann y Florian Loitsch. La [documentación de licencia de esa revisión](../third_party/licenses/nlohmann-json-3.12.0/upstream-license.md)
declara MIT para el decodificador UTF-8 y Grisu2, y CC0-1.0 para Hedley; se incluye
el [texto CC0-1.0](../third_party/licenses/nlohmann-json-3.12.0/CC0-1.0.txt).
El wrapper modificado de Hedley lleva además SPDX MIT en el propio header.
El bloque de fallback para `integer_sequence` conserva una referencia a Abseil
Apache-2.0; se incluye el [texto original de Abseil](../third_party/licenses/nlohmann-json-3.12.0/ABSEIL-APACHE-2.0.txt)
de la revisión citada en ese bloque. El fallback está condicionado a ausencia de
C++14 y no se activa con el C++20 aprobado; su texto permanece en la copia fuente.
Estos avisos embebidos no representan bibliotecas adicionales que instalar.

**PicoSHA2 1.0.1**, commit `161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29`.
Se incorporó únicamente [picosha2.h](../third_party/cache/picosha2-1.0.1/include/picosha2.h),
para hashes SHA-256 reproducibles. Copyright 2017 okdshin;
[MIT](../third_party/licenses/picosha2-1.0.1/LICENSE).
La [revisión oficial](https://github.com/okdshin/PicoSHA2/tree/161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29)
fija la fuente; este inventario no certifica propiedades criptográficas de una
integración todavía no realizada.

**doctest 2.5.3**, commit `2d0a9359a60c51affe2a9bebb1be1dca47868151`.
Se incorporó únicamente [doctest.h](../third_party/cache/doctest-2.5.3/include/doctest/doctest.h),
para tests locales, sin dependencia de runtime del núcleo. Copyright 2016-2023
Viktor Kirilov; [MIT](../third_party/licenses/doctest-2.5.3/LICENSE.txt).
La cabecera reconoce código de Catch/Catch2 y lest bajo Boost Software License
1.0; se conserva el aviso y se incluye [BSL-1.0](../third_party/licenses/doctest-2.5.3/BSL-1.0.txt).
Esta copia del texto procede de Catch2 v2.13.10; no atribuye esa versión al código
tomado por doctest, cuyo aviso no identifica las revisiones de esos fragmentos.
Fuente de versión: [publicación oficial v2.5.3](https://github.com/doctest/doctest/releases/tag/v2.5.3).

Al redistribuir estos headers o partes sustanciales se deben conservar los
copyrights y textos de permiso aplicables. Las copias originales de los headers,
incluidos sus avisos embebidos, acompañan a los textos de licencia aquí enlazados.

## Fixture de referencia Engine/Lab

El fixture [ayther-engine-synthetic](../tests/fixtures/ayther-engine-synthetic/README.md)
conserva [PROVENANCE.md](../tests/fixtures/ayther-engine-synthetic/PROVENANCE.md),
[SHA256SUMS](../tests/fixtures/ayther-engine-synthetic/SHA256SUMS) y la
[licencia original de Engine](../tests/fixtures/ayther-engine-synthetic/upstream/LICENSE).
Se verificaron los 11 hashes de SHA256SUMS, sin modificar ninguna copia.
Las cuatro fuentes preservadas son `synth_rom.h`, `make_test_pack/main.cpp`,
`ayther_env.h` y `ayther_file.h`; el aviso central de Engine las cubre con MPL-2.0.
Son referencias archivadas, sin afirmar integración o compilación en Tessera.
`tests/fixtures/graphics-replacement/synth_rom_fx03.h` es una variante de esa
fuente MPL-2.0 usada para generar ROMs de prueba durante el build. Su código
fuente y este aviso se conservan; los binarios resultantes se excluyen de
distribución por decisión del usuario.
Al redistribuir código cubierto se conserva MPL-2.0 y sus avisos; si se distribuye
en forma ejecutable, se debe cumplir la disponibilidad del código fuente cubierto
y comunicar cómo obtenerlo. Las modificaciones de archivos cubiertos conservan
las obligaciones MPL-2.0; la licencia del código propio de Tessera no las sustituye.

El mismo aviso de Engine excluye ROM, BIOS, cores, content packs y assets de su
concesión MPL-2.0 salvo declaración expresa del titular. No se encontró una
concesión separada verificable para `ayther_synthetic.md`, `test_pack.ay` y los
assets de ese pack. El registro `trust.toml` se conserva como metadato asociado
del fixture y también se mantiene fuera de una concesión inferida.
El inventario usa `LicenseRef-Permission-Pending` como estado interno: **no es una
licencia ni una autorización de redistribución**. El usuario confirmó el
2026-10-20 que no dispone de autorización verificable para publicar esos
binarios y pidió excluirlos. Solo se generan localmente y no se incorporan a
Git, paquetes ni artefactos de CI.

Antes de publicar esos artefactos debe constar una concesión expresa verificable
del titular que cubra ROM, pack y su contenido, o excluirlos de la distribución.
El SHA-256 del ejecutable generador, las dos generaciones idénticas y el enlace
parcial a una fuente rc.6 constan en PROVENANCE.md; no prueban correspondencia
binario-fuente, derechos de publicación, firma válida del pack ni corrección de
emulación. El ejecutable de Engine no se incorporó a Tessera. El fixture actual
no acredita sustitución gráfica, audio activo ni respuesta a controles.

## Backends: evaluación local y alcance construido

Las revisiones y hashes de sus archivos ZIP se conservan en
[sources.lock.json](../third_party/backends/sources.lock.json). Se contrastaron
los hashes del archivo ZIP disponible con ese lock. No se asume que un ZIP de
GitHub incluya los submódulos; los gitlinks de Clown se fijan por separado.

**ares v148**, commit `0aafd85789215e84e1e43415c07d4c88461b7899`:
el código principal declara ISC, pero el archivo
[LICENSE completo](../third_party/licenses/ares-v148/LICENSE) también contiene
avisos de terceros. La [fuente fijada](https://github.com/ares-emulator/ares/tree/0aafd85789215e84e1e43415c07d4c88461b7899)
es la referencia, sin declarar que todo su contenido sea ISC.
La evidencia CMake preservada muestra `libco`, headers `nall`, `sljit` y, para MD,
`qon`; CHD es opcional y está activado por defecto en upstream, con libchdr,
LZMA, miniz y zstd. El árbol también contiene frontend y componentes como ymfm
y librashader (MPL-2.0) que no deben confundirse con un enlace headless ya decidido.
El LICENSE agregado conserva, entre otros, BSD-2-Clause/BSD-3-Clause, zlib, MIT,
Apache-2.0 y MPL-2.0 para sus partes respectivas. La clausura exacta de archivos,
opciones, licencias y dependencias transitivas del target seleccionado deberá
contrastarse antes de integrarlo o distribuirlo; esta revisión no la da por cerrada.

**ClownMDEmu**, commit `88ef45a6585556e2247dd26b6c937d6b9fb4a12d`:
el [repositorio oficial](https://github.com/Clownacy/clownmdemu-core/tree/88ef45a6585556e2247dd26b6c937d6b9fb4a12d)
declara AGPL-3.0-or-later y conserva [LICENCE.txt](../third_party/licenses/clownmdemu-core-88ef45a/LICENCE.txt).
Sus gitlinks fijan `clown68000` a `bb198386ae3ee0cd6d246ff917f9941b02b5be6a`
([AGPL-3.0](../third_party/licenses/clown68000-bb19838/LICENCE.txt)), `clownz80` a
`aa4cae3127a5893ba7066bcc2d9e9ba8e88e1c3b`
([AGPL-3.0](../third_party/licenses/clownz80-aa4cae3/LICENCE.txt)) y `clowncommon` a
`ecee31fc78ee1eed901431d070239c9c6430607c`
([0BSD](../third_party/licenses/clowncommon-ecee31f/licence.txt)).
Los dos CPU fijan además su propio `clowncommon` a
`ddeff174c2121b40284883e0e1e3520f39d99227`
([0BSD](../third_party/licenses/clowncommon-ddeff17/licence.txt)); no se reemplaza
por el commit distinto del core. El lock y el bootstrap conservan ambas revisiones.
El inventario no extiende automáticamente «or later» a los subproyectos sólo
por la declaración del core; se conserva el texto observado en cada copia.
AGPL permite uso comercial y tiene obligaciones de copyleft y código fuente,
incluidas las relativas a interacción remota de versiones modificadas. La
[licencia preservada](../third_party/licenses/clownmdemu-core-88ef45a/LICENCE.txt),
sección 2, permite crear y ejecutar obras cubiertas sin transmitirlas. No se
identifica en esos términos un impedimento para la evaluación local autorizada.

El alcance construido de Gate 0 es el ejecutable headless ClownMDEmu: core,
intérpretes clown68000/clownz80 y sus revisiones de clowncommon, más harness,
codec y media propios MIT y headers nlohmann/json y PicoSHA2 con sus avisos.
Ese ejecutable combinado de evaluación se trata bajo AGPLv3, conservando los
avisos y licencias aplicables de cada fuente; el código propio conserva también
su concesión MIT. La matriz técnica aprobó Gate 0 y su selección local para
la integración posterior, sin iniciar la fase 1.
Esta etapa no publica ni distribuye el binario combinado ni ofrece interacción
remota; tampoco acredita cumplimiento de una distribución pública futura.

El smoke del SDK Engine es otro ejecutable: no incorpora ClownMDEmu ni sus CPU.
La integración de producto con Engine MPL-2.0 queda fuera de esta clausura local.
Una API C, un wrapper o separar bibliotecas no elimina por sí mismo obligaciones
AGPL. La ruta de obra mayor/licencia secundaria de MPL-2.0, sección 3.3, debe
evaluarse sobre los archivos y avisos concretos de la futura combinación;
no se da por resuelta para Engine, Runtime o Lab por este enlace mínimo.

T034 comprobará antes de distribuir la clausura real, fuentes correspondientes
y scripts necesarios, avisos y términos AGPL/MPL aplicables; si se ofrece
interacción remota con una versión modificada, también la obligación AGPL de
ofrecer las fuentes correspondientes a sus usuarios. Esta estrategia permite
la selección técnica local sin declarar una excepción por API ni presentar
AGPL como licencia no comercial. Los permisos pendientes del fixture original
permanecen sujetos al apartado anterior.

Los archivos CMake y `.gitmodules` bajo `cache/backend-evidence` son copias de
evidencia sin ejecutar. Sus revisiones y fuentes figuran en el lock auxiliar;
no sustituyen una auditoría de los archivos que termine enlazando el backend.
