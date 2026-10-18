// Graba las canciones de la biblioteca (src/comun/musica.cpp) en archivos WAV, para escucharlas en la PC:
//   musica_wav <carpeta> [segundos]
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/comun/musica.h"

int main(int argc, char** argv)
{
    if (argc < 2) { printf("uso: musica_wav <carpeta> [segundos]\n"); return 1; }
    int segundos = argc > 2 ? atoi(argv[2]) : 45;
    for (int c = 0; c < NUM_CANCIONES; c++)
    {
        char ruta[512];
        snprintf(ruta, sizeof ruta, "%s/%02d - %s.wav", argv[1], c + 1, MusicaNombre(c));
        FILE* f = fopen(ruta, "wb");
        if (!f) { printf("no se pudo crear %s\n", ruta); return 1; }
        uint32_t n = 48000u * segundos, bytes = n * 4;
        uint32_t cab[11] = {0x46464952, 36 + bytes, 0x45564157, 0x20746D66, 16, 0x00020001, 48000, 48000 * 4, 0x00100004, 0x61746164, bytes};
        fwrite(cab, 4, 11, f);
        MusicaElegir(c);
        MusicaActiva(true);
        static float L[256], R[256];
        static int16_t s[512];
        float pico = 0;
        for (uint32_t k = 0; k < n; k += 256)
        {
            memset(L, 0, sizeof L);
            memset(R, 0, sizeof R);
            MusicaMezclar(L, R, 256);
            for (int i = 0; i < 256; i++)
            {
                float a = L[i], b = R[i];
                float m = a < 0 ? -a : a;
                if (m > pico) pico = m;
                if (a > 1) a = 1; if (a < -1) a = -1; if (b > 1) b = 1; if (b < -1) b = -1;
                s[i * 2] = (int16_t)(a * 32000);
                s[i * 2 + 1] = (int16_t)(b * 32000);
            }
            fwrite(s, 2, 512, f);
        }
        fclose(f);
        printf("%s (pico %.2f)\n", ruta, pico);
    }
    return 0;
}
