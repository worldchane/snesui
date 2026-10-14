// Los juegos de SNES que hay en el USB o en /data.
#pragma once
#include <stdint.h>

struct Juego
{
    char ruta[512];
    char nombre[64];       // el nombre del archivo sin extensión ni marcas (o el interno de la rom si queda vacío)
    char archivo[96];
    char clave[96];        // nombre del archivo sin extensión: con él se busca la carátula
    char iniciales[4];
    char mapeo[8];
    int tamKB;
    bool pal;
    bool tieneRam;
    bool valida;
    uint32_t color1, color2;
};

static const int MAX_JUEGOS = 600;
int BuscarJuegos(Juego* lista, int max, int* progreso = 0);
const char* CarpetasDeJuegos();
