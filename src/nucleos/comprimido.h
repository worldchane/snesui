/* Lectura de archivos comprimidos (.zip, .7z y .rar) ya cargados en memoria, para abrir roms comprimidas. Usa miniz
 * (zip, licencia MIT), el LZMA SDK de 7-Zip (7z, dominio público) y dmc_unrar (rar 1.5 a 5, licencia GPL 2). Se
 * compila con los núcleos (nucleos.sh). */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { COMPRIMIDO_NINGUNO, COMPRIMIDO_ZIP, COMPRIMIDO_7Z, COMPRIMIDO_RAR };

typedef struct
{
    char nombre[256];   /* con las carpetas de adentro, separadas por '/' */
    uint64_t tam;       /* descomprimido */
    int indice;         /* para ComprimidoSacar */
} EntradaComprimido;

/* El tipo por los primeros bytes. */
int ComprimidoTipo(const uint8_t* d, size_t n);
/* Los archivos de adentro (sin las carpetas). Devuelve cuántos dejó en 'out' (a lo más 'max'), o -1 si está dañado. */
int ComprimidoListar(const uint8_t* d, size_t n, EntradaComprimido* out, int max);
/* Lo mismo leyendo del archivo solo el índice, sin cargarlo entero (para la búsqueda de la biblioteca). */
int ComprimidoListarRuta(const char* ruta, EntradaComprimido* out, int max);
/* Descomprime un archivo de adentro a un búfer nuevo (malloc). NULL si no se pudo. */
uint8_t* ComprimidoSacar(const uint8_t* d, size_t n, int indice, size_t* tam);

#ifdef __cplusplus
}
#endif
