# Autoría del header de la ABI (`ayther_api.h`)

Spec 008, RF-3.0, TP-003.4. Revisado el 2026-11-03.

## Hechos
- El archivo está en el fork de gpgx (`core/ayther/ayther_api.h`), etiqueta `ayther-abi-1.10-r4`.
- El historial de git de ese archivo tiene como único autor a David Lazarte, el responsable de AYTHER. No hereda código de gpgx: es un contrato nuevo, sin cabecera de licencia.
- El Engine lleva una copia del tag `ayther-abi-1.10` (`752a6ff7`).

## Conclusión
El header lo escribió el responsable, así que puede licenciarlo MPL-2.0 para Tessera sin problema de licencia, **siempre que el responsable lo confirme por escrito** y añada una cabecera `SPDX-License-Identifier: MPL-2.0` al copiarlo. Como el responsable consta en el registro de lectura de gpgx (`clean-room-register.md`), el header solo fija la interfaz y no se escribe con ayuda de código de gpgx. Si no se confirma, se redeclaran los layouts desde `ayther_abi_v1.md`. Decisión final en TP-006.1.
