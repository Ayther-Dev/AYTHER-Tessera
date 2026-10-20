# FX-01 — Backend sintético propio

Fuentes y datos propios bajo MIT, definidos antes de implementar el backend en
[el contrato de modelo y codec](../../../specs/001-mvp-core/contracts/synthetic-codec.md).
No es una ROM ni representa una segunda consola: ambos perfiles son TEST/NONE.

| Perfil | ID / versión | Capacidades |
| --- | --- | --- |
| Mínimo | `synthetic.minimal` / `0.1.0` | `0x100F`: frame, entradas, vídeo, PCM y estado canónico. |
| Completo | `synthetic.complete` / `0.1.0` | `0x1DFF`: añade memoria, paleta, sprites, geometría, eventos, controles y temporal. |

Ninguno anuncia checkpoints en fase 1. El completo explica un modelo de vídeo
atómico; esa completitud temporal no afirma capacidades de un VDP Mega Drive.
La aceptación autónoma corresponde a T014; el consumo desde Engine a T018.

La [CLI](../../../docs/cli.md) genera content.bin, memory.bin, config.bin y
trace.bin/trace.json mediante `demo --fixture-dir DIRECTORIO`. La configuración
y las trazas incorporan el build_id consultado: regenerarlas para cada build.
`integration.replay` acepta el recorrido autónomo con doce procesos, 36.000
frames y oráculo independiente completo. T018 verificó el consumo nativo
desde Engine; los resultados constan en la
[matriz de aceptación](../../../specs/001-mvp-core/acceptance-report.md).

## Datos y generación reproducible

El contenido binario tiene exactamente 24 bytes: ASCII `TSSYN001`, profile U32 LE
(1/2), initial_value U32 LE=7, flags U32 LE=0/1 y reserved U32 LE=0. El bit 0
activa un único fallo de preparación de reset en el arnés externo; no es estado
de máquina. La sesión copia por separado 16 bytes de memoria persistente inicial:

```text
03 14 25 36 47 58 69 7a 8b 9c ad be cf e0 f1 02
```

Estos bytes son `P[j]=(3+17*j) modulo 256`. Los tests generan contenido y memoria
directamente desde esos valores; no requieren descargas, ROMs ni herramientas
externas. Concatenar el magic y los cuatro U32 LE reproduce los artefactos:

| Artefacto | SHA-256 |
| --- | --- |
| Memoria inicial, 16 bytes | `a23d99f2cc2b11f42045500d5073b1128068fff05d23d61a8e90152099e1e647` |
| profile 1, flags 0 | `4e05612f47c7dbb6bfc8002b9f590d5e543ba35c3581df9730f9f6eadbf1c9f9` |
| profile 2, flags 0 | `86a2e612137ba66c41609ff8c5cd4fe44c72cc80779bfdacddd5458ad7f59673` |
| profile 1, flags 1 | `f217d4cd386281d38f4b9bc2bd61daa7ffc219ec1fbcd8026671a6f42a5f1357` |
| profile 2, flags 1 | `525ae2f9a5f436708bd8c2ed5b77b0519670f5e30ff216bd923bbb526a50d994` |

La configuración usa los siete tags comunes, ID/build fijados, sistema NONE,
región 0, PCM 48000 y ocho digitales 0/1 en puerto 0: up/down/left/right/a/b/c/start
con IDs 0..7. Cada control se declara una sola vez, incluidos sus ceros. Se
comprueban identidad, mapa y hash inicial; no hay defaults de locale o JSON.

## Oráculos y trazas

Inicio conocido: A=7, phase=7, P igual a la memoria anterior, frame=tick=0 y entradas
neutrales; los originales aún no están disponibles. Cada frame usa 1000 ticks a
60000/1 Hz, produce 16x16 RGBA8 y 800 pares PCM estéreo S16LE a 48000 Hz.

| Frame 1 | A | P[0] | phase | Píxel (8,8) | Primer par PCM |
| --- | --- | --- | --- | --- | --- |
| Neutral | 8 | 4 | 39 | `(8,16,5,255)` | `(1000,-1000)` |
| B, control 5 | 40 | 4 | 39 | `(40,48,5,255)` | `(1128,-1128)` |

Los tests unitarios evalúan los ocho controles por separado, combinaciones,
wrap U32, copia de buffers y rechazo sin mutación. El oráculo de estado usa sumas
cerradas de prefijos independientes de los helpers del backend. Se contrastan
todos los píxeles y muestras, no solo energía o igualdad entre ejecuciones.

Las trazas `TSTRACE1` tienen estados completos y frames consecutivos 1..N. Control:
todo neutral. Acción larga: B en 301..600 de 3000; corta: B en 21..40 de 60. Tras
3000 frames A es 3007 en control y 12607 en acción; phase es 7 en ambos. El modelo
no usa RNG, reloj del host, filtros ni PCM pendiente.

## Estado y observación

El codec `tessera.synthetic` 1.0 usa el contenedor `TSSTATE1` común. RECORD 65536
contiene exactamente A U32, P BYTES[16] y phase U32, con tags 1/2/3. Son 72 bytes
TLV LE sin padding. Frame/tick/entradas están solo en el contenedor exterior.
Quedan fuera handles, época, métricas, máscaras, pendientes e inyector de fallos.

El completo entrega regiones seleccionadas, ordenadas por kind: memoria P (1),
paleta RGBA de 256 entradas (2), sprite único 1x1 (3), geometría TLV (4) y evidencia
temporal TLV atómica (5). Las direcciones/bank/flags sintéticas son cero; tamaños
de elemento/cantidades son 1/16, 4/256, 8/1, 80/1 y 89/1. Los datos son copias;
leer regiones no modifica la máquina. La retención, eventos, transacciones y
resultados de reset/cierre se prueban mediante la sesión en T010/T012.
