// Ver pantalla.h.
#include "pantalla.h"

#include <SDL2/SDL.h>

#include "lienzo.h"
#include "plataforma.h"

static SDL_Window* ventana;
static SDL_Surface* sup;
static bool rojoAzul;
static float msDibujo, msEntrega;

bool PantallaIniciar()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
    ventana = SDL_CreateWindow("pcsx2-ps4", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, ANCHO, ALTO, 0);
    if (!ventana) return false;
    sup = SDL_GetWindowSurface(ventana);
    if (!sup || sup->format->BytesPerPixel != 4) return false;
    rojoAzul = sup->format->Rshift == 0;   // el rojo en el byte bajo: la pantalla es ABGR
    LienzoIniciar();
    return true;
}

void PantallaComenzar()
{
    SDL_LockSurface(sup);
    LienzoComenzar((uint32_t*)sup->pixels, sup->pitch / 4, rojoAzul);
}

void PantallaMostrar()
{
    double t0 = Segundos();
    LienzoTerminar();
    SDL_UnlockSurface(sup);
    double t1 = Segundos();
    SDL_UpdateWindowSurface(ventana);
    double t2 = Segundos();
    msDibujo = (float)((t1 - t0) * 1000);
    msEntrega = (float)((t2 - t1) * 1000);
}

float MsDibujo() { return msDibujo; }
float MsEntrega() { return msEntrega; }
