// Las consolas que corre SNESUI. La de SNES es el núcleo propio (src/nucleo); las demás son núcleos libretro de código
// abierto enlazados en el programa (ver nucleos/nucleos.sh y nucleo_libretro.cpp). El resto de la app (biblioteca,
// juego, guardado, estados, rebobinado) habla con la consola del juego abierto a través de esta tabla.
#pragma once
#include <stdint.h>

enum Sistema
{
    // El número de cada una se guarda en el índice de la biblioteca: las nuevas van siempre al final.
    SIS_SNES, SIS_NES, SIS_GB, SIS_GBC, SIS_GBA, SIS_NGP, SIS_NGPC, SIS_MD, SIS_A2600, SIS_A7800, SIS_LYNX,
    SIS_SMS, SIS_GG, SIS_ARCADE,
    NUM_SISTEMAS
};

// La imagen más grande que puede dar una consola (los búferes de la app tienen este tamaño).
static const int CONSOLA_MAX_ANCHO = 512, CONSOLA_MAX_ALTO = 480;

struct Consola
{
    int sistema;
    const char* nombre;        // corto, para pestañas y textos: "SNES", "NES", "GB"...
    const char* nombreLargo;   // "Game Boy Color"...
    const char* carpeta;       // carpeta de juegos en el USB y en la consola: "SNES", "NES", "GBC"...
    const char* extensiones;   // separadas por '|', en minúsculas: "sfc|smc"
    const char* caratulas;     // repositorio de libretro-thumbnails con sus carátulas
    float aspecto;             // ancho / alto de la imagen en una tele de la época (la proporción "como la TV")
    bool presente;             // el núcleo está en este programa

    bool (*Cargar)(const char* ruta, char* error);   // error: 160 bytes
    // La rom ya en memoria, sacada del comprimido 'ruta' (su nombre adentro: 'interno'). 'datos' (malloc) queda a cargo
    // de la consola, también si falla.
    bool (*CargarMemoria)(const char* ruta, const char* interno, uint8_t* datos, uint32_t tam, char* error);
    void (*Descargar)();
    void (*Reiniciar)();
    // Un cuadro: la imagen (0xRRGGBB, a lo más CONSOLA_MAX_ANCHO × CONSOLA_MAX_ALTO) con su tamaño, y el sonido del cuadro
    // (estéreo de 16 bits a Frecuencia() Hz). Devuelve cuántas muestras de sonido dejó.
    int (*Cuadro)(uint32_t* imagen, int* ancho, int* alto, int16_t* audio, int maxAudio);
    int (*Frecuencia)();
    // Botones de cada jugador con los bits SB_* de snes.h (son los mismos números que el mando de libretro).
    void (*Mando)(int jugador, uint16_t botones);
    void (*MandoConectado)(int jugador, bool si);
    // Partida guardada del cartucho (batería).
    uint32_t (*SramTam)();
    uint8_t* (*SramDatos)();
    bool (*SramCambiada)();
    // Estados rápidos.
    uint32_t (*EstadoTam)();
    uint32_t (*GuardarEstado)(uint8_t* buf, uint32_t max);
    bool (*CargarEstado)(const uint8_t* buf, uint32_t n);
    // Diagnóstico (texto) y problemas detectados por el núcleo.
    int (*Diagnostico)(char* out, int max);
    bool (*Anomalia)(char* txt, int max);
};

const Consola& ConsolaDe(int sistema);
// La consola del juego abierto (la elige JuegoAbrir; el rebobinado y el diagnóstico hablan con ella).
void ConsolaActivar(int sistema);
const Consola& ConsolaActiva();
// La consola de una rom por su extensión (NULL si no es de ninguna).
const Consola* ConsolaParaRuta(const char* ruta);

// Variante personal (CON_BIOS_INCLUIDAS): copia a /data/snes-ps4/bios las BIOS que vienen en el paquete (/app0/assets/misc) y
// todavía no están. Devuelve cuántas copió.
int CopiarBiosIncluidas();

// Roms comprimidas (.zip, .7z, .rar): se abren sacando a memoria la primera rom de adentro que sea de una consola que el
// programa trae (ver nucleos/comprimido.c).
bool EsComprimido(const char* ruta);   // por la extensión
// La consola de esa rom (-1 si no hay ninguna), con su nombre adentro y su tamaño.
int ConsolaDeComprimido(const char* ruta, char* interno, int max, uint64_t* tam);
// La consola que indica el nombre de alguna carpeta de la ruta ("roms/GBA", "Game Boy Advance", "roms-gba"...), o -1.
// La biblioteca la usa para no abrir cada comprimido de una carpeta que ya dice la consola.
int ConsolaPorCarpeta(const char* carpeta);
// La saca a un búfer nuevo (malloc). NULL si no se pudo (con la explicación en 'error', 160 bytes).
uint8_t* SacarDeComprimido(const char* ruta, char* interno, int max, uint32_t* tam, char* error);
