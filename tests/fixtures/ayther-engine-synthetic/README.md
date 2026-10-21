# Demo sintética de AYTHER Engine y Lab

Las fuentes originales se incorporaron a Tessera el 2026-10-19. Por decisión del
usuario del 2026-10-20, los binarios generados no forman parte de la distribución.
La ROM de 32 KiB se genera localmente; su extensión `.md` no indica Markdown.

| Archivo | Uso |
| --- | --- |
| `build/.../native-fixtures/roms/ayther_synthetic.md` | ROM canónica generada localmente. |
| `build/.../native-fixtures/test_pack.ay` | Pack firmado de prueba generado localmente. |
| `build/.../native-fixtures/trust.toml` | Registro local de la clave pública de prueba. |
| `upstream/tools/common/synth_rom.h` | Generador de ROM original, sin modificaciones. |
| `upstream/include/ayther/` | Dos headers originales requeridos por el generador. |
| `upstream/tools/make_test_pack/main.cpp` | Fuente original del generador de ROM y pack. |
| `upstream/LICENSE` | Licencia MPL-2.0 y aviso de alcance originales de Engine. |
| `PROVENANCE.md` | Procedencia, identidad del generador y alcance de la verificación. |
| `SHA256SUMS` | Identidades de las fuentes y salidas esperadas; las salidas se generan fuera de Git. |

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

Desde la raíz de Tessera, compilar primero `windows-dev` y el generador
`make_test_pack` de Engine. El comando siguiente genera ROM, pack, registro,
FX-02 y FX-03 bajo `build/` y verifica sus hashes:

```powershell
.\tools\generate-native-fixtures.ps1 `
    -TesseraBuild .\build\windows-dev `
    -EnginePackTool C:\ayther\AYTHER-Engine\build\windows-native\bin\make_test_pack.exe
```

La herramienta Engine escribe primero `%TEMP%\ayther_synthetic.md` y luego copia
al destino local. ROM, pack y registro se excluyen de Git y no se suben como
artefactos de CI. Compilar el generador de packs requiere Engine completo.

Los hashes permiten comprobar identidad de archivos; no son resultados de
emulación. Todavía no se han medido replay, audio, restauración ni sustitución
con un backend de Tessera.
