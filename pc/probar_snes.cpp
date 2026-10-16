// Prueba del núcleo de SNES en el PC: carga una rom (o arma una de prueba que pinta la pantalla), corre unos cuadros y
// guarda una imagen PNG, más la velocidad.   Uso: probar_snes [rom.sfc] [cuadros]
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "../src/nucleo/snes.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

// Stubs de plataforma que el núcleo necesita (rom.cpp).
#include <stdio.h>
extern "C" {}
bool ExisteArchivo(const char* r, uint64_t* tam)
{
    FILE* f = fopen(r, "rb");
    if (!f) return false;
    if (tam) { _fseeki64(f, 0, SEEK_END); *tam = (uint64_t)_ftelli64(f); }
    fclose(f);
    return true;
}
FILE* AbrirArchivo(const char* r, const char* m) { return fopen(r, m); }
int LeerTexto(const char* r, char* buf, int max) { FILE* f = fopen(r, "rb"); if (!f) return -1; int n = (int)fread(buf, 1, max - 1, f); fclose(f); buf[n < 0 ? 0 : n] = 0; return n; }

// Rom de prueba: pinta la pantalla de un color (verifica CPU + bus + PPU).
static void CrearRomPrueba(const char* ruta)
{
    unsigned char rom[0x8000] = {0};
    unsigned char code[] = {
        0x78,                   // SEI
        0xA9, 0x00, 0x8D, 0x21, 0x21,   // LDA #$00 ; STA $2121 (CGADD=0)
        0xA9, 0x00, 0x8D, 0x22, 0x21,   // LDA #$00 ; STA $2122 (color low)
        0xA9, 0x7C, 0x8D, 0x22, 0x21,   // LDA #$7C ; STA $2122 (color high = 0x7C00 azul)
        0xA9, 0x0F, 0x8D, 0x00, 0x21,   // LDA #$0F ; STA $2100 (INIDISP brillo 15)
        0x80, 0xFE,             // BRA *
    };
    memcpy(rom, code, sizeof code);
    // Cabecera interna (LoROM) en 0x7FC0.
    memcpy(rom + 0x7FC0, "PRUEBA SNES          ", 21);
    rom[0x7FD5] = 0x20;   // LoROM
    rom[0x7FFC] = 0x00; rom[0x7FFD] = 0x80;   // vector de reinicio = 0x8000
    FILE* f = fopen(ruta, "wb");
    fwrite(rom, 1, sizeof rom, f);
    fclose(f);
}

// Carpeta de las imágenes y el sonido: pc/salida, o la que diga SALIDA (para correr varios a la vez).
static const char* Salida() { const char* s = getenv("SALIDA"); return s ? s : "pc/salida"; }

int main(int argc, char** argv)
{
    const char* ruta = argc > 1 ? argv[1] : "pc/salida/prueba.sfc";
    int cuadros = argc > 2 ? atoi(argv[2]) : 120;
    if (argc <= 1) CrearRomPrueba(ruta);

    InfoRom info;
    SnesAnalizar(ruta, info);
    printf("rom: valida=%d titulo='%s' mapeo='%s' tam=%dKB pal=%d sram=%d\n  error='%s'\n",
           info.valida, info.titulo, info.mapeo, info.tamKB, info.pal, info.tieneRam, info.error);
    char error[160];
    if (!SnesCargar(ruta, error)) { printf("no cargó: %s\n", error); return 1; }
    // RAMINICIAL=hex: llena la RAM del cartucho con ese valor (para imitar a otro emulador al comparar).
    if (getenv("RAMINICIAL") && SnesSramTam()) memset(SnesSramDatos(), (int)strtol(getenv("RAMINICIAL"), NULL, 16), SnesSramTam());

    if (getenv("SUPERFX")) SnesSuperFxVelocidad(atoi(getenv("SUPERFX")));   // aceleración del Super FX
    static uint32_t pantalla[SNES_ANCHO * SNES_ALTO];
    static int16_t audio[4096];
    std::vector<int16_t> wav;
    auto t0 = std::chrono::steady_clock::now();
    // Con un tercer argumento 'jugar': aprieta Start cada 3 s y avanza a la derecha, y guarda capturas cada 5 s.
    bool jugar = argc > 3;
    const int reinicio = getenv("REINICIAR") ? atoi(getenv("REINICIAR")) : 0;   // botón de reinicio en ese cuadro
    for (int i = 0; i < cuadros; i++)
    {
        if (reinicio && i == reinicio) SnesReiniciar();
        uint16_t b = 0;
        if (jugar)
        {
            static const int boton = getenv("BOTON") ? atoi(getenv("BOTON")) : 3;   // BOTON=8: A en lugar de Start
            static const int desde = getenv("DESDE") ? atoi(getenv("DESDE")) : 0;   // no apretar antes de ese cuadro
            if (i % 180 < 6 && i >= desde) b |= (uint16_t)(1 << boton);
            if (i > 1500) b |= SB_DER | ((i / 40) % 2 ? SB_B : 0);
        }
        SnesMando(0, b);
        // TIEMPOS=1: cada 300 cuadros, cuánto tardó en promedio cada cuadro y el peor.
        static bool tiempos = getenv("TIEMPOS") != NULL;
        static double suma, peor;
        static auto tc = std::chrono::steady_clock::now();
        if (tiempos)
        {
            auto ahora = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(ahora - tc).count();
            tc = ahora;
            if (i) { suma += ms; if (ms > peor) peor = ms; }
            if (i % 300 == 299) { printf("cuadros %4d-%4d: %.2f ms promedio, peor %.2f ms\n", i - 299, i, suma / 300, peor); suma = peor = 0; }
        }
        static bool sinPpu = getenv("SINPPU") != NULL;
        int na = SnesCuadro(sinPpu ? NULL : pantalla, audio, 2048);
        wav.insert(wav.end(), audio, audio + na * 2);
        // CAPTURAS=desde,hasta guarda además cada cuadro de ese rango (para comparar el ritmo con un emulador de referencia).
        static int capDesde = -1, capHasta = -1;
        if (capDesde < 0) { const char* e = getenv("CAPTURAS"); capDesde = 0; if (e) sscanf(e, "%d,%d", &capDesde, &capHasta); }
        if ((jugar && i % 300 == 299) || (i + 1 >= capDesde && i + 1 <= capHasta))
        {
            std::vector<unsigned char> c(SNES_ANCHO * SNES_ALTO * 3);
            for (int k = 0; k < SNES_ANCHO * SNES_ALTO; k++) { c[k * 3] = pantalla[k] >> 16; c[k * 3 + 1] = pantalla[k] >> 8; c[k * 3 + 2] = pantalla[k]; }
            char r[300]; snprintf(r, sizeof r, "%s/juego_%04d.png", Salida(), i + 1);
            stbi_write_png(r, SNES_ANCHO, SNES_ALTO, 3, c.data(), SNES_ANCHO * 3);
        }
    }
    {
        // Guardar el sonido como WAV (32 kHz, estéreo, 16 bits).
        char rw[300]; snprintf(rw, sizeof rw, "%s/audio.wav", Salida());
        FILE* w = fopen(rw, "wb");
        uint32_t datos = (uint32_t)wav.size() * 2, v32; uint16_t v16;
        fwrite("RIFF", 1, 4, w); v32 = 36 + datos; fwrite(&v32, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w);
        v32 = 16; fwrite(&v32, 4, 1, w); v16 = 1; fwrite(&v16, 2, 1, w); v16 = 2; fwrite(&v16, 2, 1, w);
        v32 = 32000; fwrite(&v32, 4, 1, w); v32 = 32000 * 4; fwrite(&v32, 4, 1, w); v16 = 4; fwrite(&v16, 2, 1, w); v16 = 16; fwrite(&v16, 2, 1, w);
        fwrite("data", 1, 4, w); fwrite(&datos, 4, 1, w); fwrite(wav.data(), 2, wav.size(), w); fclose(w);
        int pico = 0; for (int16_t x : wav) { int a = x < 0 ? -x : x; if (a > pico) pico = a; }
        printf("audio: %d muestras, pico %d\n", (int)wav.size() / 2, pico);
    }
    if (SnesSramTam())   // la RAM del cartucho al final, para comparar con la de bsnes
    {
        char rr[300]; snprintf(rr, sizeof rr, "%s/sram.bin", Salida());
        if (FILE* f = fopen(rr, "wb")) { fwrite(SnesSramDatos(), 1, SnesSramTam(), f); fclose(f); }
    }
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::vector<unsigned char> rgb(SNES_ANCHO * SNES_ALTO * 3);
    for (int i = 0; i < SNES_ANCHO * SNES_ALTO; i++)
    {
        rgb[i * 3] = pantalla[i] >> 16; rgb[i * 3 + 1] = pantalla[i] >> 8; rgb[i * 3 + 2] = pantalla[i];
    }
    char rs[300]; snprintf(rs, sizeof rs, "%s/snes.png", Salida());
    stbi_write_png(rs, SNES_ANCHO, SNES_ALTO, 3, rgb.data(), SNES_ANCHO * 3);
    printf("%d cuadros en %.2f s (%.0f/s). Imagen en pc/salida/snes.png\n", cuadros, s, cuadros / s);
    return 0;
}
