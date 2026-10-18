// Probador de núcleos libretro enlazados en el programa (uno por ejecutable): carga una rom, corre N cuadros con el
// mando programado como probar_snes y guarda capturas PNG cada 300 cuadros (o las de CAPTURAS=desde,hasta).
// Uso: probar_nucleo <rom> <cuadros> [jugar]
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <vector>

#include "libretro.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

static const char* Salida() { const char* s = getenv("SALIDA"); return s ? s : "pc/salida"; }

static unsigned formato = RETRO_PIXEL_FORMAT_0RGB1555;
static std::vector<uint32_t> imagen;
static unsigned ancho, alto;
static uint16_t botones;
static long muestras;

static void Registro(enum retro_log_level, const char* fmt, ...)
{
    if (!getenv("REGISTRO")) return;
    va_list a;
    va_start(a, fmt);
    vprintf(fmt, a);
    va_end(a);
}

static bool Entorno(unsigned cmd, void* dato)
{
    switch (cmd)
    {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: formato = *(const unsigned*)dato; return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: ((retro_log_callback*)dato)->log = Registro; return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char**)dato = "pc/salida"; return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool*)dato = true; return true;
#ifdef RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE
    case RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE: *(unsigned*)dato = 48000; return true;   // la frecuencia de la PS4
#endif
    default: return false;
    }
}

static void Video(const void* datos, unsigned w, unsigned h, size_t pitch)
{
    if (!datos) return;
    ancho = w; alto = h;
    imagen.resize((size_t)w * h);
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < w; x++)
        {
            const uint8_t* fila = (const uint8_t*)datos + y * pitch;
            uint32_t c;
            if (formato == RETRO_PIXEL_FORMAT_XRGB8888) c = ((const uint32_t*)fila)[x] & 0xFFFFFF;
            else
            {
                uint16_t p = ((const uint16_t*)fila)[x];
                if (formato == RETRO_PIXEL_FORMAT_RGB565)
                    c = ((p >> 11) * 255 / 31) << 16 | (((p >> 5) & 63) * 255 / 63) << 8 | (p & 31) * 255 / 31;
                else
                    c = ((p >> 10 & 31) * 255 / 31) << 16 | ((p >> 5 & 31) * 255 / 31) << 8 | (p & 31) * 255 / 31;
            }
            imagen[(size_t)y * w + x] = c;
        }
}

static void Audio1(int16_t, int16_t) { muestras++; }
static size_t AudioLote(const int16_t*, size_t n) { muestras += (long)n; return n; }
static void Sondeo() {}
static int16_t Estado(unsigned puerto, unsigned disp, unsigned, unsigned id)
{
    if (puerto != 0 || disp != RETRO_DEVICE_JOYPAD) return 0;
    return (botones >> id) & 1;
}

static void Guardar(const char* nombre)
{
    std::vector<unsigned char> rgb((size_t)ancho * alto * 3);
    for (size_t i = 0; i < (size_t)ancho * alto; i++) { rgb[i * 3] = imagen[i] >> 16; rgb[i * 3 + 1] = imagen[i] >> 8; rgb[i * 3 + 2] = imagen[i]; }
    char r[300];
    snprintf(r, sizeof r, "%s/%s.png", Salida(), nombre);
    stbi_write_png(r, (int)ancho, (int)alto, 3, rgb.data(), (int)ancho * 3);
}

int main(int argc, char** argv)
{
    if (argc < 3) { printf("uso: probar_nucleo <rom> <cuadros> [jugar]\n"); return 1; }
    retro_set_environment(Entorno);
    retro_set_video_refresh(Video);
    retro_set_audio_sample(Audio1);
    retro_set_audio_sample_batch(AudioLote);
    retro_set_input_poll(Sondeo);
    retro_set_input_state(Estado);
    retro_init();
    retro_system_info si = {};
    retro_get_system_info(&si);
    printf("núcleo: %s %s (extensiones %s, ruta completa %d)\n", si.library_name, si.library_version, si.valid_extensions, si.need_fullpath);

    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("no se pudo abrir la rom\n"); return 1; }
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> rom((size_t)tam);
    fread(rom.data(), 1, (size_t)tam, f);
    fclose(f);
    retro_game_info gi = {argv[1], rom.data(), (size_t)tam, NULL};
    if (!retro_load_game(&gi)) { printf("el núcleo no cargó la rom\n"); return 1; }
    retro_system_av_info av = {};
    retro_get_system_av_info(&av);
    printf("video %ux%u (máx %ux%u), %.3f cuadros/s, audio %.0f Hz\n", av.geometry.base_width, av.geometry.base_height,
           av.geometry.max_width, av.geometry.max_height, av.timing.fps, av.timing.sample_rate);

    int cuadros = atoi(argv[2]);
    bool jugar = argc > 3;
    int capDesde = -1, capHasta = -1;
    if (getenv("CAPTURAS")) sscanf(getenv("CAPTURAS"), "%d,%d", &capDesde, &capHasta);
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < cuadros; i++)
    {
        botones = 0;
        if (jugar)
        {
            if (i % 180 < 6) botones |= 1 << RETRO_DEVICE_ID_JOYPAD_START;
            if (i > 1500) botones |= 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT | ((i / 40) % 2 ? 1 << RETRO_DEVICE_ID_JOYPAD_B : 0);
        }
        retro_run();
        if ((i % 300 == 299 || (i + 1 >= capDesde && i + 1 <= capHasta)) && ancho)
        {
            char n[32];
            snprintf(n, sizeof n, "juego_%04d", i + 1);
            Guardar(n);
        }
    }
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (ancho) Guardar("nucleo");
    printf("%d cuadros en %.2f s (%.0f/s), %ld muestras de audio\n", cuadros, s, cuadros / s, muestras);
    retro_unload_game();
    retro_deinit();
    return 0;
}
