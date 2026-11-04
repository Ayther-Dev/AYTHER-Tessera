# Política de sala limpia de Tessera

Origen: spec 008, RF-7.1 (`specs/008-AYTHER-tessera-parity/spec.md`). Rige para todo código del núcleo de la Mega Drive (`src/md/`) y sus pruebas.

## Reglas

- **Regla 1. No se lee ni se copia código de oráculos** (RF-7.1, primer punto). Ningún colaborador lee ni copia código de gpgx, ClownMDEmu ni ares para escribir el núcleo.
- **Regla 2. Fuentes permitidas** (RF-7.1, segundo punto). La documentación de hardware, las ROM de prueba y la observación de caja negra de los oráculos.
- **Regla 3. Identidades por caja negra** (RF-7.1, tercer punto). Los comportamientos de gpgx que fijan una identidad (RF-10.5) se averiguan con el banco diferencial, nunca leyendo su código.
- **Regla 4. Registro y separación** (RF-7.1, cuarto punto; PQ-14). Quien haya leído código de gpgx, por ejemplo al instrumentar el fork, queda en `docs/clean-room-register.md` y no escribe el componente que reproduce ese comportamiento.

## Procedimiento de caja negra

1. Se formula la pregunta como una diferencia observable (frame, línea, escritura de audio, byte de región).
2. Se ejecuta el oráculo por su interfaz libretro con una ROM de prueba o el corpus, sin abrir su fuente.
3. Se anota el resultado en `docs/parity-exceptions.md` o en la prueba, con el identificador de la entrada y del frame.
4. Se escribe el comportamiento a partir de la documentación de hardware y de esa observación.

## Revisión

Cada PR del núcleo se revisa contra esta política con `docs/pr-checklist.md`. La licencia de gpgx se consulta externamente y no forma parte de este repositorio (PQ-14). ClownMDEmu permanece como referencia local que nunca se distribuye.
