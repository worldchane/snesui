// El chip SPC7110 de algunos cartuchos: maneja una rom grande (el programa y, aparte, hasta 8 MB de datos), descomprime
// gráficos, lee la rom de datos por un puerto con incrementos programables y multiplica y divide. Algunos traen además
// un reloj Epson RTC-4513 (en $4840-$4842).
// Mapa: registros en 00-3F:4800-483F (y 50:xxxx = $4800, 58:xxxx = $4808); RAM a batería en 00-3F:6000-7FFF; la rom
// en 00-3F:8000-FFFF y C0-FF:0000-FFFF, de a 1 MB: el primero es el programa y los otros tres los eligen $4831-$4833.
// La descompresión es un codificador aritmético con modelo de contexto (lo describieron neviksti y talarubi).
#include <string.h>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#include "interno.h"
#include "rom.h"

extern uint64_t ciclosMaestros;

bool spc7110Presente, spc7110Reloj;
uint32_t spc7110RamTam;   // la RAM del juego; en la SRAM, después de ella, van 16 bytes del reloj

// Segundos desde 1970 (en la consola con gettimeofday, que el programa ya usa: no suma importaciones).
static uint64_t Ahora()
{
#ifdef _WIN32
    return Ahora();
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t)tv.tv_sec;
#endif
}

static const uint32_t PROGRAMA = 0x100000;   // el primer MB de la rom es el programa; lo demás, datos
// Una variante de 7 MB (de una traducción) agrega 1 MB de expansión al final, visto en 40-4F:0000-FFFF.
static const uint32_t EXPANSION = 0x600000;
static inline uint32_t TamDatos()
{
    if (romTam == 0x700000) return EXPANSION - PROGRAMA;
    return romTam > PROGRAMA ? romTam - PROGRAMA : 0;
}

// ---- descompresor ----
struct Dcu
{
    struct { uint8_t prediccion, cambio; } contexto[5][15];
    uint32_t bpp, posicion, bits;
    uint16_t rango, entrada;
    uint8_t salida;
    uint64_t pixeles, colores;   // colores: lista de los usados más recientemente (de a 4 bits)
    uint32_t resultado;
};

static struct
{
    uint8_t r[0x40];   // registros $4800-$483F tal cual los ve la SNES
    bool dcuListo;
    uint8_t dcuModo;
    uint32_t dcuDir, dcuPos;
    uint8_t dcuTile[32];
    Dcu d;
} s;

static uint8_t LeerDatos(uint32_t dir)
{
    uint32_t mb = 1u << (s.r[0x34] & 3);
    if ((s.r[0x34] & 3) != 3 && (dir & 0x400000)) return 0;
    uint32_t datos = TamDatos();
    if (!datos) return 0;
    return romDatos[PROGRAMA + (dir & (0x100000 * mb - 1)) % datos];
}

static uint8_t LeerRom(uint32_t a)   // a: 0x000000-0x3FFFFF (banco & 0x3F, desplazamiento)
{
    uint32_t parte = a >> 20, o = a & 0xFFFFF;
    if (parte == 0) return romDatos[o % (romTam < PROGRAMA ? romTam : PROGRAMA)];
    if (parte == 1 && (s.r[0x34] & 4)) return romDatos[o % (romTam < PROGRAMA ? romTam : PROGRAMA)];
    return LeerDatos(o | 0x100000 * (s.r[0x30 + parte] & 7));
}

// Estados del modelo de probabilidad: probabilidad del símbolo más probable y estado siguiente (si sale ese o el otro).
static const uint8_t EVOLUCION[53][3] = {
    {0x5a, 1, 1},   {0x25, 2, 6},   {0x11, 3, 8},   {0x08, 4, 10},  {0x03, 5, 12},  {0x01, 5, 15},
    {0x5a, 7, 7},   {0x3f, 8, 19},  {0x2c, 9, 21},  {0x20, 10, 22}, {0x17, 11, 23}, {0x11, 12, 25},
    {0x0c, 13, 26}, {0x09, 14, 28}, {0x07, 15, 29}, {0x05, 16, 31}, {0x04, 17, 32}, {0x03, 18, 34},
    {0x02, 5, 35},
    {0x5a, 20, 20}, {0x48, 21, 39}, {0x3a, 22, 40}, {0x2e, 23, 42}, {0x26, 24, 44}, {0x1f, 25, 45},
    {0x19, 26, 46}, {0x15, 27, 25}, {0x11, 28, 26}, {0x0e, 29, 26}, {0x0b, 30, 27}, {0x09, 31, 28},
    {0x08, 32, 29}, {0x07, 33, 30}, {0x05, 34, 31}, {0x04, 35, 33}, {0x04, 36, 33}, {0x03, 37, 34},
    {0x02, 38, 35}, {0x02, 5, 36},
    {0x58, 40, 39}, {0x4d, 41, 47}, {0x43, 42, 48}, {0x3b, 43, 49}, {0x34, 44, 50}, {0x2e, 45, 51},
    {0x29, 46, 44}, {0x25, 24, 45},
    {0x56, 48, 47}, {0x4f, 49, 47}, {0x47, 50, 48}, {0x41, 51, 49}, {0x3c, 52, 50}, {0x37, 43, 51}};

// Pixeles empaquetados de a 'bits' (big endian) a planos: los bits impares quedan en la mitad baja y los pares arriba.
static uint32_t Desentrelazar(uint64_t v, unsigned bits)
{
    v &= (1ull << bits) - 1;
    v = 0x5555555555555555ull & (v << bits | v >> 1);
    v = 0x3333333333333333ull & (v | v >> 1);
    v = 0x0f0f0f0f0f0f0f0full & (v | v >> 2);
    v = 0x00ff00ff00ff00ffull & (v | v >> 4);
    v = 0x0000ffff0000ffffull & (v | v >> 8);
    return (uint32_t)(v | v >> 16);
}

static uint64_t AlFrente(uint64_t lista, unsigned c)   // pasa el color c al principio de la lista
{
    uint64_t m = ~15ull;
    for (unsigned n = 0; n < 64; n += 4, m <<= 4)
        if ((lista >> n & 15) == c) return (lista & m) + (lista << 4 & ~m) + c;
    return lista;
}

static void DcuIniciar(Dcu& d, unsigned modo, uint32_t origen)
{
    memset(d.contexto, 0, sizeof d.contexto);
    d.bpp = 1u << modo;
    d.posicion = origen;
    d.bits = 8;
    d.rango = 0x100;
    d.entrada = LeerDatos(d.posicion++);
    d.entrada = (uint16_t)(d.entrada << 8 | LeerDatos(d.posicion++));
    d.salida = 0;
    d.pixeles = 0;
    d.colores = 0xfedcba9876543210ull;
}

static void DcuDecodificar(Dcu& d)   // 8 pixeles
{
    for (unsigned px = 0; px < 8; px++)
    {
        uint64_t mapa = d.colores;
        unsigned dif = 0;
        if (d.bpp > 1)
        {
            // Los vecinos (izquierda, arriba, arriba a la izquierda) eligen el contexto y el orden de los colores.
            unsigned pa = d.bpp == 2 ? (d.pixeles >> 2 & 3) : (d.pixeles & 15);
            unsigned pb = d.bpp == 2 ? (d.pixeles >> 14 & 3) : (d.pixeles >> 28 & 15);
            unsigned pc = d.bpp == 2 ? (d.pixeles >> 16 & 3) : (d.pixeles >> 32 & 15);
            if (pa != pb || pb != pc)
            {
                unsigned m = pa ^ pb ^ pc;
                dif = 4;
                if ((m ^ pc) == 0) dif = 3;
                if ((m ^ pb) == 0) dif = 2;
                if ((m ^ pa) == 0) dif = 1;
            }
            d.colores = AlFrente(d.colores, pa);
            mapa = AlFrente(mapa, pc);
            mapa = AlFrente(mapa, pb);
            mapa = AlFrente(mapa, pa);
        }
        for (unsigned plano = 0; plano < d.bpp; plano++)
        {
            unsigned bit = d.bpp > 1 ? 1u << plano : 1u << (px & 3);
            unsigned hist = (bit - 1) & d.salida;
            unsigned grupo = 0;
            if (d.bpp == 1) grupo = px >= 4;
            if (d.bpp == 2) grupo = dif;
            if (plano >= 2 && hist <= 1) grupo = dif;
            auto& ctx = d.contexto[grupo][bit + hist - 1];
            const uint8_t* mod = EVOLUCION[ctx.prediccion];
            uint8_t lps = (uint8_t)(d.rango - mod[0]);
            bool simbolo = d.entrada >= (lps << 8);
            d.salida = (uint8_t)(d.salida << 1 | (simbolo ^ ctx.cambio));
            if (!simbolo) d.rango = lps;
            else { d.rango = (uint16_t)(d.rango - lps); d.entrada = (uint16_t)(d.entrada - (lps << 8)); }
            while (d.rango <= 0x7F)
            {
                ctx.prediccion = mod[1 + simbolo];
                d.rango <<= 1;
                d.entrada <<= 1;
                if (--d.bits == 0) { d.bits = 8; d.entrada = (uint16_t)(d.entrada + LeerDatos(d.posicion++)); }
            }
            if (simbolo && mod[0] > 0x55) ctx.cambio ^= 1;
        }
        unsigned indice = d.salida & ((1u << d.bpp) - 1);
        if (d.bpp == 1) indice ^= d.pixeles >> 15 & 1;
        d.pixeles = d.pixeles << d.bpp | (mapa >> 4 * indice & 15);
    }
    if (d.bpp == 1) d.resultado = (uint32_t)d.pixeles;
    if (d.bpp == 2) d.resultado = Desentrelazar(d.pixeles, 16);
    if (d.bpp == 4) d.resultado = Desentrelazar(Desentrelazar(d.pixeles, 32), 32);
}

static void DcuCargarDireccion()   // entrada de la tabla: modo y dirección del bloque comprimido
{
    uint32_t a = (s.r[1] | s.r[2] << 8 | (s.r[3] & 0x7F) << 16) + (s.r[4] << 2);
    s.dcuModo = LeerDatos(a);
    s.dcuDir = (uint32_t)LeerDatos(a + 1) << 16 | LeerDatos(a + 2) << 8 | LeerDatos(a + 3);
    s.dcuDir &= 0x7FFFFF;
}

static void DcuEmpezar()
{
    s.r[0x0C] &= 0x7F;
    if ((s.dcuModo & 3) == 3) return;   // modo inválido
    DcuIniciar(s.d, s.dcuModo & 3, s.dcuDir);
    DcuDecodificar(s.d);
    unsigned saltar = (s.r[0x0B] & 2) ? (s.r[5] | s.r[6] << 8) : 0;
    while (saltar--) DcuDecodificar(s.d);
    s.r[0x0C] |= 0x80;
    s.dcuPos = 0;
}

static uint8_t DcuLeer()
{
    if (!(s.r[0x0C] & 0x80)) return 0;
    if (s.dcuPos == 0)
    {
        for (int fila = 0; fila < 8; fila++)
        {
            uint32_t v = s.d.resultado;
            switch (s.d.bpp)
            {
            case 1: s.dcuTile[fila] = (uint8_t)v; break;
            case 2: s.dcuTile[fila * 2] = (uint8_t)v; s.dcuTile[fila * 2 + 1] = (uint8_t)(v >> 8); break;
            case 4:
                s.dcuTile[fila * 2] = (uint8_t)v; s.dcuTile[fila * 2 + 1] = (uint8_t)(v >> 8);
                s.dcuTile[fila * 2 + 16] = (uint8_t)(v >> 16); s.dcuTile[fila * 2 + 17] = (uint8_t)(v >> 24);
                break;
            }
            unsigned saltar = (s.r[0x0B] & 1) ? s.r[7] : 1;
            while (saltar--) DcuDecodificar(s.d);
        }
    }
    uint8_t v = s.dcuTile[s.dcuPos++];
    s.dcuPos &= 8 * s.d.bpp - 1;
    return v;
}

// ---- puerto de datos ----
static uint32_t Desplazamiento() { return s.r[0x11] | s.r[0x12] << 8 | (s.r[0x13] & 0x7F) << 16; }
static uint32_t Ajuste() { return s.r[0x14] | s.r[0x15] << 8; }
static void PonerDesplazamiento(uint32_t a) { s.r[0x11] = (uint8_t)a; s.r[0x12] = (uint8_t)(a >> 8); s.r[0x13] = (uint8_t)(a >> 16) & 0x7F; }
static void PonerAjuste(uint32_t a) { s.r[0x14] = (uint8_t)a; s.r[0x15] = (uint8_t)(a >> 8); }

static void PuertoLeer()
{
    uint32_t aj = (s.r[0x18] & 2) ? Ajuste() : 0;
    if (s.r[0x18] & 8) aj = (uint32_t)(int16_t)aj;
    s.r[0x10] = LeerDatos(Desplazamiento() + aj);
}

static void PuertoAvanzar4810()
{
    uint32_t paso = (s.r[0x18] & 1) ? (uint32_t)(s.r[0x16] | s.r[0x17] << 8) : 1;
    uint32_t aj = Ajuste();
    if (s.r[0x18] & 4) paso = (uint32_t)(int16_t)paso;
    if (s.r[0x18] & 8) aj = (uint32_t)(int16_t)aj;
    if (!(s.r[0x18] & 16)) PonerDesplazamiento(Desplazamiento() + paso);
    else PonerAjuste(aj + paso);
    PuertoLeer();
}

static void PuertoSumarAjuste(int cual)   // $4814, $4815 o $481A suman el ajuste según el modo de $4818
{
    if (s.r[0x18] >> 5 != cual) return;
    uint32_t aj = Ajuste();
    if (s.r[0x18] & 8) aj = (uint32_t)(int16_t)aj;
    PonerDesplazamiento(Desplazamiento() + aj);
    PuertoLeer();
}

// ---- multiplicación y división ----
static void Multiplicar()
{
    uint32_t r;
    if (s.r[0x2E] & 1) r = (uint32_t)((int32_t)(int16_t)(s.r[0x24] | s.r[0x25] << 8) * (int16_t)(s.r[0x20] | s.r[0x21] << 8));
    else r = (uint32_t)(uint16_t)(s.r[0x24] | s.r[0x25] << 8) * (uint16_t)(s.r[0x20] | s.r[0x21] << 8);
    for (int i = 0; i < 4; i++) s.r[0x28 + i] = (uint8_t)(r >> 8 * i);
    s.r[0x2F] &= 0x7F;
}

static void Dividir()
{
    uint32_t dividendo = s.r[0x20] | s.r[0x21] << 8 | s.r[0x22] << 16 | (uint32_t)s.r[0x23] << 24;
    uint16_t divisor = (uint16_t)(s.r[0x26] | s.r[0x27] << 8);
    uint32_t q;
    uint16_t resto;
    if (s.r[0x2E] & 1)
    {
        int32_t a = (int32_t)dividendo;
        int16_t b = (int16_t)divisor;
        if (b) { q = (uint32_t)(int32_t)((int64_t)a / b); resto = (uint16_t)(int16_t)((int64_t)a % b); }
        else { q = 0; resto = (uint16_t)a; }
    }
    else if (divisor) { q = dividendo / divisor; resto = (uint16_t)(dividendo % divisor); }
    else { q = 0; resto = (uint16_t)dividendo; }
    for (int i = 0; i < 4; i++) s.r[0x28 + i] = (uint8_t)(q >> 8 * i);
    s.r[0x2C] = (uint8_t)resto;
    s.r[0x2D] = (uint8_t)(resto >> 8);
    s.r[0x2F] &= 0x7F;
}

// ---- reloj Epson RTC-4513 ----
// Los 16 registros de 4 bits: 0-1 segundos, 2-3 minutos, 4-5 horas, 6-7 día, 8-9 mes, 10-11 año, 12 día de la semana,
// 13-15 control. Avanza con el tiempo de la consola (128 pasos por segundo).
static const uint64_t PASO_RELOJ = 21477272 / 128;
static struct
{
    uint8_t reg[16];
    uint8_t chip, estado, dato, pos, listo, holdtick;   // estado: 0 modo, 1 dirección, 2 leer, 3 escribir
    uint64_t listoEn, proximo;
    uint32_t pasos, segundos;
    bool cargado;
} rtc;
enum { R_SEG = 0, R_MIN = 2, R_HORA = 4, R_DIA = 6, R_MES = 8, R_ANIO = 10, R_SEMANA = 12, R_C1 = 13, R_C2 = 14, R_C3 = 15 };

// Campos dentro de los registros (como en el chip; los valores BCD inválidos avanzan igual que en él).
#define LO(r) rtc.reg[r]
#define HI(r) rtc.reg[(r) + 1]
static inline bool Hold() { return rtc.reg[R_C1] & 1; }
static inline bool Calendario() { return rtc.reg[R_C1] >> 1 & 1; }
static inline bool Pausa() { return rtc.reg[R_C3] & 1; }
static inline bool Parado() { return rtc.reg[R_C3] >> 1 & 1; }
static inline bool Hora24() { return rtc.reg[R_C3] >> 2 & 1; }
// bits extra que viven en los registros altos
static uint8_t segHi, falloBateria, minHi, resync, horaHi, meridiano, diaHi, diaRam, mesHi, mesRam;
static uint8_t irqBandera, redondear, irqMascara, irqDuty, irqPeriodo;

static void TickAnio()
{
    uint8_t& lo = LO(R_ANIO); uint8_t& hi = HI(R_ANIO);
    if (lo <= 8 || lo == 12) lo++;
    else
    {
        lo = !(lo & 1);
        if (hi <= 8 || hi == 12) hi++;
        else hi = !(hi & 1);
    }
    lo &= 15; hi &= 15;
}

static void TickMes()
{
    uint8_t& lo = LO(R_MES);
    if (mesHi == 0 || !(lo & 2))
    {
        if (lo <= 8 || lo == 12) lo++;
        else { lo = !(lo & 1); mesHi ^= 1; }
    }
    else { lo = !(lo & 1); mesHi = 0; TickAnio(); }
    lo &= 15;
}

static void TickDia()
{
    if (!Calendario()) return;
    uint8_t& sem = LO(R_SEMANA);
    sem = (uint8_t)(((sem + 1) + (sem == 6)) & 7);
    static const uint8_t DIAS[32] = {30, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 30, 31, 30,
                                     31, 30, 31, 30, 31, 30, 31, 30, 31, 30, 31, 30, 31, 30, 31, 30};
    unsigned dias = DIAS[mesHi << 4 | LO(R_MES)];
    uint8_t anioLo = LO(R_ANIO), anioHi = HI(R_ANIO);
    if (dias == 28)
    {
        if ((anioHi & 1) == 0 && (anioLo & 3) == 0) dias++;
        if ((anioHi & 1) == 1 && ((anioLo - 2) & 3) == 0) dias++;
    }
    uint8_t& lo = LO(R_DIA);
    bool fin = (dias == 28 && (diaHi == 3 || (diaHi == 2 && lo >= 8))) ||
               (dias == 29 && (diaHi == 3 || (diaHi == 2 && (lo > 8 && lo != 12)))) ||
               (dias == 30 && (diaHi == 3 || (diaHi == 2 && (lo == 10 || lo == 14)))) ||
               (dias == 31 && (diaHi == 3 && (lo & 3)));
    if (fin) { lo = 1; diaHi = 0; TickMes(); return; }
    if (lo <= 8 || lo == 12) lo++;
    else { lo = !(lo & 1); diaHi = (diaHi + 1) & 3; }
    lo &= 15;
}

static void TickHora()
{
    uint8_t& lo = LO(R_HORA);
    if (Hora24())
    {
        if (horaHi < 2)
        {
            if (lo <= 8 || lo == 12) lo++;
            else { lo = !(lo & 1); horaHi++; }
        }
        else if (lo != 3 && !(lo & 4))
        {
            if (lo <= 8 || lo >= 12) lo++;
            else { lo = !(lo & 1); horaHi++; }
        }
        else { lo = !(lo & 1); horaHi = 0; TickDia(); }
    }
    else if (horaHi == 0)
    {
        if (lo <= 8 || lo == 12) lo++;
        else { lo = !(lo & 1); horaHi ^= 1; }
    }
    else
    {
        if (lo & 1) meridiano ^= 1;
        if (lo < 2 || lo == 4 || lo == 5 || lo == 8 || lo == 12) lo++;
        else { lo = !(lo & 1); horaHi ^= 1; }
        if (meridiano == 0 && !(lo & 1)) TickDia();
    }
    lo &= 15; horaHi &= 3;
}

static void TickMinuto()
{
    uint8_t& lo = LO(R_MIN);
    if (lo <= 8 || lo == 12) lo++;
    else
    {
        lo = 0;
        if (minHi < 5) minHi++;
        else { minHi = 0; TickHora(); }
    }
    lo &= 15;
}

static void TickSegundo()
{
    uint8_t& lo = LO(R_SEG);
    if (lo <= 8 || lo == 12) lo++;
    else
    {
        lo = 0;
        if (segHi < 5) segHi++;
        else { segHi = 0; TickMinuto(); }
    }
    lo &= 15;
}

static void RtcIrq(unsigned periodo) { if (!Parado() && !Pausa() && periodo == irqPeriodo) irqBandera = 1; }

static void RtcAlDia()
{
    if (redondear)   // redondeo de segundos (el chip lo hace en 125 µs)
    {
        redondear = 0;
        if (segHi >= 3) TickMinuto();
        LO(R_SEG) = 0; segHi = 0;
    }
    if (rtc.proximo > ciclosMaestros + 2 * PASO_RELOJ || rtc.proximo == 0) rtc.proximo = ciclosMaestros + PASO_RELOJ;   // estado cargado
    while (ciclosMaestros >= rtc.proximo)
    {
        rtc.proximo += PASO_RELOJ;
        rtc.pasos = (rtc.pasos + 1) & 127;
        if (irqDuty) irqBandera = 0;
        if (!(rtc.pasos & 1)) RtcIrq(0);
        if (rtc.pasos == 0)
        {
            rtc.segundos++;
            RtcIrq(1);
            if (rtc.segundos % 60 == 0) RtcIrq(2);
            if (rtc.segundos % 1440 == 0) { RtcIrq(3); rtc.segundos = 0; }
            if (!Parado() && !Pausa())
            {
                if (Hold()) rtc.holdtick = 1;
                else { resync = 1; TickSegundo(); }
            }
        }
    }
}

static uint8_t RtcLeerReg(unsigned a)
{
    switch (a)
    {
    case 1: return (uint8_t)(segHi | falloBateria << 3);
    case 3: return (uint8_t)(minHi | resync << 3);
    case 5: return (uint8_t)(horaHi | meridiano << 2 | resync << 3);
    case 7: return (uint8_t)(diaHi | diaRam << 2 | resync << 3);
    case 9: return (uint8_t)(mesHi | mesRam << 1 | resync << 3);
    case 12: return (uint8_t)(LO(R_SEMANA) | resync << 3);
    case 13:
    {
        uint8_t f = irqBandera & !irqMascara;
        irqBandera = 0;
        return (uint8_t)(Hold() | Calendario() << 1 | f << 2 | redondear << 3);
    }
    case 14: return (uint8_t)(irqMascara | irqDuty << 1 | irqPeriodo << 2);
    default: return rtc.reg[a] & 15;
    }
}

static void RtcGuardar();

static void RtcEscribirReg(unsigned a, uint8_t v)
{
    switch (a)
    {
    case 1: segHi = v & 7; falloBateria = v >> 3 & 1; break;
    case 3: minHi = v & 7; break;
    case 5: horaHi = v & 3; meridiano = v >> 2 & 1; if (Hora24()) meridiano = 0; else horaHi &= 1; break;
    case 7: diaHi = v & 3; diaRam = v >> 2 & 1; break;
    case 9: mesHi = v & 1; mesRam = v >> 1 & 3; break;
    case 12: rtc.reg[12] = v & 7; break;
    case 13:
    {
        bool antes = Hold();
        rtc.reg[13] = v & 3;
        redondear = v >> 3 & 1;
        if (antes && !Hold() && rtc.holdtick) { rtc.holdtick = 0; TickSegundo(); }
        break;
    }
    case 14: irqMascara = v & 1; irqDuty = v >> 1 & 1; irqPeriodo = v >> 2 & 3; break;
    case 15:
        rtc.reg[15] = v & 15;
        if (Hora24()) meridiano = 0;
        else horaHi &= 1;
        if (Pausa()) { LO(R_SEG) = 0; segHi = 0; }
        break;
    default: rtc.reg[a] = v & 15; break;
    }
    RtcGuardar();
}

// El reloj se guarda en los 16 bytes que siguen a la RAM del juego (en el mismo archivo de guardado), con la hora del
// sistema: al volver a cargar, avanza lo que pasó mientras tanto.
static void RtcGuardar()
{
    uint8_t* d = sram + spc7110RamTam;
    d[0] = (uint8_t)(LO(R_SEG) | segHi << 4 | falloBateria << 7);
    d[1] = (uint8_t)(LO(R_MIN) | minHi << 4 | resync << 7);
    d[2] = (uint8_t)(LO(R_HORA) | horaHi << 4 | meridiano << 6 | resync << 7);
    d[3] = (uint8_t)(LO(R_DIA) | diaHi << 4 | diaRam << 6 | resync << 7);
    d[4] = (uint8_t)(LO(R_MES) | mesHi << 4 | mesRam << 5 | resync << 7);
    d[5] = (uint8_t)(LO(R_ANIO) | HI(R_ANIO) << 4);
    d[6] = (uint8_t)(LO(R_SEMANA) | resync << 3 | Hold() << 4 | Calendario() << 5 | irqBandera << 6 | redondear << 7);
    d[7] = (uint8_t)(irqMascara | irqDuty << 1 | irqPeriodo << 2 | Pausa() << 4 | Parado() << 5 | Hora24() << 6 | (rtc.reg[15] >> 3 & 1) << 7);
    uint64_t t = Ahora();
    for (int i = 0; i < 8; i++) d[8 + i] = (uint8_t)(t >> 8 * i);
    sramCambiada = true;
}

static void RtcCargar()
{
    rtc.cargado = true;
    const uint8_t* d = sram + spc7110RamTam;
    uint64_t t = 0;
    for (int i = 0; i < 8; i++) t |= (uint64_t)d[8 + i] << 8 * i;
    if (!t)   // nunca se guardó: batería agotada, como un cartucho nuevo
    {
        memset(rtc.reg, 0, sizeof rtc.reg);
        segHi = minHi = resync = horaHi = meridiano = diaHi = diaRam = mesHi = mesRam = 0;
        irqBandera = redondear = irqMascara = irqDuty = irqPeriodo = 0;
        falloBateria = 1;
        return;
    }
    LO(R_SEG) = d[0] & 15; segHi = d[0] >> 4 & 7; falloBateria = d[0] >> 7;
    LO(R_MIN) = d[1] & 15; minHi = d[1] >> 4 & 7; resync = d[1] >> 7;
    LO(R_HORA) = d[2] & 15; horaHi = d[2] >> 4 & 3; meridiano = d[2] >> 6 & 1;
    LO(R_DIA) = d[3] & 15; diaHi = d[3] >> 4 & 3; diaRam = d[3] >> 6 & 1;
    LO(R_MES) = d[4] & 15; mesHi = d[4] >> 4 & 1; mesRam = d[4] >> 5 & 3;
    LO(R_ANIO) = d[5] & 15; HI(R_ANIO) = d[5] >> 4;
    LO(R_SEMANA) = d[6] & 7; rtc.reg[13] = d[6] >> 4 & 3; irqBandera = d[6] >> 6 & 1; redondear = d[6] >> 7;
    irqMascara = d[7] & 1; irqDuty = d[7] >> 1 & 1; irqPeriodo = d[7] >> 2 & 3; rtc.reg[15] = (uint8_t)(d[7] >> 4);
    uint64_t ahora = Ahora();
    uint64_t pasado = ahora > t ? ahora - t : 0;
    while (pasado >= 86400) { TickDia(); pasado -= 86400; }
    while (pasado >= 3600) { TickHora(); pasado -= 3600; }
    while (pasado >= 60) { TickMinuto(); pasado -= 60; }
    while (pasado--) TickSegundo();
}

static uint8_t RtcLeer(unsigned a)
{
    if (!rtc.cargado) RtcCargar();
    RtcAlDia();
    if (!rtc.listo && ciclosMaestros >= rtc.listoEn) rtc.listo = 1;
    if (a == 0) return rtc.chip;
    if (a == 2) return (uint8_t)(rtc.listo << 7);
    if (a != 1 || rtc.chip != 1 || !rtc.listo) return 0;
    if (rtc.estado == 3) return rtc.dato;
    if (rtc.estado != 2) return 0;
    rtc.listo = 0;
    rtc.listoEn = ciclosMaestros + 82;   // 8 ciclos del reloj del chip (2 MHz)
    uint8_t v = RtcLeerReg(rtc.pos);
    rtc.pos = (rtc.pos + 1) & 15;
    return v;
}

static void RtcEscribir(unsigned a, uint8_t v)
{
    if (!rtc.cargado) RtcCargar();
    RtcAlDia();
    if (!rtc.listo && ciclosMaestros >= rtc.listoEn) rtc.listo = 1;
    v &= 15;
    if (a == 0)
    {
        rtc.chip = v & 3;
        if (rtc.chip != 1) { rtc.estado = 0; rtc.pos = 0; resync = 0; rtc.reg[15] &= ~(1 | 8); }
        rtc.listo = 1;
        return;
    }
    if (a != 1 || rtc.chip != 1 || !rtc.listo) return;
    switch (rtc.estado)
    {
    case 0:
        if (v != 0x03 && v != 0x0C) return;
        rtc.estado = 1;
        break;
    case 1:
        rtc.estado = rtc.dato == 0x03 ? 3 : 2;
        rtc.pos = v;
        break;
    case 3:
        RtcEscribirReg(rtc.pos, v);
        rtc.pos = (rtc.pos + 1) & 15;
        break;
    default: return;
    }
    rtc.listo = 0;
    rtc.listoEn = ciclosMaestros + 82;
    rtc.dato = v;
}

// ---- registros ----
static uint8_t LeerRegistro(unsigned a)
{
    switch (a)
    {
    case 0x00:
    {
        uint16_t c = (uint16_t)((s.r[9] | s.r[0x0A] << 8) - 1);
        s.r[9] = (uint8_t)c; s.r[0x0A] = (uint8_t)(c >> 8);
        return DcuLeer();
    }
    case 0x08: return 0;
    case 0x10: { uint8_t v = s.r[0x10]; PuertoAvanzar4810(); return v; }
    case 0x1A: PuertoSumarAjuste(3); return 0;
    }
    if (a <= 0x18 || (a >= 0x20 && a <= 0x2F) || (a >= 0x30 && a <= 0x34)) return s.r[a];
    return 0;
}

static void EscribirRegistro(unsigned a, uint8_t v)
{
    switch (a)
    {
    case 0x01: case 0x02: case 0x05: case 0x07: case 0x09: case 0x0A: case 0x11: case 0x12: case 0x16: case 0x17:
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x26:
        s.r[a] = v; break;
    case 0x03: s.r[3] = v & 0x7F; break;
    case 0x04: s.r[4] = v; DcuCargarDireccion(); break;
    case 0x06: s.r[6] = v; DcuEmpezar(); break;
    case 0x0B: s.r[0x0B] = v & 3; break;
    case 0x13: s.r[0x13] = v & 0x7F; PuertoLeer(); break;
    case 0x14: s.r[0x14] = v; PuertoSumarAjuste(1); break;
    case 0x15: s.r[0x15] = v; if (s.r[0x18] & 2) PuertoLeer(); PuertoSumarAjuste(2); break;
    case 0x18: s.r[0x18] = v & 0x7F; PuertoLeer(); break;
    case 0x25: s.r[0x25] = v; Multiplicar(); break;
    case 0x27: s.r[0x27] = v; Dividir(); break;
    case 0x2E: s.r[0x2E] = v & 1; break;
    case 0x30: s.r[0x30] = v & 0x87; break;
    case 0x31: case 0x32: case 0x33: case 0x34: s.r[a] = v & 7; break;
    }
}

void Spc7110Reiniciar()
{
    memset(&s, 0, sizeof s);
    s.r[0x32] = 1;
    s.r[0x33] = 2;
    memset(&rtc, 0, sizeof rtc);
}

bool Spc7110Leer(uint8_t banco, uint16_t off, uint8_t& v, uint8_t abierto)
{
    uint8_t b = banco & 0x7F;
    if (banco == 0x50 || banco == 0x58) { v = LeerRegistro(banco == 0x50 ? 0 : 8); return true; }
    if (b < 0x40)
    {
        if (off >= 0x4800 && off < 0x4840) { v = LeerRegistro(off & 0x3F); return true; }
        if (spc7110Reloj && off >= 0x4840 && off <= 0x4842) { v = RtcLeer(off & 3); return true; }
        if (off >= 0x6000 && off < 0x8000) { v = (s.r[0x30] & 0x80) && spc7110RamTam ? sram[(off & 0x1FFF) % spc7110RamTam] : 0; return true; }
        if (off >= 0x8000) { v = LeerRom((uint32_t)b << 16 | off); return true; }
        return false;
    }
    if (banco >= 0xC0) { v = LeerRom((uint32_t)(banco & 0x3F) << 16 | off); return true; }
    if (banco < 0x50 && romTam == 0x700000) { v = romDatos[EXPANSION + ((uint32_t)(banco & 0x0F) << 16 | off)]; return true; }
    if (banco < 0x7E) { v = abierto; return true; }   // 40-7D: nada
    return false;
}

bool Spc7110Escribir(uint8_t banco, uint16_t off, uint8_t v)
{
    uint8_t b = banco & 0x7F;
    if (banco == 0x50 || banco == 0x58) { EscribirRegistro(banco == 0x50 ? 0 : 8, v); return true; }
    if (b < 0x40)
    {
        if (off >= 0x4800 && off < 0x4840) { EscribirRegistro(off & 0x3F, v); return true; }
        if (spc7110Reloj && off >= 0x4840 && off <= 0x4842) { RtcEscribir(off & 3, v); return true; }
        if (off >= 0x6000 && off < 0x8000)
        {
            if ((s.r[0x30] & 0x80) && spc7110RamTam)
            {
                uint8_t& m = sram[(off & 0x1FFF) % spc7110RamTam];
                if (m != v) { m = v; sramCambiada = true; }
            }
            return true;
        }
        return off >= 0x8000;
    }
    return banco >= 0x40 && banco < 0x7E ? true : banco >= 0xC0;
}

void Spc7110Estado(Serial& se)
{
    se(s);
    se(rtc);
    se(segHi); se(falloBateria); se(minHi); se(resync); se(horaHi); se(meridiano); se(diaHi); se(diaRam); se(mesHi);
    se(mesRam); se(irqBandera); se(redondear); se(irqMascara); se(irqDuty); se(irqPeriodo);
}
