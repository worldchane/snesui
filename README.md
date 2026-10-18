# ALLESIU — emulador de consolas clásicas para PS4

![ALLESIU](docs/logo.png)

ALLESIU (antes SNESUI) es un emulador de consolas clásicas para la PS4 con GoldHEN. Empezó como un emulador de SNES
hecho desde cero, y ahora suma NES, Game Boy, Game Boy Color, GBA, Master System, Mega Drive, Game Gear, Neo Geo
Pocket, Neo Geo Pocket Color, Atari 7800, Atari Lynx y **arcade** (Neo Geo, CPS1, CPS2, CPS3 y cientos de placas más),
todo en la misma biblioteca, con interfaz en alta definición, carátulas automáticas, guardado de partidas, filtros de
imagen y música propia.

![Biblioteca](docs/imagen-biblioteca.png)

## Consolas

| Consola | Extensiones | Emulador |
|---|---|---|
| SNES | `.sfc` `.smc` `.fig` `.swc` | propio, hecho desde cero |
| NES / Famicom Disk System | `.nes` `.fds` `.unf` `.unif` | |
| Game Boy / Game Boy Color | `.gb` `.gbc` | |
| Game Boy Advance | `.gba` | |
| Master System | `.sms` | |
| Mega Drive / Genesis | `.md` `.gen` `.smd` `.bin` | |
| Game Gear | `.gg` | |
| Neo Geo Pocket / Color | `.ngp` `.ngc` `.ngpc` | |
| Atari 7800 | `.a78` | |
| Atari Lynx | `.lnx` | |
| Arcade: Neo Geo, CPS1, CPS2, CPS3 y otras placas | `.zip` `.7z` | |

- **ROMs comprimidas:** las ROMs pueden estar sueltas o en `.zip`, `.7z` o `.rar`.
- **Juegos de varias consolas juntos:** ALLESIU busca en todo el USB, incluidas las subcarpetas, y reconoce la consola
  por la extensión. Si un comprimido está en una carpeta con el nombre de la consola (por ejemplo `GBA`, `roms-gba` o
  `Game Boy Advance`), la búsqueda es más rápida porque no hace falta abrirlo.
- **Velocidad:** todos los emuladores corren a la velocidad de la consola original; la SNES va sin recompilador (JIT).

### Arcade

- Cada juego es un zip con el nombre corto del set (el que usa FinalBurn Neo). ALLESIU lo reconoce solo, muestra su
  título completo y lo ordena en su pestaña: **Neo Geo**, **CPS1**, **CPS2**, **CPS3** o **Arcade** (las demás placas).
- Los sets tienen que ser de la versión de FinalBurn Neo que lleva ALLESIU. Antes de abrir un juego se revisan sus
  archivos: si falta alguno, si es de otra versión o si falta la BIOS, ALLESIU avisa exactamente qué falta.
- Las versiones alternativas (clones) necesitan el zip del juego original en la misma carpeta, y los de Neo Geo, la
  BIOS `neogeo.zip` (en la misma carpeta o en `/data/snes-ps4/bios/`). Al copiar un juego a la consola se copian
  también esos zips.
- `herramientas/reparar_arcade.py` rearma tus sets para esta versión: busca cada archivo por su huella (CRC) en todas
  las carpetas que le indiques y arma zips completos ("non-merged").

## Qué tiene

- **Biblioteca.** Una pestaña por consola (más Todos, Recientes, Favoritos y Configuración), fondo pixel art animado y
  búsqueda por nombre. Los juegos van apareciendo mientras se revisa el USB; la lista se guarda en un índice, así que
  la próxima vez aparece al instante y solo se revisan los archivos nuevos. Hasta 30.000 juegos.
- **Carátulas automáticas.** Se bajan desde la base de RetroArch para cada consola, aunque el nombre del archivo no
  sea exacto. Las ya bajadas, y las que no existen en el servidor, no se vuelven a pedir.
- **Guardado.** La batería del cartucho se guarda sola (en la SNES, `.srm` compatible con snes9x y bsnes) y hay 4
  ranuras de estado rápido por juego, cada una con miniatura.
- **Imagen.** Proporción como la consola original, píxeles cuadrados, píxel exacto o 16:9; filtros suave, nítido,
  nítido suavizado, líneas de TV, CRT y **HD** (escalado de bordes al estilo xBR, que redondea las diagonales de los
  sprites). Configuración general y propia por juego.
- **Música.** Diez canciones originales de ALLESIU, compuestas y sintetizadas por el propio programa al estilo de los
  chips de sonido de las consolas, suenan en orden aleatorio en la biblioteca y se apagan solas al jugar. Se puede
  desactivar en Configuración.
- **Avance rápido y rebobinar.** R2 acelera hasta ×4 y L2 vuelve atrás en el tiempo.
- **Dos jugadores.** Con un segundo mando.
- **Juegos en el USB o en la consola.** Se pueden copiar a la consola o al USB uno por uno, todos juntos o
  automáticamente; siempre se deja 1 GB libre para el sistema y cada copia se verifica.
- **Diagnóstico automático.** Si un juego falla, ALLESIU lo detecta y guarda un informe en el USB, en
  `ALLESIU-diagnostico/`. No incluye datos personales.

### SNES

El emulador de SNES es propio y se compara cuadro por cuadro con bsnes:

- en una colección de 762 juegos, el 90 % se ve idéntico a bsnes durante los primeros 30 segundos y ninguno se cuelga;
- están los 8 modos de video (modo 7, alta resolución, transparencias, ventanas, mosaico, HDMA, entrelazado) y el
  sonido completo con eco;
- chips especiales: **Super FX** (con aceleración ×2 a ×4), **SA-1**, **DSP-1**, **DSP-2**, **ST010**, **Cx4**,
  **S-DD1**, **SPC7110** (con su reloj) y **OBC1**. Los chips que no traen su programa en la rom se emulan con tablas
  calculadas por fórmula: no hace falta ningún archivo de firmware.

Todavía no funcionan los juegos con DSP-3, DSP-4, ST011 y ST018.

![Menú de pausa](docs/imagen-menu.png)

## Requisitos

- PS4 con **GoldHEN**. Está probado en el firmware 9.00.
- Tus propias ROMs. **No se incluye ninguna ROM ni ninguna BIOS.**

### BIOS (opcionales salvo Atari Lynx)

Se copian a `/data/snes-ps4/bios/` en la consola:

| Archivo | Para |
|---|---|
| `lynxboot.img` | Atari Lynx (**obligatoria**) |
| `disksys.rom` | juegos de Famicom Disk System |
| `gba_bios.bin` | GBA (opcional: sin ella se usa una BIOS propia del emulador) |
| `bios_MD.bin`, `bios_U.sms`, `bios_E.sms`, `bios_J.sms`, `bios.gg` | pantallas de arranque de Sega (opcionales) |
| `7800 BIOS (U).rom`, `7800 BIOS (E).rom` | Atari 7800 (opcionales: americana y europea) |
| `neogeo.zip` | arcade Neo Geo (**obligatoria** para esos juegos; también puede ir junto a los juegos) |

## Instalación

1. Descarga el PKG desde la sección [Releases](../../releases).
2. Cópialo a un USB y, en la PS4, instálalo con GoldHEN (*Ajustes de depuración → Instalador de paquetes*).
3. Copia tus ROMs en cualquier parte del USB y abre **ALLESIU**.

## Controles

| En la biblioteca | |
|---|---|
| ✕ | Jugar |
| △ | Actualizar (volver a revisar el USB) |
| □ | Bajar las carátulas que falten |
| OPTIONS | Opciones del juego: copiar a la consola o al USB, borrar, favorito, copiar o borrar todos |
| Panel táctil | Buscar por nombre (teclado en pantalla); ○ quita la búsqueda |
| L1 / R1 | Cambiar de pestaña: Todos, cada consola, Recientes, Favoritos, Configuración |
| L2 / R2 | Subir o bajar una página |
| Stick derecho ← → | Saltar a la letra anterior o siguiente |

| Jugando | |
|---|---|
| ✕ ○ □ △ | Los botones de la consola (en la SNES: B A Y X; en las de dos botones, ✕/□ y ○/△) |
| L1 / R1 | L / R |
| OPTIONS | Start |
| Panel táctil | Select |
| **R2** (mantener) | Avance rápido |
| **L2** (mantener) | Rebobinar |
| **L3 + R3** | Menú de pausa: guardar o cargar estado, proporción, filtro, reiniciar, salir |

**Dos jugadores:** inicia sesión con un segundo usuario de la PS4 en el otro mando; ALLESIU lo detecta solo.

## Dónde guarda las cosas

Todo queda en `/data/snes-ps4/` en la consola (la carpeta conserva el nombre de cuando el proyecto era solo de SNES):

- `juegos/`: los juegos copiados desde el USB;
- `guardados/`: las partidas y los estados rápidos (las consolas que no son la SNES, en `guardados/<consola>/`);
- `caratulas/`: las carátulas bajadas;
- `bios/`: las BIOS;
- `biblioteca.txt`: el índice de la biblioteca;
- `ajustes.txt`: la configuración general.

## Compilar

Se compila en Windows con Git Bash:

- **Toolchain:** [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) v0.5.3 y
  LLVM/clang 18.
- **Emuladores de las otras consolas:** se bajan aparte (ver `src/nucleos/nucleos.sh`, que los compila desde
  `C:\ps4dev\externos`).
- **PKG:** `sh construir.sh` deja los PKG en `salida/`.
- **Herramientas de PC:** `sh pc/compilar.sh` compila el simulador de la interfaz y los probadores.

Hay detalles importantes para que el programa abra en la consola: sin excepciones de C++, sin RTTI, sin constructores
globales y sin tablas constantes grandes de punteros. Están explicados en `construir.sh` y `src/nucleos/nucleos.sh`, y
`herramientas/revisar_elf.py` revisa cada ejecutable antes de probarlo.

## Créditos

Emuladores de otras consolas que usa ALLESIU:

- [FCEUmm](https://github.com/libretro/libretro-fceumm) (NES), del equipo de FCEU/FCE Ultra y libretro.
- [Gambatte](https://github.com/libretro/gambatte-libretro) (Game Boy y Game Boy Color), de Sindre Aamås y libretro.
- [mGBA](https://github.com/mgba-emu/mgba) (Game Boy Advance), de Vicki Pfau.
- [Genesis Plus GX](https://github.com/libretro/Genesis-Plus-GX) (Mega Drive, Master System y Game Gear), de Eke-Eke
  y Charles MacDonald.
- [Beetle NeoPop](https://github.com/libretro/beetle-ngp-libretro) (Neo Geo Pocket), basado en NeoPop de neopop_uk y
  Mednafen.
- [ProSystem](https://github.com/libretro/prosystem-libretro) (Atari 7800), de Greg Stanton.
- [Handy](https://github.com/libretro/libretro-handy) (Atari Lynx), de Keith Wilkins.
- [FinalBurn Neo](https://github.com/finalburnneo/FBNeo) (arcade), del equipo de FinalBurn Neo, basado en FinalBurn
  de Dave y FinalBurn Alpha.
- [miniz](https://github.com/richgel999/miniz), el [LZMA SDK](https://www.7-zip.org/sdk.html) de Igor Pavlov y
  [dmc_unrar](https://github.com/DrMcCoy/dmc_unrar), para las ROMs comprimidas.

Además:

- Carátulas: [libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails) (RetroArch).
- Letra Inter (en `assets/`).
- [stb](https://github.com/nothings/stb), para PNG y letras.
- Documentación del hardware de la SNES: fullsnes (nocash). Comparación de exactitud: snes9x y bsnes. Pruebas de
  hardware de [PeterLemon](https://github.com/PeterLemon/SNES).

ALLESIU no tiene relación con Nintendo, Sega, SNK, Atari ni Capcom. Los nombres de las consolas son marcas de sus dueños. Usa
solo ROMs y BIOS de juegos y consolas que tengas.

Creado por **RudolphGamerBkn**.
