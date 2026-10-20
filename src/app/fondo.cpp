// Ver fondo.h. Todo se calcula con funciones (senos y un hash), sin imágenes: el paisaje se desplaza sin fin y sin
// costuras. Las posiciones de lo que se mueve se redondean a píxeles enteros para que no "tiemble" al agrandarse.
#include "fondo.h"

#include <math.h>
#include <stdint.h>

#include "../comun/lienzo.h"
#include "../comun/tema.h"

static const int FW = 320, FH = 180;
static uint32_t px[FW * FH];

static inline uint32_t Hash(uint32_t x)
{
    x ^= x >> 16; x *= 0x7FEB352D; x ^= x >> 15; x *= 0x846CA68B; x ^= x >> 16;
    return x;
}

static inline uint32_t Mezcla(uint32_t a, uint32_t b, float f)
{
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    int r = (int)(((a >> 16) & 255) + (((int)((b >> 16) & 255) - (int)((a >> 16) & 255)) * f));
    int g = (int)(((a >> 8) & 255) + (((int)((b >> 8) & 255) - (int)((a >> 8) & 255)) * f));
    int c = (int)((a & 255) + (((int)(b & 255) - (int)(a & 255)) * f));
    return (uint32_t)r << 16 | g << 8 | c;
}

static inline void Punto(int x, int y, uint32_t c)
{
    if (x >= 0 && x < FW && y >= 0 && y < FH) px[y * FW + x] = c;
}

// Trama ordenada 4 × 4 (Bayer): reparte dos colores vecinos en un patrón fijo, el degradado típico del pixel art.
static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// Paleta del cielo, de arriba hacia el horizonte (violetas de la interfaz que terminan en un rosa de atardecer).
static const int N_CIELO = 9;
static uint32_t cielo[N_CIELO];
static bool listo;

static void Preparar()
{
    static const uint32_t PARADAS[4] = {0x0A0518, 0x1C0E3A, 0x3A1A63, 0x7A2E7E};
    for (int i = 0; i < N_CIELO; i++)
    {
        float f = (float)i / (N_CIELO - 1) * 3;
        int k = (int)f;
        if (k > 2) k = 2;
        cielo[i] = Mezcla(PARADAS[k], PARADAS[k + 1], f - k);
    }
    listo = true;
}

static const int HORIZONTE = 132;

// Altura (fila) de las montañas lejanas y de las colinas para una columna del mundo.
static inline int Montana(float wx)
{
    return (int)(98 - 16 * sinf(wx * .021f) - 9 * sinf(wx * .057f + 1.3f) - 4 * sinf(wx * .13f + 2.1f));
}
static inline int Colina(float wx)
{
    return (int)(130 - 7 * sinf(wx * .041f + .5f) - 4 * sinf(wx * .097f + 2.0f));
}

static void Paisaje(float t)
{
    if (!listo) Preparar();

    // Cielo con degradado tramado y un resplandor que respira suave en el horizonte.
    float respiro = .5f + .5f * sinf(t * .35f);
    for (int y = 0; y < FH; y++)
    {
        float f = (float)y / HORIZONTE;
        if (f > 1) f = 1;
        f = f * f * (N_CIELO - 1.2f) + respiro * .5f * f * f * f;
        for (int x = 0; x < FW; x++)
        {
            float u = f + (BAYER[y & 3][x & 3] / 16.f - .5f);
            int i = (int)(u + .5f);
            if (i < 0) i = 0;
            if (i >= N_CIELO) i = N_CIELO - 1;
            px[y * FW + x] = cielo[i];
        }
    }

    // Estrellas: fijas, cada una titila a su ritmo; algunas grandes con forma de cruz.
    for (uint32_t k = 0; k < 110; k++)
    {
        uint32_t h = Hash(k * 2654435761u + 17);
        int x = (int)(h % FW), y = (int)((h >> 9) % 112);
        float brillo = .55f + .45f * sinf(t * (1.2f + (h >> 20 & 7) * .35f) + (h & 255) * .1f);
        uint32_t base = (h >> 13 & 3) == 0 ? 0xFFE9B8 : (h >> 13 & 3) == 1 ? 0xB8D4FF : 0xFFFFFF;
        uint32_t fondoAqui = px[y * FW + x];
        if (brillo < .25f) continue;
        Punto(x, y, Mezcla(fondoAqui, base, brillo));
        if (k % 13 == 0 && brillo > .75f)
        {
            uint32_t c = Mezcla(fondoAqui, base, (brillo - .75f) * 2.4f);
            Punto(x - 1, y, c); Punto(x + 1, y, c); Punto(x, y - 1, c); Punto(x, y + 1, c);
        }
    }

    // Aurora boreal: cortinas de luz que ondulan. Cada columna tiene su centro (que se mueve con dos ondas) y "rayos"
    // verticales que corren de lado a lado. Se dibuja con trama para mantener el aspecto de pixel art.
    for (int x = 0; x < FW; x++)
    {
        float centro = 38 + 8 * sinf(x * .028f + t * .55f) + 4 * sinf(x * .071f - t * .9f);
        float rayo = .55f + .45f * sinf(x * .33f + t * 2.2f + 3 * sinf(x * .045f + t * .3f));
        float onda = .6f + .4f * sinf(x * .012f - t * .4f);
        for (int y = 2; y < 78; y++)
        {
            float d = (y - centro) / (y < centro ? 7.f : 16.f);   // borde de arriba nítido, cola larga hacia abajo
            float fuerza = (1 - d * d) * rayo * onda;
            if (fuerza <= 0) continue;
            // Trama solo en el borde tenue; donde es fuerte, luz continua.
            if (fuerza < .35f && fuerza * 46 <= BAYER[y & 3][x & 3]) continue;
            // Más suave cerca de los textos de la cabecera (arriba a la izquierda y a la derecha).
            float cabecera = (y < 26 && (x < 70 || x > 270)) ? .35f : 1.f;
            float tono = .5f + .5f * sinf(x * .02f + t * .25f);   // de verde a cian a violeta a lo largo del cielo
            uint32_t c = y < centro ? Mezcla(0x6CFFD2, 0x9F7BFF, tono) : Mezcla(0x2EE6A0, 0x6A4BFF, tono);
            float mezcla = (fuerza < .35f ? .22f : .14f + .26f * fuerza) * cabecera;
            px[y * FW + x] = Mezcla(px[y * FW + x], c, mezcla);
        }
    }

    // Luna con halo tramado y cráteres.
    const int lx = 200, ly = 19, lr = 12;   // entre las pestañas y el contador, sin tapar textos
    for (int y = ly - lr - 8; y <= ly + lr + 8; y++)
        for (int x = lx - lr - 8; x <= lx + lr + 8; x++)
        {
            float d = sqrtf((float)((x - lx) * (x - lx) + (y - ly) * (y - ly)));
            if (x < 0 || x >= FW || y < 0 || y >= FH) continue;
            if (d <= lr)
            {
                bool sombra = (x - lx) + (y - ly) * .4f > lr * .45f;
                Punto(x, y, sombra ? 0xC6B2EE : 0xF3EAFF);
            }
            else if (d <= lr + 8)
            {
                float fuerza = (1 - (d - lr) / 8) * (.75f + .25f * sinf(t * 1.6f));   // halo que late
                if (fuerza * 16 > BAYER[y & 3][x & 3] + 2) Punto(x, y, Mezcla(px[y * FW + x], 0xB79CFF, .28f));
            }
        }
    static const int CRATER[4][3] = {{-4, -3, 2}, {3, 2, 3}, {-2, 5, 1}, {5, -5, 1}};
    for (auto& c : CRATER)
        for (int y = -c[2]; y <= c[2]; y++)
            for (int x = -c[2]; x <= c[2]; x++)
                if (x * x + y * y <= c[2] * c[2]) Punto(lx + c[0] + x, ly + c[1] + y, 0xD4C2F5);

    // Estrellas fugaces: dos carriles desfasados, una cada 3,5 s en cada uno, en un lugar distinto cada vez.
    for (int carril = 0; carril < 2; carril++)
    {
        float tc = t + carril * 1.7f;
        float ciclo = tc / 3.5f;
        uint32_t n = (uint32_t)ciclo + carril * 100003u;
        float p = (ciclo - (uint32_t)ciclo) * 3.5f;
        if (p < .9f)
        {
            uint32_t h = Hash(n * 7919u + 3);
            float x0 = 40 + (float)(h % 200), y0 = 8 + (float)((h >> 8) % 40);
            float avance = p / .9f;
            float cx = x0 + avance * 90, cy = y0 + avance * 38;
            for (int k = 0; k < 14; k++)
            {
                float f = 1 - k / 14.f;
                int x = (int)(cx - k * 2.1f), y = (int)(cy - k * .9f);
                if (x >= 0 && x < FW && y >= 0 && y < FH) Punto(x, y, Mezcla(px[y * FW + x], 0xFFFFFF, f * (1 - avance * .6f)));
            }
        }
    }

    // Nubes: cada una con su propia forma (varias "bolas" de distinto tamaño sobre una base plana), iluminada desde
    // arriba por la luna (borde claro arriba, sombra abajo), con los bordes tramados para que se fundan con el cielo.
    // Dos capas: las lejanas, chicas, tenues y lentas; las cercanas, grandes y más rápidas.
    for (int k = 0; k < 7; k++)
    {
        bool lejana = k < 4;
        uint32_t h = Hash(k * 7349u + 11);
        float escala = lejana ? .6f + (h >> 24 & 3) * .08f : 1.f + (h >> 24 & 3) * .12f;
        float vel = lejana ? 2.5f + (h >> 20 & 3) * .6f : 6.f + (h >> 20 & 3) * 1.2f;
        int bolas = 4 + (int)(h >> 16 & 3);
        float largo = (18 + bolas * 9) * escala;
        float recorrido = FW + largo + 40;
        int baseX = (int)(fmodf((float)(h % 997) * 3.1f + t * vel, recorrido) - largo - 20);
        int baseY = (lejana ? 40 : 58) + (int)((h >> 8) % 26);
        float alfa = lejana ? .55f : .9f;
        // Las bolas: más grandes en el medio, más chicas en las puntas.
        float bx[8], by[8], br[8];
        for (int b = 0; b < bolas; b++)
        {
            float u = (b + .5f) / bolas;                    // 0..1 a lo largo de la nube
            float r = (5 + 7 * sinf(u * 3.14159f) + (Hash(h + b * 13u) % 4)) * escala;
            br[b] = r;
            bx[b] = baseX + u * largo;
            by[b] = baseY - r * .55f;
        }
        int x0 = baseX - 14, x1 = (int)(baseX + largo + 14), y0 = baseY - (int)(16 * escala) - 6, y1 = baseY;
        for (int y = y0; y <= y1; y++)
        {
            if (y < 0 || y >= FH) continue;
            for (int x = x0; x <= x1; x++)
            {
                if (x < 0 || x >= FW) continue;
                // Cuánto adentro está el píxel (0 = borde, 1 = centro de una bola) y en qué parte de su bola.
                float mejor = -1, alto = 0;
                for (int b = 0; b < bolas; b++)
                {
                    float dx = (x - bx[b]) / br[b], dy = (y - by[b]) / br[b];
                    float f = 1 - sqrtf(dx * dx + dy * dy);
                    if (f > mejor) { mejor = f; alto = dy; }
                }
                if (mejor <= 0) continue;
                // Borde tramado: cerca del contorno solo algunos píxeles (se funde con el cielo).
                if (mejor < .18f && mejor * 90 < BAYER[y & 3][x & 3]) continue;
                uint32_t c;
                if (y >= baseY - 1) c = lejana ? 0x2B1A52 : 0x34205F;                 // base en sombra
                else if (alto < -.45f) c = lejana ? 0x6E58A8 : 0xA48BE0;              // luz de arriba
                else if (alto < -.05f) c = lejana ? 0x4E3B85 : 0x7860B8;              // tono medio
                else c = lejana ? 0x3A2A6A : 0x523C8A;                                 // sombra de abajo
                // Entre tono y tono, una línea tramada para que el cambio no sea un escalón.
                if (alto > -.5f && alto < -.4f && (BAYER[y & 3][x & 3] & 1)) c = Mezcla(c, lejana ? 0x4E3B85 : 0x7860B8, .5f);
                px[y * FW + x] = Mezcla(px[y * FW + x], c, alfa);
            }
        }
    }

    // Montañas lejanas, colinas y pinos: cada capa se desplaza a otra velocidad (profundidad).
    int o1 = (int)(t * 6), o2 = (int)(t * 15), o3 = (int)(t * 30);
    for (int x = 0; x < FW; x++)
    {
        int m = Montana((float)(x + o1));
        for (int y = m; y < FH; y++) Punto(x, y, y == m ? 0x4A2A78 : (y < m + 3 && ((x + y) & 1)) ? 0x35205E : 0x26144A);
        int c = Colina((float)(x + o2));
        for (int y = c; y < FH; y++) Punto(x, y, y == c ? 0x341A5E : 0x1A0D33);
    }
    // Pinos (un pino cada 11 columnas del mundo, de altura variable) y el suelo.
    const int SUELO = 156;
    for (int x = 0; x < FW; x++)
    {
        int wx = x + o3;
        for (int d = -1; d <= 1; d++)
        {
            int idx = wx / 11 + d;
            uint32_t h = Hash((uint32_t)idx * 31u + 5);
            if ((h & 3) == 0) continue;   // algunos huecos
            int centro = idx * 11 + 5 + (int)(h >> 4 & 3) - 1;
            int alto = 16 + (int)((h >> 8) & 15);
            int arriba = SUELO - alto;
            int dx = wx - centro;
            if (dx < -6 || dx > 6) continue;
            for (int y = arriba; y < SUELO; y++)
            {
                int dd = y - arriba;
                int medio = dd >= alto - 3 ? 0 : 1 + dd / 5 + (dd % 5) / 2;   // pisos del pino; tronco abajo
                if (medio > 6) medio = 6;
                if (dx >= -medio && dx <= medio) Punto(x, y, 0x0E0720);
            }
        }
        for (int y = SUELO; y < FH; y++) Punto(x, y, y == SUELO ? 0x1B0E34 : 0x0B0518);
    }

    // Luciérnagas: puntitos verdes que suben y bajan con un brillo que late.
    for (int k = 0; k < 16; k++)
    {
        uint32_t h = Hash(k * 977u + 41);
        float fx = fmodf((float)(h % FW) + t * (3 + (h >> 10 & 7)) + 8 * sinf(t * .7f + k), (float)FW);
        float fy = 128 + (float)((h >> 16) % 40) + 6 * sinf(t * (.9f + (h >> 4 & 3) * .3f) + k);
        float b = .5f + .5f * sinf(t * (2.f + (h >> 7 & 3)) + k * 1.7f);
        int x = (int)fx, y = (int)fy;
        if (x < 0 || x >= FW || y < 0 || y >= FH) continue;
        Punto(x, y, Mezcla(px[y * FW + x], 0xD8FF8A, .35f + .65f * b));
        if (b > .6f)
        {
            uint32_t g = 0x6F9A3C;
            for (int d = -1; d <= 1; d += 2)
            {
                if (x + d >= 0 && x + d < FW) Punto(x + d, y, Mezcla(px[y * FW + x + d], g, (b - .6f) * 1.5f));
                if (y + d >= 0 && y + d < FH) Punto(x, y + d, Mezcla(px[(y + d) * FW + x], g, (b - .6f) * 1.5f));
            }
        }
    }

    // Bandadas de pájaros que cruzan el cielo aleteando (dos cuadros: alas arriba / alas abajo), cada 9 s.
    {
        float ciclo = t / 9.f;
        uint32_t n = (uint32_t)ciclo;
        float p = (ciclo - n) * 9.f;
        uint32_t h = Hash(n * 31337u + 7);
        bool haciaDerecha = h & 1;
        float avance = p / 9.f * (FW + 80);
        int baseX = haciaDerecha ? (int)avance - 40 : FW + 40 - (int)avance;
        int baseY = 26 + (int)((h >> 4) % 40);
        int cuantos = 3 + (int)((h >> 9) % 4);
        for (int b = 0; b < cuantos; b++)
        {
            // En V: el primero adelante, los demás atrás y a los costados.
            int fila = (b + 1) / 2, lado = (b & 1) ? 1 : -1;
            int bx = baseX - (haciaDerecha ? 1 : -1) * fila * 9;
            int by = baseY + lado * fila * 5 + (int)(2 * sinf(t * 3 + b));
            bool alasArriba = ((int)(t * 6 + b * 1.3f) & 1) == 0;
            uint32_t c = 0x140A2A;
            Punto(bx, by, c);
            Punto(bx - 1, by + (alasArriba ? -1 : 1), c); Punto(bx + 1, by + (alasArriba ? -1 : 1), c);
            Punto(bx - 2, by + (alasArriba ? -2 : 1), c); Punto(bx + 2, by + (alasArriba ? -2 : 1), c);
        }
    }

    // Chispas de colores que suben por toda la pantalla, titilan y se apagan al llegar arriba.
    for (int k = 0; k < 34; k++)
    {
        uint32_t h = Hash(k * 4099u + 77);
        float vel = 6 + (float)(h % 9);
        float recorrido = FH + 20.f;
        float y = FH + 10 - fmodf(t * vel + (float)((h >> 8) % 200), recorrido);
        float x = (float)((h >> 16) % FW) + 6 * sinf(t * (.8f + (h >> 3 & 3) * .25f) + k);
        float vida = y / FH;   // se apagan arriba
        float b = (.5f + .5f * sinf(t * (4.f + (h >> 5 & 3)) + k)) * vida;
        if (b < .15f) continue;
        static const uint32_t COLORES[4] = {0xFF7BD5, 0x7BE4FF, 0xFFD27B, 0xB59BFF};
        uint32_t c = COLORES[h >> 12 & 3];
        int X = (int)x, Y = (int)y;
        if (X < 0 || X >= FW || Y < 0 || Y >= FH) continue;
        Punto(X, Y, Mezcla(px[Y * FW + X], c, b));
        if (b > .7f && (h & 3) == 0)   // algunas destellan en cruz
        {
            float f = (b - .7f) * 2.5f;
            if (X > 0) Punto(X - 1, Y, Mezcla(px[Y * FW + X - 1], c, f));
            if (X < FW - 1) Punto(X + 1, Y, Mezcla(px[Y * FW + X + 1], c, f));
            if (Y > 0) Punto(X, Y - 1, Mezcla(px[(Y - 1) * FW + X], c, f));
            if (Y < FH - 1) Punto(X, Y + 1, Mezcla(px[(Y + 1) * FW + X], c, f));
        }
    }

}


// ---------- fondos de los estilos de interfaz (tema.h) ----------
// Los suaves se calculan a 240 × 135 y se agrandan con filtro bilineal (se ven igual de suaves que a resolución completa);
// los de pixel art, a 320 × 180 al píxel más cercano, como el paisaje.
static const int SW = 240, SH = 135;
static uint32_t suave[SW * SH];
static uint32_t borroso[SW * SH];   // el fondo desenfocado, para los paneles de vidrio

// Seno con tabla (los fondos piden cientos de miles por cuadro): fase en vueltas (1 = 2π).
static float tablaSeno[1024];
static inline float Seno(float vueltas)
{
    if (tablaSeno[256] == 0) for (int i = 0; i < 1024; i++) tablaSeno[i] = sinf(i * 6.2831853f / 1024);
    int k = (int)floorf(vueltas * 1024) & 1023;
    return tablaSeno[k];
}

// Color de una paleta de 4 paradas para f en [0, 1].
static inline uint32_t Paleta(const uint32_t* p, float f)
{
    if (f <= 0) return p[0];
    if (f >= 1) return p[3];
    float x = f * 3;
    int k = (int)x;
    return Mezcla(p[k], p[k + 1], x - k);
}

static inline float Luz(uint32_t c) { return (((c >> 16) & 255) * .30f + ((c >> 8) & 255) * .59f + (c & 255) * .11f) / 255; }

static void Aurora(float t, const uint32_t* p)
{
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
        {
            float fy = (float)y / SH, fx = (float)x / SW;
            uint32_t c = Mezcla(p[0], p[1], fy * .9f);
            // Tres bandas que ondulan; brillan más arriba y se desvanecen hacia el horizonte.
            for (int b = 0; b < 3; b++)
            {
                float centro = .22f + b * .1f + .06f * Seno(fx * (1.2f + b * .4f) + t * (.04f + b * .015f)) + .03f * Seno(fx * 3.1f - t * .07f);
                float d = fabsf(fy - centro) / (.05f + .03f * b);
                if (d < 1) c = Mezcla(c, Mezcla(p[2], p[3], b * .45f + .2f * Seno(fx + t * .05f)), (1 - d) * (1 - d) * .8f);
            }
            suave[y * SW + x] = c;
        }
    for (int i = 0; i < 60; i++)   // estrellas
    {
        uint32_t h = Hash(i * 977);
        int x = h % SW, y = (h >> 9) % (SH / 2);
        float b = .5f + .5f * Seno(t * .3f + (h & 255) / 256.f);
        suave[y * SW + x] = Mezcla(suave[y * SW + x], 0xFFFFFF, b * .8f);
    }
}

static void Rejilla(float t, const uint32_t* p)
{
    const int horizonte = SH * 55 / 100;
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
        {
            uint32_t c;
            if (y < horizonte)
            {
                float fy = (float)y / horizonte;
                c = Mezcla(p[0], Mezcla(p[1], p[0], .55f), fy);
                // Sol con franjas.
                float dx = x - SW / 2.f, dy = y - horizonte * .72f, r = SH * .26f;
                if (dx * dx + dy * dy < r * r)
                {
                    float f = (dy + r) / (2 * r);
                    bool franja = dy > 0 && ((int)(dy * 1.2f + t * 6) % 6) < (int)(f * 5);
                    if (!franja) c = Mezcla(p[3], p[1], f);
                }
            }
            else
            {
                float z = (float)(y - horizonte + 1) / (SH - horizonte);
                c = Mezcla(p[0], Mezcla(p[0], p[1], .25f), z);
                // Líneas: horizontales que se acercan y verticales que se abren hacia abajo.
                float fz = 2 / z + t * 1.5f;
                float lh = fz - floorf(fz);
                float xv = (x - SW / 2.f) / z / SW * 10;
                float lv = xv - floorf(xv);
                if (lh < .25f * z || lv < .06f || lv > .94f) c = Mezcla(c, p[2], .55f + .45f * z);
            }
            suave[y * SW + x] = c;
        }
}

static void Espacio(float t, const uint32_t* p)
{
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
        {
            float fx = (float)x / SW, fy = (float)y / SH;
            // Nebulosa: dos ondas cruzadas.
            float n = .5f + .25f * Seno(fx * 1.3f + fy * .7f + t * .01f) + .25f * Seno(fy * 1.9f - fx * .5f - t * .013f);
            n = n * n * n;
            suave[y * SW + x] = Mezcla(p[0], Paleta(p, .2f + n * .55f), n * .7f);
        }
    // Estrellas que vienen desde el centro.
    for (int i = 0; i < 180; i++)
    {
        uint32_t h = Hash(i * 7919 + 13);
        float ang = (h & 1023) / 1024.f, vel = .05f + ((h >> 10) & 255) / 255.f * .1f;
        float z = fmodf(t * vel + ((h >> 18) & 1023) / 1024.f, 1.f);
        float r = z * z * SW * .75f;
        int x = (int)(SW / 2 + r * Seno(ang + .25f)), y = (int)(SH / 2 + r * Seno(ang) * .75f);
        if (x < 0 || x >= SW || y < 0 || y >= SH) continue;
        suave[y * SW + x] = Mezcla(suave[y * SW + x], p[3], .3f + z * .7f);
    }
}

static void Olas(float t, const uint32_t* p)
{
    for (int y = 0; y < SH; y++)
    {
        float fy = (float)y / SH;
        for (int x = 0; x < SW; x++) suave[y * SW + x] = Mezcla(p[0], p[1], fy * .6f);
    }
    for (int capa = 0; capa < 5; capa++)
    {
        float base = SH * (.45f + capa * .12f), amp = 6 + capa * 2.5f, vel = .02f + capa * .012f;
        uint32_t c = Paleta(p, .25f + capa * .17f);
        for (int x = 0; x < SW; x++)
        {
            float fx = (float)x / SW;
            int alto = (int)(base + amp * Seno(fx * (1.1f + capa * .3f) + t * vel) + amp * .5f * Seno(fx * 2.7f - t * vel * 1.3f + capa));
            if (alto < 0) alto = 0;
            for (int y = alto; y < SH; y++) suave[y * SW + x] = y - alto < 2 ? Mezcla(c, p[3], .5f) : c;
        }
    }
}

static void Burbujas(float t, const uint32_t* p)
{
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++) suave[y * SW + x] = Mezcla(p[0], p[1], (float)y / SH);
    for (int i = 0; i < 26; i++)
    {
        uint32_t h = Hash(i * 3571 + 7);
        float r = 4 + (h & 15) * 1.2f;
        float vel = .015f + ((h >> 4) & 15) / 15.f * .03f;
        float cx = (h >> 8) % SW + 8 * Seno(t * .05f + i * .3f);
        float cy = SH + r - fmodf(t * vel * SH + ((h >> 16) & 255), SH + 2 * r);
        uint32_t c = (h >> 24) & 1 ? p[2] : p[3];
        int x0 = (int)(cx - r), x1 = (int)(cx + r), y0 = (int)(cy - r), y1 = (int)(cy + r);
        for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < SH; y++)
            for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < SW; x++)
            {
                float dx = x - cx, dy = y - cy, d = sqrtf(dx * dx + dy * dy) / r;
                if (d < 1) suave[y * SW + x] = Mezcla(suave[y * SW + x], c, (1 - d) * .55f + (d > .8f ? .25f : 0));
            }
    }
}

static void Manchas(float t, const uint32_t* p)
{
    // Cuatro manchas grandes de los colores del tema que se mueven despacio sobre una mezcla de ellas.
    float mx[4], my[4];
    for (int k = 0; k < 4; k++)
    {
        mx[k] = SW * (.5f + .38f * Seno(t * (.011f + k * .004f) + k * .27f));
        my[k] = SH * (.5f + .36f * Seno(t * (.009f + k * .005f) + k * .41f + .25f));
    }
    uint32_t base = Mezcla(Mezcla(p[0], p[1], .5f), Mezcla(p[2], p[3], .5f), .5f);
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
        {
            uint32_t c = base;
            for (int k = 0; k < 4; k++)
            {
                float dx = (x - mx[k]) / SW, dy = (y - my[k]) / SH;
                float f = 1 - (dx * dx + dy * dy) * 4.5f;
                if (f > 0) c = Mezcla(c, p[k], f * f * .9f);
            }
            suave[y * SW + x] = c;
        }
}

static void Lluvia(float t)
{
    const uint32_t* p = temaActual->f;
    for (int i = 0; i < FW * FH; i++) px[i] = p[0];
    for (int x = 0; x < FW; x += 4)
    {
        uint32_t h = Hash(x * 131 + 5);
        float vel = 20 + (h & 31);
        int cabeza = (int)fmodf(t * vel + (h >> 8 & 255), FH + 40);
        int largo = 10 + (h >> 16 & 15);
        for (int k = 0; k < largo; k++)
        {
            int y = cabeza - k * 2;
            if (y < 0 || y >= FH) continue;
            float f = 1 - (float)k / largo;
            uint32_t c = k == 0 ? p[3] : Mezcla(p[1], p[2], f);
            // Un "carácter" de 3 × 2 que cambia de forma.
            uint32_t g = Hash((uint32_t)(x * 31 + y * 17 + (k == 0 ? (int)(t * 6) : 0)));
            for (int dx = 0; dx < 3; dx++) if (g >> dx & 1) Punto(x + dx, y, c);
            if (g >> 4 & 1) Punto(x + 1, y + 1, Mezcla(c, p[0], .4f));
        }
    }
}

static void Plasma(float t, const uint32_t* p)
{
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
        {
            float fx = (float)x / SW, fy = (float)y / SH;
            float v = Seno(fx * 1.6f + t * .05f) + Seno(fy * 1.3f - t * .04f) + Seno((fx + fy) * 1.1f + t * .03f) +
                      Seno(sqrtf((fx - .5f) * (fx - .5f) + (fy - .5f) * (fy - .5f)) * 3 - t * .06f);
            float f = .5f + .5f * Seno(v * .25f + t * .02f);
            suave[y * SW + x] = Paleta(p, f);
        }
}

static void Puntos(float t, const uint32_t* p)
{
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++) suave[y * SW + x] = Mezcla(p[0], p[1], (float)y / SH * .7f);
    for (int gy = 4; gy < SH - 1; gy += 8)
        for (int gx = 4; gx < SW - 1; gx += 8)
        {
            float d = sqrtf((gx - SW / 2.f) * (gx - SW / 2.f) + (gy - SH / 2.f) * (gy - SH / 2.f)) / SW;
            float b = .5f + .5f * Seno(d * 3 - t * .25f);
            uint32_t c = Mezcla(p[2], p[3], b);
            suave[gy * SW + gx] = Mezcla(suave[gy * SW + gx], c, .4f + .6f * b);
            if (b > .7f)
            {
                suave[gy * SW + gx + 1] = Mezcla(suave[gy * SW + gx + 1], c, .5f * b);
                suave[(gy + 1) * SW + gx] = Mezcla(suave[(gy + 1) * SW + gx], c, .5f * b);
            }
        }
}

static void Cuadros(float t)
{
    const uint32_t* p = temaActual->f;
    int corre = (int)(t * 10);
    for (int y = 0; y < FH; y++)
        for (int x = 0; x < FW; x++)
        {
            int u = (x + y + corre) / 20, v = (x - y + 4000 - corre / 2) / 20;
            uint32_t c = ((u + v) & 1) ? p[0] : p[1];
            if (((x + y + corre) % 20) == 0) c = Mezcla(c, p[2], .5f);
            px[y * FW + x] = Mezcla(c, p[0], (float)y / FH * .35f);
        }
}

static void DegradadoTema(float t, const uint32_t* p)
{
    float lx = SW * (.3f + .2f * Seno(t * .01f)), ly = SH * .3f, mx = SW * (.75f + .15f * Seno(t * .013f + .3f)), my = SH * .75f;
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
        {
            uint32_t c = Mezcla(p[0], p[1], (float)y / SH);
            float d1 = ((x - lx) * (x - lx) + (y - ly) * (y - ly)) / (SW * SW * .12f);
            float d2 = ((x - mx) * (x - mx) + (y - my) * (y - my)) / (SW * SW * .1f);
            if (d1 < 1) c = Mezcla(c, p[2], (1 - d1) * (1 - d1) * .45f);
            if (d2 < 1) c = Mezcla(c, p[3], (1 - d2) * (1 - d2) * .25f);
            suave[y * SW + x] = c;
        }
}

// El paisaje con los colores del tema: cada píxel toma de la paleta según lo claro que es.
static void MapearPaisaje(const uint32_t* p)
{
    for (int i = 0; i < FW * FH; i++) px[i] = Paleta(p, Luz(px[i]) * 1.15f);
}

// Desenfoque (dos pasadas de caja) de una imagen chica, para el vidrio.
static void Desenfocar(const uint32_t* src, int w, int h, uint32_t* dst)
{
    static uint32_t tmp[FW * FH];
    const int R = 1;   // poco: detrás del vidrio se distingue el paisaje (montañas, luna), solo suavizado
    for (int pasada = 0; pasada < 2; pasada++)
    {
        const uint32_t* a = pasada ? tmp : src;
        uint32_t* b = pasada ? dst : tmp;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                uint32_t r = 0, g = 0, bl = 0, n = 0;
                for (int k = -R; k <= R; k++)
                {
                    int xx = pasada ? x : x + k, yy = pasada ? y + k : y;
                    if (xx < 0 || xx >= w || yy < 0 || yy >= h) continue;
                    uint32_t c = a[yy * w + xx];
                    r += c >> 16 & 255; g += c >> 8 & 255; bl += c & 255; n++;
                }
                b[y * w + x] = (r / n) << 16 | (g / n) << 8 | (bl / n);
            }
    }
}

void FondoPixel(float t)
{
    const Tema& tema = *temaActual;
    bool pixel = tema.fondo == FT_PAISAJE || tema.fondo == FT_PAISAJE_MAPA || tema.fondo == FT_LLUVIA || tema.fondo == FT_CUADROS;
    switch (tema.fondo)
    {
    case FT_PAISAJE: Paisaje(t); break;
    case FT_PAISAJE_MAPA: Paisaje(t); MapearPaisaje(tema.f); break;
    case FT_AURORA: Aurora(t, tema.f); break;
    case FT_REJILLA: Rejilla(t, tema.f); break;
    case FT_ESPACIO: Espacio(t, tema.f); break;
    case FT_OLAS: Olas(t, tema.f); break;
    case FT_BURBUJAS: Burbujas(t, tema.f); break;
    case FT_MANCHAS: Manchas(t, tema.f); break;
    case FT_LLUVIA: Lluvia(t); break;
    case FT_PLASMA: Plasma(t, tema.f); break;
    case FT_PUNTOS: Puntos(t, tema.f); break;
    case FT_CUADROS: Cuadros(t); break;
    default: DegradadoTema(t, tema.f); break;
    }
    if (pixel) Imagen(px, FW, FH, 0, 0, ANCHO, ALTO, 1, false);
    else Imagen(suave, SW, SH, 0, 0, ANCHO, ALTO, 1, true);
    // El fondo desenfocado, para el vidrio: las cajas de los juegos son siempre de vidrio (en todos los temas) y los
    // paneles, en los temas de vidrio.
    // Se rehace cada 3 cuadros: el fondo se mueve despacio y detrás del vidrio no se nota (cuesta un tercio).
    static uint32_t borrosoPixel[FW * FH];
    static int cuadro, fondoAntes = -1;
    bool rehacer = cuadro++ % 3 == 0 || fondoAntes != tema.fondo;
    fondoAntes = tema.fondo;
    if (pixel) { if (rehacer) Desenfocar(px, FW, FH, borrosoPixel); LienzoVidrio(borrosoPixel, FW, FH, rehacer); }
    else { if (rehacer) Desenfocar(suave, SW, SH, borroso); LienzoVidrio(borroso, SW, SH, rehacer); }
}
