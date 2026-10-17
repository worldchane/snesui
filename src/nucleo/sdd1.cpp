// El chip S-DD1 de algunos cartuchos: descomprime gráficos al vuelo mientras la DMA los lee de la rom. La SNES le dice
// qué canales de DMA descomprimen ($4800 siempre, $4801 solo para la próxima copia); cuando uno de ellos lee la dirección
// de origen que se le programó, el chip entrega bytes descomprimidos en lugar de la rom. También tiene un mapeador de
// memoria: los bancos C0-FF muestran, de a 1 MB, la parte de la rom que digan $4804-$4807.
// El algoritmo de descompresión (un codificador aritmético con códigos de Golomb y 32 contextos) es el que describió
// Andreas Naive.
#include <string.h>

#include "interno.h"
#include "rom.h"

bool sdd1Presente;

// Descompresor: módulos encadenados como en el chip (entrada → Golomb → generadores de bits → estimación de
// probabilidad → modelo de contexto → salida).
struct Descompresor
{
    uint32_t posicion;     // entrada: byte de la rom que se está leyendo
    uint8_t bitsUsados;
    struct { uint8_t mps; bool lps; } gen[8];        // generadores de bits, uno por código de Golomb
    struct { uint8_t estado, mps; } contexto[32];
    uint8_t planos, bitsContexto, numeroBit, planoActual;
    uint16_t bitsAnteriores[8];
    uint8_t r0, r1, r2;
};

static struct
{
    uint8_t r4800, r4801, banco[4];
    struct { uint32_t dir; uint16_t tam; } dma[8];
    bool listo;
    Descompresor d;
} s;

// Bancos C0-FF: de a 1 MB, según $4804-$4807.
static inline uint8_t LeerMmc(uint32_t dir)
{
    uint32_t o = (uint32_t)(s.banco[dir >> 20 & 3] & 0xF) << 20 | (dir & 0xFFFFF);
    return romTam ? romDatos[RomPos(o)] : 0;
}

// Cuántos bits iguales seguidos (el símbolo más probable) hay antes de uno distinto: el código de Golomb leído, con
// los bits que siguen al primero dados vuelta e invertidos.
static uint8_t Corrida(uint8_t palabra)
{
    if (palabra == 0) return 0;
    int n = 7;
    while (!(palabra >> n)) n--;
    uint8_t v = 0;
    for (int i = 0; i < n; i++) if (!(palabra >> i & 1)) v |= (uint8_t)(1 << (n - 1 - i));
    return v;
}

static uint8_t PalabraCodigo(Descompresor& d, uint8_t largo)
{
    uint8_t p = (uint8_t)(LeerMmc(d.posicion) << d.bitsUsados);
    d.bitsUsados++;
    if (p & 0x80)
    {
        p |= LeerMmc(d.posicion + 1) >> (9 - d.bitsUsados);
        d.bitsUsados += largo;
    }
    if (d.bitsUsados & 8) { d.posicion++; d.bitsUsados &= 7; }
    return p;
}

static uint8_t BitGenerador(Descompresor& d, int codigo, bool& finCorrida)
{
    auto& g = d.gen[codigo];
    if (!(g.mps || g.lps))
    {
        uint8_t p = PalabraCodigo(d, (uint8_t)codigo);
        if (p & 0x80) { g.lps = true; g.mps = Corrida((uint8_t)(p >> (codigo ^ 7))); }
        else g.mps = (uint8_t)(1 << codigo);
    }
    uint8_t bit;
    if (g.mps) { bit = 0; g.mps--; }
    else { bit = 1; g.lps = false; }
    finCorrida = !(g.mps || g.lps);
    return bit;
}

// Estados de la estimación de probabilidad: qué código de Golomb usar, y a qué estado pasar si sale el símbolo más
// probable o el otro.
static const uint8_t EVOLUCION[33][3] = {
    {0, 25, 25}, {0, 2, 1},   {0, 3, 1},   {0, 4, 2},   {0, 5, 3},   {1, 6, 4},   {1, 7, 5},   {1, 8, 6},
    {1, 9, 7},   {2, 10, 8},  {2, 11, 9},  {2, 12, 10}, {2, 13, 11}, {3, 14, 12}, {3, 15, 13}, {3, 16, 14},
    {3, 17, 15}, {4, 18, 16}, {4, 19, 17}, {5, 20, 18}, {5, 21, 19}, {6, 22, 20}, {6, 23, 21}, {7, 24, 22},
    {7, 24, 23}, {0, 26, 1},  {1, 27, 2},  {2, 28, 4},  {3, 29, 8},  {4, 30, 12}, {5, 31, 16}, {6, 32, 18},
    {7, 24, 22}};

static uint8_t BitProbable(Descompresor& d, uint8_t ctx)
{
    auto& info = d.contexto[ctx];
    uint8_t estado = info.estado, mps = info.mps;
    const uint8_t* e = EVOLUCION[estado];
    bool fin;
    uint8_t bit = BitGenerador(d, e[0], fin);
    if (fin)
    {
        if (bit)
        {
            if (!(estado & 0xFE)) info.mps ^= 1;
            info.estado = e[2];
        }
        else info.estado = e[1];
    }
    return bit ^ mps;
}

static uint8_t BitContexto(Descompresor& d)
{
    switch (d.planos)
    {
    case 0x00: d.planoActual ^= 1; break;
    case 0x40: d.planoActual ^= 1; if (!(d.numeroBit & 0x7F)) d.planoActual = (d.planoActual + 2) & 7; break;
    case 0x80: d.planoActual ^= 1; if (!(d.numeroBit & 0x7F)) d.planoActual ^= 2; break;
    case 0xC0: d.planoActual = d.numeroBit & 7; break;
    }
    uint16_t& previos = d.bitsAnteriores[d.planoActual];
    uint8_t ctx = (uint8_t)((d.planoActual & 1) << 4);
    switch (d.bitsContexto)
    {
    case 0x00: ctx |= ((previos & 0x01C0) >> 5) | (previos & 1); break;
    case 0x10: ctx |= ((previos & 0x0180) >> 5) | (previos & 1); break;
    case 0x20: ctx |= ((previos & 0x00C0) >> 5) | (previos & 1); break;
    case 0x30: ctx |= ((previos & 0x0180) >> 5) | (previos & 3); break;
    }
    uint8_t bit = BitProbable(d, ctx);
    previos = (uint16_t)(previos << 1 | bit);
    d.numeroBit++;
    return bit;
}

static void IniciarDescompresor(Descompresor& d, uint32_t dir)
{
    memset(&d, 0, sizeof d);
    d.posicion = dir;
    d.bitsUsados = 4;   // el primer byte trae arriba el formato (planos y contexto)
    uint8_t cab = LeerMmc(dir);
    d.planos = cab & 0xC0;
    d.bitsContexto = cab & 0x30;
    d.planoActual = d.planos == 0x40 ? 7 : d.planos == 0x80 ? 3 : 1;
    d.r0 = 1;
}

static uint8_t ByteDescomprimido(Descompresor& d)
{
    if (d.planos == 0xC0)   // modo 8 bits por pixel: un byte de 8 planos
    {
        d.r1 = 0;
        for (d.r0 = 1; d.r0; d.r0 <<= 1) if (BitContexto(d)) d.r1 |= d.r0;
        return d.r1;
    }
    // Tiles de 2, 4 u 8 planos: salen de a pares de bytes (los dos planos de una fila).
    if (d.r0 == 0) { d.r0 = 0xFF; return d.r2; }
    d.r1 = d.r2 = 0;
    for (d.r0 = 0x80; d.r0; d.r0 >>= 1)
    {
        if (BitContexto(d)) d.r1 |= d.r0;
        if (BitContexto(d)) d.r2 |= d.r0;
    }
    return d.r1;
}

void Sdd1Reiniciar()
{
    memset(&s, 0, sizeof s);
    for (int i = 0; i < 4; i++) s.banco[i] = (uint8_t)i;
}

// Escrituras de la SNES en los registros de DMA ($43x2-$43x6): el chip se guarda la dirección y el tamaño.
void Sdd1RegistroDma(int canal, int reg, uint8_t v)
{
    auto& c = s.dma[canal];
    switch (reg)
    {
    case 2: c.dir = (c.dir & 0xFFFF00) | v; break;
    case 3: c.dir = (c.dir & 0xFF00FF) | (uint32_t)v << 8; break;
    case 4: c.dir = (c.dir & 0x00FFFF) | (uint32_t)v << 16; break;
    case 5: c.tam = (uint16_t)((c.tam & 0xFF00) | v); break;
    case 6: c.tam = (uint16_t)((c.tam & 0x00FF) | v << 8); break;
    }
}

bool Sdd1Leer(uint8_t banco, uint16_t off, uint8_t& v, uint8_t abierto)
{
    uint8_t b = banco & 0x7F;
    if (b < 0x40 && off >= 0x4800 && off < 0x4810)
    {
        switch (off & 0xF)
        {
        case 0: v = s.r4800; return true;
        case 1: v = s.r4801; return true;
        case 4: case 5: case 6: case 7: v = s.banco[off & 3]; return true;
        }
        v = abierto;
        return true;
    }
    if (b < 0x40 && off >= 0x8000)   // 00-3F, 80-BF: los primeros 2 MB (o el banco de $4805/$4807 si su bit 7 lo pide)
    {
        uint32_t dir = (uint32_t)banco << 16 | off;
        if (!(dir & 1 << 23) && (dir & 1 << 21) && (s.banco[1] & 0x80)) dir &= ~(1u << 21);
        if ((dir & 1 << 23) && (dir & 1 << 21) && (s.banco[3] & 0x80)) dir &= ~(1u << 21);
        dir = (dir >> 1 & 0x1F8000) | (dir & 0x7FFF);
        v = romTam ? romDatos[RomPos(dir)] : 0;
        return true;
    }
    if (banco < 0xC0) return false;
    uint32_t dir = (uint32_t)banco << 16 | off;
    uint8_t activos = s.r4800 & s.r4801;
    for (int n = 0; activos && n < 8; n++)
    {
        if (!(activos & 1 << n) || dir != s.dma[n].dir) continue;
        // La DMA del chip usa siempre dirección fija: mientras lea esta dirección, salen bytes descomprimidos.
        if (!s.listo) { IniciarDescompresor(s.d, dir); s.listo = true; }
        v = ByteDescomprimido(s.d);
        if (--s.dma[n].tam == 0) { s.listo = false; s.r4801 &= (uint8_t)~(1 << n); }
        return true;
    }
    v = LeerMmc(dir);
    return true;
}

bool Sdd1Escribir(uint8_t banco, uint16_t off, uint8_t v)
{
    if ((banco & 0x7F) >= 0x40 || off < 0x4800 || off >= 0x4810) return banco >= 0xC0;   // la rom no se escribe
    switch (off & 0xF)
    {
    case 0: s.r4800 = v; break;
    case 1: s.r4801 = v; break;
    case 4: case 5: case 6: case 7: s.banco[off & 3] = v & 0x8F; break;
    }
    return true;
}

void Sdd1Estado(Serial& se) { se(s); }

#ifdef SDD1_TABLAS
uint8_t Sdd1Corrida(uint8_t p) { return Corrida(p); }
#endif
