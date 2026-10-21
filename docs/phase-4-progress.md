# Fase 4 — resultado local de observación y sustitución

**Estado:** T025–T029 completadas para Engine–Tessera. La validación de consumo en Runtime corresponde a T031. No se abrió PR ni se usó GitHub Actions.

Tessera instrumenta una copia de la revisión fijada de ClownMDEmu mediante un parche versionado, aplicado solo en el árbol de build y rechazado si los hashes de las fuentes VDP no coinciden. El callback registra el origen ganador de cada píxel (plano, Window, sprite y slot SAT), el tile y su posición durante el render. La captura de tiles de plano A guarda la imagen RGBA de 8×8 y la máscara exacta de píxeles visibles; nunca infiere la identidad de un recurso a partir de VRAM/CRAM/SAT finales. Los modos que esta versión no puede atribuir íntegramente se declaran incompletos.

La región temporal 5 usa TLV versión 2. Conserva los campos 1–5 de raster por línea de la versión 1 y añade 6 (`source_complete` U32), 7 (celdas: coordenadas, palabra de tile, máscara visible e imagen RGBA) y 8 (procedencia por píxel). `region.flags` vale 3 solo si raster y procedencia son completos; de otro modo marca el bit 31 y la captura excluye la región de `complete_mask`. Cada lectura retenida conserva sus bytes aunque avance la sesión.

Engine activa una política de observación únicamente tras la confirmación transaccional de Tessera. Con un `.ay` válido y una captura completa del mismo frame/sesión/época, busca el hash de cada tile observado en el catálogo existente, respeta la primera `[[sub]]` aplicable y prioriza el override. Decodifica los assets antes de componer; solo altera los bits visibles del plano ganador, dejando sprites y demás píxeles iguales al frame original. La salida original de Tessera sigue prestada e intacta. La presentación usa un buffer propio de Engine.

Si faltan paquete, política confirmada, captura, completitud, versión reconocida, geometría compatible o asset válido, Engine devuelve el frame original completo con causa por frame. No conserva un caché de composición entre frames: el nuevo frame, paquete, override, control, reinicio o restore obliga a volver a validar y componer. Un fallo de restore conserva la política y su pendiente; el éxito cancela la pendiente e invalida la vista anterior. Tras reinicio, la política se rearma en la nueva época y la presentación espera la confirmación.

`integration.megadrive_observation` verifica reconstrucción exacta del original, procedencia, dos lectores, pérdida por retención, recuperación y conservación de estado/vídeo/audio con observación activa e inactiva. `ayther.integration.tessera_presentation_test` prueba FX-03 con catálogo magenta, override cian, prioridad de plano y orden SAT, bytes fuera de la máscara idénticos, originales sin cambios, respaldo por pérdida o formato no soportado, recuperación, paquete, controles, reinicio y restore. El pack firmado y sus dos assets quedan en `tests/fixtures/graphics-replacement` con generador y oráculos reproducibles.

La evidencia T021 regenerada con este `build_id` reunió 15 procesos, 18.000 frames completos y 4.500 de continuación, sin divergencias. Las mediciones comparativas por etapas y el consumo en Runtime siguen asignados a T032 y T031, respectivamente.

Validación local: 16/16 CTest de Tessera (incluido replay T021); 8/8 pruebas
nativas de Engine, con checkpoint de 12 procesos contra la evidencia nueva.
`git diff --check` no informó errores. Los cambios se guardan en commits
locales de cada repositorio, sin PR.
