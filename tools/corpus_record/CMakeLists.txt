# TP-004.5: grabador de partidas del corpus. Proyecto aparte (no forma parte de
# la compilación de Tessera): usa el paquete del Engine para abrir gpgx por
# libretro y escribir un .arp desde arranque en frío con un guion de mando.
#
#   cmake -S tools/corpus_record -B build/corpus_record -G Ninja \
#         -DAyther_DIR=C:/ayther/AYTHER-Engine/install/windows-native/lib/cmake/Ayther
cmake_minimum_required(VERSION 3.25)
project(tessera_corpus_record CXX)
find_package(Ayther CONFIG REQUIRED)
add_executable(record_timeline record_timeline.cpp)
target_compile_features(record_timeline PRIVATE cxx_std_20)
target_link_libraries(record_timeline PRIVATE Ayther::engine)
