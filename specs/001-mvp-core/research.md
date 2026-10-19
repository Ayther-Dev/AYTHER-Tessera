# Investigación y decisiones técnicas del MVP

**Fecha:** 2026-10-19. **Estado:** decisiones aprobadas; Gates 0–3 cerrados localmente con ClownMDEmu, Engine nativo y Mega Drive de producto. Presentación y Runtime pendientes.
Este registro explica las decisiones iniciales; la evidencia de backend integrado está en [fase 3](../../docs/phase-3-results.md).
Normas: [constitución](../../docs/constitution.md) y [especificación del MVP](spec.md).

## R-01 — Integración nativa con AYTHER Engine

**Decisión:** Engine consume directamente una API C versionada de Tessera, sin wrapper libretro ni puente AYTHER legacy.
Las dependencias quedan `Runtime -> Engine -> Tessera -> backend emulador`; los resultados fluyen al consumidor.
El módulo que consume Tessera pertenece a Engine; Tessera no depende de Engine, Runtime, sus clases ni sus tipos internos.
Engine conserva la propiedad de paquetes `.ay`, selección de assets HD y presentación de producto.
Tessera entrega emulación, salidas originales, observación y primitivas de presentación que preservan el estado emulado.
La demo de sustitución del núcleo ejercita esas primitivas; no incorpora un segundo motor de paquetes `.ay`.
**Motivo:** integrar con el destinatario real sin invertir dependencias ni duplicar políticas de recursos y autoría.
**Alternativas:** exportar libretro y emular `ayther_get_interface`, o colocar Engine debajo de Tessera; descartadas para esta ruta.
La ruta libretro de Engine puede coexistir para otros proveedores; no es una dependencia ni un entregable de Tessera.
**Evidencia:** Engine recibe hoy `core_path` y `rom_path` en [Config](C:/ayther/AYTHER-Engine/include/ayther/ayther_session.h:795),
los pasa a [RetroRunner](C:/ayther/AYTHER-Engine/src/ayther_session.cpp:234) y negocia una [extensión propia](C:/ayther/AYTHER-Engine/src/retro_runner.cpp:228).
**Verificar en campo:** un consumidor Engine mínimo debe cargar Tessera y ejecutar el contrato nativo sin traducción a esas ABI.

## R-02 — Lenguaje, compilación y frontera pública

**Decisión:** C++20 y biblioteca estándar, Windows x64, familia ABI MSVC v145 con herramientas MSVC >= 14.51; CMake y Ninja.
La versión exacta del compilador y de las herramientas se registra con el artefacto y en la configuración reproducible de CI.
El backend elegido se enlaza estáticamente dentro de Tessera; Engine accede a la biblioteca nativa mediante su API C.
La API usa handles opacos, enteros de tamaño definido, versión, tamaño de estructura, capacidades y errores explícitos.
Buffers, longitudes, duración de vistas y liberación se documentan; no cruzan STL, excepciones ni objetos C++ internos.
Cada módulo libera sus propias asignaciones; el ABI C no presupone compatibilidad entre heaps o representaciones C++.
**Motivo:** compartir ecosistema con Engine/Runtime y reducir el stack manteniendo una frontera verificable.
**Alternativas:** una interfaz pública C++ ligada al ABI del consumidor, introducir Rust o incorporar un toolkit gráfico.
**Evidencia:** [frontera core/Engine existente](C:/ayther/AYTHER-Engine/docs/adr/0001-core-and-engine-component-boundary.md)
y [reglas locales del ecosistema](C:/ayther/AGENTS.md); esta elección no constituye una prueba de compatibilidad binaria.
**Verificar en campo:** build limpio, consumo fuera del árbol, símbolos/layouts y ciclo completo de propiedad de buffers.

## R-03 — Dependencias mínimas justificadas

**Decisión:** incorporar únicamente las siguientes dependencias auxiliares aprobadas, con versión y licencia registradas.

| Dependencia | Versión | Licencia | Uso y justificación |
| --- | --- | --- | --- |
| [nlohmann/json](https://github.com/nlohmann/json/releases/tag/v3.12.0) | 3.12.0 | MIT | Metadatos JSON, manifiestos del corpus y trazas; evita escribir un parser propio. |
| [PicoSHA2](https://github.com/okdshin/PicoSHA2/tree/v1.0.1) | v1.0.1 | MIT | SHA-256 de contenido, backend, fixtures y representaciones canónicas; no introduce una suite criptográfica completa. |
| [doctest](https://github.com/doctest/doctest/releases/tag/v2.5.3) | 2.5.3 | MIT | Unidad y contratos en desarrollo/CI; no es dependencia de ejecución distribuida. |

**Motivo:** cubrir formatos, identidad y pruebas con bibliotecas pequeñas y responsabilidades acotadas.
**Alternativas:** parser/hash/framework propios, frameworks generales de aplicación o dependencias sin fijar; descartadas.
El inventario debe fijar también la identidad de las fuentes y preservar licencias; la versión sola no cierra la procedencia.
**Verificar en campo:** revisión de los archivos realmente incorporados, checksums, límites JSON y build sin descargas implícitas.

## R-04 — Detección, registro, fábrica y estrategia por sistema

**Decisión:** un probe de contenido produce candidatos con identificador de sistema, evidencia y resultado explícito.
Un registro declara sistemas y capacidades; una fábrica selecciona la estrategia/adaptador correspondiente.
Firmas, estructura de encabezados y límites del contenido sustentan la detección; la extensión no determina la consola.
Contenido ambiguo, contradictorio o no soportado se rechaza o requiere una selección explícita validada por el backend.
El MVP registra Mega Drive; otros sistemas quedan fuera aunque compartan el mecanismo de selección.
Después de cargar, el backend confirma el hardware activo y publica geometría/modo reales por generación.
**Motivo:** separar formato del archivo, sistema y proveedor de emulación sin condicionales repartidos por el consumidor.
**Alternativas:** elegir por extensión o hacer que la UI conozca estructuras del emulador; descartadas.
**Evidencia:** Engine ya consulta un [descriptor de sistema](C:/ayther/AYTHER-Engine/include/ayther/libretro_host/ayther_api.h:704),
pero su clasificación previa de [medio CD es por extensión](C:/ayther/AYTHER-Engine/src/retro_runner.cpp:71); no es un detector universal.
**Verificar en campo:** entradas válidas, truncadas, renombradas y ambiguas, y coherencia entre detección y hardware cargado.

## R-05 — Backend real condicionado a Gate 0

**Decisión comprobada en T006:** seleccionar ClownMDEmu `88ef45a6585556e2247dd26b6c937d6b9fb4a12d` para la integración posterior. El qualifier local cerró estado explícito completo, PCM, replay/restauración entre procesos, controles y respaldo original íntegro. Ares v148 fue la primera hipótesis; queda descartado para este MVP por su persistencia incompleta y el coste de cerrar latches y DSP. La API de producto y la integración real siguen pendientes.
Referencia de ares: [v148](https://github.com/ares-emulator/ares/releases/tag/v148),
commit [0aafd85789215e84e1e43415c07d4c88461b7899](https://github.com/ares-emulator/ares/commit/0aafd85789215e84e1e43415c07d4c88461b7899).
Su [licencia](https://raw.githubusercontent.com/ares-emulator/ares/v148/LICENSE) declara ISC para ares y avisos de terceros.
La [biblioteca estática](https://raw.githubusercontent.com/ares-emulator/ares/v148/ares/CMakeLists.txt) usa libco, nall y sljit;
el [módulo MD](https://raw.githubusercontent.com/ares-emulator/ares/v148/ares/md/CMakeLists.txt) también enlaza qon.
El [build principal](https://raw.githubusercontent.com/ares-emulator/ares/v148/CMakeLists.txt) agrega componentes de interfaz: aislarlo sin UI debe probarse.
[ClownMDEmu](https://github.com/Clownacy/clownmdemu-core) es C con API de frame, callbacks y respaldo de estado, bajo AGPLv3+.
Sus CPU [clown68000](https://github.com/Clownacy/clown68000) y [clownz80](https://github.com/Clownacy/clownz80) también declaran AGPL.
AGPL permite uso comercial, pero no es permisiva; un enlace estático o una API C no eliminan sus obligaciones.
Fijar commit y submódulos de Clown forma parte de la evaluación; no se atribuye a `master` una revisión inmutable.
**Motivo:** Clown permitió demostrar un codec por campos sin punteros ni pilas y continuidad de todas las salidas del perfil probado. El binario combinado local se trata bajo AGPLv3; las obligaciones de distribución y de una futura combinación con Engine se conservan en el inventario y los avisos.
**Alternativas:** elegir por README, reutilizar código restringido de Genesis Plus GX o prometer un emulador propio completo; descartadas.
**Verificar antes de integrar:** licencias y fuentes transitivas, build headless, frame, entradas, audio activo, capturas temporales,
persistencia entre procesos y coste de integración. La [evaluación de Gate 0](../../docs/backend-selection.md) registra la decisión y sus límites; no acredita integración de producto ni FX-03.

## R-06 — Observación y presentación sin modificar la máquina

**Decisión:** publicar capturas inmutables con sesión, época y generación; registrar salidas originales separadas de la presentación.
Una transacción de presentación válida se aplica completa al siguiente frame; una inválida se rechaza completa.
No asumir que VRAM/CRAM/SAT al final del frame bastan: registrar los datos temporales realmente necesarios o usar respaldo íntegro.
**Motivo:** evitar una sustitución plausible pero incorrecta ante escrituras raster, cachés de sprites o pérdida de historial.
**Alternativas:** reconstrucción universal desde memoria final, punteros mutables compartidos o degradación silenciosa; descartadas.
**Evidencia:** [capturas actuales de Engine](C:/ayther/AYTHER-Engine/src/session/emulation_observer.cpp:131)
y [capacidades temporales](C:/ayther/AYTHER-Engine/include/ayther/libretro_host/ayther_api.h:314) muestran necesidades a satisfacer, no una ABI a copiar.
**Verificar en campo:** dos lectores, observación activada/desactivada, saturación, cambio de época, sustitución y respaldo observable.

## R-07 — Persistencia y hashes canónicos

**Decisión:** formatos propios versionados con tamaños/límites explícitos, guardado atómico y validación antes de modificar la sesión.
No persistir structs nativos, punteros, padding ni pilas de ejecución; la compatibilidad inicial puede rechazar versiones ajenas.
**Motivo:** cumplir restauración entre procesos y comparar estado semántico, no direcciones o residuos de memoria.
**Alternativas:** volcar el estado nativo completo del backend o asumir que sus savestates satisfacen el contrato; descartadas.
**Evidencia:** [Thread de ares v148](https://raw.githubusercontent.com/ares-emulator/ares/v148/ares/ares/scheduler/thread.cpp)
copia la pila en serialización no sincronizada; no es una ruta de persistencia admisible para Tessera.
**Verificar en campo:** modo sincronizado, ausencia de direcciones persistidas y continuidad de filtros, remuestreo y colas de audio.
Comparar los tres checkpoints y continuaciones exigidos por la spec; la existencia de un serializador no demuestra ese resultado.

## R-08 — Corpus, aceptación y mediciones

**Decisión:** conservar la [demo incorporada](../../tests/fixtures/ayther-engine-synthetic/README.md)
y su [procedencia](../../tests/fixtures/ayther-engine-synthetic/PROVENANCE.md); ROM sintética y backend sintético son piezas distintas.
La ROM de 32 KiB no configura FM/PSG; el pack existente contiene texto de prueba y no acredita audio ni sustitución gráfica.
FX-01 tiene recorrido autónomo verificado en fase 1; su consumo desde Engine
sigue T018. FX-02 completo (audio real activo) y FX-03 (recurso gráfico real)
permanecen obligatorios y pendientes. La evidencia se detalla en
[resultados de fase 1](../../docs/phase-1-results.md).
Engine conserva `.ay`/assets HD; la biblioteca Tessera no interpreta ese formato. Los tests integrados consumen FX-03 mediante Engine y Runtime.
**Motivo:** evitar aceptación vacía por silencio, imagen constante, entrada ignorada o un paquete sin reemplazo.
**Alternativas:** dar los fixtures actuales por suficientes o heredar garantías de Engine/Lab; descartadas.
**Verificar en campo:** unidad, contratos, replay, regresiones por bug, corrupción, Engine→Tessera y el recorrido obligatorio Runtime→Engine→Tessera.
Registrar p50/p95/p99, memoria máxima, asignaciones, bytes copiados por frame, pérdidas, respaldos, regularidad de presentación y cortes de audio, con calentamiento, repeticiones y configuración identificados. Comparar integración sobre el mismo backend; separar cambios de emulador.
No prometer FPS, latencia, catálogo completo ni equivalencia entre backends/plataformas sin mediciones que lo sustenten.
Los puntos de verificación anteriores son condiciones técnicas de ejecución del plan aprobado, no nuevas preguntas de alcance.
