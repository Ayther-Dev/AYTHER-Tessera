# Procedimiento de determinismo (TP-007.3)

Spec 008, RF-4.2, PQ-12. Define `integration.md_determinism`.

1. **Procesos.** Se lanzan dos procesos independientes del CLI con la misma ROM, región y traza `TSTRACE1`.
2. **Duración.** 3 000 frames desde arranque en frío.
3. **Modos.** Se repite con todas las suscripciones y con ninguna. La salida de vídeo, audio y el hash de estado deben ser iguales en los cuatro casos.
4. **Comparación.** Hash por frame del framebuffer, del PCM y del savestate cada 600 frames. Cualquier diferencia falla la prueba e informa del primer frame.
5. **Alcance.** Mismo sistema operativo y arquitectura. La igualdad entre plataformas no se exige (PQ-12).
