// El chip Super FX (GSU) que traen algunos cartuchos: un procesador de 16 bits que corre en paralelo con la SNES y
// dibuja en la RAM del cartucho (polígonos, rotaciones, efectos). La SNES le carga el programa, le escribe R15 para
// arrancarlo y, cuando termina (STOP), recibe una interrupción y copia la imagen a la VRAM.
// Escrito a partir de la documentación del hardware (fullsnes). Detalles importantes:
//   - Cauce de instrucciones: el byte que sigue a la instrucción ya está leído ("pipe"), así que después de un salto
//     se ejecuta una instrucción más (la del "hueco") antes de seguir en el destino.
//   - Prefijos: ALT1/ALT2/ALT3 cambian la instrucción siguiente; TO/WITH/FROM eligen los registros de destino y
//     origen. Todo eso vuelve a cero después de cada instrucción normal.
//   - Caché de 512 bytes: el código que corre desde ahí va a 1 ciclo por byte; el de la rom o la RAM, más lento.
//   - PLOT dibuja un píxel en el mapa de mosaicos de la RAM (2, 4 u 8 bits; alto de 128, 160, 192 o modo sprites).
#include "interno.h"
#include "rom.h"

bool gsuPresente;
bool irqGsu;

// Bits de SFR.
enum { S_Z = 1 << 1, S_CY = 1 << 2, S_S = 1 << 3, S_OV = 1 << 4, S_G = 1 << 5, S_R = 1 << 6, S_ALT1 = 1 << 8,
       S_ALT2 = 1 << 9, S_B = 1 << 12, S_IRQ = 1 << 15 };

static struct
{
    uint16_t r[16];
    uint16_t sfr;
    uint8_t pbr, rombr, rambr, bramr, cfgr, scbr, clsr, scmr, por, colr;
    uint16_t cbr;
    uint8_t pipe;
    bool r15Escrito;
    int sreg, dreg;
    uint8_t romBuf;
    uint16_t ultimaRam;
    uint8_t cache[512];
    bool valida[32];
    uint64_t ciclos;   // en el reloj maestro de la SNES
} g;

static uint32_t ramMascara = 0xFFFF;
static uint8_t version = 1;

// ---------- memoria ----------
static inline uint8_t Rom(uint8_t banco, uint16_t dir)
{
    uint32_t a = banco < 0x40 ? ((uint32_t)(banco & 0x3F) << 15) | (dir & 0x7FFF) : ((uint32_t)(banco & 0x1F) << 16) | dir;
    return romTam ? romDatos[a % romTam] : 0;
}
static inline uint8_t& Ram(uint32_t a) { return sram[a & ramMascara]; }
static inline uint32_t DirRam(uint16_t dir) { return ((uint32_t)(g.rambr & 1) << 16) | dir; }

// Costo de un acceso a la rom o la RAM, en ciclos maestros (el chip espera a la memoria).
static inline int CostoMemoria() { return (g.clsr & 1) ? 5 : 6; }
static inline int CostoCiclo() { return (g.clsr & 1) ? 1 : 2; }

static inline uint8_t LeerMem(uint8_t banco, uint16_t dir)
{
    g.ciclos += CostoMemoria();
    return banco >= 0x70 ? Ram(((uint32_t)(banco & 1) << 16) | dir) : Rom(banco, dir);
}

static inline uint8_t Ram8(uint16_t dir) { g.ciclos += CostoMemoria(); return Ram(DirRam(dir)); }
static inline void Ram8(uint16_t dir, uint8_t v) { g.ciclos += CostoMemoria(); Ram(DirRam(dir)) = v; }
// Palabras: el byte alto está en la dirección con el bit 0 invertido (así lo hace el chip).
static inline uint16_t RamW(uint16_t dir) { g.ultimaRam = dir; return Ram8(dir) | Ram8(dir ^ 1) << 8; }
static inline void RamW(uint16_t dir, uint16_t v) { g.ultimaRam = dir; Ram8(dir, (uint8_t)v); Ram8(dir ^ 1, v >> 8); }

static void InvalidarCache() { for (bool& v : g.valida) v = false; }

// Un byte del programa: desde la caché si la dirección cae en ella (llenando la línea de 16 bytes si hace falta).
static uint8_t LeerPrograma(uint16_t pc)
{
    uint16_t off = pc - g.cbr;
    if (off < 512)
    {
        int linea = off >> 4;
        if (!g.valida[linea])
        {
            for (int i = 0; i < 16; i++) g.cache[(linea << 4) + i] = LeerMem(g.pbr, g.cbr + (linea << 4) + i);
            g.valida[linea] = true;
        }
        g.ciclos += CostoCiclo();
        return g.cache[off];
    }
    g.ciclos += CostoCiclo();
    return LeerMem(g.pbr, pc);
}

// ---------- registros y cauce ----------
static inline void PonerR(int n, uint16_t v)
{
    g.r[n] = v;
    if (n == 14) g.romBuf = Rom(g.rombr, v);   // escribir R14 pide un byte de la rom (lo lee GETB/GETC)
    if (n == 15) g.r15Escrito = true;
}

static inline uint8_t Pipe()
{
    uint8_t r = g.pipe;
    if (g.r15Escrito) g.r15Escrito = false;
    else g.r[15]++;
    g.pipe = LeerPrograma(g.r[15]);
    return r;
}

static inline void Fin()
{
    g.sfr &= ~(S_ALT1 | S_ALT2 | S_B);
    g.sreg = g.dreg = 0;
}

static inline void Bandera(uint16_t b, bool v) { if (v) g.sfr |= b; else g.sfr &= ~b; }
static inline void ZS(uint16_t v) { Bandera(S_Z, v == 0); Bandera(S_S, v & 0x8000); }
static inline uint16_t S() { return g.r[g.sreg]; }
static inline void D(uint16_t v) { PonerR(g.dreg, v); }

// ---------- píxeles ----------
static int Bpp() { int m = g.scmr & 3; return m == 0 ? 2 : m == 3 ? 8 : 4; }

static uint32_t DirPixel(uint8_t x, uint8_t y, int bpp)
{
    int alto = ((g.scmr >> 2) & 1) | ((g.scmr >> 4) & 2);   // 0: 128, 1: 160, 2: 192, 3: modo sprites
    if (g.por & 0x10) alto = 3;
    uint32_t cn;
    switch (alto)
    {
    case 0: cn = ((x & 0xF8) << 1) + ((y & 0xF8) >> 3); break;
    case 1: cn = ((x & 0xF8) << 1) + ((x & 0xF8) >> 1) + ((y & 0xF8) >> 3); break;
    case 2: cn = ((x & 0xF8) << 1) + (x & 0xF8) + ((y & 0xF8) >> 3); break;
    default: cn = ((y & 0x80) << 2) + ((x & 0x80) << 1) + ((y & 0x78) << 1) + ((x & 0x78) >> 3); break;
    }
    return (uint32_t)g.scbr * 1024 + cn * (bpp * 8) + ((y & 7) << 1);
}

static void Plot()
{
    uint8_t x = (uint8_t)g.r[1], y = (uint8_t)g.r[2];
    uint8_t c = g.colr;
    int bpp = Bpp();
    bool transparente = false;
    if (!(g.por & 1))
    {
        if (bpp == 8 && !(g.por & 8)) transparente = c == 0;
        else transparente = (c & (bpp == 2 ? 3 : 0x0F)) == 0;
    }
    if (!transparente)
    {
        if ((g.por & 2) && bpp != 8) c = ((x ^ y) & 1) ? (c >> 4) : (c & 0x0F);   // tramado
        uint32_t base = DirPixel(x, y, bpp);
        uint8_t bit = 0x80 >> (x & 7);
        for (int i = 0; i < bpp; i++)
        {
            uint8_t& b = Ram(base + ((i >> 1) << 4) + (i & 1));
            if (c & (1 << i)) b |= bit;
            else b &= ~bit;
        }
    }
    // El chip junta los píxeles en un búfer de 8 y los escribe juntos: cada PLOT cuesta poco más de un ciclo.
    g.ciclos += CostoCiclo() + (CostoMemoria() * bpp) / 16;
    PonerR(1, g.r[1] + 1);
}

static uint8_t LeerPixel()
{
    uint8_t x = (uint8_t)g.r[1], y = (uint8_t)g.r[2];
    int bpp = Bpp();
    uint32_t base = DirPixel(x, y, bpp);
    uint8_t bit = 0x80 >> (x & 7), c = 0;
    for (int i = 0; i < bpp; i++)
        if (Ram(base + ((i >> 1) << 4) + (i & 1)) & bit) c |= 1 << i;
    g.ciclos += CostoMemoria() * (bpp / 2);
    return c;
}

static uint8_t Color(uint8_t v)
{
    if (g.por & 4) return (g.colr & 0xF0) | (v >> 4);     // usar el nibble alto
    if (g.por & 8) return (g.colr & 0xF0) | (v & 0x0F);   // congelar el nibble alto
    return v;
}

// ---------- una instrucción ----------
static void Paso()
{
    uint8_t op = Pipe();
    int alt = (g.sfr >> 8) & 3;
    int n = op & 15;
    switch (op >> 4)
    {
    case 0x0:
        switch (op)
        {
        case 0x00:   // STOP
            g.sfr &= ~S_G;
            if (!(g.cfgr & 0x80)) { g.sfr |= S_IRQ; irqGsu = true; }
            Fin();
            return;
        case 0x01: Fin(); return;   // NOP
        case 0x02:                  // CACHE
            if (g.cbr != (g.r[15] & 0xFFF0)) { g.cbr = g.r[15] & 0xFFF0; InvalidarCache(); }
            Fin();
            return;
        case 0x03: { uint16_t s = S(); Bandera(S_CY, s & 1); uint16_t v = s >> 1; D(v); ZS(v); Fin(); return; }   // LSR
        case 0x04: { uint16_t s = S(); uint16_t v = (s << 1) | ((g.sfr & S_CY) ? 1 : 0); Bandera(S_CY, s & 0x8000); D(v); ZS(v); Fin(); return; }   // ROL
        default:   // saltos: BRA, BGE, BLT, BNE, BEQ, BPL, BMI, BCC, BCS, BVC, BVS
        {
            int8_t e = (int8_t)Pipe();
            bool z = g.sfr & S_Z, cy = g.sfr & S_CY, s = g.sfr & S_S, ov = g.sfr & S_OV;
            bool salta;
            switch (op)
            {
            case 0x05: salta = true; break;
            case 0x06: salta = s == ov; break;
            case 0x07: salta = s != ov; break;
            case 0x08: salta = !z; break;
            case 0x09: salta = z; break;
            case 0x0A: salta = !s; break;
            case 0x0B: salta = s; break;
            case 0x0C: salta = !cy; break;
            case 0x0D: salta = cy; break;
            case 0x0E: salta = !ov; break;
            default: salta = ov; break;
            }
            if (salta) PonerR(15, g.r[15] + e);
            Fin();
            return;
        }
        }
        return;
    case 0x1:   // TO Rn (o MOVE Rn ← Sreg con el prefijo B)
        if (g.sfr & S_B) { PonerR(n, S()); Fin(); }
        else g.dreg = n;
        return;
    case 0x2:   // WITH Rn
        g.sreg = g.dreg = n;
        g.sfr |= S_B;
        return;
    case 0x3:
        if (n <= 11)   // STW / STB (Rn)
        {
            if (alt & 1) { g.ultimaRam = g.r[n]; Ram8(g.r[n], (uint8_t)S()); }
            else RamW(g.r[n], S());
            Fin();
            return;
        }
        switch (n)
        {
        case 12:   // LOOP
        {
            uint16_t v = g.r[12] - 1;
            PonerR(12, v);
            ZS(v);
            if (v) PonerR(15, g.r[13]);
            Fin();
            return;
        }
        case 13: g.sfr = (g.sfr & ~S_B) | S_ALT1; return;            // ALT1
        case 14: g.sfr = (g.sfr & ~S_B) | S_ALT2; return;            // ALT2
        default: g.sfr = (g.sfr & ~S_B) | S_ALT1 | S_ALT2; return;   // ALT3
        }
    case 0x4:
        if (n <= 11)   // LDW / LDB (Rn)
        {
            uint16_t v = (alt & 1) ? (g.ultimaRam = g.r[n], Ram8(g.r[n])) : RamW(g.r[n]);
            D(v);
            Fin();
            return;
        }
        switch (n)
        {
        case 12:
            if (alt & 1) { uint8_t c = LeerPixel(); D(c); ZS(c); }   // RPIX
            else Plot();                                             // PLOT
            Fin();
            return;
        case 13: { uint16_t s = S(); uint16_t v = (uint16_t)(s >> 8 | s << 8); D(v); ZS(v); Fin(); return; }   // SWAP
        case 14:
            if (alt & 1) g.por = S() & 0x1F;   // CMODE
            else g.colr = Color((uint8_t)S());   // COLOR
            Fin();
            return;
        default: { uint16_t v = ~S(); D(v); ZS(v); Fin(); return; }   // NOT
        }
    case 0x5:   // ADD / ADC (registro o #n)
    {
        uint16_t s = S(), b = (alt & 2) ? n : g.r[n];
        uint32_t r = (uint32_t)s + b + ((alt & 1) && (g.sfr & S_CY) ? 1 : 0);
        Bandera(S_OV, ~(s ^ b) & (s ^ r) & 0x8000);
        Bandera(S_CY, r > 0xFFFF);
        ZS((uint16_t)r);
        D((uint16_t)r);
        Fin();
        return;
    }
    case 0x6:   // SUB / SBC / SUB #n / CMP
    {
        uint16_t s = S(), b = alt == 2 ? n : g.r[n];
        int32_t r = (int32_t)s - b - (alt == 1 && !(g.sfr & S_CY) ? 1 : 0);
        Bandera(S_OV, (s ^ b) & (s ^ r) & 0x8000);
        Bandera(S_CY, r >= 0);
        ZS((uint16_t)r);
        if (alt != 3) D((uint16_t)r);
        Fin();
        return;
    }
    case 0x7:
        if (n == 0)   // MERGE
        {
            uint16_t v = (g.r[7] & 0xFF00) | (g.r[8] >> 8);
            D(v);
            Bandera(S_OV, v & 0xC0C0);
            Bandera(S_S, v & 0x8080);
            Bandera(S_CY, v & 0xE0E0);
            Bandera(S_Z, v & 0xF0F0);   // así lo hace el chip: Z se enciende si hay bits ahí (no si es cero)
            Fin();
            return;
        }
        else   // AND / BIC (registro o #n)
        {
            uint16_t b = (alt & 2) ? n : g.r[n];
            uint16_t v = (alt & 1) ? (S() & ~b) : (S() & b);
            D(v);
            ZS(v);
            Fin();
            return;
        }
    case 0x8:   // MULT / UMULT (registro o #n): 8 × 8 bits
    {
        uint16_t b = (alt & 2) ? n : g.r[n];
        uint16_t v = (alt & 1) ? (uint16_t)((uint8_t)S() * (uint8_t)b) : (uint16_t)((int8_t)S() * (int8_t)b);
        D(v);
        ZS(v);
        if (!(g.cfgr & 0x20)) g.ciclos += CostoCiclo();
        Fin();
        return;
    }
    case 0x9:
        switch (n)
        {
        case 0: RamW(g.ultimaRam, S()); Fin(); return;   // SBK
        case 1: case 2: case 3: case 4: PonerR(11, g.r[15] + n); Fin(); return;   // LINK #n
        case 5: { uint16_t v = (uint16_t)(int8_t)S(); D(v); ZS(v); Fin(); return; }   // SEX
        case 6:   // ASR / DIV2
        {
            uint16_t s = S();
            Bandera(S_CY, s & 1);
            uint16_t v = (alt & 1) && s == 0xFFFF ? 0 : (uint16_t)((int16_t)s >> 1);
            D(v);
            ZS(v);
            Fin();
            return;
        }
        case 7: { uint16_t s = S(); uint16_t v = (s >> 1) | ((g.sfr & S_CY) ? 0x8000 : 0); Bandera(S_CY, s & 1); D(v); ZS(v); Fin(); return; }   // ROR
        case 14: { uint16_t v = S() & 0xFF; D(v); Bandera(S_Z, v == 0); Bandera(S_S, v & 0x80); Fin(); return; }   // LOB
        case 15:   // FMULT / LMULT: 16 × 16 bits con R6
        {
            int32_t r = (int32_t)(int16_t)S() * (int16_t)g.r[6];
            if (alt & 1) PonerR(4, (uint16_t)r);
            uint16_t v = (uint16_t)(r >> 16);
            D(v);
            Bandera(S_CY, (r >> 15) & 1);
            ZS(v);
            g.ciclos += CostoCiclo() * ((g.cfgr & 0x20) ? 3 : 7);
            Fin();
            return;
        }
        default:   // 8..13: JMP Rn / LJMP Rn
            if (alt & 1)
            {
                g.pbr = g.r[n] & 0x7F;
                PonerR(15, S());
                g.cbr = g.r[15] & 0xFFF0;
                InvalidarCache();
            }
            else PonerR(15, g.r[n]);
            Fin();
            return;
        }
    case 0xA:   // IBT Rn,#pp / LMS Rn,(yy) / SMS (yy),Rn
    {
        uint8_t p = Pipe();
        if (alt == 1) PonerR(n, RamW((uint16_t)p << 1));
        else if (alt == 2) RamW((uint16_t)p << 1, g.r[n]);
        else PonerR(n, (uint16_t)(int8_t)p);
        Fin();
        return;
    }
    case 0xB:   // FROM Rn (o MOVES Dreg ← Rn con el prefijo B)
        if (g.sfr & S_B)
        {
            uint16_t v = g.r[n];
            D(v);
            ZS(v);
            Bandera(S_OV, v & 0x80);
            Fin();
        }
        else g.sreg = n;
        return;
    case 0xC:
        if (n == 0)   // HIB
        {
            uint16_t v = S() >> 8;
            D(v);
            Bandera(S_Z, v == 0);
            Bandera(S_S, v & 0x80);
            Fin();
            return;
        }
        else   // OR / XOR (registro o #n)
        {
            uint16_t b = (alt & 2) ? n : g.r[n];
            uint16_t v = (alt & 1) ? (S() ^ b) : (S() | b);
            D(v);
            ZS(v);
            Fin();
            return;
        }
    case 0xD:
        if (n < 15) { uint16_t v = g.r[n] + 1; PonerR(n, v); ZS(v); Fin(); return; }   // INC Rn
        switch (alt)
        {
        case 2: g.rambr = S() & 1; break;                                     // RAMB
        case 3: g.rombr = S() & 0x7F; g.romBuf = Rom(g.rombr, g.r[14]); break;   // ROMB
        default: g.colr = Color(g.romBuf); break;                             // GETC
        }
        Fin();
        return;
    case 0xE:
        if (n < 15) { uint16_t v = g.r[n] - 1; PonerR(n, v); ZS(v); Fin(); return; }   // DEC Rn
        switch (alt)   // GETB / GETBH / GETBL / GETBS
        {
        case 0: D(g.romBuf); break;
        case 1: D((uint16_t)(g.romBuf << 8) | (S() & 0xFF)); break;
        case 2: D((S() & 0xFF00) | g.romBuf); break;
        default: D((uint16_t)(int8_t)g.romBuf); break;
        }
        g.ciclos += CostoMemoria();
        Fin();
        return;
    default:   // 0xF: IWT Rn,#xx / LM Rn,(xx) / SM (xx),Rn
    {
        uint16_t v = Pipe();
        v |= Pipe() << 8;
        if (alt == 1) PonerR(n, RamW(v));
        else if (alt == 2) RamW(v, g.r[n]);
        else PonerR(n, v);
        Fin();
        return;
    }
    }
}

// ---------- con la SNES ----------
extern uint64_t ciclosMaestros;

static void Arrancar()
{
    if (g.ciclos < ciclosMaestros) g.ciclos = ciclosMaestros;
    g.sfr |= S_G;
    g.r15Escrito = false;
    g.pipe = LeerPrograma(g.r[15]);
}

void GsuCorrerHasta(uint64_t hasta)
{
    if (!gsuPresente) return;
    if (!(g.sfr & S_G)) { if (g.ciclos < hasta) g.ciclos = hasta; return; }
    while ((g.sfr & S_G) && g.ciclos < hasta) Paso();
    if (!(g.sfr & S_G) && g.ciclos < hasta) g.ciclos = hasta;
}

uint8_t GsuLeer(uint16_t dir)
{
    if (dir >= 0x3100 && dir < 0x3300) return g.cache[dir - 0x3100];
    if (dir >= 0x3000 && dir < 0x3020) { uint16_t r = g.r[(dir >> 1) & 15]; return (dir & 1) ? r >> 8 : r & 0xFF; }
    switch (dir)
    {
    case 0x3030: return g.sfr & 0xFF;
    case 0x3031:
    {
        uint8_t v = g.sfr >> 8;
        g.sfr &= ~S_IRQ;   // leer el byte alto borra la interrupción
        irqGsu = false;
        return v;
    }
    case 0x3033: return g.bramr;
    case 0x3034: return g.pbr;
    case 0x3036: return g.rombr;
    case 0x303B: return version;
    case 0x303C: return g.rambr;
    case 0x303E: return g.cbr & 0xFF;
    case 0x303F: return g.cbr >> 8;
    }
    return 0;
}

void GsuEscribir(uint16_t dir, uint8_t v)
{
    if (dir >= 0x3100 && dir < 0x3300)
    {
        int i = dir - 0x3100;
        g.cache[i] = v;
        if ((i & 15) == 15) g.valida[i >> 4] = true;
        return;
    }
    if (dir >= 0x3000 && dir < 0x3020)
    {
        int n = (dir >> 1) & 15;
        uint16_t r = (dir & 1) ? (uint16_t)((g.r[n] & 0x00FF) | (v << 8)) : (uint16_t)((g.r[n] & 0xFF00) | v);
        g.r[n] = r;
        if (n == 14) g.romBuf = Rom(g.rombr, r);
        if (dir == 0x301F) Arrancar();   // escribir el byte alto de R15 arranca el chip
        return;
    }
    switch (dir)
    {
    case 0x3030:
    {
        bool corria = g.sfr & S_G;
        g.sfr = (g.sfr & 0xFF00) | v;
        if (corria && !(g.sfr & S_G)) { g.cbr = 0; InvalidarCache(); }   // detenerlo desde la SNES
        break;
    }
    case 0x3031: g.sfr = (g.sfr & 0x00FF) | (v << 8); break;
    case 0x3033: g.bramr = v & 1; break;
    case 0x3034: g.pbr = v & 0x7F; InvalidarCache(); break;
    case 0x3037: g.cfgr = v; break;
    case 0x3038: g.scbr = v; break;
    case 0x3039: g.clsr = v & 1; break;
    case 0x303A: g.scmr = v; break;
    }
}

bool GsuCorriendo() { return gsuPresente && (g.sfr & S_G); }

void GsuReiniciar()
{
    memset(&g, 0, sizeof g);
    irqGsu = false;
}

void GsuConfigurar(bool presente, uint32_t ramTam, bool gsu2)
{
    gsuPresente = presente;
    uint32_t t = 1;
    while (t < ramTam) t <<= 1;
    ramMascara = (t ? t : 0x10000) - 1;
    if (ramMascara > sizeof sram - 1) ramMascara = sizeof sram - 1;
    version = gsu2 ? 4 : 1;
    GsuReiniciar();
}

uint64_t GsuCiclos() { return g.ciclos; }

void GsuEstado(Serial& s)
{
    s(g);
    s(irqGsu);
}
