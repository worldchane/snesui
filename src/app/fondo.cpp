// Ver fondo.h. Todo se calcula con funciones (senos y un hash), sin imágenes: el paisaje se desplaza sin fin y sin
// costuras. Las posiciones de lo que se mueve se redondean a píxeles enteros para que no "tiemble" al agrandarse.
#include "fondo.h"

#include <math.h>
#include <stdint.h>

#include "../comun/lienzo.h"

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

void FondoPixel(float t)
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

    Imagen(px, FW, FH, 0, 0, ANCHO, ALTO, 1, false);
}
