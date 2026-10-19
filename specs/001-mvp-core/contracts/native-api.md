# Contrato de integración nativa — Engine → Tessera

**Estado:** ABI 1.0 y cruce nativo con Engine validados localmente hasta T024; Mega Drive de producto, codec explícito y checkpoints entre procesos probados. La frontera de allocators DLL y la distribución final siguen pendientes.
**Referencias:** [spec](../spec.md), [modelo de datos](../data-model.md) y [constitución](../../../docs/constitution.md).
Este documento fija operaciones, garantías, símbolos, valores y representaciones de ABI 1.0. El encabezado implementado está en [tessera.h](../../../include/tessera/tessera.h); los bloques C siguientes conservan la especificación documental.

## Dirección y alcance

- Tessera exporta una API C consumida por Engine; el emulador queda detrás de `IEmulatedSystem`.
- `SystemDetector` y `SystemRegistry`/factory resuelven el sistema y backend; Mega Drive es el único sistema de producto del MVP.
- El perfil sintético propio implementa el contrato para pruebas y no equivale a la ROM sintética ejecutada por Mega Drive.
- Engine conserva propiedad de `.ay`, validación, composición HD y políticas de presentación; Tessera valida y aplica sus controles nativos seguros.
- RF-05/RF-08/CA-05 se aceptan mediante pruebas Engine+Tessera; delegar funciones no elimina sus condiciones.
- No se usa Libretro ni un adaptador de su descriptor de observación heredado en la ruta nativa objetivo.

## Reglas de ABI y resultados

- El encabezado público es C, con handles opacos y datos de representación C explícita; no exporta STL, clases C++ ni excepciones.
- Negociar versión mayor/menor y capacidades antes de crear sesión. Un desacuerdo incompatible se rechaza antes de usar datos.
- Toda estructura extensible declara versión/tamaño; toda región declara tamaño, formato, alineación y validez.
- Los tipos, orden de bytes, calling convention, alineación, extensibilidad y resultados de ABI 1.0 se fijan en el anexo concreto siguiente; están comprobados entre unidades C/C++ y procesos CLI. El consumo binario desde Engine queda pendiente de T016/T018.
- El consumidor no libera memoria de Tessera con su propio allocator; el contrato proporciona las operaciones de liberación correspondientes.
- Una operación fallida devuelve código estable, contexto suficiente y ownership definido; un buffer insuficiente informa el tamaño requerido sin escribir una carga parcial.
- Código, comentarios y logs propios nuevos en inglés; documentación y mensajes al usuario en español; eventos estructurados con códigos estables. Ninguna lógica depende del texto humano.
- Las fuentes externas conservan idioma, avisos y procedencia documentada; sus copias de referencia permanecen intactas.
- Los resultados distinguen incompatibilidad, capacidad ausente, contenido desconocido/ambiguo/inválido, handle inválido, época obsoleta, presupuesto agotado, `CONTROL_PENDING` y `END_OF_TRACE`, conforme a las reglas siguientes.

## Operaciones requeridas

Los nombres de esta tabla describen operaciones; el anexo asigna sus símbolos exportados de ABI 1.0.

| Operación | Entrada / precondición | Resultado y efecto permitido |
| --- | --- | --- |
| Negociar API | Versiones y tamaños del consumidor. | Versión compatible, capacidades y límites; no crea máquina. |
| Detectar contenido | Bytes o acceso acotado al archivo, override opcional. | Candidatos/evidencias o rechazo definido; no altera sesiones existentes. |
| Enumerar registros | API negociada. | Identidades/versiones/capacidades implementadas y política de elección estable. |
| Crear/preparar sesión | Contenido identificado, sistema/backend y configuración compatibles. | Handle propietario; instancia candidata validada o error sin reemplazar la anterior. |
| Consultar sesión | Handle válido. | Estado, identidades y capacidades copiadas o con vigencia explícita. |
| Pausar | Sesión activa, petición procesada en punto seguro. | Se detiene en un límite de frame; no añade un frame de avance como efecto de pausar. |
| Avanzar un frame | Sesión preparada/pausada; estado completo de entradas para el próximo frame y época actuales. | Valida antes de avanzar; éxito completa exactamente un frame, produce identidad lógica y salidas originales y vuelve a pausa. Un rechazo no avanza ni aplica controles pendientes. |
| Ejecutar | Política de avance explícita y entradas válidas para cada próximo frame. | Ejecución por límites de frame; respeta las reglas de entradas y final de traza. |
| Reiniciar | Sesión válida y petición procesada en punto seguro. | Reconstruye el estado inicial registrado; éxito abre época nueva, deja cero frames, entradas y controles nativos neutros y pausa; fallo conserva sesión y pendiente. |
| Enviar transacción de controles | Sesión/época/frame objetivo y controles nativos soportados; grupo completo validable. | Valida íntegramente antes de aceptar una única pendiente; otra solicitud devuelve `CONTROL_PENDING` sin reemplazo ni cambios efectivos. La aceptada se aplica completa al inicio del próximo frame ejecutado y confirma identidad, época y frame. |
| Consultar frame | Frame completado disponible. | Video/audio originales con tamaños, formatos, geometría y vigencia; ninguna composición HD. |
| Adquirir/retener/liberar captura | Frame disponible y presupuesto de retención. | Handle de datos inmutables coherentes; referencias independientes para dos lectores. |
| Leer observación | Cursor del consumidor y capacidad de salida. | Eventos ordenados, cursor y pérdidas/gap explícitos; no consume el cursor de otro lector. |
| Consultar pérdidas | Handle válido, incluso con cola saturada. | Contadores/intervalos y causa fuera de la cola saturada; no altera la máquina. |
| Exportar checkpoint | Sesión en límite seguro; formato negociado. | Máquina y contexto de replay explícitos/versionados, sin punteros ni buffers prestados, presentación HD o pendientes. Exportar no aplica ni cancela la pendiente de la sesión actual. |
| Restaurar checkpoint | Identidades compatibles y entrada validable. | Instancia candidata validada, intercambio completo y época nueva; éxito cancela la pendiente con resultado explícito; fallo conserva íntegramente sesión y pendiente anteriores. |
| Cerrar/liberar sesión | Handle propietario válido. | El éxito termina actividad y cancela la pendiente con resultado explícito; las referencias retenidas se liberan por separado. |

## Tiempo, orden y concurrencia

- El propietario serializa creación, avance, reinicio, restauración y cierre. Ninguna concurrencia adicional se presupone.
- Solo las operaciones de lectura y referencia de capturas retenidas admiten otros hilos, conforme al anexo; las restantes requieren al propietario.
- Cada avance recibe el estado completo de controles configurados para el próximo frame de la época actual, con una única asignación por puerto/control; las repeticiones se rechazan aunque coincidan sus valores.
- Conjuntos incompletos o contradictorios, entradas tardías para un frame iniciado/completado, futuras posteriores al próximo esperado o de época obsoleta se rechazan sin avanzar ni aplicar controles pendientes.
- La reproducción valida antes de usarla una traza consecutiva y ordenada de 1 a N, sin duplicados ni huecos; el rechazo no modifica la sesión. FX-01 y FX-02 verifican entradas de efecto conocido frente a su control sin entrada.
- Agotar la traza devuelve `END_OF_TRACE` y deja la sesión pausada sin otro frame ni aplicación pendiente; CA-02 exige completar los 3.000 frames solicitados.
- Reiniciar usa el mismo contenido, backend, configuración y datos iniciales registrados de memoria persistente, cuando correspondan; descarta entradas pendientes de la época anterior y devuelve el cursor de replay al inicio. Un fallo conserva la sesión anterior y su pendiente.
- Tras reiniciar hay cero frames completados; el primer avance completa el frame 1. Un checkpoint K representa el estado posterior a K y su continuación comienza en K+1.
- Una transacción aceptada permanece pendiente durante la pausa; solo se aplica al ejecutar un frame válido o se cancela por una operación definida.
- Reinicio, restauración y cierre exitosos informan identidad y causa de cancelación en su resultado, fuera de la cola saturable. Reinicio o restauración fallidos conservan la pendiente.
- Callbacks, si se necesitan, no reentran operaciones mutables ni bloquean la emulación; preferir lotes por frame.
- Reloj lógico y orden de eventos usan unidades documentadas; timings de benchmarks y orden de lectura quedan fuera del estado emulado.
- Capacidad ausente y parámetros inválidos se detectan antes de mutar; observar, retener o liberar no cambia estado/video/audio originales.

## Buffers y capturas retenidas

- Una vista prestada declara exactamente qué operación termina su vigencia; no se presume válida después del siguiente avance.
- Capturas retenidas conservan sus datos hasta la última liberación, sin sobrescribir memoria que todavía tiene lectores.
- Dos consumidores leen la misma sesión/época/generación y no se roban deltas ni referencias.
- Configurar límites de memoria/objetos y consultar su uso. La falta de presupuesto tiene resultado explícito y no espera a un consumidor lento.
- Un fallo de captura opcional no elimina las salidas originales necesarias ni autoriza declarar completa la observación parcial.
- Reinicio/restauración hacen obsoleta la captura para operaciones actuales; los bytes históricos retenidos permanecen legibles hasta liberación.
- Handles liberados o asociados a una sesión cerrada no autorizan nuevas operaciones de sesión; la captura retenida tiene liberación independiente.

## Integración de presentación con Engine

1. Engine valida contenido, manifiesto `.ay`, hashes, autores/licencias, recursos, rutas y límites exigidos por RF-08.
2. Prepara controles y recursos completos sin tocar memoria emulada; una transacción inválida se rechaza completa.
3. Envía el grupo nativo y solo activa las políticas de Engine al confirmar identidad, época y frame de su aplicación completa al comienzo del próximo frame ejecutado. Una pendiente o un rechazo de Tessera no activa media transacción en Engine.
4. Consume video/audio originales y captura de ese frame; compone en buffers propios y conserva separadas las salidas originales.
5. Usa sesión/época/generación, controles, geometría y versión del paquete para validar cachés y recursos.
6. Ante datos temporales incompletos, pérdida de historial o modo no soportado, presenta el video original completo e informa la causa.
7. Registra la presentación final por separado; sus píxeles solo difieren donde lo permite la regla del paquete.
8. Para tiles, un override de ejecución aplicable prevalece sobre el catálogo; en este gana la primera entrada `[[sub]]` aplicable según el orden declarado. Los sprites superpuestos conservan cadena SAT y prioridad respecto de planos; otros tipos admitidos identifican el contrato versionado de Engine que fija precedencia y desempate.
9. Tras reinicio exitoso, Engine conserva el paquete seleccionado y rearma su política mediante transacción de la época nueva, con confirmación antes de presentar el frame correspondiente. La neutralización de controles nativos corresponde al reinicio.

La API de Tessera no interpreta `.ay` ni compone HD. Engine no sustituye bytes de ROM, VRAM o registros para implementar presentación.

## Persistencia y recuperación

- Antes de usar datos, validar formato, identidades, versiones, tamaños, rangos e integridad con límites documentados.
- Reconstruir en candidato independiente; si el backend no permite ese procedimiento, no cumple el gate de restauración requerido.
- Solo después del éxito se reemplaza la sesión activa; se publica la época nueva y Engine invalida sus derivados.
- Persistir máquina y contexto de replay necesarios para continuar, nunca handles ABI, direcciones, padding, métricas, presentación HD ni transacciones pendientes; exportar no aplica ni cancela la pendiente actual.
- La escritura a archivo usa temporal y reemplazo atómico; el contrato de exportación a memoria no afirma por sí solo atomicidad de archivo.
- Ante interrupción del proceso, un destino previamente válido conserva el archivo anterior o contiene el nuevo completo; en el primer guardado queda ausente o contiene el nuevo completo. Nunca se acepta un parcial ni un temporal incompleto como checkpoint publicado.
- Los errores de lectura, escritura y publicación identifican código estable, fase fallida y diagnóstico del sistema operativo, distinguiendo espacio insuficiente, acceso denegado y fallo de publicación.
- Un error anterior a publicar conserva intacto el destino previo o su ausencia; un error de lectura o restauración conserva íntegramente sesión y pendiente. El guardado solo comunica éxito después de publicar el archivo completo.
- El consumidor responsable de persistir induce fallos y verifica destino, resultado y sesión en Windows x64. La garantía cubre interrupción del proceso y errores de E/S; no exige durabilidad ante pérdida de alimentación.

## Definiciones de medición

- Las métricas quedan fuera del estado y reloj lógicos; registrar configuración, calentamiento, repeticiones, punto de medición, origen y cobertura de cada serie.
- La regularidad de entrega a presentación son intervalos en milisegundos, con reloj monotónico, entre entregas consecutivas al presentador de Runtime; publicar p50/p95/p99 y máximo.
- Los timestamps de presentación efectiva, cuando estén disponibles, forman una serie separada con su origen identificado.
- Un corte de audio es un intervalo continuo en que el dispositivo solicita muestras y Runtime no las tiene disponibles a tiempo; registrar episodios, muestras faltantes y duración total y máxima. El silencio previsto por el fixture es audio válido.
- Medir por etapas con el mismo núcleo/configuración/traza y resultados equivalentes; comparar instrumentación activa/inactiva. Los presupuestos posteriores derivan de la línea base y variación medidas, sin umbrales de FPS inventados.

## Gates antes de aceptar la integración

- Gate 0: backend elegido y revisión del ABI concreto, ownership, límites, matriz de compatibilidad y obligaciones de distribución.
- FX-01: contratos sobre perfil sintético mínimo/completo, entradas conocidas, errores, dos lectores, retención y saturación.
- RF-01/06/09 y CA-07: reinicio/frame/entradas/EOF, solicitudes consecutivas, cancelación con cola saturada, exportación con pendiente, primer guardado y errores de E/S con resultados verificables.
- CA-02/03/04: cada demo ejecutable completa 3.000 frames en dos procesos independientes; checkpoints 500/1.500/2.500 y 500 frames de continuación; observación sin interferencia y hashes canónicos de estado, video original y audio original.
- FX-02/CA-02/06/10: entrada de efecto conocido y control sin ella; oráculos de estado observable, video original y actividad/transiciones sonoras derivados de fuente/generador antes de contrastar con Tessera, no exclusivamente del backend integrado. Verificarlos por API autónoma y Engine; silencio completo falla y la ROM copiada permanece intacta. Sin sustitución sonora.
- FX-03/RF-05/RF-08/CA-05: `.ay` gráfico real integrado en Engine, reglas simultáneamente aplicables y recursos superpuestos con ganador y píxeles esperados explícitos, repetibles entre procesos; conservar hashes originales y probar rechazo, invalidación y respaldo.
- CA-09/CA-10: detección, despacho y llamadas desde Engine sin ruta Libretro/descriptor heredado; completar Runtime→Engine→Tessera en el entorno declarado. Las pruebas del componente no sustituyen la integración obligatoria.
- CA-11: A/B por etapas sobre el mismo núcleo/configuración/traza, documentando p50/p95/p99, memoria máxima, asignaciones, bytes/frame, volumen/pérdidas de eventos y respaldos; Engine/Runtime añaden regularidad de entrega a presentación y cortes de audio según las definiciones anteriores. Las comparaciones con otro backend se identifican por separado.
- No hay todavía mediciones ni resultados de emulación de Tessera; los hashes de archivos solo prueban identidad de esos archivos.

## Anexo T004 — ABI C 1.0 concreta

### Representación, negociación y extensibilidad

- Plataforma inicial: Windows x64 little-endian, punteros de 8 bytes y ABI MSVC. Cada símbolo usa `extern "C"` bajo C++, `__cdecl` y sufijo `_v1`; el target estático no requiere una DLL. No cruzan excepciones ni tipos internos de Engine.
- Tipos escalares: `uint8_t/16_t/32_t/64_t`, `int32_t/64_t`. `tessera_status` es `int32_t`; handles de sesión/captura son tokens `uint64_t`, cero inválido, con tipo y generación comprobados. No son direcciones ni se persisten. No usar `bool`, `long`, `size_t`, bitfields ni enums C como almacenamiento ABI.
- La alineación es natural, de hasta 8 bytes; no usar packing. Todo agregado extensible empieza por `tessera_header_v1 { uint32_t size; uint16_t major; uint16_t minor; }`, tamaño 8/alineación 4. El llamador inicializa tamaño real, versión 1.0 y reservados a cero.
- `api_query` exige major 1 y minor mínimo 0; devuelve la versión negociada y capacidades. Un major distinto, minor requerido no disponible o prefijo insuficiente devuelve `ABI_INCOMPATIBLE`. En revisiones compatibles solo se añaden campos al final; se leen/escriben prefijos negociados y no se toca la cola ajena.
- Se comprueban `sizeof`, `alignof` y `offsetof` desde C y C++ antes de publicar binarios. Los tamaños/offsets siguientes son el contrato, todavía sin verificación compilada T007.
- `B` significa vista `{ const uint8_t *data; uint64_t bytes; }`; `W`, salida `{ uint8_t *data; uint64_t capacity; }`: ambas 16 bytes/alineación 8, offsets 0/8. `H` significa el header de 8 bytes. Las cadenas son UTF-8 con longitud explícita, sin NUL incrustado; un ID fijo declara su terminador.

| Tipo `tessera_*_v1` | Campos en orden y offsets decimales | Tamaño / alineación |
| --- | --- | --- |
| `frame_id` | session:u64@0, epoch:u64@8, frame:u64@16 | 24 / 8 |
| `context` | H@0, code:i32@8, phase:u32@12, os_domain:u32@16, os_code:u32@20, frame_id@24, transaction:u64@48, cancel_reason:u32@56, flags:u32@60 | 64 / 8 |
| `api_info` | H@0, capabilities:u64@8, major:u16@16, minor:u16@18, pointer_bytes:u32@20, max_alignment:u32@24, reserved:u32@28 | 32 / 8 |
| `limits` | H@0, content_bytes:u64@8, checkpoint_bytes:u64@16, retained_bytes:u64@24, trace_bytes:u64@32, video_width:u32@40, video_height:u32@44, pcm_frames:u32@48, event_count:u32@52, event_payload_bytes:u32@56, input_count:u32@60, control_count:u32@64, retained_count:u32@68, trace_frames:u32@72, ports:u32@76, controls_per_port:u32@80, reserved:u32@84, snapshot_bytes:u64@88 | 96 / 8 |
| `backend_info` | H@0, kind:u32@8, system:u32@12, capabilities:u64@16, id:char[64]@24, version:char[32]@88, build_id:u8[32]@120 | 152 / 8 |
| `detection` | H@0, outcome:u32@8, system:u32@12, evidence:u64@16, evidence_offset:u64@24, evidence_bytes:u64@32, content_sha256:u8[32]@40, candidate_count:u32@72, reserved:u32@76 | 80 / 8 |
| `candidate` | system:u32@0, evidence_class:u32@4, evidence:u64@8 | 16 / 8 |
| `session_create_info` | H@0, backend_id:B@8, content:B@24, config:B@40, initial_persistent_memory:B@56, system_override:u32@72, flags:u32@76 | 80 / 8 |
| `session_info` | H@0, frame_id@8, generation:u64@32, logical_tick:u64@40, tick_hz_num:u64@48, tick_hz_den:u64@56, capabilities:u64@64, state:u32@72, system:u32@76, content_sha256:u8[32]@80, pending_transaction:u64@112, next_replay_frame:u64@120, backend_build_id:u8[32]@128 | 160 / 8 |
| `input_assignment` | port:u16@0, control:u16@2, value:i32@4 | 8 / 4 |
| `input_frame` | H@0, epoch:u64@8, frame:u64@16, assignments:pointer@24, count:u32@32, reserved:u32@36 | 40 / 8 |
| `control_item` | id:u32@0, value_type:u32@4, value:u64@8 | 16 / 8 |
| `control_transaction` | H@0, epoch:u64@8, frame:u64@16, id:u64@24, items:pointer@32, count:u32@40, reserved:u32@44 | 48 / 8 |
| `original_frame` | H@0, frame_id@8, width:u32@32, height:u32@36, row_bytes:u32@40, video_format:u32@44, aspect_num:u32@48, aspect_den:u32@52, pixels:B@56, pcm:B@72, sample_rate:u32@88, channels:u32@92, pcm_frames:u64@96, tick_begin:u64@104, tick_end:u64@112 | 120 / 8 |
| `snapshot_info` | H@0, frame_id@8, generation:u64@32, region_mask:u64@40, complete_mask:u64@48, retained_bytes:u64@56, logical_tick:u64@64, region_count:u32@72, reserved:u32@76 | 80 / 8 |
| `snapshot_region` | H@0, kind:u32@8, encoding:u32@12, address:u64@16, byte_count:u64@24, element_bytes:u32@32, element_count:u32@36, data:B@40, bank:u32@56, flags:u32@60 | 64 / 8 |

`backend_info.id` y `.version` son ASCII, terminados en NUL y con cola cero. `build_id` es SHA-256 del manifiesto fijado de fuentes/dependencias/opciones del backend; el hash del binario distribuido se registra por separado. Los límites y los índices de enumeración no sustituyen esta identidad estable.

### Valores estables, límites y controles seguros

| Valores de resultado | Significado |
| --- | --- |
| `OK=0`, `END_OF_TRACE=1` | Operación completada; final explícito sin avance, respectivamente. |
| `INVALID_ARGUMENT=-1`, `ABI_INCOMPATIBLE=-2`, `BUFFER_TOO_SMALL=-3`, `INVALID_HANDLE=-4`, `WRONG_STATE=-5`, `UNSUPPORTED_CAPABILITY=-6` | Validación del contrato. |
| `UNKNOWN_CONTENT=-7`, `AMBIGUOUS_CONTENT=-8`, `INVALID_CONTENT=-9`, `IMPLEMENTATION_UNAVAILABLE=-10` | Detección/selección. |
| `STALE_EPOCH=-11`, `INPUT_LATE=-12`, `INPUT_OUT_OF_ORDER=-13`, `INPUT_DUPLICATE=-14`, `INPUT_INCOMPLETE=-15`, `INPUT_CONTRADICTORY=-16`, `INVALID_TRACE=-17` | Entradas/replay; sin avance ni aplicación pendiente. |
| `BUDGET_EXCEEDED=-18`, `CONTROL_PENDING=-19`, `CHECKPOINT_INCOMPATIBLE=-20`, `CHECKPOINT_INVALID=-21` | Presupuestos, pendiente única y persistencia. |
| `IO_NO_SPACE=-22`, `IO_ACCESS_DENIED=-23`, `IO_PUBLISH_FAILED=-24`, `IO_OTHER=-25`, `OUT_OF_MEMORY=-26`, `BACKEND_FAILURE=-27` | Fallo identificado; no se comunica un éxito parcial. |

- Fases `context.phase`: `NONE=0`, `VALIDATE=1`, `DETECT=2`, `CREATE=3`, `STEP=4`, `RESET=5`, `READ=6`, `WRITE=7`, `PUBLISH=8`, `RESTORE=9`, `CLOSE=10`. `os_domain`: `NONE=0`, `WIN32=1`; `os_code` captura `GetLastError` antes de otra operación. `context.flags` bit 0 confirma aplicación de controles; bit 1 informa cancelación. Razones: `NONE=0`, `RESET=1`, `RESTORE=2`, `CLOSE=3`. El resultado de la llamada lleva la identidad de la transacción, no solo un evento.
- Estados de sesión: `PAUSED=1`, `RUNNING=2`, `CLOSED=3`; la consulta nunca permite operar un token ya cerrado. Sistemas: `NONE=0`, `MEGA_DRIVE=1`; tipos de backend: `PRODUCT=1`, `TEST=2`. El backend sintético declara tipo TEST y sistema NONE; no anuncia otra consola. Detección: `RECOGNIZED=1`, `UNKNOWN=2`, `AMBIGUOUS=3`, `INVALID=4`.
- Bits de capacidades `u64`: 0 frame, 1 entradas, 2 vídeo, 3 PCM, 4 memoria, 5 paleta, 6 sprites, 7 geometría, 8 eventos, 9 checkpoint, 10 controles seguros, 11 observación temporal, 12 estado canónico. Los demás bits están reservados; las capacidades reales se consultan por backend/sesión. Completitud temporal por captura no se deduce del bit de capacidad.
- Máscara de regiones: memoria bit 0, paleta 1, sprites 2, geometría 3, temporal 4. Máscara de clases de eventos: ciclo de vida bit 0, controles 1, backend 2, capturas 3, pérdidas 4. Seleccionar un bit desconocido/no soportado se rechaza.
- Controles nativos: `OBSERVATION_REGION_MASK=1` y `OBSERVATION_EVENT_MASK=2`, ambos con `value_type=MASK64=1`. Su valor neutro es cero. Solo seleccionan observación; no modifican la máquina ni vídeo/PCM originales. IDs repetidos, tipos/valores incompatibles o bits no soportados invalidan el grupo completo. No hay controles de ROM/VRAM/registros ni de composición HD.
- Límites iniciales: contenido/checkpoint/traza 64 MiB cada uno; 2 puertos y 32 controles por puerto; 64 asignaciones por frame; 1 pendiente y 64 items por transacción; vídeo 1.024×1.024; 16.384 frames de muestras estéreo por frame emulado; 4.096 eventos con payload de hasta 256 bytes; 8 capturas, hasta 8 MiB cada una y 64 MiB retenidos en total; 65.536 frames de traza. Son techos de presupuesto, no compatibilidad prometida del catálogo. Un backend puede declarar límites inferiores; nunca truncar silenciosamente.
- Las multiplicaciones/sumas de tamaños se validan contra overflow y límites antes de leer, asignar o copiar. Todas las capacidades y límites se consultan antes de usar recursos.

### Firmas y ownership

Las abreviaturas C documentales `S`, `P`, `B`, `W`, `C` corresponden respectivamente a handle `uint64_t` de sesión, handle `uint64_t` de captura, vistas anteriores y `tessera_context_v1`. Cada función devuelve `tessera_status`; `ctx` es salida requerida, inicializada con H. Los nombres de estructura de la tabla llevan prefijo `tessera_` y sufijo `_v1`.

```c
tessera_status __cdecl tessera_api_query_v1(uint16_t major, uint16_t minor, tessera_api_info_v1 *out, C *ctx);
tessera_status __cdecl tessera_limits_query_v1(S session, tessera_limits_v1 *out, C *ctx);
tessera_status __cdecl tessera_backends_enumerate_v1(tessera_backend_info_v1 *out, uint32_t capacity, uint32_t *required, C *ctx);
tessera_status __cdecl tessera_detect_v1(B content, uint32_t system_override, tessera_detection_v1 *out, tessera_candidate_v1 *candidates, uint32_t capacity, uint32_t *required, C *ctx);
tessera_status __cdecl tessera_session_create_v1(const tessera_session_create_info_v1 *in, S *out, C *ctx);
tessera_status __cdecl tessera_session_query_v1(S session, tessera_session_info_v1 *out, C *ctx);
tessera_status __cdecl tessera_session_pause_v1(S session, C *ctx);
tessera_status __cdecl tessera_session_step_v1(S session, const tessera_input_frame_v1 *in, C *ctx);
tessera_status __cdecl tessera_session_reset_v1(S session, C *ctx);
tessera_status __cdecl tessera_session_close_v1(S session, C *ctx);
tessera_status __cdecl tessera_trace_validate_v1(B trace, C *ctx);
tessera_status __cdecl tessera_session_bind_trace_v1(S session, B trace, C *ctx);
tessera_status __cdecl tessera_session_replay_next_v1(S session, C *ctx);
tessera_status __cdecl tessera_controls_submit_v1(S session, const tessera_control_transaction_v1 *in, C *ctx);
tessera_status __cdecl tessera_original_frame_query_v1(S session, tessera_original_frame_v1 *out, C *ctx);
tessera_status __cdecl tessera_snapshot_acquire_v1(S session, P *out, C *ctx);
tessera_status __cdecl tessera_snapshot_retain_v1(P snapshot, C *ctx);
tessera_status __cdecl tessera_snapshot_query_v1(P snapshot, tessera_snapshot_info_v1 *out, C *ctx);
tessera_status __cdecl tessera_snapshot_region_query_v1(P snapshot, uint32_t index, tessera_snapshot_region_v1 *out, C *ctx);
tessera_status __cdecl tessera_snapshot_release_v1(P snapshot, C *ctx);
tessera_status __cdecl tessera_events_read_v1(S session, uint64_t cursor, W out, uint64_t *required, uint64_t *next_cursor, C *ctx);
tessera_status __cdecl tessera_losses_query_v1(S session, W out, uint64_t *required, C *ctx);
tessera_status __cdecl tessera_state_export_canonical_v1(S session, W out, uint64_t *required, C *ctx);
tessera_status __cdecl tessera_checkpoint_export_v1(S session, W out, uint64_t *required, C *ctx);
tessera_status __cdecl tessera_checkpoint_restore_v1(S session, B checkpoint, B trace, C *ctx);
tessera_status __cdecl tessera_status_format_v1(const C *context, W out, uint64_t *required);
```

- Para consultar tamaños, `out.data=NULL` y capacidad 0; `required` incluye todos los bytes de carga, sin terminador implícito. No se copia carga ni se avanza cursor con `BUFFER_TOO_SMALL`. El caller es dueño de sus buffers; Tessera no los retiene salvo copia interna completada antes del retorno. Estructuras y buffers de entrada/salida no pueden solaparse.
- `limits_query(0,...)` consulta límites globales; otro valor exige sesión viva. `detect` devuelve resultado/candidatos válidos también con los códigos semánticos de contenido; no crea máquina. Los demás fallos solo publican contexto/tamaños requeridos, sin carga parcialmente escrita. Orden de candidatos: `system` ascendente; no se elige según orden de archivos del entorno.
- Crear copia contenido, configuración y memoria persistente inicial; no toma ownership del caller. Las opciones de configuración usan TLV canónico, nunca texto dependiente de locale. Un override incompatible falla antes de crear. La configuración e identidad inicial se registran para reset y compatibilidad.
- `step` recibe exactamente el estado completo del próximo frame/época; ninguna asignación se repite, aunque coincida su valor. Se valida todo antes de aplicar pendientes. `replay_next` hace lo mismo con la traza ligada y deja pausa tras el frame; ejecución continua es un bucle propietario explícito. `pause` no añade un frame.
- Ligar una traza exige pausa con cero frames completados; valida 1…N y compatibilidad antes de copiarla. Sus entradas se identifican con la época vigente al entregarse. `END_OF_TRACE` conserva cursor, máquina y pendiente y deja pausa. Los 3.000 frames solicitados deben completarse para aprobar replay.
- Transacciones: ID no cero, época actual, frame objetivo siguiente y una sola pendiente; la segunda devuelve `CONTROL_PENDING`. `step` válido confirma aplicación en `ctx`; Engine solo entonces activa su política. La pausa no consume una transacción. Reset/restore/close exitosos cancelan con identidad/causa en `ctx`; fallos de reset/restore conservan todo.
- Reset exitoso vuelve al estado inicial registrado, controles/entradas neutrales, cero frames y cursor inicial bajo época nueva, en pausa. Engine conserva `.ay` y rearma su política en esa época con confirmación antes de presentar. La neutralización corresponde a reset; no se añade a restore.
- La vista de originales vence al siguiente avance/reset/restore/close exitoso. Consultas y exportaciones no la invalidan. Antes del primer frame o sin salida original disponible se devuelve `WRONG_STATE`. Una captura retenida posee bytes inmutables hasta última liberación, incluso tras reset/restore/close; no sirve como captura actual de otra época.
- El propietario serializa toda operación de sesión. Solo `snapshot_query`, `snapshot_region_query`, `snapshot_retain` y `snapshot_release` pueden usarse entre hilos sobre referencias válidas independientes. Adquirir/retener no espera al lector lento; agotar presupuesto devuelve `BUDGET_EXCEEDED`. Las salidas originales necesarias no dependen de la cola opcional.
- Eventos y pérdidas usan TLV versionado en buffers del caller. Evento RECORD: tags 1 secuencia U64, 2 sesión U64, 3 época U64, 4 frame U64, 5 tick U64, 6 código U32, 7 payload BYTES, 8 encoding U32. Lote: tags 1 eventos ARRAY, 2 siguiente cursor U64, 3 pérdidas RECORD. Pérdidas: tags 1 cantidad U64, 2 primera secuencia U64, 3 última secuencia U64, 4 causa U32, 5 época U64; flags de causa declaran si el rango es conocido. Las pérdidas acumuladas siguen consultables fuera de la cola. Cada lector conserva su propio cursor.
- El tick es entero; su frecuencia racional no nula se declara por backend/configuración. No equivale a reloj del sistema ni se presupone 60 Hz. Cambios de época no convierten contadores del observador en estado emulado.

### Precisiones verificadas en fase 1

- En arrays de estructuras extensibles, cada elemento empieza después de `header.size` bytes del anterior; su tamaño debe conservar la alineación natural del siguiente elemento. La enumeración valida todos los prefijos que publicará antes de escribir el primero. La exclusión de solapes abarca el tamaño declarado completo, incluidas las colas que Tessera no interpreta.
- La cancelación identifica en `ctx.frame_id` la sesión, época y frame objetivo de la pendiente cancelada; se consulta la nueva época por `session_query`. La confirmación identifica el frame efectivamente completado. Ambas quedan fuera de la cola saturable.
- Adquirir otra referencia del mismo snapshot devuelve el mismo token e incrementa una referencia comprobada, sin crear metadatos ilimitados. El presupuesto cuenta objetos de captura distintos, incluido el actual; `retained_bytes` cuenta sus bytes de regiones. La captura actual se omite con pérdida explícita cuando ocho objetos históricos siguen retenidos.
- Detección inicial: vistas Mega Drive RAW (offset 0) y COPIER512 (offset 512); se exige cabecera `SEGA`, longitud par >=512, vectores alineados y acotados y rango de ROM válido. El checksum no excluye homebrew. Evidencias de bits RAW=1, COPIER512=2, vectores=4 y cabecera=8; clases de evidencia RAW=1 y COPIER512=2. Dos vistas válidas siguen siendo ambiguas, incluso con override Mega Drive; el orden secundario es clase/evidencia ascendente. Un probe inválido no oculta otro válido.
- FX-01 reconoce su contenedor propio como sistema NONE, evidencia 16 y clase 3; su creación exige un ID TEST explícito. Esto no anuncia otra consola. El registro de producto Mega Drive crea `clownmdemu` tras validar detección, override y configuración.
- El codec y los oráculos de FX-01 se fijan en [synthetic-codec.md](synthetic-codec.md). Desde T022/T024 ambos perfiles admiten checkpoint y continuación; el perfil mínimo sigue rechazando observación no soportada.

### Encoding canónico, checkpoints y hashes

- Todos los enteros persistidos son little-endian. TLV: `tag:u32, encoding:u32, length:u64`, seguido de exactamente `length` bytes, sin alineación ni padding. Tags únicos y ascendentes; longitudes comprobadas antes de acceder. Encodings: `U32=1`, `I32=2`, `U64=3`, `I64=4`, `BYTES=5`, `UTF8=6`, `BOOL8=7`, `ARRAY=8`, `F32=9`, `F64=10`, `RECORD=11`. Arrays: cantidad `u64` y elementos con longitud `u64`; RECORD contiene TLV anidados. Floats, solo donde el codec los declare, son IEEE de ancho explícito y little-endian, sin valores no finitos; no se serializa un struct de host.
- Configuración común: tags 1 sistema U32, 2 backend UTF8, 3 build_id BYTES[32], 4 región U32, 5 frecuencia PCM U32, 6 mapa completo de controles ARRAY, 7 SHA-256 de memoria persistente inicial BYTES[32]. Cada control declara puerto/control U32, tipo U32 y rango I32. Digital es 0/1; cualquier analógico exige rango declarado. Tags desconocidos obligatorios/incompatibles se rechazan; no se sustituyen defaults silenciosamente.
- Estado canónico `TSSTATE1` (8 bytes) seguido de TLV: 1 codec_id UTF8, 2 codec_major U32, 3 codec_minor U32, 4 content_sha256 BYTES[32], 5 backend_build_id BYTES[32], 6 config_sha256 BYTES[32], 7 completed_frames U64, 8 logical_tick U64, 9 entradas fijadas ARRAY, 65536 estado de máquina RECORD. `config_sha256` se calcula sobre su TLV canónico, incluida identidad inicial pertinente.
- El codec de máquina debe enumerar todos los campos necesarios: registros y estado interno de CPU/chips, memorias/mapper/memoria persistente actual, temporizadores, scheduler, interrupciones, RNG cuando exista, filtros/remuestreo y PCM pendiente. Excluye punteros, handles, padding, pilas de ejecución, sesión/época, contadores del observador, métricas, presentación HD y pendientes. La posición de replay se persiste aparte y no se mezcla con el hash de máquina.
- **Codec implementado:** T022 incorporó al producto el codec explícito auditado de ClownMDEmu, con RECORD 65536 y frecuencia de tick identificados; round-trip y continuaciones K+1 se probaron entre procesos. El codec sintético se define en [synthetic-codec.md](synthetic-codec.md).
- Vídeo canónico `TSVIDEO1` (8 bytes), ancho U32, alto U32, formato U32=`RGBA8=1`, seguido de `width*height*4` bytes en orden superior→inferior/izquierda→derecha, R/G/B/A con alpha 255; excluye padding de pitch. La vista ABI puede declarar `row_bytes>=width*4`; solo los píxeles activos entran al encoding. No incluir composición, escalado ni geometría de ventana del host.
- PCM canónico `TSAUDIO1` (8 bytes), frecuencia U32, canales U32=2, frames de muestras U64, seguido de S16 little-endian intercalado L/R y exactamente `frames*4` bytes. Se fija la frecuencia en la configuración; no se supone cantidad constante por frame. La normalización/exportación determinista desde el candidato se audita en T006, sin conversión float implícita ni mixer del dispositivo. Cero muestras puede ser válido en un frame; silencio constante no aprueba FX-02.
- Cada digest es SHA-256 de la representación completa con su dominio anterior. Estado en frame 0 registra el inicio; tras completar cada frame 1…3.000 se comparan estado, vídeo original y PCM original, antes de composición/dispositivo. La presentación final tiene otro digest de Engine y nunca reemplaza esos tres. Comparar instrumentación/observación activa e inactiva.
- Traza `TSTRACE1` (8 bytes) y TLV: 1 content_sha256, 2 config_sha256, 3 longitud N U64, 4 frames ARRAY. Cada frame es RECORD con 1 frame U64 y 2 asignaciones ARRAY; cada asignación tiene puerto/control U32 y valor I32. Frames 1…N consecutivos; asignaciones ordenadas por puerto/control, completas y únicas. Época/sesión se vinculan al consumir, no se persisten como destinos obsoletos. El hash de traza usa todos estos bytes.
- Checkpoint: header de 64 bytes: magic `TSCHK001`@0, major:u16@8=1, minor:u16@10=0, header_bytes:u32@12=64, total_bytes:u64@16, payload_sha256:u8[32]@24, record_count:u32@56, reserved:u32@60=0; después TLV. Tags: 1 estado canónico BYTES, 2 trace_sha256 BYTES[32], 3 trace_length U64, 4 next_replay_frame U64, 5 initial_identity_sha256 BYTES[32]. Sin replay ligado, longitud/hash/cursor se codifican cero. Con replay, un checkpoint K tiene cursor K+1.
- Restore recibe la traza externa cuyo hash/longitud coinciden; la valida antes de reconstruir candidato. Conservar la traza junto al checkpoint permite restaurar en proceso nuevo sin persistir handles. Contenido/backend/configuración/identidad inicial deben coincidir. La carga exitosa crea época nueva, cancela la pendiente anterior y no incorpora HD; el error conserva sesión y pendiente. Exportar no aplica ni cancela la pendiente actual.
- Guardado a archivo: publicar el encoding completo mediante temporal en el mismo volumen y reemplazo atómico validado en Windows. Antes de publicar, conservar destino previo o su ausencia. Después de publicar puede comunicarse éxito. Interrupción del proceso: anterior válido o nuevo completo; primer guardado: ausente o nuevo completo. Un temporal parcial no está publicado. Probar READ/WRITE/PUBLISH y espacio/acceso/publicación con contexto del SO. La garantía no exige durabilidad ante pérdida de alimentación.
- Checkpoints 500/1.500/2.500 representan el estado posterior a K; restaurar en procesos nuevos y comparar K+1…K+500 con la referencia continua. La existencia de encoder/contenedor no demuestra todavía continuidad del codec real.

### Mediciones y enlace de Gate 0

- CPU por etapa y GPU/espera cuando corresponda, memoria máxima, asignaciones, bytes/frame, volumen/pérdidas de eventos y respaldos identifican backend, configuración, origen, cobertura, calentamiento, repeticiones y cantidad de muestras. Los relojes de medición no entran al estado canónico.
- Entrega a presentador de Runtime: intervalos consecutivos en milisegundos con reloj monotónico, p50/p95/p99 y máximo. Presentación efectiva, si existe, es otra serie con origen/cobertura declarados. Cortes de audio: episodios continuos de muestras solicitadas no disponibles a tiempo, cantidad faltante y duración total/máxima; silencio esperado es audio válido.
- Referencia local de enlace: `C:/ayther/AYTHER-Engine/install/windows-release-engine-vpx`, target estático `Ayther::engine`, SDK 0.1.0/Core ABI 8, C++20 y `/MD` con `Release`; dependencias en `C:/ayther/AYTHER-Engine/build/windows-release-engine-vpx/vcpkg_installed/x64-windows`. Importar el paquete con `find_package(Ayther 0.1 CONFIG REQUIRED COMPONENTS engine)`; no enlazar manualmente archivos ni introducir Engine como dependencia de Tessera.
- Un consumidor externo de Gate 0 debe llamar un símbolo real fuera de línea de Engine y comprobar un intercambio C/POD entre unidades compatibles. Su enlace no demuestra aún conducción de Tessera por Engine: eso corresponde a T015–T018. T006 también exige el probe del candidato real y sus oráculos de T005. No hay resultados binarios nuevos acreditados por este anexo.

### Trazabilidad hacia tests previstos

| Requisito / criterio | Pruebas que deben aportar evidencia; tareas |
| --- | --- |
| RF-01 | Ciclo de vida, reset inicial, frames 1…N, entradas completas/negativas y EOF sin avance; T009/T010/T013/T014/T018/T019/T030/T031. |
| RF-02 / CA-02 | Tres digests por frame, 3.000 completos/dos procesos y oráculos independientes; T004–T006/T009/T014/T020/T021/T024. |
| RF-03 | Negociación/ABI/tamaños/capacidades ausentes sin mutación; T007–T010/T016/T018/T019. |
| RF-04 / CA-04 | Dos lectores, coherencia, retención histórica, saturación y no interferencia; T012/T014/T021/T025. |
| RF-05 / CA-05 | FX-03, reglas coincidentes, superposición y píxeles esperados preservando originales; T026–T029/T031. |
| RF-06 | Pendiente única, CONTROL_PENDING, pausa, confirmación y rechazo íntegro; T007/T012/T016/T027/T029/T031. |
| RF-07 | Respaldo, invalidación, cancelación fuera de cola y fallos que conservan pendiente; T012/T016/T023–T025/T027/T029/T031. |
| RF-08 | Rechazo previo a activar `.ay`, rangos/rutas/hashes/metadatos; T026/T028/T029. |
| RF-09 / CA-03 | Formatos, K→K+1, exportación con pendiente, primer guardado y E/S; T006/T022–T024/T033. |
| RF-10 | Secuencias/cursor propio/gaps/pérdidas fuera de cola y cierre; T012–T014/T025/T033. |
| RF-11 / CA-09 | ROM renombrada, inválidos/ambigüedad/override/backend ausente; T011/T013/T017–T019/T030. |
| RF-12 / CA-10 | Mismo contrato por CLI/Engine y Runtime obligatorio; T015–T018/T021/T024/T027–T031. |
| RF-13 / CA-11 | A/B equivalente por etapas, series/unidades/cobertura y línea base; T014/T018/T021/T031/T032/T036. |
| CA-01 / CA-06 | Corpus con procedencia, entradas conocidas y ventanas/transiciones de audio reales; T002/T005/T009/T019–T021/T024/T026. |
| CA-07 | Negativos, EOF, cancelaciones con saturación, preservación y fallos E/S inducidos; T007/T010/T012/T014/T023/T024/T027–T029/T033. |
| CA-08 | Todos los fixtures, CI y paquete verificados en entorno limpio; T034–T036. |
| RC-01 / RC-02 | Dependencias justificadas, trazabilidad e independencia sin Engine/UI/GPU; T001–T004/T008/T013/T015/T034/T036. |
| RC-03 | Tests antes de implementación, regresiones y diagnósticos compatibles; T003/T007/T014/T018/T024/T031/T033/T035/T036. |
| RC-04 | Inglés propio en código/logs, español en documentación/mensajes y fuentes externas intactas; T002/T008/T013/T034/T036. |
| RC-05 | Fuentes, checksums, permisos, instalación y ejecución limpias; T002/T034–T036. |

La tabla relaciona requisitos y tareas; por sí sola no acredita ejecución. Los
[resultados de fase 1](../../../docs/phase-1-results.md) identifican las pruebas
autónomas ejecutadas de T007–T014. Engine, el codec de producto y restore siguen
en las tareas posteriores indicadas, conservando el cierre privado de T006.
