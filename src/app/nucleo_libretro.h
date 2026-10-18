// Adaptador de los núcleos libretro a la tabla Consola (ver nucleo_libretro.cpp).
#pragma once
#include "../nucleos/api.h"
#include "consola.h"

// Llena todas las funciones de la tabla menos Cargar, que es propia de cada consola (activa su núcleo).
void LibretroLlenar(Consola& c);
bool LibretroCargar(const ApiLibretro* api, const char* nombre, const char* ruta, char* error);
// Sin leer el archivo: el núcleo lo abre por la ruta (arcade).
bool LibretroCargarRuta(const ApiLibretro* api, const char* nombre, const char* ruta, char* error);
bool LibretroCargarMemoria(const ApiLibretro* api, const char* nombre, const char* ruta, const char* interno, uint8_t* datos,
                           uint32_t tam, char* error);
