# Fase 4 — avance local de observación

**Estado:** T025 iniciada; T026–T029 y Gate 4 pendientes. No se ha abierto PR
ni se ha usado GitHub Actions.

Tessera captura, cuando se solicita la región temporal, el índice de color y
la paleta vigentes al renderizar cada línea de Mega Drive. La región 5 contiene
un registro TLV versión 1: campos 1–4 (version, width, height,
raster_complete) en U32 y campo 5 (raster) en bytes. Cada línea del raster
lleva 256 colores U16LE y después width índices U8. El bit 0 de
region.flags señala filas completas; el bit 31 señala captura incompleta y
excluye la región temporal de snapshot.complete_mask. Los otros campos y
regiones conservan sus contratos existentes. Una captura retenida conserva sus
bytes aunque la sesión avance.

La prueba integration.megadrive_observation comprueba reconstrucción del
frame original, estado/vídeo/audio iguales con observación activa e inactiva,
dos lectores, pérdida por presupuesto de capturas y recuperación en el siguiente
frame completo. Los datos finales VRAM/CRAM/SAT y este raster **no identifican
por píxel el tile o sprite de origen ni todos los cambios internos del VDP**.
Engine debe presentar el frame original completo cuando una sustitución
necesite esa atribución o el raster esté incompleto. Por ello, T025 no se
declara cerrada hasta fijar y verificar la atribución temporal necesaria para
FX-03.

FX-03 sigue pendiente de un asset gráfico propio, un paquete .ay con
metadatos y reglas reales y oráculos de píxeles para override/catálogo,
[[sub]] y solapamientos. Las pruebas Engine de controles, validación de
paquetes y presentación nativa (T027–T029) tampoco están implementadas. El
generador make_test_pack existente solo empaqueta un asset de texto; no
constituye FX-03.
