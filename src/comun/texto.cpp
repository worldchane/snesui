// Ver texto.h. Cada tamaño de letra se rasteriza una vez al abrir en un atlas de 8 bits (cuánto cubre la letra cada
// píxel); al dibujar, cada píxel del atlas se mezcla con el color pedido.
#include "texto.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lienzo.h"

#ifndef RUTA_ASSETS
#define RUTA_ASSETS "/app0/assets"   // en el simulador de PC es la carpeta assets del proyecto
#endif

#define STBTT_STATIC
#define STBTT_assert(x) ((void)0)
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

// Caracteres: ASCII, Latin-1 (acentos, ñ, ¿¡, «», ×) y unos signos más.
static const int EXTRAS[] = {0x2026, 0x2022, 0x2013, 0x2014, 0x2190, 0x2191, 0x2192, 0x2193, 0x201C, 0x201D, 0x2018, 0x2019};
static const int N_EXTRAS = sizeof EXTRAS / sizeof EXTRAS[0];
static const int N_CARACTERES = 95 + 96 + N_EXTRAS;

struct FuenteCargada
{
    stbtt_packedchar car[N_CARACTERES];
    uint8_t* atlas;
    int aw, ah;
    float subida, alto;   // de la parte de arriba de la línea a la base de las letras; alto de la línea
};
static FuenteCargada fuentes[NUM_FUENTES];

static int Casilla(int cp)
{
    if (cp >= 32 && cp <= 126) return cp - 32;
    if (cp >= 160 && cp <= 255) return 95 + cp - 160;
    for (int i = 0; i < N_EXTRAS; i++)
        if (EXTRAS[i] == cp) return 191 + i;
    return '?' - 32;
}

// Lee un carácter UTF-8 y avanza; los bytes mal formados cuentan como '?'.
static int SiguienteCar(const char*& s, const char* fin)
{
    unsigned c = (unsigned char)*s++;
    if (c < 0x80) return (int)c;
    int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : -1;
    if (n < 0) return '?';
    int cp = (int)(c & (0x3F >> n));
    for (int i = 0; i < n; i++)
    {
        if (s >= fin || ((unsigned char)*s & 0xC0) != 0x80) return '?';
        cp = (cp << 6) | ((unsigned char)*s++ & 0x3F);
    }
    return cp;
}

static uint8_t* LeerArchivo(const char* ruta)
{
    FILE* f = fopen(ruta, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* d = (uint8_t*)malloc(n > 0 ? n : 1);
    if (d && fread(d, 1, n, f) != (size_t)n) { free(d); d = NULL; }
    fclose(f);
    return d;
}

static bool Rasterizar(FuenteCargada& fc, const uint8_t* ttf, float tam)
{
    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) return false;
    float esc = stbtt_ScaleForPixelHeight(&info, tam);
    int sube, baja, hueco;
    stbtt_GetFontVMetrics(&info, &sube, &baja, &hueco);
    fc.subida = sube * esc;
    fc.alto = (sube - baja) * esc;

    static int extras[N_EXTRAS];
    memcpy(extras, EXTRAS, sizeof extras);
    stbtt_pack_range rangos[3];
    memset(rangos, 0, sizeof rangos);
    rangos[0].font_size = tam; rangos[0].first_unicode_codepoint_in_range = 32; rangos[0].num_chars = 95; rangos[0].chardata_for_range = fc.car;
    rangos[1].font_size = tam; rangos[1].first_unicode_codepoint_in_range = 160; rangos[1].num_chars = 96; rangos[1].chardata_for_range = fc.car + 95;
    rangos[2].font_size = tam; rangos[2].array_of_unicode_codepoints = extras; rangos[2].num_chars = N_EXTRAS; rangos[2].chardata_for_range = fc.car + 191;

    // Atlas cada vez más grande hasta que entren todas las letras.
    for (int aw = 512, ah = 256; aw <= 4096; aw *= 2, ah *= 2)
    {
        uint8_t* atlas = (uint8_t*)calloc((size_t)aw * ah, 1);
        if (!atlas) return false;
        stbtt_pack_context ctx;
        if (stbtt_PackBegin(&ctx, atlas, aw, ah, 0, 1, NULL))
        {
            stbtt_PackSetOversampling(&ctx, 1, 1);
            int ok = stbtt_PackFontRanges(&ctx, ttf, 0, rangos, 3);
            stbtt_PackEnd(&ctx);
            if (ok) { fc.atlas = atlas; fc.aw = aw; fc.ah = ah; return true; }
        }
        free(atlas);
    }
    return false;
}

bool CargarFuentes()
{
    const char* archivos[4] = {"Inter-Regular.ttf", "Inter-Medium.ttf", "Inter-SemiBold.ttf", "Inter-Bold.ttf"};
    struct { int archivo; float tam; } def[NUM_FUENTES] = {
        {1, 20}, {0, 26}, {1, 26}, {2, 24}, {2, 32}, {3, 44}, {3, 64}, {3, 92}, {3, 128}, {3, 19},
    };
    uint8_t* datos[4] = {};
    char ruta[128];
    bool ok = true;
    for (int i = 0; i < 4; i++)
    {
        snprintf(ruta, sizeof ruta, "%s/%s", RUTA_ASSETS, archivos[i]);
        datos[i] = LeerArchivo(ruta);
    }
    for (int f = 0; f < NUM_FUENTES; f++)
        if (!datos[def[f].archivo] || !Rasterizar(fuentes[f], datos[def[f].archivo], def[f].tam)) ok = false;
    for (int i = 0; i < 4; i++) free(datos[i]);
    return ok;
}

float AltoLinea(int f) { return fuentes[f].alto; }

static float AnchoN(int f, const char* s, const char* fin)
{
    const FuenteCargada& fc = fuentes[f];
    if (!fc.atlas) return 0;
    float w = 0;
    while (s < fin && *s) w += fc.car[Casilla(SiguienteCar(s, fin))].xadvance;
    return w;
}

float AnchoTexto(int f, const char* s) { return AnchoN(f, s, s + strlen(s)); }

void Texto(int f, const char* s, float x, float y, uint32_t c, float a, int al)
{
    if (!fuentes[f].atlas || !*s) return;
    if (al) x -= AnchoTexto(f, s) * (al == 1 ? .5f : 1);
    OrdenTexto(f, s, (int)strlen(s), floorf(x + .5f), floorf(y + .5f), c, a);
}

float Parrafo(int f, const char* s, float x, float y, float ancho, uint32_t c, float a, float inter)
{
    float alto = AltoLinea(f) * inter, yy = y;
    char linea[512];
    while (*s)
    {
        // Toma palabras mientras quepan en la línea.
        const char* fin = s;
        const char* corte = NULL;
        for (;;)
        {
            const char* p = fin;
            while (*p == ' ') p++;
            const char* q = p;
            while (*q && *q != ' ' && *q != '\n') q++;
            if (AnchoN(f, s, q) > ancho && corte) break;
            fin = q;
            corte = q;
            if (!*q || *q == '\n') break;
        }
        int n = (int)(fin - s);
        if (n > (int)sizeof linea - 1) n = sizeof linea - 1;
        memcpy(linea, s, n);
        linea[n] = 0;
        Texto(f, linea, x, yy, c, a);
        yy += alto;
        s = fin;
        if (*s == '\n') s++;
        while (*s == ' ') s++;
    }
    return yy - y;
}

const char* Acortar(int f, const char* s, float ancho, char* buf, int max)
{
    if (AnchoTexto(f, s) <= ancho) return s;
    float puntos = AnchoTexto(f, "…");
    int n = (int)strlen(s);
    if (n > max - 4) n = max - 4;
    while (n > 0)
    {
        // Retrocede sin partir un carácter UTF-8.
        n--;
        while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
        if (AnchoN(f, s, s + n) + puntos <= ancho) break;
    }
    while (n > 0 && s[n - 1] == ' ') n--;
    memcpy(buf, s, n);
    memcpy(buf + n, "…", 4);   // "…" ocupa 3 bytes + el 0
    return buf;
}

static inline uint32_t Mezcla(uint32_t d, uint32_t s, uint32_t a)
{
    uint32_t rb = (((s & 0xff00ff) * a + (d & 0xff00ff) * (256 - a)) >> 8) & 0xff00ff;
    uint32_t g = (((s & 0x00ff00) * a + (d & 0x00ff00) * (256 - a)) >> 8) & 0x00ff00;
    return rb | g | 0xff000000;
}

void PintarTexto(const FranjaTexto& fr, int f, const char* s, int n, float x, float y, uint32_t c, int alfa)
{
    const FuenteCargada& fc = fuentes[f];
    const char* fin = s + n;
    float pluma = x, base = y + fc.subida;
    while (s < fin)
    {
        const stbtt_packedchar& g = fc.car[Casilla(SiguienteCar(s, fin))];
        int gx = (int)floorf(pluma + g.xoff + .5f), gy = (int)floorf(base + g.yoff + .5f);
        int gw = g.x1 - g.x0, gh = g.y1 - g.y0;
        pluma += g.xadvance;
        int y0 = gy > fr.y0 ? gy : fr.y0, y1 = gy + gh < fr.y1 ? gy + gh : fr.y1;
        int x0 = gx > fr.x0 ? gx : fr.x0, x1 = gx + gw < fr.x1 ? gx + gw : fr.x1;
        for (int py = y0; py < y1; py++)
        {
            const uint8_t* cob = fc.atlas + (size_t)(g.y0 + py - gy) * fc.aw + g.x0 - gx;
            uint32_t* fila = fr.px + (size_t)py * fr.pitch;
            for (int px = x0; px < x1; px++)
            {
                uint32_t k = cob[px];
                if (!k) continue;
                uint32_t a = (k * (uint32_t)alfa + 128) >> 8;
                fila[px] = Mezcla(fila[px], c, a + (a >> 7));
            }
        }
    }
}
