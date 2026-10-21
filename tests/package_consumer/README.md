# Consumidor del paquete instalado

`package.installed_consumer` instala el build ya compilado en un prefijo exclusivo,
lo mueve y configura un proyecto C11/C++20 separado con `find_package(Tessera 0.1
CONFIG REQUIRED)` y `Tessera::core`. Compila con un solo job y comprueba consultas
ABI públicas, ausencia de dependencias privadas/rutas del checkout, CLI,
metadatos, inventario, avisos/licencias y rechazo de una versión incompatible y del runtime `/MT`.
La prueba solo usa fuentes propias y no instala fixtures. El paquete incluye
las bibliotecas fijadas de ClownMDEmu y sus avisos para el backend Mega Drive;
su revisión de distribución sigue pendiente de T034.

El consumidor debe configurar `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL`
antes de crear sus targets y usar Windows x64 con ABI MSVC. El paquete publica
`Tessera_VERSION`, `Tessera_ABI_MAJOR`, `Tessera_ABI_MINOR`, `Tessera_POINTER_BYTES`,
`Tessera_MSVC_RUNTIME`, `Tessera_DIAGNOSTICS`, `Tessera_BUILD_ID` y la identidad del
compilador. El target estático conserva la interfaz C; esta prueba no acredita una
frontera real de allocators entre DLL.

El paquete ASAN se verifica también: conserva la instrumentación del consumidor,
incluye los binarios del runtime local y requiere el mismo compilador/versión.
`tessera_copy_runtime_dependencies(mi_target)` copia la DLL instalada junto al
ejecutable. No requiere rutas del LLVM productor. Sus metadatos registran origen,
hash y alcance de desarrollo local; la revisión de licencias y distribución de
T034 sigue pendiente. El consumo de Engine de esta fase usa el paquete Release.
