# Registro de lectura de c贸digo de gpgx

Spec 008, RF-7.1 y PQ-14. Una persona que ha le铆do c贸digo de gpgx no escribe el componente que reproduce ese comportamiento.

| Persona | Qu茅 ley贸 | Cu谩ndo | No puede escribir |
|---|---|---|---|
| El responsable del proyecto, quien instrument贸 el fork gpgx (`ayther-abi-1.10-r4`) | C贸digo de gpgx relacionado con la observaci贸n AYTHER y con el VDP, el bus y el audio que se instrument贸 | Hasta 2026-11-03 | Temporizaci贸n del VDP, orden de sprites, escrituras FM y bandas raster de Tessera (`src/md/vdp`, `src/md/audio`) |
| Sesi髇 de Claude Code (2026-11-05, trabajo de TP-108.4) | C骴igo de gpgx: `get_region` y la tabla de excepciones por producto/checksum (`core/loadrom.c`), `io_reset` (`core/io_ctrl.c`), el refresco de bus del 68000 (`core/m68k/m68kcpu.c`, `m68kops.h`), la cadena de actualizaci髇 de entradas (`core/system.c`, `libretro/libretro.c`), y `core/debug/audio_probe.h` (ventana de coincidencia, anillo) | 2026-11-05 | Detecci髇 de regi髇 (`src/md/region.hpp`, TP-108.4: reproduce la regla le韉a, hay que rehacerla por caja negra), modelo de refresco del 68000, orden de sondeo de entradas y el transporte y agrupaci髇 de `poll_audio_events` (TP-107.6) |

Toda persona nueva que lea c贸digo de gpgx se a帽ade aqu铆 antes de escribir en Tessera.
