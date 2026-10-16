// El chip Cx4 (Hitachi HG51B) de algunos cartuchos: hace cuentas de geometría (giros, escalas, líneas en 3D) y arma la
// tabla de sprites. La SNES lo ve en 00-3F:6000-7FFF: 3 KB de RAM de datos (6000-6BFF, repetida en 7000-7BFF) y 256
// registros (7F00-7FFF). Escribir en 7F47 copia un bloque de la memoria de la SNES a la RAM; escribir en 7F4F corre un
// comando. Como el programa del chip no viene con la rom, aquí está hecho lo que hace cada comando (igual que bsnes y
// snes9x sin el firmware), con el resultado listo en el momento.
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "interno.h"

bool cx4Presente;

static struct
{
    uint8_t ram[0xC00];
    uint8_t reg[0x100];
    uint32_t r0, r1, r2, r3, r4, r5;
} c;

// Valores de trabajo de los comandos de alambre (se calculan de nuevo en cada comando).
static int16_t wfX, wfY, wfZ, wfX2, wfY2, wfDist, wfEscala;

// Tablas del chip, sacadas de la fórmula (dan exactamente los mismos valores):
//  seno24: 65536·sin(π·i/256) truncado, 128 valores positivos y otros 128 negativos (el negativo resta uno más, salvo en
//  dos lugares donde el seno cae casi en un entero).
//  seno16/coseno16: 32768·sin(2π·i/512) truncado, sin pasar de ±32767.
static uint32_t seno24[256];
static int16_t seno16[512], coseno16[512];
static bool tablasListas;

static void PrepararTablas()
{
    if (tablasListas) return;
    tablasListas = true;
    for (int i = 0; i < 128; i++)
    {
        int32_t p = (int32_t)(65536.0 * sin(M_PI * i / 256));
        seno24[i] = (uint32_t)p;
        int32_t n = (p == 0 || i == 71 || i == 111) ? -p : -p - 1;
        seno24[i + 128] = (uint32_t)n & 0xFFFFFF;
    }
    for (int i = 0; i < 512; i++)
    {
        int32_t s = (int32_t)(32768.0 * sin(2 * M_PI * i / 512)), k = (int32_t)(32768.0 * cos(2 * M_PI * i / 512));
        seno16[i] = (int16_t)(s > 32767 ? 32767 : s < -32767 ? -32767 : s);
        coseno16[i] = (int16_t)(k > 32767 ? 32767 : k < -32767 ? -32767 : k);
    }
}

static const uint8_t datosInmediatos[48] = {
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
    0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x80, 0xff, 0xff, 0x7f,
    0x00, 0x80, 0x00, 0xff, 0x7f, 0x00, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0xff,
    0x00, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x00, 0x01, 0x00, 0xff, 0xfe, 0x00};

// Memoria del chip vista con direcciones de 13 bits (0000-0BFF RAM, 1F00-1FFF registros).
static uint8_t Leer(uint32_t a)
{
    a &= 0x1FFF;
    if (a < 0xC00) return c.ram[a];
    if (a >= 0x1F00) return c.reg[a & 0xFF];
    return 0;
}
static uint16_t LeerW(uint32_t a) { return (uint16_t)(Leer(a) | Leer(a + 1) << 8); }
static uint32_t LeerL(uint32_t a) { return Leer(a) | Leer(a + 1) << 8 | Leer(a + 2) << 16; }
static void Escribir(uint32_t a, uint8_t v);
static void EscribirW(uint32_t a, uint16_t v) { Escribir(a, (uint8_t)v); Escribir(a + 1, (uint8_t)(v >> 8)); }

// Registros de 24 bits del chip (en 1F80 + 3·r).
static uint32_t Ldr(int r) { int a = 0x80 + r * 3; return c.reg[a] | c.reg[a + 1] << 8 | c.reg[a + 2] << 16; }
static void Str(int r, uint32_t v) { int a = 0x80 + r * 3; c.reg[a] = (uint8_t)v; c.reg[a + 1] = (uint8_t)(v >> 8); c.reg[a + 2] = (uint8_t)(v >> 16); }

// Producto con signo de dos números de 24 bits: los 24 bits bajos y los 24 altos.
static void Mul(uint32_t x, uint32_t y, uint32_t& lo, uint32_t& hi)
{
    int64_t a = x & 0xFFFFFF, b = y & 0xFFFFFF;
    if (a & 0x800000) a |= ~0x7FFFFFll;
    if (b & 0x800000) b |= ~0x7FFFFFll;
    a *= b;
    lo = (uint32_t)(a & 0xFFFFFF);
    hi = (uint32_t)((a >> 24) & 0xFFFFFF);
}

static uint32_t Seno(uint32_t x)
{
    c.r0 = x & 0x1FF;
    if (c.r0 & 0x100) c.r0 ^= 0x1FF;
    if (c.r0 & 0x080) c.r0 ^= 0x0FF;
    return (x & 0x100) ? seno24[c.r0 + 0x80] : seno24[c.r0];
}
static uint32_t Coseno(uint32_t x) { return Seno(x + 0x80); }

static void InmediatoRegistro(int desde)
{
    c.r0 = Ldr(0);
    for (int i = desde; i < 48; i++)
    {
        if ((c.r0 & 0xFFF) < 0xC00) c.ram[c.r0 & 0xFFF] = datosInmediatos[i];
        c.r0++;
    }
    Str(0, c.r0);
}

// ---- Alambres en 3D ----

static void Girar(double& x, double& y, double& z, bool ajusteZ)
{
    if (ajusteZ) z -= 0x95;
    double t = -(double)wfX2 * M_PI * 2 / 128;   // giro en X
    double y2 = y * cos(t) - z * sin(t);
    double z2 = y * sin(t) + z * cos(t);
    t = -(double)wfY2 * M_PI * 2 / 128;           // giro en Y
    double x2 = x * cos(t) + z2 * sin(t);
    z = x * -sin(t) + z2 * cos(t);
    t = -(double)wfDist * M_PI * 2 / 128;         // giro en Z
    x = x2 * cos(t) - y2 * sin(t);
    y = x2 * sin(t) + y2 * cos(t);
}

static void TransformarAlambre()   // con perspectiva
{
    double x = wfX, y = wfY, z = wfZ;
    Girar(x, y, z, true);
    wfX = (int16_t)(x * wfEscala / (0x90 * (z + 0x95)) * 0x95);
    wfY = (int16_t)(y * wfEscala / (0x90 * (z + 0x95)) * 0x95);
}

static void TransformarAlambre2()   // sin perspectiva
{
    double x = wfX, y = wfY, z = wfZ;
    Girar(x, y, z, false);
    wfX = (int16_t)(x * wfEscala / 0x100);
    wfY = (int16_t)(y * wfEscala / 0x100);
}

static void CalcularAlambre()   // paso de una línea: el lado largo avanza de a 256 (un pixel en 8.8)
{
    wfX = (int16_t)(wfX2 - wfX);
    wfY = (int16_t)(wfY2 - wfY);
    if (abs(wfX) > abs(wfY))
    {
        wfDist = (int16_t)(abs(wfX) + 1);
        wfY = (int16_t)((256 * (int32_t)wfY) / abs(wfX));
        wfX = wfX < 0 ? -256 : 256;
    }
    else if (wfY != 0)
    {
        wfDist = (int16_t)(abs(wfY) + 1);
        wfX = (int16_t)((256 * (int32_t)wfX) / abs(wfY));
        wfY = wfY < 0 ? -256 : 256;
    }
    else wfDist = 0;
}

static void DibujarLinea(int32_t x1, int32_t y1, int16_t z1, int32_t x2, int32_t y2, int16_t z2, uint8_t color)
{
    wfX = (int16_t)x1; wfY = (int16_t)y1; wfZ = z1;
    wfEscala = Leer(0x1F90);
    wfX2 = Leer(0x1F86);
    wfY2 = Leer(0x1F87);
    wfDist = Leer(0x1F88);
    TransformarAlambre2();
    x1 = (wfX + 48) << 8;
    y1 = (wfY + 48) << 8;
    wfX = (int16_t)x2; wfY = (int16_t)y2; wfZ = z2;
    TransformarAlambre2();
    x2 = (wfX + 48) << 8;
    y2 = (wfY + 48) << 8;

    wfX = (int16_t)(x1 >> 8); wfY = (int16_t)(y1 >> 8);
    wfX2 = (int16_t)(x2 >> 8); wfY2 = (int16_t)(y2 >> 8);
    CalcularAlambre();
    x2 = wfX;
    y2 = wfY;

    for (int32_t i = wfDist ? wfDist : 1; i > 0; i--)
    {
        if (x1 > 0xFF && y1 > 0xFF && x1 < 0x6000 && y1 < 0x6000)
        {
            // Pixel en un mapa de tiles de 2 bits por pixel (12 tiles por fila).
            uint16_t a = (uint16_t)((((y1 >> 8) >> 3) << 8) - (((y1 >> 8) >> 3) << 6) + (((x1 >> 8) >> 3) << 4) + ((y1 >> 8) & 7) * 2);
            uint8_t bit = (uint8_t)(0x80 >> ((x1 >> 8) & 7));
            c.ram[(a + 0x300) % 0xC00] &= (uint8_t)~bit;
            c.ram[(a + 0x301) % 0xC00] &= (uint8_t)~bit;
            if (color & 1) c.ram[(a + 0x300) % 0xC00] |= bit;
            if (color & 2) c.ram[(a + 0x301) % 0xC00] |= bit;
        }
        x1 += x2;
        y1 += y2;
    }
}

static void DibujarAlambre()
{
    uint32_t linea = LeerL(0x1F80);
    uint32_t banco = (uint32_t)Leer(0x1F82) << 16;
    for (int32_t i = c.ram[0x295]; i > 0; i--, linea += 5)
    {
        uint32_t p1, p2;
        if (BusVer(linea) == 0xFF && BusVer(linea + 1) == 0xFF)
        {
            // Una línea que sigue a la anterior: arranca donde terminó la última que no era así.
            int32_t t = (int32_t)linea - 5;
            while (t + 2 >= 0 && BusVer(t + 2) == 0xFF && BusVer(t + 3) == 0xFF) t -= 5;
            p1 = banco | BusVer(t + 2) << 8 | BusVer(t + 3);
        }
        else p1 = banco | BusVer(linea) << 8 | BusVer(linea + 1);
        p2 = banco | BusVer(linea + 2) << 8 | BusVer(linea + 3);
        int16_t x1 = (int16_t)(BusVer(p1) << 8 | BusVer(p1 + 1)), y1 = (int16_t)(BusVer(p1 + 2) << 8 | BusVer(p1 + 3)), z1 = (int16_t)(BusVer(p1 + 4) << 8 | BusVer(p1 + 5));
        int16_t x2 = (int16_t)(BusVer(p2) << 8 | BusVer(p2 + 1)), y2 = (int16_t)(BusVer(p2 + 2) << 8 | BusVer(p2 + 3)), z2 = (int16_t)(BusVer(p2 + 4) << 8 | BusVer(p2 + 5));
        DibujarLinea(x1, y1, z1, x2, y2, z2, BusVer(linea + 4));
    }
}

// ---- Escala y giro de una imagen (4 bits por pixel, de 0600 a la RAM en tiles) ----

static void EscalarGirar(int relleno)
{
    int32_t ex = LeerW(0x1F8F), ey = LeerW(0x1F92);
    if (ex & 0x8000) ex = 0x7FFF;
    if (ey & 0x8000) ey = 0x7FFF;
    int16_t a, b, cc, d;
    uint16_t ang = LeerW(0x1F80);
    if (ang == 0) { a = (int16_t)ex; b = 0; cc = 0; d = (int16_t)ey; }
    else if (ang == 128) { a = 0; b = (int16_t)-ey; cc = (int16_t)ex; d = 0; }
    else if (ang == 256) { a = (int16_t)-ex; b = 0; cc = 0; d = (int16_t)-ey; }
    else if (ang == 384) { a = 0; b = (int16_t)ey; cc = (int16_t)-ex; d = 0; }
    else
    {
        a = (int16_t)((coseno16[ang & 0x1FF] * ex) >> 15);
        b = (int16_t)-((seno16[ang & 0x1FF] * ey) >> 15);
        cc = (int16_t)((seno16[ang & 0x1FF] * ex) >> 15);
        d = (int16_t)((coseno16[ang & 0x1FF] * ey) >> 15);
    }
    uint8_t w = Leer(0x1F89) & ~7, h = Leer(0x1F8C) & ~7;
    uint32_t borrar = (uint32_t)((w + relleno / 4) * h / 2);
    memset(c.ram, 0, borrar < sizeof c.ram ? borrar : sizeof c.ram);

    int32_t cx = (int16_t)LeerW(0x1F83), cy = (int16_t)LeerW(0x1F86);
    // 12 bits de fracción: se arranca en el punto que queda en (0, 0) de la salida.
    int32_t lineaX = (cx << 12) - cx * a - cx * b;
    int32_t lineaY = (cy << 12) - cy * cc - cy * d;
    int32_t sal = 0;
    uint8_t bit = 0x80;
    for (int32_t y = 0; y < h; y++)
    {
        uint32_t X = (uint32_t)lineaX, Y = (uint32_t)lineaY;
        for (int32_t x = 0; x < w; x++)
        {
            uint8_t px = 0;
            if ((X >> 12) < w && (Y >> 12) < h)
            {
                uint32_t dir = (Y >> 12) * w + (X >> 12);
                px = Leer(0x600 + (dir >> 1));
                if (dir & 1) px >>= 4;
            }
            // De un pixel de 4 bits a los cuatro planos del tile.
            if (px & 1) c.ram[sal % 0xC00] |= bit;
            if (px & 2) c.ram[(sal + 1) % 0xC00] |= bit;
            if (px & 4) c.ram[(sal + 16) % 0xC00] |= bit;
            if (px & 8) c.ram[(sal + 17) % 0xC00] |= bit;
            bit >>= 1;
            if (!bit) { bit = 0x80; sal += 32; }
            X += a;
            Y += cc;
        }
        sal += 2 + relleno;
        if (sal & 0x10) sal &= ~0x10;
        else sal -= w * 4 + relleno;
        lineaX += b;
        lineaY += d;
    }
}

// ---- Comandos ----

static void ArmarOam()   // 00-00: la tabla de sprites a partir de la lista de objetos
{
    uint32_t oam = c.ram[0x626] << 2;
    for (int32_t i = 0x1FD; i > (int32_t)oam && i >= 0; i -= 4) c.ram[i] = 0xE0;   // los que sobran, fuera de la pantalla
    uint16_t gx = LeerW(0x621), gy = LeerW(0x623);
    uint32_t oam2 = 0x200 + (c.ram[0x626] >> 2);
    if (!c.ram[0x620]) return;
    uint8_t quedan = (uint8_t)(128 - c.ram[0x626]);
    uint8_t despl = (c.ram[0x626] & 3) * 2;
    uint32_t src = 0x220;
    for (int i = c.ram[0x620]; i > 0 && quedan > 0; i--, src += 16)
    {
        int16_t sx = (int16_t)(LeerW(src) - gx), sy = (int16_t)(LeerW(src + 2) - gy);
        uint8_t nombre = c.ram[src + 5];
        uint8_t attr = c.ram[src + 4] | c.ram[src + 6];
        uint32_t lista = LeerL(src + 7);
        if (BusVer(lista))
        {
            for (int n = BusVer(lista++); n > 0 && quedan > 0; n--, lista += 4)
            {
                uint8_t f = BusVer(lista);
                int16_t x = (int8_t)BusVer(lista + 1);
                if (attr & 0x40) x = (int16_t)(-x - ((f & 0x20) ? 16 : 8));
                x = (int16_t)(x + sx);
                if (x < -16 || x > 272) continue;
                int16_t y = (int8_t)BusVer(lista + 2);
                if (attr & 0x80) y = (int16_t)(-y - ((f & 0x20) ? 16 : 8));
                y = (int16_t)(y + sy);
                if (y < -16 || y > 224) continue;
                c.ram[oam] = (uint8_t)x;
                c.ram[oam + 1] = (uint8_t)y;
                c.ram[oam + 2] = (uint8_t)(nombre + BusVer(lista + 3));
                c.ram[oam + 3] = attr ^ (f & 0xC0);
                c.ram[oam2] &= (uint8_t)~(3 << despl);
                if (x & 0x100) c.ram[oam2] |= (uint8_t)(1 << despl);
                if (f & 0x20) c.ram[oam2] |= (uint8_t)(2 << despl);
                oam += 4;
                quedan--;
                despl = (despl + 2) & 6;
                if (!despl) oam2++;
            }
        }
        else
        {
            c.ram[oam] = (uint8_t)sx;
            c.ram[oam + 1] = (uint8_t)sy;
            c.ram[oam + 2] = nombre;
            c.ram[oam + 3] = attr;
            c.ram[oam2] &= (uint8_t)~(3 << despl);
            c.ram[oam2] |= (uint8_t)(((sx & 0x100) ? 3 : 2) << despl);
            oam += 4;
            quedan--;
            despl = (despl + 2) & 6;
            if (!despl) oam2++;
        }
    }
}

static void TransformarLineas()   // 00-05
{
    wfX2 = Leer(0x1F83);
    wfY2 = Leer(0x1F86);
    wfDist = Leer(0x1F89);
    wfEscala = Leer(0x1F8C);
    uint32_t p = 0;
    for (int32_t i = LeerW(0x1F80); i > 0; i--, p += 0x10)   // vértices
    {
        wfX = (int16_t)LeerW(p + 1);
        wfY = (int16_t)LeerW(p + 5);
        wfZ = (int16_t)LeerW(p + 9);
        TransformarAlambre();
        EscribirW(p + 1, (uint16_t)(wfX + 0x80));
        EscribirW(p + 5, (uint16_t)(wfY + 0x50));
    }
    EscribirW(0x600, 23); EscribirW(0x602, 0x60); EscribirW(0x605, 0x40);
    EscribirW(0x608, 23); EscribirW(0x60A, 0x60); EscribirW(0x60D, 0x40);
    p = 0xB02;
    uint32_t p2 = 0;
    for (int32_t i = LeerW(0xB00); i > 0; i--, p += 2, p2 += 8)   // líneas entre vértices
    {
        wfX = (int16_t)LeerW((Leer(p) << 4) + 1);
        wfY = (int16_t)LeerW((Leer(p) << 4) + 5);
        wfX2 = (int16_t)LeerW((Leer(p + 1) << 4) + 1);
        wfY2 = (int16_t)LeerW((Leer(p + 1) << 4) + 5);
        CalcularAlambre();
        EscribirW(p2 + 0x600, (uint16_t)(wfDist ? wfDist : 1));
        EscribirW(p2 + 0x602, (uint16_t)wfX);
        EscribirW(p2 + 0x605, (uint16_t)wfY);
    }
}

static void Desintegrar()   // 00-0B
{
    uint8_t w = Leer(0x1F89), h = Leer(0x1F8C);
    int32_t cx = LeerW(0x1F80), cy = LeerW(0x1F83);
    int32_t ex = (int16_t)LeerW(0x1F86), ey = (int16_t)LeerW(0x1F8F);
    uint32_t x0 = (uint32_t)(-cx * ex + (cx << 8)), y0 = (uint32_t)(-cy * ey + (cy << 8));
    uint32_t src = 0x600;
    for (int32_t i = 0; i < (w * h) >> 1; i++) Escribir((uint32_t)i, 0);
    uint32_t y = y0;
    for (int32_t i = 0; i < h; i++, y += ey)
    {
        uint32_t x = x0;
        for (int32_t j = 0; j < w; j++, x += ex)
        {
            if ((x >> 8) < w && (y >> 8) < h && (y >> 8) * w + (x >> 8) < 0x2000)
            {
                uint8_t px = (j & 1) ? (uint8_t)(c.ram[src % 0xC00] >> 4) : c.ram[src % 0xC00];
                int32_t idx = (int32_t)((y >> 11) * w * 4 + (x >> 11) * 32 + ((y >> 8) & 7) * 2);
                uint8_t m = (uint8_t)(0x80 >> ((x >> 8) & 7));
                if (px & 1) c.ram[idx % 0xC00] |= m;
                if (px & 2) c.ram[(idx + 1) % 0xC00] |= m;
                if (px & 4) c.ram[(idx + 16) % 0xC00] |= m;
                if (px & 8) c.ram[(idx + 17) % 0xC00] |= m;
            }
            if (j & 1) src++;
        }
    }
}

static void OndaPlanos()   // 00-0C: ondula una imagen de planos de bits según una tabla de alturas
{
    uint32_t dest = 0, onda = Leer(0x1F83);
    uint16_t m1 = 0xC0C0, m2 = 0x3F3F;
    for (int j = 0; j < 0x10; j++)
    {
        for (int mitad = 0; mitad < 2; mitad++)
        {
            do
            {
                int16_t alto = (int16_t)(-((int8_t)Leer(onda + 0xB00)) - 16);
                for (int i = 0; i < 40; i++)
                {
                    uint32_t a = dest + (i >> 3) * 0x200 + (i & 7) * 2;
                    uint16_t t = LeerW(a) & m2;
                    if (alto >= 0) t |= m1 & (alto < 8 ? LeerW((mitad ? 0xA10 : 0xA00) + alto * 2) : 0xFF00);
                    EscribirW(a, t);
                    alto++;
                }
                onda = (onda + 1) & 0x7F;
                m1 = (uint16_t)((m1 >> 2) | (m1 << 6));
                m2 = (uint16_t)((m2 >> 2) | (m2 << 6));
            } while (m1 != 0xC0C0);
            dest += 16;
        }
    }
}

static int32_t Tangente(int a) { return coseno16[a] ? (((int32_t)seno16[a]) << 16) / coseno16[a] : (int32_t)0x80000000; }

static void Comando(uint8_t op)
{
    switch (op)
    {
    case 0x00:   // sprites
        switch (c.reg[0x4D])
        {
        case 0x00: ArmarOam(); break;
        case 0x03: EscalarGirar(0); break;
        case 0x05: TransformarLineas(); break;
        case 0x07: EscalarGirar(64); break;
        case 0x08: DibujarAlambre(); break;
        case 0x0B: Desintegrar(); break;
        case 0x0C: OndaPlanos(); break;
        }
        break;
    case 0x01:   // alambre sobre un fondo limpio
        memset(c.ram + 0x300, 0, 2304);
        DibujarAlambre();
        break;
    case 0x05:   // propulsión
    {
        int32_t t = 0x10000;
        if (LeerW(0x1F83)) t = ((t / LeerW(0x1F83)) * LeerW(0x1F81)) >> 8;
        EscribirW(0x1F80, (uint16_t)t);
        break;
    }
    case 0x0D:   // largo de un vector
    {
        int16_t x = (int16_t)LeerW(0x1F80), y = (int16_t)LeerW(0x1F83), dist = (int16_t)LeerW(0x1F86);
        double t = sqrt((double)y * y + (double)x * x);
        t = (double)dist / t;
        y = (int16_t)(((double)y * t) * 0.99);
        x = (int16_t)(((double)x * t) * 0.98);
        EscribirW(0x1F89, (uint16_t)x);
        EscribirW(0x1F8C, (uint16_t)y);
        break;
    }
    case 0x10:   // triángulo (8 bits de fracción)
    case 0x13:   // triángulo (16 bits de fracción)
    {
        c.r0 = Ldr(0);
        c.r1 = Ldr(1);
        c.r4 = c.r0 & 0x1FF;
        if (op == 0x10) c.r1 = (c.r1 & 0x8000) ? (c.r1 | ~0x7FFFu) : (c.r1 & 0x7FFF);
        Mul(Coseno(c.r4), c.r1, c.r5, c.r2);
        if (op == 0x10) { c.r5 = (c.r5 >> 16) & 0xFF; c.r2 = (c.r2 << 8) + c.r5; }
        else { c.r5 = (c.r5 >> 8) & 0xFFFF; c.r2 = (c.r2 << 16) + c.r5; }
        Mul(Seno(c.r4), c.r1, c.r5, c.r3);
        if (op == 0x10) { c.r5 = (c.r5 >> 16) & 0xFF; c.r3 = (c.r3 << 8) + c.r5; }
        else { c.r5 = (c.r5 >> 8) & 0xFFFF; c.r3 = (c.r3 << 16) + c.r5; }
        Str(0, c.r0); Str(1, c.r1); Str(2, c.r2); Str(3, c.r3); Str(4, c.r4); Str(5, c.r5);
        break;
    }
    case 0x15:   // hipotenusa
    {
        int16_t x = (int16_t)LeerW(0x1F80), y = (int16_t)LeerW(0x1F83);
        EscribirW(0x1F80, (uint16_t)(int16_t)sqrt((double)x * x + (double)y * y));
        break;
    }
    case 0x1F:   // ángulo de un vector (512 por vuelta)
    {
        int16_t x = (int16_t)LeerW(0x1F80), y = (int16_t)LeerW(0x1F83), ang;
        if (!x) ang = y > 0 ? 0x080 : 0x180;
        else
        {
            ang = (int16_t)(atan((double)y / (double)x) / (M_PI * 2) * 512);
            if (x < 0) ang = (int16_t)(ang + 0x100);
            ang &= 0x1FF;
        }
        EscribirW(0x1F86, (uint16_t)ang);
        break;
    }
    case 0x22:   // trapecio: bordes izquierdo y derecho de cada línea de pantalla
    {
        int32_t t1 = Tangente(LeerW(0x1F8C) & 0x1FF), t2 = Tangente(LeerW(0x1F8F) & 0x1FF);
        int16_t y = (int16_t)(LeerW(0x1F83) - LeerW(0x1F89));
        for (int32_t j = 0; j < 225; j++, y++)
        {
            int16_t izq, der;
            if (y >= 0)
            {
                izq = (int16_t)(((int32_t)((uint32_t)t1 * (uint32_t)(int32_t)y) >> 16) - LeerW(0x1F80) + LeerW(0x1F86));
                der = (int16_t)(((int32_t)((uint32_t)t2 * (uint32_t)(int32_t)y) >> 16) - LeerW(0x1F80) + LeerW(0x1F86) + LeerW(0x1F93));
                if (izq < 0 && der < 0) { izq = 1; der = 0; }
                else if (izq < 0) izq = 0;
                else if (der < 0) der = 0;
                if (izq > 255 && der > 255) { izq = 255; der = 254; }
                else if (izq > 255) izq = 255;
                else if (der > 255) der = 255;
            }
            else { izq = 1; der = 0; }
            c.ram[j + 0x800] = (uint8_t)izq;
            c.ram[j + 0x900] = (uint8_t)der;
        }
        break;
    }
    case 0x25:   // multiplicación
        c.r0 = Ldr(0); c.r1 = Ldr(1);
        Mul(c.r0, c.r1, c.r0, c.r1);
        Str(0, c.r0); Str(1, c.r1);
        break;
    case 0x2D:   // transformar coordenadas
        wfX = (int16_t)LeerW(0x1F81);
        wfY = (int16_t)LeerW(0x1F84);
        wfZ = (int16_t)LeerW(0x1F87);
        wfX2 = Leer(0x1F89);
        wfY2 = Leer(0x1F8A);
        wfDist = Leer(0x1F8B);
        wfEscala = (int16_t)LeerW(0x1F90);
        TransformarAlambre2();
        EscribirW(0x1F80, (uint16_t)wfX);
        EscribirW(0x1F83, (uint16_t)wfY);
        break;
    case 0x40:   // suma de la RAM
        c.r0 = 0;
        for (int i = 0; i < 0x800; i++) c.r0 += c.ram[i];
        Str(0, c.r0);
        break;
    case 0x54:   // cuadrado
        c.r0 = Ldr(0);
        Mul(c.r0, c.r0, c.r1, c.r2);
        Str(1, c.r1); Str(2, c.r2);
        break;
    case 0x5C: Str(0, 0); InmediatoRegistro(0); break;
    case 0x89: Str(0, 0x054336); Str(1, 0xFFFFFF); break;   // datos fijos de la rom del chip
    default:
        if (op >= 0x5E && op <= 0x7C && !(op & 1)) InmediatoRegistro((op - 0x5E) / 2 * 3);   // 5E, 60... 7C
        break;
    }
}

static void Escribir(uint32_t a, uint8_t v)
{
    a &= 0x1FFF;
    if (a < 0xC00) { c.ram[a] = v; return; }
    if (a < 0x1F00) return;
    c.reg[a & 0xFF] = v;
    if (a == 0x1F47)   // copia de la memoria de la SNES a la RAM del chip
    {
        uint32_t src = c.reg[0x40] | c.reg[0x41] << 8 | c.reg[0x42] << 16;
        uint16_t n = (uint16_t)(c.reg[0x43] | c.reg[0x44] << 8), dest = (uint16_t)(c.reg[0x45] | c.reg[0x46] << 8);
        for (uint32_t i = 0; i < n; i++) Escribir(dest++, BusVer(src++));
        return;
    }
    if (a == 0x1F4F)
    {
        if (c.reg[0x4D] == 0x0E && !(v & 0xC3)) { c.reg[0x80] = v >> 2; return; }   // comando de prueba
        PrepararTablas();
        Comando(v);
    }
}

void Cx4Reiniciar()
{
    PrepararTablas();
    memset(&c, 0, sizeof c);
}

// 6000-6BFF RAM, 7000-7BFF la misma RAM, 7F00-7FFF registros.
static inline uint32_t Direccion(uint16_t off)
{
    uint32_t a = off & 0x1FFF;
    if (a >= 0x1000 && a < 0x1C00) a -= 0x1000;
    return a;
}

uint8_t Cx4Leer(uint16_t off, uint8_t abierto)
{
    uint32_t a = Direccion(off);
    if (a < 0xC00) return c.ram[a];
    if (a >= 0x1F00) return c.reg[a & 0xFF];
    return abierto;
}

void Cx4Escribir(uint16_t off, uint8_t v) { Escribir(Direccion(off), v); }

void Cx4Estado(Serial& s) { s(c); }

#ifdef CX4_TABLAS
// Para comparar las tablas generadas con las del chip (pc/diag).
const uint32_t* Cx4TablaSeno24() { PrepararTablas(); return seno24; }
const int16_t* Cx4TablaSeno16() { PrepararTablas(); return seno16; }
const int16_t* Cx4TablaCoseno16() { PrepararTablas(); return coseno16; }
#endif
