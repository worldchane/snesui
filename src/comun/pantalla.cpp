// Ver pantalla.h. En la consola se habla directo con la salida de video (VideoOut), sin pasar por SDL:
//  - Los dos búferes son siempre de 1920 × 1080 y la consola los escala a lo que salga por HDMI (720p, 1080p o 4K).
//    SDL los creaba del tamaño de la salida, y con la consola en 720p o una Pro en 4K la imagen quedaba cortada o en
//    una esquina.
//  - Se dibuja directo en el búfer que no se está mostrando (SDL además copiaba y borraba la pantalla entera en cada
//    cuadro).
//  - Al mostrar el primer cuadro se quita la pantalla de carga del sistema. En algunas consolas no se va sola y tapa
//    al programa: se ve negro aunque el menú suena.
#include "pantalla.h"

#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <string.h>

#include "lienzo.h"
#include "plataforma.h"

static const int BUFERES = 2;
static int32_t video = -1;
static OrbisKernelEqueue colaFlip;
static uint32_t* bufer[BUFERES];
static int actual;
static bool primerCuadro = true;
static float msDibujo, msEntrega;

// sceSystemServiceHideSplashScreen se busca por nombre en el momento (como la red en plataforma.cpp): así el programa
// no suma una función del sistema a las que pide para abrir, y si no está no pasa nada. La versión "segura" (sin red)
// no carga módulos: no pide ni siquiera estas dos funciones del sistema.
static void QuitarPantallaDeCarga()
{
#ifndef SIN_RED
    static const char* const RUTAS[] = {"libSceSystemService.sprx", "/system/common/lib/libSceSystemService.sprx"};
    for (const char* r : RUTAS)
    {
        int h = (int)sceKernelLoadStartModule(r, 0, NULL, 0, NULL, NULL);
        if (h < 0) continue;
        int (*ocultar)() = NULL;
        if (sceKernelDlsym(h, "sceSystemServiceHideSplashScreen", (void**)&ocultar) == 0 && ocultar) ocultar();
        return;
    }
#endif
}

bool PantallaIniciar()
{
    video = sceVideoOutOpen(0xFF, 0, 0, NULL);   // usuario del sistema, salida principal
    if (video < 0) return false;

    const size_t tam = (size_t)ANCHO * ALTO * 4, alineado = (tam + 0xFFFFF) & ~(size_t)0xFFFFF;   // de a 1 MB
    off_t fisica = 0;
    void* mem = NULL;
    if (sceKernelAllocateDirectMemory(0, 0x180000000u, alineado * BUFERES, 0x100000, 3 /* WC_GARLIC */, &fisica) != 0) return false;
    if (sceKernelMapDirectMemory(&mem, alineado * BUFERES, 0x32 /* CPU y GPU, lectura y escritura */, 0, fisica, 0x100000) != 0)
        return false;
    for (int i = 0; i < BUFERES; i++) bufer[i] = (uint32_t*)((uint8_t*)mem + i * alineado);
    memset(mem, 0, alineado * BUFERES);

    OrbisVideoOutBufferAttribute attr;
    memset(&attr, 0, sizeof attr);
    attr.format = ORBIS_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB;
    attr.tmode = ORBIS_VIDEO_OUT_TILING_MODE_LINEAR;
    attr.aspect = ORBIS_VIDEO_OUT_ASPECT_RATIO_16_9;
    attr.width = ANCHO;
    attr.height = ALTO;
    attr.pixelPitch = ANCHO;
    if (sceKernelCreateEqueue(&colaFlip, "flip") != 0 || sceVideoOutAddFlipEvent(colaFlip, video, NULL) != 0) return false;
    if (sceVideoOutRegisterBuffers(video, 0, (void* const*)bufer, BUFERES, &attr) != 0) return false;
    sceVideoOutSetFlipRate(video, 0);   // 60 Hz
    LienzoIniciar();
    return true;
}

// Espera a que la consola muestre el último cuadro entregado: recién ahí el otro búfer deja de verse.
static void EsperarFlip()
{
    OrbisKernelEvent ev;
    int n = 0;
    while (sceVideoOutIsFlipPending(video) > 0) sceKernelWaitEqueue(colaFlip, &ev, 1, &n, NULL);
}

void PantallaComenzar()
{
    EsperarFlip();
    // A8B8G8R8: el rojo va en el byte bajo.
    LienzoComenzar(bufer[actual], ANCHO, true);
}

void PantallaMostrar()
{
    double t0 = Segundos();
    LienzoTerminar();
    double t1 = Segundos();
    sceVideoOutSubmitFlip(video, actual, 1 /* en el vblank */, 0);
    actual = (actual + 1) % BUFERES;
    if (primerCuadro) { primerCuadro = false; QuitarPantallaDeCarga(); }
    double t2 = Segundos();
    msDibujo = (float)((t1 - t0) * 1000);
    msEntrega = (float)((t2 - t1) * 1000);
}

float MsDibujo() { return msDibujo; }
float MsEntrega() { return msEntrega; }
