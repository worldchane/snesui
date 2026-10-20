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

# -O3 en todos los núcleos: la emulación es lo que más pesa (a NeoPop le dio 40-75 % más que -O2).
case "$DESTINO" in
  ps4) CC="$LLVM/clang --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -fno-exceptions -O3 -isysroot $TCW -isystem $TCW\\include"
       # C++ como la app (construir.sh): sin excepciones ni RTTI, con la libc++ de OpenOrbis.
       CXX="$LLVM/clang++ --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -fno-exceptions -fno-rtti -fno-threadsafe-statics -D_LIBCPP_DISABLE_EXTERN_TEMPLATE -O3 -isysroot $TCW -isystem $TCW\\include\\c++\\v1 -isystem $TCW\\include -std=gnu++17" ;;
  pc)  CC="$LLVM/clang -O3 -Wno-error=implicit-function-declaration -D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES -I$AQUI/pc/compat_win"
       CXX="$LLVM/clang++ -O3 -fno-exceptions -fno-rtti -std=gnu++17 -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES -I$AQUI/pc/compat_win" ;;
  *) echo "destino desconocido: $DESTINO (ps4 o pc)"; exit 1 ;;
esac
# HAVE_NO_LANGEXTRA: sin las traducciones de las opciones de cada núcleo (miles de punteros a textos que no se usan:
# llevaban la tabla de relocaciones del programa de 900 a 228.000 entradas).
COMUN="-w -include $AQUI/src/nucleos/prefijo.h -D__LIBRETRO__ -DHAVE_NO_LANGEXTRA"

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
  # Si cambiaron las opciones de compilación (las del script, no solo los archivos), se compila todo de nuevo.
  if [ "$(cat "$OBJ/$N/opciones.txt" 2>/dev/null)" != "$CC $CXX $PRE $OPC" ]; then
    rm -f "$OBJ/$N"/*.o
    echo "$CC $CXX $PRE $OPC" > "$OBJ/$N/opciones.txt"
  fi
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
# BUILD_TABLES: las tablas de instrucciones del 68000 (dos de 65.536 punteros) se arman al iniciar en vez de ser
# constantes. Constantes iban a la zona RELRO (1 MB de punteros a corregir al cargar) y la PS4 no abría el programa
# (CE-41839-5; pruebas 6 y 7 del 2026-10-17).
gpgx() {
  D=$EXT/Genesis-Plus-GX
  [ -d "$D" ] || return 0
  DIRS="core core/z80 core/m68k core/ntsc core/sound core/input_hw core/cd_hw core/cart_hw core/cart_hw/svp"
  ARCH="$D/libretro/libretro.c"
  INC="-I$D/libretro -I$LC/include -I$D/libretro/deps/zlib-1.2.11 -I$D/core/sound/minimp3"
  for d in $DIRS; do ARCH="$ARCH $(ls $D/$d/*.c)"; INC="$INC -I$D/$d"; done
  for f in adler32 crc32 inffast inflate inftrees zutil; do ARCH="$ARCH $D/libretro/deps/zlib-1.2.11/$f.c"; done
  OPC="$INC -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -DUSE_16BPP_RENDERING -DFRONTEND_SUPPORTS_RGB565 -DUSE_LIBRETRO_VFS
       -DM68K_OVERCLOCK_SHIFT=20 -DZ80_OVERCLOCK_SHIFT=20 -DHAVE_YM3438_CORE -DHAVE_OPLL_CORE -DMAXROMSIZE=10485760
       -DHAVE_ZLIB -include $AQUI/src/nucleos/incluir/inline.h"
  # Las dos copias del procesador (68000 y el del Sega CD) con BUILD_TABLES (ver arriba); solo esos archivos lo admiten.
  mkdir -p "$OBJ/gpgx/fuente"
  for f in m68kcpu s68kcpu; do
    c="$OBJ/gpgx/fuente/$f.c"
    if [ ! -f "$c" ] || [ "$D/core/m68k/$f.c" -nt "$c" ]; then
      # (upstream BUILD_TABLES declara la tabla de ciclos después de que m68kcpu.h la usa: se declara antes)
      { echo "#define BUILD_TABLES"; echo "static unsigned char m68ki_cycles[0x10000];"; cat "$D/core/m68k/$f.c"; } > "$c"
    fi
    ARCH=$(echo "$ARCH" | sed "s|$D/core/m68k/$f.c|$c|")
  done
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
  # bios_enabled queda en true para siempre una vez cargada una BIOS: un juego europeo después de uno americano (sin la
  # BIOS PAL) arrancaba "con BIOS" sin tenerla y quedaba en negro. Copia que lo apaga antes de cada carga.
  mkdir -p "$OBJ/prosystem/fuente"
  c="$OBJ/prosystem/fuente/libretro.c"
  if [ ! -f "$c" ] || [ "$D/core/libretro.c" -nt "$c" ]; then
    sed 's/^   if (bios_Load(biospath))$/   bios_enabled = false;   \/* ALLESIU: ver nucleos.sh *\/\n   if (bios_Load(biospath))/' "$D/core/libretro.c" > "$c"
    grep -q "ALLESIU: ver nucleos.sh" "$c" || { echo "prosystem: no se pudo corregir libretro.c"; rm -f "$c"; exit 1; }
  fi
  ARCH=$(echo "$ARCH" | sed "s|$D/core/libretro.c|$c|")
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

# Stella 2014 (Atari 2600), licencia GPL 2. En C++. Los pocos archivos con try/catch(...) o throw se compilan como copias
# pasadas por sin_excepciones.sed (en la consola no puede haber excepciones). Usa libretro-common de comun().
stella() {
  D=$EXT/stella2014-libretro
  [ -d "$D" ] || return 0
  C=$D/stella/src
  F=$OBJ/stella/fuente
  mkdir -p "$F"
  ARCH="$D/libretro.cxx $C/emucore/Thumbulator.c $C/common/Base.cxx $C/common/Sound.cxx"
  for f in AtariVox Booster Cart CartARM Cart0840 Cart0FA0 Cart03E0 CartWD Cart2K Cart3E Cart3EX Cart3EPlus Cart3F Cart4A50 \
           Cart4K Cart4KSC CartAR CartBF CartBFSC CartBUS CartCDF CartCM CartCTY CartCV CartDF CartDFSC CartDPC CartDPCPlus \
           CartE0 CartE7 CartEF CartEnhanced CartEFSC CartF0 CartF4 CartF4SC CartF6 CartF6SC CartF8 CartF8SC CartFA CartFA2 \
           CartFC CartFE CartGL CartMC CartMDM CartMVC CartSB CartTVBoy CartUA CartX07 CompuMate Console Control Driving \
           Genesis Joystick Keyboard KidVid M6502 M6532 MD5 MindLink MT24LC256 NullDev Paddles QuadTari Props PropsSet Random \
           SaveKey Serializer Settings StateManager Switches System TIA TIASnd TIATables TrackBall; do
    ARCH="$ARCH $C/emucore/$f.cxx"
  done
  for f in $D/libretro.cxx $C/emucore/CartARM.cxx $C/emucore/CartBUS.cxx $C/emucore/CartCDF.cxx $C/emucore/CartEnhanced.cxx \
           $C/emucore/CartMVC.cxx $C/emucore/Serializer.cxx; do
    c="$F/$(basename "$f")"
    if [ ! -f "$c" ] || [ "$f" -nt "$c" ] || [ "$AQUI/src/nucleos/sin_excepciones.sed" -nt "$c" ]; then
      sed -f "$AQUI/src/nucleos/sin_excepciones.sed" "$f" > "$c"
    fi
    ARCH=$(echo "$ARCH" | sed "s|$f|$c|")
  done
  compilar stella a26 "-I$D -I$D/stella -I$D/stella/stubs -I$C -I$C/emucore -I$C/common -I$C/gui -I$LC/include -I$F" $ARCH
}

# FinalBurn Neo (arcade: CPS1/2/3, Neo Geo, Cave, Toaplan, Sega System 16/18, Konami, IGS PGM...), licencia no comercial
# (ver su LICENSE.md). En C++. La lista de archivos sale de sus Makefiles (herramientas/lista_fbneo.py, sin CHD ni 7z).
# Su decodificador MPEG (mpeg_audio, que usan ymz770 y otros) corta con una excepción al quedarse sin datos: se compilan
# copias donde levanta una marca (limite_superado) y no lee más allá. Usa libretro-common de comun().
fbneo() {
  D=$EXT/FBNeo/src
  [ -d "$D" ] || return 0
  LISTA=$OBJ/fbneo/lista_fuentes.txt
  INC=$OBJ/fbneo/cabeceras.txt
  mkdir -p "$OBJ/fbneo/fuente"
  F=$OBJ/fbneo/fuente
  # La lista, sin los drivers de consolas (con su driverlist.h, en $F: tiene prioridad sobre la original).
  if [ ! -f "$LISTA" ] || [ ! -f "$F/driverlist.h" ] || [ "$AQUI/herramientas/lista_fbneo.py" -nt "$LISTA" ]; then
    python "$AQUI/herramientas/lista_fbneo.py" "$(cygpath -m "$EXT/FBNeo")" "$(cygpath -m "$LISTA")" "$(cygpath -m "$INC")" \
      "$(cygpath -m "$F/driverlist.h")" || exit 1
  fi
  S=$D/burn/snd
  if [ ! -f "$F/mpeg_audio.h" ] || [ "$S/mpeg_audio.h" -nt "$F/mpeg_audio.h" ]; then
    sed -e 's/int current_pos, current_limit;/int current_pos, current_limit; bool limite_superado;/' \
        -e 's/throw limit_hit();/{ limite_superado = true; return 0; }/' "$S/mpeg_audio.h" > "$F/mpeg_audio.h"
    sed -e 's/try {/{ limite_superado = false;/' -e 's/} catch(limit_hit) {/} if (limite_superado) {/' "$S/mpeg_audio.cpp" > "$F/mpeg_audio.cpp"
    cp "$S/ymz770.cpp" "$F/ymz770.cpp"
  fi
  # burn.cpp: BurnLibExit (retro_deinit) libera la copia de los nombres de los juegos pero deja los punteros de los
  # drivers apuntando ahí, y no la marca como liberada: el siguiente retro_init la libera de nuevo y copia nombres de
  # memoria ya liberada (la app se caía al abrir el segundo juego de arcade). La copia se arma una sola vez y queda.
  if [ ! -f "$F/burn.cpp" ] || [ "$D/burn/burn.cpp" -nt "$F/burn.cpp" ]; then
    sed -e 's/^	if (0 == nBurnDrvCount) return;$/	static bool listaHecha = false; if (0 == nBurnDrvCount || listaHecha) return; listaHecha = true;/' \
        -e '/^static void BurnGameListExit()$/{n;s/^{$/{ if (1) return;   \/\/ ALLESIU: ver nucleos.sh/;}' "$D/burn/burn.cpp" > "$F/burn.cpp"
    grep -q "listaHecha = true" "$F/burn.cpp" && grep -q "ALLESIU: ver nucleos.sh" "$F/burn.cpp" || { echo "fbneo: no se pudo corregir burn.cpp"; rm -f "$F/burn.cpp"; exit 1; }
  fi
  # libretro.cpp y retro_input.cpp arman textos con std::ostringstream (macro SSTR): eso trae iostream, locales y el
  # runtime de excepciones de C++ (CE-41839-5). Copias con SSTR_ALLESIU (incluir/sstr_allesiu.h), que concatena sin eso.
  R=$D/burner/libretro
  # En retro_input.cpp además: al configurar los mandos sin un juego andando (el anterior se cerró y el nuevo quedó en la
  # pantalla de error por romset incompleto) refrescaba los controles del driver activo, que no hay: la app se caía.
  for f in libretro retro_input; do
    if [ ! -f "$F/$f.cpp" ] || [ "$R/$f.cpp" -nt "$F/$f.cpp" ] || ! grep -q "ALLESIU" "$F/$f.cpp"; then
      sed -e 's/SSTR(/SSTR_ALLESIU(/g' \
          -e 's/^		if (bControllersSetOnce)$/		if (bControllersSetOnce \&\& nBurnDrvActive != ~0U)   \/\/ ALLESIU: ver nucleos.sh/' "$R/$f.cpp" > "$F/$f.cpp"
    fi
  done
  # Y a libretro.cpp una función para que la app sepa si la carga terminó en su pantalla de error (consola.cpp).
  grep -q "ALLESIU_ErrorFBNeo" "$F/libretro.cpp" || echo 'extern "C" bool arc_ALLESIU_ErrorFBNeo(void) { return gui_show; }   /* ya con el prefijo: ver simbolos_nucleo.py */' >> "$F/libretro.cpp"
  grep -q "ALLESIU: ver nucleos.sh" "$F/retro_input.cpp" || { echo "fbneo: no se pudo corregir retro_input.cpp"; rm -f "$F/retro_input.cpp"; exit 1; }
  # vdc.cpp (video de PC Engine, que usan también algunas placas arcade) lee las DIP del driver de PC Engine, que no va.
  [ -f "$F/pce_dips.cpp" ] || echo "unsigned char PCEDips[4];" > "$F/pce_dips.cpp"
  # dcs2k.cpp (el sonido de las placas de Midway): con el atajo de MAME 2003 para su transformada (la placa Wolf iba
  # lenta: el sonido era el 73 % del tiempo). Ver herramientas/parche_dcs.py.
  if [ ! -f "$F/dcs2k.cpp" ] || [ "$D/burn/drv/midway/dcs2k.cpp" -nt "$F/dcs2k.cpp" ] || [ "$AQUI/herramientas/parche_dcs.py" -nt "$F/dcs2k.cpp" ]; then
    python "$AQUI/herramientas/parche_dcs.py" "$(cygpath -m "$D/burn/drv/midway/dcs2k.cpp")" "$(cygpath -m "$F/dcs2k.cpp")" >/dev/null || { echo "fbneo: no se pudo corregir dcs2k.cpp"; rm -f "$F/dcs2k.cpp"; exit 1; }
  fi
  ARCH=$(sed 's|^C:/ps4dev/externos|'"$EXT"'|' "$LISTA" | sed "s|$S/mpeg_audio.cpp|$F/mpeg_audio.cpp|; s|$S/ymz770.cpp|$F/ymz770.cpp|; s|$R/libretro.cpp|$F/libretro.cpp|; s|$R/retro_input.cpp|$F/retro_input.cpp|; s|$D/burn/burn.cpp|$F/burn.cpp|; s|$D/burn/drv/midway/dcs2k.cpp|$F/dcs2k.cpp|" | tr '\n' ' ')
  ARCH="$ARCH $F/pce_dips.cpp"
  # Lo que en su Makefile va en bloques opcionales (la lista no los trae): su zlib, los sets en .7z y las imágenes CHD
  # (su código de CD las usa siempre; también sirven para Neo Geo CD).
  L=$D/dep/libs
  for f in inflate zutil inffast crc32 adler32 inftrees deflate trees; do ARCH="$ARCH $L/zlib/$f.c"; done
  ARCH="$ARCH $D/burner/un7z.cpp"
  for f in 7zArcIn 7zBuf 7zCrc 7zCrcOpt 7zDec 7zStream Bcj2 Bra Bra86 BraIA64 CpuArch Delta Lzma2Dec LzmaDec Ppmd7 Ppmd7Dec; do
    ARCH="$ARCH $L/lib7z/$f.c"
  done
  ARCH="$ARCH $D/intf/cd/cd_chd.cpp $L/lzma/libchdr_lzma.c $L/zstd/zstddeclib.c"
  for f in chd codec_zlib codec_cdzl bitstream huffman codec_huff codec_flac codec_lzma codec_zstd codec_cdlz codec_cdfl codec_cdzs cdrom; do
    ARCH="$ARCH $L/libchdr/libchdr_$f.c"
  done
  OPC="-I$F -I$D/burn/drv/midway $(sed 's|^C:/ps4dev/externos|'"$EXT"'|; s|^|-I|' "$INC" | grep -v libretro-common | tr '\n' ' ') -I$LC/include
       -I$L/libchdr/include -I$L/lzma -I$L/lzma/include -I$L/zstd -idirafter $D/burner/libretro/libretro-common/include
       -DLSB_FIRST -DUSE_SPEEDHACKS -DHAVE_UNISTD_H -DINCLUDE_7Z_SUPPORT -DZ7_PPMD_SUPPORT -DZ7_ST -DINCLUDE_CHD_SUPPORT
       -DUSE_LIBRETRO_VFS -DCHDR_SYSTEM_ZLIB -Wno-register -Wno-c++11-narrowing -include $AQUI/src/nucleos/incluir/sstr_allesiu.h"
  compilar fbneo arc "$OPC" $ARCH
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
ninguno() { :; }   # para armar la app sin núcleos (prueba de arranque)
comun
comprimir
# Snes9x (SNES), licencia no comercial (ver su LICENSE). Reemplaza al emulador de SNES propio como el de la app (el
# propio queda en src/nucleo como alternativa). En C y C++, preparado para compilarse sin excepciones ni RTTI. Usa
# libretro-common de comun() (STATIC_LINKING: la capa de archivos la pone la app).
snes9x() {
  D=$EXT/snes9x
  [ -d "$D" ] || return 0
  ARCH=""
  for f in c4emu msu1 zipfile srtc obc1 bsflash tile bsx spc7110 fxemu sdd1 seta sa1hw dsp filter/snes_ntsc; do ARCH="$ARCH $D/$f.c"; done
  for f in apu/apu apu/bapu/dsp/sdsp apu/bapu/smp/smp apu/bapu/smp/smp_state cheats2 clip controls cpu cpuexec cpuops \
           crosshairs dma gfx globals loadzip memmap ppu sa1 sa1cpu snapshot sha256 bml fscompat libretro/libretro; do
    ARCH="$ARCH $D/$f.cpp"
  done
  # Su descompresor de zip (rinflate_*, para las roms en .zip), que en libretro va aparte en su libretro-common.
  ARCH="$ARCH $D/libretro/libretro-common/encodings/encoding_deflate.c"
  OPC="-I$D/libretro -I$D -I$D/apu -I$D/apu/bapu -I$LC/include -DALLOW_CPU_OVERCLOCK -DHAVE_STRINGS_H -DSTATIC_LINKING -DSKIP_STDIO_REDEFINES -idirafter $D/libretro/libretro-common/include
       -fno-strict-aliasing"
  [ "$DESTINO" = pc ] && OPC="$OPC -Dstrcasecmp=_stricmp -Dstrncasecmp=_strnicmp"   # Windows no las tiene con ese nombre
  compilar snes9x s9x "$OPC" $ARCH
  # En la PC, los archivos de C piden sin decorar las variables globales de C++ (ver herramientas/alternativos_pc.py).
  if [ "$DESTINO" = pc ]; then
    python "$AQUI/herramientas/alternativos_pc.py" s9x "$(cygpath -m "$OBJ/snes9x/alternativos.txt")" $(cygpath -m -f "$OBJ/snes9x/lista.txt") || exit 1
    cat "$OBJ/snes9x/alternativos.txt" >> "$OBJ/enlazar.rsp.$$"
  fi
}

# Mupen64Plus-Next (N64), licencia GPL 2. En C y C++ (GLideN64). La lista de archivos y las opciones salen de su
# Makefile.common (herramientas/lista_mupen.py): OpenGL ES 2 (el de la PS4, Piglet), el recompilador x86-64 (en la
# consola, con memoria RWX: ver src/app/prueba_jit.cpp), Angrylion por software y el RSP por HLE. Las cabeceras de
# OpenGL ES y EGL son las del toolchain de OpenOrbis (también en la PC: las funciones se piden por nombre en el momento).
n64() {
  D=$EXT/mupen64plus-libretro-nx
  [ -d "$D" ] || return 0
  mkdir -p "$OBJ/n64"
  L=$OBJ/n64/lista_fuentes.txt
  if [ ! -f "$L" ] || [ "$AQUI/herramientas/lista_mupen.py" -nt "$L" ]; then
    python "$AQUI/herramientas/lista_mupen.py" "$(cygpath -m "$D")" "$(cygpath -m "$L")" "$(cygpath -m "$OBJ/n64/nasm.txt")" \
      "$(cygpath -m "$OBJ/n64/opciones_mupen.txt")" || exit 1
  fi
  ARCH=$(sed 's|^C:/ps4dev/externos|'"$EXT"'|' "$L" | tr '\n' ' ')
  # Sin su propia capa de archivos de libretro-common (pedía stat, read, write, rename... al sistema): usa la de la app
  # (archivos.c en comun()). Sin las gz* de zlib ni el acceso a carpetas de GLideN64 (solo para la caché de texturas en
  # alta resolución): en su lugar, src/nucleos/n64 (no tocan el disco). En la PC, la versión de Windows de las carpetas.
  for f in libretro-common/vfs/vfs_implementation.c libretro-common/streams/file_stream.c libretro-common/file/file_path_io.c \
           custom/dependencies/libzlib/gzclose.c custom/dependencies/libzlib/gzlib.c custom/dependencies/libzlib/gzread.c \
           custom/dependencies/libzlib/gzwrite.c; do
    ARCH=$(echo " $ARCH " | sed "s| $D/$f | |")
  done
  ARCH="$ARCH $AQUI/src/nucleos/n64/gz_allesiu.c"
  # glsm.c (la capa de OpenGL de libretro) tiene una tabla fija de 81 MB (program_uniforms, 1024 × 1024): en una copia
  # se pide recién al armar el contexto de OpenGL (solo con el dibujo por GPU). Fija, dejaba a la app sin memoria.
  mkdir -p "$OBJ/n64/fuente"
  G=$OBJ/n64/fuente/glsm.c
  if [ ! -f "$G" ] || [ "$D/libretro-common/glsm/glsm.c" -nt "$G" ]; then
    sed -e 's/^static struct gl_program_uniforms program_uniforms\[MAX_UNIFORMS\]\[MAX_UNIFORMS\];$/static struct gl_program_uniforms (*program_uniforms)[MAX_UNIFORMS];   \/* ALLESIU: ver nucleos.sh *\//'         -e '/^static void glsm_state_setup(void)$/{n;s/^{$/{ if (!program_uniforms) program_uniforms = calloc(MAX_UNIFORMS, sizeof *program_uniforms);/;}'         "$D/libretro-common/glsm/glsm.c" > "$G"
    grep -q "ALLESIU: ver nucleos.sh" "$G" && grep -q "calloc(MAX_UNIFORMS" "$G" || { echo "n64: no se pudo corregir glsm.c"; rm -f "$G"; exit 1; }
  fi
  ARCH=$(echo "$ARCH" | sed "s|$D/libretro-common/glsm/glsm.c|$G|")
  # La máquina de N64 (g_dev, 60 MB) se pide al abrir el núcleo y se suelta al cerrarlo (ver n64/memoria_n64.h): en una
  # copia de libretro.c, retro_init la pide primero y retro_deinit la suelta al final.
  G=$OBJ/n64/fuente/libretro.c
  if [ ! -f "$G" ] || [ "$D/libretro/libretro.c" -nt "$G" ]; then
    sed -e '/^void retro_init(void)$/{n;s/^{$/{ void n64_dispositivo_pedir(void); n64_dispositivo_pedir();   \/* ALLESIU *\//;}' \
        -e '/^void retro_deinit(void)$/,/^}$/{s/^}$/    { void n64_dispositivo_soltar(void); n64_dispositivo_soltar(); }   \/* ALLESIU *\/ }/;}' \
        "$D/libretro/libretro.c" > "$G"
    [ "$(grep -c "n64_dispositivo_" "$G")" = 2 ] || { echo "n64: no se pudo corregir libretro.c"; rm -f "$G"; exit 1; }
  fi
  ARCH=$(echo "$ARCH" | sed "s|$D/libretro/libretro.c|$G|")
  ARCH="$ARCH $AQUI/src/nucleos/n64/memoria_n64.c"
  # Angrylion en varios hilos con los hilos de la app (su parallel_al.cpp usa std::thread, que cierra el programa en la
  # consola): ver n64/parallel_allesiu.cpp.
  ARCH=$(echo " $ARCH " | sed "s| $D/mupen64plus-video-angrylion/parallel_al.cpp | $AQUI/src/nucleos/n64/parallel_allesiu.cpp |")
  [ "$DESTINO" = ps4 ] && ARCH=$(echo "$ARCH" | sed "s|$D/GLideN64/src/osal/osal_files_unix\\.c|$AQUI/src/nucleos/n64/osal_allesiu.c|")
  # Los archivos de C++ con excepciones (try / catch / throw), en copias pasadas por sin_excepciones.sed; la carpeta
  # original va como carpeta de cabeceras (sus #include "..." son relativos a ella).
  F=$OBJ/n64/fuente
  mkdir -p "$F"
  OTRAS=""
  for f in $ARCH; do
    case "$f" in *.cpp) ;; *) continue ;; esac
    grep -qE '\btry\b|\bthrow\b|catch *\(' "$f" || continue
    c="$F/$(basename "$f")"
    if [ ! -f "$c" ] || [ "$f" -nt "$c" ] || [ "$AQUI/src/nucleos/sin_excepciones.sed" -nt "$c" ]; then
      sed -f "$AQUI/src/nucleos/sin_excepciones.sed" "$f" > "$c"
    fi
    ARCH=$(echo "$ARCH" | sed "s|$f|$c|")
    OTRAS="$OTRAS -I$(dirname "$f")"
  done
  OPC="$(sed 's|C:/ps4dev/externos|'"$EXT"'|g' "$OBJ/n64/opciones_mupen.txt") $OTRAS -idirafter $(echo "$TCW" | tr '\' /)/include"
  # En la PC (Windows) faltan cosas de sistemas tipo Unix que el núcleo da por hechas.
  # En la consola, mprotect (lo usa el recompilador para dar permiso de ejecución a su memoria) por la de la app, que la
  # busca en el sistema por nombre (sin sumarla a las funciones que pide el programa para abrir).
  [ "$DESTINO" = ps4 ] && OPC="$OPC -Dmprotect=nucleo_mprotect"
  OPC="$OPC -I$D/libretro -I$D/mupen64plus-video-angrylion"
  # En la consola, la máquina de N64 se pide al abrir el núcleo (ver n64/memoria_n64.h); en la PC queda fija.
  [ "$DESTINO" = ps4 ] && OPC="$OPC -DN64_DISPOSITIVO_DINAMICO -include $AQUI/src/nucleos/n64/memoria_n64.h"
  # En la PC, de las cabeceras del toolchain de la PS4 solo las de OpenGL ES, EGL y KHR (las demás chocan con las de
  # Windows).
  if [ "$DESTINO" = pc ]; then
    G=$OBJ/n64/gl
    TCU=$(echo "$TCW" | tr '\' /)
    [ -d "$G/GLES2" ] || { mkdir -p "$G"; cp -r "$TCU/include/GLES2" "$TCU/include/EGL" "$TCU/include/KHR" "$G/"; }
    OPC=$(echo "$OPC" | sed "s|-idirafter $TCU/include|-idirafter $G|")
    OPC="$OPC -DPATH_MAX=4096 -idirafter $AQUI/src/nucleos/incluir/pc -include $AQUI/src/nucleos/incluir/pc/tipos_bsd.h -D_XKEYCHECK_H -DNOMINMAX -DKHRONOS_STATIC"
    ARCH=$(echo "$ARCH" | sed 's|osal_files_unix\.c|osal_files_win32.c|')
  fi
  compilar n64 n64 "$OPC" $ARCH
  # El recompilador x86-64 tiene una parte en ensamblador (NASM). Necesita las posiciones de los campos de sus
  # estructuras (asm_defines.c compilado: los textos "@ASM_DEFINE nombre valor" del objeto; distintas en la PC y en la
  # consola) y los nombres de C con el prefijo del núcleo (renombrar.h pasado a %define).
  NASM=${NASM:-/c/ps4dev/herramientas_ext/nasm/nasm-2.16.03/nasm.exe}
  A=$OBJ/n64/asm
  mkdir -p "$A"
  REN="$AQUI/obj/nucleos/renombrar/n64.h"
  C64=$D/mupen64plus-core
  if [ ! -f "$A/linkage_x64.o" ] || [ "$C64/src/device/r4300/new_dynarec/x64/linkage_x64.asm" -nt "$A/linkage_x64.o" ] || [ "$REN" -nt "$A/linkage_x64.o" ]; then
    $CC $COMUN -DNUCLEO=n64_ -include "$REN" $OPC -c -o "$A/asm_defines.o" "$C64/src/asm_defines/asm_defines.c" || exit 1
    python -c "
import re, sys
d = open(sys.argv[1], 'rb').read().decode('latin-1')
l = ['%define {} ({})'.format(n, v) for n, v in re.findall(r'@ASM_DEFINE (offsetof_struct_\w+) (0x[0-9a-fA-F]+)', d)]
open(sys.argv[2], 'w').write(chr(10).join(l) + chr(10))
print('asm_defines:', len(l), 'posiciones')" "$(cygpath -m "$A/asm_defines.o")" "$(cygpath -m "$A/asm_defines_nasm.h")" || exit 1
    python "$AQUI/herramientas/nasm_nombres.py" "$(cygpath -m "$REN")" "$(cygpath -m "$A/renombrar.inc")" || exit 1
    if [ "$DESTINO" = ps4 ]; then FORMATO="-f elf64"; else FORMATO="-f win64 -DWIN64"; fi
    "$NASM" $FORMATO -i"$(cygpath -m "$A")/" -p"$(cygpath -m "$A/renombrar.inc")" \
      -o "$(cygpath -m "$A/linkage_x64.o")" "$(cygpath -m "$C64/src/device/r4300/new_dynarec/x64/linkage_x64.asm")" || { echo "falló: linkage_x64.asm"; exit 1; }
  fi
  cygpath -m "$A/linkage_x64.o" >> "$OBJ/enlazar.rsp.$$"
  # En la PC, las variables de C que GLideN64 (C++) pide con el nombre de C++ (ver herramientas/alternativos_pc.py).
  if [ "$DESTINO" = pc ]; then
    python "$AQUI/herramientas/alternativos_pc.py" n64 "$(cygpath -m "$OBJ/n64/alternativos.txt")" $(cygpath -m -f "$OBJ/n64/lista.txt") || exit 1
    cat "$OBJ/n64/alternativos.txt" >> "$OBJ/enlazar.rsp.$$"
  fi
}

for n in ${*:-gambatte fceumm gpgx mgba neopop prosystem handy}; do $n; done   # stella y fbneo: todavía sin probar (sh nucleos.sh ps4 stella fbneo)   # sameboy: más preciso pero lento en la PS4
# Recién ahora, completas, reemplazan a las anteriores (si corren dos a la vez, ninguna queda a medias ni repetida).
mv -f "$OBJ/enlazar.rsp.$$" "$OBJ/enlazar.rsp"
mv -f "$OBJ/definiciones.txt.$$" "$OBJ/definiciones.txt"
