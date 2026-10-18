#!/bin/sh
# Compila y empaqueta como PKG instalable con GoldHEN (Ajustes de depuración → Instalador de paquetes).
# Uso (desde esta carpeta, en Git Bash):
#   sh construir.sh              → todas las variantes de config.sh, en salida/<CONTENT_ID>.pkg
#   sh construir.sh app jit      → solo esas
# Herramientas en C:\ps4dev: OpenOrbis v0.5.3 (LLVM 18) en oo/, clang y lld 18.1.8 en llvm/.
# Ajustes que hacen que el programa ABRA en la consola (FW 9.00 + GoldHEN; ver LEEME.md): sin excepciones, sin RTTI, sin
# estáticos con hilos, los objetos antes que las librerías, y src/comun/sistema_ps4.cpp con lo que la librería de C++
# pediría al sistema. Al final revisa cada eboot: funciones del sistema nuevas y constructores globales.
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
. "$AQUI/config.sh"
# Se pueden cambiar con variables de entorno si el toolchain está en otra carpeta.
TC=${PS4_TOOLCHAIN:-/c/ps4dev/oo/OpenOrbis/PS4Toolchain}
TCW=${PS4_TOOLCHAIN_WIN:-'C:\ps4dev\oo\OpenOrbis\PS4Toolchain'}
export OO_PS4_TOOLCHAIN="$TCW"   # create-fself lo exige
export DOTNET_ROLL_FORWARD=Major  # PkgTool.Core pide .NET 3.0: que use el .NET más nuevo instalado
LLVM=${LLVM_BIN:-/c/ps4dev/llvm/bin}
BIN=$TC/bin/windows
REFERENCIA=

LIBS="-lc -lkernel -lc++ -lSceUserService -lSceVideoOut -lSceAudioOut -lScePad -lSceSysmodule -lSceSystemService -lSDL2"
FLAGS="--target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -fno-exceptions -fno-rtti -fno-threadsafe-statics -D_LIBCPP_DISABLE_EXTERN_TEMPLATE -O2 -isysroot $TCW"
# La firma "piglet" es la del ejemplo oficial de Piglet (OpenOrbis): otro PAID y permisos ampliados (authinfo).
AUTH_PIGLET=000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

construir() {
  BIOS=; NUCLEOS=; LDEXTRA=; GUION=   # GUION: guion del enlazador (vacío = el de OpenOrbis)
  variante "$1" || { echo "variante desconocida: $1 (hay: $TODAS personal)"; exit 1; }
  # Los núcleos libretro de las otras consolas (solo recompila lo que cambió); la app sabe cuáles hay por -DCON_NUCLEO_<X>.
  sh "$AQUI/src/nucleos/nucleos.sh" ps4 $NUCLEOS >/dev/null   # NUCLEOS: solo esos (pruebas); vacío = todos
  NUCLEOS_DEF=$(tr '\n' ' ' < "$AQUI/obj/nucleos/ps4/definiciones.txt")
  OBJ=$AQUI/obj/$1
  PAQ=$AQUI/paquete/$1                 # lo que va dentro del PKG
  mkdir -p "$OBJ" "$PAQ/sce_sys/about" "$PAQ/sce_module" "$AQUI/salida"
  rm -f "$OBJ"/*.o
  # Número de compilación (commit + fecha), para saber en la consola qué versión exacta está instalada.
  COMMIT=$(git -C "$AQUI" rev-parse --short HEAD 2>/dev/null || echo sin-git)
  git -C "$AQUI" diff --quiet 2>/dev/null || COMMIT="$COMMIT+"
  printf '#define COMPILACION "%s %s"\n' "$COMMIT" "$(date +%Y-%m-%d)" > "$OBJ/compilacion.h"
  for c in $CARPETAS; do
    for f in "$AQUI"/src/$c/*.cpp; do
      "$LLVM/clang++" $FLAGS $DEFS -isystem "$TCW\\include\\c++\\v1" -isystem "$TCW\\include" -std=gnu++17 -Wall -Wno-unused-function -I "$OBJ" -DCON_COMPILACION $NUCLEOS_DEF \
        -c -o "$OBJ/${c}_$(basename "${f%.cpp}").o" "$f"
    done
  done
  "$LLVM/ld.lld" -m elf_x86_64 -pie --script "${GUION:-$TCW\\link.x}" --eh-frame-hdr -Map "$OBJ/eboot.map" -o "$OBJ/eboot.elf" "-L$TCW\\lib" "$TCW\\lib\\crt1.o" "$OBJ"/*.o @$(cd "$AQUI/obj/nucleos/ps4" && pwd -W)/enlazar.rsp $LIBS $LDEXTRA
  "$BIN/create-fself.exe" -in "$OBJ/eboot.elf" --out "$OBJ/eboot.oelf" --eboot "$PAQ/eboot.bin" --paid 0x3800000000000011 >/dev/null

  # Archivos fijos del paquete: los módulos y right.sprx que trae el propio OpenOrbis, el ícono (512 × 512) y assets/. Sin pic1.png: la pantalla de
  # carga queda negra; el programa la quita al mostrar el primer cuadro (src/comun/pantalla.cpp).
  cp "$TC/samples/SDL2/sce_module/"*.prx "$PAQ/sce_module/"
  cp "$TC/samples/SDL2/sce_sys/about/right.sprx" "$PAQ/sce_sys/about/"
  cp "$AQUI/icon0.png" "$PAQ/sce_sys/icon0.png"
  rm -rf "$PAQ/assets" "$PAQ/bios"
  cp -r "$AQUI/assets" "$PAQ/assets"   # el programa los lee en /app0/assets
  # Variante personal: las BIOS del usuario en /app0/assets/misc (create-gp4 solo arma sus carpetas fijas: sce_sys,
  # sce_module y assets con audio, fonts, images, misc y videos). La app las copia a /data/snes-ps4/bios al abrir.
  if [ -n "$BIOS" ]; then
    [ -d "$AQUI/bios" ] || { echo "falta la carpeta bios/ con las BIOS"; exit 1; }
    # Sin espacios ni paréntesis en el paquete (la lista de archivos del PKG va separada por espacios): "7800 BIOS (U).rom"
    # queda "7800_BIOS__U_.rom"; la app la copia con su nombre de verdad (consola.cpp, CopiarBiosIncluidas).
    mkdir -p "$PAQ/assets/misc"
    for f in "$AQUI"/bios/*; do cp "$f" "$PAQ/assets/misc/$(basename "$f" | tr ' ()' '___')"; done
  fi
  (
    cd "$PAQ"
    SFO=sce_sys/param.sfo
    P="$BIN/PkgTool.Core.exe"
    "$P" sfo_new $SFO
    "$P" sfo_setentry $SFO APP_TYPE --type Integer --maxsize 4 --value 1
    "$P" sfo_setentry $SFO APP_VER --type Utf8 --maxsize 8 --value "$VERSION"
    "$P" sfo_setentry $SFO ATTRIBUTE --type Integer --maxsize 4 --value 0
    "$P" sfo_setentry $SFO CATEGORY --type Utf8 --maxsize 4 --value gd
    "$P" sfo_setentry $SFO CONTENT_ID --type Utf8 --maxsize 48 --value "$CONTENT_ID"
    "$P" sfo_setentry $SFO DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
    "$P" sfo_setentry $SFO SYSTEM_VER --type Integer --maxsize 4 --value 0
    "$P" sfo_setentry $SFO TITLE --type Utf8 --maxsize 128 --value "$TITULO"
    "$P" sfo_setentry $SFO TITLE_ID --type Utf8 --maxsize 12 --value "$TITLE_ID"
    "$P" sfo_setentry $SFO VERSION --type Utf8 --maxsize 8 --value "$VERSION"
    ARCHIVOS="eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(ls sce_module | sed 's|^|sce_module/|' | tr '\n' ' ') $(find assets -type f | tr '\n' ' ')"
    "$BIN/create-gp4.exe" -out pkg.gp4 --content-id="$CONTENT_ID" --files "$ARCHIVOS" >/dev/null
    "$P" pkg_build pkg.gp4 "$AQUI/salida" >/dev/null
  )
  echo "== $1: salida/$CONTENT_ID.pkg ($(du -k "$AQUI/salida/$CONTENT_ID.pkg" | cut -f1) KB)"
  if [ -f "$REFERENCIA" ]; then python "$AQUI/herramientas/revisar_elf.py" "$OBJ/eboot.elf" "$REFERENCIA"; fi
}

for v in ${*:-$TODAS}; do construir "$v"; done
