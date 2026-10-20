# Revisión técnica de distribución nativa del MVP

Estado: **abierta** (2026-10-20). Esta revisión cubre los dos candidatos Windows x64 de software: Tessera `42b8870ab8d83744c895f6d5cef5ab3d98b36b5bd74c135c8d0520b1af12b907` y Engine con VPX y proveedor nativo `5bd460bfe713ac5b3b86b9ebf3caef564ee5f9930dd8d33c69d47f2880808c3e`. El usuario autorizó publicar solo esos tipos de paquete una vez cerrada esta revisión y actualizado el lock de Runtime. Los hashes identifican candidatos locales, no assets publicados.

| Comprobación | Evidencia | Estado |
| --- | --- | --- |
| Binarios excluidos | Inventario de 27 y 122 entradas: ninguna ROM, pack, `trust.toml` ni generador compilado. | Conforme |
| Avisos de Tessera | Licencia principal, inventario, lock y avisos coinciden byte a byte con las fuentes; diez textos de licencias de terceros también coinciden. | Conforme |
| Fuente de Tessera | El paquete instala `SOURCE_AVAILABILITY.md` y declara commit `488f8aa` con árbol limpio en `package-metadata.json`; `third_party/backends/sources.lock.json` fija core y gitlinks de Clown, y el parche de procedencia documenta la transformación. El consumidor instalado comprueba la presencia y coherencia del manifiesto. | Verificado en candidato; falta URL del ZIP publicado |
| Fuente de Engine | Código en `Ayther-Dev/AYTHER-Engine` commit `fb3d9a8`; el candidato nativo usa Tessera instalada y VPX. Su `NOTICE.md` actualizado identifica AGPL y la dependencia separada. | Verificado en candidato; falta oferta vinculada desde release y procedencia publicada |
| Licencias de la combinación | Tessera instala bibliotecas estáticas ClownMDEmu, clown68000 y clownz80 bajo AGPL; Engine enlaza `Tessera::core`. El texto de Engine que califica a Tessera únicamente como MIT es incompleto. | Corregir antes de publicar |
| Consumo limpio | Tessera: consumidor C11/C++20 1/1 y replay Release sin divergencias; Engine: 223/223 pruebas del build Release (último checkpoint repetido con corpus compatible), 44/44 payload; Engine y Tessera extraídos: consumidor externo compiló y ejecutó; Runtime: 3/3 pruebas nativas GPU/audio con esos prefijos. | Conforme para candidatos locales; falta CI de los assets publicados |
| Procedencia publicada | No existen todavía assets versionados, manifiesto de checksums, atestación SLSA ni lock Runtime de esta ruta nativa. | Pendiente |
| Dispositivo en CI | GPU y audio pasaron localmente; el usuario facilitará un runner físico, aún sin registrar ni ejecutar. | Pendiente |

La licencia [AGPL-3.0](https://www.gnu.org/licenses/agpl-3.0.en.html) exige ofrecer el código fuente correspondiente al distribuir objeto, incluidos scripts necesarios para generarlo e instalarlo. [MPL-2.0](https://www.mozilla.org/en-US/MPL/2.0/) exige disponibilidad del código cubierto y conserva condiciones de combinación bajo su sección 3.3. Estas referencias fijan las comprobaciones; la compatibilidad y el alcance de la obra combinada deben revisarse sobre el artefacto final, sin presumir que una API C o archivos `.lib` eliminan las obligaciones.

El cierre requiere: corregir el aviso de Engine; vincular en la entrega las fuentes, revisiones y scripts exactos de Tessera, Clown, Engine y dependencias distribuidas; reconstruir los paquetes con metadatos de revisión cerrada; verificar de nuevo payload, hashes, consumo externo y ausencia de fixtures prohibidos; publicar únicamente los paquetes autorizados con checksums y procedencia; fijar sus identidades verificadas en Runtime. Esta revisión no otorga derechos sobre ROM, packs, trust ni generador compilado.
