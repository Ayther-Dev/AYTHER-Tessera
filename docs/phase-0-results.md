# Resultados de la fase 0

Fecha: 2026-10-19. Alcance autorizado: contratos, infraestructura y evaluación
local de candidatos. **T001–T006 completadas; Gate 0 aprobado localmente y
ClownMDEmu seleccionado para la integración posterior.
La fase 1 no ha comenzado.**

## Evidencia disponible

| Configuración y alcance ejecutado | Resultado confirmado |
| --- | --- |
| `windows-dev`, infraestructura y probes de ambos candidatos | 9/9 CTest: dependencias, C11, generación/reproducibilidad, trazas control/acción de ambos candidatos y contratos del qualifier ClownMDEmu. |
| `windows-release`, solo infraestructura | 2/2 CTest; no equivale a probar ambos candidatos en release. |
| `windows-diagnostics`, solo infraestructura | 3/3 CTest, incluida prueba negativa que exige detectar un acceso fuera del heap con AddressSanitizer. |
| `windows-diagnostics`, generador y ClownMDEmu instrumentado, ares desactivado | Corrida final 8/8 CTest sobre fuentes congeladas, incluidos contratos del qualifier. |
| `windows-engine-integration`, SDK instalado Release `/MD` | 3/3 CTest: símbolo fuera de línea e intercambio POD C/C++ en dos unidades. |
| Fixture copiado de Engine/Lab | 11/11 archivos coinciden con `SHA256SUMS`; copia original intacta. |
| Matriz congelada del runner | Código 0; seis baselines de 3.000 frames y seis restores de 500, tres modos de observación de 60 frames y contraste parcial de ambos candidatos. |
| Contratos/media del qualifier | 78 comprobaciones de contrato y 876 de media en 11 grupos aprobadas. |
| Identidad de instancia de proceso | 3/3 regresiones; PID más hora UTC de creación del proceso. |
| Invocaciones inválidas del probe privado | 4/4 rechazadas sin informe de éxito; no es validación de la CLI de producto T013. |

Los conteos corresponden a alcances diferentes; no se suman como una suite del
MVP. El ABI C 1.0 de Tessera está concretado documentalmente en T004;
su implementación y validación binaria T007 no se han ejecutado.
Los artefactos de bootstrap están en
[phase0-results.json](../build/evidence/phase0-results.json); el inventario,
en [environment.json](../build/evidence/environment.json). Los archivos bajo
`build/` son evidencia local regenerable, fuera de los entregables versionados.

El entorno usa clang-cl 22.1.6, MSVC 14.51.36231, C++20/C11, `/MD` y Ninja.
ASAN conserva comprobaciones de direcciones; `_DISABLE_STL_ANNOTATION` desactiva
las anotaciones de contenedores STL para no mezclarlas con bibliotecas MSVC
instaladas. Se instrumentan el qualifier/probe C++ y los cinco targets C
utilizados de ClownMDEmu. **ares no tiene cobertura ASAN en esta suite.**

## Fixtures y oráculos independientes

La [demo propia](../tests/fixtures/backend-probe/README.md), fuentes y ROM MIT,
ocupa 32 KiB; SHA-256
`5296c4fa331fd8cb47663fc7ce3b0abef03364221b309a65f2966c6a8642225c`.
El generador fija trazas y expectativas antes de contrastar candidatos; dos
generaciones verifican identidad de archivos. Control neutral y acción B
contrastan RAM/VRAM/CRAM, píxel central, periodo/atenuación del PSG y ventanas
de actividad/silencio del audio. El silencio esperado cuenta como audio válido.

Ambos probes iniciales de 60 frames contrastaron RAM/VRAM/CRAM, color central
y actividad/silencio del stream PSG, con hashes parciales reproducibles entre
procesos. ClownMDEmu comprueba además el periodo y atenuación del chip;
no se atribuye esa inspección interna al probe de ares. Los hashes parciales
de memoria no representan toda la máquina; el qualifier ClownMDEmu añade
estado completo explícito y PCM canónico. Se compara cada backend consigo
mismo; no se exige igualdad de PCM entre emuladores.

El fixture original copiado añade regresión de replay/restauración, sin un
oráculo independiente de actividad sonora. Los permisos de publicación de su
ROM/pack siguen pendientes según [los avisos](../legal/THIRD_PARTY_NOTICES.md).
La semilla propia no cierra FX-02 por API autónoma y Engine; FX-01 y FX-03
tampoco están cerrados.

## Codec y media del qualifier ClownMDEmu

El backup upstream omite estado de mando y CDD: guardar countdown 9.876,
cambiarlo a 1.234 y cargar conserva 1.234. La regresión permanece; no se usa
ese backup como formato de persistencia ni hash de estado.
El [codec explícito](../tests/backend_probe/clown_state_codec.cpp) y su
[auditoría](../tools/generate-clown-codec.audit.json) cubren 12 raíces y
281 hojas, con 78 SHA-256 de fuentes fijadas. Esquema:
`3fffabc5b4e97e04506138221ce7f03f7c6576e6bafaea6ca18e9f0d35549cfc`.
Su payload tiene 1.131.929 bytes y la envoltura 188; cada campo se codifica
explícitamente, sin punteros, padding, pilas ni bytes de objetos.

`TSSTATE1` usa TLV ordenado little-endian e incluye identidades de contenido,
build/configuración, frame/tick, entrada latcheada y bloques de máquina/media.
Excluye sesión/época, observación, métricas, HD y transacciones pendientes.
`TSCHK001` añade identidad y cursor de replay e identidad inicial. La carga
valida esquema, checksums, límites, campos y perfil en una instancia privada
antes del intercambio; un fallo conserva sesión y pendiente. Exportar no la
cancela; restore exitoso devuelve la cancelación con identidad/causa fuera de
la cola y continúa en K+1 sin neutralizar controles activos.

El vídeo original es RGBA8 por filas sin padding, alfa 255, bajo `TSVIDEO1`.
Tras restore el frame actual vuelve a estar disponible al completar el
siguiente frame; las capturas históricas retenidas siguen legibles.
`TSAUDIO1` entrega S16LE estéreo L/R a 48 kHz, mezcla entera causal ZOH,
ganancias Q15 FM 32.768 y PSG 4.096, redondeo y saturación definidos.
Los filtros low-pass están activos y FM ladder desactivado. La cadencia usa
895.780 ticks/frame y reloj 53.693.175 Hz; el contador FM recibe 127.968
ciclos/frame y conserva su resto. Se guardan fases racionales, muestras
retenidas y colas de entrada pendientes, además de filtros del core; no hay
salida PCM pendiente oculta. PCM/CDDA se generan y deben ser cero en el perfil
sin CD. Las pruebas de media incluyen conteos, ganancias, saturación,
segmentación de callbacks, continuidad y rechazos sin mutación.

## Observación, transacciones y respaldo

El qualifier admite máscaras de regiones `0xF` y eventos `0x1F`, con tipos,
bits y destino validados, una pendiente única y confirmación en el frame
asignado. Entrada inválida, pausa y EOF no avanzan ni aplican pendientes.
Reset reconstruye la inicialización registrada con época nueva, frame cero,
entrada y controles neutrales. Reset/restore/cierre devuelven la cancelación
fuera de la cola; un restore fallido conserva la pendiente.

Dos lectores conservan capturas inmutables sin interferirse, incluso después
de reset/restore/cierre. Hay ocho capturas de hasta 8 MiB, cola de 4.096
eventos, cursores independientes y pérdidas detectables fuera de cola.
El bit de región temporal `0x10` se rechaza: **no se declara cobertura temporal
completa**. Ante esa limitación se entrega el frame original íntegro y una
causa, sin aplicar parcialmente un reemplazo. Se contrasta observación
desactivada, completa y saturada conservando hashes originales. Esto no cierra
reconstrucción temporal T025, sustitución real FX-03 ni Engine T029.

## Matriz final y selección local

[check-backends.ps1](../tools/check-backends.ps1) completó seis baselines
de 3.000 frames: dos procesos para control propio, acción propia y control del
fixture original. Los tres hashes coincidieron por frame; la demo propia
cumplió los oráculos independientes y la acción tuvo el efecto previsto.
Pasaron seis continuaciones en procesos nuevos desde K=500/1.500/2.500,
500 frames K+1..K+500 para ambos contenidos, y tres modos de observación
de 60 frames sin alterar hashes. También pasó el contraste parcial de ambos
candidatos. El total del qualifier es 18.000 frames de baseline, 3.000
restaurados y 180 de observación; no se suman como un único replay de producto.

El [log final](../build/evidence/gate-matrix-final.log) y
[backend-gate.json](../build/evidence/backend-gate.json), esquema 2, registran
`gate0_pass: true`, `selected_backend: ClownMDEmu`, diez pruebas obligatorias
verdaderas, seis continuaciones con `three_hashes_match: true` y ningún error.
La corrida duró 562,313 segundos y produjo 23 informes: ocho probes parciales
y quince ejecuciones de calificación. El build está identificado como
`0cba7e755e07166e748e7c3cc33100e25be219fcca86d5ac2553c6802c1c47bd`.
Con `pwsh -NoProfile -File ./tools/check-backends.ps1`, el código 0 requiere
todas las pruebas; 1 indica fallo de validación. La ejecución final terminó
con código 0. La comprobación de procesos usa PID y hora de creación UTC para
distinguir instancias aunque Windows recicle un PID; sus tres regresiones
pasaron. No se hallaron divergencias del backend en la matriz final.
El [resumen de invocaciones inválidas](../build/evidence/backends/gate0-20261019T195047554Z-7b7af288a5414b92a44f30fffc064acb/cli-negatives/summary.json)
registra cuatro rechazos correctos en el alcance del probe privado.

ares mantiene los impedimentos auditados: `serialize(false)` persiste pilas;
`serialize(true)` sincroniza sin neutralidad temporal probada, y quedan fuera
latches/select del mando y DSP/filtros/remuestreo/muestras pendientes de Stream.
No se han usado esos serializers para declarar continuidad; ares queda sin
seleccionar para esta ruta.

Los tiempos locales separan emulación/PCM y codificación/hashes por frame;
sin el protocolo de calentamiento, repeticiones y comparación Engine/Runtime
no cierran el benchmark T032 ni fijan un umbral arbitrario de FPS.

## Engine, licencia y límites de fase

El SDK probado es `install/windows-release-engine-vpx`: SDK 0.1.0, Core ABI 8,
Release y CRT `/MD`. El smoke enlaza símbolos reales e intercambia POD C/C++;
es un ejecutable separado que no incorpora ClownMDEmu ni conduce una sesión
Tessera. El prefijo `install/windows-native`, Core ABI 7, es solo inventario
previo y no se mezcla con el SDK comprobado.

El binario local ClownMDEmu combina core/CPU AGPL y código propio MIT con sus
headers auxiliares; se trata bajo AGPLv3 conservando avisos y licencias.
No se publica ni distribuye ese binario ni se ofrece un servicio remoto.
El alcance está registrado en [los avisos](../legal/THIRD_PARTY_NOTICES.md);
la distribución futura con Engine MPL-2.0 exige cerrar las obligaciones de la
obra concreta en T034. Una API C no elimina obligaciones AGPL.

No se implementó Engine→Tessera ni Runtime→Engine→Tessera, no se modificaron
repositorios consumidores y no se atribuye presentación/audio/rendimiento real
a una enumeración GPU. CI está preparada con caché, límites y concurrencia,
pero **no se ejecutó GitHub Actions ni se creó push o PR**. El resultado local
no declara terminado el MVP ni inicia la fase 1.
