# Procedencia del fixture Engine/Lab

Fecha de incorporación: 2026-10-19. Autorización: el usuario proporcionó las
rutas de la demo y del generador y solicitó incorporar una copia a Tessera.

## Fuentes preservadas

| Copia local | Archivo de origen |
| --- | --- |
| `upstream/tools/common/synth_rom.h` | `C:\ayther\AYTHER-Engine\tools\common\synth_rom.h` |
| `upstream/include/ayther/ayther_env.h` | `C:\ayther\AYTHER-Engine\include\ayther\ayther_env.h` |
| `upstream/include/ayther/ayther_file.h` | `C:\ayther\AYTHER-Engine\include\ayther\ayther_file.h` |
| `upstream/tools/make_test_pack/main.cpp` | `C:\ayther\AYTHER-Engine\tools\make_test_pack\main.cpp` |
| `upstream/LICENSE` | `C:\ayther\AYTHER-Engine\LICENSE` |

No se modificó ninguna de estas fuentes. Cada copia se comparó mediante SHA-256
con el archivo de origen. Los headers auxiliares son los originales de Engine;
no se copiaron los puentes propios de Lab.

El hash de `synth_rom.h` coincide también con
`C:\ayther\AYTHER-Lab\tests\fixtures\ayther-engine-v0.1.0-rc.6\synth_rom.h`:
`5a4c2b0b77b57f505ec9e4b6c31bd5f62cb67d9183e4cc2c95a29c85726faca8`.
El `PROVENANCE.md` de ese fixture de Lab identifica el tag Engine
`v0.1.0-rc.6`, commit `091ee4ca088dca66837ee285d8a02dd5a1d90e86`.
Esta correspondencia no atribuye la revisión rc.6 al ejecutable actual.

`upstream/LICENSE` contiene MPL-2.0 y el aviso central que incluye herramientas
y headers de Engine. Se conserva íntegro. Los términos específicos de distribución
de la ROM y del pack generados no están declarados por separado en los archivos
inspeccionados; deben registrarse expresamente antes de una distribución pública.
La incorporación local no les asigna MIT por inferencia.

## Generación de artefactos

- Ejecutable usado: `C:\ayther\AYTHER-Engine\build\windows-native\bin\make_test_pack.exe`.
- SHA-256 del ejecutable: `417e9d6828c738ae7a890324e1a86e460a6451854cb362c309dbe4c1548bb2d6`.
- Argumentos: rutas de `test_pack.ay` y `trust.toml`, identificador
  `crc32:rc000001` y ruta de `ayther_synthetic.md`, en ese orden.
- La fuente archivada del generador llama a `canonical_rom_path()`, que utiliza
  `Rom("AYTHER SYNTHETIC ROM")` y `program_canonical(r, true)`.
- La ROM temporal indicada por el usuario no existía al incorporar el fixture;
  se generó con el ejecutable disponible, sin ejecutar Lab ni un emulador.

Se ejecutó el generador dos veces, con destinos distintos. ROM, pack y registro
resultaron idénticos byte a byte según sus hashes SHA-256. Se verificaron además
el tamaño de la ROM, 32.768 bytes, y su identificador `SEGA MEGA DRIVE ` en 0x100.
Los hashes individuales y de las fuentes están en `SHA256SUMS`.

No se recompiló el generador ni se comprobó que su binario corresponda exactamente
a las fuentes locales archivadas. Tampoco se ejecutó el contenido en Tessera ni
se validó criptográficamente la firma del pack durante esta incorporación.
