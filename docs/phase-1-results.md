# Resultados de fase 1 — Núcleo sintético, API y CLI

**Fecha:** 2026-10-19. **Estado:** fase 1 cerrada localmente; T007–T014 completadas.
Alcance T007–T014; la fase 2 no se ha iniciado. Se conservan las 36 tareas y
sus dependencias. Gate 0 y las copias originales permanecen intactos.

## Implementación entregada

| Tarea | Resultado y comprobación |
| --- | --- |
| T007 | Contratos de 19 layouts, 26 firmas, negociación, extensibilidad, errores y buffers sin publicación parcial; wrappers C11/C++20. |
| T008 | API C 1.0, handles opacos con tipo/generación y barrera de excepciones. Núcleo estático independiente de Engine, Runtime, Lab y GPU. |
| T009 | FX-01 propio MIT, perfiles TEST mínimo/completo, memoria inicial conocida, entradas efectivas, estado/píxeles/PCM y fallo de reset inducible. Modelo y oráculos fijados antes del backend. |
| T010 | Sesión propietaria, pausa/paso/reset/cierre, época/reloj enteros, entradas completas y rechazos sin avance; reset fallido conserva originales, replay y pendiente. |
| T011 | Detector por bytes, evidencias RAW/COPIER512, ambigüedad y override; registro con factories y estrategia IEmulatedSystem. Mega Drive reconocida sin factory de producto en esta fase. |
| T012 | Capturas inmutables, dos referencias/hilos, vigencia tras reset/cierre, presupuesto de ocho objetos, eventos acotados y pérdidas fuera de cola. Pendiente única y confirmación/cancelación con identidad explícita. |
| T013 | CLI por encabezado público, ciclo de vida, fixtures reproducibles, trazas JSON/binarias estrictas, informes en español y tres hashes canónicos. Sin construcción JSON ni escritura de logs por frame. |
| T014 | Replay en procesos nuevos contra modelo independiente cerrado, conteo exacto, EOF y metadatos; igualdad con observación desactivada y dos lectores. Costes básicos por etapa fuera del estado lógico. |

La enumeración y la creación comparten el registro; la sesión crea y reinicia
su estrategia por factory. No se inspecciona otra vez la ROM en cada frame.
El mínimo anuncia `0x100F`; el completo `0x1DFF`. Los dos omiten checkpoint.
El ciclo de originales produce 16×16 RGBA8 y 800 pares S16LE a 48 kHz por frame,
con 1000 ticks a 60000/1 Hz, según el modelo explícito de FX-01.

## Validación local

| Configuración | Resultado |
| --- | --- |
| windows-dev | 9/9 pruebas aprobadas en la repetición final. |
| windows-diagnostics | 10/10 pruebas aprobadas, incluido el detector ASAN negativo esperado. |
| windows-release | 9/9 pruebas aprobadas en la ejecución final. |

Cada configuración ejecuta esta matriz completa:

| Perfil | Traza | Observación | Procesos nuevos | Frames por proceso |
| --- | --- | --- | --- | --- |
| Mínimo | Neutral y B en 301…600 | Desactivada | 2 por traza | 3000 |
| Completo | Neutral y B en 301…600 | Desactivada | 2 por traza | 3000 |
| Completo | Neutral y B en 301…600 | Dos lectores | 2 por traza | 3000 |

Son doce procesos y 36.000 frames por configuración: 108.000 frames y 324.000
comparaciones de digests canónicos en las tres configuraciones. Se verifican los tres
hashes completos de cada frame frente a fórmulas independientes, además de
compararlos entre repeticiones y modos de observación. El primer desvío se
informa con ejecución/frame. PID y creación UTC se contrastan con el proceso
real; el uso repetido de un PID no puede aparentar una ejecución nueva.
Estado inicial, identidades, traza, reloj, muestras de costes y EOF se verifican.
El cierre autónomo de FX-01 no acredita todavía su consumo desde Engine (T018).

Pruebas negativas: entradas incompletas, repetidas iguales/contradictorias,
tardías/futuras/otra época, configuración incompatible, traza inválida completa,
EOF prematuro, presupuesto de capturas y cola de 4096 eventos saturada. Los
préstamos previos siguen leyéndose tras reset fallido; los originales faltan
tras un reset exitoso hasta el siguiente frame. Dos hilos leen referencias
históricas después de reset/cierre; los lectores de CLI son secuenciales y lo
declaran. La cancelación sobrevive a la saturación sin depender del journal.

La revisión agregó regresiones para solapes en colas extensibles, stride de
arrays extendidos, adquisiciones repetidas sin metadatos ilimitados y prefijos
inválidos que ocultaban una vista COPIER512 válida. ASAN detectó además un fallo
al traducir JSON truncado durante el manejo de otra excepción. El parser ahora
rechaza sintaxis mediante resultado descartado y traduce fuera del catch;
mantiene INVALID_TRACE y cero frames. Los casos cubren también arrays truncados,
tokens sobrantes, claves duplicadas anidadas y valores float incompatibles.

## Reproducción y evidencia

```powershell
./tools/check.ps1 -Preset windows-dev -Jobs 1
./tools/check.ps1 -Preset windows-diagnostics -Jobs 1
./tools/check.ps1 -Preset windows-release -Jobs 1
```

Los logs finales son `build/evidence/phase1-windows-*-final.log`. Informes
por proceso y summary.json: `build/evidence/phase1-<config>-<ASAN>`. Configuración
y estado incluyen el build_id: no se comparan hashes de estado entre builds
distintos. El [manifiesto de cierre](../build/evidence/phase1-gate.json) identifica
fuentes, ejecutables e informes de esta ejecución local.
ASAN instrumenta núcleo, CLI, contratos y workers; su prueba negativa demuestra
que detecta el acceso inválido esperado. Las anotaciones de contenedores STL
siguen desactivadas por la disponibilidad del runtime local.

Se usó una compilación con un trabajo y, como máximo, dos frentes de edición;
las pruebas/mediciones corren después de compilar. Una consulta nativa confirmó
15,7 GiB físicos y unos 2,7 GiB disponibles. Se reutilizaron cachés verificadas,
sin descargas nuevas, pushes, PR ni ejecuciones de GitHub Actions. El job de CI
existente registra las suites nuevas de CTest, sin ampliar la matriz.

## Costes y pendientes

La CLI conserva muestras de reloj monotónico en microsegundos y p50/p95/p99
para avance público, exportación/encoding/hash y lectura de observación. Setup,
preflight, metadatos y formato/escritura final quedan fuera de esos tramos.
Son costes sintéticos locales, no presupuestos de rendimiento del MVP ni
mediciones de CPU/GPU, entrega/presentación o dispositivo de audio de T032.

Muestra Release, primera repetición de la traza acción, 3000 muestras por fila,
sin descartar calentamiento; preflight excluido. Tiempos en microsegundos:

| Perfil / observación | Avance p50 / p95 / p99 | Hashes p50 | Lectura observación p50 |
| --- | --- | --- | --- |
| Mínimo / desactivada | 3,3 / 4,3 / 6,1 | 41,3 | 0,0 |
| Completo / desactivada | 3,2 / 4,4 / 7,8 | 36,1 | 0,0 |
| Completo / dos lectores | 8,0 / 11,2 / 29,1 | 36,5 | 20,7 |

El cero corresponde al camino vacío y resolución del reloj. El informe conserva
las muestras y build_id completo; estos valores no predicen el rendimiento de
Mega Drive, Engine o Runtime.

- **Engine:** paquete/consumo, ownership y recorrido nativo T015–T018 pendientes.
  Los wrappers C/C++ actuales no prueban una frontera de allocators DLL.
- **Mega Drive de producto:** T019; el qualifier privado de Gate 0 sigue separado.
- **FX-02/audio real:** T020 conserva pendientes estímulos/oráculos completos y
  validación API/Engine. La ROM copiada original no inicializa FM/PSG; la semilla
  sonora de Gate 0 no cierra por sí sola ese fixture.
- **FX-03/sustitución gráfica:** T026 exige `.ay` y recurso gráfico reales,
  precedencia, solapamientos y píxeles esperados. El pack de texto no lo satisface.
- **Persistencia:** codec/checkpoint/exportación/restauración de producto T022–T024.
  **Runtime y aceptación del MVP:** fases posteriores; no se acreditan aquí.

Referencias: [CLI](cli.md), [codec/oráculos](../specs/001-mvp-core/contracts/synthetic-codec.md),
[contrato](../specs/001-mvp-core/contracts/native-api.md) y [tareas](../specs/001-mvp-core/tasks.md).
