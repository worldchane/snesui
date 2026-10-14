// Prueba del rebobinado: captura cada 2 cuadros guardando una huella del estado; después rebobina y comprueba que cada
// paso atrás deja la consola exactamente en el estado capturado en ese momento. Uso: probar_rebobinar <rom> <cuadros>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "../src/nucleo/snes.h"
#include "../src/app/rebobinar.h"
#include "../src/comun/plataforma.h"

bool ExisteArchivo(const char* r, uint64_t* t) { FILE* f = fopen(r, "rb"); if (!f) return false; if (t) { fseek(f, 0, SEEK_END); *t = ftell(f); } fclose(f); return true; }
FILE* AbrirArchivo(const char* r, const char* m) { return fopen(r, m); }
int LeerTexto(const char* r, char* b, int max) { FILE* f = fopen(r, "rb"); if (!f) return -1; int n = (int)fread(b, 1, max - 1, f); b[n] = 0; fclose(f); return n; }

static uint64_t Huella()
{
    static std::vector<uint8_t> e(SnesEstadoTam());
    SnesGuardarEstado(e.data(), (uint32_t)e.size());
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : e) h = (h ^ b) * 1099511628211ull;
    return h;
}

int main(int argc, char** argv)
{
    char error[160];
    if (argc < 3 || !SnesCargar(argv[1], error)) { printf("no cargó\n"); return 1; }
    int n = atoi(argv[2]);
    static uint32_t pantalla[SNES_ANCHO * SNES_ALTO];
    static int16_t audio[4096];
    if (!RebobinarIniciar()) { printf("sin memoria\n"); return 1; }
    std::vector<uint64_t> huellas;
    for (int i = 0; i < n; i++)
    {
        uint16_t b = 0;
        if (i % 180 < 6) b |= SB_START;
        if (i > 1500) b |= SB_DER | ((i / 40) % 2 ? SB_B : 0);
        SnesMando(0, b);
        SnesCuadro(pantalla, audio, 2048);
        if (i % 2 == 1) { RebobinarCapturar(); huellas.push_back(Huella()); }
    }
    printf("capturas: %d, historia: %.1f s\n", (int)huellas.size(), RebobinarSegundos());
    // Rebobinar: después de cada paso, el estado tiene que ser el de la captura anterior.
    int bien = 0, mal = 0, pasos = 0;
    for (int k = (int)huellas.size() - 2; k >= 0; k--)
    {
        if (!RebobinarAtras()) break;
        pasos++;
        if (Huella() == huellas[k]) bien++; else mal++;
    }
    printf("pasos atrás: %d   iguales: %d   distintos: %d   %s\n", pasos, bien, mal, mal ? "FALLA" : "OK");
    return mal ? 2 : 0;
}
