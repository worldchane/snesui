// Los juegos de SNES que hay en el USB o en /data.
//
// La lista se arma en un hilo aparte y se guarda en un índice (/data/snes-ps4/biblioteca.txt): al abrir el programa
// se muestra el índice al instante y, mientras tanto, se vuelve a recorrer el USB. Las roms que ya estaban en el
// índice con el mismo tamaño no se vuelven a leer; solo se analizan las nuevas o cambiadas. Cuando termina, la lista
// nueva reemplaza a la anterior (BibliotecaNueva).
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
    uint64_t bytes;        // tamaño del archivo (si cambia, se vuelve a analizar)
    int tamKB;
    bool pal;
    bool tieneRam;
    bool valida;
    bool enUsb;            // está en un USB (si no, en la consola: /data/snes-ps4/juegos)
    bool copiado;          // está en el USB y también hay una copia igual en la consola (se muestra solo la copia)
    bool tambienEnUsb;     // está en la consola y también hay una copia igual en un USB
    uint32_t color1, color2;
};

static const int MAX_JUEGOS = 10000;

void BibliotecaIniciar();                  // carga el índice guardado y empieza a buscar en segundo plano
void BibliotecaBuscar();                   // vuelve a buscar (si ya está buscando, no hace nada)
bool BibliotecaBuscando(int* encontrados); // true mientras busca; cuántas roms lleva
bool BibliotecaNueva();                    // true (una vez) cuando hay una lista nueva: volver a pedir BibliotecaJuegos
const Juego* BibliotecaJuegos(int* n);     // la lista actual, ordenada por nombre (no cambia hasta BibliotecaNueva)
const char* CarpetasDeJuegos();
// Carpeta de la consola donde se copian los juegos (y donde también se buscan).
static const char* const CARPETA_JUEGOS_CONSOLA = "/data/snes-ps4/juegos";
