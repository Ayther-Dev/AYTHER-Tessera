# AYTHER Tessera

Núcleo abierto de emulación multisistema para ejecución determinista,
observabilidad estructurada y sustitución de gráficos y audio en ejecución.

**Estado:** fases 0–4 completadas localmente (T001–T029) y fase 5 en curso
(T033 cerrada; T030–T032 y T034–T036 abiertas). El MVP aún no está aceptado.
Se seleccionó ClownMDEmu tras compararlo con ares y comprobar estado completo,
PCM canónico, replay, restauración entre procesos y respaldo original íntegro.
La fase 1 añade la API C, núcleo sintético autónomo y CLI con observación retenida,
controles transaccionales y replay por procesos; las fases 2–4, la integración
nativa con Engine, checkpoints y observación/sustitución. Hay un PR borrador por
repositorio (Tessera #1, Engine #48 y Runtime #55); falta publicar la cadena de
paquetes Tessera→Engine→Runtime. Consultar los resultados de las
[fases 0](docs/phase-0-results.md), [1](docs/phase-1-results.md),
[2](docs/phase-2-results.md) y [3](docs/phase-3-results.md), y el progreso de las
[fases 4](docs/phase-4-progress.md) y [5](docs/phase-5-progress.md).

**Núcleo Mega Drive propio (spec 008, desde `v0.2.0`).** El backend de producto
de Mega Drive es `megadrive`, un núcleo propio escrito en sala limpia y
contrastado contra Genesis Plus GX como caja negra: 68000 (Moira), Z80 (floooh/chips),
YM2612 (ymfm), PSG, VDP con FIFO y DMA, mandos de tres y seis botones, SRAM y la
API de observación 1.1. Se compila por defecto (`TESSERA_BUILD_MEGADRIVE=ON`) y
es el que se elige para contenido Mega Drive cuando no se pide un backend por id.
Las [excepciones de paridad](docs/parity-exceptions.md) aprobadas y el
[contrato de observación](docs/observation-contract.md) documentan lo que
difiere del oráculo y por qué.

## Alcance y arquitectura aprobados

El MVP inicial es Windows x64 y Mega Drive. Tessera detectará el sistema y
despachará su implementación mediante una API C nativa, utilizable sin interfaz
gráfica. Engine consumirá esa API directamente y conservará identificación de
recursos, paquetes `.ay` y composición HD. Runtime y Lab consumirán Engine.

La ruta nativa no requiere libretro. Observación y presentación deben conservar
el estado emulado y las salidas originales; el respaldo utiliza el vídeo original.
Otros sistemas, editor nuevo y sustitución sonora quedan fuera del MVP.
La ampliación multisistema y de audio requiere implementación y validación propias.

El MVP debe demostrar replay de 3.000 frames, restauración entre procesos,
capturas coherentes, sustitución gráfica real y presentación mediante Runtime.
El qualifier de fase 0 verificó tres pares de 3.000 frames y seis continuaciones
de 500 frames, con hashes de estado, vídeo y PCM sin divergencias. Son pruebas
privadas de viabilidad. FX-01 verifica ahora ambos perfiles por la API pública:
doce procesos nuevos, 36.000 frames y tres hashes por frame contra un oráculo
independiente. Engine y Runtime siguen pendientes, al igual que FX-02 completo
y FX-03. La demo propia de fase 0 es una semilla de FX-02.

## Compilación y validación local

Se requiere Windows x64 con Visual Studio/MSVC v145 14.51 o posterior, SDK Windows,
LLVM `clang-cl`, CMake y Ninja. Los presets y el wrapper usan `clang-cl` con ABI
MSVC y CRT `/MD`; el wrapper configura el entorno x64 y comprueba
una caché de dependencias fijadas por hash. Desde la raíz, con la caché disponible:

```powershell
./tools/bootstrap-dependencies.ps1 -Offline
./tools/check.ps1 -Preset windows-dev -Jobs 1
```

Ese preset construye lo que se distribuye, con el núcleo `megadrive` y sin
ClownMDEmu, y ejecuta sus pruebas unitarias y de contrato. El banco diferencial
contra gpgx usa `windows-md-dev`. Para comparar con ClownMDEmu se usa el árbol de
referencia, que requiere preparar las fuentes fijadas del backend con
`./tools/bootstrap-backends.ps1` y no es instalable; allí cada prueba
`integration.megadrive_*` corre con `megadrive` y, con el sufijo `_clown`, con
ClownMDEmu:

```powershell
./tools/check.ps1 -Preset windows-clown-reference -Jobs 1
```

Estos comandos ejecutan unidad, contratos C/C++, CLI y replay sintético.
Las descargas requieren `-Download` explícito. La [guía de validación](specs/001-mvp-core/quickstart.md)
incluye diagnósticos, SDK Engine y probes. Mantener una compilación activa;
los candidatos se construyen con un trabajo para limitar memoria.

```powershell
./build/windows-dev/src/tessera.exe backend list
./build/windows-dev/src/tessera.exe demo --backend synthetic.complete --scenario action --frames 3000 --observer two-readers
```

La [hoja de ruta](docs/roadmap.md) resume las decisiones de alcance, las fases y las puertas.
La [guía de CLI](docs/cli.md) describe trazas, generación de fixtures e informes.
Mega Drive se reconoce por bytes y se abre con `megadrive`; ClownMDEmu, en el
árbol de referencia, solo se elige por su id. Los perfiles TEST no anuncian
checkpoint; su implementación sigue T022/T023.

## Documentación del MVP

- [Constitución](docs/constitution.md), [especificación](specs/001-mvp-core/spec.md)
  y [plan técnico](specs/001-mvp-core/plan.md).
- [Contrato ABI C 1.0](specs/001-mvp-core/contracts/native-api.md),
  [encabezado público](include/tessera/tessera.h)
  y [tareas](specs/001-mvp-core/tasks.md).
- [Entorno comprobado](docs/development-environment.md),
  [evaluación de candidatos](docs/backend-selection.md) y
  [resultados y límites](docs/phase-0-results.md).

## Licencias y contribuciones

El código y la documentación originales de Tessera se ofrecen bajo
[MPL-2.0](LICENSE), la misma licencia que Engine, Runtime y Play CE. Los
paquetes `v0.1.0-mvp.1` y `v0.1.0-mvp.2` se publicaron antes del cambio y
conservan MIT para el código propio.
Los componentes externos conservan sus licencias y obligaciones: consultar el
[inventario](legal/DEPENDENCIES.csv) y los
[avisos de terceros](legal/THIRD_PARTY_NOTICES.md).

ClownMDEmu, clown68000 y clownz80 (AGPL-3.0) ya no forman parte de lo que se
instala ni se publica: el backend Mega Drive basado en ellos es una referencia
local para el banco diferencial. Se compila solo con
`-DTESSERA_BUILD_CLOWN_REFERENCE=ON` (preset `windows-clown-reference`), en un
árbol separado que no se puede instalar, y ningún artefacto distribuido contiene
código AGPL. Desde `v0.2.0` el paquete incluye el núcleo propio `megadrive`,
compilado con Moira (MIT), el Z80 de floooh/chips (Zlib) e ymfm (BSD-3-Clause),
cuyas licencias viajan en `share/tessera/licenses/`.

No incorporar código restringido de Genesis Plus GX sin permisos adicionales
suficientes. La disponibilidad de fuentes no concede por sí sola derechos
comerciales. Juegos, BIOS, audio y gráficos tienen derechos independientes;
los fixtures públicos deben ser propios o contar con autorización explícita.

Las contribuciones deben incluir validación reproducible y procedencia del
material aportado, conforme a la constitución del proyecto.
