# Savestate v1 de Tessera (TP-007.1)

Spec 008, RF-4.1, D-4. Diseño; aún sin implementar (TP-110).

## Contenedor
- Contenedor `TSSTATE1` existente, `codec_id = "tessera.megadrive.v1"`.
- Cabecera TLV con la generación D-4: `emulator = "tessera"`, `determinism_contract = 1`, el `backend_id`, el `build_id` (informativo) y la versión del codec.
- Un estado de otra generación o de otro codec se rechaza con `TESSERA_CHECKPOINT_INCOMPATIBLE` (-20).

## Contenido por componente

| Componente | Bytes máximos |
|---|---|
| Cabecera TLV (generación, backend_id, build_id, codec) | 512 |
| 68000 (Moira: registros, cola de prefetch, IPL, flags, reloj) | 256 |
| Z80 (z80_t, RAM de 8 KiB, banco, bus request, reset) | 8704 |
| Work RAM de 64 KiB | 65536 |
| VDP: VRAM 64 KiB, CRAM 128 B, VSRAM 80 B, 24 registros, FIFO, DMA en curso, latches, contadores, SAT en caché | 67792 |
| YM2612 (ymfm save_restore) | 4096 |
| YM2413 (solo SMS, fase 6) | 1024 |
| PSG SN76489 | 128 |
| Cartucho: SRAM hasta 64 KiB, EEPROM hasta 8 KiB, bancos SSF2, estado de TMSS | 73984 |
| I/O: dos puertos, latch TH, control, tiempos de espera | 256 |
| Fase del bus y arbitraje (68000/Z80/DMA), reloj maestro | 256 |
| Continuidad del audio (resto de muestras, fase del reloj de mezcla, contadores de escrituras) | 4096 |
| Estado de observación que forma parte del estado (LAYER_MASK, LAYER_DIM, generaciones) | 512 |
| **Total** | **227152** (≈ 0.22 MiB) |

La suma es el peor caso con SRAM y EEPROM máximas y es menor que el límite de 2 MiB de RNF-2 (`integration.md_savestate`). Quedan fuera del estado `AUDIO_MUTE` y los búferes de observación, que se regeneran (plan §1.4).

## Reglas
- Orden de campos fijo y little-endian; sin `memcpy` de estructuras con relleno.
- Dos ejecuciones desde el mismo estado producen la misma salida (RF-4.2).
- El estado no es portable entre núcleos: el arranque en frío es el estado inicial común (D-7).
