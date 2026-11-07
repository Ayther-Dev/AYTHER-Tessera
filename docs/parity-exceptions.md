# Excepciones de paridad

Spec 008, D-3 y RF-10.5. El `diffbench` lee las filas que empiezan por `| EXC-`. `Afecta a identidad` solo puede ser «No»: si una divergencia tocara un identificador sería un defecto y no una excepción. Toda fila lleva aprobación del responsable.

| Id | Plano | Alcance | Descripción | Afecta a identidad | Aprobación |
|---|---|---|---|---|---|
| EXC-001 | audio_writes | golden-axe-world, frames 1–600 | Los ciclos de las escrituras FM/PSG difieren de gpgx (máx. 1 502 ciclos maestros); la secuencia (chip, addr, data) es idéntica. Causa: modelo del refresco DRAM del 68000 (TP-107.7). | No | Propietario, 2026-11-07 |
| EXC-002 | audio_writes | sonic1-usa-eur, frames 1–600 | Solo ciclos (máx. 98 en los frames 180/240); en 540/600 una escritura cae en el frame vecino (máx. 980). Misma causa. | No | Propietario, 2026-11-07 |
| EXC-003 | audio_writes | streets-of-rage-2-usa, frames 1–600 | Solo ciclos (máx. 70). Misma causa. | No | Propietario, 2026-11-07 |
| EXC-004 | audio_writes | phantasy-star-iv-usa, frames 1–600 | Aprobada para planos 1–2, estrechada. Divergencia conocida: desfase de ciclos maestros (refresco DRAM del 68000; el driver del Z80 arranca ~5 400 ciclos antes en gpgx, ~7 400 en el frame 10) más el desplazamiento de ±1 escritura DAC (reg 0x2A) por borde de frame. Las secuencias de escrituras no-DAC (FM/PSG) son idénticas en los frames medidos (470, 480, 490, 540, 600). El «máx. 121 085» del frame 480 es un artefacto del emparejamiento por posición (una escritura de borde cae en el frame vecino), no un desfase real (−2 364…+3 866). **Defecto conocido, no bloqueante, pendiente de caracterización:** en el frame 540 el núcleo nativo escribe dos veces 0x80 en el DAC que gpgx no escribe (una en el ciclo 1 475, otra a mitad de frame). | No | Propietario, 2026-11-07 |

**Condición común (2026-11-07):** cada excepción solo vale mientras no afecte a ningún identificador de TP-209.3 (hash de estado, firma de audio, etc.). Si el desfase de ciclos cambia un identificador, deja de ser una excepción y hay que arreglar el modelo de refresco del 68000 antes de declarar paridad de identidades. Esa corrección es línea de trabajo explícita antes de G2. Gunstar Heroes no tiene escrituras en 600 frames y no necesita excepción.
