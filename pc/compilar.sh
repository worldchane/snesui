#!/bin/sh
# Compila las herramientas de PC (Windows, con clang y las librerías de Visual Studio):
#   pc/salida/simular.exe        la interfaz real con un mando programado; guarda capturas PNG en pc/salida/
#   pc/salida/probar_snes.exe    el núcleo solo: <rom> <cuadros> [jugar] → capturas y audio.wav
#   pc/salida/probar_estado.exe  verifica que guardar/cargar estado da exactamente el mismo resultado
# pc/incluir/stb tiene copias de las cabeceras stb (dominio público). Uso, desde la carpeta del proyecto: sh pc/compilar.sh
set -e
AQUI=$(cd "$(dirname "$0")/.." && pwd)
CXX=${CXX:-/c/ps4dev/llvm/bin/clang++}
mkdir -p "$AQUI/pc/salida" "$AQUI/pc/data"
OPC="-std=gnu++20 -O2 -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS -isystem $AQUI/pc/incluir"
NUCLEO="$AQUI/src/nucleo/snes.cpp $AQUI/src/nucleo/cpu.cpp $AQUI/src/nucleo/bus.cpp $AQUI/src/nucleo/ppu.cpp $AQUI/src/nucleo/apu.cpp $AQUI/src/nucleo/rom.cpp"
INTERFAZ="$AQUI/src/comun/lienzo.cpp $AQUI/src/comun/texto.cpp $AQUI/src/comun/estilo.cpp $AQUI/src/app/interfaz.cpp $AQUI/src/app/biblioteca.cpp $AQUI/src/app/caratula.cpp $AQUI/src/app/ajustes.cpp $AQUI/src/app/fondo.cpp $AQUI/src/app/rebobinar.cpp $AQUI/src/app/juego.cpp"
"$CXX" $OPC -DRUTA_ASSETS=\"assets\" -Dfseeko=_fseeki64 -Dftello=_ftelli64 "$AQUI/pc/simular.cpp" "$AQUI/pc/plataforma_pc.cpp" $INTERFAZ $NUCLEO -lws2_32 -o "$AQUI/pc/salida/simular.exe"
"$CXX" $OPC "$AQUI/pc/probar_snes.cpp" $NUCLEO -o "$AQUI/pc/salida/probar_snes.exe"
"$CXX" $OPC "$AQUI/pc/probar_estado.cpp" $NUCLEO -o "$AQUI/pc/salida/probar_estado.exe"
echo "listo: pc/salida/simular.exe, probar_snes.exe, probar_estado.exe"
