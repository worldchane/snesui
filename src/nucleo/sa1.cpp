// El chip SA-1 (Super Accelerator) de algunos cartuchos: un segundo 65816 a 10,74 MHz (sa1cpu.cpp) con su propio mapa
// de memoria y unidades de ayuda. Comparte con la SNES:
//   - la rom, en bancos de 1 MB que se eligen con $2220-$2223 (Super MMC);
//   - la BW-RAM (la RAM del cartucho, con batería), que el SA-1 también puede ver como mapa de bits de 2 o 4 bits;
//   - la I-RAM, 2 KB rápidos dentro del chip;
//   - registros en $2200-$23FF para mandarse interrupciones, multiplicar y dividir, leer bits de largo variable, el
//     temporizador y el DMA (normal o de conversión de gráficos de mapa de bits a mosaicos).
// Escrito a partir de la documentación del hardware y de cómo lo emula bsnes. El SA-1 corre en paralelo: se lo pone al
// día al final de cada línea y cada vez que la SNES toca algo que comparten.
#include "interno.h"
#include "rom.h"

bool sa1Presente;
bool irqSa1;              // pedido de interrupción del SA-1 a la SNES
uint64_t sa1Ciclos;       // reloj del SA-1, en ciclos maestros
bool sa1Nmi, sa1Irq;      // pedidos de interrupción al procesador del SA-1

extern Cpu sa1cpu;
void Sa1CpuReiniciar();
void Sa1CpuCorrerHasta(uint64_t hasta);
void Sa1CpuEstado(Serial& s);
extern uint64_t ciclosMaestros;

static uint8_t iram[2048];

static struct
{
    // Control y vectores
    bool sa1Irq, sa1Rdyb, sa1Resb, sa1NmiPin; uint8_t smeg;          // $2200 CCNT
    bool cpuIrqen, chdmaIrqen;                                        // $2201 SIE
    uint16_t crv, cnv, civ;                                           // vectores del SA-1
    bool cpuIrq, cpuIvsw, cpuNvsw; uint8_t cmeg;                      // $2209 SCNT
    bool sa1Irqen, timerIrqen, dmaIrqen, sa1Nmien;                    // $220A CIE
    bool sa1Irqcl, timerIrqcl, dmaIrqcl, sa1Nmicl;                    // $220B CIC
    uint16_t snv, siv;                                                // vectores de la SNES cuando el SA-1 los cambia
    bool cpuIrqcl, chdmaIrqcl;                                        // $2202 SIC
    // Temporizador
    bool hvselb, ven, hen; uint16_t hcnt, vcnt;
    uint16_t hcont, vcont;                                            // contadores (h en ciclos maestros)
    // Memoria
    bool cbmode, dbmode, ebmode, fbmode; uint8_t cb, db, eb, fb;      // Super MMC
    uint8_t sbm; bool sw46; uint8_t cbm;                              // ventanas de la BW-RAM
    bool swen, cwen; uint8_t bwp, siwp, ciwp;                         // protección de escritura
    // DMA
    bool dmaen, dprio, cden, cdsel, dd; uint8_t sd;
    bool chdend; uint8_t dmasize, dmacb;
    uint32_t dsa, dda; uint16_t dtc;
    bool bbf; uint8_t brf[16]; uint8_t lineaCC2;
    bool dmaBw;                                                       // conversión tipo 1 en curso
    // Aritmética y bits variables
    bool acm, md; uint16_t ma, mb; uint64_t mr; bool desborde;
    bool hl; uint8_t vb; uint32_t va; uint8_t vbit;
    // Banderas
    bool cpuIrqfl, chdmaIrqfl, sa1Irqfl, timerIrqfl, dmaIrqfl, sa1Nmifl;
    uint16_t hcr, vcr;
} m;

// ---------- interrupciones ----------
static void ActualizarIrq()
{
    if (m.sa1NmiPin && !m.sa1Nmicl) { sa1Nmi = true; m.sa1Nmifl = true; m.sa1Nmicl = true; }
    sa1Irq = (m.timerIrqen && !m.timerIrqcl) || (m.dmaIrqen && !m.dmaIrqcl) || (m.sa1Irq && !m.sa1Irqcl);
}

static void IrqSnes(bool si) { irqSa1 = si; }

// ---------- memoria ----------
static inline uint8_t LeerRom(uint32_t a) { return romTam ? romDatos[a % romTam] : 0; }

// La rom vista desde la SNES (dirección ya traducida: 00-3F/80-BF:8000-FFFF → 000000-3FFFFF; C0-FF sin traducir).
static uint8_t RomCpu(uint32_t a)
{
    if ((a & 0xffffe0) == 0x007fe0)   // vectores de la SNES: el SA-1 puede cambiar los de NMI e IRQ
    {
        if (a == 0x7fea && m.cpuNvsw) return (uint8_t)m.snv;
        if (a == 0x7feb && m.cpuNvsw) return m.snv >> 8;
        if (a == 0x7fee && m.cpuIvsw) return (uint8_t)m.siv;
        if (a == 0x7fef && m.cpuIvsw) return m.siv >> 8;
    }
    bool lo = a < 0x400000;   // el modo "sin bancos" solo vale en 00-3F/80-BF:8000-FFFF
    a &= 0x3fffff;
    bool modo; uint8_t banco;
    switch (a >> 20)
    {
    case 0: modo = m.cbmode; banco = m.cb; break;
    case 1: modo = m.dbmode; banco = m.db; break;
    case 2: modo = m.ebmode; banco = m.eb; break;
    default: modo = m.fbmode; banco = m.fb; break;
    }
    if (lo && !modo) return LeerRom(a);
    return LeerRom((uint32_t)banco << 20 | (a & 0x0fffff));
}

static inline uint32_t TraducirLo(uint32_t a) { return (a & 0x800000) >> 2 | (a & 0x3f0000) >> 1 | (a & 0x007fff); }
static uint8_t RomSa1(uint32_t a) { if ((a & 0x408000) == 0x008000) a = TraducirLo(a); return RomCpu(a); }

static inline uint32_t MascaraBw() { uint32_t t = 1; while (t < sramTam) t <<= 1; return sramTam ? t - 1 : 0; }
static inline uint8_t BwLeer(uint32_t a) { return sramTam ? sram[a & MascaraBw()] : 0; }
static inline void BwEscribir(uint32_t a, uint8_t v) { if (sramTam) { sram[a & MascaraBw()] = v; sramCambiada = true; } }
static inline bool BwProtegida(uint32_t a) { return !m.swen && !m.cwen && (a & 0x3ffff) < (0x100u << m.bwp); }

static uint8_t BitmapLeer(uint32_t a)
{
    a &= 0xfffff;
    if (!m.bbf) return (BwLeer(a >> 1) >> ((a & 1) * 4)) & 15;   // 4 bits por píxel
    return (BwLeer(a >> 2) >> ((a & 3) * 2)) & 3;               // 2 bits por píxel
}
static void BitmapEscribir(uint32_t a, uint8_t v)
{
    a &= 0xfffff;
    if (!m.bbf) { int s = (a & 1) * 4; BwEscribir(a >> 1, (uint8_t)((BwLeer(a >> 1) & ~(15 << s)) | (v & 15) << s)); }
    else { int s = (a & 3) * 2; BwEscribir(a >> 2, (uint8_t)((BwLeer(a >> 2) & ~(3 << s)) | (v & 3) << s)); }
}

static inline void IramEscribir(uint16_t a, uint8_t v, uint8_t proteccion) { if (proteccion & (1 << ((a >> 8) & 7))) iram[a & 0x7ff] = v; }

// ---------- conversión de gráficos (DMA de mapa de bits a mosaicos) ----------
static uint8_t Cc1Leer(uint32_t a)
{
    // Tipo 1: la SNES lee la BW-RAM como mosaicos; el chip convierte cada mosaico a la I-RAM cuando empieza.
    uint32_t mascaraChar = (1u << (6 - m.dmacb)) - 1;
    if ((a & mascaraChar) == 0)
    {
        uint32_t bpp = 2u << (2 - m.dmacb);
        uint32_t bpl = (8u << m.dmasize) >> m.dmacb;
        uint32_t bwmask = MascaraBw();
        uint32_t tile = ((a - m.dsa) & bwmask) >> (6 - m.dmacb);
        uint32_t ty = tile >> m.dmasize, tx = tile & ((1u << m.dmasize) - 1);
        uint32_t bwa = m.dsa + ty * 8 * bpl + tx * bpp;
        for (int y = 0; y < 8; y++)
        {
            uint64_t dato = 0;
            for (uint32_t b = 0; b < bpp; b++) dato |= (uint64_t)BwLeer((bwa + b) & bwmask) << (b << 3);
            bwa += bpl;
            uint8_t out[8] = {0};
            for (int x = 0; x < 8; x++)
            {
                out[0] |= (dato & 1) << (7 - x); dato >>= 1;
                out[1] |= (dato & 1) << (7 - x); dato >>= 1;
                if (m.dmacb == 2) continue;
                out[2] |= (dato & 1) << (7 - x); dato >>= 1;
                out[3] |= (dato & 1) << (7 - x); dato >>= 1;
                if (m.dmacb == 1) continue;
                for (int k = 4; k < 8; k++) { out[k] |= (dato & 1) << (7 - x); dato >>= 1; }
            }
            for (uint32_t b = 0; b < bpp; b++)
                iram[(m.dda + (y << 1) + ((b & 6) << 3) + (b & 1)) & 0x7ff] = out[b];
        }
    }
    return iram[(m.dda + (a & mascaraChar)) & 0x7ff];
}

static void Cc2()
{
    // Tipo 2: el SA-1 escribe 8 píxeles por $2240-$224F y el chip los pasa a planos en la I-RAM.
    const uint8_t* brf = &m.brf[(m.lineaCC2 & 1) << 3];
    uint32_t bpp = 2u << (2 - m.dmacb);
    uint32_t a = m.dda & 0x07ff;
    a &= ~((1u << (7 - m.dmacb)) - 1);
    a += (m.lineaCC2 & 8) * bpp;
    a += (m.lineaCC2 & 7) * 2;
    for (uint32_t b = 0; b < bpp; b++)
    {
        uint8_t o = 0;
        for (int bit = 0; bit < 8; bit++) o |= ((brf[bit] >> b) & 1) << (7 - bit);
        iram[(a + ((b & 6) << 3) + (b & 1)) & 0x7ff] = o;
    }
    m.lineaCC2 = (m.lineaCC2 + 1) & 15;
}

static void DmaNormal()
{
    // Copia directa entre la rom, la BW-RAM y la I-RAM (el SA-1 queda ocupado mientras tanto).
    while (m.dtc--)
    {
        uint32_t org = m.dsa++ & 0xffffff, dst = m.dda++ & 0xffffff;
        uint8_t v = 0;
        int costo = 4;
        if (m.sd == 0) { v = RomSa1(org); if (!m.dd) costo = 2; }
        else if (m.sd == 1) v = BwLeer(org);
        else v = iram[org & 0x7ff];
        if (m.dd) BwEscribir(dst, v); else iram[dst & 0x7ff] = v;
        sa1Ciclos += costo;
    }
    m.dmaIrqfl = true;
    if (m.dmaIrqen) m.dmaIrqcl = false;
    ActualizarIrq();
}

// ---------- registros ----------
static void EscribirCompartido(uint16_t r, uint8_t v)
{
    switch (r)
    {
    case 0x2231:
        m.chdend = v & 0x80;
        m.dmasize = (v >> 2) & 7; if (m.dmasize > 5) m.dmasize = 5;
        m.dmacb = v & 3; if (m.dmacb > 2) m.dmacb = 2;
        if (m.chdend) m.dmaBw = false;
        return;
    case 0x2232: m.dsa = (m.dsa & 0xffff00) | v; return;
    case 0x2233: m.dsa = (m.dsa & 0xff00ff) | v << 8; return;
    case 0x2234: m.dsa = (m.dsa & 0x00ffff) | v << 16; return;
    case 0x2235: m.dda = (m.dda & 0xffff00) | v; return;
    case 0x2236:
        m.dda = (m.dda & 0xff00ff) | v << 8;
        if (m.dmaen)
        {
            if (!m.cden && !m.dd) DmaNormal();
            else if (m.cden && m.cdsel)
            {
                // Conversión tipo 1: avisa a la SNES, que lee los mosaicos de la BW-RAM.
                m.dmaBw = true;
                m.chdmaIrqfl = true;
                if (m.chdmaIrqen) { m.chdmaIrqcl = false; IrqSnes(true); }
            }
        }
        return;
    case 0x2237:
        m.dda = (m.dda & 0x00ffff) | v << 16;
        if (m.dmaen && !m.cden && m.dd) DmaNormal();
        return;
    }
}

// La SNES escribe en $2200-$23FF.
static void EscribirIoCpu(uint16_t r, uint8_t v)
{
    switch (r)
    {
    case 0x2200:
        if (m.sa1Resb && !(v & 0x20))
        {
            // Sale del reinicio: arranca en el vector de reinicio del SA-1.
            sa1cpu.pc = m.crv;
            sa1cpu.pbr = 0;
            sa1cpu.dbr = 0;
            sa1cpu.parado = false;
            m.ciwp = 0;
            if (sa1Ciclos < ciclosMaestros) sa1Ciclos = ciclosMaestros;
        }
        m.sa1Irq = v & 0x80; m.sa1Rdyb = v & 0x40; m.sa1Resb = v & 0x20; m.sa1NmiPin = v & 0x10; m.smeg = v & 0x0f;
        if (m.sa1Irq) { m.sa1Irqfl = true; if (m.sa1Irqen) m.sa1Irqcl = false; }
        if (m.sa1NmiPin) { m.sa1Nmifl = true; if (m.sa1Nmien) m.sa1Nmicl = false; }
        ActualizarIrq();
        return;
    case 0x2201:
        if (!m.cpuIrqen && (v & 0x80) && m.cpuIrqfl) { m.cpuIrqcl = false; IrqSnes(true); }
        if (!m.chdmaIrqen && (v & 0x20) && m.chdmaIrqfl) { m.chdmaIrqcl = false; IrqSnes(true); }
        m.cpuIrqen = v & 0x80; m.chdmaIrqen = v & 0x20;
        return;
    case 0x2202:
        m.cpuIrqcl = v & 0x80; m.chdmaIrqcl = v & 0x20;
        if (m.cpuIrqcl) m.cpuIrqfl = false;
        if (m.chdmaIrqcl) m.chdmaIrqfl = false;
        if (!m.cpuIrqfl && !m.chdmaIrqfl) IrqSnes(false);
        return;
    case 0x2203: m.crv = (m.crv & 0xff00) | v; return;
    case 0x2204: m.crv = (uint16_t)(v << 8 | (m.crv & 0xff)); return;
    case 0x2205: m.cnv = (m.cnv & 0xff00) | v; return;
    case 0x2206: m.cnv = (uint16_t)(v << 8 | (m.cnv & 0xff)); return;
    case 0x2207: m.civ = (m.civ & 0xff00) | v; return;
    case 0x2208: m.civ = (uint16_t)(v << 8 | (m.civ & 0xff)); return;
    case 0x2220: m.cbmode = v & 0x80; m.cb = v & 7; return;
    case 0x2221: m.dbmode = v & 0x80; m.db = v & 7; return;
    case 0x2222: m.ebmode = v & 0x80; m.eb = v & 7; return;
    case 0x2223: m.fbmode = v & 0x80; m.fb = v & 7; return;
    case 0x2224: m.sbm = v & 0x1f; return;
    case 0x2226: m.swen = v & 0x80; return;
    case 0x2228: m.bwp = v & 0x0f; return;
    case 0x2229: m.siwp = v; return;
    case 0x2231: case 0x2232: case 0x2233: case 0x2234: case 0x2235: case 0x2236: case 0x2237:
        EscribirCompartido(r, v);
        return;
    }
}

static uint8_t LeerIoCpu(uint16_t r, uint8_t abierto)
{
    if (r == 0x2300)
        return (uint8_t)(m.cpuIrqfl << 7 | m.cpuIvsw << 6 | m.chdmaIrqfl << 5 | m.cpuNvsw << 4 | m.cmeg);
    return abierto;
}

// El SA-1 escribe en $2200-$23FF.
static void EscribirIoSa1(uint16_t r, uint8_t v)
{
    switch (r)
    {
    case 0x2209:
        m.cpuIrq = v & 0x80; m.cpuIvsw = v & 0x40; m.cpuNvsw = v & 0x10; m.cmeg = v & 0x0f;
        if (m.cpuIrq) { m.cpuIrqfl = true; if (m.cpuIrqen) { m.cpuIrqcl = false; IrqSnes(true); } }
        return;
    case 0x220a:
        if (!m.sa1Irqen && (v & 0x80) && m.sa1Irqfl) m.sa1Irqcl = false;
        if (!m.timerIrqen && (v & 0x40) && m.timerIrqfl) m.timerIrqcl = false;
        if (!m.dmaIrqen && (v & 0x20) && m.dmaIrqfl) m.dmaIrqcl = false;
        if (!m.sa1Nmien && (v & 0x10) && m.sa1Nmifl) m.sa1Nmicl = false;
        m.sa1Irqen = v & 0x80; m.timerIrqen = v & 0x40; m.dmaIrqen = v & 0x20; m.sa1Nmien = v & 0x10;
        ActualizarIrq();
        return;
    case 0x220b:
        m.sa1Irqcl = v & 0x80; m.timerIrqcl = v & 0x40; m.dmaIrqcl = v & 0x20; m.sa1Nmicl = v & 0x10;
        if (m.sa1Irqcl) m.sa1Irqfl = false;
        if (m.timerIrqcl) m.timerIrqfl = false;
        if (m.dmaIrqcl) m.dmaIrqfl = false;
        if (m.sa1Nmicl) m.sa1Nmifl = false;
        ActualizarIrq();
        return;
    case 0x220c: m.snv = (m.snv & 0xff00) | v; return;
    case 0x220d: m.snv = (uint16_t)(v << 8 | (m.snv & 0xff)); return;
    case 0x220e: m.siv = (m.siv & 0xff00) | v; return;
    case 0x220f: m.siv = (uint16_t)(v << 8 | (m.siv & 0xff)); return;
    case 0x2210: m.hvselb = v & 0x80; m.ven = v & 0x02; m.hen = v & 0x01; return;
    case 0x2211: m.vcont = 0; m.hcont = 0; return;
    case 0x2212: m.hcnt = (m.hcnt & 0xff00) | v; return;
    case 0x2213: m.hcnt = (uint16_t)((m.hcnt & 0x00ff) | v << 8); return;
    case 0x2214: m.vcnt = (m.vcnt & 0xff00) | v; return;
    case 0x2215: m.vcnt = (uint16_t)((m.vcnt & 0x00ff) | v << 8); return;
    case 0x2225: m.sw46 = v & 0x80; m.cbm = v & 0x7f; return;
    case 0x2227: m.cwen = v & 0x80; return;
    case 0x222a: m.ciwp = v; return;
    case 0x2230:
        m.dmaen = v & 0x80; m.dprio = v & 0x40; m.cden = v & 0x20; m.cdsel = v & 0x10; m.dd = v & 0x04; m.sd = v & 0x03;
        if (!m.dmaen) m.lineaCC2 = 0;
        return;
    case 0x2231: case 0x2232: case 0x2233: case 0x2234: case 0x2235: case 0x2236: case 0x2237:
        EscribirCompartido(r, v);
        return;
    case 0x2238: m.dtc = (m.dtc & 0xff00) | v; return;
    case 0x2239: m.dtc = (uint16_t)((m.dtc & 0x00ff) | v << 8); return;
    case 0x223f: m.bbf = v & 0x80; return;
    case 0x2250: m.acm = v & 0x02; m.md = v & 0x01; if (m.acm) m.mr = 0; return;
    case 0x2251: m.ma = (m.ma & 0xff00) | v; return;
    case 0x2252: m.ma = (uint16_t)((m.ma & 0x00ff) | v << 8); return;
    case 0x2253: m.mb = (m.mb & 0xff00) | v; return;
    case 0x2254:
        m.mb = (uint16_t)((m.mb & 0x00ff) | v << 8);
        if (!m.acm)
        {
            if (!m.md) { m.mr = (uint32_t)((int16_t)m.ma * (int16_t)m.mb); m.mb = 0; }   // multiplicación con signo
            else
            {
                // División con signo: el chip redondea hacia menos infinito (resto siempre positivo).
                if (!m.mb) m.mr = 0;
                else
                {
                    int16_t dividendo = (int16_t)m.ma;
                    uint16_t divisor = m.mb;
                    uint32_t ext = (uint32_t)(dividendo + (uint32_t)divisor * 65536);
                    uint16_t resto = (uint16_t)(ext % divisor);
                    uint16_t cociente = (uint16_t)(ext / divisor - 65536);
                    m.mr = (uint32_t)resto << 16 | cociente;
                }
                m.ma = 0; m.mb = 0;
            }
        }
        else
        {
            // Suma acumulada de productos (40 bits).
            m.mr += (uint64_t)(int64_t)((int16_t)m.ma * (int16_t)m.mb);
            m.desborde = (m.mr >> 40) & 1;
            m.mr &= 0xffffffffffull;
            m.mb = 0;
        }
        return;
    case 0x2258:
        m.hl = v & 0x80; m.vb = v & 0x0f; if (!m.vb) m.vb = 16;
        if (!m.hl) { m.vbit += m.vb; m.va += m.vbit >> 3; m.vbit &= 7; }
        return;
    case 0x2259: m.va = (m.va & 0xffff00) | v; return;
    case 0x225a: m.va = (m.va & 0xff00ff) | v << 8; return;
    case 0x225b: m.va = (m.va & 0x00ffff) | v << 16; m.vbit = 0; return;
    }
    if (r >= 0x2240 && r <= 0x224f)
    {
        m.brf[r - 0x2240] = v;
        if ((r == 0x2247 || r == 0x224f) && m.dmaen && m.cden && !m.cdsel) Cc2();
    }
}

// Lectura de bits de largo variable: la rom, la BW-RAM o la I-RAM, sin efectos.
static uint8_t LeerVbr(uint32_t a)
{
    a &= 0xffffff;
    if ((a & 0x408000) == 0x008000 || (a & 0xc00000) == 0xc00000) return RomSa1(a);
    if ((a & 0x40e000) == 0x006000 || (a & 0xf00000) == 0x400000) return BwLeer(a);
    if ((a & 0x40f800) == 0x000000 || (a & 0x40f800) == 0x003000) return iram[a & 0x7ff];
    return 0xff;
}

static uint8_t LeerIoSa1(uint16_t r)
{
    switch (r)
    {
    case 0x2301: return (uint8_t)(m.sa1Irqfl << 7 | m.timerIrqfl << 6 | m.dmaIrqfl << 5 | m.sa1Nmifl << 4 | m.smeg);
    case 0x2302: m.hcr = m.hcont >> 2; m.vcr = m.vcont; return (uint8_t)m.hcr;
    case 0x2303: return m.hcr >> 8;
    case 0x2304: return (uint8_t)m.vcr;
    case 0x2305: return m.vcr >> 8;
    case 0x2306: return (uint8_t)m.mr;
    case 0x2307: return (uint8_t)(m.mr >> 8);
    case 0x2308: return (uint8_t)(m.mr >> 16);
    case 0x2309: return (uint8_t)(m.mr >> 24);
    case 0x230a: return (uint8_t)(m.mr >> 32);
    case 0x230b: return m.desborde << 7;
    case 0x230c: case 0x230d:
    {
        uint32_t d = LeerVbr(m.va) | LeerVbr(m.va + 1) << 8 | LeerVbr(m.va + 2) << 16;
        d >>= m.vbit;
        if (r == 0x230c) return (uint8_t)d;
        if (m.hl) { m.vbit += m.vb; m.va += m.vbit >> 3; m.vbit &= 7; }   // avance automático
        return (uint8_t)(d >> 8);
    }
    }
    return 0xff;
}

// ---------- el bus del SA-1 ----------
uint8_t Sa1Leer(uint32_t a)
{
    a &= 0xffffff;
    if ((a & 0x40fe00) == 0x002200) return LeerIoSa1(0x2200 | (a & 0x1ff));
    // Vectores del SA-1: los da el chip (sus registros de reinicio, NMI e IRQ).
    if ((a & 0xfffff0) == 0x00fff0 || (a & 0xfffff0) == 0x00ffe0)
    {
        uint16_t o = a & 0xffff;
        if (o == 0xffea || o == 0xfffa) return (uint8_t)m.cnv;
        if (o == 0xffeb || o == 0xfffb) return m.cnv >> 8;
        if (o == 0xffee || o == 0xfffe) return (uint8_t)m.civ;
        if (o == 0xffef || o == 0xffff) return m.civ >> 8;
        if (o == 0xfffc) return (uint8_t)m.crv;
        if (o == 0xfffd) return m.crv >> 8;
    }
    if ((a & 0x408000) == 0x008000 || (a & 0xc00000) == 0xc00000) return RomSa1(a);
    if ((a & 0x40e000) == 0x006000)
    {
        if (!m.sw46) return BwLeer((m.cbm & 0x1f) * 0x2000 + (a & 0x1fff));
        return BitmapLeer(m.cbm * 0x2000 + (a & 0x1fff));
    }
    if ((a & 0xe00000) == 0x400000) return BwLeer(a);
    if ((a & 0xf00000) == 0x600000) return BitmapLeer(a);
    if ((a & 0x40f800) == 0x000000 || (a & 0x40f800) == 0x003000) return iram[a & 0x7ff];
    return sa1cpu.a >> 8;   // nada: bus abierto (aproximado)
}

void Sa1Escribir(uint32_t a, uint8_t v)
{
    a &= 0xffffff;
    if ((a & 0x40fe00) == 0x002200) { EscribirIoSa1(0x2200 | (a & 0x1ff), v); return; }
    if ((a & 0x408000) == 0x008000 || (a & 0xc00000) == 0xc00000) return;   // rom
    if ((a & 0x40e000) == 0x006000)
    {
        if (!m.sw46) { uint32_t d = (m.cbm & 0x1f) * 0x2000 + (a & 0x1fff); if (!BwProtegida(d)) BwEscribir(d, v); }
        else BitmapEscribir(m.cbm * 0x2000 + (a & 0x1fff), v);
        return;
    }
    if ((a & 0xe00000) == 0x400000) { if (!BwProtegida(a)) BwEscribir(a, v); return; }
    if ((a & 0xf00000) == 0x600000) { BitmapEscribir(a, v); return; }
    if ((a & 0x40f800) == 0x000000 || (a & 0x40f800) == 0x003000) IramEscribir(a & 0x7ff, v, m.ciwp);
}

// Costo de un acceso del SA-1 en ciclos maestros: la BW-RAM es más lenta (dos ciclos del SA-1); lo demás, uno.
int Sa1Costo(uint32_t a)
{
    a &= 0xffffff;
    if ((a & 0x40e000) == 0x006000 || (a & 0xe00000) == 0x400000 || (a & 0xf00000) == 0x600000) return 4;
    return 2;
}

// El temporizador del SA-1 avanza con su reloj.
void Sa1Ciclo(int c)
{
    sa1Ciclos += c;
    if (!m.hen && !m.ven && !m.hvselb) { m.hcont = (uint16_t)(m.hcont + c); if (m.hcont >= 1364) { m.hcont -= 1364; m.vcont++; } return; }
    uint16_t antes = m.hcont;
    bool vuelta = false;
    if (!m.hvselb)
    {
        m.hcont = (uint16_t)(m.hcont + c);
        if (m.hcont >= 1364)
        {
            m.hcont -= 1364;
            vuelta = true;
            if (++m.vcont >= (SnesInfo().pal ? 312 : 262)) m.vcont = 0;
        }
    }
    else
    {
        uint32_t h = m.hcont + c;
        m.vcont = (uint16_t)((m.vcont + (h >> 11)) & 0x1ff);
        m.hcont = h & 0x7ff;
        vuelta = h >> 11;
    }
    uint32_t objetivo = (uint32_t)m.hcnt << 2;
    bool cruzaH = vuelta ? (objetivo > antes || objetivo <= m.hcont) : (objetivo > antes && objetivo <= m.hcont);
    bool salta = false;
    if (m.hen && !m.ven) salta = cruzaH;
    else if (!m.hen && m.ven) salta = vuelta && m.vcont == m.vcnt;
    else if (m.hen && m.ven) salta = cruzaH && m.vcont == m.vcnt;
    if (salta)
    {
        m.timerIrqfl = true;
        if (m.timerIrqen) m.timerIrqcl = false;
        ActualizarIrq();
    }
}

// ---------- el lado de la SNES ----------
// La SNES lee el cartucho. Devuelve true si la dirección es del SA-1 (y deja el valor en v).
bool Sa1LeerCpu(uint8_t banco, uint16_t off, uint8_t& v, uint8_t abierto)
{
    uint8_t b = banco & 0x7f;
    if (banco >= 0xc0) { v = RomCpu(((uint32_t)banco << 16 | off)); return true; }
    if (b >= 0x40)
    {
        if (b < 0x50) { Sa1CorrerHasta(ciclosMaestros); v = m.dmaBw ? Cc1Leer((uint32_t)banco << 16 | off) : BwLeer((uint32_t)b << 16 | off); return true; }
        v = abierto;
        return true;
    }
    if (off >= 0x8000) { v = RomCpu(TraducirLo((uint32_t)banco << 16 | off)); return true; }
    if (off >= 0x6000) { Sa1CorrerHasta(ciclosMaestros); uint32_t d = m.sbm * 0x2000u + (off & 0x1fff); v = m.dmaBw ? Cc1Leer(d) : BwLeer(d); return true; }
    if (off >= 0x3000 && off < 0x3800) { Sa1CorrerHasta(ciclosMaestros); v = iram[off & 0x7ff]; return true; }
    if (off >= 0x2200 && off < 0x2400) { Sa1CorrerHasta(ciclosMaestros); v = LeerIoCpu(0x2200 | (off & 0x1ff), abierto); return true; }
    return false;
}

bool Sa1EscribirCpu(uint8_t banco, uint16_t off, uint8_t v)
{
    uint8_t b = banco & 0x7f;
    if (banco >= 0xc0) return true;
    if (b >= 0x40)
    {
        if (b < 0x50) { Sa1CorrerHasta(ciclosMaestros); uint32_t d = (uint32_t)b << 16 | off; if (!BwProtegida(d)) BwEscribir(d, v); }
        return true;
    }
    if (off >= 0x8000) return true;
    if (off >= 0x6000) { Sa1CorrerHasta(ciclosMaestros); uint32_t d = m.sbm * 0x2000u + (off & 0x1fff); if (!BwProtegida(d)) BwEscribir(d, v); return true; }
    if (off >= 0x3000 && off < 0x3800) { Sa1CorrerHasta(ciclosMaestros); IramEscribir(off & 0x7ff, v, m.siwp); return true; }
    if (off >= 0x2200 && off < 0x2400) { Sa1CorrerHasta(ciclosMaestros); EscribirIoCpu(0x2200 | (off & 0x1ff), v); return true; }
    return false;
}

// ---------- marcha ----------
void Sa1CorrerHasta(uint64_t hasta)
{
    if (!sa1Presente || sa1Ciclos >= hasta) return;
    if (m.sa1Rdyb || m.sa1Resb)
    {
        // Detenido (en reinicio o en espera): solo avanza el temporizador.
        while (sa1Ciclos < hasta) Sa1Ciclo((int)(hasta - sa1Ciclos > 1364 ? 1364 : hasta - sa1Ciclos));
        return;
    }
    ActualizarIrq();
    Sa1CpuCorrerHasta(hasta);
}

void Sa1Reiniciar()
{
    memset(&m, 0, sizeof m);
    memset(iram, 0, sizeof iram);
    m.sa1Resb = true;
    m.db = 1; m.eb = 2; m.fb = 3;
    m.bwp = 0x0f;
    m.vb = 16;
    sa1Nmi = sa1Irq = false;
    irqSa1 = false;
    sa1Ciclos = 0;
    Sa1CpuReiniciar();   // (lee el vector con el registro de reinicio en 0; se fija al salir del reinicio)
}

void Sa1Estado(Serial& s)
{
    s(m);
    s(iram);
    s(sa1Ciclos);
    s(sa1Nmi); s(sa1Irq); s(irqSa1);
    Sa1CpuEstado(s);
}
