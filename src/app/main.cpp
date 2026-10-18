// Emulador de SNES para PS4: la interfaz en alta definición y el emulador, dibujados por software entre varios
// núcleos. Hecho con lo que ya funcionó en la PS4 del usuario (ver LEEME.md): sin excepciones de C++, sin RTTI, sin
// std::thread y sin variables globales con constructores.
#include <stdio.h>

#include <orbis/libkernel.h>

#include "../comun/estilo.h"
#include "../comun/lienzo.h"
#include "../comun/pantalla.h"
#include "../comun/plataforma.h"
#include "../comun/texto.h"
#include "consola.h"
#include "interfaz.h"

// Los constructores globales de los núcleos libretro (ver herramientas/sin_constructores.py): en vez de quedar en
// .init_array, que el sistema correría solo, van en la sección nucleo_init y se llaman acá, al empezar.
extern "C" {
typedef void (*Constructor)(void);
extern const Constructor __start_nucleo_init[];
extern const Constructor __stop_nucleo_init[];
}
// Una entrada vacía propia: así la sección existe siempre (aunque no haya núcleos) y el enlazador define __start y
// __stop (si no, quedarían como funciones a pedir al sistema).
__attribute__((section("nucleo_init"), used)) static const Constructor nadaQueIniciar = 0;

static void IniciarNucleos()
{
    for (const Constructor* c = __start_nucleo_init; c < __stop_nucleo_init; c++)
        if (*c) (*c)();
}

#ifdef PRUEBA_BSS_MB
// Prueba de arranque: relleno vacío para ver si hay un tope de memoria estática al cargar el programa.
static char lastre[PRUEBA_BSS_MB * 1024 * 1024];
#endif

int main()
{
#ifdef PRUEBA_BSS_MB
    ((volatile char*)lastre)[0] = 1;
    ((volatile char*)lastre)[sizeof lastre - 1] = 1;
#endif
#ifndef SIN_INICIAR_NUCLEOS
    IniciarNucleos();
#endif
    setvbuf(stdout, NULL, _IONBF, 0);
    Segundos();
    IniciarMando();
    IniciarAudio();
    CrearCarpetas(CARPETA_DATOS);   // /data/snes-ps4 (guardados, carátulas, ajustes)
    CopiarBiosIncluidas();          // solo en la variante personal
    IniciarTrabajadores(4);
    if (!PantallaIniciar()) for (;;) sceKernelUsleep(1000000);
    bool fuentes = CargarFuentes();
    InterfazIniciar();
    Sonar(SON_INICIO);

    double t0 = Segundos(), antes = t0;
    Mando m;
    for (;;)
    {
        double ahora = Segundos();
        float dt = (float)(ahora - antes);
        if (dt > .05f) dt = .05f;
        antes = ahora;
        LeerMando(m, dt);

        PantallaComenzar();
        if (fuentes) InterfazCuadro((float)(ahora - t0), dt, m);
        else { FondoAnimado((float)ahora, true); Circulo(ANCHO / 2, ALTO / 2, 80, C_MAL, 1); }
        PantallaMostrar();
    }
}
