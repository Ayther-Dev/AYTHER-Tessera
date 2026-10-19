# CLI de verificación nativa

La CLI Windows x64 consume exclusivamente `tessera.h`. No accede a clases del
núcleo, fixtures privados de Gate 0 ni headers internos de sesión/backend.
Su cierre de FX-01 es autónomo; Engine corresponde a T018 y Runtime a fases posteriores.

```powershell
$cli = './build/windows-dev/src/tessera.exe'
& $cli backend list
& $cli demo --backend synthetic.complete --scenario action --frames 60 --observer two-readers --report './report.json' --fixture-dir './demo-fixture'
& $cli detect './demo-fixture/content.bin'
& $cli replay --backend synthetic.complete --content './demo-fixture/content.bin' --config './demo-fixture/config.bin' --initial-memory './demo-fixture/memory.bin' --trace './demo-fixture/trace.json' --frames 60
```

`demo` admite los dos IDs sintéticos, scenario `neutral`/`action` y observer
`off`/`full`/`two-readers`. Defaults: synthetic.complete, action, 60 y off.
Los frames admitidos son 1..65536. La acción B ocurre en 21..40 para cantidades
de hasta 60; en 301..600 para cantidades mayores. Los fixtures de aceptación
usan exactamente 60 o 3000, conforme al [modelo FX-01](../specs/001-mvp-core/contracts/synthetic-codec.md).

La demo comprueba create→pause sin avance→step neutral exactamente uno→reset al
inicio, antes de ligar la traza y medir su replay. Los hashes publicados comienzan
en el estado inicial tras ese reset; sesión/época quedan fuera del estado canónico.
`replay` exige archivos explícitos y no hace ese preflight. `--frames` es opcional
en replay; por defecto exige completar toda la traza. EOF prematuro es un fallo,
aunque los frames anteriores hayan sido correctos. EOF exacto devuelve el código
de API END_OF_TRACE sin otro avance y conserva máquina/cursor/pausa.

`--fixture-dir` escribe content.bin, config.bin, memory.bin, trace.bin y trace.json
antes del primer avance, para reproducir y alterar entradas en pruebas negativas.
No genera ROM ni `.ay`. Los archivos se escriben en el directorio indicado;
volver a usarlo reemplaza esos cinco archivos. Los permisos y hashes de la semilla
propia constan en [FX-01](../tests/fixtures/synthetic/README.md).

`backend list` declara IDs, versiones, build_id, tipo, sistema y capacidades.
`detect ARCHIVO [--system mega-drive]` inspecciona bytes mediante la API; no usa
la extensión para elegir un backend. La CLI conserva los rechazos de contenido
desconocido/ambiguo/inválido y selección incompatible.

## Traza JSON o binaria

Un archivo cuyo magic sea `TSTRACE1` se entrega intacto a la API. Alternativamente,
el JSON tiene exactamente estas claves y se transforma en ese encoding canónico:

```json
{
  "trace_version": 1,
  "content_sha256": "64 dígitos hexadecimales del contenido",
  "config_sha256": "64 dígitos hexadecimales de la configuración",
  "length": 1,
  "frames": [
    {"frame": 1, "assignments": [
      {"port": 0, "control": 0, "value": 0},
      {"port": 0, "control": 1, "value": 0},
      {"port": 0, "control": 2, "value": 0},
      {"port": 0, "control": 3, "value": 0},
      {"port": 0, "control": 4, "value": 0},
      {"port": 0, "control": 5, "value": 0},
      {"port": 0, "control": 6, "value": 0},
      {"port": 0, "control": 7, "value": 0}
    ]}
  ]
}
```

Los hashes del ejemplo se sustituyen por los reales del contenido/configuración.
Se rechazan claves repetidas/desconocidas, floats, campos fuera de rango, huecos,
orden incorrecto, asignaciones duplicadas e incompletas. No se ordena, deduplica
ni corrige una traza inválida. Se limita el archivo y encoding a 64 MiB, su longitud
a 65536 frames y la profundidad JSON a 32. Toda traza pasa `trace_validate` y
`bind_trace` antes de ejecutar el primer frame; contenido/configuración deben coincidir.

## Informes y medición

stdout contiene un único JSON final, tanto en éxito como en error. stderr contiene
el diagnóstico humano en español. `--report`, disponible en demo/replay, escribe el mismo JSON; la escritura
de informes no afirma la atomicidad reservada a checkpoints de T023.
Exit 0: éxito; 1: fallo de datos/API/E/S; 2: uso inválido.

Los informes demo/replay preparados identifican API, backend/build, contenido, configuración, traza,
PID y creación real del proceso (FILETIME UTC en unidades de 100 ns desde 1601),
además del hash de estado inicial y cantidad solicitada/completada. `frames`
contiene `{frame,state,video,audio}` para cada frame completado: SHA-256 de
TSSTATE1 exportado, TSVIDEO1 RGBA8 sin padding y TSAUDIO1 S16LE estéreo originales.
No se comparan píxeles compuestos ni audio del dispositivo.
`backend list`, `detect` y los errores tempranos publican los campos disponibles
para esa operación, sin inventar identidades de una sesión aún no preparada.

`full` y `two-readers` activan observación por transacción y verifican su
confirmación. Los dos lectores conservan referencias y cursores independientes;
esta CLI los lee secuencialmente y declara `concurrent=false`. La concurrencia
real de capturas se verifica en T012. `off` no solicita capturas ni eventos.

Los costes básicos registran muestras por proceso en microsegundos de reloj
monotónico: llamada de replay; exportación/encoding/hashing canónico; lecturas de
observación. Los percentiles usan rango más próximo. Se guardan registros simples
en un vector reservado; el JSON se construye al terminar o tras un fallo. No incluyen
setup, preflight, preparación de metadatos o escritura final. No son umbrales de FPS ni aceptación T032,
y no entran al reloj lógico ni al estado canónico.
