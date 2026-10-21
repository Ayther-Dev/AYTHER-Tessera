# Resultados de fase 3 — Mega Drive, FX-02 y checkpoints

**Fecha:** 2026-10-19. **Alcance:** T019–T024. La evidencia es local; no se
ejecutaron GitHub Actions ni se abrió un PR. El job existente mantiene replay
FX-01 y suites breves; la matriz Mega Drive de 627 s se ejecuta localmente para
evitar ese coste en cada push. El trabajo de presentación HD,
FX-03 y Runtime corresponde a fases posteriores.

## Implementación

Tessera incorpora ClownMDEmu, seleccionado en Gate 0, como backend de producto
Mega Drive. `SystemDetector` reconoce el contenido y `SystemRegistry` despacha
la fábrica nativa. La sesión entrega RGBA8 320×224, PCM original S16LE estéreo
de 48 kHz, reloj lógico, entradas completas y estado canónico. La ROM copiada
Engine/Lab conserva su SHA-256 `c7666ec2a4d05ad3df8ce14648b5abd5bc4b0f9711c2edcaad228e127070c34f`;
no se ha modificado ni se le atribuye audio que no programa. La configuración
Mega Drive de esta fase acepta el perfil NTSC/overseas con memoria inicial de
64 KiB en cero; rechaza otras condiciones en vez de simular soporte.

FX-02 es una ROM separada, generada desde fuente propia MIT. Su manifiesto fija
versión, derechos, hashes de ROM/generador/trazas/oráculo y la entrada B en los
frames 301–600. Las ventanas 100–200, 350–550 y 650–850 comprueban RAM,
VRAM, CRAM, píxel central y PCM. La acción debe producir azul→rojo→azul y
silencio→PSG activo→silencio; el control mantiene el resultado neutral. Silencio
constante o entrada ignorada hacen fallar el oráculo aunque los hashes sean
repetibles.

El checkpoint `TSCHK001` contiene estado de máquina y audio, reloj y cursor de
replay, hashes de contenido/backend/configuración/identidad inicial y la traza
vinculada por SHA-256. K representa el límite posterior al frame K; la primera
entrada tras restaurar es K+1. Se excluyen HD y controles pendientes. Los TLV
se validan antes de construir una máquina candidata, y el intercambio ocurre
solo después de comprobar su re-encoding. El guardado local escribe un temporal
en el mismo volumen, vacía el archivo y publica por operación atómica de Windows.
El contrato cubre interrupción de proceso y fallos de lectura, escritura, espacio,
permisos y publicación, sin prometer durabilidad ante pérdida eléctrica.

Engine consume el paquete instalado de Tessera por la fachada `tessera_native`
y compara estado, vídeo y audio originales con el runner autónomo. La prueba
integrada crea una nueva sesión y un proceso para cada continuación. No usa la
ruta libretro ni entrega PCM al dispositivo legacy durante este test.

## Validación local

| Comprobación | Resultado |
| --- | --- |
| Replay autónomo dev Mega Drive | 15 procesos, 18.000 frames completos y 4.500 frames continuados; tres escenarios (ROM copiada, FX-02 control y acción), observación desactivada/activada y checkpoints 500/1.500/2.500; cero divergencias. |
| FX-02 autónomo | 503 evaluaciones del oráculo por ejecución observada; la acción incluye 201 comprobaciones de audio activo y 302 neutrales. |
| Formato y E/S | Round-trip, versiones, rangos, TLV externo/interno corrupto con hash externo válido, primer guardado, reemplazo y fallos inyectados; proceso detenido tras escritura temporal parcial. |
| Restauración FX-01 | Rechazo sin mutación, exportación sin cancelar pendiente, cancelación explícita al éxito aun con cola saturada, captura histórica retenida, K+1 y reset posterior. |
| Producto Mega Drive | Detección, identidad de ROM, geometría, PCM/reloj y rechazo de estado de máquina corrupto sin alterar sesión. |
| Engine nativo, build compatible | 12 procesos y 13.500 frames: 3 baselines de 3.000 y 9 continuaciones de 500. Cero divergencias frente a la evidencia autónoma; oráculos FX-02 aplicados también desde Engine. CTest 1/1 en 304,39 s. |
| Regresiones locales finales | Dev 14/14 pruebas breves más replay Mega Drive 1/1; Release 14/14 pruebas breves; ASan 15/15 pruebas breves, incluido su replay FX-01 y negativo ASan esperado. |
| Regresiones Engine previas | 4/4 pruebas nativas de fase 2; el contrato de checkpoint de FX-01 se comprueba según la capacidad negociada, compatible también con el pin CI anterior. |

La evidencia autónoma final está en
[`build/evidence/phase3-RelWithDebInfo-OFF/t021-42760-390200281/summary.json`](../build/evidence/phase3-RelWithDebInfo-OFF/t021-42760-390200281/summary.json).
Sus informes y checkpoints son reproducibles y están ignorados por Git. La
prueba de 3.000 frames tarda unos 627 s en la máquina local; por ello se ejecutó
una vez con el código final, y los checks breves se repiten por perfil.

ASan detectó durante la primera pasada un acceso inválido al relanzar una
excepción desde la ruta interna de restore. La corrección evita ese `catch`,
valida TLV de checkpoint con códigos propios y añadió regresiones para
corrupción externa e interna. Cuatro pruebas de sesión, formato, guardado y
restore pasaron después en diagnósticos; el primer resultado fallido no se
considera aprobación.

## Alcance que sigue pendiente

FX-03 aún necesita imagen y `.ay` reales, observación temporal suficiente,
composición, precedencia visual y respaldo original comprobados desde Engine.
Runtime, métricas completas de rendimiento y la revisión de obligaciones de
distribución de ClownMDEmu pertenecen a T025–T036. El paquete instalado lleva
sus bibliotecas y avisos; la revisión de distribución T034 sigue pendiente.
