# Datos del PKG del emulador de SNES. TITLE_ID: 4 letras + 5 números. CONTENT_ID: IV0000-<TITLE_ID>_00-<16 letras/números>.
VERSION=01.40
# Desde la 1.4: todos los núcleos, con el arcade (FinalBurn Neo), y el guion del enlazador propio, que deja la zona
# RELRO chica (las tablas de los núcleos van a .data: ver herramientas/link_allesiu.x). Sin él la PS4 no abre el programa.
NUCLEOS_TODOS="gambatte fceumm gpgx mgba neopop prosystem handy fbneo"
GUION_ALLESIU="$(cd "$(dirname "$0")/herramientas" && pwd -W)/link_allesiu.x"
variante() {
  case "$1" in
    app)    TITLE_ID=SNES00001; CONTENT_ID=IV0000-SNES00001_00-SNESPS4000000000; TITULO="ALLESIU";           CARPETAS="comun nucleo app"; DEFS=""; NUCLEOS="$NUCLEOS_TODOS"; GUION="$GUION_ALLESIU" ;;
    segura) TITLE_ID=SNES00009; CONTENT_ID=IV0000-SNES00009_00-SNESSEGURA000000; TITULO="ALLESIU (segura)"; CARPETAS="comun nucleo app"; DEFS="-DSIN_LISTADO -DSIN_RED" ;;
    # Solo para la consola del usuario, NUNCA para publicar: lleva sus BIOS (carpeta bios/ del proyecto, fuera de git).
    personal) TITLE_ID=SNES00002; CONTENT_ID=IV0000-SNES00002_00-SNESPERSONAL0000; TITULO="ALLESIU"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1; NUCLEOS="$NUCLEOS_TODOS"; GUION="$GUION_ALLESIU" ;;
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
    *) return 1 ;;
  esac
}
TODAS="app segura"   # las públicas; la personal se arma aparte: sh construir.sh personal
