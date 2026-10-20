# Fixture propio de entrada y PSG para Gate 0

El generador original de Tessera está en `tests/backend_probe/main.cpp`.
Emite instrucciones 68000 y datos propios; no incorpora código ni bytes del
fixture de Engine ni de otro generador externo. Su finalidad es evaluar
candidatos reales antes de elegir backend. Este fixture no es arquitectura de
producto ni define el formato de la futura API C.

El código propio del generador, la ROM emitida y las trazas y metadatos originales
se distribuyen bajo la licencia MIT del proyecto, disponible en `LICENSE` en la
raíz del repositorio. Este aviso no modifica las licencias de los emuladores ni
de las dependencias externas utilizadas para construir o ejecutar los probes.

## Generación

Con `TESSERA_BUILD_BACKEND_PROBES=ON`, construir el target
`tessera_backend_probe` y ejecutar:

```text
tessera_backend_probe --generate build/phase0/fixtures
```

La ejecución no descarga nada ni abre un emulador. Escribe una ROM de 32 KiB
`program.md`, cuatro trazas JSON, `expected.json` y `manifest.json`.
El manifiesto registra tamaño y SHA-256 de los otros seis archivos; no contiene
fecha, ruta absoluta ni resultados de candidatos. La prueba
`phase0.fixture_reproducibility` genera en dos directorios y compara todos los
hashes, incluido el propio manifiesto, además de validar cabecera y muestras
de las trazas. Los archivos generados permanecen bajo build.

## Comportamiento declarado antes de ejecutar

La ROM configura un mando de tres botones en el puerto 0, mantiene TH alto y
lee B como bit 4 activo bajo de `0xA10003`. Un bucle continuo actualiza los
oráculos siguientes. Enmascara interrupciones; cualquier excepción marca
`0xDEAD` en la palabra RAM `0xFF0002` y se detiene en un bucle local.

| Oráculo | B sin pulsar | B pulsado |
| --- | --- | --- |
| Byte RAM `0xFF0000` | `0x10` | `0x00` |
| Palabra VRAM en dirección de byte `0xFFFC` | `0x1357` | `0x2468` |
| CRAM, color 0, representación del bus VDP | `0x0E00` (azul) | `0x000E` (rojo) |
| Período del tono 0 de PSG | `0x100` | `0x100` |
| Atenuación del tono 0 de PSG | 15: silencio | 0: tono activo |
| Palabra RAM `0xFF0002` | `0x0000` | `0x0000` |

El PSG recibe los bytes de período `0x80, 0x10` al arrancar. Las otras dos
voces y el ruido permanecen silenciados. La transición usa volumen
`0x9F → 0x90 → 0x9F`; no sustituye audio durante la ejecución.

El perfil inicial exige VRAM/CRAM/VSRAM a cero, comportamiento comprobado en
las fuentes fijadas de ambos candidatos: ares
`ares/md/vdp/vdp.cpp:128–132` y ClownMDEmu `source/vdp.c:411–414`.
La ROM no pretende inicializar hardware arbitrario. Los planos vacíos dejan
visible el color de fondo; el marcador VRAM queda fuera de las tablas
configuradas. El valor de CRAM es un oráculo exacto; los valores RGB finales
se contrastan dentro de cada backend, con su formato y conversión declarados.

## Entradas y ventanas

Cada traza enumera frames consecutivos desde 1 y los ocho controles completos
del único mando configurado. Su JSON es privado del probe de fase 0; la época
se asigna al consumirlo en una sesión, no se fija en el archivo.

| Traza de acción | B pulsado | Muestras de estado | Ventanas centrales |
| --- | --- | --- | --- |
| Corta: 60 frames | 21–40 | 10, 30, 50 | 5–15, 25–35, 45–55 |
| Completa: 3000 frames | 301–600 | 150, 450, 750 | 100–200, 350–550, 650–850 |

Las trazas de control tienen B sin pulsar en todos sus frames. La variante
corta sirve para diagnóstico rápido; la completa queda disponible para replay
y checkpoints. Las ventanas omiten los límites de transición.

Para audio se exige generar muestras incluso en silencio y comprobar señal
variable en la ventana activa frente al control. El análisis debe declarar
escala, tasa, filtros, remuestreo y tolerancia al transitorio que realmente use
el backend. No se presupone una mezcla canónica de 48 kHz ni igualdad de
muestras entre emuladores.

## Alcance de la evidencia

`expected.json` contiene expectativas derivadas del generador antes del
contraste. Los hashes del manifiesto acreditan archivos reproducibles;
no acreditan emulación correcta, restauración, rendimiento ni pruebas de
integración. Los probes posteriores deben registrar sus resultados por
separado y comparar cada backend consigo mismo.

Este trabajo no cierra FX-02 por API autónoma o Engine, ni FX-03: no contiene
un paquete real de sustitución gráfica. Conserva intacto el fixture previo
de Engine y sus avisos.

