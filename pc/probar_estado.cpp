// Prueba de estados rápidos: guarda, corre N cuadros, carga y corre los mismos N; la imagen y el sonido tienen que
// salir idénticos. Uso: probar_estado <rom> <cuadros antes> <cuadros después>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "../src/nucleo/snes.h"
#include "../src/comun/plataforma.h"

bool ExisteArchivo(const char* r, uint64_t* t) { FILE* f = fopen(r, "rb"); if (!f) return false; if (t) { fseek(f, 0, SEEK_END); *t = ftell(f); } fclose(f); return true; }
FILE* AbrirArchivo(const char* r, const char* m) { return fopen(r, m); }
int LeerTexto(const char* r, char* b, int max) { FILE* f = fopen(r, "rb"); if (!f) return -1; int n = (int)fread(b, 1, max - 1, f); b[n] = 0; fclose(f); return n; }

static uint32_t pantalla[SNES_ANCHO * SNES_ALTO];
static int16_t audio[4096];

static uint16_t Botones(int i) { uint16_t b = 0; if (i % 180 < 6) b |= SB_START; if (i > 1500) b |= SB_DER | ((i / 40) % 2 ? SB_B : 0); return b; }

static uint64_t Correr(int desde, int n)
{
    uint64_t h = 1469598103934665603ull;
    for (int i = desde; i < desde + n; i++)
    {
        SnesMando(0, Botones(i));
        int na = SnesCuadro(pantalla, audio, 2048);
        const uint8_t* p = (const uint8_t*)audio;
        for (int k = 0; k < na * 4; k++) h = (h ^ p[k]) * 1099511628211ull;
        p = (const uint8_t*)pantalla;
        for (size_t k = 0; k < sizeof pantalla; k++) h = (h ^ p[k]) * 1099511628211ull;
    }
    return h;
}

int main(int argc, char** argv)
{
    if (argc < 4) return 1;
    char error[160];
    if (!SnesCargar(argv[1], error)) { printf("no cargó: %s\n", error); return 1; }
    int antes = atoi(argv[2]), despues = atoi(argv[3]);
    Correr(0, antes);
    std::vector<uint8_t> e(SnesEstadoTam());
    uint32_t n = SnesGuardarEstado(e.data(), (uint32_t)e.size());
    printf("estado: %u bytes (%u KB)\n", n, n / 1024);
    uint64_t a = Correr(antes, despues);
    bool ok = SnesCargarEstado(e.data(), n);
    uint64_t b = Correr(antes, despues);
    printf("cargar: %s   recorrido 1: %016llx   recorrido 2: %016llx   %s\n", ok ? "ok" : "FALLÓ",
           (unsigned long long)a, (unsigned long long)b, a == b ? "IDÉNTICOS" : "DISTINTOS");
    // Un estado alterado (otro juego) tiene que rechazarse.
    e[20] ^= 0xFF;
    printf("estado de otro juego rechazado: %s\n", SnesCargarEstado(e.data(), n) ? "NO" : "sí");
    return a == b ? 0 : 2;
}
