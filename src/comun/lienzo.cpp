// Ver lienzo.h. Las formas redondeadas usan la distancia con signo a una caja redondeada (negativa adentro): de ahí sale
// cuánto cubre cada píxel el borde (suavizado), el difuminado de las sombras y el grosor de los bordes.
#include "lienzo.h"

#include <emmintrin.h>   // SSE2: lo tienen el procesador de la PS4 (Jaguar) y cualquier PC de 64 bits
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "plataforma.h"
#include "texto.h"

enum { O_DEGRADADO, O_FONDO4, O_CAJA, O_SOMBRA, O_BORDE, O_BRILLO, O_LINEA, O_TEXTO, O_IMAGEN };

struct Orden
{
    uint8_t tipo, horizontal, suave, hueca;
    int16_t fuente;
    float x, y, w, h, r, s;   // s: grosor del borde o de la línea, o difuminado de la sombra
    float x1, y1;             // fin de la línea
    uint32_t c1, c2;
    float a, a2;              // a2: opacidad abajo en un velo (la opacidad va de a arriba a a2 abajo)
    uint8_t velo;
    int bx0, by0, bx1, by1;   // píxeles que puede tocar, ya recortados
    const void* dato;
    int n, iw, ih;
};

static const int MAX_ORDENES = 6000;
static const int ARENA = 1 << 17;
static Orden* ordenes;
static char* arena;
static int nOrdenes, usoArena, ordenesAntes;
static uint32_t* destino;          // donde se dibuja: memoria normal (rápida de leer para mezclar colores)
static int pitchDestino;
static uint32_t* salida;           // la pantalla: cada tira terminada se copia ahí
static int pitchSalida;
static bool cambiarRB;
static float opacidad = 1;
static int rx0, ry0, rx1, ry1;   // recorte

void LienzoIniciar()
{
    static Orden lista[MAX_ORDENES];
    static char memoria[ARENA];
    ordenes = lista;
    arena = memoria;
    destino = (uint32_t*)malloc((size_t)ANCHO * ALTO * 4);
    pitchDestino = ANCHO;
}

void LienzoComenzar(uint32_t* pixeles, int pitch, bool rojoAzul)
{
    salida = pixeles;
    pitchSalida = pitch;
    cambiarRB = rojoAzul;
    nOrdenes = 0;
    usoArena = 0;
    opacidad = 1;
    SinRecorte();
}

int OrdenesUltimoCuadro() { return ordenesAntes; }
void Opacidad(float a) { opacidad = a < 0 ? 0 : a > 1 ? 1 : a; }
float OpacidadActual() { return opacidad; }

void Recorte(float x, float y, float w, float h)
{
    rx0 = (int)floorf(x); ry0 = (int)floorf(y); rx1 = (int)ceilf(x + w); ry1 = (int)ceilf(y + h);
    if (rx0 < 0) rx0 = 0;
    if (ry0 < 0) ry0 = 0;
    if (rx1 > ANCHO) rx1 = ANCHO;
    if (ry1 > ALTO) ry1 = ALTO;
}

void SinRecorte() { rx0 = 0; ry0 = 0; rx1 = ANCHO; ry1 = ALTO; }

static uint32_t Color(uint32_t c)
{
    c &= 0xffffff;
    return cambiarRB ? (c & 0x00ff00) | (c >> 16) | ((c & 0xff) << 16) : c;
}

// Nueva orden con la caja de píxeles que puede tocar (recortada); NULL si no se ve o no hay lugar.
static Orden* Nueva(int tipo, float bx0, float by0, float bx1, float by1, float a)
{
    a *= opacidad;
    if (a <= 1 / 512.f || nOrdenes >= MAX_ORDENES) return NULL;
    int x0 = (int)floorf(bx0), y0 = (int)floorf(by0), x1 = (int)ceilf(bx1), y1 = (int)ceilf(by1);
    if (x0 < rx0) x0 = rx0;
    if (y0 < ry0) y0 = ry0;
    if (x1 > rx1) x1 = rx1;
    if (y1 > ry1) y1 = ry1;
    if (x0 >= x1 || y0 >= y1) return NULL;
    Orden* o = &ordenes[nOrdenes++];
    memset(o, 0, sizeof *o);
    o->tipo = (uint8_t)tipo;
    o->a = a > 1 ? 1 : a;
    o->bx0 = x0; o->by0 = y0; o->bx1 = x1; o->by1 = y1;
    return o;
}

void Degradado(uint32_t arriba, uint32_t abajo)
{
    Orden* o = Nueva(O_DEGRADADO, 0, 0, ANCHO, ALTO, 1);
    if (!o) return;
    o->c1 = Color(arriba); o->c2 = Color(abajo);
}

static void CajaGeneral(int tipo, float x, float y, float w, float h, float radio, float s, uint32_t c1, uint32_t c2, bool horiz, float a)
{
    if (w <= 0 || h <= 0) return;
    float m = tipo == O_SOMBRA ? s : 1;
    Orden* o = Nueva(tipo, x - m, y - m, x + w + m, y + h + m, a);
    if (!o) return;
    float rmax = (w < h ? w : h) / 2;
    o->x = x; o->y = y; o->w = w; o->h = h;
    o->r = radio < 0 ? 0 : radio > rmax ? rmax : radio;
    o->s = s;
    o->c1 = Color(c1); o->c2 = Color(c2);
    o->horizontal = horiz;
}

void Caja(float x, float y, float w, float h, float radio, uint32_t c, float a) { CajaGeneral(O_CAJA, x, y, w, h, radio, 1, c, c, false, a); }
void CajaDegradada(float x, float y, float w, float h, float radio, uint32_t c1, uint32_t c2, float a) { CajaGeneral(O_CAJA, x, y, w, h, radio, 1, c1, c2, false, a); }
void CajaDegradadaH(float x, float y, float w, float h, float radio, uint32_t c1, uint32_t c2, float a) { CajaGeneral(O_CAJA, x, y, w, h, radio, 1, c1, c2, true, a); }
void Sombra(float x, float y, float w, float h, float radio, float dif, float a, bool hueca)
{
    int antes = nOrdenes;
    CajaGeneral(O_SOMBRA, x, y, w, h, radio, dif < 1 ? 1 : dif, 0, 0, false, a);
    if (nOrdenes > antes) ordenes[nOrdenes - 1].hueca = hueca;
}

void FondoReducido(const uint32_t* px)
{
    Orden* o = Nueva(O_FONDO4, 0, 0, ANCHO, ALTO, 1);
    if (o) o->dato = px;
}
void Borde(float x, float y, float w, float h, float radio, float grosor, uint32_t c, float a) { CajaGeneral(O_BORDE, x, y, w, h, radio, grosor, c, c, false, a); }
void Circulo(float cx, float cy, float r, uint32_t c, float a) { Caja(cx - r, cy - r, 2 * r, 2 * r, r, c, a); }
void Aro(float cx, float cy, float r, float grosor, uint32_t c, float a) { Borde(cx - r, cy - r, 2 * r, 2 * r, r, grosor, c, a); }

void Velo(float x, float y, float w, float h, float radio, uint32_t c, float aArriba, float aAbajo)
{
    float am = aArriba > aAbajo ? aArriba : aAbajo;
    if (am <= 0 || w <= 0 || h <= 0) return;
    Orden* o = Nueva(O_CAJA, x, y, x + w, y + h, am);
    if (!o) return;
    float rmax = (w < h ? w : h) / 2;
    o->x = x; o->y = y; o->w = w; o->h = h; o->s = 1;
    o->r = radio < 0 ? 0 : radio > rmax ? rmax : radio;
    o->c1 = o->c2 = Color(c);
    o->velo = 1;
    o->a = aArriba * opacidad; o->a2 = aAbajo * opacidad;
}

void Brillo(float cx, float cy, float r, uint32_t c, float a)
{
    Orden* o = Nueva(O_BRILLO, cx - r, cy - r, cx + r, cy + r, a);
    if (!o) return;
    o->x = cx; o->y = cy; o->r = r; o->c1 = Color(c);
}

void Linea(float x0, float y0, float x1, float y1, float grosor, uint32_t c, float a)
{
    float m = grosor / 2 + 1;
    Orden* o = Nueva(O_LINEA, fminf(x0, x1) - m, fminf(y0, y1) - m, fmaxf(x0, x1) + m, fmaxf(y0, y1) + m, a);
    if (!o) return;
    o->x = x0; o->y = y0; o->x1 = x1; o->y1 = y1; o->s = grosor; o->c1 = Color(c);
}

void Imagen(const uint32_t* px, int iw, int ih, float x, float y, float w, float h, float a, bool suave)
{
    if (!px || iw <= 0 || ih <= 0) return;
    Orden* o = Nueva(O_IMAGEN, x, y, x + w, y + h, a);
    if (!o) return;
    o->x = x; o->y = y; o->w = w; o->h = h;
    o->dato = px; o->iw = iw; o->ih = ih; o->suave = suave;
}

void ImagenSumada(const uint32_t* px, int iw, int ih, float x, float y, float w, float h, float a)
{
    if (!px || iw <= 0 || ih <= 0) return;
    Orden* o = Nueva(O_IMAGEN, x, y, x + w, y + h, a);
    if (!o) return;
    o->x = x; o->y = y; o->w = w; o->h = h;
    o->dato = px; o->iw = iw; o->ih = ih; o->suave = true;
    o->hueca = 1;   // en las imágenes, "hueca" quiere decir sumada
}

void OrdenTexto(int fuente, const char* s, int n, float x, float y, uint32_t c, float a)
{
    // La caja vertical alcanza para letras con acentos y colas (de -0,3 a 1,3 veces el alto de línea).
    float al = AltoLinea(fuente);
    Orden* o = Nueva(O_TEXTO, x - 4, y - al * .3f, x + AnchoTexto(fuente, s) + 4, y + al * 1.3f, a);
    if (!o) return;
    if (usoArena + n + 1 > ARENA) { nOrdenes--; return; }
    char* copia = arena + usoArena;
    memcpy(copia, s, n);
    copia[n] = 0;
    usoArena += n + 1;
    o->fuente = (int16_t)fuente; o->dato = copia; o->n = n;
    o->x = x; o->y = y; o->c1 = Color(c);
}

// ---------- pintar ----------
static inline uint32_t Mezcla(uint32_t d, uint32_t s, uint32_t a)   // a en [0, 256]
{
    uint32_t rb = (((s & 0xff00ff) * a + (d & 0xff00ff) * (256 - a)) >> 8) & 0xff00ff;
    uint32_t g = (((s & 0x00ff00) * a + (d & 0x00ff00) * (256 - a)) >> 8) & 0x00ff00;
    return rb | g | 0xff000000;
}

// Mezcla n píxeles seguidos con un mismo color, de a 4 por vez con SSE2 (cada canal en 16 bits:
// destino × (256 − a) + color × a nunca pasa de 255 × 256, así que entra sin desbordar).
static void MezclarTramo(uint32_t* p, int n, uint32_t c, uint32_t a)
{
    int i = 0;
    if (a >= 256)
    {
        uint32_t v = c | 0xff000000;
        for (; i < n; i++) p[i] = v;
        return;
    }
    const __m128i cero = _mm_setzero_si128(), opaco = _mm_set1_epi32((int)0xff000000);
    const __m128i ca = _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_set1_epi32((int)c), cero), _mm_set1_epi16((short)a));
    const __m128i inv = _mm_set1_epi16((short)(256 - a));
    for (; i + 4 <= n; i += 4)
    {
        __m128i d = _mm_loadu_si128((const __m128i*)(p + i));
        __m128i lo = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(d, cero), inv), ca), 8);
        __m128i hi = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(_mm_unpackhi_epi8(d, cero), inv), ca), 8);
        _mm_storeu_si128((__m128i*)(p + i), _mm_or_si128(_mm_packus_epi16(lo, hi), opaco));
    }
    for (; i < n; i++) p[i] = Mezcla(p[i], c, a);
}

static inline int A256(float cobertura, float a)
{
    float v = cobertura * a * 256 + .5f;
    return v <= 0 ? 0 : v >= 256 ? 256 : (int)v;
}

static inline float Limitar01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// Cobertura de un píxel según su distancia con signo al borde.
static inline float Cobertura(const Orden& o, float sdf)
{
    switch (o.tipo)
    {
    case O_SOMBRA: { float t = Limitar01(.5f - sdf / o.s); return t * t * (3 - 2 * t); }
    case O_BORDE: { float d = fabsf(sdf + o.s / 2) - o.s / 2; return Limitar01(.5f - d); }
    default: return Limitar01(.5f - sdf);
    }
}

static void PintarCaja(const Orden& o, int y0, int y1)
{
    float cx = o.x + o.w / 2, cy = o.y + o.h / 2, hw = o.w / 2, hh = o.h / 2, r = o.r;
    for (int py = y0; py < y1; py++)
    {
        uint32_t* fila = destino + (size_t)py * pitchDestino;
        float fy = py + .5f;
        float qy = fabsf(fy - cy) - (hh - r);
        uint32_t cf = o.c1;
        if (!o.horizontal && o.c1 != o.c2) cf = Mezcla(o.c1, o.c2, (uint32_t)(Limitar01((fy - o.y) / o.h) * 256));
        // Tramo del medio, donde todos los píxeles de la fila cubren lo mismo y se pintan de una pasada:
        //  - en las filas de los lados rectos (qy <= 0) la distancia es max(qx, qy) - r, y por debajo de T (lo bastante
        //    adentro) la cobertura ya no cambia: llena en cajas y sombras, nula en bordes;
        //  - si no, donde |x - cx| - (hw - r) <= min(qy, 0) la distancia es qy - r, igual en toda la fila.
        const float T = o.tipo == O_SOMBRA ? -o.s / 2 : o.tipo == O_BORDE ? -(o.s + .5f) : -.5f;
        float S, sdfMedio;
        if (qy <= 0 && qy - r <= T) { S = hw + T; sdfMedio = T; }
        else { S = hw - r + (qy < 0 ? qy : 0); sdfMedio = qy - r; }
        int t0 = (int)ceilf(cx - S - .5f), t1 = (int)floorf(cx + S - .5f) + 1;
        float aFila = o.velo ? o.a + (o.a2 - o.a) * Limitar01((fy - o.y) / o.h) : o.a;
        int aMedio = A256(Cobertura(o, sdfMedio), aFila);
        if (t0 < o.bx0) t0 = o.bx0;
        if (t0 > o.bx1) t0 = o.bx1;
        if (t1 > o.bx1) t1 = o.bx1;
        if (t1 < t0) t1 = t0;
        // Bordes izquierdo y derecho, píxel por píxel.
        for (int pass = 0; pass < 2; pass++)
        {
            int a0 = pass ? t1 : o.bx0, a1 = pass ? o.bx1 : t0;
            for (int px = a0; px < a1; px++)
            {
                float qx = fabsf(px + .5f - cx) - (hw - r);
                // Solo en las esquinas (qx y qy positivos) hace falta la raíz; en los lados la distancia es directa.
                float sdf = qx > 0 && qy > 0 ? sqrtf(qx * qx + qy * qy) - r : (qx > qy ? qx : qy) - r;
                int a = A256(Cobertura(o, sdf), aFila);
                if (!a) continue;
                uint32_t c = o.horizontal ? Mezcla(o.c1, o.c2, (uint32_t)(Limitar01((px + .5f - o.x) / o.w) * 256)) : cf;
                fila[px] = Mezcla(fila[px], c, (uint32_t)a);
            }
        }
        if (!aMedio || t0 >= t1 || (o.hueca && aMedio == A256(1, aFila))) continue;
        if (o.horizontal)
            for (int px = t0; px < t1; px++)
                fila[px] = Mezcla(fila[px], Mezcla(o.c1, o.c2, (uint32_t)(Limitar01((px + .5f - o.x) / o.w) * 256)), (uint32_t)aMedio);
        else MezclarTramo(fila + t0, t1 - t0, cf, (uint32_t)aMedio);
    }
}

static void PintarBrillo(const Orden& o, int y0, int y1)
{
    float r2 = o.r * o.r;
    for (int py = y0; py < y1; py++)
    {
        float dy = py + .5f - o.y;
        if (dy * dy >= r2) continue;
        float medio = sqrtf(r2 - dy * dy);
        int x0 = (int)(o.x - medio), x1 = (int)(o.x + medio) + 1;
        if (x0 < o.bx0) x0 = o.bx0;
        if (x1 > o.bx1) x1 = o.bx1;
        uint32_t* fila = destino + (size_t)py * pitchDestino;
        for (int px = x0; px < x1; px++)
        {
            float dx = px + .5f - o.x;
            float t = 1 - (dx * dx + dy * dy) / r2;
            if (t <= 0) continue;
            int a = (int)(t * t * o.a * 256);
            if (a > 0) fila[px] = Mezcla(fila[px], o.c1, (uint32_t)a);
        }
    }
}

static void PintarLinea(const Orden& o, int y0, int y1)
{
    float bax = o.x1 - o.x, bay = o.y1 - o.y;
    float bb = bax * bax + bay * bay;
    float medio = o.s / 2;
    for (int py = y0; py < y1; py++)
    {
        uint32_t* fila = destino + (size_t)py * pitchDestino;
        float pay = py + .5f - o.y;
        for (int px = o.bx0; px < o.bx1; px++)
        {
            float pax = px + .5f - o.x;
            float h = bb > 0 ? Limitar01((pax * bax + pay * bay) / bb) : 0;
            float dx = pax - bax * h, dy = pay - bay * h;
            float d = sqrtf(dx * dx + dy * dy) - medio;
            int a = A256(Limitar01(.5f - d), o.a);
            if (a) fila[px] = Mezcla(fila[px], o.c1, (uint32_t)a);
        }
    }
}

static inline uint32_t RB(uint32_t c) { return cambiarRB ? (c & 0x00ff00) | ((c >> 16) & 0xff) | ((c & 0xff) << 16) : c & 0xffffff; }

static void PintarImagen(const Orden& o, int y0, int y1)
{
    const uint32_t* src = (const uint32_t*)o.dato;
    float sx = o.iw / o.w, sy = o.ih / o.h;
    int a = A256(1, o.a);
    if (o.hueca)
    {
        // Sumada (ImagenSumada): bilineal, y cada canal se suma al fondo con tope en 255. Solo para imágenes chicas en
        // pantalla (el logo de la cabecera): va por el camino lento.
        for (int py = y0; py < y1; py++)
        {
            uint32_t* fila = destino + (size_t)py * pitchDestino;
            float v = (py + .5f - o.y) * sy - .5f;
            int iy = (int)floorf(v);
            uint32_t fy = (uint32_t)((v - iy) * 256);
            const uint32_t* s0 = src + (size_t)(iy < 0 ? 0 : iy >= o.ih ? o.ih - 1 : iy) * o.iw;
            const uint32_t* s1 = src + (size_t)(iy + 1 < 0 ? 0 : iy + 1 >= o.ih ? o.ih - 1 : iy + 1) * o.iw;
            for (int px = o.bx0; px < o.bx1; px++)
            {
                float u = (px + .5f - o.x) * sx - .5f;
                int ix = (int)floorf(u);
                uint32_t fx = (uint32_t)((u - ix) * 256);
                int ix0 = ix < 0 ? 0 : ix >= o.iw ? o.iw - 1 : ix;
                int ix1 = ix + 1 >= o.iw ? o.iw - 1 : ix + 1 < 0 ? 0 : ix + 1;
                uint32_t c = RB(Mezcla(Mezcla(s0[ix0], s0[ix1], fx), Mezcla(s1[ix0], s1[ix1], fx), fy));
                uint32_t d = fila[px], r = 0;
                for (int k = 0; k < 24; k += 8)
                {
                    uint32_t v2 = ((d >> k) & 255) + (((c >> k) & 255) * (uint32_t)a >> 8);
                    r |= (v2 > 255 ? 255 : v2) << k;
                }
                fila[px] = r | 0xff000000;
            }
        }
        return;
    }
    uint16_t tablaX[ANCHO], tablaF[ANCHO];   // columna de origen y peso de cada columna de destino (camino rápido)
    bool tablaLista = false;
    for (int py = y0; py < y1; py++)
    {
        uint32_t* fila = destino + (size_t)py * pitchDestino;
        float v = (py + .5f - o.y) * sy - (o.suave ? .5f : 0);
        if (!o.suave)
        {
            int iy = (int)v;
            if (iy < 0) iy = 0;
            if (iy >= o.ih) iy = o.ih - 1;
            const uint32_t* s = src + (size_t)iy * o.iw;
            for (int px = o.bx0; px < o.bx1; px++)
            {
                int ix = (int)((px + .5f - o.x) * sx);
                if (ix < 0) ix = 0;
                if (ix >= o.iw) ix = o.iw - 1;
                uint32_t c = RB(s[ix]);
                fila[px] = a == 256 ? c | 0xff000000 : Mezcla(fila[px], c, (uint32_t)a);
            }
            continue;
        }
        int iy = (int)floorf(v);
        uint32_t fy = (uint32_t)((v - iy) * 256);
        int iy0 = iy < 0 ? 0 : iy >= o.ih ? o.ih - 1 : iy;
        int iy1 = iy + 1 < 0 ? 0 : iy + 1 >= o.ih ? o.ih - 1 : iy + 1;
        const uint32_t* s0 = src + (size_t)iy0 * o.iw;
        const uint32_t* s1 = src + (size_t)iy1 * o.iw;
        if (o.iw <= 1024)
        {
            // Rápido (la imagen del juego, una vez por cuadro): primero se mezclan las dos filas de origen (una vez por
            // fila), y la posición y el peso horizontal de cada columna de destino se calculan una vez por tira.
            uint32_t fila2[1025];
            for (int i = 0; i < o.iw; i++) fila2[i] = RB(Mezcla(s0[i], s1[i], fy));
            fila2[o.iw] = fila2[o.iw - 1];
            if (py == y0 || !tablaLista)
            {
                for (int px = o.bx0; px < o.bx1; px++)
                {
                    float u = (px + .5f - o.x) * sx - .5f;
                    int ix = (int)floorf(u);
                    uint32_t fx = (uint32_t)((u - ix) * 256);
                    if (ix < 0) { ix = 0; fx = 0; }
                    if (ix >= o.iw) { ix = o.iw - 1; fx = 0; }
                    tablaX[px] = (uint16_t)ix;
                    tablaF[px] = (uint16_t)fx;
                }
                tablaLista = true;
            }
            if (a == 256)
                for (int px = o.bx0; px < o.bx1; px++) fila[px] = Mezcla(fila2[tablaX[px]], fila2[tablaX[px] + 1], tablaF[px]);
            else
                for (int px = o.bx0; px < o.bx1; px++)
                    fila[px] = Mezcla(fila[px], Mezcla(fila2[tablaX[px]], fila2[tablaX[px] + 1], tablaF[px]), (uint32_t)a);
            continue;
        }
        for (int px = o.bx0; px < o.bx1; px++)
        {
            float u = (px + .5f - o.x) * sx - .5f;
            int ix = (int)floorf(u);
            uint32_t fx = (uint32_t)((u - ix) * 256);
            int ix0 = ix < 0 ? 0 : ix >= o.iw ? o.iw - 1 : ix;
            int ix1 = ix + 1 < 0 ? 0 : ix + 1 >= o.iw ? o.iw - 1 : ix + 1;
            uint32_t c = Mezcla(Mezcla(s0[ix0], s0[ix1], fx), Mezcla(s1[ix0], s1[ix1], fx), fy);
            c = RB(c);
            fila[px] = a == 256 ? c | 0xff000000 : Mezcla(fila[px], c, (uint32_t)a);
        }
    }
}

// Fondo reducido: cada fila sale de mezclar dos filas de la imagen chica (una sola vez por fila) y después cada píxel
// mezcla dos vecinos de esa fila con uno de 4 pesos fijos (la imagen se agranda exactamente 4 veces).
static void PintarFondo4(const Orden& o, int y0, int y1)
{
    const uint32_t* src = (const uint32_t*)o.dato;
    uint32_t fila4[FONDO_W + 1];
    for (int py = y0; py < y1; py++)
    {
        float v = (py + .5f) / 4 - .5f;
        int iy = (int)floorf(v);
        uint32_t fy = (uint32_t)((v - iy) * 256);
        int a0 = iy < 0 ? 0 : iy, a1 = iy + 1 >= FONDO_H ? FONDO_H - 1 : iy + 1;
        const uint32_t* s0 = src + (size_t)a0 * FONDO_W;
        const uint32_t* s1 = src + (size_t)a1 * FONDO_W;
        for (int i = 0; i < FONDO_W; i++) fila4[i] = RB(Mezcla(s0[i], s1[i], fy));
        fila4[FONDO_W] = fila4[FONDO_W - 1];
        uint32_t* fila = destino + (size_t)py * pitchDestino;
        // Los píxeles x = 4i+2 … 4i+5 caen entre fila4[i] y fila4[i+1] con pesos 0,125 / 0,375 / 0,625 / 0,875:
        // los cuatro salen juntos con SSE2.
        fila[0] = fila[1] = fila4[0] | 0xff000000;
        const __m128i cero = _mm_setzero_si128(), opaco = _mm_set1_epi32((int)0xff000000);
        const __m128i wlo = _mm_setr_epi16(32, 32, 32, 32, 96, 96, 96, 96), whi = _mm_setr_epi16(160, 160, 160, 160, 224, 224, 224, 224);
        const __m128i ilo = _mm_sub_epi16(_mm_set1_epi16(256), wlo), ihi = _mm_sub_epi16(_mm_set1_epi16(256), whi);
        for (int i = 0; i < FONDO_W - 1; i++)
        {
            __m128i d = _mm_unpacklo_epi8(_mm_set1_epi32((int)fila4[i]), cero);
            __m128i s = _mm_unpacklo_epi8(_mm_set1_epi32((int)fila4[i + 1]), cero);
            __m128i lo = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(d, ilo), _mm_mullo_epi16(s, wlo)), 8);
            __m128i hi = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(d, ihi), _mm_mullo_epi16(s, whi)), 8);
            _mm_storeu_si128((__m128i*)(fila + 4 * i + 2), _mm_or_si128(_mm_packus_epi16(lo, hi), opaco));
        }
        fila[ANCHO - 2] = fila[ANCHO - 1] = fila4[FONDO_W - 1] | 0xff000000;
    }
}

static void PintarDegradado(const Orden& o, int y0, int y1)
{
    int a = A256(1, o.a);
    for (int py = y0; py < y1; py++)
    {
        uint32_t c = Mezcla(o.c1, o.c2, (uint32_t)(py * 256 / ALTO));
        uint32_t* fila = destino + (size_t)py * pitchDestino;
        MezclarTramo(fila + o.bx0, o.bx1 - o.bx0, c, (uint32_t)a);
    }
}

#ifdef MEDIR_ORDENES
#include <intrin.h>
uint64_t ciclosPorTipo[16];   // solo el simulador de PC: tiempo gastado en cada tipo de orden
#endif

// Las filas se reparten en tiras intercaladas para que todos los núcleos tengan trabajo parecido aunque la pantalla
// esté más cargada en el medio que arriba.
static const int TIRA = 24;

static void PintarParte(int parte, int partes, void*)
{
    for (int t0 = parte * TIRA; t0 < ALTO; t0 += partes * TIRA)
    {
        int t1 = t0 + TIRA < ALTO ? t0 + TIRA : ALTO;
        for (int i = 0; i < nOrdenes; i++)
        {
            const Orden& o = ordenes[i];
            int y0 = o.by0 > t0 ? o.by0 : t0, y1 = o.by1 < t1 ? o.by1 : t1;
            if (y0 >= y1) continue;
#ifdef MEDIR_ORDENES
            uint64_t c0 = __rdtsc();
#endif
            switch (o.tipo)
            {
            case O_DEGRADADO: PintarDegradado(o, y0, y1); break;
            case O_FONDO4: PintarFondo4(o, y0, y1); break;
            case O_CAJA: case O_SOMBRA: case O_BORDE: PintarCaja(o, y0, y1); break;
            case O_BRILLO: PintarBrillo(o, y0, y1); break;
            case O_LINEA: PintarLinea(o, y0, y1); break;
            case O_IMAGEN: PintarImagen(o, y0, y1); break;
            case O_TEXTO:
            {
                FranjaTexto fr = {destino, pitchDestino, y0, y1, o.bx0, o.bx1};
                PintarTexto(fr, o.fuente, (const char*)o.dato, o.n, o.x, o.y, o.c1, A256(1, o.a));
                break;
            }
            }
#ifdef MEDIR_ORDENES
            ciclosPorTipo[o.tipo == O_CAJA && o.velo ? 9 : o.tipo == O_CAJA && o.horizontal ? 10 : o.tipo] += __rdtsc() - c0;
#endif
        }
        // La tira ya está completa: a la pantalla mientras sigue en la caché del núcleo.
        if (salida != destino)
            for (int y = t0; y < t1; y++)
                memcpy(salida + (size_t)y * pitchSalida, destino + (size_t)y * pitchDestino, ANCHO * 4);
    }
}

void LienzoTerminar()
{
    EnParalelo(PintarParte, NULL);
    ordenesAntes = nOrdenes;
}
