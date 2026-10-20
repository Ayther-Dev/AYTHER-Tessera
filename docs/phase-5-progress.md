# Fase 5 — progreso local

Fecha: 2026-10-20. Estado: **en curso**; T030–T036 siguen abiertas hasta
acreditar todos sus criterios. Sin PR, Actions ni cambio del lock publicado
de Runtime. La matriz detallada está en
[`acceptance-report.md`](../specs/001-mvp-core/acceptance-report.md).

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
FX-03 se generan bajo `build/` y quedan fuera del índice Git. Se repitieron
Tessera 16/16, Engine 5/5 y Runtime 4/4 con esas rutas. La historia anterior
de las ramas debe limpiarse antes de publicar PR.

## Rendimiento y suites

`tests/performance/benchmark.cpp` genera una ROM/configuración/traza común y
ejecuta tres repeticiones de Tessera autónoma sin y con observación, Engine
sin ventana y Runtime regulado con dispositivos. Cada repetición contiene 180
frames y descarta 30 de calentamiento. Percentiles, memoria máxima, hashes de
binarios, hardware y límites están en `docs/performance-baseline.md`. Faltan
asignaciones, bytes copiados, pérdidas/eventos, respaldos y GPU/espera
separados. No se fijó un umbral de FPS.

Tessera pasó 16/16 CTest breves, incluido consumidor instalado. Se repitió el
replay largo con la ROM generada fuera de Git: 15 procesos, 18.000 frames
completos, 4.500 continuados y cero divergencias (`summary.json` local de
`t021-44556-404038921`; 646,52 s).
Engine compiló e instaló la conversión RGBA→BGRA para Vulkan y pasó 5/5
pruebas nativas afectadas. `integration.robustness` y los diagnósticos ASan
previos siguen documentados, pero T033/T034 requieren consolidación final.

## Entrega y aceptación

El workflow Engine aún fija Tessera `dfef722`; Runtime aún bloquea Engine
`v0.1.0-rc.19`. Se verificaron los ocho contextos protegidos de Runtime en
GitHub, pero ninguna rama de trabajo tiene una ejecución remota. T035 exige
publicar la cadena real de paquetes con hashes/procedencia y repetir los
checks sin omisiones, incluido un entorno GPU/audio declarado. T036 y la
aceptación final permanecen pendientes.
