# Datos del PKG del emulador de SNES. TITLE_ID: 4 letras + 5 números. CONTENT_ID: IV0000-<TITLE_ID>_00-<16 letras/números>.
VERSION=01.30
variante() {
  case "$1" in
    app)    TITLE_ID=SNES00001; CONTENT_ID=IV0000-SNES00001_00-SNESPS4000000000; TITULO="ALLESIU";           CARPETAS="comun nucleo app"; DEFS="" ;;
    segura) TITLE_ID=SNES00009; CONTENT_ID=IV0000-SNES00009_00-SNESSEGURA000000; TITULO="ALLESIU (segura)"; CARPETAS="comun nucleo app"; DEFS="-DSIN_LISTADO -DSIN_RED" ;;
    # Solo para la consola del usuario, NUNCA para publicar: lleva sus BIOS (carpeta bios/ del proyecto, fuera de git).
    personal) TITLE_ID=SNES00002; CONTENT_ID=IV0000-SNES00002_00-SNESPERSONAL0000; TITULO="ALLESIU"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1 ;;
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
    *) return 1 ;;
  esac
}
TODAS="app segura"   # las públicas; la personal se arma aparte: sh construir.sh personal
