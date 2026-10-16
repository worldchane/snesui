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
#include "interfaz.h"

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    Segundos();
    IniciarMando();
    IniciarAudio();
    CrearCarpetas(CARPETA_DATOS);   // /data/snes-ps4 (guardados, carátulas, ajustes)
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
