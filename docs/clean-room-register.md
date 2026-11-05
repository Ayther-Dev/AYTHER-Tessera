# Registro de lectura de c贸digo de gpgx

Spec 008, RF-7.1 y PQ-14. Una persona que ha le铆do c贸digo de gpgx no escribe el componente que reproduce ese comportamiento.

| Persona | Qu茅 ley贸 | Cu谩ndo | No puede escribir |
|---|---|---|---|
| El responsable del proyecto, quien instrument贸 el fork gpgx (`ayther-abi-1.10-r4`) | C贸digo de gpgx relacionado con la observaci贸n AYTHER y con el VDP, el bus y el audio que se instrument贸 | Hasta 2026-11-03 | Temporizaci贸n del VDP, orden de sprites, escrituras FM y bandas raster de Tessera (`src/md/vdp`, `src/md/audio`) |
| Sesi髇 de Claude Code (2026-11-05, trabajo de TP-108.4) | C骴igo de gpgx: `get_region` y la tabla de excepciones por producto/checksum (`core/loadrom.c`), `io_reset` (`core/io_ctrl.c`), el refresco de bus del 68000 (`core/m68k/m68kcpu.c`, `m68kops.h`), la cadena de actualizaci髇 de entradas (`core/system.c`, `libretro/libretro.c`), y `core/debug/audio_probe.h` (ventana de coincidencia, anillo) | 2026-11-05 | Detecci髇 de regi髇 (`src/md/region.hpp`, TP-108.4: reproduce la regla le韉a, hay que rehacerla por caja negra), modelo de refresco del 68000, orden de sondeo de entradas y el transporte y agrupaci髇 de `poll_audio_events` (TP-107.6) |
| Sesi贸n de Claude Code (2026-11-05, rehecho de TP-108.4 en sala limpia) | **Nada de gpgx**: no abri贸 c贸digo de gpgx, ClownMDEmu ni ares, ni `third_party/`, ni el commit 10ddf81 (revertido en 588e336). Conocimiento previo: el formato p煤blico de la cabecera de Sega (campo de pa铆s en $1F0: letras J/U/E o un d铆gito hexadecimal con bits 1 = Jap贸n NTSC, 2 = Jap贸n PAL, 4 = Am茅rica, 8 = Europa). gpgx se us贸 solo como binario, por el banco (`tessera_diffbench --libretro`), observando `SYSTEM.region_pal`, el registro de versi贸n de E/S y las l铆neas por fotograma con la ROM `region_detect` | 2026-11-05 | Nada: es la autora limpia de `src/md/region.hpp` (TP-108.4); su regla viene de la especificaci贸n p煤blica y de las observaciones siguientes |

Observaciones de caja negra de TP-108.4 (gpgx como binario; ROM `region_detect`, 865 cabeceras comparadas con el n煤cleo nativo sin diferencias, 24 juegos del corpus iguales):

- Solo se leen los cuatro primeros bytes de $1F0; una letra o d铆gito en el quinto byte o despu茅s no cuenta.
- J, U, E (may煤scula o min煤scula) marcan un bit cada una; cualquier otro d铆gito hexadecimal (0-9, A-F, a-f) aporta sus cuatro bits; las marcas de los cuatro bytes se unen. La `E` siempre es la letra Europa, nunca el d铆gito 14.
- Con varias regiones gana Am茅rica, despu茅s Jap贸n NTSC, despu茅s Europa, y al final Jap贸n PAL (bit 2): `JE` y `EJ` dan Jap贸n, `JU` Am茅rica, `A` (Jap贸n PAL + Europa) da Europa, `3` (Jap贸n NTSC + PAL) da Jap贸n NTSC.
- Sin ninguna regi贸n reconocida (vac铆o, `X`, `0`, bytes 00 o FF) el resultado es Am茅rica/NTSC.
- Registro de versi贸n observado: Am茅rica $A0 (262 l铆neas), Jap贸n $20 (262), Europa $E0 (313), Jap贸n PAL $60 (313; solo por el bit 2 de la cabecera).
- Discrepancias con la especificaci贸n p煤blica: la especificaci贸n p煤blica no dice cu谩ntos bytes se leen ni el orden entre regiones, ni que las letras y los d铆gitos se mezclen; las tres cosas salen de la observaci贸n. No se aplican excepciones por producto o checksum: ning煤n juego del corpus las necesita y no se ha observado ninguna. gpgx no se pudo forzar a una regi贸n desde el banco (el banco no pasa opciones al n煤cleo), as铆 que la elecci贸n manual se comprueba contra los valores de versi贸n observados, no contra gpgx.

Toda persona nueva que lea c贸digo de gpgx se a帽ade aqu铆 antes de escribir en Tessera.
