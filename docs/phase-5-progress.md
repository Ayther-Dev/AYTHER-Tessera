# Fase 5 — progreso local

Fecha: 2026-10-20. Estado: **T030–T036 cerradas**, con trazabilidad en la
[matriz de aceptación](../specs/001-mvp-core/acceptance-report.md). PR borradores
[Tessera #1](https://github.com/Ayther-Dev/AYTHER-Tessera/pull/1),
[Engine #48](https://github.com/Ayther-Dev/AYTHER-Engine/pull/48) y
[Runtime #55](https://github.com/Ayther-Dev/AYTHER-Runtime/pull/55); el lock
predeterminado de Runtime no cambió; sus locks nativos fijan los dos paquetes
públicos. Los párrafos de validaciones anteriores conservan el estado de aquel
candidato y no sustituyen este corte final.

T031 pasó el [perfil GPU/audio en CI físico privado de Lab](https://github.com/Ayther-Dev/AYTHER-Lab/actions/runs/39490219830)
con Runtime `7bccf53`; el runner efímero se retiró al terminar. El workflow
corrió sin subir ROM, pack ni trust y no requirió un cuarto PR. Las métricas
exactas y sus límites de interpretación constan en la
[línea base](performance-baseline.md).
T032 cerró la comparación progresiva, los contadores ON/OFF y la definición
del déficit de frames estéreo en el stream SDL para el formato físico 48 kHz
sin remuestreo; conserva el episodio aislado de 337 frames del candidato
anterior y la variación de entrega sin atribución causal.

## Runtime: GPU, dispositivo de audio y proveedor nativo

Runtime consume Engine→Tessera mediante `--provider tessera` sin core
libretro. Reconoce la ROM por contenido, prepara ocho controles completos por
frame, confirma la política de presentación y expone hashes de vídeo
original/presentado y PCM. Se añadieron ventana SDL/Vulkan y stream SDL de
audio como opciones explícitas. La ruta se habilita mediante
`AYTHER_ENABLE_TESSERA_NATIVE=ON`; la CI previa con Engine rc.19 continúa
compilando con esa opción desactivada.

El test headless FX-03 verificó pack activo/inactivo/reactivado y las tres
confirmaciones. El test GPU leyó la imagen de 960×720: 1.224 píxeles magenta
con pack, cero en los frames originales y el mismo hash al reactivarlo. En
90 frames regulados, la RTX 3060 produjo 89 intervalos de entrega, p50=16,655
ms, p95=17,759 ms, p99=17,956 ms y cero errores de validación Vulkan. El
contador de demanda SDL observó cero faltantes; se declara aproximado, no
equivalente a una medición física de cortes audibles.

La ROM FX-02 generada localmente conservó SHA-256
`5296c4fa331fd8cb47663fc7ce3b0abef03364221b309a65f2966c6a8642225c`.
Su test de 180 frames confirmó PCM no silencioso en el dispositivo. Una prueba
manual de 300 frames registró `source_peak=1022` y 478.628 muestras activas.
CTest local: 4/4 de `runtime_options`, Runtime headless, FX-02 audio y FX-03
GPU/audio. El build local contra Engine rc.19 cacheado, con la ruta nativa
desactivada, pasó `runtime_options` 1/1. Los negativos de máscara de entrada
fuera de dominio, controles fuera de orden y pack ausente rechazan sin avanzar
frame; faltan pendientes y EOF en Runtime.

Tras la decisión de distribución del usuario, ROM y packs de Engine/Lab y ROM
FX-03 se generan bajo `build/` y quedan fuera de las ramas publicables. Se
repitieron Tessera 16/16, Engine 5/5 y Runtime 4/4 con esas rutas. Las ramas
anteriores se conservaron localmente como respaldo; los commits publicables
`1f9389d` y `91b85da` no contienen esos objetos.

## Rendimiento y suites

`tests/performance/benchmark.cpp` genera una ROM/configuración/traza común y
ejecuta tres repeticiones de Tessera autónoma sin y con observación, Engine
sin ventana y Runtime regulado con dispositivos. Cada repetición contiene 180
frames y descarta 30 de calentamiento. Percentiles, memoria máxima, hashes de
binarios, hardware y límites están en `docs/performance-baseline.md`. La
comparación progresiva posterior separó asignaciones, copias, eventos,
respaldos, ejecución GPU, espera de imagen y entrega al presentador. No se
fijó un umbral de FPS.

Tessera pasó 16/16 CTest breves, incluido consumidor instalado. Se repitió el
replay largo con la ROM generada fuera de Git: 15 procesos, 18.000 frames
completos, 4.500 continuados y cero divergencias (`summary.json` local de
`t021-44556-404038921`; 646,52 s).
Engine compiló e instaló la conversión RGBA→BGRA para Vulkan y pasó 5/5
pruebas nativas afectadas. `integration.robustness` y los diagnósticos ASan
quedaron consolidados en T033 (6/6 local y 4/4 ASan); T034 requiere
consolidación final.

## Entrega y aceptación — historial de candidatos

El workflow Engine fija Tessera `5704605`; Runtime aún bloquea Engine
`v0.1.0-rc.19`. Se verificaron los ocho contextos protegidos de Runtime en
GitHub; los tres PR borradores tienen sus checks actuales aprobados:
Tessera `5704605`, Engine `3deb4c2` (incluido su gate C++ combinado) y
Runtime `a1fc482`; la revisión actual de Engine `6bbe484` se está
revalidando en CI. T035 exige
publicar la cadena real de paquetes con hashes/procedencia y repetir los
checks sin omisiones, incluido un entorno GPU/audio declarado. T036 y la
aceptación final permanecen pendientes.

**Recorrido sobre paquetes extraídos:** Tessera `d074cb4` y Engine `3deb4c2`
produjeron ZIP reproducibles sin ROM ni pack; Engine activó la dependencia
nativa. El consumidor externo compiló y ejecutó, y Runtime `a1fc482` pasó
230/230 CTest usando ambos prefijos extraídos. FX-03 produjo 179 intervalos
de entrega en 180 frames, p95=18,105 ms, cero errores Vulkan y cero faltantes
SDL; FX-02 activo produjo 287.128 muestras no silenciosas, p95 de entrega
18,002 ms y cero faltantes SDL. Los hashes de paquetes y el alcance exacto de
las mediciones figuran en `docs/release-checklist.md` y
`docs/performance-baseline.md`. Sigue faltando la presentación efectiva y la
observación acústica física, además de publicación/lock/CI del candidato.
