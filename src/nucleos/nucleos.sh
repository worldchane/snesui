#!/bin/sh
# Compila los núcleos libretro (código abierto, en C:\ps4dev\externos) que SNESUI lleva dentro, para la PS4 o para la PC.
# Cada archivo se compila con -include prefijo.h y -DNUCLEO=<prefijo>_: sus funciones retro_* pasan a llamarse
# <prefijo>_retro_* y así varios núcleos conviven en el mismo programa (consola.cpp arma la tabla con api.h).
# Solo recompila los archivos que cambiaron. Al final deja en obj/nucleos/<destino>/enlazar.rsp los objetos a enlazar (con
# rutas de Windows, para pasarlo al enlazador como @enlazar.rsp: son demasiados para la línea de comandos) y en
# definiciones.txt los -DCON_NUCLEO_<X> para la app.
# Uso (desde la carpeta del proyecto): sh src/nucleos/nucleos.sh ps4|pc [núcleos...]   (por defecto: todos los que hay)
set -e
AQUI=$(cd "$(dirname "$0")/../.." && pwd)
EXT=${EXTERNOS:-/c/ps4dev/externos}
DESTINO=${1:-ps4}
shift || true
LLVM=${LLVM_BIN:-/c/ps4dev/llvm/bin}
TCW=${PS4_TOOLCHAIN_WIN:-'C:\ps4dev\oo\OpenOrbis\PS4Toolchain'}
OBJ=$AQUI/obj/nucleos/$DESTINO
mkdir -p "$OBJ"

case "$DESTINO" in
  ps4) CC="$LLVM/clang --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -fno-exceptions -O2 -isysroot $TCW -isystem $TCW\\include"
       # C++ como la app (construir.sh): sin excepciones ni RTTI, con la libc++ de OpenOrbis.
       CXX="$LLVM/clang++ --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -fno-exceptions -fno-rtti -fno-threadsafe-statics -D_LIBCPP_DISABLE_EXTERN_TEMPLATE -O2 -isysroot $TCW -isystem $TCW\\include\\c++\\v1 -isystem $TCW\\include -std=gnu++17" ;;
  pc)  CC="$LLVM/clang -O2 -Wno-error=implicit-function-declaration -D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES -I$AQUI/pc/compat_win"
       CXX="$LLVM/clang++ -O2 -fno-exceptions -fno-rtti -std=gnu++17 -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES -I$AQUI/pc/compat_win" ;;
  *) echo "destino desconocido: $DESTINO (ps4 o pc)"; exit 1 ;;
esac
COMUN="-w -include $AQUI/src/nucleos/prefijo.h -D__LIBRETRO__"

# compilar <núcleo> <prefijo> <opciones> <archivos...>: un .o por archivo en obj/nucleos/<destino>/<núcleo>/. Lo
# compartido (sin prefijo) va sin prefijo.h.
#
# Cada núcleo se compila además con obj/nucleos/renombrar/<núcleo>.h, que pasa todos sus nombres globales a
# <prefijo>_nombre: así dos núcleos (o un núcleo y la app) nunca chocan al enlazar. Ese archivo sale de los objetos de la
# PS4 (herramientas/simbolos_nucleo.py): la primera vez se compila sin él, se arma, y se vuelve a compilar con él. Para
# la PC se arma antes con la PS4.
compilar() {
  N=$1; P=$2; OPC=$3; shift 3
  REN="$AQUI/obj/nucleos/renombrar/$N.h"
  if [ -n "$P" ] && [ ! -f "$REN" ] && [ "$DESTINO" = pc ]; then sh "$0" ps4 "$N" >/dev/null || exit 1; fi
  if [ -n "$P" ]; then
    PRE="$COMUN -DNUCLEO=${P}_"
    [ -f "$REN" ] && PRE="$PRE -include $REN"
  else PRE="-w -D__LIBRETRO__"; fi
  mkdir -p "$OBJ/$N"
  # Primera pasada (sin renombrar.h): los objetos viejos pueden estar renombrados; se compila todo de nuevo.
  [ -n "$P" ] && [ ! -f "$REN" ] && rm -f "$OBJ/$N"/*.o
  : > "$OBJ/$N/lista.txt"
  for f in "$@"; do
    o="$OBJ/$N/$(echo "${f#$EXT/}" | tr '/' '_' | sed 's/\.c$/.o/; s/\.cpp$/.o/; s/\.cxx$/.o/; s/\.cc$/.o/')"
    if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ "$AQUI/src/nucleos/prefijo.h" -nt "$o" ] || { [ -f "$REN" ] && [ "$REN" -nt "$o" ]; }; then
      case "$f" in *.c) C="$CC";; *) C="$CXX";; esac
      $C $PRE $OPC -c -o "$o" "$f" || { echo "falló: $f"; exit 1; }
    fi
    echo "$o" >> "$OBJ/$N/lista.txt"
  done
  if [ -n "$P" ] && [ ! -f "$REN" ]; then
    mkdir -p "$(dirname "$REN")"
    cygpath -m -f "$OBJ/$N/lista.txt" > "$OBJ/$N/lista_win.txt"
    python "$AQUI/herramientas/simbolos_nucleo.py" "$P" "$(cygpath -m "$REN")" @"$(cygpath -m "$OBJ/$N/lista_win.txt")" --fuentes="$(cygpath -m "$D")" || exit 1
    compilar "$N" "$P" "$OPC" "$@"   # otra vez, ahora con renombrar.h
    return
  fi
  # En la consola, sin constructores globales: se mueven a la sección nucleo_init, que la app llama al arrancar.
  if [ "$DESTINO" = ps4 ]; then
    cygpath -m -f "$OBJ/$N/lista.txt" > "$OBJ/$N/lista_win.txt"
    python "$AQUI/herramientas/sin_constructores.py" @"$(cygpath -m "$OBJ/$N/lista_win.txt")" || exit 1
  fi
  cygpath -m -f "$OBJ/$N/lista.txt" >> "$OBJ/enlazar.rsp.$$"
  [ -n "$P" ] && echo "-DCON_NUCLEO_$(echo "$P" | tr a-z A-Z)" >> "$OBJ/definiciones.txt.$$"
  echo "== $N: $(wc -l < "$OBJ/$N/lista.txt") archivos"
}

# Lo compartido por todos los núcleos, sin prefijo: la parte de libretro-common que usan (cadenas, rutas, memoria; la
# copia que trae FCEUmm, licencia MIT) y archivos.c, que reemplaza su capa de archivos (ver ese archivo). Así cada núcleo
# no trae su propia copia (chocarían al enlazar).
LC=$EXT/fceumm/src/drivers/libretro/libretro-common
comun() {
  ARCH="$AQUI/src/nucleos/archivos.c"
  for f in streams/memory_stream compat/compat_posix_string compat/compat_snprintf compat/compat_strcasestr compat/compat_strl \
           compat/fopen_utf8 encodings/encoding_utf string/stdstring; do ARCH="$ARCH $LC/$f.c"; done
  compilar comun "" "-I$LC/include" $ARCH
}

# Genesis Plus GX (Mega Drive / Genesis, Master System y Game Gear), licencia no comercial (ver su LICENSE.txt). Sin
# los CD con pistas OGG ni imágenes CHD (no se usan libchdr ni tremor). Usa libretro-common de comun().
gpgx() {
  D=$EXT/Genesis-Plus-GX
  [ -d "$D" ] || return 0
  DIRS="core core/z80 core/m68k core/ntsc core/sound core/input_hw core/cd_hw core/cart_hw core/cart_hw/svp"
  ARCH="$D/libretro/libretro.c"
  INC="-I$D/libretro -I$LC/include -I$D/libretro/deps/zlib-1.2.11 -I$D/core/sound/minimp3"
  for d in $DIRS; do ARCH="$ARCH $(ls $D/$d/*.c)"; INC="$INC -I$D/$d"; done
  for f in adler32 crc32 inffast inflate inftrees zutil; do ARCH="$ARCH $D/libretro/deps/zlib-1.2.11/$f.c"; done
  OPC="$INC -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -DUSE_16BPP_RENDERING -DFRONTEND_SUPPORTS_RGB565 -DUSE_LIBRETRO_VFS
       -DM68K_OVERCLOCK_SHIFT=20 -DZ80_OVERCLOCK_SHIFT=20 -DHAVE_YM3438_CORE -DHAVE_OPLL_CORE -DMAXROMSIZE=33554432
       -DHAVE_ZLIB -include $AQUI/src/nucleos/incluir/inline.h"
  compilar gpgx md "$OPC" $ARCH
}

# mGBA (GBA), licencia MPL 2.0. La lista de archivos sale de su CMake (configurado una vez en externos/mgba-cfg, que
# también genera version.c), sin los que abren archivos o carpetas del sistema (vfs-fd, vfs-dirent, 7zFile, vfs-lzma; para
# su BIOS usa vfs-file, con fopen):
# la rom llega en memoria y la batería se guarda desde la app.
mgba() {
  D=$EXT/mgba
  [ -d "$D" ] && [ -f "$EXT/mgba-cfg/lista.txt" ] || return 0
  ARCH=$(sed 's|^C:/ps4dev/externos|'"$EXT"'|' "$EXT/mgba-cfg/lista.txt" | grep -v "vfs-fd.c\|vfs-dirent.c\|7zFile.c\|vfs-lzma.c\|third-party/lzma/\|renderers/gl.c\|core/directories.c\|core/thread.c" | tr '\n' ' ')
  ARCH="$ARCH $D/src/util/vfs/vfs-file.c"
  OPC="-std=gnu11 -fwrapv -DENABLE_VFS_FILE -DCOLOR_16_BIT -DCOLOR_5_6_5 -DDISABLE_THREADING -DENABLE_VFS -DHAVE_STRDUP -DHAVE_STRNDUP
       -DMGBA_STANDALONE -DMINIMAL_CORE=2 -DM_CORE_GB -DM_CORE_GBA -DM_PI=3.14159265358979323846 -DPATH_MAX=4096 -D_GNU_SOURCE
       -I$EXT/mgba-cfg/include -I$D/src -I$D/include"
  # Los locale de la libc solo existen en la consola (en Windows mGBA usa los suyos).
  [ "$DESTINO" = ps4 ] && OPC="$OPC -DHAVE_LOCALE -DHAVE_STRTOF_L"
  [ "$DESTINO" = pc ] && ARCH=$(echo "$ARCH" | sed "s|platform/posix/memory.c|platform/windows/memory.c|")
  compilar mgba gba "$OPC" $ARCH
}

# Beetle NeoPop (Neo Geo Pocket y Pocket Color), licencia GPL 2. Usa libretro-common de comun().
neopop() {
  D=$EXT/beetle-ngp-libretro
  [ -d "$D" ] || return 0
  M=$D/mednafen; N=$M/ngp
  ARCH="$D/libretro.c $M/state.c $M/settings.c $M/mempatcher.cpp $M/sound/Stereo_Buffer.cpp $M/sound/Blip_Buffer.cpp
        $N/sound.cpp $N/T6W28_Apu.cpp $N/biosHLE.c $N/bios.c $N/flash.c $N/dma.c $N/gfx.c $N/interrupt.c $N/mem.c $N/rom.c
        $N/system.c $N/rtc.c $N/Z80_interface.c $M/hw_cpu/z80-fuse/z80_ops.c $M/hw_cpu/z80-fuse/z80.c"
  for f in interpret interpret_dst interpret_reg interpret_single interpret_src registers; do ARCH="$ARCH $N/TLCS-900h/TLCS900h_$f.c"; done
  OPC="-I$D -I$M -I$M/include -I$M/intl -I$M/hw_sound -I$M/hw_cpu -I$M/hw_misc -I$LC/include -DWANT_16BPP
       -DFRONTEND_SUPPORTS_RGB565 -DNEED_BLIP -DMEDNAFEN_VERSION_NUMERIC=931 -DSTDC_HEADERS -D__STDC_LIMIT_MACROS
       -include $AQUI/src/nucleos/incluir/inline.h"
  compilar neopop ngp "$OPC" $ARCH
}

# ProSystem (Atari 7800), licencia GPL 2. Usa libretro-common de comun().
prosystem() {
  D=$EXT/prosystem-libretro
  [ -d "$D" ] || return 0
  ARCH="$D/core/libretro.c"
  for f in Bios BupChip Cartridge Database Hash Maria Memory Palette Pokey ProSystem Region Riot Sally Tia; do ARCH="$ARCH $D/core/$f.c"; done
  for f in channel coretone music sample; do ARCH="$ARCH $D/bupboop/coretone/$f.c"; done
  # En Windows su boolean.h define bool como unsigned char (truco viejo de MSVC) y choca con stdbool: SN_TARGET_PS3 lo saltea.
  OPC="-I$D/core -I$D/bupboop -I$LC/include"
  [ "$DESTINO" = pc ] && OPC="$OPC -DSN_TARGET_PS3"
  compilar prosystem a78 "$OPC" $ARCH
}

# Handy (Atari Lynx), licencia zlib. En C++ (sin excepciones). Necesita la BIOS lynxboot.img. Usa libretro-common de
# comun().
handy() {
  D=$EXT/libretro-handy
  [ -d "$D" ] || return 0
  ARCH="$D/libretro/libretro.cpp $D/blip/Blip_Buffer.cpp $D/blip/Stereo_Buffer.cpp"
  for f in lynxdec cart memmap mikie ram rom susie system eeprom; do ARCH="$ARCH $D/lynx/$f.cpp"; done
  compilar handy lynx "-I$D/lynx -I$D/libretro -I$D -I$LC/include -DFRONTEND_SUPPORTS_RGB565 -DWANT_CRC32" $ARCH
}

# Gambatte (GB y GBC), licencia GPL 2. En C++ (sin excepciones). Mucho más liviano que SameBoy: en la consola SameBoy no
# llega a 60 cuadros por segundo. Usa libretro-common de comun().
gambatte() {
  D=$EXT/gambatte/libgambatte
  [ -d "$D" ] || return 0
  S=$D/src
  ARCH="$D/libretro/gambatte_log.c $D/libretro/blipper.c $D/libretro/cc_resampler.c $D/libretro/libretro.cpp"
  for f in bootloader cpu gambatte initstate interrupter interruptrequester gambatte-memory sound statesaver tima video \
           video_libretro mem/cartridge mem/cartridge_libretro mem/huc3 mem/memptrs mem/rtc sound/channel1 sound/channel2 \
           sound/channel3 sound/channel4 sound/duty_unit sound/envelope_unit sound/length_counter video/ly_counter \
           video/lyc_irq video/next_m0_time video/ppu video/sprite_mapper; do ARCH="$ARCH $S/$f.cpp"; done
  compilar gambatte gbt "-I$S -I$D/include -I$EXT/gambatte/common -I$EXT/gambatte/common/resample -I$D/libretro -I$LC/include
       -idirafter $D/libretro-common/include
       -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DVIDEO_RGB565" $ARCH
}

# Roms comprimidas (comprimido.c): miniz para .zip (MIT), el LZMA SDK de 7-Zip para .7z (dominio público) y dmc_unrar
# para .rar (GPL 2). Sin funciones de archivos: todo desde memoria.
comprimir() {
  M=$EXT/miniz; S=$EXT/7zip/C; R=$EXT/dmc_unrar
  [ -d "$M" ] && [ -d "$S" ] && [ -d "$R" ] || return 0
  ARCH="$AQUI/src/nucleos/comprimido.c $M/miniz.c $M/miniz_tinfl.c $M/miniz_zip.c $M/miniz_tdef.c $R/dmc_unrar.c"
  for f in 7zAlloc 7zArcIn 7zBuf 7zCrc 7zCrcOpt 7zDec 7zStream Bcj2 Bra Bra86 BraIA64 CpuArch Delta LzmaDec Lzma2Dec Ppmd7 Ppmd7Dec; do
    ARCH="$ARCH $S/$f.c"
  done
  compilar comprimir "" "-I$AQUI/src/nucleos/incluir -I$M -I$S -I$R -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DMINIZ_NO_ARCHIVE_WRITING_APIS
       -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DZ7_PPMD_SUPPORT -DDMC_UNRAR_DISABLE_STDIO=1" $ARCH
}

# SameBoy (GB y GBC), licencia MIT. Las roms de arranque van incluidas (libretro/*_boot.c, libres).
sameboy() {
  D=$EXT/sameboy
  [ -d "$D" ] || return 0
  ARCH="$D/Core/gb.c $D/Core/sgb.c $D/Core/apu.c $D/Core/memory.c $D/Core/mbc.c $D/Core/timing.c $D/Core/display.c
        $D/Core/camera.c $D/Core/sm83_cpu.c $D/Core/joypad.c $D/Core/save_state.c $D/Core/random.c $D/Core/rumble.c
        $D/libretro/agb_boot.c $D/libretro/cgb_boot.c $D/libretro/cgb0_boot.c $D/libretro/mgb_boot.c $D/libretro/dmg_boot.c
        $D/libretro/sgb_boot.c $D/libretro/sgb2_boot.c $D/libretro/libretro.c"
  OPC="-std=gnu11 -D_GNU_SOURCE -DGB_INTERNAL -DGB_VERSION=\"1.0.4\" -DGB_COPYRIGHT_YEAR=\"2025\" -DGB_DISABLE_TIMEKEEPING
       -DGB_DISABLE_REWIND -DGB_DISABLE_DEBUGGER -DGB_DISABLE_CHEATS -I$D"
  [ "$DESTINO" = pc ] && { OPC="$OPC -I$D/Windows"; ARCH="$ARCH $D/Windows/math.c $D/Windows/stdio.c $AQUI/pc/compat_win/vasprintf.c"; }
  # apu.c y random.c traen funciones que corren solas al abrir el programa (constructores globales: en la consola mejor
  # ninguno). Se compilan copias donde son funciones comunes, que consola.cpp llama antes del primer juego de GB.
  mkdir -p "$OBJ/sameboy/fuente"
  for f in apu random; do
    c="$OBJ/sameboy/fuente/$f.c"
    if [ ! -f "$c" ] || [ "$D/Core/$f.c" -nt "$c" ]; then
      sed 's/static void __attribute__((constructor)) \(band_limited_init\|init_seed\)(void)/void SameBoyIniciar_\1(void)/' "$D/Core/$f.c" > "$c"
    fi
    ARCH=$(echo "$ARCH" | sed "s|$D/Core/$f.c|$c|")
  done
  OPC="$OPC -I$D/Core"
  compilar sameboy gb "$OPC" $ARCH
}

# FCEUmm (NES y Famicom Disk System), licencia GPL 2. Usa libretro-common de comun().
fceumm() {
  D=$EXT/fceumm/src
  [ -d "$D" ] || return 0
  ARCH="$(ls $D/boards/*.c $D/input/*.c) $D/drivers/libretro/libretro.c $D/drivers/libretro/libretro_dipswitch.c"
  for f in cart cheat crc32 fceu-endian fceu-memory fceu fds fds_apu file filter general input md5 nsf palette ppu sound \
           state video vsuni ines unif x6502; do ARCH="$ARCH $D/$f.c"; done
  OPC="-DPATH_MAX=1024 -DFCEU_VERSION_NUMERIC=9900 -DFRONTEND_SUPPORTS_RGB565 -DINLINE=inline -DLSB_FIRST -DLOCAL_LE=1
       -I$D/drivers/libretro -I$LC/include -I$D -I$D/input -I$D/boards"
  compilar fceumm nes "$OPC" $ARCH
}

: > "$OBJ/enlazar.rsp.$$"
: > "$OBJ/definiciones.txt.$$"
comun
comprimir
for n in ${*:-gambatte fceumm gpgx mgba neopop prosystem handy}; do $n; done   # sameboy: más preciso pero lento en la PS4
# Recién ahora, completas, reemplazan a las anteriores (si corren dos a la vez, ninguna queda a medias ni repetida).
mv -f "$OBJ/enlazar.rsp.$$" "$OBJ/enlazar.rsp"
mv -f "$OBJ/definiciones.txt.$$" "$OBJ/definiciones.txt"
