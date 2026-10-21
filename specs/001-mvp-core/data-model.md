# Modelo de datos — MVP nativo de AYTHER Tessera

**Estado:** diseño aprobado; sintético/API/CLI, Engine nativo, Mega Drive y persistencia de T007–T024 validados localmente. Presentación HD y Runtime siguen pendientes.
**Referencias:** [spec](spec.md), [constitución](../../docs/constitution.md) y [contrato nativo](contracts/native-api.md).
Los campos siguientes son semánticos: no fijan layouts de ABI ni formatos binarios.

## Límites de responsabilidad

- Tessera ejecuta la máquina, detecta y despacha sistemas, publica observación nativa y persiste estado.
- Engine consume la API C de Tessera y es dueño de paquetes `.ay`, composición HD y políticas de presentación. Tessera valida y aplica los controles nativos; Engine activa la política preparada después de su confirmación.
- Runtime consume Engine y su recorrido integrado es obligatorio para aceptar el MVP; no es una dependencia de ejecución del núcleo de Tessera.
- La ruta objetivo no utiliza Libretro ni traduce un descriptor de observación heredado.
- El backend real se selecciona en gate 0; solo Mega Drive pertenece al MVP. El perfil sintético prueba contratos.

## Entidades e invariantes

| Entidad | Datos semánticos mínimos | Invariante |
| --- | --- | --- |
| `ContentIdentity` | Hash del contenido, tamaño y formato identificado. | No deriva su identidad de la ruta o extensión. |
| `DetectionResult` | Candidatos, evidencias, resultado reconocido/ambiguo/desconocido/inválido y override solicitado. | Detectar un sistema no implica disponer de implementación; contenido truncado o estructuralmente inválido tiene resultado explícito. |
| `SystemRegistration` | Identificador estable, factory, backend/versiones, capacidades y configuración admitida. | Solo registros implementados pueden crear sesiones. |
| `Session` | Identidad, contenido, sistema/backend, configuración, estado inicial registrado, ciclo de vida, época, frames completados y cursor de replay. | Un propietario serializa las mutaciones; un reinicio exitoso recupera el estado inicial bajo una época nueva. |
| `InputFrame` | Época y frame destino, estado completo de los controles configurados por puerto. | Exactamente una asignación por puerto/control; solo se consume para el próximo frame válido de la época actual. |
| `ReplayTrace` | Versión, hash, contenido/configuración asociados, secuencia de estados completos, longitud N y cursor. | Frames consecutivos de 1 a N, sin huecos ni duplicados; se valida antes de reproducirse y termina explícitamente. |
| `MachineState` | Estado lógico de chips, memoria, entradas fijadas y fases temporales necesarias. | Basta para continuar dentro de la matriz declarada; no contiene punteros. |
| `OriginalFrame` | Identidad de frame, video, geometría, formato y audio con cantidad de muestras. | Proviene del backend antes de cualquier modificación de presentación. |
| `Snapshot` | Sesión, época, generación, frame, regiones, paleta, sprites, geometría y completitud temporal. | Datos coherentes e inmutables durante su vigencia anunciada. |
| `ObservationEvent` | Código, secuencia, reloj lógico, sesión/época/frame y carga tipada/versionada. | Observar o consumir eventos no modifica `MachineState`. |
| `LossReport` | Rango o cantidad de pérdidas conocida, causa y periodo afectado. | Una pérdida no se oculta por saturación de la propia cola de eventos. |
| `PresentationTransaction` | Identidad, sesión, época, frame objetivo, controles nativos y política de Engine preparados, estado y resultado. | Una sola pendiente por sesión; Tessera aplica los controles al inicio del próximo frame ejecutado y Engine activa su política tras confirmar identidad, época y frame. |
| `PresentationFrame` | Identidad original, controles/recursos usados, salida final y estado exacto/respaldo. | Una salida alterada no sustituye al video original para comparar determinismo. |
| `Checkpoint` | Formato/versiones, identidades, configuración, máquina, frames completados y contexto de replay necesario. | Representa el estado posterior al frame K y continúa en K+1; excluye presentación HD y transacciones pendientes. |
| `PersistenceResult` | Operación, resultado, código estable, fase y diagnóstico del sistema operativo; publicación completada cuando corresponda. | No comunica éxito de guardado antes de publicar el archivo completo; un error de carga conserva la sesión. |
| `Measurement` | Etapa/configuración, unidad, punto/origen/cobertura de la serie, muestras, resúmenes y recursos utilizados. | Métricas y reloj monotónico de medición quedan fuera del estado lógico; entrega y presentación efectiva tienen series distintas. |

## Detección, registro y estrategia

1. `SystemDetector` inspecciona bytes con límites documentados y produce `DetectionResult` sin crear una máquina.
2. Un override explícito se valida frente al contenido, el registro y las capacidades; no evita validar tamaños.
3. `SystemRegistry` resuelve una política estable y una factory; no selecciona según el orden de DLLs del entorno.
4. La factory crea una estrategia `IEmulatedSystem`; la sesión registra la elección antes del primer frame.
5. El registro del MVP contiene Mega Drive y un perfil sintético propio identificado como prueba, no como otra consola.
6. Ambigüedad, formato desconocido, sistema no soportado o backend ausente producen resultados distintos y definidos.

## Ciclo de vida y tiempo lógico

- Ciclo previsto: creada → preparada/pausada → ejecución por frames → pausada → cerrada; los errores no autorizan transiciones parciales.
- Abrir contenido registra el estado inicial con contenido, backend, configuración y datos iniciales de memoria persistente, cuando correspondan.
- Abrir contenido, reiniciar y restaurar se preparan sin sustituir parcialmente la sesión activa; una preparación fallida conserva la máquina, cursor, época, controles y transacción pendiente anteriores.
- Un reinicio exitoso reconstruye ese estado inicial, crea una época nueva, deja cero frames completados, entradas neutrales, cursor de replay al inicio y sesión pausada; descarta las entradas pendientes de la época anterior.
- Antes del primer avance hay cero frames completados; el primero completa el frame 1. Un checkpoint del frame K representa el estado después de completar K y su continuación empieza en K+1.
- Cada avance válido consume el estado completo de entradas de ese frame y produce sus salidas originales y observación; un rechazo no avanza ni aplica controles pendientes.
- El reloj lógico usa unidades enteras documentadas; el reloj del sistema no decide la evolución emulada.
- Reiniciar o restaurar con éxito abre una época nueva. Un mismo número de frame en otra época no identifica los mismos datos.
- Sesión, época, generación, contadores del observador y rutas no forman parte del hash del estado emulado.

### Entradas y cursor de replay

- `InputFrame` lleva la época actual y el próximo frame esperado. Cada puerto/control configurado tiene exactamente una asignación; se rechazan conjuntos incompletos, asignaciones repetidas aunque coincidan y valores contradictorios.
- Una entrada para un frame ya iniciado/completado es tardía; una posterior al próximo esperado está fuera de orden. Se rechazan también las entradas de épocas obsoletas, sin alterar la máquina ni aplicar la transacción pendiente.
- `ReplayTrace` se valida completa antes de reproducirse: frames ordenados y consecutivos de 1 a N, sin duplicados, huecos ni incompatibilidades. El rechazo conserva la sesión.
- El cursor identifica el siguiente frame por consumir. Tras un reinicio exitoso vuelve al inicio de la misma traza; cada entrega se identifica con la nueva época. Tras una restauración exitosa continúa desde el contexto de replay del checkpoint.
- Agotar la traza devuelve `END_OF_TRACE` y deja la sesión pausada, sin ejecutar un frame adicional ni aplicar controles pendientes. La prueba de 3.000 frames debe completar los 3.000 solicitados.

## Propiedad, capturas y presupuesto

- Los buffers originales prestados tienen vigencia explícita hasta la siguiente operación que pueda reutilizarlos.
- Una captura retenida posee datos inmutables o memoria fijada que el backend no puede sobrescribir; dos lectores comparten esa captura.
- La retención tiene límite de bytes y objetos documentado. Retener/liberar modifica referencias, no la máquina ni el delta de otro lector.
- Al agotarse el presupuesto, la retención falla de forma explícita o se omite observación opcional con pérdida informada; la emulación no espera al lector.
- Las salidas originales necesarias para el frame no se sustituyen por una captura incompleta ni se descartan silenciosamente.
- Restaurar/reiniciar invalida el uso de capturas anteriores como datos actuales; sus copias retenidas siguen íntegramente legibles hasta liberarlas.
- Cerrar no libera memoria aún retenida por el consumidor; este debe liberar sus referencias mediante el contrato nativo.

## Gaps y presentación

- Secuencias y reportes permiten localizar discontinuidades; un consumidor no vacía el historial disponible para otro.
- Perder un delta exige una captura completa para reanudar ese historial. La pérdida temporal hace no reconstruible el frame afectado.
- Engine usa el video original completo cuando faltan datos o el modo no está soportado y registra la causa por frame.
- La causa de respaldo no se arrastra automáticamente a un frame siguiente que sí tenga información completa.
- Engine invalida sus cachés al cambiar época, paquete, controles, geometría o versión de recursos relevante.
- `PresentationTransaction`, assets y cachés HD no se incorporan a `MachineState`; las entradas del juego sí pueden modificarlo.

### Transacciones y políticas de presentación

- Se valida el grupo íntegro antes de aceptarlo. Mientras exista una pendiente, otra solicitud, incluso para el mismo control, devuelve `CONTROL_PENDING` y conserva la anterior y los controles efectivos.
- La pendiente permanece mientras la sesión esté pausada. Tessera aplica sus controles completos al comienzo del próximo frame válido ejecutado y confirma identidad, época y frame; solo entonces Engine activa la política preparada.
- Reiniciar, restaurar o cerrar con éxito cancela la pendiente anterior con identidad y causa como parte del resultado de la operación, sin depender de la cola de eventos saturable. Un reinicio o una restauración fallidos la conservan.
- En el reinicio exitoso, los controles nativos vuelven a su configuración neutra. Engine conserva el paquete seleccionado y rearma su política mediante una transacción de la nueva época, confirmada antes de presentar el frame correspondiente.
- Exportar un checkpoint no aplica ni cancela la pendiente de la sesión que continúa. La restauración exitosa cancela la pendiente anterior e invalida las cachés afectadas; el checkpoint no restaura presentación HD ni transacciones pendientes.

### Precedencia de reglas y recursos superpuestos

- Engine conserva la resolución de `.ay`; Tessera entrega las identidades y datos de profundidad necesarios sin duplicar esas políticas.
- Para tiles, un override de ejecución aplicable prevalece sobre el catálogo; en este gana la primera entrada `[[sub]]` cuyas condiciones se cumplen, según el orden declarado.
- Los sprites superpuestos conservan el orden de la cadena SAT y su prioridad respecto de los planos. Los otros tipos admitidos identifican el contrato versionado de Engine que define su precedencia y desempate.
- FX-03 declara reglas simultáneamente aplicables y recursos superpuestos con ganador y píxeles esperados explícitos, repetibles entre procesos y conservando los hashes originales.

## Persistencia y comparación

- Definir versiones, límites, orden de bytes, tamaños, validación de rangos e integridad antes de implementar el formato.
- Serializar campos explícitos; no persistir punteros, handles, padding de structs ni direcciones de buffers.
- Verificar contenido, sistema/backend, configuración y compatibilidad antes de reconstruir la instancia candidata.
- Incluir las fases temporales y el contexto de entradas/replay necesarios para reproducir la continuación; excluir presentación HD y transacciones pendientes. No prometer migraciones ajenas a la matriz.
- Guardar con temporal y publicación atómica del archivo completo. Si existía un destino válido, una interrupción del proceso deja el anterior o el nuevo completo; en el primer guardado queda ausente o contiene el nuevo completo. Un temporal incompleto no es un checkpoint publicado.
- Un fallo anterior a publicar conserva el destino previo o su ausencia; solo se comunica éxito después de publicar. La garantía cubre interrupción del proceso y errores de E/S, sin exigir durabilidad ante pérdida de alimentación.
- Lectura, escritura y publicación identifican fase, código estable y diagnóstico del sistema operativo; se distinguen espacio insuficiente, acceso denegado y fallo de publicación. Un error de lectura o restauración conserva íntegramente la sesión y su pendiente.
- Comparar por separado estado canónico, video original y audio original; documentar formatos, orden y puntos de comparación.
- El hash de presentación final pertenece a la validación Engine+Tessera; no sustituye ninguno de los tres anteriores.

## Mediciones y evidencia

- Cada serie identifica punto de medición, origen, unidad, configuración, cobertura, calentamiento, repeticiones y cantidad de muestras. Se separan emulación, salidas originales, observación, capturas, hashing y composición.
- Registrar p50/p95/p99, memoria máxima, asignaciones, bytes copiados por frame, volumen/pérdidas de eventos y respaldos; separar CPU, GPU y espera de presentación cuando corresponda.
- La regularidad de entrega se expresa como intervalos en milisegundos, medidos con reloj monotónico entre entregas consecutivas de frames al presentador de Runtime; se publican p50/p95/p99 y máximo.
- Los timestamps de presentación efectiva disponibles forman otra serie con origen y cobertura declarados; no se presentan como intervalos de entrega ni se presume su disponibilidad.
- Un corte de audio es un intervalo continuo en que el dispositivo solicita muestras y Runtime no dispone de ellas a tiempo. Se cuentan episodios, muestras faltantes y duración total y máxima. El silencio previsto por el fixture es audio válido.
- Comparar Tessera autónoma y Engine con el mismo backend, contenido, traza y resultados equivalentes; separar referencias con otro emulador. Los presupuestos derivan de la línea base medida, sin incrementos arbitrarios de FPS.

## Idioma y procedencia

- Código, comentarios y logs propios nuevos en inglés; documentación y mensajes al usuario en español; eventos estructurados con códigos estables.
- Las fuentes externas conservan su idioma y avisos con procedencia documentada. Las copias de referencia se mantienen intactas en su idioma original.

## Gates de verificación

- Gate 0: elegir backend real verificando dos candidatos, permisos, integración nativa, capacidades y restauración exigidas.
- FX-01: perfil sintético mínimo/completo, entradas con respuesta conocida, salidas y estado de referencia propios.
- FX-02: demo del backend real con una entrada de efecto conocido, control sin ella y oráculos de estado observable, video original y actividad/transiciones sonoras derivados de su fuente o generador antes de contrastarlos con Tessera. Verificarlos por API autónoma y Engine; la ROM actual permanece intacta y no configura FM/PSG.
- FX-03: `.ay` gráfico real, recurso conocido, coincidencias y solapamientos con ganador y resultado visual esperados según el contrato de Engine; el pack de texto actual no satisface el requisito.
- CA-09/CA-10: probar detección/despacho y consumo de esta API desde Engine, incluida retención, rechazo y respaldo; completar el recorrido obligatorio Runtime→Engine→Tessera en el entorno declarado.
- CA-11: medir por etapas con configuración reproducible; los hashes de archivos incorporados no son evidencia de emulación.
