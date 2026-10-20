# FX-02: demo Mega Drive con audio activo

La fuente es el generador propio [main.cpp](../../backend_probe/main.cpp),
derivado de la semilla T005 y distribuido bajo MIT (`LICENSE` en la raíz).
Genera una ROM de 32 KiB separada de la copia Engine/Lab, dos trazas completas
y oráculos derivados de sus instrucciones 68000 antes de ejecutar el emulador.
La ROM copiada `ayther_synthetic.md` permanece intacta y no programa FM/PSG.

El [manifiesto](manifest.json) fija versión, derechos, SHA‑256 de fuente,
contenido, trazas y oráculo. Los binarios y JSON completos se generan bajo
`build/` con `tessera_audio_fixture_generator --generate DIRECTORIO`; no se
incluyen duplicados de 2 MiB en Git. La prueba T021 valida los hashes antes de
consumirlos y los reproduce por la API autónoma y Engine.

El programa lee el botón B del puerto 0. La traza de acción lo mantiene pulsado
en frames 301–600; el control lo deja libre. El bucle escribe RAM `0xFF0000`,
VRAM `0xFFFC`, CRAM color 0 y la atenuación del tono 0 de PSG. En las ventanas
centrales 100–200, 350–550 y 650–850, los valores esperados están fijados
en el manifiesto. El vídeo original central cambia de azul a rojo y vuelve a
azul. El PCM original es S16LE estéreo a 48 kHz: la ventana activa debe tener
variación mayor que 16; las ventanas neutrales, tras el transitorio, quedan en
[-2, 2]. La señal activa seguida por silencio previsto prueba la transición;
silencio constante falla. Son oráculos funcionales independientes de los tres
hashes deterministas.

El audio procede de PSG real de ClownMDEmu y del mezclador entero causal
auditado en Gate 0; no hay sustitución de audio ni mixer de dispositivo. Las
ventanas se comprueban a través de Tessera y de la ruta nativa de Engine.
