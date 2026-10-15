// La PPU: el chip de gráficos de la SNES. Guarda la memoria de video (VRAM), la paleta (CGRAM) y los sprites (OAM), y
// dibuja cada línea de la imagen como la consola real:
//   - Los 8 modos de video (0 a 7), incluidos el modo 7 (rotación/zoom), los de alta resolución (5 y 6) y el
//     desplazamiento por columna (modos 2, 4 y 6).
//   - Dos pantallas, la principal y la secundaria, y la mezcla de colores entre ellas (sumas, restas, mitades: es lo que
//     hace las transparencias, sombras, niebla y degradados).
//   - Ventanas (recortes por columna), mosaico, color directo y brillo.
//   - Sprites con el orden real: entre sprites gana el de número más bajo, y su prioridad decide contra los fondos;
//     hasta 32 sprites y 34 mosaicos por línea, como el chip.
// El color de la SNES es de 15 bits (BGR555) y se convierte a 0xRRGGBB al final.
#include "interno.h"
#include <stdlib.h>

#include <stdio.h>

void SnesContadores(int* h, int* v);   // snes.cpp: posición actual del haz (punto y línea)

static uint8_t vram[64 * 1024];
static uint16_t cgram[256];
static uint8_t oam[544];

// ---------- registros ----------
static uint8_t inidisp, obsel, bgmode, mosaico, setini;
static uint16_t oamAdd;          // OAMADD tal como la escribió el juego (dirección de palabra, 9 bits)
static uint16_t oamDir;          // dirección interna en bytes (0..0x3FF)
static uint8_t oamLatch;
static bool oamRotar;            // bit 7 de $2103: el sprite de mayor prioridad es el que apunta OAMADD
static uint8_t bgsc[4], bgnba[2];
static uint16_t hofs[4], vofs[4];
static uint8_t ofsLatch, ofsLatchH;
static int16_t m7hofs, m7vofs;
static uint8_t vmain;
static uint16_t vmadd, vramLatch;
static uint8_t m7sel, m7Latch;
static int16_t m7[6];            // A B C D X Y
static uint8_t cgadd, cgBuf;
static bool cgAlto;
static uint8_t w12sel, w34sel, wobjsel, wh[4], wbglog, wobjlog;
static uint8_t tm, ts, tmw, tsw;
static uint8_t cgwsel, cgadsub;
static uint8_t fijoR, fijoG, fijoB;
static uint16_t hLatch, vLatch;
static bool hAlto, vAlto, contadorLatch;
static uint8_t ppu1Bus, ppu2Bus;
static bool rangoExcedido, tiempoExcedido;

void PpuReiniciar()
{
    memset(vram, 0, sizeof vram);
    memset(cgram, 0, sizeof cgram);
    memset(oam, 0, sizeof oam);
    inidisp = 0x80; obsel = bgmode = mosaico = setini = 0;
    oamAdd = oamDir = 0; oamLatch = 0; oamRotar = false;
    memset(bgsc, 0, sizeof bgsc); memset(bgnba, 0, sizeof bgnba);
    memset(hofs, 0, sizeof hofs); memset(vofs, 0, sizeof vofs);
    ofsLatch = ofsLatchH = 0; m7hofs = m7vofs = 0;
    vmain = 0; vmadd = vramLatch = 0;
    m7sel = m7Latch = 0; memset(m7, 0, sizeof m7);
    cgadd = cgBuf = 0; cgAlto = false;
    w12sel = w34sel = wobjsel = wbglog = wobjlog = 0; memset(wh, 0, sizeof wh);
    tm = ts = tmw = tsw = 0;
    cgwsel = cgadsub = 0; fijoR = fijoG = fijoB = 0;
    hLatch = vLatch = 0; hAlto = vAlto = contadorLatch = false;
    ppu1Bus = ppu2Bus = 0;
    rangoExcedido = tiempoExcedido = false;
}

void PpuNuevoCuadro() { rangoExcedido = tiempoExcedido = false; }
// Al empezar el vblank se recarga la dirección de los sprites (si la pantalla no está apagada a la fuerza).
void PpuVblank() { if (!(inidisp & 0x80)) oamDir = oamAdd << 1; }
bool PpuEntrelazado() { return setini & 1; }
bool PpuSobrebarrido() { return setini & 4; }

// Dirección de VRAM según VMAIN: los bits 2-3 reordenan la dirección (sirve para escribir gráficos de 2/4/8 bpp por
// filas). Devuelve una dirección de palabra (15 bits).
static inline uint16_t DirVram()
{
    uint16_t a = vmadd;
    switch ((vmain >> 2) & 3)
    {
    case 1: a = (a & 0xFF00) | ((a & 0x00E0) >> 5) | ((a & 0x001F) << 3); break;
    case 2: a = (a & 0xFE00) | ((a & 0x01C0) >> 6) | ((a & 0x003F) << 3); break;
    case 3: a = (a & 0xFC00) | ((a & 0x0380) >> 7) | ((a & 0x007F) << 3); break;
    }
    return a & 0x7FFF;
}
static inline uint16_t PalabraVram(uint32_t w) { w &= 0x7FFF; return vram[w * 2] | vram[w * 2 + 1] << 8; }
static inline void AvanzarVram() { static const uint16_t PASO[4] = {1, 32, 128, 128}; vmadd += PASO[vmain & 3]; }

// La lectura de la PPU devuelve 'abierto' (el último valor del bus) en los bits que el chip no maneja.
uint8_t PpuLeer(uint16_t reg, uint8_t abierto)
{
    switch (reg)
    {
    case 0x2134: case 0x2135: case 0x2136:
    {
        // Resultado de multiplicar M7A (16 bits con signo) por el byte alto de M7B (8 bits con signo).
        int32_t r = (int32_t)m7[0] * (int8_t)(m7[1] >> 8);
        return ppu1Bus = (uint8_t)(r >> ((reg - 0x2134) * 8));
    }
    case 0x2137:
    {
        int h, v;
        SnesContadores(&h, &v);
        hLatch = h; vLatch = v; contadorLatch = true;
        return abierto;
    }
    case 0x2138:
    {
        uint8_t v = (oamDir & 0x200) ? oam[0x200 | (oamDir & 0x1F)] : oam[oamDir & 0x1FF];
        oamDir = (oamDir + 1) & 0x3FF;
        return ppu1Bus = v;
    }
    case 0x2139:
    {
        uint8_t v = vramLatch & 0xFF;
        if (!(vmain & 0x80)) { vramLatch = PalabraVram(DirVram()); AvanzarVram(); }
        return ppu1Bus = v;
    }
    case 0x213A:
    {
        uint8_t v = vramLatch >> 8;
        if (vmain & 0x80) { vramLatch = PalabraVram(DirVram()); AvanzarVram(); }
        return ppu1Bus = v;
    }
    case 0x213B:
    {
        uint8_t v;
        if (!cgAlto) v = cgram[cgadd] & 0xFF;
        else { v = ((cgram[cgadd] >> 8) & 0x7F) | (ppu2Bus & 0x80); cgadd++; }
        cgAlto = !cgAlto;
        return ppu2Bus = v;
    }
    case 0x213C:
    {
        uint8_t v = hAlto ? (((hLatch >> 8) & 1) | (ppu2Bus & 0xFE)) : (hLatch & 0xFF);
        hAlto = !hAlto;
        return ppu2Bus = v;
    }
    case 0x213D:
    {
        uint8_t v = vAlto ? (((vLatch >> 8) & 1) | (ppu2Bus & 0xFE)) : (vLatch & 0xFF);
        vAlto = !vAlto;
        return ppu2Bus = v;
    }
    case 0x213E:
        return ppu1Bus = (tiempoExcedido ? 0x80 : 0) | (rangoExcedido ? 0x40 : 0) | (ppu1Bus & 0x10) | 0x01;
    case 0x213F:
    {
        uint8_t v = (contadorLatch ? 0x40 : 0) | (SnesInfo().pal ? 0x10 : 0) | (ppu2Bus & 0x20) | 0x03;
        hAlto = vAlto = false;
        contadorLatch = false;
        return ppu2Bus = v;
    }
    }
    // Registros de solo escritura: lo que haya en el bus interno de la PPU.
    if ((reg >= 0x2104 && reg <= 0x2106) || (reg >= 0x2108 && reg <= 0x210A) || (reg >= 0x2114 && reg <= 0x2116) ||
        (reg >= 0x2118 && reg <= 0x211A) || (reg >= 0x2124 && reg <= 0x2126) || (reg >= 0x2128 && reg <= 0x212A))
        return ppu1Bus;
    return abierto;
}

// Contadores del haz por el puerto de E/S ($4201 bit 7 al bajar): igual que leer $2137.
void PpuLatchContadores() { int h, v; SnesContadores(&h, &v); hLatch = h; vLatch = v; contadorLatch = true; }

void PpuEscribir(uint16_t reg, uint8_t v)
{
#ifdef GSU_TRAZA
    static int log_ = getenv("PPUREG") ? 1 : 0;
    extern int depurCuadro;
    if (log_ && reg >= 0x2105 && reg <= 0x2133 && depurCuadro) { int h, l; SnesContadores(&h, &l); printf("  ppu %04X=%02X linea %d\n", reg, v, l); }
#endif
    switch (reg)
    {
    case 0x2100:
        // Al encender la pantalla justo en el borde del vblank, el chip recarga la dirección de los sprites.
        if ((inidisp & 0x80) && !(v & 0x80)) { int h, l; SnesContadores(&h, &l); if (l == (PpuSobrebarrido() ? 240 : 225)) oamDir = oamAdd << 1; }
        inidisp = v;
        break;
    case 0x2101: obsel = v; break;
    case 0x2102: oamAdd = (oamAdd & 0x100) | v; oamDir = oamAdd << 1; break;
    case 0x2103: oamAdd = (oamAdd & 0xFF) | ((v & 1) << 8); oamRotar = v & 0x80; oamDir = oamAdd << 1; break;
    case 0x2104:
        if (oamDir & 0x200) oam[0x200 + (oamDir & 0x1F)] = v;   // tabla alta: byte a byte
        else
        {
            // Tabla baja: se escribe de a palabras (el primer byte espera al segundo).
            if (!(oamDir & 1)) oamLatch = v;
            else { oam[(oamDir - 1) & 0x1FF] = oamLatch; oam[oamDir & 0x1FF] = v; }
        }
        oamDir = (oamDir + 1) & 0x3FF;
        break;
    case 0x2105: bgmode = v; break;
    case 0x2106: mosaico = v; break;
    case 0x2107: case 0x2108: case 0x2109: case 0x210A: bgsc[reg - 0x2107] = v; break;
    case 0x210B: bgnba[0] = v; break;
    case 0x210C: bgnba[1] = v; break;
    case 0x210D:
        m7hofs = (int16_t)((v << 8) | m7Latch); m7Latch = v;
        hofs[0] = ((v << 8) | (ofsLatch & ~7) | (ofsLatchH & 7)) & 0x3FF; ofsLatch = v; ofsLatchH = v;
        break;
    case 0x210E:
        m7vofs = (int16_t)((v << 8) | m7Latch); m7Latch = v;
        vofs[0] = ((v << 8) | ofsLatch) & 0x3FF; ofsLatch = v;
        break;
    case 0x210F: case 0x2111: case 0x2113:
        hofs[(reg - 0x210D) / 2] = ((v << 8) | (ofsLatch & ~7) | (ofsLatchH & 7)) & 0x3FF; ofsLatch = v; ofsLatchH = v; break;
    case 0x2110: case 0x2112: case 0x2114:
        vofs[(reg - 0x210E) / 2] = ((v << 8) | ofsLatch) & 0x3FF; ofsLatch = v; break;
    case 0x2115: vmain = v; break;
    case 0x2116: vmadd = (vmadd & 0xFF00) | v; vramLatch = PalabraVram(DirVram()); break;
    case 0x2117: vmadd = (vmadd & 0x00FF) | (v << 8); vramLatch = PalabraVram(DirVram()); break;
    case 0x2118: vram[DirVram() * 2] = v; if (!(vmain & 0x80)) AvanzarVram(); break;
    case 0x2119: vram[DirVram() * 2 + 1] = v; if (vmain & 0x80) AvanzarVram(); break;
    case 0x211A: m7sel = v; break;
    case 0x211B: case 0x211C: case 0x211D: case 0x211E: case 0x211F: case 0x2120:
        m7[reg - 0x211B] = (int16_t)((v << 8) | m7Latch); m7Latch = v; break;
    case 0x2121: cgadd = v; cgAlto = false; break;
    case 0x2122:
        if (!cgAlto) cgBuf = v;
        else cgram[cgadd++] = (cgBuf | (v << 8)) & 0x7FFF;
        cgAlto = !cgAlto;
        break;
    case 0x2123: w12sel = v; break;
    case 0x2124: w34sel = v; break;
    case 0x2125: wobjsel = v; break;
    case 0x2126: case 0x2127: case 0x2128: case 0x2129: wh[reg - 0x2126] = v; break;
    case 0x212A: wbglog = v; break;
    case 0x212B: wobjlog = v; break;
    case 0x212C: tm = v; break;
    case 0x212D: ts = v; break;
    case 0x212E: tmw = v; break;
    case 0x212F: tsw = v; break;
    case 0x2130: cgwsel = v; break;
    case 0x2131: cgadsub = v; break;
    case 0x2132:
        if (v & 0x20) fijoR = v & 31;
        if (v & 0x40) fijoG = v & 31;
        if (v & 0x80) fijoB = v & 31;
        break;
    case 0x2133: setini = v; break;
    }
}

// ---------- dibujo ----------
// Cada capa deja su línea en un búfer: color BGR555 y "altura" z (0 = transparente; más alto = más adelante). Las
// alturas salen de la tabla de prioridades de cada modo, así comparar dos capas es comparar dos números.
enum { L_BG1, L_BG2, L_BG3, L_BG4, L_OBJ, L_FONDO };
struct LineaCapa { uint16_t color[512]; uint8_t z[512]; };
static LineaCapa capas[4];
static uint16_t objColor[256];
static uint8_t objZ[256], objMate[256];   // objMate: paleta 4..7 (solo esas participan en la mezcla de color)

// Alturas por modo: [modo][capa][bit de prioridad] y la de los sprites por prioridad 0..3.
struct TablaModo { uint8_t bg[4][2]; uint8_t obj[4]; };
static const TablaModo MODO0 = {{{8, 11}, {7, 10}, {2, 5}, {1, 4}}, {3, 6, 9, 12}};
static const TablaModo MODO1 = {{{6, 9}, {5, 8}, {1, 3}, {0, 0}}, {2, 4, 7, 10}};
static const TablaModo MODO1_BG3 = {{{5, 8}, {4, 7}, {2, 10}, {0, 0}}, {1, 3, 6, 9}};   // BG3 adelante de todo
static const TablaModo MODO2A5 = {{{3, 7}, {1, 5}, {0, 0}, {0, 0}}, {2, 4, 6, 8}};
static const TablaModo MODO6 = {{{2, 5}, {0, 0}, {0, 0}, {0, 0}}, {1, 3, 4, 6}};
static const TablaModo MODO7 = {{{3, 3}, {1, 5}, {0, 0}, {0, 0}}, {2, 4, 6, 7}};

// Color directo (fondos de 8 bits con el bit 0 de CGWSEL): el índice es el color mismo (BBGGGRRR) y la paleta del
// mosaico aporta un bit más a cada componente.
static inline uint16_t ColorDirecto(int c, int pal)
{
    int r = ((c & 7) << 2) | ((pal & 1) << 1);
    int g = (((c >> 3) & 7) << 2) | (pal & 2);
    int b = (((c >> 6) & 3) << 3) | (pal & 4);
    return (uint16_t)(r | g << 5 | b << 10);
}

// Una fila de 8 píxeles de un mosaico (sin voltear). base y tile en palabras; bpp 2/4/8.
// Tabla para pasar de planos a píxeles: para cada byte de un plano, los 8 bits repartidos en 8 bytes (el píxel de la
// izquierda en el byte 0). Así una fila de 8 píxeles se arma con una suma de desplazamientos de 64 bits por plano.
static uint64_t planoAPixeles[256];
static void PrepararPlanos()
{
    for (int b = 0; b < 256; b++)
    {
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) if (b & (0x80 >> i)) v |= (uint64_t)1 << (i * 8);
        planoAPixeles[b] = v;
    }
}

static inline void FilaMosaico(uint32_t dirPalabra, int bpp, uint8_t out[8])
{
    uint64_t v = 0;
    for (int k = 0; k < bpp; k += 2)
    {
        uint32_t w = (dirPalabra + (k / 2) * 8) & 0x7FFF;
        v |= planoAPixeles[vram[w * 2]] << k;
        v |= planoAPixeles[vram[w * 2 + 1]] << (k + 1);
    }
    memcpy(out, &v, 8);
}

static inline uint32_t BaseCaracteres(int capa)
{
    return (uint32_t)(((capa < 2 ? bgnba[0] >> (capa * 4) : bgnba[1] >> ((capa - 2) * 4)) & 0xF) << 12);
}

// Entrada del mapa de una capa en la posición (px, py) en píxeles del fondo (ya con el desplazamiento aplicado).
static inline uint16_t EntradaMapa(int capa, int px, int py, int tw, int th)
{
    uint8_t sc = bgsc[capa];
    uint32_t mapa = (uint32_t)(sc & 0xFC) << 8;
    int tx = (px >> (tw == 16 ? 4 : 3)) & 63, ty = (py >> (th == 16 ? 4 : 3)) & 63;
    if (!(sc & 1)) tx &= 31;
    if (!(sc & 2)) ty &= 31;
    uint32_t off = (ty & 31) * 32 + (tx & 31);
    if (tx & 32) off += 0x400;
    if (ty & 32) off += (sc & 1) ? 0x800 : 0x400;
    return PalabraVram(mapa + off);
}

// Dibuja una capa de fondo de mosaicos. V = línea del haz (1..224). ancho 256, o 512 en alta resolución.
static void DibujarFondo(int capa, int bpp, const uint8_t z[2], int V, bool hires, bool porColumna, int basePaleta)
{
    LineaCapa& L = capas[capa];
    int ancho = hires ? 512 : 256;
    bool t16 = bgmode & (0x10 << capa);
    int tw = (t16 || hires) ? 16 : 8, th = t16 ? 16 : 8;
    int mapW = ((bgsc[capa] & 1) ? 64 : 32) * tw, mapH = ((bgsc[capa] & 2) ? 64 : 32) * th;
    uint32_t car = BaseCaracteres(capa);
    int tam = (mosaico & (1 << capa)) ? (mosaico >> 4) + 1 : 1;
    int y = V;
    if (tam > 1) y = V - (V - 1) % tam;
    int hs = hofs[capa], vs = vofs[capa];
    if (hires) hs <<= 1;
    bool directo = (bpp == 8) && (cgwsel & 1);
    // Desplazamiento por columna: la capa 3 guarda un desplazamiento por cada columna de mosaicos.
    bool bg3t16 = bgmode & 0x40;
    int tw3 = bg3t16 ? 16 : 8, th3 = bg3t16 ? 16 : 8;
    int modo = bgmode & 7;
    uint16_t bit = 0x2000 << capa;

    if (!porColumna && tam == 1)
    {
        // Camino rápido (el caso normal): de a tramos de hasta 8 píxeles que comparten la entrada del mapa y la fila
        // del mosaico. Una sola búsqueda en el mapa y una decodificación por tramo.
        int py = (y + vs) & (mapH - 1);
        int fyMosaico = py & (th - 1);
        int x = 0, px = hs;
        while (x < ancho)
        {
            int pxm = px & (mapW - 1);
            uint16_t ent = EntradaMapa(capa, pxm, py, tw, th);
            int fxt = pxm & (tw - 1);
            int n = 8 - (fxt & 7);
            if (n > ancho - x) n = ancho - x;
            bool vx = ent & 0x4000;
            int fy = (ent & 0x8000) ? th - 1 - fyMosaico : fyMosaico;
            int fxIni = vx ? tw - 1 - fxt : fxt;
            int tile = ((ent & 0x3FF) + (fxIni >= 8 ? 1 : 0) + (fy >= 8 ? 16 : 0)) & 0x3FF;
            uint8_t f[8];
            FilaMosaico(car + tile * bpp * 4 + (fy & 7), bpp, f);
            int pal = (ent >> 10) & 7;
            uint8_t zz = z[(ent >> 13) & 1];
            const uint16_t* paleta = cgram + ((basePaleta + (bpp == 8 ? 0 : pal << bpp)) & 0xFF);
            int f0 = fxt & 7;
            for (int k = 0; k < n; k++)
            {
                int fx = vx ? 7 - (f0 + k) : f0 + k;
                int c = f[fx];
                if (!c) { L.z[x + k] = 0; continue; }
                L.z[x + k] = zz;
                L.color[x + k] = directo ? ColorDirecto(c, pal) : paleta[c];
            }
            x += n;
            px += n;
        }
        return;
    }

    int ultimoTile = -1;
    uint8_t fila[8] = {0};
    for (int x = 0; x < ancho; x++)
    {
        int xm = tam > 1 ? x - x % (hires ? tam * 2 : tam) : x;
        int px = xm + hs, py = y + vs;
        if (porColumna)
        {
            int ox = xm + (hs & 7);
            if (ox >= 8)
            {
                int bx = (ox - 8) + (hofs[2] & ~7);
                uint16_t hval = EntradaMapa(2, bx, vofs[2], tw3, th3);
                if (modo == 4)
                {
                    if (hval & bit)
                    {
                        if (!(hval & 0x8000)) px = ox + (hval & 0x3F8);
                        else py = y + (hval & 0x3FF);
                    }
                }
                else
                {
                    uint16_t vval = EntradaMapa(2, bx, vofs[2] + 8, tw3, th3);
                    if (hval & bit) px = ox + (hval & 0x3F8);
                    if (vval & bit) py = y + (vval & 0x3FF);
                }
            }
        }
        px &= mapW - 1; py &= mapH - 1;
        uint16_t ent = EntradaMapa(capa, px, py, tw, th);
        int fx = px % tw, fy = py % th;
        if (ent & 0x4000) fx = tw - 1 - fx;
        if (ent & 0x8000) fy = th - 1 - fy;
        int tile = ((ent & 0x3FF) + (fx >= 8 ? 1 : 0) + (fy >= 8 ? 16 : 0)) & 0x3FF;
        // Se decodifica la fila del mosaico una sola vez por cada 8 píxeles.
        int clave = tile << 3 | (fy & 7);
        if (clave != ultimoTile)
        {
            FilaMosaico(car + tile * bpp * 4 + (fy & 7), bpp, fila);
            ultimoTile = clave;
        }
        int c = fila[fx & 7];
        if (!c) { L.z[x] = 0; continue; }
        int pal = (ent >> 10) & 7;
        L.z[x] = z[(ent >> 13) & 1];
        if (directo) L.color[x] = ColorDirecto(c, pal);
        else L.color[x] = cgram[(basePaleta + (bpp == 8 ? 0 : pal << bpp) + c) & 0xFF];
    }
}

// Modo 7: un único fondo de 1024×1024 que se gira y agranda con la matriz A B C D alrededor del centro X Y.
// extbg: la capa 2 del modo 7 usa el bit 7 del píxel como prioridad.
static void DibujarModo7(int capa, const uint8_t z[2], int V)
{
    LineaCapa& L = capas[capa];
    int tam = (mosaico & (1 << capa)) ? (mosaico >> 4) + 1 : 1;
    int y = V;
    if (tam > 1) y = V - (V - 1) % tam;
    // Los valores de 13 bits con signo.
    auto s13 = [](int v) { return (v & 0x1FFF) - ((v & 0x1000) << 1); };
    int a = m7[0], b = m7[1], c = m7[2], d = m7[3];
    int cx = s13(m7[4]), cy = s13(m7[5]);
    int hof = s13(m7hofs), vof = s13(m7vofs);
    auto recorte = [](int v) { return (v & 0x2000) ? (v | ~1023) : (v & 1023); };
    if (m7sel & 2) y = 255 - y;
    int ox = recorte(hof - cx), oy = recorte(vof - cy);
    int psx = ((a * ox) & ~63) + ((b * oy) & ~63) + ((b * y) & ~63) + (cx << 8);
    int psy = ((c * ox) & ~63) + ((d * oy) & ~63) + ((d * y) & ~63) + (cy << 8);
    bool directo = capa == 0 && (cgwsel & 1);
    int fuera = m7sel >> 6;
#ifdef GSU_TRAZA
    { extern int depurCuadro; static int l_ = getenv("MODO7") ? 1 : 0;
      if (l_ && depurCuadro && V % 20 == 0) printf("m7 V=%d a=%d b=%d c=%d d=%d cx=%d cy=%d hof=%d vof=%d psx=%d psy=%d sel=%02X\n", V, a, b, c, d, cx, cy, hof, vof, psx, psy, m7sel); }
#endif
    for (int x = 0; x < 256; x++)
    {
        int xm = tam > 1 ? x - x % tam : x;
        int X = (m7sel & 1) ? 255 - xm : xm;
        int px = (psx + a * X) >> 8, py = (psy + c * X) >> 8;
        int tile;
        if ((px | py) & ~1023)
        {
            if (fuera == 2) { L.z[x] = 0; continue; }   // afuera del mapa: transparente
            if (fuera == 3) tile = 0;                    // afuera del mapa: repite el mosaico 0
            else { px &= 1023; py &= 1023; tile = vram[(((py >> 3) * 128 + (px >> 3)) & 0x7FFF) * 2]; }
        }
        else tile = vram[(((py >> 3) * 128 + (px >> 3)) & 0x7FFF) * 2];
        int col = vram[((tile * 64 + (py & 7) * 8 + (px & 7)) & 0x7FFF) * 2 + 1];
        if (capa == 1)
        {
            int p = col >> 7;
            col &= 0x7F;
            if (!col) { L.z[x] = 0; continue; }
            L.z[x] = z[p];
            L.color[x] = cgram[col];
        }
        else
        {
            if (!col) { L.z[x] = 0; continue; }
            L.z[x] = z[0];
            L.color[x] = directo ? ColorDirecto(col, 0) : cgram[col];
        }
    }
}

// Sprites de la línea (fila de pantalla 0..223). Primero se buscan los que tocan la línea (hasta 32, en orden desde el
// primero), luego se cargan sus mosaicos de 8 píxeles (hasta 34, desde el último hacia el primero) y se dibujan de
// atrás hacia adelante: así el de número más bajo queda arriba, como en la consola.
static void DibujarSprites(int fila, const uint8_t zObj[4], int campo)
{
    static const uint8_t ANCHO[8][2] = {{8, 16}, {8, 32}, {8, 64}, {16, 32}, {16, 64}, {32, 64}, {16, 32}, {16, 32}};
    static const uint8_t ALTO[8][2] = {{8, 16}, {8, 32}, {8, 64}, {16, 32}, {16, 64}, {32, 64}, {32, 64}, {32, 32}};
    int tamSel = (obsel >> 5) & 7;
    uint32_t base = (obsel & 7) << 13;
    uint32_t base2 = (((obsel >> 3) & 3) + 1) << 12;
    memset(objZ, 0, sizeof objZ);

    int primero = oamRotar ? (oamAdd >> 1) & 0x7F : 0;
    int rango[32], n = 0;
    for (int k = 0; k < 128; k++)
    {
        int i = (primero + k) & 127;
        uint8_t hi = oam[0x200 + (i >> 2)] >> ((i & 3) * 2);
        int w = ANCHO[tamSel][(hi >> 1) & 1], h = ALTO[tamSel][(hi >> 1) & 1];
        int ox = oam[i * 4] | ((hi & 1) << 8);
        if (ox > 256 && ox + w - 1 < 512) continue;   // completamente fuera por la izquierda
        int oy = oam[i * 4 + 1];
        int r = (fila - oy) & 0xFF;
        if (r >= (campo >= 0 ? h >> 1 : h)) continue;   // con sprites entrelazados ocupan la mitad de líneas
        if (n == 32) { rangoExcedido = true; break; }
        rango[n++] = i;
    }

    int mosaicos = 0;
    for (int k = n - 1; k >= 0; k--)
    {
        int i = rango[k];
        const uint8_t* o = oam + i * 4;
        uint8_t hi = oam[0x200 + (i >> 2)] >> ((i & 3) * 2);
        int w = ANCHO[tamSel][(hi >> 1) & 1], h = ALTO[tamSel][(hi >> 1) & 1];
        int ox = o[0] | ((hi & 1) << 8);
        if (ox >= 256) ox -= 512;
        int attr = o[3];
        int r = (fila - o[1]) & 0xFF;
        if (campo >= 0) r = r * 2 + campo;   // entrelazado: cada campo toma una fila alterna del sprite
        if (attr & 0x80) r = h - 1 - r;   // volteo vertical
        int pal = 128 + ((attr >> 1) & 7) * 16;
        uint8_t z = zObj[(attr >> 4) & 3];
        uint8_t mate = ((attr >> 1) & 7) >= 4;
        uint32_t cb = base + ((attr & 1) ? base2 : 0);
        int chrx = o[2] & 15, chry = (o[2] >> 4) & 15;
        chry = (chry + (r >> 3)) & 15;
        for (int t = 0; t < w / 8; t++)
        {
            int sx = ox + t * 8;
            if (sx <= -8 || sx >= 256) continue;
            if (++mosaicos > 34) { tiempoExcedido = true; break; }
            int tcol = (attr & 0x40) ? (w / 8 - 1 - t) : t;
            int chr = chry << 4 | ((chrx + tcol) & 15);
            uint8_t px[8];
            FilaMosaico(cb + chr * 16 + (r & 7), 4, px);
            for (int j = 0; j < 8; j++)
            {
                int x = sx + j;
                if (x < 0 || x >= 256) continue;
                int c = px[(attr & 0x40) ? 7 - j : j];
                if (!c) continue;
                objColor[x] = cgram[pal + c];
                objZ[x] = z;
                objMate[x] = mate;
            }
        }
        if (mosaicos > 34) break;
    }
}

// Ventanas: para cada columna, si está dentro de la ventana de esa capa. sel: 4 bits (W1 invertir, W1 activa, W2
// invertir, W2 activa); logica: 0 O, 1 Y, 2 O exclusivo, 3 su contrario.
static void CalcularVentana(uint8_t sel, uint8_t logica, uint8_t* dentro)
{
    bool e1 = sel & 2, e2 = sel & 8;
    if (!e1 && !e2) { memset(dentro, 0, 256); return; }
    bool i1 = sel & 1, i2 = sel & 4;
    for (int x = 0; x < 256; x++)
    {
        bool a = (x >= wh[0] && x <= wh[1]) != i1;
        bool b = (x >= wh[2] && x <= wh[3]) != i2;
        bool r;
        if (e1 && !e2) r = a;
        else if (e2 && !e1) r = b;
        else switch (logica & 3) { case 0: r = a || b; break; case 1: r = a && b; break; case 2: r = a != b; break; default: r = a == b; }
        dentro[x] = r;
    }
}

// Tablas de brillo: componente de 5 bits → 8 bits con el brillo maestro (0..15).
static uint8_t tablaBrillo[16][32];
static bool tablaLista;
static void PrepararBrillo()
{
    PrepararPlanos();
    for (int b = 0; b < 16; b++)
        for (int c = 0; c < 32; c++)
        {
            int v = (c << 3) | (c >> 2);
            tablaBrillo[b][c] = (uint8_t)(v * b / 15);
        }
    tablaLista = true;
}

static inline uint16_t Mezclar(uint16_t a, uint16_t b, bool restar, bool mitad)
{
    int ra = a & 31, ga = (a >> 5) & 31, ba = (a >> 10) & 31;
    int rb = b & 31, gb = (b >> 5) & 31, bb = (b >> 10) & 31;
    int r, g, bl;
    if (!restar) { r = ra + rb; g = ga + gb; bl = ba + bb; }
    else { r = ra - rb; g = ga - gb; bl = ba - bb; if (r < 0) r = 0; if (g < 0) g = 0; if (bl < 0) bl = 0; }
    if (mitad) { r >>= 1; g >>= 1; bl >>= 1; }
    if (r > 31) r = 31;
    if (g > 31) g = 31;
    if (bl > 31) bl = 31;
    return (uint16_t)(r | g << 5 | bl << 10);
}

// Compone una línea en BGR555. y = fila de pantalla (0..223); vFondo = línea que leen los fondos (V, o 2V+campo en
// entrelazado).
static void ComponerLinea(int y, int vFondo, int campo, uint16_t* linea)
{
    int V = vFondo;
    int modo = bgmode & 7;
    bool hires = modo == 5 || modo == 6;
    bool pseudo = setini & 8;
    uint8_t activas = tm | ts;
#ifdef GSU_TRAZA
    // Herramientas de PC: CAPAS=hex deja solo esas capas (1 BG1, 2 BG2, 4 BG3, 8 BG4, 10 sprites).
    static int capasDepur = getenv("CAPAS") ? (int)strtol(getenv("CAPAS"), NULL, 16) : 0x1F;
    activas &= capasDepur;
#endif

    // Capas del modo: bpp de cada fondo (0 = no existe).
    int bpp[4] = {0, 0, 0, 0};
    const TablaModo* t;
    switch (modo)
    {
    case 0: bpp[0] = bpp[1] = bpp[2] = bpp[3] = 2; t = &MODO0; break;
    case 1: bpp[0] = bpp[1] = 4; bpp[2] = 2; t = (bgmode & 8) ? &MODO1_BG3 : &MODO1; break;
    case 2: bpp[0] = bpp[1] = 4; t = &MODO2A5; break;
    case 3: bpp[0] = 8; bpp[1] = 4; t = &MODO2A5; break;
    case 4: bpp[0] = 8; bpp[1] = 2; t = &MODO2A5; break;
    case 5: bpp[0] = 4; bpp[1] = 2; t = &MODO2A5; break;
    case 6: bpp[0] = 4; t = &MODO6; break;
    default: t = &MODO7; break;
    }
    bool porColumna = modo == 2 || modo == 4 || modo == 6;
#ifdef GSU_TRAZA
    { extern int depurCuadro; static int l_ = getenv("MODO7") ? 1 : 0;
      if (l_ && depurCuadro && V % 20 == 0) printf("linea V=%d modo=%d bgmode=%02X activas=%02X inidisp=%02X\n", V, modo, bgmode, activas, inidisp); }
#endif

    bool hay[4] = {false, false, false, false};
    if (modo == 7)
    {
        if (activas & 1) { DibujarModo7(0, t->bg[0], V); hay[0] = true; }
        if ((activas & 2) && (setini & 0x40)) { DibujarModo7(1, t->bg[1], V); hay[1] = true; }
    }
    else
        for (int c = 0; c < 4; c++)
            if (bpp[c] && (activas & (1 << c)))
            {
                DibujarFondo(c, bpp[c], t->bg[c], V, hires, porColumna && c < 2, modo == 0 ? c * 32 : 0);
                hay[c] = true;
            }
    bool hayObj = activas & 0x10;
    if (hayObj) DibujarSprites(y, t->obj, (setini & 2) ? campo : -1);

    // Ventanas de cada capa y la de color.
    static uint8_t vent[6][256];
    static const uint8_t* selCapa[5] = {&w12sel, &w12sel, &w34sel, &w34sel, &wobjsel};
    for (int c = 0; c < 4; c++)
        if (hay[c] && ((tmw | tsw) & (1 << c))) CalcularVentana(*selCapa[c] >> ((c & 1) * 4), wbglog >> (c * 2), vent[c]);
    if (hayObj && ((tmw | tsw) & 0x10)) CalcularVentana(wobjsel & 0xF, wobjlog, vent[4]);
    CalcularVentana(wobjsel >> 4, wobjlog >> 2, vent[5]);

    uint16_t fijo = (uint16_t)(fijoR | fijoG << 5 | fijoB << 10);
    int recorte = cgwsel >> 6, prevenir = (cgwsel >> 4) & 3;
    bool usarSub = cgwsel & 2;
    bool restar = cgadsub & 0x80, mitad = cgadsub & 0x40;

    // Arma una pantalla (principal o secundaria) capa por capa: para cada columna queda el color, la altura y la capa
    // del píxel más adelante. Cada capa es un bucle simple sobre la línea (rápido). La columna x de la imagen usa la
    // columna (x << sh) + off de los fondos (en alta resolución, la impar para la principal y la par para la secundaria).
    static uint8_t mz[256], sz[256], mcap[256];
    static uint16_t mc[256], scol[256], sraw[256];
    auto Pantalla = [&](uint8_t habil, uint8_t mascara, int sh, int off, uint8_t* zz, uint16_t* cc, uint8_t* cap) {
        memset(zz, 0, 256);
        if (cap) memset(cap, L_FONDO, 256);
        for (int c = 0; c < 4; c++)
        {
            if (!hay[c] || !(habil & (1 << c))) continue;
            const uint8_t* lz = capas[c].z;
            const uint16_t* lc = capas[c].color;
            const uint8_t* w = (mascara & (1 << c)) ? vent[c] : NULL;
            for (int x = 0; x < 256; x++)
            {
                int cx = (x << sh) + off;
                uint8_t z = lz[cx];
                if (z > zz[x] && !(w && w[x])) { zz[x] = z; cc[x] = lc[cx]; if (cap) cap[x] = (uint8_t)c; }
            }
        }
        if (hayObj && (habil & 0x10))
        {
            const uint8_t* w = (mascara & 0x10) ? vent[4] : NULL;
            for (int x = 0; x < 256; x++)
            {
                uint8_t z = objZ[x];
                if (z > zz[x] && !(w && w[x])) { zz[x] = z; cc[x] = objColor[x]; if (cap) cap[x] = L_OBJ; }
            }
        }
    };

    int sh = hires ? 1 : 0;
    Pantalla(tm, tmw, sh, hires ? 1 : 0, mz, mc, mcap);
    bool algunaMezcla = (cgadsub & 0x3F) && prevenir != 3;
    if (usarSub && algunaMezcla) Pantalla(ts, tsw, sh, hires ? 1 : 0, sz, scol, NULL);
    bool dosMitades = hires || pseudo;
    if (dosMitades)
    {
        if (hires || !(usarSub && algunaMezcla)) Pantalla(ts, tsw, sh, 0, sz, sraw, NULL);
        else memcpy(sraw, scol, sizeof sraw);
        for (int x = 0; x < 256; x++) if (!sz[x]) sraw[x] = cgram[0];
        if (hires && usarSub && algunaMezcla) Pantalla(ts, tsw, sh, 1, sz, scol, NULL);
    }

    const uint8_t* dentroColor = vent[5];
    for (int x = 0; x < 256; x++)
    {
        int capa = mcap[x];
        uint16_t c = capa == L_FONDO ? cgram[0] : mc[x];
        bool dentro = dentroColor[x];
        bool negro = recorte == 3 || (recorte == 1 && !dentro) || (recorte == 2 && dentro);
        if (negro) c = 0;
        if (algunaMezcla)
        {
            bool permitir = prevenir == 0 || (prevenir == 1 && dentro) || (prevenir == 2 && !dentro);
            int bitCapa = capa == L_FONDO ? 0x20 : (1 << capa);
            if (permitir && (cgadsub & bitCapa) && (capa != L_OBJ || objMate[x]))
            {
                uint16_t otro = fijo;
                bool m = mitad && !negro;
                if (usarSub)
                {
                    if (sz[x]) otro = scol[x];
                    else m = false;   // la secundaria está vacía: se usa el color fijo y no se divide
                }
                c = Mezclar(c, otro, restar, m);
            }
        }
        // Alta resolución: las columnas pares muestran la pantalla secundaria y las impares la principal; acá se
        // promedian los dos medios píxeles en uno.
        if (dosMitades) c = Mezclar(c, sraw[x], false, true);
        linea[x] = c;
    }
}


void PpuLinea(int y, uint32_t* salida)
{
    if (!tablaLista) PrepararBrillo();
    int brillo = inidisp & 0xF;
    if ((inidisp & 0x80) || brillo == 0)
    {
        for (int x = 0; x < 256; x++) salida[x] = 0;
        return;
    }
    const uint8_t* br = tablaBrillo[brillo];
    static uint16_t a[256], b[256];
    int modo = bgmode & 7;
    bool hires = modo == 5 || modo == 6;
    if ((setini & 1) && (hires || (setini & 2)))
    {
        // Entrelazado: los fondos de alta resolución y los sprites entrelazados tienen una línea distinta en cada campo.
        // En lugar de alternar campos (parpadeo a 30 Hz) se dibujan los dos y se promedian: la imagen completa, estable.
        int v0 = hires ? (y + 1) * 2 : y + 1, v1 = hires ? v0 + 1 : v0;
        ComponerLinea(y, v0, 0, a);
        ComponerLinea(y, v1, 1, b);
        for (int x = 0; x < 256; x++) a[x] = Mezclar(a[x], b[x], false, true);
    }
    else ComponerLinea(y, y + 1, -1, a);
    for (int x = 0; x < 256; x++)
    {
        uint16_t c = a[x];
        salida[x] = (uint32_t)br[c & 31] << 16 | (uint32_t)br[(c >> 5) & 31] << 8 | br[(c >> 10) & 31];
    }
}

uint8_t PpuInidisp() { return inidisp; }

// Depuración: registros principales.
void PpuVolcado(char* o, int m)
{
    snprintf(o, m, "BGMODE=%02X TM=%02X TS=%02X OBSEL=%02X SC=%02X %02X %02X %02X NBA=%02X %02X HOFS=%d %d VOFS=%d %d CGWSEL=%02X CGADSUB=%02X",
             bgmode, tm, ts, obsel, bgsc[0], bgsc[1], bgsc[2], bgsc[3], bgnba[0], bgnba[1], hofs[0], hofs[1], vofs[0], vofs[1], cgwsel, cgadsub);
}
int PpuColoresNoNegros() { int n = 0; for (int i = 0; i < 256; i++) if (cgram[i]) n++; return n; }
uint8_t PpuTs() { return ts; }
void PpuVolcadoVram(char* o, int m)
{
    int nz = 0; for (int i = 0; i < 65536; i++) if (vram[i]) nz++;
    snprintf(o, m, "VRAM no-cero=%d", nz);
}

void PpuEstado(Serial& s)
{
    s(vram); s(cgram); s(oam);
    s(inidisp); s(obsel); s(bgmode); s(mosaico); s(setini); s(oamAdd); s(oamDir); s(oamLatch); s(oamRotar);
    s(bgsc); s(bgnba); s(hofs); s(vofs); s(ofsLatch); s(ofsLatchH); s(m7hofs); s(m7vofs);
    s(vmain); s(vmadd); s(vramLatch); s(m7sel); s(m7Latch); s(m7); s(cgadd); s(cgBuf); s(cgAlto);
    s(w12sel); s(w34sel); s(wobjsel); s(wh); s(wbglog); s(wobjlog); s(tm); s(ts); s(tmw); s(tsw);
    s(cgwsel); s(cgadsub); s(fijoR); s(fijoG); s(fijoB); s(hLatch); s(vLatch); s(hAlto); s(vAlto); s(contadorLatch);
    s(ppu1Bus); s(ppu2Bus); s(rangoExcedido); s(tiempoExcedido);
}

#ifdef GSU_TRAZA
const uint8_t* PpuOam() { return oam; }
#endif
#ifdef GSU_TRAZA
const uint8_t* PpuVram() { return (const uint8_t*)vram; }
#endif
