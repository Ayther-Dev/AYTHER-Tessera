# Demo sintética de AYTHER Engine y Lab

Copia incorporada a Tessera el 2026-10-19 por indicación del usuario para el corpus
del MVP. La ROM es un binario Mega Drive de 32 KiB; su extensión `.md` no indica
un documento Markdown.

| Archivo | Uso |
| --- | --- |
| `ayther_synthetic.md` | ROM canónica generada por `make_test_pack.exe`. |
| `test_pack.ay` | Pack firmado de prueba de Engine. |
| `trust.toml` | Registro de la clave pública de prueba del pack. |
| `upstream/tools/common/synth_rom.h` | Generador de ROM original, sin modificaciones. |
| `upstream/include/ayther/` | Dos headers originales requeridos por el generador. |
| `upstream/tools/make_test_pack/main.cpp` | Fuente original del generador de ROM y pack. |
| `upstream/LICENSE` | Licencia MPL-2.0 y aviso de alcance originales de Engine. |
| `PROVENANCE.md` | Procedencia, identidad del generador y alcance de la verificación. |
| `SHA256SUMS` | Hashes de todos los archivos del fixture salvo este listado. |

Las fuentes de `upstream/` se conservan byte a byte, incluidos sus comentarios
en el idioma original. Mantienen su licencia de origen; no se relicencian con
la MIT de Tessera. La incorporación no selecciona C++ como lenguaje del núcleo
ni integra Engine o Lab como dependencias de ejecución de Tessera.

## Uso previsto en el MVP

Esta ROM sustituye la necesidad de crear otra demo Mega Drive desde cero para
las pruebas que pueda cubrir: ejecución, observación, cambios de estado gráfico,
replay, restauración y futura sustitución gráfica sobre recursos conocidos.
El backend sintético para probar contratos sigue siendo un componente distinto.

La escena dibuja una celda roja, un sprite verde y un fondo azul. Alterna datos
de CRAM y VRAM que no modifican los elementos visibles; no debe exigirse que
su imagen presentada cambie entre frames. El programa canónico no configura
FM ni PSG: se necesita otro estímulo con audio activo para aceptar los requisitos
sonoros y evitar una comparación vacía de silencio.

El pack contiene `manifest.toml` y `assets/tone.bin`; este último es texto de
prueba, no audio reproducible ni un reemplazo gráfico. No acredita por sí mismo
el requisito de sustitución del MVP ni define el futuro formato de Tessera.
Su clave fija es de fixture y `crc32:rc000001` es un identificador literal,
no el CRC calculado de la ROM.

## Regeneración con el ejecutable disponible

Desde la raíz de Tessera, el siguiente comando regenera los tres artefactos
utilizando el generador externo indicado por el usuario:

```powershell
$fixture = Join-Path (Get-Location) 'tests\fixtures\ayther-engine-synthetic'
& 'C:\ayther\AYTHER-Engine\build\windows-native\bin\make_test_pack.exe' `
    (Join-Path $fixture 'test_pack.ay') `
    (Join-Path $fixture 'trust.toml') `
    'crc32:rc000001' `
    (Join-Path $fixture 'ayther_synthetic.md')
if ($LASTEXITCODE -ne 0) { throw 'Fixture generation failed.' }
```

La herramienta escribe primero `%TEMP%\ayther_synthetic.md` y luego lo copia al
destino. Reemplaza los artefactos existentes. El ejecutable no se incorpora al
repositorio; compilar de nuevo el generador de packs requiere el proyecto Engine
completo. Los archivos de `upstream/` son una copia de referencia, no ese proyecto.

Los hashes permiten comprobar identidad de archivos; no son resultados de
emulación. Todavía no se han medido replay, audio, restauración ni sustitución
con un backend de Tessera.
