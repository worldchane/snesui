// Coordina las partes: corre el procesador línea por línea, dibuja la imagen con la PPU y, al terminar la parte visible,
#include <stdio.h>
// dispara la interrupción de "vblank" (NMI) que los juegos usan para sincronizarse. Un cuadro = una pasada de esto.
#include "interno.h"
#include "rom.h"

#include <stdlib.h>

uint64_t ciclosMaestros;
void BusCiclo(int c) { ciclosMaestros += c; }

// Declaraciones de bus.cpp que usa el cuadro.
void BusLeerMandos();
bool BusNmiHabilitado();
void BusMarcarNmi();
void BusHvbjoy(uint8_t v);
void BusReiniciar();
void BusEncender();
void PpuVblank();
bool PpuSobrebarrido();
void HdmaIniciar();
void HdmaLinea();
int BusIrqModo();
int BusHtime();
int BusVtime();
void BusDispararIrq();
void BusFinVblank();
void BusProgramarNmi(uint64_t t);
void BusMandoLinea();

static InfoRom info;

// Punto de la línea y (en ciclos desde su comienzo) en que salta la IRQ por tiempo, o -1.
static int PuntoIrq(int y)
{
    int modo = BusIrqModo();
    if (modo == 1 && BusHtime() <= 339) return BusHtime() * 4 + 14;
    if (modo == 2 && BusVtime() == y) return 10;
    if (modo == 3 && BusVtime() == y && BusHtime() <= 339) return BusHtime() * 4 + 14;
    return -1;
}
static bool cargado;

// Tiempos de la consola: 1 línea = 1364 ciclos maestros; 262 líneas en NTSC (312 en PAL). La parte visible son 224
// líneas que empiezan en la línea 1.
static const int CICLOS_LINEA = 1364;

bool SnesCargar(const char* ruta, char* error)
{
    if (!RomCargar(ruta, info)) { if (error) snprintf(error, 160, "%s", info.error); return false; }
    cargado = true;
    BusEncender();
    SnesReiniciar();
    return true;
}

void SnesDescargar()
{
    free(romDatos);
    romDatos = NULL;
    romTam = 0;
    cargado = false;
}

static void DiagReiniciar();

void SnesReiniciar()
{
    ciclosMaestros = 0;
    DiagReiniciar();
    BusReiniciar();
    GsuReiniciar();
    if (dsp1Presente) Dsp1Reiniciar();
    if (dsp2Presente) Dsp2Reiniciar();
    if (obc1Presente) Obc1Reiniciar();
    if (sa1Presente) Sa1Reiniciar();
    if (cx4Presente) Cx4Reiniciar();
    if (sdd1Presente) Sdd1Reiniciar();
    if (spc7110Presente) Spc7110Reiniciar();
    if (st010Presente) St010Reiniciar();
    PpuReiniciar();
    ApuReiniciar();
    CpuReiniciar();
}

void SnesMando(int jugador, uint16_t botones) { if (jugador >= 0 && jugador < 2) mando[jugador] = botones; }
void SnesMandoConectado(int jugador, bool si) { if (jugador >= 0 && jugador < 2) mandoConectado[jugador] = si; }
const InfoRom& SnesInfo() { return info; }

// Posición del haz, para los contadores que lee el juego ($2137/$213C/$213D).
static uint64_t inicioLinea, inicioCuadro;
static int lineaActual, lineasCuadro = 262;

// Cuándo (en el reloj maestro) es la próxima vez que corre el HDMA: el hblank de una línea visible, o el comienzo del
// cuadro siguiente. Un DMA en un canal que también hace HDMA se corta ahí (ver bus.cpp).
uint64_t SnesProximoHdma()
{
    const int HBLANK = 1096, VBLANK = 225;
    uint64_t t = ciclosMaestros;
    uint64_t finCuadro = inicioCuadro + (uint64_t)lineasCuadro * CICLOS_LINEA;
    if (t >= finCuadro) return finCuadro;   // ya pasó: el corte es inmediato
    uint64_t linea = (t - inicioCuadro) / CICLOS_LINEA, h = (t - inicioCuadro) % CICLOS_LINEA;
    if (linea < VBLANK && h < HBLANK) return inicioCuadro + linea * CICLOS_LINEA + HBLANK;
    if (linea + 1 < VBLANK) return inicioCuadro + (linea + 1) * CICLOS_LINEA + HBLANK;
    return finCuadro;
}
void SnesContadores(int* h, int* v)
{
    int64_t d = (int64_t)(ciclosMaestros - inicioLinea) / 4;
    if (d < 0) d = 0;
    if (d > 339) d = 339;
    *h = (int)d;
    *v = lineaActual;
}

// ---------- diagnóstico ----------
// Un historial de los últimos 30 segundos (una muestra cada 60 cuadros), para ver qué hizo el juego antes del fallo.
struct Muestra
{
    uint32_t nmi, irq, gsuArranques, gsuParadas, gsuVblank;
    uint64_t gsuInstrucciones;
    uint32_t gsuOcupado;   // porcentaje del tiempo con el Super FX trabajando
    uint8_t inidisp;
};
static const int HISTORIAL = 30;
static Muestra historial[HISTORIAL];
static int nHistorial, posHistorial;
static uint32_t cuadros, cuadrosSegundo, gsuVblankSegundo;
static uint32_t nmiAntes, irqAntes;
static uint64_t gArrAntes, gParAntes, gInsAntes, gOcuAntes, ciclosAntes;

// Anomalías que se detectan solas (cada una se avisa una vez por partida): ver SnesAnomalia.
enum { AN_SIN_NMI = 1, AN_PARADO = 2, AN_GSU_NO_TERMINA = 4, AN_GSU_NO_ARRANCA = 8 };
static uint32_t anomaliasVistas;
static int segSinNmi, segGsuSinParar;
static char anomalia[200];

static void Anomalia(uint32_t cual, const char* txt)
{
    if (anomaliasVistas & cual) return;
    anomaliasVistas |= cual;
    if (!anomalia[0]) snprintf(anomalia, sizeof anomalia, "%s", txt);
}

static void RevisarAnomalias(const Muestra& m)
{
    // El juego ya no recibe la interrupción de cada cuadro: casi siempre quedó trabado esperando algo del hardware.
    // Muchos juegos apagan la NMI mientras cargan o descomprimen; solo es raro si el juego ya andaba (10 s seguidos con
    // NMI) y después pasa 20 s sin ninguna (cargar un nivel puede tardar más de 10).
    static int segConNmi;
    if (m.nmi || m.irq) { segConNmi++; segSinNmi = 0; }   // algunos juegos usan solo la IRQ
    else segSinNmi++;
    if (segSinNmi == 20 && segConNmi >= 10)
        Anomalia(AN_SIN_NMI, "El juego lleva 20 segundos sin interrupciones (ni NMI ni IRQ) después de andar bien: parece trabado");
    if (segSinNmi >= 20) segConNmi = 0;
    if (cpu.parado) Anomalia(AN_PARADO, "El procesador se detuvo (instrucción STP)");
    if (gsuPresente)
    {
        // Un dibujo del Super FX dura una fracción de segundo: si no para en 5 segundos mientras la SNES lo espera
        // (consultando su estado sin parar), el programa del chip se perdió. Hay juegos que lo dejan corriendo a
        // propósito, atendiendo comandos: eso no es un problema.
        static uint32_t consultasAntes;
        uint32_t consultas = gsuConsultasSfr - consultasAntes;
        consultasAntes = gsuConsultasSfr;
        segGsuSinParar = gsuCorre && m.gsuParadas == 0 && consultas > 1000 ? segGsuSinParar + 1 : 0;
        if (segGsuSinParar == 5) Anomalia(AN_GSU_NO_TERMINA, "El Super FX lleva 5 segundos corriendo sin terminar");
        if (nHistorial == 15 && gArrAntes == 0) Anomalia(AN_GSU_NO_ARRANCA, "El juego tiene Super FX pero en 15 segundos nunca lo arrancó");
    }
}

bool SnesAnomalia(char* txt, int max)
{
    if (!anomalia[0]) return false;
    snprintf(txt, max, "%s", anomalia);
    anomalia[0] = 0;
    return true;
}

static void DiagReiniciar()
{
    nHistorial = posHistorial = 0;
    cuadros = cuadrosSegundo = gsuVblankSegundo = 0;
    nmiAntes = diagNmi; irqAntes = diagIrq;
    gArrAntes = gParAntes = gInsAntes = gOcuAntes = ciclosAntes = 0;
    anomaliasVistas = 0;
    segSinNmi = segGsuSinParar = 0;
    anomalia[0] = 0;
}

static void AnotarSegundo()
{
    uint64_t a = 0, p = 0, i = 0, o = 0;
    if (gsuPresente) GsuContadores(&a, &p, &i, &o);
    Muestra& m = historial[posHistorial];
    m.nmi = diagNmi - nmiAntes;
    m.irq = diagIrq - irqAntes;
    m.gsuArranques = (uint32_t)(a - gArrAntes);
    m.gsuParadas = (uint32_t)(p - gParAntes);
    m.gsuInstrucciones = i - gInsAntes;
    uint64_t total = ciclosMaestros - ciclosAntes;
    m.gsuOcupado = total ? (uint32_t)((o - gOcuAntes) * 100 / total) : 0;
    m.gsuVblank = gsuVblankSegundo;
    extern uint8_t PpuInidisp();
    m.inidisp = PpuInidisp();
    posHistorial = (posHistorial + 1) % HISTORIAL;
    if (nHistorial < HISTORIAL) nHistorial++;
    nmiAntes = diagNmi; irqAntes = diagIrq;
    gArrAntes = a; gParAntes = p; gInsAntes = i; gOcuAntes = o;
    ciclosAntes = ciclosMaestros;
    gsuVblankSegundo = 0;
    RevisarAnomalias(m);
}

int SnesCuadro(uint32_t* pantalla, int16_t* audio, int maxAudio)
{
    if (!cargado) return 0;
    if (++cuadrosSegundo >= 60) { cuadrosSegundo = 0; AnotarSegundo(); }
    cuadros++;
    int lineasTotales = info.pal ? 312 : 262;
    uint64_t base = ciclosMaestros;
    inicioCuadro = base;
    lineasCuadro = lineasTotales;

    // Cada línea: se dibuja al empezar (con lo que dejó la línea anterior, incluido su HDMA), corre el procesador hasta
    // la interrupción por tiempo si toca, luego hasta el hblank (ahí corre el HDMA para la línea siguiente) y al final
    // hasta el fin de la línea.
    for (int y = 0; y < lineasTotales; y++)
    {
        inicioLinea = base + (uint64_t)y * CICLOS_LINEA;
#ifdef GSU_TRAZA
        { extern int depurCuadro; static int l_ = getenv("LINEAS") ? 1 : 0;
          if (l_ && depurCuadro) printf("linea %3d: CPU %02X:%04X %s%s P=%02X\n", y, cpu.pbr, cpu.pc, cpu.esperando ? "WAI " : "", gsuCorre ? "GSU " : "", cpu.p); }
#endif
        lineaActual = y;
        int lineaVblank = PpuSobrebarrido() ? 240 : 225;
        bool enVblank = y >= lineaVblank;
        if (y == 0)
        {
            BusFinVblank();
            BusProgramarNmi(base + (uint64_t)lineaVblank * CICLOS_LINEA);
            PpuNuevoCuadro();
            HdmaIniciar();
        }
        if (y == lineaVblank)
        {
            if (gsuCorre) gsuVblankSegundo++;   // el Super FX todavía estaba dibujando al terminar la imagen
            PpuVblank();
            BusLeerMandos();
            BusMarcarNmi();
        }
        BusHvbjoy(enVblank ? 0x80 : 0);
        BusMandoLinea();
        // Se muestran 224 líneas: las 1..224, o en el modo de 239 líneas las del centro (8..231), como bsnes.
        int primera = PpuSobrebarrido() ? 8 : 1;
        if (y >= primera && y < primera + SNES_ALTO && pantalla) PpuLinea(y - 1, pantalla + (y - primera) * SNES_ANCHO);

        // Interrupción por tiempo (H/V IRQ): el punto se recalcula si el juego cambia $4200 o HTIME/VTIME a mitad de
        // línea (hay juegos que encienden la IRQ horizontal y esperan que salte en esa misma línea).
        const int HBLANK = 1096, REFRESCO = 538;
        int ultimoPunto = -1;   // punto donde ya saltó en esta línea (si se reprograma más adelante, puede saltar otra vez)
        bool reprogramadaEnLinea = false;   // si no se tocó, salta aunque la instrucción anterior se haya pasado unos ciclos
        auto Correr = [&](uint64_t hasta) {
            for (;;)
            {
                int p = PuntoIrq(y);
                uint64_t t = inicioLinea + (uint64_t)p;
                irqReprogramada = false;
                // La IRQ de este tramo: si el procesador ya pasó el punto (por ejemplo, detenido en un DMA largo), queda
                // pedida igual, salvo que el juego la haya reprogramado en esta línea después del punto.
                if (p > ultimoPunto && t <= hasta && (t >= ciclosMaestros || !reprogramadaEnLinea))
                {
                    if (ciclosMaestros < t)
                    {
                        CpuCorrerHasta(t);
                        if (irqReprogramada)
                        {
                            reprogramadaEnLinea = true;
                            if (ciclosMaestros < t) continue;   // cambió antes de llegar: se recalcula
                        }
                    }
                    BusDispararIrq();
                    ultimoPunto = p;
#ifdef GSU_TRAZA
                    { extern int depurCuadro; static int l_ = getenv("IRQREG") ? 1 : 0; if (l_ && depurCuadro) printf("  >> IRQ disparada linea %d punto %d (modo %d) ciclo %d\n", y, p, BusIrqModo(), (int)(ciclosMaestros - inicioLinea)); }
#endif
                    continue;
                }
                if (ciclosMaestros >= hasta) break;
                CpuCorrerHasta(hasta);
                if (irqReprogramada) reprogramadaEnLinea = true;
            }
        };
        Correr(inicioLinea + REFRESCO);
        // Refresco de la RAM: el procesador queda detenido 40 ciclos en cada línea.
        BusCiclo(40);
        Correr(inicioLinea + HBLANK);
        BusHvbjoy((enVblank ? 0x80 : 0) | 0x40);
        if (y < lineaVblank) HdmaLinea();
        Correr(inicioLinea + CICLOS_LINEA);
        GsuCorrerHasta(inicioLinea + CICLOS_LINEA);   // el Super FX corre en paralelo: se lo pone al día cada línea
        Sa1CorrerHasta(inicioLinea + CICLOS_LINEA);   // igual el SA-1
    }
    return ApuCuadro(audio, maxAudio, info.pal);
}

// ---------- guardado ----------
uint32_t SnesSramTam() { return cargado ? sramTam : 0; }
uint8_t* SnesSramDatos() { return sram; }
bool SnesSramCambiada() { bool c = sramCambiada; sramCambiada = false; return c; }

// Cabecera de un estado: marca, versión y qué juego es (tamaño y título de la rom).
struct CabeceraEstado { char marca[8]; uint32_t version, romTam; char titulo[24]; };
static const uint32_t VERSION_ESTADO = 1;

static void Estado(Serial& s)
{
    CabeceraEstado c;
    memset(&c, 0, sizeof c);
    memcpy(c.marca, "SNESPS4E", 8);
    c.version = VERSION_ESTADO;
    c.romTam = romTam;
    memcpy(c.titulo, info.titulo, sizeof c.titulo);
    if (s.cargar)
    {
        CabeceraEstado leida;
        s(leida);
        if (memcmp(&leida, &c, sizeof c) != 0) { s.ok = false; return; }
    }
    else s(c);
    s(ciclosMaestros); s(inicioLinea); s(lineaActual);
    CpuEstado(s);
    BusEstado(s);
    PpuEstado(s);
    ApuEstado(s);
    if (dsp1Presente) Dsp1Estado(s);
    if (dsp2Presente) Dsp2Estado(s);
    if (obc1Presente) Obc1Estado(s);
    if (sa1Presente) Sa1Estado(s);
    if (cx4Presente) Cx4Estado(s);
    if (sdd1Presente) Sdd1Estado(s);
    if (spc7110Presente) Spc7110Estado(s);
    if (st010Presente) St010Estado(s);
    if (gsuPresente)
    {
        // El chip y toda su RAM (no solo la parte con batería).
        GsuEstado(s);
        s.Bloque(sram, 128 * 1024);
    }
    else s.Bloque(sram, sramTam);
}

uint32_t SnesEstadoTam()
{
    Serial s = {NULL, 0, 0, false, true};
    Estado(s);
    return s.pos;
}

uint32_t SnesGuardarEstado(uint8_t* buf, uint32_t max)
{
    if (!cargado) return 0;
    Serial s = {buf, 0, max, false, true};
    Estado(s);
    return s.ok ? s.pos : 0;
}

bool SnesCargarEstado(const uint8_t* buf, uint32_t n)
{
    if (!cargado || n != SnesEstadoTam()) return false;
    // Primero se revisa la cabecera sin tocar nada; recién después se carga todo.
    CabeceraEstado c;
    memcpy(&c, buf, sizeof c);
    if (memcmp(c.marca, "SNESPS4E", 8) || c.version != VERSION_ESTADO || c.romTam != romTam ||
        memcmp(c.titulo, info.titulo, sizeof c.titulo))
        return false;
    Serial s = {(uint8_t*)buf, 0, n, true, true};
    Estado(s);
    return s.ok;
}

void SnesSuperFxVelocidad(int v) { GsuVelocidad(v); }

// Informe de diagnóstico en texto: la rom, el estado de cada parte y el historial de los últimos segundos.
int SnesDiagnostico(char* o, int m)
{
    int n = 0;
#define mas(...) do { if (n < m - 1) { int k_ = snprintf(o + n, m - n, __VA_ARGS__); n += k_ > 0 ? k_ : 0; if (n > m - 1) n = m - 1; } } while (0)
    const uint8_t* h = romDatos ? romDatos + (romHiRom ? 0xFFC0 : 0x7FC0) : NULL;
    mas("Juego (cabecera): \"%s\"   %s   %d KB   %s\n", info.titulo, info.mapeo, info.tamKB, info.pal ? "PAL" : "NTSC");
    if (h && romTam > (uint32_t)(romHiRom ? 0xFFE0 : 0x7FE0))
        mas("  mapa=%02X tipo=%02X rom=%02X ram=%02X país=%02X versión=%02X suma=%02X%02X  cabecera extendida=%s ram exp=%02X\n",
            h[0x15], h[0x16], h[0x17], h[0x18], h[0x19], h[0x1B], h[0x1F], h[0x1E], h[0x1A] == 0x33 ? "sí" : "no", h[-3]);
    mas("  tamaño real %u bytes, SRAM con batería %u bytes, chip: %s\n", romTam, sramTam, romChip[0] ? romChip : "ninguno");
    mas("Cuadros emulados: %u\n", cuadros);
    mas("CPU: PC=%02X:%04X A=%04X X=%04X Y=%04X S=%04X D=%04X DB=%02X P=%02X E=%d %s%s\n", cpu.pbr, cpu.pc, cpu.a, cpu.x,
        cpu.y, cpu.s, cpu.d, cpu.dbr, cpu.p, cpu.e, cpu.esperando ? "(esperando WAI) " : "", cpu.parado ? "(parado STP)" : "");
    mas("  interrupciones: NMI=%u IRQ=%u\n", diagNmi, diagIrq);
    char t[400];
    extern void PpuVolcado(char*, int);
    PpuVolcado(t, sizeof t);
    extern uint8_t PpuInidisp();
    mas("PPU: INIDISP=%02X %s\n", PpuInidisp(), t);
    if (gsuPresente) { n += GsuDiagnostico(o + n, m - n); }
    mas("\nÚltimos %d segundos (el más reciente al final):\n", nHistorial);
    mas("  seg   NMI   IRQ   INIDISP   SFX arranques  paradas  instrucciones  ocupado  corriendo-en-vblank\n");
    for (int k = 0; k < nHistorial; k++)
    {
        const Muestra& s = historial[(posHistorial - nHistorial + k + HISTORIAL) % HISTORIAL];
        mas("  %3d  %4u  %4u     %02X        %5u      %5u    %11llu     %3u%%   %u\n", k - nHistorial + 1, s.nmi, s.irq,
            s.inidisp, s.gsuArranques, s.gsuParadas, (unsigned long long)s.gsuInstrucciones, s.gsuOcupado, s.gsuVblank);
    }
    return n;
#undef mas
}

// Depuración.
#include <stdio.h>
void ApuVolcado(char* out, int max);
uint8_t PpuInidisp();
void SnesVolcado(char* out, int max)
{
    char a[160];
    ApuVolcado(a, sizeof a);
    snprintf(out, max, "CPU pc=%02X:%04X A=%04X X=%04X Y=%04X S=%04X P=%02X e=%d  INIDISP=%02X  %s",
             cpu.pbr, cpu.pc, cpu.a, cpu.x, cpu.y, cpu.s, cpu.p, cpu.e, PpuInidisp(), a);
}
