// Corre una rom N cuadros (con los botones del modo 'jugar' si se pide) e imprime el informe de diagnóstico del núcleo
// y los problemas que detectó. Uso: diag <rom> <cuadros> [jugar]
// Compilado con -DGSU_TRAZA, TRAZA=n imprime además las últimas n instrucciones del Super FX.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/nucleo/snes.h"

bool ExisteArchivo(const char* r, uint64_t* t) { FILE* f = fopen(r, "rb"); if (!f) return false; if (t) { _fseeki64(f, 0, SEEK_END); *t = (uint64_t)_ftelli64(f); } fclose(f); return true; }
FILE* AbrirArchivo(const char* r, const char* m) { return fopen(r, m); }
int LeerTexto(const char* r, char* buf, int max) { FILE* f = fopen(r, "rb"); if (!f) return -1; int n = (int)fread(buf, 1, max - 1, f); fclose(f); buf[n < 0 ? 0 : n] = 0; return n; }

extern uint8_t wram[128 * 1024];
extern uint8_t sram[128 * 1024];

#ifdef GSU_TRAZA
struct Traza { uint8_t pbr; uint16_t pc; uint8_t op; uint16_t sfr; uint16_t r[16]; };
extern Traza gsuTraza[4096];
extern unsigned gsuTrazaN;
#endif

int main(int argc, char** argv)
{
    char e[160];
    if (argc < 3 || !SnesCargar(argv[1], e)) { printf("no cargó\n"); return 1; }
    int cuadros = atoi(argv[2]);
    bool jugar = argc > 3;
    static uint32_t fb[256 * 224];
    static int16_t au[4096];
    for (int i = 0; i < cuadros; i++)
    {
        uint16_t b = 0;
        if (jugar) { if (i % 180 < 6) b |= SB_START; if (i > 1500) b |= SB_DER | ((i / 40) % 2 ? SB_B : 0); }
        SnesMando(0, b);
        SnesCuadro(fb, au, 2048);
        // VOLCAR=c1,c2,...: guarda la WRAM y la RAM del cartucho al final de esos cuadros (pc/salida/mem_<c>_*.bin).
        static const char* volcar = getenv("VOLCAR");
        if (volcar)
        {
            char lista[256], *p;
            snprintf(lista, sizeof lista, ",%s,", volcar);
            char busca[16];
            snprintf(busca, sizeof busca, ",%d,", i + 1);
            if ((p = strstr(lista, busca)))
            {
                char r[64];
                snprintf(r, sizeof r, "pc/salida/mem_%d_wram.bin", i + 1);
                FILE* f = fopen(r, "wb"); fwrite(wram, 1, sizeof wram, f); fclose(f);
                snprintf(r, sizeof r, "pc/salida/mem_%d_sram.bin", i + 1);
                f = fopen(r, "wb"); fwrite(sram, 1, sizeof sram, f); fclose(f);
            }
        }
        char m[200];
        if (SnesAnomalia(m, sizeof m)) printf("cuadro %d: PROBLEMA: %s\n", i, m);
    }
#ifdef GSU_TRAZA
    int cuantas = getenv("TRAZA") ? atoi(getenv("TRAZA")) : 0;
    for (int k = cuantas; k > 0; k--)
    {
        const Traza& x = gsuTraza[(gsuTrazaN - k) & 4095];
        printf("%02X:%04X %02X sfr=%04X", x.pbr, x.pc, x.op, x.sfr);
        for (int j = 0; j < 16; j++) printf(" %04X", x.r[j]);
        printf("\n");
    }
#endif
    static char t[24 * 1024];
    SnesDiagnostico(t, sizeof t);
    fputs(t, stdout);
    return 0;
}
