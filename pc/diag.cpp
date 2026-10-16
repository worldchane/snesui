// Corre una rom N cuadros (con los botones del modo 'jugar' si se pide) e imprime el informe de diagnóstico del núcleo
// y los problemas que detectó. Uso: diag <rom> <cuadros> [jugar]
// Compilado con -DGSU_TRAZA, TRAZA=n imprime además las últimas n instrucciones del Super FX.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/nucleo/snes.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

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

int depurCuadro;   // 1 durante el último cuadro (para los registros de depuración)

int main(int argc, char** argv)
{
    char e[160];
    if (argc < 3 || !SnesCargar(argv[1], e)) { printf("no cargó\n"); return 1; }
    int cuadros = atoi(argv[2]);
    // RAMINICIAL=hex: llena la RAM del cartucho como otro emulador (snes9x usa 60), para comparar memorias.
    if (getenv("RAMINICIAL")) memset(sram, (int)strtol(getenv("RAMINICIAL"), NULL, 16), sizeof sram);
    bool jugar = argc > 3;
    static uint32_t fb[256 * 224];
    static int16_t au[4096];
    for (int i = 0; i < cuadros; i++)
    {
        uint16_t b = 0;
        if (jugar) { if (i % 180 < 6) b |= SB_START; if (i > 1500) b |= SB_DER | ((i / 40) % 2 ? SB_B : 0); }
        SnesMando(0, b);
        depurCuadro = i >= cuadros - (getenv("DEPUR_CUADROS") ? atoi(getenv("DEPUR_CUADROS")) : 1);
        SnesCuadro(fb, au, 2048);
        // REBOBINAR=1: guarda un estado cada 2 cuadros, como la consola para poder rebobinar.
        static bool rebobinar = getenv("REBOBINAR") != NULL;
        if (rebobinar && (i & 1))
        {
            static uint8_t* buf = (uint8_t*)malloc(SnesEstadoTam());
            SnesGuardarEstado(buf, SnesEstadoTam());
        }
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
#ifdef GSU_TRAZA
                {
                    const uint8_t* PpuVram();
                    snprintf(r, sizeof r, "pc/salida/mem_%d_vram.bin", i + 1);
                    FILE* fv = fopen(r, "wb"); fwrite(PpuVram(), 1, 65536, fv); fclose(fv);
                }
#endif
                snprintf(r, sizeof r, "pc/salida/mem_%d_sram.bin", i + 1);
                f = fopen(r, "wb"); fwrite(sram, 1, sizeof sram, f); fclose(f);
            }
        }
        // MUESTREO=n: cada n cuadros, dónde está el procesador.
        static int muestreo = getenv("MUESTREO") ? atoi(getenv("MUESTREO")) : 0;
        if (muestreo && (i + 1) % muestreo == 0)
        {
            static char t2[24 * 1024];
            SnesDiagnostico(t2, sizeof t2);
            char* c = strstr(t2, "CPU:");
            char* f = c ? strchr(c, '\n') : NULL;
            if (f) *f = 0;
            printf("cuadro %d: %s\n", i + 1, c ? c : "?");
        }
        char m[200];
        if (SnesAnomalia(m, sizeof m)) printf("cuadro %d: PROBLEMA: %s\n", i, m);
    }
#ifdef GSU_TRAZA
    extern unsigned gsuLecturasBloqueadas, gsuRomBloqueada;
    printf("lecturas de la SNES bloqueadas: RAM del chip %u, rom %u\n", gsuLecturasBloqueadas, gsuRomBloqueada);
    if (getenv("HISTOGRAMA"))
    {
        extern unsigned gsuHistograma[1024];
        for (int k = 0; k < 1024; k++)
            if (gsuHistograma[k]) printf("op %02X alt%d: %u\n", k & 255, k >> 8, gsuHistograma[k]);
    }
    int cuantas = getenv("TRAZA") ? atoi(getenv("TRAZA")) : 0;
    for (int k = cuantas; k > 0; k--)
    {
        const Traza& x = gsuTraza[(gsuTrazaN - k) & 4095];
        printf("%02X:%04X %02X sfr=%04X", x.pbr, x.pc, x.op, x.sfr);
        for (int j = 0; j < 16; j++) printf(" %04X", x.r[j]);
        printf("\n");
    }
#endif
    {
        // La última imagen, en pc/salida/diag.png.
        static unsigned char rgb[256 * 224 * 3];
        for (int k = 0; k < 256 * 224; k++) { rgb[k * 3] = fb[k] >> 16; rgb[k * 3 + 1] = fb[k] >> 8; rgb[k * 3 + 2] = fb[k]; }
        stbi_write_png(getenv("PNG") ? getenv("PNG") : "pc/salida/diag.png", 256, 224, 3, rgb, 256 * 3);
    }
#ifdef GSU_TRAZA
    if (getenv("OAM"))
    {
        const uint8_t* PpuOam();
        const uint8_t* o = PpuOam();
        for (int k = 0; k < 128; k++)
        {
            int x = o[k * 4] | (((o[512 + k / 4] >> ((k & 3) * 2)) & 1) << 8), y = o[k * 4 + 1];
            if (y >= 224 && y < 240) continue;
            printf("spr %3d x=%3d y=%3d tile=%02X attr=%02X (prio %d pal %d)%s\n", k, x, y, o[k * 4 + 2], o[k * 4 + 3],
                   (o[k * 4 + 3] >> 4) & 3, (o[k * 4 + 3] >> 1) & 7, ((o[512 + k / 4] >> ((k & 3) * 2)) & 2) ? " grande" : "");
        }
    }
#endif
    static char t[24 * 1024];
    SnesDiagnostico(t, sizeof t);
    fputs(t, stdout);
    return 0;
}
