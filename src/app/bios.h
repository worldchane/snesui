// Las BIOS que usan las consolas: se reconocen por su contenido (tamaño y CRC-32), no por el nombre del archivo, y se
// copian a /data/allesiu/bios con el nombre que espera cada emulador.
#pragma once
#include <stdint.h>

struct BiosConocida
{
    int sistema;               // SIS_*
    const char* archivo;       // el nombre en /data/allesiu/bios
    uint32_t tam, crc;
    const char* descripcion;   // para la lista: "Game Boy Advance", "Master System (Japón)"...
    bool necesaria;            // sin ella la consola no corre (si no, mejora la compatibilidad o el arranque)
};

enum { BIOS_FALTA, BIOS_BIEN, BIOS_DISTINTA };   // distinta: hay un archivo con ese nombre pero no es la BIOS correcta

int BiosCuantas();
const BiosConocida& BiosDe(int i);
int BiosEstado(int i);         // lo que hay en /data/allesiu/bios (se revisa en BiosRevisar)
void BiosRevisar();            // vuelve a mirar /data/allesiu/bios
// Busca BIOS en las carpetas de BIOS de los USB (bios, BIOS, roms/bios, system...), sueltas o dentro de un zip, y copia a
// la consola las que falten. Llamar con el acceso completo abierto. Devuelve cuántas copió.
int BiosBuscarEnUsb();
// Un archivo cualquiera que encontró la búsqueda de juegos: si es una BIOS (por su extensión, tamaño y contenido), la
// copia a la consola si falta y devuelve true (no es un juego). Así se encuentran aunque estén desordenadas en el USB.
bool BiosProbar(const char* ruta, const char* nombre);
// Cuántas BIOS hay, de las que sirven a 'sistema' (y cuántas se conocen para él).
int BiosDeConsola(int sistema, int* conocidas);
