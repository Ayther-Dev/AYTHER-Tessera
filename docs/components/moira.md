# Moira (CPU 68000)

Spec 008, DA-4, RNF-4. Verificado el 2026-11-03 contra el repositorio público.

## Licencia
MIT (`Moira/LICENSE`, copyright 2019-2023 Dirk W. Hoffmann). El `LICENSE` raíz aclara que solo el núcleo Moira es MIT; el probador integra Musashi y GNU binutils con otras licencias, y **no se incorporan**. Sin copyleft.

## Procedencia
`github.com/dirkwhoffmann/Moira`, commit `ce28239b0507ebfe1bf2ad2b9c9252dc2f2cc031` (rama master). Solo se usaría el directorio `Moira/`. Fila en `legal/DEPENDENCIES.csv`.

## Estado completo
Los miembros de estado (`clock`, `reg`, `queue`, `ipl`, `fcl`, `fcSource`, `exception`, `flags`, `readBuffer`, `writeBuffer`, `irqMode`) son `protected`. Una clase derivada de `Moira` puede serializarlos todos para el savestate (RF-4.1). Existen accesores públicos (`getClock`, `setClock`, `getIRC`). Para 68000 se ignoran `iCache` y `budget` (solo 68020).

## Temporización
`virtual void sync(int cycles)` recibe los ciclos de cada acceso y permite sincronizar el bus (VDP, Z80) con granularidad de acceso. `read8/16` y `write8/16` son virtuales, lo que permite contar esperas de bus. Suficiente para el modelo de tiempo del plan §1.3. La precisión por ciclo de la contención del bus de la MD es trabajo de Tessera (fase 1) y se mide con el banco diferencial.
