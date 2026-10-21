# Especificación del MVP — AYTHER Tessera

**Identificador:** `001-mvp-core`  
**Creada y aprobada:** 2026-10-19  
**Estado:** aprobada; T001–T036 verificadas. Los locks nativos y los dos paquetes de software públicos coinciden; el perfil GPU/audio pasó en CI físico privado. La auditoría de aceptación T036 está cerrada.
**Solicitud:** guardar el MVP aprobado, incorporando la demo de Engine/Lab y
los fixtures pendientes; actualizarlo con integración nativa Engine→Tessera,
detección y despacho por consola y mediciones de rendimiento.  
**Revisión aprobada:** 2026-10-19; integración nativa, plan técnico asociado y diez correcciones de QA.  
**Norma aplicable:** [Constitución aprobada](../../docs/constitution.md).

## Objetivo y alcance

Un desarrollador debe poder ejecutar una demo de Mega Drive, inspeccionar sus
recursos, sustituir un recurso gráfico durante la ejecución y verificar que el
comportamiento emulado permanece idéntico.

- Plataforma de entrega inicial: Windows x64.
- Sistema inicial: Mega Drive, mediante un backend real seleccionado y un
  backend sintético propio para verificar contratos.
- Entrega: núcleo con API C nativa, herramienta de consola, corpus de pruebas,
  documentación e integración verificable Engine→Tessera y Runtime→Engine→Tessera.
- Selección: detección del contenido y despacho a una implementación registrada
  por consola; el MVP implementa Mega Drive y un backend sintético de pruebas.
- Presentación: sustitución gráfica básica mediante Engine, respaldo al vídeo
  original y conservación del audio original de Tessera.
- Licencia: MIT para el material original nuevo de Tessera; dependencias,
  fuentes copiadas y recursos conservan sus permisos y avisos correspondientes.
  La distribución debe permitir uso comercial y documentar sus obligaciones.

Quedan fuera del MVP: editor gráfico, Master System y otros sistemas, Sega CD,
sustitución sonora, identificación semántica de sonidos, red y multijugador.
La arquitectura admite evolución multisistema; esta entrega valida Mega Drive.
No se promete compatibilidad con el catálogo completo ni un presupuesto de
rendimiento sin mediciones previas.

### Frontera entre proyectos

Tessera identifica el sistema, ejecuta la máquina y entrega vídeo/audio originales,
capturas, eventos, estados persistentes y controles seguros mediante una API C
nativa. Engine conduce la sesión de producto, identifica recursos, valida los
paquetes `.ay` y compone la presentación HD. Runtime y Lab consumen Engine.

La ruta nativa no utiliza libretro ni traduce capturas a `ayther_interface_v1`.
Engine mantiene su integración de Tessera como un módulo privado. Tessera no
depende de Engine, Runtime, Lab ni de sus tipos internos; la CLI utiliza la misma
API pública. Las pruebas integradas pueden consumir los paquetes de esos proyectos
sin introducir esa dependencia en la biblioteca del núcleo.

RF-05, RF-07 y RF-08 se aceptan en el recorrido Engine–Tessera, conservando todos
sus criterios. Reutilizar Engine no demuestra automáticamente su cumplimiento:
las capacidades faltantes se implementan y prueban donde corresponda, sin crear
otro sistema de paquetes o compositor HD por defecto.

## Escenarios de usuario y pruebas

### Historia 1 — Ejecutar y reproducir una sesión (prioridad P1)

Como integrador, quiero abrir una demo y ejecutar entradas registradas sin
interfaz gráfica para reproducir y comparar el comportamiento del núcleo.

**Valor:** establece una ejecución verificable independiente del editor.

**Prueba independiente:** abrir la demo desde consola, ejecutar la traza,
pausar, avanzar un frame, reiniciar, cerrar y exportar identidades y hashes.

**Escenarios de aceptación:**

1. Dada una demo y configuración soportadas, al ejecutar la misma traza en dos
   procesos independientes, los hashes de estado emulado, vídeo original y audio
   original coinciden en todos los puntos de comparación declarados.
2. Dada una sesión activa, al pausar y avanzar un frame, la sesión se detiene en
   un límite de frame y avanza exactamente una unidad antes de volver a pausarse.
3. Dado contenido incompatible o una capacidad ausente, al solicitar la operación
   se informa un resultado explícito sin invalidar una sesión válida existente.
4. Dados FX-01 y una demo del backend real con respuesta conocida a entradas,
   al reproducir una entrada documentada su estado observable cambia según el
   resultado esperado; el control sin esa entrada produce el resultado distinto
   previsto. Las expectativas proceden del programa fuente o generador, y se
   verifican mediante la API pública autónoma y mediante Engine.
5. Dada la misma ROM Mega Drive con otro nombre o extensión, al inspeccionar su
   contenido se conserva el sistema detectado y el despacho. Contenido inválido,
   desconocido o ambiguo produce un resultado definido, sin selección silenciosa.
6. Dada una selección explícita incompatible o una implementación ausente, al
   abrir contenido se rechaza la petición sin dañar una sesión válida existente.
7. Dada una sesión avanzada, al reiniciarla con éxito su estado emulado coincide
   con el estado inicial registrado, excluyendo identidades de sesión y época.
   Queda pausada con cero frames completados y entradas neutrales; el siguiente
   avance ejecuta el frame 1 con la traza desde su inicio.
8. Dadas entradas duplicadas, contradictorias, tardías, fuera de orden o de una
   época obsoleta, se rechazan conforme a las reglas de entradas sin avanzar la
   máquina ni aplicar controles pendientes. Agotar la traza devuelve
   `END_OF_TRACE` y deja la sesión pausada sin ejecutar otro frame.

### Historia 2 — Inspeccionar y sustituir un recurso (prioridad P1)

Como creador de mejoras gráficas, quiero identificar un recurso de Tessera y
asociarlo a un asset autorizado mediante un paquete `.ay` consumido por Engine
para cambiar su presentación.

**Valor:** demuestra la sustitución en ejecución conservando la máquina emulada.

**Prueba independiente:** ejecutar Engine con Tessera nativa, con y sin el paquete
gráfico real, exportar capturas y comparar presentación y salidas originales.
La aceptación final añade el recorrido de presentación mediante Runtime.

**Escenarios de aceptación:**

1. Dada una captura, al leerla desde dos consumidores, ambos reciben datos
   idénticos e inmutables de la misma sesión, época y generación.
2. Dado un paquete válido y un recurso presente, al activar la sustitución cambia
   al menos un píxel previsto de la presentación final; el estado emulado y el
   vídeo y audio originales conservan sus hashes.
3. Dada una sesión sin transacción pendiente y un grupo válido de controles de
   presentación, al aceptarlo se aplica completo al comienzo del próximo frame
   ejecutado, con confirmación de identidad, época y frame; si es inválido, se
   rechaza completo. Mientras la sesión esté pausada, permanece pendiente.
4. Dado un frame no reconstruible o un historial incompleto, al presentarlo se
   utiliza en Engine el frame original completo y se registra la causa del respaldo.
5. Dadas dos solicitudes consecutivas, incluida la modificación del mismo
   control, la segunda se rechaza con `CONTROL_PENDING` mientras la primera
   siga pendiente; se conservan la primera y los controles efectivos.
6. Dadas reglas simultáneamente aplicables y recursos superpuestos de FX-03,
   la precedencia de Engine produce el ganador y los píxeles esperados en dos
   procesos independientes, conservando los hashes originales.
7. Dada una transacción pendiente, reiniciar, restaurar o cerrar con éxito la
   cancela con identidad y causa en el resultado de la operación, incluso con
   la cola de eventos saturada. Un reinicio o una restauración fallidos la conservan.

### Historia 3 — Guardar y continuar una sesión (prioridad P1)

Como desarrollador, quiero guardar una sesión y restaurarla en otro proceso para
continuar exactamente desde el mismo punto de ejecución.

**Valor:** hace reproducibles los checkpoints y las investigaciones de errores.

**Prueba independiente:** guardar en tres checkpoints, cerrar el proceso y
comparar cada continuación restaurada con la ejecución continua de referencia.

**Escenarios de aceptación:**

1. Dado un estado compatible, al restaurarlo en un proceso nuevo se reproducen
   los hashes de estado, vídeo original y audio original del tramo posterior.
2. Dado un estado corrupto o incompatible, o un error de lectura, al intentar
   cargarlo se informa el rechazo o error sin modificar la sesión anterior,
   incluida su transacción de presentación pendiente.
3. Dado un guardado interrumpido por terminación del proceso, si existía un
   destino válido queda el archivo anterior o el nuevo completo. En el primer
   guardado, el destino queda ausente o contiene el nuevo completo. Nunca se
   acepta un archivo parcial ni un temporal incompleto como checkpoint publicado.
4. Dada una restauración o un reinicio exitosos, las capturas y cachés de la época previa
   quedan invalidadas y no se confunden con frames del mismo número.
5. Dados fallos inducidos de lectura, escritura o publicación, se identifican
   espacio insuficiente, acceso denegado y fallo de publicación mediante código
   estable, fase y diagnóstico del sistema operativo. Un fallo anterior a publicar
   conserva el destino previo o su ausencia; un fallo de carga conserva la sesión.
6. Dado un checkpoint exportado con una transacción pendiente, el archivo
   conserva la máquina y el contexto de replay, sin presentación HD ni
   transacciones pendientes. Exportarlo no aplica ni cancela la pendiente de la
   sesión que continúa ejecutándose.

### Casos límite

- Contenido, configuración o backend diferentes de los declarados por un estado
  o paquete: rechazo explícito conforme a la matriz de compatibilidad.
- Estados y paquetes con tamaños, rangos, versiones, hashes o referencias
  inválidos, o rutas que escapan del paquete: rechazo sin cambios parciales.
- Consumidor lento, saturación de eventos y cierre durante actividad: pérdidas
  observables, terminación definida y ausencia de bloqueo de la emulación.
- Cambio de paquete, controles o geometría: invalidación del resultado afectado.
- Pérdida de información temporal: respaldo explícito; nunca declarar exacta una
  reconstrucción incompleta.
- Audio silencioso constante: no satisface el fixture sonoro obligatorio.
- Imagen visible estable de la ROM canónica: no es un fallo; sus cambios de
  estado gráfico deben observarse sin exigir movimiento en la imagen presentada.
- Dos lectores, siguiente frame, restauración, cierre y liberación de capturas:
  se respeta la vigencia publicada sin vistas mutables ni referencias inválidas.
- Instrumentación de rendimiento activa o inactiva: no cambia el reloj lógico,
  el despacho, las entradas ni el resultado emulado.
- Dos transacciones consecutivas, incluso sobre el mismo control: una sola
  pendiente y rechazo de la segunda sin reemplazo ni cambios efectivos.
- Transacción pendiente al reiniciar, restaurar o cerrar: cancelación explícita
  en el resultado de la operación exitosa; un reinicio o una restauración fallidos
  la conservan.
- Primer guardado interrumpido, disco lleno, acceso denegado y publicación
  fallida: se conservan destino y sesión conforme a RF-09 y sus reglas de E/S.
- Reglas gráficas coincidentes y recursos superpuestos: precedencia y píxeles
  esperados explícitos de FX-03, repetibles entre procesos.
- Entradas duplicadas, contradictorias, tardías, fuera de orden, de época
  obsoleta o traza agotada: resultado explícito sin avance ni aplicación pendiente.
- Backend real que ignora entradas o produce un resultado incorrecto repetible:
  falla el oráculo funcional independiente de FX-02 aunque sus hashes se repitan.

## Requisitos

### Requisitos funcionales

| ID | Requisito | Criterio de aceptación |
| --- | --- | --- |
| RF-01 | Gestionar sesiones sin interfaz gráfica. | La consola permite abrir, ejecutar, pausar, avanzar un frame, reiniciar y cerrar. Registra identidad y hashes del contenido y backend, configuración y traza de entradas. Reinicio, numeración de frames, entradas y final de traza cumplen las reglas de ejecución definidas abajo. |
| RF-02 | Producir ejecución determinista. | Mismo contenido, backend, configuración y entradas producen hashes idénticos de estado emulado, vídeo original y audio original en ejecuciones independientes de la matriz soportada. |
| RF-03 | Declarar capacidades. | El consumidor consulta capacidades antes de utilizarlas. Una operación ausente devuelve un resultado explícito sin invalidar la sesión. El backend real cubre todas las capacidades obligatorias del MVP. |
| RF-04 | Publicar capturas coherentes. | Memoria, paleta, sprites y geometría disponibles pertenecen a una misma sesión, época y generación. Dos lectores reciben datos idénticos e inmutables; leerlos no altera la emulación. |
| RF-05 | Sustituir gráficos durante la ejecución mediante Engine–Tessera. | Engine consume la observación nativa y un paquete `.ay` real para sustituir al menos un recurso con un asset autorizado. Activar o desactivar la sustitución conserva los hashes emulados y las salidas originales de Tessera; la presentación final cambia únicamente donde corresponde a la regla aplicada. La precedencia, desempate y composición cumplen las reglas gráficas definidas abajo y se verifican con FX-03. |
| RF-06 | Aplicar controles de forma atómica. | Cada sesión admite una transacción pendiente, identificada por sesión, época y frame objetivo, validada íntegramente antes de aceptarla. Otra solicitud mientras exista pendiente se rechaza con `CONTROL_PENDING`, conservando la anterior y los controles efectivos. La aceptada se aplica completa al comienzo del próximo frame ejecutado y confirma identidad, época y frame; mientras esté pausada continúa pendiente. Una transacción inválida se rechaza completa, sin cambios parciales. Las cancelaciones cumplen las reglas de ejecución. |
| RF-07 | Ofrecer respaldo e invalidación en Engine–Tessera. | Tessera informa las limitaciones y pérdidas; Engine presenta el frame original y registra la causa cuando no puede reconstruirlo. Restaurar, reiniciar o cambiar paquete, controles o geometría invalida los datos afectados. Reiniciar, restaurar o cerrar con éxito cancela la transacción pendiente con identidad y causa; un reinicio o una restauración fallidos conservan la sesión y su pendiente. |
| RF-08 | Cargar paquetes gráficos `.ay` verificables mediante Engine. | El manifiesto y metadatos del paquete declaran versión, contenido compatible, reglas, hashes, autores y licencias. Engine rechaza hashes incorrectos, referencias inválidas, rutas externas y tamaños superiores a los límites documentados antes de activar el paquete. No se presupone cobertura por la implementación actual. |
| RF-09 | Guardar y restaurar estados. | El formato identifica contenido, backend, versiones y configuración; es explícito, versionado, validado y sin punteros. La carga prepara una instancia candidata antes de reemplazar la sesión; corrupción, incompatibilidad o error de lectura conservan la sesión anterior. El guardado publica el archivo completo atómicamente y solo entonces comunica éxito. Ante interrupción del proceso, un destino previamente válido conserva el anterior o contiene el nuevo completo; en el primer guardado queda ausente o contiene el nuevo completo. Los errores de E/S y la exclusión de presentación y transacciones pendientes cumplen las reglas de ejecución definidas abajo. |
| RF-10 | Emitir observabilidad estructurada. | Los eventos incluyen código estable, orden e identidad de sesión y frame cuando corresponda. La saturación informa pérdidas sin bloquear la emulación; activar observación conserva los resultados emulados. |
| RF-11 | Detectar el sistema y despachar la implementación por consola. | Se inspecciona estructura y contenido; la extensión es orientativa. El resultado distingue contenido reconocido, desconocido, ambiguo o inválido. El registro crea la implementación Mega Drive disponible; otras consolas no se anuncian como ejecutables. La selección explícita se valida y se registran sistema, backend, versiones y configuración. |
| RF-12 | Integrar Engine y Runtime mediante la API nativa de Tessera. | Engine consume directamente ciclo de vida, entradas, salidas originales, capturas, estados y controles; no requiere libretro ni traducción de observación legacy para esa ruta. La CLI usa la misma API. Se verifican Engine→Tessera y Runtime→Engine→Tessera con capacidades, ownership y vigencia explícitos. |
| RF-13 | Medir el rendimiento por etapas de forma reproducible. | Se registran p50/p95/p99, memoria máxima, asignaciones, bytes copiados por frame, pérdidas, respaldos, regularidad de entrega a presentación y cortes de audio conforme a las definiciones de medición, con calentamiento, repeticiones y configuración identificada. Los costes de integración se comparan con el mismo backend y resultados equivalentes; las diferencias de emulador se informan por separado. No se fijan incrementos de FPS ni umbrales sin una línea base medida. |

La igualdad de vídeo y audio se refiere a las salidas originales del backend.
La presentación compuesta con sustituciones se registra por separado. No se exige
igualdad entre backends, versiones o plataformas diferentes. Las entradas del
juego pueden modificar el estado emulado; la observación y los controles de
presentación no deben hacerlo.

### Reglas de ejecución y aceptación

Estas reglas concretan los requisitos y escenarios anteriores. Son condiciones
de aceptación; no acreditan una implementación ni resultados ya obtenidos.

#### Reinicio y numeración de frames

Reiniciar reconstruye el estado inicial registrado al abrir la sesión, con el
mismo contenido, backend, configuración y datos iniciales de memoria persistente,
cuando correspondan. El éxito crea una época nueva, deja cero frames completados,
entradas neutrales y la sesión pausada antes del primer frame. Descarta entradas
pendientes de la época anterior y devuelve el cursor de replay al inicio. Un
fallo conserva la sesión anterior, incluida su transacción pendiente.

El primer avance completa el frame 1. Un checkpoint en el frame K representa
el estado posterior a completar K; su continuación comienza en K+1. El replay
vuelve a consumir la traza desde su inicio después de un reinicio exitoso.

Los controles nativos de presentación vuelven a su configuración neutra al
reiniciar. Engine conserva el paquete seleccionado y rearma su política mediante
una transacción de la nueva época, con confirmación antes de presentar el frame
correspondiente. Se invalidan las cachés afectadas conforme a RF-07.

#### Entradas y final de la traza

Cada avance recibe el estado completo de los controles configurados para el
próximo frame de la época actual, con una única asignación por puerto y control.
Las asignaciones repetidas se rechazan, aunque sus valores coincidan.

Una traza contiene frames consecutivos y ordenados desde 1 hasta N, sin
duplicados ni huecos, y se valida antes de reproducirla. El rechazo no modifica
la sesión. Un conjunto incompleto o con asignaciones contradictorias también
se rechaza antes de avanzar.

En la API, una entrada destinada a un frame ya iniciado o completado se rechaza
como tardía; una destinada a un frame posterior al próximo esperado se rechaza
por orden incorrecto. Las entradas de una época obsoleta se rechazan. Ningún
rechazo avanza la máquina ni aplica controles pendientes.

Al agotarse la traza se devuelve `END_OF_TRACE` y la sesión permanece pausada,
sin ejecutar otro frame. Las pruebas de CA-02 exigen completar los 3.000 frames
solicitados. Una transacción pendiente solo se aplica al ejecutar un frame
válido o se cancela por una de las operaciones definidas a continuación.

#### Transacciones de presentación y cancelación

Cada sesión admite una transacción pendiente, identificada por sesión, época y
frame objetivo. Se valida íntegramente antes de aceptarla. Mientras exista una
pendiente, otra solicitud se rechaza con `CONTROL_PENDING`, conservando la
anterior y los controles efectivos. La aceptada se aplica completa al comienzo
del próximo frame ejecutado y confirma su identidad, época y frame. Mientras
la sesión permanezca pausada, continúa pendiente. Engine activa la política
preparada al recibir esa confirmación.

Reiniciar, restaurar o cerrar con éxito cancela cualquier transacción pendiente
de la época anterior. La operación informa la identidad y causa de cancelación
como parte de su resultado, sin depender de una cola de eventos saturable.
Una restauración o un reinicio fallidos conservan también la transacción pendiente.

Los checkpoints contienen la máquina y el contexto de replay; excluyen
transacciones pendientes y presentación HD. Exportar un checkpoint no aplica
ni cancela la transacción pendiente de la sesión que continúa ejecutándose.

#### Precedencia gráfica y solapamientos

Para las sustituciones de tiles, un override de ejecución aplicable prevalece
sobre el catálogo; dentro del catálogo gana la primera entrada `[[sub]]` cuyas
condiciones se cumplen, según el orden declarado. Los sprites superpuestos
conservan el orden de la cadena SAT y su prioridad respecto de los planos.
Los demás tipos admitidos identifican el contrato versionado de Engine que
determina su precedencia y desempate.

FX-03 incluye reglas simultáneamente aplicables y recursos superpuestos, con
ganador y píxeles esperados explícitos. El resultado debe repetirse entre
procesos y conservar los hashes originales. Se reutilizan las políticas de
Engine, sin duplicarlas en Tessera ni introducir otro formato de paquetes.

#### Guardado, interrupciones y errores de E/S

Ante interrupción del proceso durante un guardado, si existía un destino válido
queda el archivo anterior o el nuevo completo. En el primer guardado, el destino
queda ausente o contiene el nuevo completo. Nunca se acepta un archivo parcial;
un temporal incompleto no se considera un checkpoint publicado. Esta garantía
se limita a interrupciones del proceso y errores de E/S; la durabilidad ante
pérdida de alimentación queda fuera de esta aceptación.

Los errores de lectura, escritura y publicación devuelven un código estable,
la fase fallida y el diagnóstico del sistema operativo. Se distinguen espacio
insuficiente, acceso denegado y fallo de publicación.

Un error anterior a publicar el archivo conserva intacto el destino previo,
o su ausencia si todavía no existía. Un error de lectura o restauración conserva
íntegramente la sesión activa. El guardado solo comunica éxito después de
publicar el archivo completo. Las pruebas inducen estos fallos y comprueban
archivo destino, resultado devuelto y conservación de la sesión.

#### Oráculos funcionales del backend real

Además del replay determinista, FX-01 y una demo ejecutada por el backend Mega
Drive deben verificar una entrada con efecto conocido y un control sin esa
entrada cuyo resultado esperado sea distinto.

FX-02 incorpora ese estímulo interactivo y resultados esperados de estado
observable, vídeo original y actividad/transiciones de audio. Sus expectativas
se documentan a partir del programa fuente o generador, antes de contrastarlas
con Tessera; no se obtienen exclusivamente de una ejecución del backend
integrado. Los mismos oráculos se verifican mediante la API pública autónoma
y mediante Engine. La ROM copiada se conserva intacta.

#### Definiciones de medición

La regularidad de entrega a presentación se mide mediante los intervalos,
en milisegundos y con reloj monotónico, entre entregas consecutivas de frames
al presentador de Runtime. Se publican p50/p95/p99 y máximo. Los timestamps de
presentación efectiva, cuando estén disponibles, se registran como una serie
separada, identificando su origen.

Un corte de audio es un intervalo continuo durante el cual el dispositivo
solicita muestras y Runtime no dispone de ellas para suministrarlas a tiempo.
Se registran episodios, muestras faltantes y duración total y máxima. Las
ventanas de silencio previstas por el fixture constituyen audio válido.

El informe identifica el punto de medición, calentamiento, repeticiones y
cobertura de cada serie. Los presupuestos de aceptación posteriores se derivan
de la línea base.

### Requisitos de calidad y entrega

- **RC-01 — Stack y trazabilidad:** usar biblioteca estándar; justificar y fijar
  versión y licencia de cada dependencia externa. Cada requisito debe tener
  pruebas identificables; spec, código y tests se mantienen alineados.
- **RC-02 — Independencia:** el núcleo y sus pruebas funcionan sin editor ni
  interfaz gráfica. La incorporación del fixture no selecciona el lenguaje del
  núcleo ni convierte Engine o Lab en dependencias de ejecución de Tessera.
- **RC-03 — Tests:** CI exige compilación, unidad, contratos, replay y regresiones.
  Cada bug corregido incluye su regresión. Los diagnósticos relevantes de memoria
  o comportamiento indefinido hacen fallar la validación cuando estén disponibles.
  Se separan núcleo sin GPU, integración con Engine y presentación en Runtime en
  su entorno declarado; una prueba no ejecutada no se presenta como aprobada.
- **RC-04 — Idioma:** código, comentarios y logs propios nuevos de Tessera en
  inglés; documentación y mensajes al usuario en español; eventos estructurados
  con códigos estables. Las fuentes externas conservan su idioma y avisos, con
  procedencia documentada. Las copias de referencia de fuentes externas se
  mantienen intactas, en su idioma original, con sus avisos y procedencia documentados.
- **RC-05 — Distribución:** el paquete final se construye y ejecuta en un entorno
  limpio con instrucciones versionadas. Incluye fuentes, checksums, matriz de
  soporte, inventario de dependencias, procedencia y avisos correspondientes.

### Entidades principales

- **Sesión:** contenido, backend y configuración identificados; ciclo de vida y época.
- **Traza de entradas:** secuencia reproducible de estados completos de los
  controles configurados, por frame y con final explícito.
- **Sistema y detección:** consola candidata, evidencias y resultado de reconocimiento.
- **Registro e implementación:** relación explícita entre sistema y fábrica del backend.
- **Backend y capacidades:** identidad, versión y operaciones soportadas, distintas de la consola.
- **Captura:** datos coherentes e inmutables de una sesión, época y generación.
- **Recurso gráfico:** elemento identificado dentro de los datos de una captura.
- **Paquete gráfico `.ay`:** manifiesto, assets, reglas, compatibilidad y procedencia, gestionados por Engine.
- **Transacción de presentación:** único grupo pendiente por sesión, identificado
  por época y frame objetivo, aplicado completo o cancelado con resultado explícito.
- **Checkpoint:** estado persistente versionado con identidad, compatibilidad y
  contexto de replay; excluye presentación HD y transacciones pendientes.
- **Diagnóstico:** evento estructurado con código estable y contexto de ejecución.
- **Medición:** muestra y resumen por etapa, configuración y entorno; no forma parte del estado emulado.

## Criterios de éxito

### Resultados medibles

- **CA-01 — Corpus:** disponer del backend sintético, la ROM Mega Drive copiada,
  un fixture con audio activo y un paquete de sustitución gráfica real. Cada uno
  identifica fuentes, versión, hashes, configuración y derechos de uso.
- **CA-02 — Replay:** ejecutar cada demo ejecutable del corpus en dos procesos
  independientes durante 3.000 frames, con cero divergencias de estado emulado,
  vídeo original y audio original en los puntos de comparación definidos.
  Verificar además en FX-01 y una demo del backend real una entrada con efecto
  conocido y un control sin ella con resultado esperado distinto. FX-02 cubre el
  caso real con oráculos de estado observable, vídeo original y actividad y
  transiciones sonoras derivados de su fuente o generador, no exclusivamente
  de una ejecución del backend integrado. Repetirlos por API autónoma y Engine;
  las pruebas exigen completar los 3.000 frames solicitados.
- **CA-03 — Restauración:** guardar en los frames 500, 1.500 y 2.500; restaurar
  cada checkpoint en un proceso nuevo y comparar los 500 frames posteriores con
  la ejecución continua, con cero divergencias de las tres categorías de hashes.
- **CA-04 — Observación:** con observación activa e inactiva y con dos lectores,
  conservar los hashes emulados y originales; ambos lectores reciben capturas
  coherentes. Saturación y cierre producen resultados definidos y observables.
- **CA-05 — Gráficos:** cambiar mediante Engine–Tessera de forma visible al menos
  un recurso de la demo usando el paquete `.ay` real; comparar contra la
  presentación esperada, sin diferencias
  fuera de la modificación solicitada. Conservar los hashes emulados y originales.
  Verificar además rechazo atómico, respaldo e invalidación. FX-03 incluye
  coincidencias simultáneas y recursos superpuestos con ganador y píxeles
  esperados explícitos según la precedencia de Engine, repetibles entre procesos.
- **CA-06 — Audio no vacío:** el fixture sonoro tiene actividad y transiciones
  esperadas en ventanas documentadas. Una salida íntegramente silenciosa falla.
  El audio original conserva sus hashes en replay, observación y restauración.
- **CA-07 — Robustez:** estados y paquetes corruptos, errores de E/S al cargar
  o guardar, guardados interrumpidos incluido el primero, consumidores lentos y
  cierre durante actividad producen los resultados definidos, sin accesos
  inválidos ni cambios parciales en la sesión. Los fallos inducidos de espacio
  insuficiente, acceso denegado y publicación comprueban código, fase, diagnóstico,
  destino y conservación de la sesión conforme a RF-09. Verificar solicitudes
  consecutivas de controles y cancelación de pendientes por reinicio, restauración
  y cierre exitosos, incluso con cola saturada; un reinicio o una restauración
  fallidos conservan la pendiente. Verificar rechazo de entradas duplicadas, contradictorias,
  tardías, fuera de orden y de época obsoleta, además de final de traza explícito.
- **CA-08 — Entrega:** todos los requisitos tienen validación aprobada en CI para
  la matriz declarada; el paquete final se verifica en un entorno limpio y cumple
  RC-01 a RC-05. Los fixtures pendientes están incorporados y sus derechos de
  distribución documentados antes de declarar terminado el MVP.
- **CA-09 — Detección y despacho:** reconocer la ROM canónica por contenido aunque
  cambie su nombre; verificar archivos truncados, extensión falsa, ambigüedad,
  selección explícita incompatible e implementación ausente. Repetir la selección
  conserva su identidad y no modifica una sesión válida ante errores.
- **CA-10 — Integración nativa:** ejecutar FX-01 y el backend real desde Engine
  mediante la API de Tessera; verificar inputs, frame, audio original, capturas,
  controles y estados. Aplicar los mismos oráculos funcionales independientes
  de FX-01 y FX-02 exigidos por CA-02. Ejecutar además la sustitución, el respaldo y la
  presentación mediante Runtime, con versiones y entorno identificados.
- **CA-11 — Línea base de rendimiento:** publicar mediciones por etapa de Tessera
  autónoma y Engine–Tessera, separando la presentación de Runtime; comparar
  condiciones equivalentes tras calentamiento y repeticiones. La regularidad de
  entrega a presentación y los cortes de audio cumplen las definiciones de
  medición: intervalos en milisegundos con p50/p95/p99 y máximo; episodios,
  muestras faltantes y duración total y máxima de cortes. Identificar punto de
  medición, origen y cobertura de las series, separando timestamps de presentación
  efectiva cuando estén disponibles. Una comparación
  con libretro solo se atribuye al cambio de integración si mantiene el mismo
  backend; un emulador diferente se informa como referencia distinta. Los
  presupuestos posteriores se derivan de la base y variación medidas. La medición
  no altera los resultados deterministas y no sustituye los tests de corrección.

Los puntos de comparación, la representación de estado y los límites de formatos
deben documentarse antes de implementar sus pruebas. Los criterios anteriores
son exigencias de aceptación, no resultados ya obtenidos ni garantías heredadas
de Engine, Lab o Genesis Plus GX.

## Supuestos y dependencias

- El usuario aprobó Windows x64, Mega Drive y el alcance de núcleo con consola.
- El [plan aprobado](plan.md) fija C++20, API C nativa, CMake/Ninja/CTest y
  compatibilidad Windows con ABI MSVC para integrar los paquetes de Engine.
  Se registran las versiones exactas del toolchain y de los paquetes consumidos.
- La selección del backend real permanece condicionada a la fase 0 del plan.
  Antes de integrarlo se comparan al menos dos candidatos, con versiones,
  permisos, capacidades y restauración verificadas.
- Engine es consumidor de Tessera y conserva paquetes y composición HD. Sus
  cambios de integración y los de Runtime forman parte del trabajo aprobado;
  guardar esta especificación y el plan no significa que ya estén implementados.
- El backend sintético es una implementación de prueba del contrato; una ROM
  sintética ejecutada en un backend real no lo sustituye.
- La compatibilidad inicial de estados se limita a la matriz expresamente
  soportada; se permite rechazar versiones ajenas sin implementar migraciones.
- El trabajo pendiente descrito abajo no impide planificar, pero sí impide
  aceptar el MVP mientras falten los fixtures o su validación.

### Demo incorporada y estado de evidencia

La demo existente está en
[`tests/fixtures/ayther-engine-synthetic/`](../../tests/fixtures/ayther-engine-synthetic/README.md).
La incorporación conserva las fuentes originales, licencia, procedencia y
`SHA256SUMS`. La ROM `ayther_synthetic.md`, el pack `test_pack.ay` y
`trust.toml` se generan y verifican localmente bajo `build/`, fuera de Git y
de los artefactos publicados, conforme a la decisión del usuario del 2026-10-20.

- La ROM mide 32.768 bytes y tiene SHA-256
  `c7666ec2a4d05ad3df8ce14648b5abd5bc4b0f9711c2edcaad228e127070c34f`.
- Dibuja una celda roja y un sprite verde sobre fondo azul. Alterna datos de
  CRAM y VRAM no visibles; no configura FM ni PSG y no prueba audio activo.
- `test_pack.ay` contiene un manifiesto y `assets/tone.bin`, que es texto de
  prueba. No contiene un reemplazo gráfico ni audio reproducible y no satisface
  FX-03. Se reutilizará el contrato `.ay` de Engine con un fixture real distinto;
  su registro y clave fija actuales son material de prueba.
- La copia de fuentes y dos generaciones de artefactos se comprobaron mediante
  hashes. No hay todavía resultados de ejecución, replay, restauración ni
  sustitución en Tessera, ni validación criptográfica de ese pack.
- Las fuentes copiadas conservan MPL-2.0. Los términos específicos para distribuir
  la ROM y el pack generados deben registrarse expresamente antes de publicarlos,
  según el [registro de procedencia](../../tests/fixtures/ayther-engine-synthetic/PROVENANCE.md).

### Fixtures obligatorios del MVP

| ID | Pendiente | Entregable y condición de aceptación |
| --- | --- | --- |
| FX-01 | Backend sintético de contratos. | Implementación propia con salidas y estado conocidos, perfiles mínimo y completo, capacidades presentes y ausentes y resultados deterministas. Incluye una entrada documentada que modifica un estado conocido y un control sin esa entrada, verificando su consumo efectivo. La ROM copiada no sustituye este backend. |
| FX-02 | Fixture de audio activo y respuesta funcional del backend real. | Demo propia o autorizada, o variante separada de la ROM con procedencia explícita, con fuentes o generador, versión, hashes, entradas y ventanas de actividad sonora esperada. Incluye una entrada con efecto conocido, su control sin ella y resultados esperados de estado observable, vídeo original y actividad/transiciones sonoras derivados de la fuente o generador antes de contrastarlos con Tessera, no exclusivamente de ejecutar el backend integrado. Se verifican por API autónoma y Engine. Debe detectar silencio incorrecto y verificar transiciones; satisface CA-02, CA-03, CA-06 y CA-10. No incluye sustitución sonora; la ROM copiada se conserva intacta. |
| FX-03 | Paquete `.ay` de sustitución gráfica real. | Asset gráfico propio o autorizado, regla asociada a un recurso conocido de la demo, identidad del contenido, manifiesto y metadatos conforme a RF-08 y resultado visual esperado. Incluye reglas simultáneamente aplicables y recursos superpuestos con ganador y píxeles esperados explícitos según la precedencia de Engine, repetibles entre procesos. Se consume mediante Engine–Tessera y se valida también en Runtime. Debe cambiar píxeles previstos sin alterar el estado ni las salidas originales y cubrir rechazo y respaldo de CA-05 y CA-07. El pack de prueba actual no satisface este pendiente. |

Para cada fixture se exige procedencia, licencia, hashes e instrucciones
reproducibles. Los datos adicionales para corrupción, saturación e
incompatibilidad se incorporarán al desarrollar las pruebas correspondientes.
Ningún pendiente se considera aprobado por la mera presencia de archivos o por
comparar silencio, una imagen sin cambio de sustitución o un manifiesto vacío.

**Estado de verificación:** FX-01 pasó replay determinista y consumo desde
Engine; FX-02 pasó oráculos de estado, vídeo y audio activo por API autónoma y
Engine, además de Runtime con dispositivo; FX-03 pasó composición, precedencia,
respaldo y readback en Engine/Runtime y el perfil físico de CI. ROM, pack y
trust generados permanecen locales por la decisión de distribución. La
[matriz de aceptación](acceptance-report.md) conserva los runs y hashes.

### Referencias de elaboración

- [Constitución aprobada](../../docs/constitution.md).
- [Plan técnico aprobado](plan.md).
- [Demo y limitaciones verificadas](../../tests/fixtures/ayther-engine-synthetic/README.md).
- Base inicial consultada en
  `C:\Users\david\Workspaces\gpgx-src\docs\base-conocimiento-comercial`:
  `03_REQUISITOS_NUEVOS.md`, `04_ARQUITECTURA_PROPUESTA.md`,
  `05_CONTRATOS_FUNCIONALES.md`, `06_PRUEBAS_Y_LECCIONES.md`,
  `07_BACKLOG.md` y `08_CONTEXTO_DE_ARRANQUE.md`.
