# Datos del PKG del emulador de SNES. TITLE_ID: 4 letras + 5 números. CONTENT_ID: IV0000-<TITLE_ID>_00-<16 letras/números>.
VERSION=01.50
# Desde la 1.4: todos los núcleos, con el arcade (FinalBurn Neo), y el guion del enlazador propio, que deja la zona
# RELRO chica (las tablas de los núcleos van a .data: ver herramientas/link_allesiu.x). Sin él la PS4 no abre el programa.
ENVOLVER_MEMORIA="--wrap=malloc --wrap=calloc --wrap=realloc --wrap=free"
NUCLEOS_TODOS="gambatte fceumm gpgx mgba neopop prosystem handy fbneo snes9x"
GUION_ALLESIU="$(cd "$(dirname "$0")/herramientas" && pwd -W)/link_allesiu.x"
variante() {
  case "$1" in
    app)    TITLE_ID=SNES00001; CONTENT_ID=IV0000-SNES00001_00-SNESPS4000000000; TITULO="ALLESIU";           CARPETAS="comun nucleo app"; DEFS="-DMEMORIA_DIRECTA"; NUCLEOS="$NUCLEOS_TODOS"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    segura) TITLE_ID=SNES00009; CONTENT_ID=IV0000-SNES00009_00-SNESSEGURA000000; TITULO="ALLESIU (segura)"; CARPETAS="comun nucleo app"; DEFS="-DSIN_LISTADO -DSIN_RED" ;;
    # Solo para la consola del usuario, NUNCA para publicar: lleva sus BIOS (carpeta bios/ del proyecto, fuera de git).
    personal) TITLE_ID=SNES00002; CONTENT_ID=IV0000-SNES00002_00-SNESPERSONAL0000; TITULO="ALLESIU"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # Pruebas de arranque (2026-10-17: la 1.3 no abría en la consola). Cada una con su título, para instalarlas juntas.
    prueba1) TITLE_ID=SNES00003; CONTENT_ID=IV0000-SNES00003_00-SNESPRUEBA100000; TITULO="ALLESIU prueba 1"; CARPETAS="comun nucleo app"; DEFS=""; NUCLEOS=ninguno ;;
    prueba2) TITLE_ID=SNES00004; CONTENT_ID=IV0000-SNES00004_00-SNESPRUEBA200000; TITULO="ALLESIU prueba 2"; CARPETAS="comun nucleo app"; DEFS=""; NUCLEOS="gambatte fceumm mgba neopop prosystem handy" ;;
    prueba3) TITLE_ID=SNES00005; CONTENT_ID=IV0000-SNES00005_00-SNESPRUEBA300000; TITULO="ALLESIU prueba 3"; CARPETAS="comun nucleo app"; DEFS="-DSIN_INICIAR_NUCLEOS"; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy" ;;
    prueba4) TITLE_ID=SNES00006; CONTENT_ID=IV0000-SNES00006_00-SNESPRUEBA400000; TITULO="ALLESIU prueba 4"; CARPETAS="comun nucleo app"; DEFS=""; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy" ;;
    prueba5) TITLE_ID=SNES00007; CONTENT_ID=IV0000-SNES00007_00-SNESPRUEBA500000; TITULO="ALLESIU prueba 5"; CARPETAS="comun nucleo app"; DEFS="-DPRUEBA_BSS_MB=40"; NUCLEOS=ninguno ;;
    prueba6) TITLE_ID=SNES00008; CONTENT_ID=IV0000-SNES00008_00-SNESPRUEBA600000; TITULO="ALLESIU prueba 6"; CARPETAS="comun nucleo app"; DEFS=""; NUCLEOS=gpgx ;;
    prueba7) TITLE_ID=SNES00010; CONTENT_ID=IV0000-SNES00010_00-SNESPRUEBA700000; TITULO="ALLESIU prueba 7"; CARPETAS="comun nucleo app"; DEFS="-DPRUEBA_BSS_MB=20"; NUCLEOS=ninguno ;;
    prueba8) TITLE_ID=SNES00011; CONTENT_ID=IV0000-SNES00011_00-SNESPRUEBA800000; TITULO="ALLESIU prueba 8"; CARPETAS="comun nucleo app"; DEFS=""; NUCLEOS=gpgx ;;
    prueba9) TITLE_ID=SNES00012; CONTENT_ID=IV0000-SNES00012_00-SNESPRUEBA900000; TITULO="ALLESIU prueba 9"; CARPETAS="comun nucleo app"; DEFS=""; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy"; LDEXTRA="-z norelro" ;;
    # ALLESIU con el arcade (FinalBurn Neo) y sin zona RELRO, con las BIOS del usuario: para probar en su consola.
    prueba10) TITLE_ID=SNES00013; CONTENT_ID=IV0000-SNES00013_00-SNESPRUEBA100010; TITULO="ALLESIU prueba 10"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy fbneo"; LDEXTRA="-z norelro" ;;
    # Pruebas de arranque (2026-10-18): la app que abre más 600.000 reubicaciones (prueba11) o 100 MB de bss (prueba12).
    prueba11) TITLE_ID=SNES00014; CONTENT_ID=IV0000-SNES00014_00-SNESPRUEBA110000; TITULO="ALLESIU prueba 11"; CARPETAS="comun nucleo app"; DEFS="-DPRUEBA_PUNTEROS=600000"; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy" ;;
    prueba12) TITLE_ID=SNES00015; CONTENT_ID=IV0000-SNES00015_00-SNESPRUEBA120000; TITULO="ALLESIU prueba 12"; CARPETAS="comun nucleo app"; DEFS="-DPRUEBA_BSS_MB=100"; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy" ;;
    # prueba13: la app con 17 MB más de código y 8 MB de datos constantes (sin RELRO, como la 10). prueba14: la 10 sin llamar a
    # los constructores de los núcleos al empezar (el arcade no anda, pero muestra si el problema está ahí).
    prueba13) TITLE_ID=SNES00016; CONTENT_ID=IV0000-SNES00016_00-SNESPRUEBA130000; TITULO="ALLESIU prueba 13"; CARPETAS="comun nucleo app"; DEFS="-DPRUEBA_CODIGO_MB=17"; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy"; LDEXTRA="-z norelro" ;;
    prueba14) TITLE_ID=SNES00017; CONTENT_ID=IV0000-SNES00017_00-SNESPRUEBA140000; TITULO="ALLESIU prueba 14"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DSIN_INICIAR_NUCLEOS"; BIOS=1; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy fbneo"; LDEXTRA="-z norelro" ;;
    # prueba15: como la 13 (17 MB más de código y 8 de datos constantes) pero con la zona RELRO de siempre.
    prueba15) TITLE_ID=SNES00018; CONTENT_ID=IV0000-SNES00018_00-SNESPRUEBA150000; TITULO="ALLESIU prueba 15"; CARPETAS="comun nucleo app"; DEFS="-DPRUEBA_CODIGO_MB=17"; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy" ;;
    # prueba16: como la 10 (arcade y BIOS) pero con la zona RELRO chica: las tablas de los núcleos van a .data
    # (herramientas/link_allesiu.x). Es la que debería abrir.
    prueba16) TITLE_ID=SNES00019; CONTENT_ID=IV0000-SNES00019_00-SNESPRUEBA160000; TITULO="ALLESIU prueba 16"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy fbneo"; GUION="$(cd "$AQUI/herramientas" && pwd -W)/link_allesiu.x" ;;
    # prueba17: la 16 con la tabla arcade cargada al empezar, aviso propio si al set le faltan archivos y más registro.
    prueba17) TITLE_ID=SNES00020; CONTENT_ID=IV0000-SNES00020_00-SNESPRUEBA170000; TITULO="ALLESIU prueba 17"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy fbneo"; GUION="$(cd "$AQUI/herramientas" && pwd -W)/link_allesiu.x" ;;
    # prueba18: la 17 con las correcciones de la revisión completa (copias a la consola, BIOS al copiar, 7800 PAL...).
    prueba18) TITLE_ID=SNES00021; CONTENT_ID=IV0000-SNES00021_00-SNESPRUEBA180000; TITULO="ALLESIU prueba 18"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy fbneo"; GUION="$(cd "$AQUI/herramientas" && pwd -W)/link_allesiu.x" ;;
    # prueba19: la 18 con el filtro HD (escalado de bordes estilo xBR).
    prueba19) TITLE_ID=SNES00022; CONTENT_ID=IV0000-SNES00022_00-SNESPRUEBA190000; TITULO="ALLESIU prueba 19"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy fbneo"; GUION="$(cd "$AQUI/herramientas" && pwd -W)/link_allesiu.x" ;;
    # prueba20: la 18 con el filtro HD y la música de la biblioteca (R2 cambia de canción).
    prueba20) TITLE_ID=SNES00023; CONTENT_ID=IV0000-SNES00023_00-SNESPRUEBA200000; TITULO="ALLESIU prueba 20"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy fbneo"; GUION="$(cd "$AQUI/herramientas" && pwd -W)/link_allesiu.x" ;;
    # prueba21: la 20 con la música en orden aleatorio (opción en Configuración), R2 de vuelta a bajar página y bloques
    # de audio de 1024 muestras (el sonido se cortaba al buscar juegos).
    prueba21) TITLE_ID=SNES00024; CONTENT_ID=IV0000-SNES00024_00-SNESPRUEBA210000; TITULO="ALLESIU prueba 21"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="gambatte fceumm gpgx mgba neopop prosystem handy fbneo"; GUION="$(cd "$AQUI/herramientas" && pwd -W)/link_allesiu.x" ;;
    # prueba22: la 1.4 personal con la prueba de memoria ejecutable (JIT) para el emulador de N64.
    prueba22) TITLE_ID=SNES00025; CONTENT_ID=IV0000-SNES00025_00-SNESPRUEBA220000; TITULO="ALLESIU prueba 22"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DPRUEBA_JIT"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS"; GUION="$GUION_ALLESIU" ;;
    # prueba23: la 1.4 personal con el núcleo de N64 (Mupen64Plus-Next), para ver qué le falta al enlazar.
    prueba23) TITLE_ID=SNES00026; CONTENT_ID=IV0000-SNES00026_00-SNESPRUEBA230000; TITULO="ALLESIU prueba 23"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DPRUEBA_JIT"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="--why-extract=C:/Users/Notebook/AppData/Local/Temp/n64/porque.txt --error-limit=0" ;;
    # prueba24: la carpeta /data/allesiu (migra la vieja), copias por sistema, arcade por contenido (nombres largos),
    # Snes9x para la SNES y N64 (por software) para probar.
    prueba24) TITLE_ID=SNES00027; CONTENT_ID=IV0000-SNES00027_00-SNESPRUEBA240000; TITULO="ALLESIU prueba 24"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU" ;;
    # prueba25: la 24 sin N64 (para ver si la memoria fija de N64, ~300 MB, es lo que hace fallar los juegos).
    prueba25) TITLE_ID=SNES00028; CONTENT_ID=IV0000-SNES00028_00-SNESPRUEBA250000; TITULO="ALLESIU prueba 25"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS"; GUION="$GUION_ALLESIU" ;;
    # prueba26: la 24 (todas las consolas y N64) con la máquina de N64 pedida al abrirla (no fija) y la mudanza de /data que junta carpetas.
    prueba26) TITLE_ID=SNES00029; CONTENT_ID=IV0000-SNES00029_00-SNESPRUEBA260000; TITULO="ALLESIU prueba 26"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU" ;;
    # prueba27: la 26 con el dibujo de N64 en un solo hilo y el tiempo de cada paso al salir de un juego en el registro.
    prueba27) TITLE_ID=SNES00030; CONTENT_ID=IV0000-SNES00030_00-SNESPRUEBA270000; TITULO="ALLESIU prueba 27"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU" ;;
    # prueba28: la 27 con los bloques grandes de memoria en la memoria directa (memoria_grande.cpp) y la memoria libre en el registro.
    prueba28) TITLE_ID=SNES00031; CONTENT_ID=IV0000-SNES00031_00-SNESPRUEBA280000; TITULO="ALLESIU prueba 28"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba29: la 28 con el RSP CXD4 para N64 (Angrylion no abre con el RSP por HLE) y el aviso de memoria al abrir.
    prueba29) TITLE_ID=SNES00032; CONTENT_ID=IV0000-SNES00032_00-SNESPRUEBA290000; TITULO="ALLESIU prueba 29"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba30: la 29 sin la imagen del juego anterior al abrir otro.
    prueba30) TITLE_ID=SNES00033; CONTENT_ID=IV0000-SNES00033_00-SNESPRUEBA300000; TITULO="ALLESIU prueba 30"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba31: la 30 con las carpetas de Batocera (mudanza), el detector de BIOS y las carátulas copiadas del USB.
    prueba31) TITLE_ID=SNES00034; CONTENT_ID=IV0000-SNES00034_00-SNESPRUEBA310000; TITULO="ALLESIU prueba 31"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba32: la 31 con la descarga de carátulas arreglada (lista entera del servidor: la de NES tiene 13.418) y en el registro.
    prueba32) TITLE_ID=SNES00035; CONTENT_ID=IV0000-SNES00035_00-SNESPRUEBA320000; TITULO="ALLESIU prueba 32"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba33: candidata a la 1.5 (personal, con N64 en pausa): carátulas automáticas y BIOS en cualquier carpeta del USB.
    prueba33) TITLE_ID=SNES00036; CONTENT_ID=IV0000-SNES00036_00-SNESPRUEBA330000; TITULO="ALLESIU prueba 33"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba34: la 33 con 40 estilos de interfaz (L2) y 40 de menú (R2) para elegir.
    prueba34) TITLE_ID=SNES00037; CONTENT_ID=IV0000-SNES00037_00-SNESPRUEBA340000; TITULO="ALLESIU prueba 34"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba35: todo lo de la etapa 1-3 (roms/<consola>, asistente, estilos, trucos, respaldo, bezels...), arcade de Midway
    # más rápido (DCS) y las pruebas para el N64 (memoria ejecutable y GPU con shaders desde texto, -DPRUEBA_JIT).
    prueba35) TITLE_ID=SNES00038; CONTENT_ID=IV0000-SNES00038_00-SNESPRUEBA350000; TITULO="ALLESIU prueba 35"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA -DPRUEBA_JIT"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba36: la 35 con la prueba de la GPU una sola vez (Piglet deja la pantalla en negro; después muestra el resultado guardado).
    prueba36) TITLE_ID=SNES00039; CONTENT_ID=IV0000-SNES00039_00-SNESPRUEBA360000; TITULO="ALLESIU prueba 36"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA -DPRUEBA_JIT"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba37: estilos recortados, por defecto Aero clásico + Clásico + Tecla vidrio, vidrio transparente.
    prueba37) TITLE_ID=SNES00040; CONTENT_ID=IV0000-SNES00040_00-SNESPRUEBA370000; TITULO="ALLESIU prueba 37"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA -DPRUEBA_JIT"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba38: la 37 con vidrio líquido (lente y brillo en el borde, como iOS).
    prueba38) TITLE_ID=SNES00041; CONTENT_ID=IV0000-SNES00041_00-SNESPRUEBA380000; TITULO="ALLESIU prueba 38"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA -DPRUEBA_JIT"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba39: vidrio líquido más rápido (por tramos, fondo agrandado de a tercios), L3 = música, 5 trabajadores, cierre.txt.
    prueba39) TITLE_ID=SNES00042; CONTENT_ID=IV0000-SNES00042_00-SNESPRUEBA390000; TITULO="ALLESIU prueba 39"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA -DPRUEBA_JIT"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba40: la 39 con los candados de malloc de musl activados (__libc.threads_minus_1) y cierre.txt con contexto y pila.
    prueba40) TITLE_ID=SNES00043; CONTENT_ID=IV0000-SNES00043_00-SNESPRUEBA400000; TITULO="ALLESIU prueba 40"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA -DPRUEBA_JIT"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba41: la 40 con el teclado en pantalla según la interfaz (panel) y el estilo de botones (teclas).
    prueba41) TITLE_ID=SNES00044; CONTENT_ID=IV0000-SNES00044_00-SNESPRUEBA410000; TITULO="ALLESIU prueba 41"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA -DPRUEBA_JIT"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba42: la 41 con las pistas de la ventana de carátulas (y de Novedades) dentro de su ventana.
    prueba42) TITLE_ID=SNES00045; CONTENT_ID=IV0000-SNES00045_00-SNESPRUEBA420000; TITULO="ALLESIU prueba 42"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA -DPRUEBA_JIT"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba43: el mismo código que la 1.5 pública, con N64 y las BIOS (para probar lo que se publicaría).
    prueba43) TITLE_ID=SNES00046; CONTENT_ID=IV0000-SNES00046_00-SNESPRUEBA430000; TITULO="ALLESIU prueba 43"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    # prueba44: lo subido el 2026-10-20 (créditos, Super FX en Snes9x, "Preparando las carátulas") y el recompilador de N64
    # (máquina en n64_g_dev, a 256 MB del programa). Desde acá las pruebas se instalan con el título "ALLESIU".
    prueba44) TITLE_ID=SNES00047; CONTENT_ID=IV0000-SNES00047_00-SNESPRUEBA440000; TITULO="ALLESIU"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS -DMEMORIA_DIRECTA"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS n64"; GUION="$GUION_ALLESIU"; LDEXTRA="$ENVOLVER_MEMORIA" ;;
    *) return 1 ;;
  esac
}
TODAS="app segura"   # las públicas; la personal se arma aparte: sh construir.sh personal
