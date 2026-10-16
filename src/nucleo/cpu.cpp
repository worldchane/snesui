// El procesador 65816 de la SNES por intérprete. El mismo código hace también el procesador del chip SA-1 (sa1cpu.cpp
// lo compila otra vez con su memoria, su reloj y sus interrupciones). Cada instrucción sigue el comportamiento del chip real.
// Bits de ancho: con M=1 el acumulador es de 8 bits y con M=0 de 16; con X=1 los índices son de 8 y con X=0 de 16.
// En modo emulación (E=1, al encender) M y X quedan forzados a 8 bits, como el 6502.
#include "interno.h"
#ifdef GSU_TRAZA
#include <stdio.h>
#include <stdlib.h>
#endif

Cpu cpu;

extern uint64_t ciclosMaestros;   // reloj maestro (snes.cpp)

// ---------- acceso a memoria (cada acceso cuesta ciclos del reloj maestro) ----------
static inline uint8_t rd8(uint32_t a) { BusCiclo(VelocidadAcceso(a)); return BusLeer(a); }
static inline void wr8(uint32_t a, uint8_t v) { BusCiclo(VelocidadAcceso(a)); BusEscribir(a, v); }
#ifndef CICLO_INTERNO
#define CICLO_INTERNO 6   // un ciclo interno del procesador de la SNES, en ciclos maestros (el SA-1 usa 2)
#endif
static inline void io() { BusCiclo(CICLO_INTERNO); }   // un ciclo interno (sin acceso a memoria)

// Ciclos internos de cada instrucción (los accesos a memoria se cuentan solos). Los casos que dependen del estado
// (página directa no alineada, índice que cruza de página, salto tomado) se suman en cada modo.
static const uint8_t CICLOS_INTERNOS[256] = {
    0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0,
    0, 0, 0, 2, 1, 1, 2, 0, 1, 0, 1, 1, 1, 0, 1, 0,
    1, 1, 1, 1, 0, 0, 1, 0, 2, 0, 1, 2, 0, 0, 1, 0,
    0, 0, 0, 2, 1, 1, 2, 0, 1, 0, 1, 1, 0, 0, 1, 0,
    2, 1, 0, 1, 2, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0,
    0, 0, 0, 2, 2, 1, 2, 0, 1, 0, 1, 1, 0, 0, 1, 0,
    3, 1, 1, 1, 0, 0, 1, 0, 2, 0, 1, 2, 0, 0, 1, 0,
    0, 0, 0, 2, 1, 1, 2, 0, 1, 0, 2, 1, 1, 0, 1, 0,
    0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 2, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0,
    0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 0, 0, 0, 0,
    0, 0, 0, 2, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 2, 0, 0, 1, 0,
    0, 0, 0, 2, 0, 1, 2, 0, 1, 0, 1, 2, 0, 0, 1, 0,
    0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 2, 0, 0, 1, 0,
    0, 0, 0, 2, 0, 1, 2, 0, 1, 0, 2, 1, 1, 0, 1, 0
};
static inline uint16_t rd16(uint32_t a) { uint8_t l = rd8(a); uint8_t h = rd8((a & 0xFF0000) | ((a + 1) & 0xFFFF)); return l | h << 8; }
static inline void wr16(uint32_t a, uint16_t v) { wr8(a, (uint8_t)v); wr8((a & 0xFF0000) | ((a + 1) & 0xFFFF), v >> 8); }
static inline uint16_t ptr16(uint16_t a) { uint8_t l = rd8(a); uint8_t h = rd8((uint16_t)(a + 1)); return l | h << 8; }   // puntero en banco 0

static inline uint8_t fetch8() { uint8_t v = rd8((cpu.pbr << 16) | cpu.pc); cpu.pc++; return v; }
static inline uint16_t fetch16() { uint8_t l = fetch8(); uint8_t h = fetch8(); return l | h << 8; }

static inline bool m8() { return cpu.e || (cpu.p & F_M); }
static inline bool x8() { return cpu.e || (cpu.p & F_X); }

static inline void znB(uint8_t v) { cpu.p = (cpu.p & ~(F_Z | F_N)) | (v ? 0 : F_Z) | (v & 0x80 ? F_N : 0); }
static inline void znW(uint16_t v) { cpu.p = (cpu.p & ~(F_Z | F_N)) | (v ? 0 : F_Z) | (v & 0x8000 ? F_N : 0); }
static inline void fijar(uint8_t bit, bool s) { if (s) cpu.p |= bit; else cpu.p &= ~bit; }

// ---------- pila ----------
static inline void push8(uint8_t v) { wr8(cpu.s, v); cpu.s--; if (cpu.e) cpu.s = 0x0100 | (cpu.s & 0xFF); }
static inline uint8_t pop8() { cpu.s++; if (cpu.e) cpu.s = 0x0100 | (cpu.s & 0xFF); return rd8(cpu.s); }
static inline void push16(uint16_t v) { push8(v >> 8); push8((uint8_t)v); }
static inline uint16_t pop16() { uint8_t l = pop8(); uint8_t h = pop8(); return l | h << 8; }

// ---------- modos de direccionamiento (devuelven la dirección de 24 bits y avanzan el PC) ----------
static inline void dpLento() { if (cpu.d & 0xFF) io(); }
static inline void cruce(uint16_t base, uint16_t idx) { if (!x8() || ((base + idx) & 0xFF00) != (base & 0xFF00)) io(); }
static inline uint32_t dp() { uint16_t o = fetch8(); dpLento(); return (uint16_t)(cpu.d + o); }
static inline uint32_t dpX() { uint16_t o = fetch8(); dpLento(); return (uint16_t)(cpu.d + o + cpu.x); }
static inline uint32_t dpY() { uint16_t o = fetch8(); dpLento(); return (uint16_t)(cpu.d + o + cpu.y); }
static inline uint32_t absol() { uint16_t o = fetch16(); return (cpu.dbr << 16) | o; }
static inline uint32_t absX() { uint16_t o = fetch16(); cruce(o, cpu.x); return (((uint32_t)cpu.dbr << 16) | o) + cpu.x; }
static inline uint32_t absY() { uint16_t o = fetch16(); cruce(o, cpu.y); return (((uint32_t)cpu.dbr << 16) | o) + cpu.y; }
static inline uint32_t largo() { uint16_t o = fetch16(); uint8_t b = fetch8(); return ((uint32_t)b << 16) | o; }
static inline uint32_t largoX() { return largo() + cpu.x; }
static inline uint32_t indX() { dpLento(); uint16_t p = (uint16_t)(cpu.d + fetch8() + cpu.x); return (cpu.dbr << 16) | ptr16(p); }
static inline uint32_t indY() { dpLento(); uint16_t p = (uint16_t)(cpu.d + fetch8()); uint16_t b = ptr16(p); cruce(b, cpu.y); return (((uint32_t)cpu.dbr << 16) | b) + cpu.y; }
static inline uint32_t ind() { dpLento(); uint16_t p = (uint16_t)(cpu.d + fetch8()); return (cpu.dbr << 16) | ptr16(p); }
static inline uint32_t indL() { dpLento(); uint16_t p = (uint16_t)(cpu.d + fetch8()); uint16_t o = ptr16(p); uint8_t b = rd8((uint16_t)(p + 2)); return ((uint32_t)b << 16) | o; }
static inline uint32_t indLY() { return indL() + cpu.y; }
static inline uint32_t pilaR() { uint16_t o = fetch8(); return (uint16_t)(cpu.s + o); }
static inline uint32_t pilaRY() { uint16_t p = (uint16_t)(cpu.s + fetch8()); return (((uint32_t)cpu.dbr << 16) | ptr16(p)) + cpu.y; }

// ---------- operaciones sobre A (8 o 16 bits según M) ----------
// Suma con acarreo, binaria o decimal (BCD con la bandera D), en 8 o 16 bits. 'resta' invierte el operando: SBC es una
// suma del complemento, y en decimal corrige hacia abajo. Sigue el algoritmo del chip (el mismo que usa bsnes).
static void Sumar(uint16_t dato, bool resta)
{
    uint32_t c = cpu.p & F_C;
    if (m8())
    {
        uint32_t a = cpu.a & 0xFF, d = (resta ? ~dato : dato) & 0xFF, r;
        if (cpu.p & F_D)
        {
            r = (a & 0x0F) + (d & 0x0F) + c;
            if (resta) { if (r <= 0x0F) r -= 0x06; } else if (r > 0x09) r += 0x06;
            c = r > 0x0F;
            r = (a & 0xF0) + (d & 0xF0) + (c << 4) + (r & 0x0F);
        }
        else r = a + d + c;
        fijar(F_V, ~(a ^ d) & (a ^ r) & 0x80);
        if (cpu.p & F_D) { if (resta) { if (r <= 0xFF) r -= 0x60; } else if (r > 0x9F) r += 0x60; }
        fijar(F_C, r > 0xFF);
        cpu.a = (cpu.a & 0xFF00) | (r & 0xFF);
        znB((uint8_t)r);
    }
    else
    {
        uint32_t a = cpu.a, d = (resta ? ~dato : dato) & 0xFFFF, r;
        if (cpu.p & F_D)
        {
            r = (a & 0x000F) + (d & 0x000F) + c;
            if (resta) { if (r <= 0x000F) r -= 0x0006; } else if (r > 0x0009) r += 0x0006;
            c = r > 0x000F;
            r = (a & 0x00F0) + (d & 0x00F0) + (c << 4) + (r & 0x000F);
            if (resta) { if (r <= 0x00FF) r -= 0x0060; } else if (r > 0x009F) r += 0x0060;
            c = r > 0x00FF;
            r = (a & 0x0F00) + (d & 0x0F00) + (c << 8) + (r & 0x00FF);
            if (resta) { if (r <= 0x0FFF) r -= 0x0600; } else if (r > 0x09FF) r += 0x0600;
            c = r > 0x0FFF;
            r = (a & 0xF000) + (d & 0xF000) + (c << 12) + (r & 0x0FFF);
        }
        else r = a + d + c;
        fijar(F_V, ~(a ^ d) & (a ^ r) & 0x8000);
        if (cpu.p & F_D) { if (resta) { if (r <= 0xFFFF) r -= 0x6000; } else if (r > 0x9FFF) r += 0x6000; }
        fijar(F_C, r > 0xFFFF);
        cpu.a = (uint16_t)r;
        znW(cpu.a);
    }
}
static void ADC(uint32_t a) { Sumar(m8() ? rd8(a) : rd16(a), false); }
static void SBC(uint32_t a) { Sumar(m8() ? rd8(a) : rd16(a), true); }
static void CMPr(uint16_t reg, uint32_t a, bool ocho)
{
    if (ocho) { uint8_t v = rd8(a); uint8_t r = (reg & 0xFF) - v; fijar(F_C, (reg & 0xFF) >= v); znB(r); }
    else { uint16_t v = rd16(a); uint16_t r = reg - v; fijar(F_C, reg >= v); znW(r); }
}
static void LDA(uint32_t a) { if (m8()) { cpu.a = (cpu.a & 0xFF00) | rd8(a); znB(cpu.a & 0xFF); } else { cpu.a = rd16(a); znW(cpu.a); } }
static void STA(uint32_t a) { if (m8()) wr8(a, (uint8_t)cpu.a); else wr16(a, cpu.a); }
static void AND_(uint32_t a) { if (m8()) { cpu.a = (cpu.a & 0xFF00) | ((cpu.a & rd8(a)) & 0xFF); znB(cpu.a & 0xFF); } else { cpu.a &= rd16(a); znW(cpu.a); } }
static void ORA_(uint32_t a) { if (m8()) { cpu.a = (cpu.a & 0xFF00) | ((cpu.a | rd8(a)) & 0xFF); znB(cpu.a & 0xFF); } else { cpu.a |= rd16(a); znW(cpu.a); } }
static void EOR_(uint32_t a) { if (m8()) { cpu.a = (cpu.a & 0xFF00) | ((cpu.a ^ rd8(a)) & 0xFF); znB(cpu.a & 0xFF); } else { cpu.a ^= rd16(a); znW(cpu.a); } }
static void BIT(uint32_t a)
{
    if (m8()) { uint8_t v = rd8(a); fijar(F_Z, !(v & cpu.a & 0xFF)); fijar(F_N, v & 0x80); fijar(F_V, v & 0x40); }
    else { uint16_t v = rd16(a); fijar(F_Z, !(v & cpu.a)); fijar(F_N, v & 0x8000); fijar(F_V, v & 0x4000); }
}
static void LDX(uint32_t a) { if (x8()) { cpu.x = rd8(a); znB(cpu.x & 0xFF); } else { cpu.x = rd16(a); znW(cpu.x); } }
static void LDY(uint32_t a) { if (x8()) { cpu.y = rd8(a); znB(cpu.y & 0xFF); } else { cpu.y = rd16(a); znW(cpu.y); } }

// Lectura-modifica-escritura (ASL/LSR/ROL/ROR/INC/DEC sobre memoria).
static void RMW(uint32_t a, int op)
{
    if (m8())
    {
        uint8_t v = rd8(a), c = cpu.p & F_C;
        switch (op) { case 0: fijar(F_C, v & 0x80); v <<= 1; break; case 1: fijar(F_C, v & 1); v >>= 1; break;
                      case 2: { uint8_t n = v << 1 | c; fijar(F_C, v & 0x80); v = n; break; }
                      case 3: { uint8_t n = v >> 1 | (c ? 0x80 : 0); fijar(F_C, v & 1); v = n; break; }
                      case 4: v++; break; case 5: v--; break; }
        znB(v); wr8(a, v);
    }
    else
    {
        uint16_t v = rd16(a), c = cpu.p & F_C;
        switch (op) { case 0: fijar(F_C, v & 0x8000); v <<= 1; break; case 1: fijar(F_C, v & 1); v >>= 1; break;
                      case 2: { uint16_t n = v << 1 | c; fijar(F_C, v & 0x8000); v = n; break; }
                      case 3: { uint16_t n = v >> 1 | (c ? 0x8000 : 0); fijar(F_C, v & 1); v = n; break; }
                      case 4: v++; break; case 5: v--; break; }
        znW(v); wr16(a, v);
    }
}
static void RMWa(int op)   // sobre el acumulador
{
    if (m8())
    {
        uint8_t v = cpu.a & 0xFF, c = cpu.p & F_C;
        switch (op) { case 0: fijar(F_C, v & 0x80); v <<= 1; break; case 1: fijar(F_C, v & 1); v >>= 1; break;
                      case 2: { uint8_t n = v << 1 | c; fijar(F_C, v & 0x80); v = n; break; }
                      case 3: { uint8_t n = v >> 1 | (c ? 0x80 : 0); fijar(F_C, v & 1); v = n; break; } }
        znB(v); cpu.a = (cpu.a & 0xFF00) | v;
    }
    else
    {
        uint16_t v = cpu.a, c = cpu.p & F_C;
        switch (op) { case 0: fijar(F_C, v & 0x8000); v <<= 1; break; case 1: fijar(F_C, v & 1); v >>= 1; break;
                      case 2: { uint16_t n = v << 1 | c; fijar(F_C, v & 0x8000); v = n; break; }
                      case 3: { uint16_t n = v >> 1 | (c ? 0x8000 : 0); fijar(F_C, v & 1); v = n; break; } }
        znW(v); cpu.a = v;
    }
}

static void rama(bool cond)
{
    int8_t o = (int8_t)fetch8();
    if (!cond) return;
    io();
    uint16_t nuevo = cpu.pc + o;
    if (cpu.e && (nuevo & 0xFF00) != (cpu.pc & 0xFF00)) io();
    cpu.pc = nuevo;
}

static void INTERRUP(uint16_t vector)
{
    if (!cpu.e) push8(cpu.pbr);
    push16(cpu.pc);
    push8(cpu.e ? (cpu.p & ~0x10) | 0x20 : cpu.p);
    cpu.p |= F_I; cpu.p &= ~F_D;
    cpu.pbr = 0;
    cpu.pc = rd16(vector);
}

void CpuReiniciar()
{
    memset(&cpu, 0, sizeof cpu);
    cpu.e = true;
    cpu.p = F_M | F_X | F_I;
    cpu.s = 0x01FF;
    cpu.d = 0;
    cpu.dbr = cpu.pbr = 0;
    cpu.pc = BusLeer(0xFFFC) | BusLeer(0xFFFD) << 8;
}

uint32_t diagNmi, diagIrq;

void CpuInterrupcion(bool nmi)
{
    if (nmi) diagNmi++;
    else diagIrq++;
    cpu.esperando = false;
    io(); io();
    INTERRUP(nmi ? (cpu.e ? 0xFFFA : 0xFFEA) : (cpu.e ? 0xFFFE : 0xFFEE));
}

void CpuCorrerHasta(uint64_t hasta)
{
    while (ciclosMaestros < hasta)
    {
        // Interrupciones pendientes: la NMI siempre; la IRQ si no está bloqueada (bandera I). WAI despierta con
        // cualquiera de las dos, aunque la IRQ esté bloqueada.
        if (nmiPendiente) { nmiPendiente = false; CpuInterrupcion(true); }
        else if ((irqLinea || irqGsu || irqSa1) && !(cpu.p & F_I)) CpuInterrupcion(false);
        else if ((irqLinea || irqGsu || irqSa1) && cpu.esperando) cpu.esperando = false;
        if (cpu.parado || cpu.esperando) { ciclosMaestros = hasta; return; }
        if (irqReprogramada) return;   // el juego cambió la IRQ por tiempo: el cuadro recalcula cuándo salta
#if defined(GSU_TRAZA) && !defined(CPU_SA1)
        {
            // Herramientas de PC: las últimas instrucciones del procesador; CPUBRK=1 las imprime al primer BRK.
            struct T { uint8_t pbr; uint16_t pc, a, x, y, s; uint8_t p, op; };
            static T anillo[256];
            static unsigned n;
            static int brk = getenv("CPUBRK") ? 1 : 0;
            uint8_t opv = BusLeer((uint32_t)cpu.pbr << 16 | cpu.pc);
            anillo[n++ & 255] = {cpu.pbr, cpu.pc, cpu.a, cpu.x, cpu.y, cpu.s, cpu.p, opv};
            // CPULOG=desde,hasta: cada instrucción entre esas líneas, en los cuadros de depuración.
            static int lDesde = -2, lHasta;
            if (lDesde == -2) { lDesde = -1; if (getenv("CPULOG")) sscanf(getenv("CPULOG"), "%d,%d", &lDesde, &lHasta); }
            extern int depurCuadro;
            if (lDesde >= 0 && depurCuadro)
            {
                void SnesContadores(int*, int*);
                int h, l;
                SnesContadores(&h, &l);
                static uint64_t antes;
                if (l >= lDesde && l <= lHasta)
                    printf("  [%3d] %02X:%04X op=%02X A=%04X X=%04X Y=%04X S=%04X P=%02X  (+%d)\n", l, cpu.pbr, cpu.pc, opv, cpu.a, cpu.x, cpu.y, cpu.s, cpu.p, (int)(ciclosMaestros - antes));
                antes = ciclosMaestros;
            }
            if (brk == 1 && opv == 0x00)
            {
                brk = 2;
                for (unsigned k = n - 256; k != n; k++)
                {
                    const T& t = anillo[k & 255];
                    printf("  %02X:%04X op=%02X A=%04X X=%04X Y=%04X S=%04X P=%02X\n", t.pbr, t.pc, t.op, t.a, t.x, t.y, t.s, t.p);
                }
            }
        }
#endif
        uint8_t op = fetch8();
        switch (op)
        {
        // ORA
        case 0x01: ORA_(indX()); break; case 0x03: ORA_(pilaR()); break; case 0x05: ORA_(dp()); break;
        case 0x07: ORA_(indL()); break; case 0x09: if (m8()) { cpu.a = (cpu.a & 0xFF00) | ((cpu.a | fetch8()) & 0xFF); znB(cpu.a); } else { cpu.a |= fetch16(); znW(cpu.a); } break;
        case 0x0D: ORA_(absol()); break; case 0x0F: ORA_(largo()); break; case 0x11: ORA_(indY()); break;
        case 0x12: ORA_(ind()); break; case 0x13: ORA_(pilaRY()); break; case 0x15: ORA_(dpX()); break;
        case 0x17: ORA_(indLY()); break; case 0x19: ORA_(absY()); break; case 0x1D: ORA_(absX()); break; case 0x1F: ORA_(largoX()); break;
        // AND
        case 0x21: AND_(indX()); break; case 0x23: AND_(pilaR()); break; case 0x25: AND_(dp()); break;
        case 0x27: AND_(indL()); break; case 0x29: if (m8()) { cpu.a = (cpu.a & 0xFF00) | ((cpu.a & fetch8()) & 0xFF); znB(cpu.a); } else { cpu.a &= fetch16(); znW(cpu.a); } break;
        case 0x2D: AND_(absol()); break; case 0x2F: AND_(largo()); break; case 0x31: AND_(indY()); break;
        case 0x32: AND_(ind()); break; case 0x33: AND_(pilaRY()); break; case 0x35: AND_(dpX()); break;
        case 0x37: AND_(indLY()); break; case 0x39: AND_(absY()); break; case 0x3D: AND_(absX()); break; case 0x3F: AND_(largoX()); break;
        // EOR
        case 0x41: EOR_(indX()); break; case 0x43: EOR_(pilaR()); break; case 0x45: EOR_(dp()); break;
        case 0x47: EOR_(indL()); break; case 0x49: if (m8()) { cpu.a = (cpu.a & 0xFF00) | ((cpu.a ^ fetch8()) & 0xFF); znB(cpu.a); } else { cpu.a ^= fetch16(); znW(cpu.a); } break;
        case 0x4D: EOR_(absol()); break; case 0x4F: EOR_(largo()); break; case 0x51: EOR_(indY()); break;
        case 0x52: EOR_(ind()); break; case 0x53: EOR_(pilaRY()); break; case 0x55: EOR_(dpX()); break;
        case 0x57: EOR_(indLY()); break; case 0x59: EOR_(absY()); break; case 0x5D: EOR_(absX()); break; case 0x5F: EOR_(largoX()); break;
        // ADC
        case 0x61: ADC(indX()); break; case 0x63: ADC(pilaR()); break; case 0x65: ADC(dp()); break;
        case 0x67: ADC(indL()); break; case 0x69: if (m8()) { uint32_t a = (cpu.pbr << 16) | cpu.pc; cpu.pc++; ADC(a); } else { uint32_t a = (cpu.pbr << 16) | cpu.pc; cpu.pc += 2; ADC(a); } break;
        case 0x6D: ADC(absol()); break; case 0x6F: ADC(largo()); break; case 0x71: ADC(indY()); break;
        case 0x72: ADC(ind()); break; case 0x73: ADC(pilaRY()); break; case 0x75: ADC(dpX()); break;
        case 0x77: ADC(indLY()); break; case 0x79: ADC(absY()); break; case 0x7D: ADC(absX()); break; case 0x7F: ADC(largoX()); break;
        // SBC
        case 0xE1: SBC(indX()); break; case 0xE3: SBC(pilaR()); break; case 0xE5: SBC(dp()); break;
        case 0xE7: SBC(indL()); break; case 0xE9: if (m8()) { uint32_t a = (cpu.pbr << 16) | cpu.pc; cpu.pc++; SBC(a); } else { uint32_t a = (cpu.pbr << 16) | cpu.pc; cpu.pc += 2; SBC(a); } break;
        case 0xED: SBC(absol()); break; case 0xEF: SBC(largo()); break; case 0xF1: SBC(indY()); break;
        case 0xF2: SBC(ind()); break; case 0xF3: SBC(pilaRY()); break; case 0xF5: SBC(dpX()); break;
        case 0xF7: SBC(indLY()); break; case 0xF9: SBC(absY()); break; case 0xFD: SBC(absX()); break; case 0xFF: SBC(largoX()); break;
        // CMP
        case 0xC1: CMPr(cpu.a, indX(), m8()); break; case 0xC3: CMPr(cpu.a, pilaR(), m8()); break; case 0xC5: CMPr(cpu.a, dp(), m8()); break;
        case 0xC7: CMPr(cpu.a, indL(), m8()); break; case 0xC9: { uint32_t a = (cpu.pbr << 16) | cpu.pc; cpu.pc += m8() ? 1 : 2; CMPr(cpu.a, a, m8()); break; }
        case 0xCD: CMPr(cpu.a, absol(), m8()); break; case 0xCF: CMPr(cpu.a, largo(), m8()); break; case 0xD1: CMPr(cpu.a, indY(), m8()); break;
        case 0xD2: CMPr(cpu.a, ind(), m8()); break; case 0xD3: CMPr(cpu.a, pilaRY(), m8()); break; case 0xD5: CMPr(cpu.a, dpX(), m8()); break;
        case 0xD7: CMPr(cpu.a, indLY(), m8()); break; case 0xD9: CMPr(cpu.a, absY(), m8()); break; case 0xDD: CMPr(cpu.a, absX(), m8()); break; case 0xDF: CMPr(cpu.a, largoX(), m8()); break;
        case 0xE0: { uint32_t a = (cpu.pbr << 16) | cpu.pc; cpu.pc += x8() ? 1 : 2; CMPr(cpu.x, a, x8()); break; }   // CPX #
        case 0xE4: CMPr(cpu.x, dp(), x8()); break; case 0xEC: CMPr(cpu.x, absol(), x8()); break;
        case 0xC0: { uint32_t a = (cpu.pbr << 16) | cpu.pc; cpu.pc += x8() ? 1 : 2; CMPr(cpu.y, a, x8()); break; }   // CPY #
        case 0xC4: CMPr(cpu.y, dp(), x8()); break; case 0xCC: CMPr(cpu.y, absol(), x8()); break;
        // LDA
        case 0xA1: LDA(indX()); break; case 0xA3: LDA(pilaR()); break; case 0xA5: LDA(dp()); break;
        case 0xA7: LDA(indL()); break; case 0xA9: { uint32_t a = (cpu.pbr << 16) | cpu.pc; cpu.pc += m8() ? 1 : 2; LDA(a); break; }
        case 0xAD: LDA(absol()); break; case 0xAF: LDA(largo()); break; case 0xB1: LDA(indY()); break;
        case 0xB2: LDA(ind()); break; case 0xB3: LDA(pilaRY()); break; case 0xB5: LDA(dpX()); break;
        case 0xB7: LDA(indLY()); break; case 0xB9: LDA(absY()); break; case 0xBD: LDA(absX()); break; case 0xBF: LDA(largoX()); break;
        // STA
        case 0x81: STA(indX()); break; case 0x83: STA(pilaR()); break; case 0x85: STA(dp()); break;
        case 0x87: STA(indL()); break; case 0x8D: STA(absol()); break; case 0x8F: STA(largo()); break;
        case 0x91: STA(indY()); break; case 0x92: STA(ind()); break; case 0x93: STA(pilaRY()); break;
        case 0x95: STA(dpX()); break; case 0x97: STA(indLY()); break; case 0x99: STA(absY()); break;
        case 0x9D: STA(absX()); break; case 0x9F: STA(largoX()); break;
        // LDX / LDY
        case 0xA2: { uint32_t a = (cpu.pbr << 16) | cpu.pc; cpu.pc += x8() ? 1 : 2; LDX(a); break; }
        case 0xA6: LDX(dp()); break; case 0xAE: LDX(absol()); break; case 0xB6: LDX(dpY()); break; case 0xBE: LDX(absY()); break;
        case 0xA0: { uint32_t a = (cpu.pbr << 16) | cpu.pc; cpu.pc += x8() ? 1 : 2; LDY(a); break; }
        case 0xA4: LDY(dp()); break; case 0xAC: LDY(absol()); break; case 0xB4: LDY(dpX()); break; case 0xBC: LDY(absX()); break;
        // STX / STY / STZ
        case 0x86: if (x8()) wr8(dp(), (uint8_t)cpu.x); else wr16(dp(), cpu.x); break;
        case 0x8E: if (x8()) wr8(absol(), (uint8_t)cpu.x); else wr16(absol(), cpu.x); break;
        case 0x96: if (x8()) wr8(dpY(), (uint8_t)cpu.x); else wr16(dpY(), cpu.x); break;
        case 0x84: if (x8()) wr8(dp(), (uint8_t)cpu.y); else wr16(dp(), cpu.y); break;
        case 0x8C: if (x8()) wr8(absol(), (uint8_t)cpu.y); else wr16(absol(), cpu.y); break;
        case 0x94: if (x8()) wr8(dpX(), (uint8_t)cpu.y); else wr16(dpX(), cpu.y); break;
        case 0x64: if (m8()) wr8(dp(), 0); else wr16(dp(), 0); break;
        case 0x74: if (m8()) wr8(dpX(), 0); else wr16(dpX(), 0); break;
        case 0x9C: if (m8()) wr8(absol(), 0); else wr16(absol(), 0); break;
        case 0x9E: if (m8()) wr8(absX(), 0); else wr16(absX(), 0); break;
        // ASL/LSR/ROL/ROR/INC/DEC memoria y acumulador
        case 0x06: RMW(dp(), 0); break; case 0x0E: RMW(absol(), 0); break; case 0x16: RMW(dpX(), 0); break; case 0x1E: RMW(absX(), 0); break; case 0x0A: RMWa(0); break;
        case 0x46: RMW(dp(), 1); break; case 0x4E: RMW(absol(), 1); break; case 0x56: RMW(dpX(), 1); break; case 0x5E: RMW(absX(), 1); break; case 0x4A: RMWa(1); break;
        case 0x26: RMW(dp(), 2); break; case 0x2E: RMW(absol(), 2); break; case 0x36: RMW(dpX(), 2); break; case 0x3E: RMW(absX(), 2); break; case 0x2A: RMWa(2); break;
        case 0x66: RMW(dp(), 3); break; case 0x6E: RMW(absol(), 3); break; case 0x76: RMW(dpX(), 3); break; case 0x7E: RMW(absX(), 3); break; case 0x6A: RMWa(3); break;
        case 0xE6: RMW(dp(), 4); break; case 0xEE: RMW(absol(), 4); break; case 0xF6: RMW(dpX(), 4); break; case 0xFE: RMW(absX(), 4); break;
        case 0xC6: RMW(dp(), 5); break; case 0xCE: RMW(absol(), 5); break; case 0xD6: RMW(dpX(), 5); break; case 0xDE: RMW(absX(), 5); break;
        case 0x1A: if (m8()) { cpu.a = (cpu.a & 0xFF00) | ((cpu.a + 1) & 0xFF); znB(cpu.a); } else { cpu.a++; znW(cpu.a); } break;   // INC A
        case 0x3A: if (m8()) { cpu.a = (cpu.a & 0xFF00) | ((cpu.a - 1) & 0xFF); znB(cpu.a); } else { cpu.a--; znW(cpu.a); } break;   // DEC A
        // INX/INY/DEX/DEY
        case 0xE8: if (x8()) { cpu.x = (cpu.x + 1) & 0xFF; znB(cpu.x); } else { cpu.x++; znW(cpu.x); } break;
        case 0xC8: if (x8()) { cpu.y = (cpu.y + 1) & 0xFF; znB(cpu.y); } else { cpu.y++; znW(cpu.y); } break;
        case 0xCA: if (x8()) { cpu.x = (cpu.x - 1) & 0xFF; znB(cpu.x); } else { cpu.x--; znW(cpu.x); } break;
        case 0x88: if (x8()) { cpu.y = (cpu.y - 1) & 0xFF; znB(cpu.y); } else { cpu.y--; znW(cpu.y); } break;
        // BIT
        case 0x24: BIT(dp()); break; case 0x2C: BIT(absol()); break; case 0x34: BIT(dpX()); break; case 0x3C: BIT(absX()); break;
        case 0x89: { uint16_t v; if (m8()) { v = fetch8(); fijar(F_Z, !(v & (cpu.a & 0xFF))); } else { v = fetch16(); fijar(F_Z, !(v & cpu.a)); } break; }   // BIT # (solo Z)
        // TRB/TSB
        case 0x14: if (m8()) { uint32_t a = dp(); uint8_t v = rd8(a); fijar(F_Z, !(v & cpu.a & 0xFF)); wr8(a, v & ~(cpu.a & 0xFF)); } else { uint32_t a = dp(); uint16_t v = rd16(a); fijar(F_Z, !(v & cpu.a)); wr16(a, v & ~cpu.a); } break;
        case 0x1C: if (m8()) { uint32_t a = absol(); uint8_t v = rd8(a); fijar(F_Z, !(v & cpu.a & 0xFF)); wr8(a, v & ~(cpu.a & 0xFF)); } else { uint32_t a = absol(); uint16_t v = rd16(a); fijar(F_Z, !(v & cpu.a)); wr16(a, v & ~cpu.a); } break;
        case 0x04: if (m8()) { uint32_t a = dp(); uint8_t v = rd8(a); fijar(F_Z, !(v & cpu.a & 0xFF)); wr8(a, v | (cpu.a & 0xFF)); } else { uint32_t a = dp(); uint16_t v = rd16(a); fijar(F_Z, !(v & cpu.a)); wr16(a, v | cpu.a); } break;
        case 0x0C: if (m8()) { uint32_t a = absol(); uint8_t v = rd8(a); fijar(F_Z, !(v & cpu.a & 0xFF)); wr8(a, v | (cpu.a & 0xFF)); } else { uint32_t a = absol(); uint16_t v = rd16(a); fijar(F_Z, !(v & cpu.a)); wr16(a, v | cpu.a); } break;
        // ramas
        case 0x90: rama(!(cpu.p & F_C)); break; case 0xB0: rama(cpu.p & F_C); break;
        case 0xD0: rama(!(cpu.p & F_Z)); break; case 0xF0: rama(cpu.p & F_Z); break;
        case 0x10: rama(!(cpu.p & F_N)); break; case 0x30: rama(cpu.p & F_N); break;
        case 0x50: rama(!(cpu.p & F_V)); break; case 0x70: rama(cpu.p & F_V); break;
        case 0x80: rama(true); break;
        case 0x82: { int16_t o = (int16_t)fetch16(); cpu.pc += o; break; }   // BRL
        // saltos
        case 0x4C: cpu.pc = fetch16(); break;                                                       // JMP a
        case 0x5C: { uint16_t o = fetch16(); cpu.pbr = fetch8(); cpu.pc = o; break; }               // JML al
        case 0x6C: { uint16_t p = fetch16(); cpu.pc = rd8(p) | rd8((uint16_t)(p + 1)) << 8; break; } // JMP (a)
        case 0x7C: { uint16_t p = fetch16() + cpu.x; cpu.pc = rd8((cpu.pbr << 16) | p) | rd8((cpu.pbr << 16) | (uint16_t)(p + 1)) << 8; break; }   // JMP (a,X)
        case 0xDC: { uint16_t p = fetch16(); cpu.pc = rd8(p) | rd8((uint16_t)(p + 1)) << 8; cpu.pbr = rd8((uint16_t)(p + 2)); break; }   // JML [a]
        case 0x20: { uint16_t d = fetch16(); push16(cpu.pc - 1); cpu.pc = d; break; }               // JSR a
        case 0x22: { uint16_t d = fetch16(); uint8_t b = fetch8(); push8(cpu.pbr); push16(cpu.pc - 1); cpu.pbr = b; cpu.pc = d; break; }   // JSL al
        case 0xFC: { uint16_t d = fetch16(); push16(cpu.pc - 1); uint16_t p = d + cpu.x; cpu.pc = rd8((cpu.pbr << 16) | p) | rd8((cpu.pbr << 16) | (uint16_t)(p + 1)) << 8; break; }   // JSR (a,X)
        case 0x60: cpu.pc = pop16() + 1; break;                                                     // RTS
        case 0x6B: cpu.pc = pop16() + 1; cpu.pbr = pop8(); break;                                   // RTL
        case 0x40: { cpu.p = pop8(); cpu.pc = pop16(); if (!cpu.e) cpu.pbr = pop8(); if (x8()) { cpu.x &= 0xFF; cpu.y &= 0xFF; } break; }   // RTI
        // pila
        case 0x48: if (m8()) push8((uint8_t)cpu.a); else push16(cpu.a); break;   // PHA
        case 0xDA: if (x8()) push8((uint8_t)cpu.x); else push16(cpu.x); break;   // PHX
        case 0x5A: if (x8()) push8((uint8_t)cpu.y); else push16(cpu.y); break;   // PHY
        case 0x08: push8(cpu.p); break;        // PHP
        case 0x8B: push8(cpu.dbr); break;      // PHB
        case 0x0B: push16(cpu.d); break;       // PHD
        case 0x4B: push8(cpu.pbr); break;      // PHK
        case 0x68: if (m8()) { cpu.a = (cpu.a & 0xFF00) | pop8(); znB(cpu.a); } else { cpu.a = pop16(); znW(cpu.a); } break;   // PLA
        case 0xFA: if (x8()) { cpu.x = pop8(); znB(cpu.x); } else { cpu.x = pop16(); znW(cpu.x); } break;   // PLX
        case 0x7A: if (x8()) { cpu.y = pop8(); znB(cpu.y); } else { cpu.y = pop16(); znW(cpu.y); } break;   // PLY
        case 0x28: cpu.p = pop8(); if (cpu.e) cpu.p |= F_M | F_X; if (x8()) { cpu.x &= 0xFF; cpu.y &= 0xFF; } break;   // PLP
        case 0xAB: cpu.dbr = pop8(); znB(cpu.dbr); break;   // PLB
        case 0x2B: cpu.d = pop16(); znW(cpu.d); break;      // PLD
        case 0xF4: push16(fetch16()); break;               // PEA
        case 0xD4: push16(ptr16((uint16_t)(cpu.d + fetch8()))); break;   // PEI
        case 0x62: { int16_t o = (int16_t)fetch16(); push16(cpu.pc + o); break; }   // PER
        // transferencias
        case 0xAA: if (x8()) { cpu.x = cpu.a & 0xFF; znB(cpu.x); } else { cpu.x = cpu.a; znW(cpu.x); } break;   // TAX
        case 0xA8: if (x8()) { cpu.y = cpu.a & 0xFF; znB(cpu.y); } else { cpu.y = cpu.a; znW(cpu.y); } break;   // TAY
        case 0x8A: if (m8()) { cpu.a = (cpu.a & 0xFF00) | (cpu.x & 0xFF); znB(cpu.a); } else { cpu.a = cpu.x; znW(cpu.a); } break;   // TXA
        case 0x98: if (m8()) { cpu.a = (cpu.a & 0xFF00) | (cpu.y & 0xFF); znB(cpu.a); } else { cpu.a = cpu.y; znW(cpu.a); } break;   // TYA
        case 0xBA: if (x8()) { cpu.x = cpu.s & 0xFF; znB(cpu.x); } else { cpu.x = cpu.s; znW(cpu.x); } break;   // TSX
        case 0x9A: cpu.s = cpu.e ? 0x0100 | (cpu.x & 0xFF) : cpu.x; break;   // TXS
        case 0x9B: if (x8()) { cpu.y = cpu.x & 0xFF; znB(cpu.y); } else { cpu.y = cpu.x; znW(cpu.y); } break;   // TXY
        case 0xBB: if (x8()) { cpu.x = cpu.y & 0xFF; znB(cpu.x); } else { cpu.x = cpu.y; znW(cpu.x); } break;   // TYX
        case 0x5B: cpu.d = cpu.a; znW(cpu.d); break;   // TCD
        case 0x7B: cpu.a = cpu.d; znW(cpu.a); break;   // TDC
        case 0x1B: cpu.s = cpu.e ? 0x0100 | (cpu.a & 0xFF) : cpu.a; break;   // TCS
        case 0x3B: cpu.a = cpu.s; znW(cpu.a); break;   // TSC
        case 0xEB: { uint8_t l = cpu.a & 0xFF, h = cpu.a >> 8; cpu.a = l << 8 | h; znB(h); break; }   // XBA
        // banderas
        case 0x18: cpu.p &= ~F_C; break; case 0x38: cpu.p |= F_C; break;
        case 0x58: cpu.p &= ~F_I; break; case 0x78: cpu.p |= F_I; break;
        case 0xB8: cpu.p &= ~F_V; break;
        case 0xD8: cpu.p &= ~F_D; break; case 0xF8: cpu.p |= F_D; break;
        case 0xC2: cpu.p &= ~fetch8(); if (cpu.e) cpu.p |= F_M | F_X; break;   // REP
        case 0xE2: cpu.p |= fetch8(); if (x8()) { cpu.x &= 0xFF; cpu.y &= 0xFF; } break;   // SEP
        case 0xFB: { bool c = cpu.p & F_C; fijar(F_C, cpu.e); cpu.e = c; if (cpu.e) { cpu.p |= F_M | F_X; cpu.x &= 0xFF; cpu.y &= 0xFF; cpu.s = 0x0100 | (cpu.s & 0xFF); } break; }   // XCE
        // movimiento de bloques
        case 0x54: { uint8_t db = fetch8(); uint8_t sb = fetch8(); cpu.dbr = db; wr8((db << 16) | cpu.y, rd8((sb << 16) | cpu.x)); cpu.x++; cpu.y++; if (x8()) { cpu.x &= 0xFF; cpu.y &= 0xFF; } if (cpu.a-- != 0) cpu.pc -= 3; break; }   // MVN
        case 0x44: { uint8_t db = fetch8(); uint8_t sb = fetch8(); cpu.dbr = db; wr8((db << 16) | cpu.y, rd8((sb << 16) | cpu.x)); cpu.x--; cpu.y--; if (x8()) { cpu.x &= 0xFF; cpu.y &= 0xFF; } if (cpu.a-- != 0) cpu.pc -= 3; break; }   // MVP
        // control
        case 0xEA: break;                 // NOP
        case 0x42: fetch8(); break;       // WDM
        case 0xDB: cpu.parado = true; break;   // STP
        case 0xCB: cpu.esperando = true; break;   // WAI
        case 0x00: fetch8(); INTERRUP(cpu.e ? 0xFFFE : 0xFFE6); break;   // BRK
        case 0x02: fetch8(); INTERRUP(cpu.e ? 0xFFF4 : 0xFFE4); break;   // COP
        default: break;   // no debería pasar: todos los códigos están cubiertos
        }
        BusCiclo(CICLOS_INTERNOS[op] * 6);
        cpu.ciclos++;
    }
}

uint16_t CpuPc() { return cpu.pc; }
bool apuLogOn = false;

void CpuEstado(Serial& s) { s(cpu); }
