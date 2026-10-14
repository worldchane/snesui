// Ver estilo.h.
#include "estilo.h"

#include <math.h>

#include "lienzo.h"
#include "plataforma.h"
#include "texto.h"

void Suave(float& v, float objetivo, float rapidez, float dt)
{
    v += (objetivo - v) * (1 - expf(-rapidez * dt));
    if (fabsf(objetivo - v) < 1e-4f) v = objetivo;
}

float Facilidad(float t)
{
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return 1 - powf(1 - t, 3);   // sale rápido y frena al llegar
}

float DibujarBoton(int b, float cx, float cy, float a)
{
    if (b == BM_L1 || b == BM_R1 || b == BM_OPTIONS)
    {
        const char* t = b == BM_L1 ? "L1" : b == BM_R1 ? "R1" : "OPTIONS";
        float w = AnchoTexto(F_BOTON, t) + 22, h = 30;
        Caja(cx - w / 2, cy - h / 2, w, h, 9, 0xFFFFFF, .14f * a);
        Texto(F_BOTON, t, cx, cy - AltoLinea(F_BOTON) / 2, C_TEXTO, a, 1);
        return w;
    }
    const float r = 17;
    Circulo(cx, cy, r, 0xFFFFFF, .13f * a);
    switch (b)
    {
    case BM_CRUZ:
        Linea(cx - 6.5f, cy - 6.5f, cx + 6.5f, cy + 6.5f, 2.6f, 0x86B4FF, a);
        Linea(cx - 6.5f, cy + 6.5f, cx + 6.5f, cy - 6.5f, 2.6f, 0x86B4FF, a);
        break;
    case BM_CIRCULO: Aro(cx, cy, 7.5f, 2.6f, 0xFF7C8A, a); break;
    case BM_CUADRADO: Borde(cx - 7, cy - 7, 14, 14, 1.5f, 2.6f, 0xF59BDD, a); break;
    case BM_TRIANGULO:
    {
        float x0 = cx - 8, x1 = cx + 8, yb = cy + 5.5f, yt = cy - 8;
        Linea(x0, yb, x1, yb, 2.6f, 0x6FE6BE, a);
        Linea(x0, yb, cx, yt, 2.6f, 0x6FE6BE, a);
        Linea(x1, yb, cx, yt, 2.6f, 0x6FE6BE, a);
        break;
    }
    }
    return 2 * r;
}

float Pista(int b, const char* texto, float x, float cy, float a, bool medir)
{
    float wb = b == BM_L1 || b == BM_R1 || b == BM_OPTIONS ? AnchoTexto(F_BOTON, b == BM_OPTIONS ? "OPTIONS" : "L1") + 22 : 34;
    if (!medir)
    {
        DibujarBoton(b, x + wb / 2, cy, a);
        Texto(F_CUERPO_M, texto, x + wb + 12, cy - AltoLinea(F_CUERPO_M) / 2, C_TEXTO_2, a);
    }
    return wb + 12 + AnchoTexto(F_CUERPO_M, texto);
}

void Pistas(const int* botones, const char* const* textos, int n, float a)
{
    const float sep = 40, cy = 1012;
    float total = 0;
    for (int i = 0; i < n; i++) total += Pista(botones[i], textos[i], 0, 0, a, true) + (i ? sep : 0);
    float x = ANCHO - MARGEN - total;
    for (int i = 0; i < n; i++)
    {
        x += Pista(botones[i], textos[i], x, cy, a) + sep;
    }
}

float Pastilla(const char* texto, float x, float y, uint32_t fondo, float alfaFondo, uint32_t color, float a)
{
    float w = AnchoTexto(F_PEQUENA, texto) + 28, h = 36;
    Caja(x, y, w, h, h / 2, fondo, alfaFondo * a);
    Texto(F_PEQUENA, texto, x + 14, y + (h - AltoLinea(F_PEQUENA)) / 2, color, a);
    return w;
}

// El fondo se calcula en una imagen de 480 × 270 (repartida entre los núcleos) y se agranda a la pantalla con filtro:
// degradado vertical, tres luces grandes que se mueven lentamente y una viñeta arriba y abajo.
struct Luz { float x, y, r2inv; float c[3]; float a; };
struct DatosFondo { Luz luces[3]; float arriba[3], abajo[3]; };
static uint32_t fondo[FONDO_W * FONDO_H];

static void Componentes(uint32_t c, float* f) { f[0] = (c >> 16 & 255); f[1] = (c >> 8 & 255); f[2] = (c & 255); }

static void PintarFondoParte(int parte, int partes, void* arg)
{
    const DatosFondo& d = *(const DatosFondo*)arg;
    int y0 = FONDO_H * parte / partes, y1 = FONDO_H * (parte + 1) / partes;
    for (int y = y0; y < y1; y++)
    {
        float ty = (y + .5f) / FONDO_H;
        float base[3];
        for (int k = 0; k < 3; k++) base[k] = d.arriba[k] + (d.abajo[k] - d.arriba[k]) * ty;
        // Viñeta: oscurece el 17 % de arriba y el 24 % de abajo.
        float vin = ty < .17f ? .35f * (1 - ty / .17f) : ty > .76f ? .6f * (ty - .76f) / .24f : 0;
        uint32_t* fila = fondo + y * FONDO_W;
        for (int x = 0; x < FONDO_W; x++)
        {
            float c[3] = {base[0], base[1], base[2]};
            for (int l = 0; l < 3; l++)
            {
                const Luz& L = d.luces[l];
                float dx = x + .5f - L.x, dy = y + .5f - L.y;
                float t = 1 - (dx * dx + dy * dy) * L.r2inv;
                if (t <= 0) continue;
                float w = t * t * L.a;
                for (int k = 0; k < 3; k++) c[k] += (L.c[k] - c[k]) * w;
            }
            uint32_t v = 0;
            for (int k = 0; k < 3; k++)
            {
                float f = c[k] * (1 - vin) + .5f;
                v = v << 8 | (uint32_t)(f > 255 ? 255 : f);
            }
            fila[x] = v;
        }
    }
}

void FondoAnimado(float t, bool animado)
{
    if (!animado) t = 0;
    DatosFondo d;
    Componentes(C_FONDO_ARRIBA, d.arriba);
    Componentes(C_FONDO_ABAJO, d.abajo);
    // Posición (en la pantalla de 1920 × 1080), radio, color y fuerza de cada luz; cada una con su propio ritmo.
    const float luces[3][5] = {
        {420 + 260 * sinf(t * .11f), 260 + 140 * cosf(t * .09f), 820, (float)0x6A2DBF, .22f},
        {1560 + 220 * cosf(t * .08f + 1), 860 + 120 * sinf(t * .13f), 760, (float)0x00A89A, .12f},
        {1180 + 300 * sinf(t * .06f + 2), 160 + 90 * cosf(t * .1f + 1), 640, (float)0x9B3FD0, .13f},
    };
    for (int i = 0; i < 3; i++)
    {
        Luz& L = d.luces[i];
        L.x = luces[i][0] / 4; L.y = luces[i][1] / 4;
        float r = luces[i][2] / 4;
        L.r2inv = 1 / (r * r);
        Componentes((uint32_t)luces[i][3], L.c);
        L.a = luces[i][4];
    }
    EnParalelo(PintarFondoParte, &d);
    FondoReducido(fondo);
}
