# Procedencia del header de observación (TP-006.1)

Spec 008, RF-3.0. Medido el 2026-11-03.

| Copia | Origen | Estado |
|---|---|---|
| Referencia | fork gpgx, `core/ayther/ayther_api.h`, etiqueta `ayther-abi-1.10-r4` (`7109a7c2`) | 1070 líneas |
| Engine | `AYTHER-Engine/include/ayther/libretro_host/ayther_api.h`, copia del tag `ayther-abi-1.10` (`752a6ff7`) | formateada con clang-format y con una cabecera de 70 líneas |

## Diff
Comparando sin espacios, líneas en blanco, directivas de preprocesador ni comentarios, las diferencias son solo comentarios (referencias a incidencias `#26`, `#32`… y reflujo de líneas). No hay diferencia de código: mismos tipos, ids y constantes.

## Decisión
Se parte del header r4. Tessera no copia el archivo: `tools/gen_observation_header.py` genera `include/tessera/tessera_observation.h` con constantes `TESSERA_OBS_*`, y `contract.observation_abi` compara cada una con la referencia en tiempo de compilación (`-DTESSERA_ABI_REFERENCE_HEADER=<ruta>`). La autoría del header está en `components/ayther-abi-header.md`.
