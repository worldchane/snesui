// El bus de memoria de la SNES: traduce cada dirección de 24 bits a la RAM de trabajo, la rom, la SRAM de guardado o un
// registro de hardware (PPU, APU, mando, DMA...). El mapa sigue el de la consola real.
//   bancos 0x00-0x3F y 0x80-0xBF: 0x0000-0x1FFF espejo de la RAM, 0x2100-0x21FF PPU/APU, 0x4000-0x43FF mando/DMA/CPU,
//                                 0x8000-0xFFFF rom.
//   bancos 0x40-0x7D y 0xC0-0xFF: rom (en HiROM, los 64 KB completos).
//   bancos 0x7E-0x7F: los 128 KB de RAM de trabajo.
#include "interno.h"
#include <stdlib.h>
#include <stdio.h>
#include "rom.h"
#include <stdio.h>

void ApuLog(int escribe, int puerto, uint8_t v);

uint8_t wram[128 * 1024];
uint16_t mando[2];
bool sramCambiada;

// Registros del propio CPU y del DMA.
static uint8_t mdr;                 // último valor del bus (lo que se lee de una dirección "vacía")
static uint8_t nmitimen, hvbjoy, rdnmi;
static bool nmiHabilitado, autoLeerMando;
static uint16_t mult_a; static uint8_t mult_b; static uint16_t divisor; static uint16_t dividendo;
static uint16_t prodDiv;            // resultado de multiplicación/división (RDMPYL/H)
static uint16_t cociente;
static uint16_t mandoLeido[2];      // copia leída al empezar el cuadro (auto-join)
static uint16_t registroMando[2];   // los botones en el orden de la consola (B en el bit 15 ... R en el bit 4)
static uint16_t serieMando[2];      // lectura bit por bit por $4016/$4017
static bool cerrojoMando;
static uint8_t wmaddL, wmaddM, wmaddH;   // puerto de acceso a la RAM (0x2180-0x2183)
static uint16_t htime = 0x1FF, vtime = 0x1FF;   // posición de la interrupción por tiempo (punto y línea)
static bool irqBandera;             // $4211 bit 7: hubo interrupción por tiempo
static uint8_t wrio = 0xFF;          // $4201: puerto de E/S (el bit 7 al bajar fija los contadores del haz)
static uint8_t hdmaen;               // $420C: canales con HDMA
static int mandoOcupado;             // líneas que faltan para terminar la lectura automática del mando
bool memselRapido;                   // $420D: la rom de los bancos 0x80-0xFF se lee a 6 ciclos (FastROM)
bool irqReprogramada;               // se escribió $4200 o HTIME/VTIME: el cuadro recalcula la IRQ por tiempo
bool irqLinea, nmiPendiente;         // pedidos de interrupción que el procesador atiende entre instrucciones

// DMA: 8 canales.
// tam también es la dirección indirecta del HDMA; dirHdma es la posición en la tabla; lineas el contador de líneas.
struct Canal { uint8_t ctrl, dest; uint16_t dir; uint8_t banco; uint16_t tam; uint8_t bancoInd; uint16_t dirHdma; uint8_t lineas, libre; };
static Canal dma[8];
static bool hdmaFin[8], hdmaHacer[8];

static inline uint32_t MapaRom(uint8_t banco, uint16_t off)
{
    uint32_t o;
    if (romHiRom)
    {
        if ((banco & 0x7F) >= 0x40) o = ((uint32_t)(banco & 0x3F) << 16) | off;
        else o = ((uint32_t)(banco & 0x3F) << 16) | off;   // 0x00-0x3F usan 0x8000-0xFFFF de la mitad alta
    }
    else
        o = ((uint32_t)(banco & 0x7F) << 15) | (off & 0x7FFF);
    return romTam ? o % romTam : 0;
}

// SRAM de guardado. Devuelve su dirección, o -1 si ahí no hay SRAM.
static inline int32_t MapaSram(uint8_t banco, uint16_t off)
{
    if (!sramTam) return -1;
    if (romHiRom)
    {
        if ((banco & 0x7F) >= 0x20 && (banco & 0x7F) <= 0x3F && off >= 0x6000 && off < 0x8000)
            return (int32_t)((((banco & 0x1F) << 13) | (off - 0x6000)) % sramTam);
    }
    else
    {
        uint8_t b = banco & 0x7F;
        if (b >= 0x70 && b <= 0x7D && off < 0x8000) return (int32_t)(((uint32_t)(b - 0x70) << 15 | off) % sramTam);
    }
    return -1;
}

extern uint64_t ciclosMaestros;
extern uint32_t ramMascara;   // superfx.cpp
#ifdef GSU_TRAZA
unsigned gsuLecturasBloqueadas, gsuRomBloqueada;
#endif

// Cartuchos Super FX: rom LoROM en 00-3F:8000-FFFF y lineal en 40-5F; RAM del chip en 70-71 (y sus primeros 8 KB en
// 00-3F:6000-7FFF); registros del chip en 00-3F:3000-32FF. Antes de tocar algo del chip se lo pone al día.
// Devuelve true si la dirección es del cartucho Super FX (y deja el valor en 'v').
// Mientras el chip usa la rom, la SNES no la ve: el cartucho contesta con direcciones fijas en los vectores de
// interrupción (NMI → $0108, IRQ → $010C...), que apuntan a la RAM de la consola. Los juegos ponen ahí sus rutinas
// para cuando el chip está dibujando.
static const uint8_t VECTOR_GSU[16] = {0x00, 0x01, 0x00, 0x01, 0x04, 0x01, 0x00, 0x01, 0x00, 0x01, 0x08, 0x01, 0x00, 0x01, 0x0C, 0x01};

static inline bool LeerSuperFx(uint8_t banco, uint16_t off, uint8_t& v)
{
    uint8_t b = banco & 0x7F;
    if (b < 0x40)
    {
        if (off >= 0x3000 && off < 0x3300) { GsuCorrerHasta(ciclosMaestros); v = GsuLeer(off); return true; }
        if (off >= 0x6000 && off < 0x8000)
        {
            GsuCorrerHasta(ciclosMaestros);
    #ifdef GSU_TRAZA
        if (GsuTieneRam()) gsuLecturasBloqueadas++;
#endif
        v = GsuTieneRam() ? mdr : sram[(off - 0x6000) & ramMascara];
            return true;
        }
        if (off >= 0x8000 && gsuCorre)
        {
            GsuCorrerHasta(ciclosMaestros);
            if (GsuTieneRom()) {
#ifdef GSU_TRAZA
            { gsuRomBloqueada++; static int n_ = 0; if (getenv("ROMBLOQ") && n_ < 40 && (gsuRomBloqueada % 500) == 1) { n_++; printf("  rom bloqueada %02X:%04X desde PC=%02X:%04X\n", banco, off, cpu.pbr, cpu.pc); } }
#endif
            v = VECTOR_GSU[off & 15]; return true; }
        }
        return false;
    }
    if (b < 0x60)
    {
        if (gsuCorre) { GsuCorrerHasta(ciclosMaestros); if (GsuTieneRom()) {
#ifdef GSU_TRAZA
            { gsuRomBloqueada++; static int n_ = 0; if (getenv("ROMBLOQ") && n_ < 40 && (gsuRomBloqueada % 500) == 1) { n_++; printf("  rom bloqueada %02X:%04X desde PC=%02X:%04X\n", banco, off, cpu.pbr, cpu.pc); } }
#endif
            v = VECTOR_GSU[off & 15]; return true; } }
        v = romDatos[(((uint32_t)(b & 0x1F) << 16) | off) % romTam];
        return true;
    }
    if (b == 0x70 || b == 0x71)
    {
        GsuCorrerHasta(ciclosMaestros);
#ifdef GSU_TRAZA
        if (GsuTieneRam()) gsuLecturasBloqueadas++;
#endif
        v = GsuTieneRam() ? mdr : sram[(((uint32_t)(b & 1) << 16) | off) & ramMascara];
        return true;
    }
    if (b >= 0x60 && b < 0x7E) { v = mdr; return true; }
    return false;
}

static inline bool EscribirSuperFx(uint8_t banco, uint16_t off, uint8_t v)
{
    uint8_t b = banco & 0x7F;
    if (b < 0x40)
    {
        if (off >= 0x3000 && off < 0x3300) { GsuCorrerHasta(ciclosMaestros); GsuEscribir(off, v); return true; }
        if (off >= 0x6000 && off < 0x8000)
        {
            GsuCorrerHasta(ciclosMaestros);
#ifdef GSU_TRAZA
            { void VigilarRam(uint32_t, uint8_t, const char*); VigilarRam(off - 0x6000, v, GsuTieneRam() ? "SNES(bloq)" : "SNES"); }
#endif
            if (!GsuTieneRam()) { sram[(off - 0x6000) & ramMascara] = v; sramCambiada = true; }   // con la RAM en uso por el chip, se ignora
            return true;
        }
        return false;
    }
    if (b == 0x70 || b == 0x71)
    {
        GsuCorrerHasta(ciclosMaestros);
#ifdef GSU_TRAZA
        { void VigilarRam(uint32_t, uint8_t, const char*); VigilarRam(((uint32_t)(b & 1) << 16) | off, v, GsuTieneRam() ? "SNES(bloq)" : "SNES"); }
#endif
        if (!GsuTieneRam()) { sram[(((uint32_t)(b & 1) << 16) | off) & ramMascara] = v; sramCambiada = true; }
        return true;
    }
    return b >= 0x40 && b < 0x7E;   // la rom y lo que no existe no se escriben
}

uint8_t BusLeer(uint32_t dir)
{
    uint8_t banco = dir >> 16;
    uint16_t off = dir & 0xFFFF;
    uint8_t b = banco & 0x7F;
    if (gsuPresente && banco != 0x7E && banco != 0x7F) { uint8_t v; if (LeerSuperFx(banco, off, v)) return mdr = v; }

    if (banco == 0x7E || banco == 0x7F) return mdr = wram[(banco - 0x7E) << 16 | off];

    if (b < 0x40)   // bancos de sistema (0x00-0x3F y 0x80-0xBF)
    {
        if (off < 0x2000) return mdr = wram[off];
        if (off < 0x2100) return mdr;
        if (off < 0x2140) return mdr = PpuLeer(off, mdr);
        if (off < 0x2144) { uint8_t v = ApuLeer(off & 3); ApuLog(0, off & 3, v); return mdr = v; }
        if (off == 0x2180) { uint32_t a = (wmaddH << 16 | wmaddM << 8 | wmaddL) & 0x1FFFF; uint8_t v = wram[a]; a = (a + 1) & 0x1FFFF; wmaddL = a; wmaddM = a >> 8; wmaddH = a >> 16; return mdr = v; }
        if (off < 0x4000) return mdr;
        if (off == 0x4016 || off == 0x4017)
        {
            int j = off - 0x4016;
            if (cerrojoMando) serieMando[j] = registroMando[j];
            uint8_t bit = serieMando[j] >> 15;
            serieMando[j] = (uint16_t)(serieMando[j] << 1 | 1);
            // $4017: los bits 2-4 están siempre en 1 en la consola (algunos juegos lo usan para detectar el multitap).
            return mdr = j ? (uint8_t)((mdr & 0xE0) | 0x1C | bit) : (uint8_t)((mdr & 0xFC) | bit);
        }
        if (off == 0x4210) { uint8_t v = (rdnmi & 0x80) | (mdr & 0x70) | 0x02; rdnmi &= 0x7F; return mdr = v; }   // NMI ocurrida (bit 7, se borra al leer); 4-6 bus abierto
        if (off == 0x4211) { uint8_t v = (irqBandera ? 0x80 : 0) | (mdr & 0x7F); irqBandera = irqLinea = false; return mdr = v; }   // IRQ por tiempo (se borra al leer)
        if (off == 0x4212) return mdr = (hvbjoy & 0xC0) | (mandoOcupado ? 1 : 0) | (mdr & 0x3E);   // vblank/hblank/mando; 1-5 bus abierto
        if (off == 0x4213) return mdr = wrio;
        if (off == 0x4214) return mdr = cociente & 0xFF;
        if (off == 0x4215) return mdr = cociente >> 8;
        if (off == 0x4216) return mdr = prodDiv & 0xFF;
        if (off == 0x4217) return mdr = prodDiv >> 8;
        if (off == 0x4218) return mdr = mandoLeido[0] & 0xFF;
        if (off == 0x4219) return mdr = mandoLeido[0] >> 8;
        if (off == 0x421A) return mdr = mandoLeido[1] & 0xFF;
        if (off == 0x421B) return mdr = mandoLeido[1] >> 8;
        if (off >= 0x421C && off <= 0x421F) return mdr = 0;   // mandos 3 y 4 (por un multitap): no hay ninguno
        if (off >= 0x4300 && off < 0x4380)
        {
            Canal& c = dma[(off >> 4) & 7];
            switch (off & 0xF)
            {
            case 0: return mdr = c.ctrl;
            case 1: return mdr = c.dest;
            case 2: return mdr = c.dir & 0xFF;
            case 3: return mdr = c.dir >> 8;
            case 4: return mdr = c.banco;
            case 5: return mdr = c.tam & 0xFF;
            case 6: return mdr = c.tam >> 8;
            case 7: return mdr = c.bancoInd;
            case 8: return mdr = c.dirHdma & 0xFF;
            case 9: return mdr = c.dirHdma >> 8;
            case 0xA: return mdr = c.lineas;
            case 0xB: case 0xF: return mdr = c.libre;
            }
            return mdr;
        }
        if (off >= 0x8000) return mdr = romDatos[MapaRom(banco, off)];
        int32_t sr = MapaSram(banco, off);
        if (sr >= 0) return mdr = sram[sr];
        return mdr;
    }
    // bancos de rom 0x40-0x7D y 0xC0-0xFF
    int32_t sr = MapaSram(banco, off);
    if (sr >= 0) return mdr = sram[sr];
    return mdr = romDatos[MapaRom(banco, off)];
}

static void IniciarDma(uint8_t mascara);

void BusEscribir(uint32_t dir, uint8_t v)
{
    uint8_t banco = dir >> 16;
    uint16_t off = dir & 0xFFFF;
    uint8_t b = banco & 0x7F;
    mdr = v;
    if (gsuPresente && banco != 0x7E && banco != 0x7F && EscribirSuperFx(banco, off, v)) return;

    if (banco == 0x7E || banco == 0x7F) { wram[(banco - 0x7E) << 16 | off] = v; return; }

    if (b < 0x40)
    {
        if (off < 0x2000) { wram[off] = v; return; }
        if (off >= 0x2100 && off < 0x2140) { PpuEscribir(off, v); return; }
        if (off >= 0x2140 && off < 0x2144) { ApuLog(1, off & 3, v); ApuEscribir(off & 3, v); return; }
        switch (off)
        {
        case 0x2180: { uint32_t a = (wmaddH << 16 | wmaddM << 8 | wmaddL) & 0x1FFFF; wram[a] = v; a = (a + 1) & 0x1FFFF; wmaddL = a; wmaddM = a >> 8; wmaddH = a >> 16; return; }
        case 0x2181: wmaddL = v; return;
        case 0x2182: wmaddM = v; return;
        case 0x2183: wmaddH = v & 1; return;
        case 0x4016: cerrojoMando = v & 1; if (cerrojoMando) { serieMando[0] = registroMando[0]; serieMando[1] = registroMando[1]; } return;
        default: break;
        }
        if (off == 0x4200 || (off >= 0x4207 && off <= 0x420A)) irqReprogramada = true;
#ifdef GSU_TRAZA
        if (off == 0x4200 || (off >= 0x4207 && off <= 0x420A))
        { extern int depurCuadro; static int l_ = getenv("IRQREG") ? 1 : 0; if (l_ && depurCuadro) { void SnesContadores(int*, int*); int h_, li; SnesContadores(&h_, &li); printf("  irq %04X=%02X linea %d h %d\n", off, v, li, h_); } }
#endif
        switch (off)
        {
        case 0x4200:
            // Encender la NMI en pleno vblank (con la bandera todavía puesta) la dispara enseguida.
            if (!nmiHabilitado && (v & 0x80) && (rdnmi & 0x80)) nmiPendiente = true;
            nmitimen = v; nmiHabilitado = v & 0x80; autoLeerMando = v & 1;
            if (!(v & 0x30)) irqBandera = irqLinea = false;
            return;
        case 0x4201:
            if ((wrio & 0x80) && !(v & 0x80)) { extern void PpuLatchContadores(); PpuLatchContadores(); }
            wrio = v;
            return;
        case 0x4207: htime = (htime & 0x100) | v; return;
        case 0x4208: htime = (htime & 0xFF) | ((v & 1) << 8); return;
        case 0x4209: vtime = (vtime & 0x100) | v; return;
        case 0x420A: vtime = (vtime & 0xFF) | ((v & 1) << 8); return;
        case 0x420D: memselRapido = v & 1; return;
        case 0x4202: mult_a = v; return;
        case 0x4203: mult_b = v; prodDiv = (uint16_t)(mult_a & 0xFF) * mult_b; return;
        case 0x4204: dividendo = (dividendo & 0xFF00) | v; return;
        case 0x4205: dividendo = (dividendo & 0x00FF) | (v << 8); return;
        case 0x4206:
            divisor = v;
            if (v) { cociente = dividendo / v; prodDiv = dividendo % v; }
            else { cociente = 0xFFFF; prodDiv = dividendo; }
            return;
        case 0x420B:
#ifdef GSU_TRAZA
            { extern int depurCuadro; static int l_ = getenv("PPUREG") ? 1 : 0; if (l_ && depurCuadro) { void SnesContadores(int*, int*); int h_, li; SnesContadores(&h_, &li); printf("  DMA %02X linea %d h %d", v, li, h_); for (int k = 0; k < 8; k++) if (v & (1 << k)) printf(" [c%d %02X:%04X -> 21%02X tam %u]", k, dma[k].banco, dma[k].dir, dma[k].dest, dma[k].tam ? dma[k].tam : 65536); printf("\n"); } }
#endif
            IniciarDma(v); return;         // DMA general
        case 0x420C: hdmaen = v;
#ifdef GSU_TRAZA
            { extern int depurCuadro; static int l_ = getenv("PPUREG") ? 1 : 0; if (l_ && depurCuadro) { { void SnesContadores(int*, int*); int h_, l_; SnesContadores(&h_, &l_); printf("  HDMAEN=%02X linea %d\n", v, l_); } for (int k = 0; k < 8; k++) if (v & (1 << k)) printf("    canal %d ctrl=%02X dest=21%02X fuente=%02X:%04X ind=%02X\n", k, dma[k].ctrl, dma[k].dest, dma[k].banco, dma[k].dir, dma[k].bancoInd); } }
#endif
            return;            // HDMA: arranca al empezar el cuadro siguiente
        }
        if (off >= 0x4300 && off < 0x4380)
        {
            Canal& c = dma[(off >> 4) & 7];
#ifdef GSU_TRAZA
            { extern int depurCuadro; static int l_ = getenv("PPUREG") ? 1 : 0; if (l_ && depurCuadro) { void SnesContadores(int*, int*); int h_, li; SnesContadores(&h_, &li); printf("  dma %04X=%02X linea %d\n", off, v, li); } }
#endif
            switch (off & 0xF)
            {
            case 0: c.ctrl = v; return;
            case 1: c.dest = v; return;
            case 2: c.dir = (c.dir & 0xFF00) | v; return;
            case 3: c.dir = (c.dir & 0x00FF) | (v << 8); return;
            case 4: c.banco = v; return;
            case 5: c.tam = (c.tam & 0xFF00) | v; return;
            case 6: c.tam = (c.tam & 0x00FF) | (v << 8); return;
            case 7: c.bancoInd = v; return;
            case 8: c.dirHdma = (c.dirHdma & 0xFF00) | v; return;
            case 9: c.dirHdma = (c.dirHdma & 0x00FF) | (v << 8); return;
            case 0xA: c.lineas = v; return;
            case 0xB: case 0xF: c.libre = v; return;
            }
            return;
        }
        if (off >= 0x8000) return;   // la rom no se escribe
        int32_t sr = MapaSram(banco, off);
        if (sr >= 0 && sram[sr] != v) { sram[sr] = v; sramCambiada = true; }
        return;
    }
    int32_t sr = MapaSram(banco, off);
    if (sr >= 0 && sram[sr] != v) { sram[sr] = v; sramCambiada = true; }
}

// ---------- DMA ----------
// Copia bloques entre la RAM/rom y los registros de la PPU. El patrón (dest) dice a cuántos registros seguidos escribe.
static const uint8_t PATRON[8][4] = {
    {0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
    {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
};
static const int LARGO_PATRON[8] = {1, 2, 2, 4, 4, 4, 2, 4};

static void IniciarDma(uint8_t mascara)
{
    if (mascara) BusCiclo(8);
    for (int i = 0; i < 8; i++)
    {
        if (!(mascara & (1 << i))) continue;
        Canal& c = dma[i];
        bool aPpu = !(c.ctrl & 0x80);        // dirección: a la PPU (escribir) o desde la PPU (leer)
        int paso = (c.ctrl & 0x10) ? -1 : (c.ctrl & 0x08) ? 0 : 1;
        int modo = c.ctrl & 7;
        uint32_t cuenta = c.tam ? c.tam : 0x10000;
        int fase = 0;
        // Si el canal también hace HDMA, el próximo HDMA corta esta copia (así lo hace la consola): se copian solo
        // los bytes que entran hasta ese momento (en pantalla, el hblank de esta línea o la siguiente; en el vblank,
        // el comienzo del cuadro siguiente).
        int64_t limite = -1;
        if (hdmaen & (1 << i))
        {
            extern uint64_t SnesProximoHdma();
            extern uint64_t ciclosMaestros;
            uint64_t hasta = SnesProximoHdma();
            limite = hasta > ciclosMaestros ? (int64_t)((hasta - ciclosMaestros) / 8) : 0;
        }
        while (cuenta--)
        {
            if (limite >= 0 && limite-- == 0) break;
            uint16_t regB = 0x2100 + c.dest + PATRON[modo][fase];
            uint32_t dirA = ((uint32_t)c.banco << 16) | c.dir;
            if (aPpu) BusEscribir(regB, BusLeer(dirA));
            else BusEscribir(dirA, BusLeer(regB));
            c.dir += paso;
            fase = (fase + 1) % LARGO_PATRON[modo];
            BusCiclo(8);
        }
        c.tam = 0;
        BusCiclo(8);
    }
}

// ---------- HDMA ----------
// Cada canal recorre una tabla: un byte con la cantidad de líneas (bit 7 = escribir en todas) y los datos que se copian
// a los registros de la PPU entre línea y línea. Así se hacen degradados, ondas, perspectivas y cambios de ventana.
static void HdmaRecargar(int i)
{
    Canal& c = dma[i];
    c.lineas = BusLeer((uint32_t)c.banco << 16 | c.dirHdma++);
    BusCiclo(8);
    if (c.ctrl & 0x40)   // indirecto: la tabla trae la dirección de los datos
    {
        uint8_t lo = BusLeer((uint32_t)c.banco << 16 | c.dirHdma++);
        uint8_t hi = BusLeer((uint32_t)c.banco << 16 | c.dirHdma++);
        c.tam = lo | hi << 8;
        BusCiclo(16);
    }
    if (!c.lineas) hdmaFin[i] = true;
    hdmaHacer[i] = true;
}

void HdmaIniciar()
{
    // Como en la consola: todos los canales quedan "sin terminar" al empezar el cuadro, aunque estén apagados. Si el
    // juego enciende uno a mitad de cuadro ($420C), sigue desde donde estaba su tabla (hay juegos que lo hacen).
    // Si no hay ninguno encendido, quedan sin dato pendiente: al encenderlos, la primera línea solo cuenta y lee la
    // tabla (Yoshi's Island lo usa en su pantalla del logo). Igual que bsnes.
    for (int i = 0; i < 8; i++)
    {
        hdmaFin[i] = false;
        hdmaHacer[i] = hdmaen != 0;
        if (!(hdmaen & (1 << i))) continue;
        dma[i].dirHdma = dma[i].dir;
        HdmaRecargar(i);
    }
    if (hdmaen) BusCiclo(18);
}

void HdmaLinea()
{
    if (!hdmaen) return;
    BusCiclo(18);
    for (int i = 0; i < 8; i++)
    {
        if (!(hdmaen & (1 << i)) || hdmaFin[i]) continue;
        Canal& c = dma[i];
        BusCiclo(8);
        if (hdmaHacer[i])
        {
            int modo = c.ctrl & 7;
            for (int k = 0; k < LARGO_PATRON[modo]; k++)
            {
                uint16_t regB = 0x2100 + (uint8_t)(c.dest + PATRON[modo][k]);
                uint32_t dirA = (c.ctrl & 0x40) ? ((uint32_t)c.bancoInd << 16 | c.tam++) : ((uint32_t)c.banco << 16 | c.dirHdma++);
                if (!(c.ctrl & 0x80)) BusEscribir(regB, BusLeer(dirA));
                else BusEscribir(dirA, BusLeer(regB));
                BusCiclo(8);
            }
        }
        c.lineas--;
        hdmaHacer[i] = c.lineas & 0x80;
        if (!(c.lineas & 0x7F)) HdmaRecargar(i);
    }
}

// ---------- interrupción por tiempo (H/V IRQ) ----------
int BusIrqModo() { return (nmitimen >> 4) & 3; }   // 0 nada, 1 por punto, 2 por línea, 3 punto y línea
int BusHtime() { return htime; }
int BusVtime() { return vtime; }
void BusDispararIrq() { irqBandera = true; irqLinea = true; }
void BusFinVblank() { rdnmi &= 0x7F; }
void BusMandoLinea() { if (mandoOcupado) mandoOcupado--; }

// ---------- al empezar y terminar cada cuadro ----------
void BusLeerMandos()
{
    // "Auto-join": la consola copia el estado de los mandos a 0x4218 al inicio del cuadro.
    for (int j = 0; j < 2; j++)
    {
        uint16_t r = 0;
        for (int i = 0; i < 12; i++)
            if (mando[j] & (1 << i)) r |= 0x8000 >> i;
        registroMando[j] = r;
    }
    if (autoLeerMando) { mandoLeido[0] = registroMando[0]; mandoLeido[1] = registroMando[1]; mandoOcupado = 3; }
}

bool BusNmiHabilitado() { return nmiHabilitado; }
void BusMarcarNmi() { rdnmi |= 0x80; if (nmiHabilitado) nmiPendiente = true; }
void BusHvbjoy(uint8_t v) { hvbjoy = v; }
void BusReiniciar()
{
    memset(wram, 0, sizeof wram);
    memset(dma, 0, sizeof dma);
    nmitimen = hvbjoy = rdnmi = 0;
    nmiHabilitado = autoLeerMando = false;
    mdr = 0;
    mult_a = divisor = dividendo = prodDiv = cociente = 0;
    mult_b = 0;
    wmaddL = wmaddM = wmaddH = 0;
    htime = vtime = 0x1FF;
    irqBandera = irqLinea = nmiPendiente = false;
    wrio = 0xFF; hdmaen = 0; memselRapido = false; mandoOcupado = 0;
    for (int i = 0; i < 8; i++)
    {
        Canal& c = dma[i];
        c.ctrl = c.dest = c.banco = c.bancoInd = c.lineas = c.libre = 0xFF;
        c.dir = c.tam = c.dirHdma = 0xFFFF;
        hdmaFin[i] = true;
    }
}

uint8_t BusInidisp() { extern uint8_t PpuInidisp(); return PpuInidisp(); }


// Registro de los primeros accesos a los puertos del APU (depuracion).
static int nLog;
void ApuLog(int escribe, int puerto, uint8_t v)
{
    extern bool apuLogOn;
    extern uint16_t CpuPc();
    if (!apuLogOn || nLog >= 400000) return;
    printf("%3d  CPU %04X  %s P%d = %02X\n", nLog, CpuPc(), escribe ? "escribe" : "lee    ", puerto, v);
    nLog++;
}

void BusEstado(Serial& s)
{
    s(wram); s(mdr); s(nmitimen); s(hvbjoy); s(rdnmi); s(nmiHabilitado); s(autoLeerMando);
    s(mult_a); s(mult_b); s(divisor); s(dividendo); s(prodDiv); s(cociente);
    s(mandoLeido); s(registroMando); s(serieMando); s(cerrojoMando); s(wmaddL); s(wmaddM); s(wmaddH);
    s(htime); s(vtime); s(irqBandera); s(wrio); s(hdmaen); s(mandoOcupado); s(memselRapido); s(irqLinea); s(nmiPendiente);
    s(dma); s(hdmaFin); s(hdmaHacer);
}
