// El chip ST010 (NEC µPD96050 con un programa de Seta) de un juego de carreras: rumbo y distancia entre puntos, giros
// y escalas en 2D, el orden de los autos en la carrera, la "conducción" de los rivales y la tabla de escalas del modo 7
// para cada línea. Su RAM de datos (4 KB, con batería) está en 68-6F:0000-0FFF; la SNES deja ahí los datos, el
// comando en $0020 y lo arranca con el bit 7 de $0021, que el chip borra al terminar.
// Se emula lo que calcula cada comando (como snes9x), según la investigación de Feather, Kris Bleakley, Overload,
// John Weidman y Matthew Kendora. Las tablas (seno, arcotangente y escalas del modo 7) se calculan con fórmulas, con
// los pocos valores donde el chip se aparta de ellas.
#include "interno.h"
#include "rom.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

bool st010Presente;

static int16_t seno[256];          // 32767·sin(2π·i/256)
static uint8_t arcotan[32][32];    // ángulo de (y, x) en 256 por vuelta
static int16_t escalaM7[176];      // escala del modo 7 para cada línea
static bool tablasListas;

static struct { uint8_t comando, ejecutar; bool habilitado; } c;

static void PrepararTablas()
{
    if (tablasListas) return;
    tablasListas = true;
    for (int i = 0; i < 256; i++) seno[i] = (int16_t)lround(32767.0 * sin(2 * M_PI * i / 256));
    for (int x = 0; x < 32; x++)
        for (int y = 0; y < 32; y++) arcotan[x][y] = (uint8_t)lround(atan2((double)x, (double)y) * 128 / M_PI);
    // Donde la tabla del chip se aparta de la fórmula (redondeos propios, en una unidad).
    static const uint8_t SENO_MENOS[] = {120, 131, 147, 163};
    static const uint8_t SENO_MAS[] = {206, 227, 231, 234, 238, 239, 241, 244, 246, 248, 252, 254};
    for (uint8_t i : SENO_MENOS) seno[i]--;
    for (uint8_t i : SENO_MAS) seno[i]++;
    arcotan[10][9] = 0x20;
    arcotan[31][8] = 0x35;
    for (int i = 0; i < 176; i++) escalaM7[i] = (int16_t)lround(7885.0 / (i + 8.8));
}

static uint8_t* const ram = sram;   // los 4 KB del chip son la "SRAM" del cartucho (se guardan con la partida)
static int16_t W(int a) { return (int16_t)(ram[a] | ram[a + 1] << 8); }
static int32_t DW(int a) { return (int32_t)(ram[a] | ram[a + 1] << 8 | ram[a + 2] << 16 | (uint32_t)ram[a + 3] << 24); }
static void PonerW(int a, int32_t v) { ram[a] = (uint8_t)v; ram[a + 1] = (uint8_t)(v >> 8); }
static void PonerDW(int a, int32_t v) { for (int i = 0; i < 4; i++) ram[a + i] = (uint8_t)((uint32_t)v >> 8 * i); }

static int16_t Sen(int16_t t) { return seno[(t >> 8) & 0xFF]; }
static int16_t Cos(int16_t t) { return seno[((t + 0x4000) >> 8) & 0xFF]; }

// Rumbo de (x0, y0): cuadrante, punto llevado al primer cuadrante y reducido a 5 bits, y el ángulo.
static void Brujula(int16_t x0, int16_t y0, int16_t& x1, int16_t& y1, int16_t& cuadrante, int16_t& angulo)
{
    if (x0 <= 0 && y0 < 0) { x1 = (int16_t)-x0; y1 = (int16_t)-y0; cuadrante = (int16_t)-0x8000; }
    else if (x0 < 0) { x1 = y0; y1 = (int16_t)-x0; cuadrante = -0x4000; }
    else if (y0 < 0) { x1 = (int16_t)-y0; y1 = x0; cuadrante = 0x4000; }
    else { x1 = x0; y1 = y0; cuadrante = 0; }
    while (x1 > 0x1F || y1 > 0x1F)
    {
        if (x1 > 1) x1 >>= 1;
        if (y1 > 1) y1 >>= 1;
    }
    angulo = (int16_t)(arcotan[x1 & 0x1F][y1 & 0x1F] << 8);
    angulo = (int16_t)((angulo | cuadrante) ^ 0x8000);
    if (x0 == 0 && y0 < 0) cuadrante = 0x4000;
}

static void OrdenarAutos()   // 02: posiciones (de mayor a menor) y los pilotos con ellas
{
    int n = (uint16_t)W(0x24);
    if (n > 32) n = 32;
    uint16_t lugar[32], piloto[32];
    for (int i = 0; i < n; i++) { lugar[i] = (uint16_t)W(0x40 + i * 2); piloto[i] = (uint16_t)W(0x80 + i * 2); }
    if (n > 1)
    {
        bool ordenado;
        int m = n;
        do
        {
            ordenado = true;
            for (int i = 0; i < m - 1; i++)
                if (lugar[i] < lugar[i + 1])
                {
                    uint16_t t = lugar[i]; lugar[i] = lugar[i + 1]; lugar[i + 1] = t;
                    t = piloto[i]; piloto[i] = piloto[i + 1]; piloto[i + 1] = t;
                    ordenado = false;
                }
            m--;
        } while (!ordenado);
    }
    for (int i = 0; i < n; i++) { PonerW(0x40 + i * 2, lugar[i]); PonerW(0x80 + i * 2, piloto[i]); }
}

static void Distancia()   // 04: largo aproximado del vector (x, y)
{
    int16_t x = W(0), y = W(2);
    x = (int16_t)(x < 0 ? -x : x);
    y = (int16_t)(y < 0 ? -y : y);
    int32_t p = (uint16_t)x >= (uint16_t)y ? ((x * 0x3D78 << 1) + (y * 0x1976 << 1)) << 1
                                           : ((y * 0x3D78 << 1) + (x * 0x1976 << 1)) << 1;
    PonerW(0x10, (p + 0x8000) >> 16);
}

static void Navegar()   // 05: un rival apunta al próximo punto de la pista y avanza
{
    int16_t maxX = W(0xC0), maxY = W(0xC2);
    int32_t x0 = DW(0xC4), y0 = DW(0xC8);
    int16_t ang0 = W(0xCC), ang1, x1, y1, d1, d2, d3;
    uint16_t radio = (uint16_t)W(0xD4), aumento = (uint16_t)W(0xD6), radioMax = (uint16_t)W(0xD8);
    int16_t brujula = W(0xDA), banderas = W(0xDC), nuevoX = W(0xDE), nuevoY = W(0xE0);

    x1 = (int16_t)(maxX - (x0 >> 16));
    y1 = (int16_t)(maxY - (y0 >> 16));
    Brujula(x1, y1, d1, d2, d3, ang1);
    ang1 = (int16_t)(ang1 - ang0);
    if (ang1 & 0xFF00) ang0 = (int16_t)(ang0 + ((ang1 & 0x8000) ? 0xFD80 : 0x0280));
    uint16_t giro = (uint16_t)((ang1 < 0 ? (int16_t)-ang1 : ang1) >> 4);
    if (giro < 0x100)
    {
        int32_t t = radio + aumento;
        radio = t >= radioMax ? radioMax : (uint16_t)t;
    }
    else
    {
        int32_t t = radio - giro;
        radio = t <= 0 ? 0 : (uint16_t)t;
    }
    x0 -= ((Sen(ang0) >> 5) * (radio >> 8)) << 1;
    y0 -= ((Cos(ang0) >> 5) * (radio >> 8)) << 1;
    x0 &= 0x1FFFFFFF;
    y0 &= 0x1FFFFFFF;
    int16_t rx = (brujula & 0x8000) ? 0x08 : 0x80, ry = (brujula & 0x8000) ? 0x80 : 0x08;
    if (abs(x1) < rx && abs(y1) < ry)   // llegó: el próximo punto
    {
        maxX = nuevoX;
        maxY = nuevoY & 0x0FFF;
        brujula = (nuevoY & 0x8000) ? (int16_t)-1 : 0;
        banderas |= 0x0008;
    }
    PonerW(0xC0, maxX); PonerW(0xC2, maxY);
    PonerDW(0xC4, x0); PonerDW(0xC8, y0);
    PonerW(0xCC, ang0); PonerW(0xCE, ang1);
    PonerW(0xD0, x1); PonerW(0xD2, y1);
    PonerW(0xD4, radio);
    PonerW(0xDA, brujula); PonerW(0xDC, banderas);
}

static void Raster()   // 07: matriz del modo 7 de cada línea para el ángulo dado
{
    int16_t ang = W(0);
    for (int i = 0, o = 0; i < 176; i++, o += 2)
    {
        int16_t v = (int16_t)(escalaM7[i] * Cos(ang) >> 15);
        PonerW(0x0F0 + o, v);
        PonerW(0x510 + o, v);
        v = (int16_t)(escalaM7[i] * Sen(ang) >> 15);
        PonerW(0x250 + o, v);
        if (v) v = (int16_t)~v;
        PonerW(0x3B0 + o, v);
    }
    ram[0] = ram[1];   // el ángulo queda listo para usarlo de índice
    ram[1] = 0;
}

static void Ejecutar()
{
    PrepararTablas();
    switch (c.comando)
    {
    case 0x01:   // rumbo
    {
        ram[6] = ram[2]; ram[7] = ram[3];
        int16_t x1, y1, q, a;
        Brujula(W(0), W(2), x1, y1, q, a);
        PonerW(0, x1); PonerW(2, y1); PonerW(4, q); PonerW(0x10, a);
        break;
    }
    case 0x02: OrdenarAutos(); break;
    case 0x03:   // escala 2D
    {
        int16_t m = W(4);
        PonerDW(0x10, W(0) * m << 1);
        PonerDW(0x14, W(2) * m << 1);
        break;
    }
    case 0x04: Distancia(); break;
    case 0x05: Navegar(); break;
    case 0x06: PonerDW(0x10, W(0) * W(2) << 1); break;   // producto de 16 bits
    case 0x07: Raster(); break;
    case 0x08:   // giro 2D
    {
        int16_t t = W(4), x = W(0), y = W(2);
        PonerW(0x10, (y * Sen(t) >> 15) + (x * Cos(t) >> 15));
        PonerW(0x12, (y * Cos(t) >> 15) - (x * Sen(t) >> 15));
        break;
    }
    }
    c.comando = c.ejecutar = 0;   // listo
}

void St010Reiniciar() { memset(&c, 0, sizeof c); }

// Registros del chip en 60-67:0000-3FFF; su RAM en 68-6F:0000-7FFF (4 KB repetidos).
bool St010Leer(uint8_t banco, uint16_t off, uint8_t& v)
{
    uint8_t b = banco & 0x7F;
    if (b >= 0x60 && b < 0x68 && off < 0x4000) { v = 0x80; return true; }
    if (b < 0x68 || b >= 0x70 || off >= 0x8000) return false;
    unsigned a = off & 0xFFF;
    v = a == 0x20 ? c.comando : a == 0x21 ? c.ejecutar : ram[a];
    return true;
}

bool St010Escribir(uint8_t banco, uint16_t off, uint8_t v)
{
    uint8_t b = banco & 0x7F;
    if (b >= 0x60 && b < 0x68 && off < 0x4000) { c.habilitado = true; return true; }
    if (b < 0x68 || b >= 0x70 || off >= 0x8000) return false;
    unsigned a = off & 0xFFF;
    if (a == 0x20 && c.habilitado) c.comando = v;
    if (a == 0x21 && c.habilitado) c.ejecutar = v;
    else if (ram[a] != v) { ram[a] = v; sramCambiada = true; }
    if (c.ejecutar & 0x80) Ejecutar();
    return true;
}

void St010Estado(Serial& s) { s(c); }

#ifdef ST010_PRUEBA
// Herramientas de PC: cambiar las tablas por otras, para comparar los cálculos con otra implementación.
void St010PonerTablas(const int16_t* s, const uint8_t* a)
{
    PrepararTablas();
    memcpy(seno, s, sizeof seno);
    memcpy(arcotan, a, sizeof arcotan);
}
#endif
