# Hoja de ruta de Tessera

Fuente: spec 008 (`specs/008-AYTHER-tessera-parity` del monorrepo) y su plan. Borrador (TP-010.1), pendiente de fusión.

## Decisiones de alcance
- DA-1: núcleo propio con paridad con gpgx; Lab, Runtime y Play CE lo incluyen.
- DA-2: toda la observación de la ABI 1.10, salvo los canales de supresión (PQ-1).
- DA-3: licencia MPL-2.0; ClownMDEmu queda como referencia local no distribuida.
- DA-4: híbrido permisivo: Moira (MIT), chips `z80.h` (zlib) y ymfm (BSD-3); VDP, bus, I/O y PSG propios.
- DA-8: gpgx es la base de comparación y el único core externo por ahora.

## Fases y puertas
| Fase | Contenido | Puerta |
|---|---|---|
| 0 | Fundaciones: política de sala limpia, banco diferencial, contratos | G0 |
| 1 | Mega Drive de cartucho con paridad de framebuffer y escrituras de audio | G1 |
| 2 | Paridad de observación y coste de la observación | G2 |
| 3 | Integración en Engine, Lab y Runtime; Tessera por defecto | G3 |
| 4 a 7 | Más sistemas y extensiones, planificadas al llegar | G4 a G7 |
