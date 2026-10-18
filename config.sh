# Datos del PKG del emulador de SNES. TITLE_ID: 4 letras + 5 números. CONTENT_ID: IV0000-<TITLE_ID>_00-<16 letras/números>.
VERSION=01.30
variante() {
  case "$1" in
    app)    TITLE_ID=SNES00001; CONTENT_ID=IV0000-SNES00001_00-SNESPS4000000000; TITULO="SNESUI";            CARPETAS="comun nucleo app"; DEFS="" ;;
    segura) TITLE_ID=SNES00009; CONTENT_ID=IV0000-SNES00009_00-SNESSEGURA000000; TITULO="SNESUI (segura)"; CARPETAS="comun nucleo app"; DEFS="-DSIN_LISTADO -DSIN_RED" ;;
    # Solo para la consola del usuario, NUNCA para publicar: lleva sus BIOS (carpeta bios/ del proyecto, fuera de git).
    personal) TITLE_ID=SNES00002; CONTENT_ID=IV0000-SNES00002_00-SNESPERSONAL0000; TITULO="SNESUI"; CARPETAS="comun nucleo app"; DEFS="-DCON_BIOS_INCLUIDAS"; BIOS=1 ;;
    *) return 1 ;;
  esac
}
TODAS="app segura"   # las públicas; la personal se arma aparte: sh construir.sh personal
