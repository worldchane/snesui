// Emulador de SNES para PS4: la interfaz en alta definición y el emulador, dibujados por software entre varios
// núcleos. Hecho con lo que ya funcionó en la PS4 del usuario (ver LEEME.md): sin excepciones de C++, sin RTTI, sin
// std::thread y sin variables globales con constructores.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <orbis/libkernel.h>

#include "../comun/estilo.h"
#include "../comun/lienzo.h"
#include "../comun/pantalla.h"
#include "../comun/plataforma.h"
#include "../comun/texto.h"
#include "arcade.h"
#include "consola.h"
#include "interfaz.h"
#include "prueba_jit.h"
#include "mudanza.h"
#include "registro.h"

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

#ifdef PRUEBA_CODIGO_MB
// Prueba de arranque: relleno en el código y en los datos constantes, para ver si hay un tope de tamaño del programa
// (con FinalBurn Neo el código pasa de 5,6 a 22 MB y los datos constantes de 0,1 a 8 MB).
#define PRUEBA_TEXTO2(x) #x
#define PRUEBA_TEXTO(x) PRUEBA_TEXTO2(x)
__asm__(".section .text.relleno,\"ax\",@progbits\n.globl RellenoDeCodigo\nRellenoDeCodigo:\n.fill " PRUEBA_TEXTO(PRUEBA_CODIGO_MB) "*1048576,1,0xcc\n"
        ".section .rodata.relleno,\"a\",@progbits\n.globl RellenoDeDatos\nRellenoDeDatos:\n.fill 8*1048576,1,0x5a\n.text\n");
extern "C" const char RellenoDeCodigo[], RellenoDeDatos[];
#endif

#ifdef PRUEBA_PUNTEROS
// Prueba de arranque: una tabla de punteros que la PS4 tiene que corregir al cargar (una reubicación por cada uno),
// para ver si hay un tope de reubicaciones. Escribible (no va a la zona RELRO) y armada en compilación (sin constructor).
static void PunteroDePrueba() {}
struct TablaDePrueba
{
    void (*p[PRUEBA_PUNTEROS])();
    constexpr TablaDePrueba() : p() { for (int i = 0; i < PRUEBA_PUNTEROS; i++) p[i] = PunteroDePrueba; }
};
static TablaDePrueba tablaDePrueba;
#endif

int main()
{
    ActivarCandadosLibc();          // malloc/free a salvo entre hilos (plataforma.cpp): antes que nada
#ifdef PRUEBA_JIT
    // La prueba de GPU antes que todo lo demás (mando, audio, carpetas, acceso de GoldHEN, núcleos y pantalla):
    // eglGetDisplay fallaba sin dar error con todo eso ya iniciado (pruebas 45 a 47). Con el registro de cierres ya
    // activo (si se cae en la prueba, queda anotado dónde) y una marca de que el programa llegó a main.
    CrearCarpetas(CARPETA_DIAGNOSTICO);
    {
        char m[160];
        snprintf(m, sizeof m, "%s/llego_a_main.txt", CARPETA_DIAGNOSTICO);
        EscribirTexto(m, "si\n");
    }
    PruebaGpu();
#endif
#ifdef SOLO_PRUEBA_GPU
    // Variante mínima (sin núcleos, sin interfaz): solo la prueba de GPU; el resultado queda en el registro del USB.
    RegistroIniciar("prueba de GPU mínima");
    Registrar("Resultado: %s", PruebaGpu());
    for (;;) { RegistroCuadro(); sceKernelUsleep(500000); }
#endif
#ifdef PRUEBA_BSS_MB
    ((volatile char*)lastre)[0] = 1;
    ((volatile char*)lastre)[sizeof lastre - 1] = 1;
#endif
#ifdef PRUEBA_CODIGO_MB
    if (((volatile const char*)RellenoDeDatos)[5] != 0x5a || ((volatile const char*)RellenoDeCodigo)[0] != (char)0xcc) printf("relleno mal\n");
#endif
#ifdef PRUEBA_PUNTEROS
    if (((void (*volatile*)())tablaDePrueba.p)[PRUEBA_PUNTEROS - 1] != PunteroDePrueba) printf("tabla de prueba mal\n");
#endif
#ifndef SIN_INICIAR_NUCLEOS
    IniciarNucleos();
#endif
    setvbuf(stdout, NULL, _IONBF, 0);
    CrearCarpetas(CARPETA_DIAGNOSTICO);
    CapturarCierres();              // si el programa se cae, queda anotado dónde en /data/allesiu/diagnostico/cierre.txt
    Segundos();
    IniciarMando();
    IniciarAudio();
    Mudanza();                      // datos de versiones anteriores a /data/allesiu y sus carpetas nuevas (mudanza.cpp)
    CopiarBiosIncluidas();          // solo en la variante personal
    // La tabla de juegos arcade se carga acá, antes de los hilos: la búsqueda y las carátulas la consultan a la vez y,
    // cargada al vuelo, el que llegaba segundo la veía vacía (juegos en "Arcade" con el nombre del zip).
    ArcadeIniciar();
    MudanzaOrdenarRoms();           // los juegos sueltos de la consola, cada uno a roms/<consola>
    IniciarTrabajadores(5);         // con el hilo principal, 6 partes: los 6 núcleos de la consola para la app
    if (!PantallaIniciar()) { Registrar("No se pudo abrir la pantalla"); for (;;) sceKernelUsleep(1000000); }
    bool fuentes = CargarFuentes();
    InterfazIniciar();
    if (MudanzaResumen()[0]) Registrar("%s", MudanzaResumen());   // después de la cabecera del registro
    char memoria[160];
    MemoriaResumen(memoria, sizeof memoria);
    Registrar("Al iniciar: %s", memoria);
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
