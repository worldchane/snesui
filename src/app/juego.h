// La pantalla donde se juega: carga la rom, corre el emulador un cuadro por fotograma, muestra la imagen de la SNES
// (256 × 224) agrandada a la pantalla y pasa el mando. ○ o OPTIONS vuelve a la biblioteca.
#pragma once
#include "../comun/plataforma.h"

// 'sistema': la consola que eligió la biblioteca (SIS_*), o -1 para deducirla de la extensión.
bool JuegoAbrir(const char* ruta, const char* nombre, int sistema = -1);
bool JuegoActivo();
void JuegoCuadro(float t, float dt, const Mando& m);
