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
// Fija una opción del núcleo (la que pediría RetroArch en su archivo de opciones), antes de cargar el juego.
void LibretroOpcion(const char* clave, const char* valor);
// Alias de archivos para el núcleo: cuando pida 'virtual_' se abre 'real' (hasta 8; LibretroAliasLimpiar los borra).
void LibretroAlias(const char* virtual_, const char* real);
void LibretroAliasLimpiar();
bool LibretroFaltoMemoria();   // en la última carga, el núcleo avisó que no pudo pedir memoria
