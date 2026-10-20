# FX-01 — Modelo y codec sintético 1.0

Definido antes de los tests y de la implementación T009. Este backend de
prueba propio MIT no representa una consola. IDs `synthetic.minimal` y
`synthetic.complete`, versión 0.1.0, tipo TEST y sistema NONE.

## Contenido, configuración e inicio

Contenido de 24 bytes: ASCII `TSSYN001`, profile U32 LE (1 mínimo, 2 completo),
initial_value U32 LE, flags U32 LE y reserved U32 LE=0. Solo flags bit 0 es
válido: inyecta un fallo en la primera preparación de reset, antes del commit.
Es un inyector del arnés externo a la máquina; consumir el fallo no cambia
estado lógico, frame, época, originales, replay ni transacción pendiente.

La memoria persistente inicial es una copia de exactamente 16 bytes aportados
por el caller. La configuración exige los siete tags comunes del contrato:
sistema NONE, ID exacto, build_id de 32 bytes, región 0, PCM 48000, mapa de
ocho controles y SHA-256 de esa memoria. Cada control del mapa es RECORD con
tags 1 puerto U32=0, 2 control U32=0..7, 3 tipo U32=1 (digital), 4 mínimo I32=0,
5 máximo I32=1. IDs de controles: up/down/left/right/a/b/c/start = 0..7.
No se admiten tags, bits, rangos ni perfiles desconocidos.

Inicio: A=initial_value, P=memoria inicial, phase=initial_value modulo 256,
frame=tick=0 y ocho entradas neutrales. No hay originales antes del frame 1.
Frecuencia lógica 60000/1 ticks por segundo; cada frame avanza 1000 ticks.

## Transición y salidas independientes

Para el próximo frame f, b_i son las ocho entradas y u=sum(b_i*2^i).
Después de validar íntegramente entradas y controles:

1. A=(A+1+u) modulo 2^32.
2. Para j=0..15, P[j]=(P[j]+1+b_(j modulo 8)) modulo 256.
3. Vídeo: 16x16 RGBA8, filas de 64 bytes, aspect 1/1. Paleta C[x+16*y]=
   ((x+u) modulo 256, (y+(A modulo 256)) modulo 256,
   (P[(x+y) modulo 16]+f) modulo 256, 255).
   Un sprite 1x1 en (u&7,(u>>3)&7) usa C[255] con prioridad superior.
   Los demás píxeles usan C[x+16*y].
4. PCM: 800 frames estéreo a 48000 Hz, S16LE L/R. s=1+(u&15), amp=1000+4*u;
   para n=0..799, q=(phase+n*s) modulo 256, L=amp si q<128 o -amp en otro
   caso, R=-L. Después phase=(phase+800*s) modulo 256.

No hay RNG, filtros, colas de PCM ocultas ni cambios dentro del frame.
El mínimo produce las mismas salidas que el completo, sin exponer observación
ni controles. Ambos exponen frame/entradas/vídeo/PCM/estado canónico. El completo
añade memoria/paleta/sprites/geometría/eventos/controles y evidencia temporal
del modelo atómico. Ninguno anuncia checkpoint en fase 1.

## Estado, observación y eventos

`TSSTATE1` conserva el contenedor común y codec_id UTF8 `tessera.synthetic`,
codec_major=1, codec_minor=0. RECORD 65536 contiene exactamente:

| Tag | Encoding | Campo |
| --- | --- | --- |
| 1 | U32 | A |
| 2 | BYTES[16] | P actual |
| 3 | U32 | phase, 0..255 |

Frame/tick/entradas se guardan solo en el contenedor común. No se serializan
métricas, observadores, máscaras, pendientes, handles ni el inyector externo.
Las trazas usan `TSTRACE1` del contrato, con ocho asignaciones ordenadas por frame.

Regiones del completo: kinds 1 memoria P (16 bytes); 2 paleta RGBA (256x4);
3 sprite (x U16, y U16, palette_index U16=255, priority U16=1); 4 geometría
RECORD (tags 1 width, 2 height, 3 aspect_num, 4 aspect_den, todos U32);
5 temporal RECORD (1 frame U64, 2 tick_begin U64, 3 tick_end U64,
4 atomic_frame BOOL8=1). Las regiones seleccionadas son completas por diseño.

Eventos: códigos lifecycle=1, controls_applied=2, backend_frame=3, capture=4,
loss=5; máscaras de clase son los bits 0..4 del contrato. Los envelopes y lotes
usan el TLV aprobado. Cursores independientes parten de 0; un cursor futuro
se rechaza. Una cola circular de 4096 entradas conserva los eventos más nuevos;
al expulsar un evento se acumulan cantidad/rango fuera de la cola. Causas de
pérdida: QUEUE_OVERFLOW=1, SNAPSHOT_BUDGET=2; bit 31 RANGE_KNOWN señala rango
de secuencias conocido. Cuando no se conoce rango, first=last=0. Las capturas
opcionales no bloquean ni invalidan las salidas originales.

La cantidad de pérdidas acumula incidentes: expulsiones individuales de eventos
y capturas opcionales omitidas por presupuesto. Las causas se combinan por OR;
si una captura no tiene secuencia asignada, se borra RANGE_KNOWN y el rango
acumulado pasa a desconocido (first=last=0). La época identifica el incidente
más reciente. Las secuencias no se reinician al cambiar de época y ningún lector
modifica el cursor de otro lector ni los contadores acumulados.

## Oráculos previos al contraste

Corpus inicial I=7 y P[j]=(3+17*j) modulo 256:
`03 14 25 36 47 58 69 7a 8b 9c ad be cf e0 f1 02`.
Frame 1 neutral: A=8, P[0]=4, phase=39, tick=1000,
píxel (8,8)=(8,16,5,255), primer par PCM=(1000,-1000).
Frame 1 con B: u=32, A=40, P[0]=4, P[5]=90, P[13]=226, phase=39,
píxel (8,8)=(40,48,5,255), primer par PCM=(1128,-1128).
En cada frame sum(L^2)=800*amp^2 y todos los pares satisfacen R=-L.

Trazas de control: todo neutral. Acción: B en frames 301..600 de 3000;
variante corta: frames 21..40 de 60. Todas contienen el estado completo 1..N.
El modelo y estos oráculos preceden a las comparaciones de hashes; la igualdad
de hashes por sí sola no acepta la ejecución. Las pruebas por Engine siguen T018.
