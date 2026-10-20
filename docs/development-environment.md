# Entorno local y presupuesto de validación

Fecha de inventario: 2026-10-19. Reproducir desde la raíz con
`./tools/inventory.ps1`; evidencia local en `build/evidence/environment.json`
y `build/evidence/vulkan-summary.txt`. Es un inventario, no aceptación del MVP.

| Recurso | Resultado comprobado |
| --- | --- |
| Sistema | Windows x64, build 26200.9711, DisplayVersion 25H2. El registro informa ProductName «Windows 10 Home»; se conserva ese dato sin inferir otra edición. |
| CPU | Intel Core i7-1280P, 20 procesadores lógicos. |
| Memoria | 15,71 GiB físicos; primera lectura 3,78 GiB disponibles, 75 % de carga. La disponibilidad varía y se registra por ejecución. |
| Disco C | Aproximadamente 164 GiB libres al inicio. |
| Shell | PowerShell 7.5.2 de 32 bits; el entorno Developer Shell selecciona compiladores anfitrión x64 y destino x64. |
| Toolchain | VS Build Tools 2026 18.9.12120.119; MSVC v145 14.51.36231; Windows SDK 10.0.26100.0. |
| Compilador seleccionado | `C:/Program Files/LLVM/bin/clang-cl.exe`, 22.1.6, target `x86_64-pc-windows-msvc`; misma familia ABI que el paquete Engine local. |
| Herramientas | CMake 4.3.3; Ninja 1.13.2; CTest de la distribución CMake. |
| Auditoría manual del codec | Python 3.10.11 (stdlib) y clang 22.1.6 para generar/comprobar campos desde el AST de C. No participan en CMake ni en la ejecución del núcleo. |
| GPU integrada | Intel Iris Xe; Vulkan 1.3.212, driver 101.2079. |
| GPU dedicada | NVIDIA RTX 3060 Laptop; Vulkan 1.4.351, driver 616.56. |
| Vulkan | `vulkaninfo --summary` finaliza con código 0; loader 1.4.350. Advierte una capa OBS 1.3 y búsqueda de manifiestos de capas: quedan en el log original. |

La consulta de memoria usa `GlobalMemoryStatusEx`, sin depender de permisos CIM.
La consulta inicial del registro por todas sus propiedades falló en el host
de 32 bits; se lee el valor de CPU por nombre. No se instalaron herramientas.
La enumeración de GPU no acredita presentación, timestamps efectivos ni audio
de Runtime. Esas comprobaciones siguen pendientes en su fase declarada.

La fase 5 configuró un build local `C:/ayther/AYTHER-Runtime/out/build/tessera-local`
con MSVC v145, CRT `/MD`, Engine instalado en
`C:/ayther/AYTHER-Engine/install/windows-native` y Tessera en
`C:/ayther/AYTHER-Tessera/install/windows-dev`. El probe de enlace y el test
FX-03 sin GPU pasaron; este perfil no sustituye la prueba de presentación
efectiva ni el perfil CI de Runtime. El `install/` preexistente de Runtime
permanece fuera de Git.

El test `integration.robustness` pasó en `windows-dev` y
`windows-diagnostics` (AddressSanitizer), junto con `unit.state_format`,
`integration.state_store` e `integration.checkpoint` en diagnósticos.
La cobertura nueva ejercita entradas incompletas, transacciones consecutivas,
traza truncada, EOF repetido, productor saturado y snapshot retenido tras
cerrar la sesión. Los fallos controlados de E/S siguen cubiertos por
`integration.state_store`. UBSan no está configurado en Windows y no se le
atribuye cobertura.

## Consumo local de Engine y Runtime

- Paquete Engine comprobado mediante enlace: `C:/ayther/AYTHER-Engine/install/windows-release-engine-vpx`,
  SDK de encabezados 0.1.0, targets `Ayther::engine` y `Ayther::core`,
  configuración Release, Core ABI 8 y CRT `/MD`. La llamada fuera de línea
  devuelve versión 0.1.0, igual a sus encabezados.
- Prefijo de sus dependencias: `C:/ayther/AYTHER-Engine/build/windows-release-engine-vpx/vcpkg_installed/x64-windows`.
- El inventario fija SHA-256 de biblioteca, encabezados y configuración CMake.
  No se atribuye el paquete a una release por su nombre de carpeta: su revisión
  de origen no está verificada. `git -C` sobre Engine/Runtime fue denegado en el
  sandbox; los archivos del SDK son legibles. El enlace mínimo se ejecutó con
  `windows-engine-integration`; no acredita todavía Engine→Tessera.
- El paquete anterior `install/windows-native` declara Core ABI 7; se conserva
  como dato del inventario inicial y no se mezcla con el SDK ABI 8 comprobado.
- El lock de Runtime declara Engine `v0.1.0-rc.18`; es la dependencia declarada,
  no prueba de identidad del paquete local ni integración nativa existente.
- Engine exige por workflow `repository-policy`, `rust`, `headless`,
  `sanitizers`, `fuzz-smoke`, `rust-coverage`, `cpp-coverage` y `native`, agregados
  en `Required CI gate`. Runtime declara jobs Windows y Linux. Las protecciones
  remotas y el saldo de facturación no se han consultado ni modificado.

## Política de ejecución

Una compilación activa; arranque con dos trabajos en bootstrap. Para compilación
pesada, bajar a uno si sigue disponible menos de 4 GiB o se observa presión de
memoria. Reutilizar caché verificada y builds incrementales. Dos frentes de
edición como máximo; benchmarks y mediciones de GPU/audio aislados de builds.
Conservar hashes, informes breves y primeras divergencias, sin dumps por frame
por defecto. No detener aplicaciones del usuario para liberar recursos.

Validar primero en local, después los checks obligatorios de CI en los hitos
pertinentes. No crear PR ni publicar tags por tarea: objetivo tres PR coordinados,
uno por repositorio, con adicionales solo si son imprescindibles y justificados
en T035. Esta fase no ejecuta GitHub Actions, no publica releases ni modifica
repositorios consumidores. El workflow de Tessera podrá crecer con las suites
realmente disponibles, sin declarar aprobadas las pruebas todavía no implementadas.
