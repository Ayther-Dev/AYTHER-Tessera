# Hoja de ruta de Tessera

Fuente: spec 008 (`specs/008-AYTHER-tessera-parity` del monorrepo) y su plan. Completa DA-1 a DA-8 y los enlaces (TP-010.1); pendiente de fusión en el PR unificado.

## Decisiones de alcance
- DA-1: núcleo propio con paridad con gpgx; Lab, Runtime y Play CE lo incluyen.
- DA-2: toda la observación de la ABI 1.10, salvo los canales de supresión (PQ-1).
- DA-3: licencia MPL-2.0; ClownMDEmu queda como referencia local no distribuida.
- DA-4: híbrido permisivo: Moira (MIT), chips `z80.h` (zlib) y ymfm (BSD-3); VDP, bus, I/O y PSG propios.
- DA-5: las tomas quedan ligadas al emulador que las grabó y declaran su procedencia; los packs `.ay` no se vuelven a hornear al cambiar de núcleo.
- DA-6: Lab y Play CE pueden usar Tessera o un core externo; la opción se elige en Ajustes generales.
- DA-7: la identidad de los elementos es la misma en ambos núcleos, así que los packs son portables entre ellos.
- DA-8: gpgx es el referente y la base de comparación; es el único core externo por ahora.

## Enlaces
- Spec y plan: `specs/008-AYTHER-tessera-parity/spec.md` y `plan.md` del monorrepo (locales, no están en este repositorio).
- Política de sala limpia: [clean-room-policy.md](clean-room-policy.md) y su [registro](clean-room-register.md).
- Contrato de observación: [observation-contract.md](observation-contract.md); excepciones de paridad: [parity-exceptions.md](parity-exceptions.md).
- Savestate propio: [savestate-v1.md](savestate-v1.md); línea base de rendimiento: [performance-baseline.md](performance-baseline.md).

## Fases y puertas
| Fase | Contenido | Puerta |
|---|---|---|
| 0 | Fundaciones: política de sala limpia, banco diferencial, contratos | G0 |
| 1 | Mega Drive de cartucho con paridad de framebuffer y escrituras de audio | G1 |
| 2 | Paridad de observación y coste de la observación | G2 |
| 3 | Integración en Engine, Lab y Runtime; Tessera por defecto | G3 |
| 4 a 7 | Más sistemas y extensiones, planificadas al llegar | G4 a G7 |
