# Datos del PKG del emulador de SNES. TITLE_ID: 4 letras + 5 números. CONTENT_ID: IV0000-<TITLE_ID>_00-<16 letras/números>.
VERSION=01.00
variante() {
  case "$1" in
    app)    TITLE_ID=SNES00001; CONTENT_ID=IV0000-SNES00001_00-SNESPS4000000000; TITULO="SNESUI";            CARPETAS="comun nucleo app"; DEFS="" ;;
    segura) TITLE_ID=SNES00009; CONTENT_ID=IV0000-SNES00009_00-SNESSEGURA000000; TITULO="SNESUI (segura)"; CARPETAS="comun nucleo app"; DEFS="-DSIN_LISTADO -DSIN_RED" ;;
    *) return 1 ;;
  esac
}
TODAS="app segura"
