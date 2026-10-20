# Fase 4 — avance local de observación

**Estado:** T025 iniciada; T026–T029 y Gate 4 pendientes. No se ha abierto PR
ni se ha usado GitHub Actions.

Tessera captura, cuando se solicita la región temporal, el índice de color y
la paleta vigentes al renderizar cada línea de Mega Drive. El callback del VDP
puede dividir una línea entre Window y Plane A: ambos tramos se reúnen por
rango, y la conversión RGBA escribe solo los píxeles cubiertos por cada tramo.
La región 5 contiene
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

FX-03 tiene ya una ROM variante reproducible con superposición de sprites,
orden SAT y prioridad de plano, dos assets propios y un `.ay` firmado con
identidad de ROM, metadatos, dos `[[sub]]` coincidentes y hash verificado contra
el tile rojo realmente observado. `tests/fixtures/graphics-replacement` guarda
los artefactos, el generador y oráculos de píxeles originales y sustituidos.
El fixture satisface el contenido solicitado en T026; la dependencia T025 y
su consumo por composición aún faltan, por lo que T026 y Gate 4 siguen abiertos.

Engine ya abre un pack nativo como candidato independiente después de validar
identidad de ROM, metadatos obligatorios, reglas de tiles, rutas y referencias.
Antes de activarlo verifica todos los hashes del índice firmado en streaming,
con memoria acotada; un fallo conserva el pack anterior. El lector de tiles
respeta ahora la primera entrada `[[sub]]` aplicable incluso cuando ambas son
incondicionales; el override sigue prevaleciendo. Tests Rust y Engine con pack
firmado cubren estos cambios. Esto avanza T028, pero no la cierra: falta el
variante completa de FX-03, la presentación visual y pruebas integradas de
controles.
