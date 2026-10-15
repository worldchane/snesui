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
void PpuVblank();
bool PpuSobrebarrido();
void HdmaIniciar();
void HdmaLinea();
int BusIrqModo();
int BusHtime();
int BusVtime();
void BusDispararIrq();
void BusFinVblank();
void BusMandoLinea();

static InfoRom info;
static bool cargado;

// Tiempos de la consola: 1 línea = 1364 ciclos maestros; 262 líneas en NTSC (312 en PAL). La parte visible son 224
// líneas que empiezan en la línea 1.
static const int CICLOS_LINEA = 1364;

bool SnesCargar(const char* ruta, char* error)
{
    if (!RomCargar(ruta, info)) { if (error) snprintf(error, 160, "%s", info.error); return false; }
    cargado = true;
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

void SnesReiniciar()
{
    ciclosMaestros = 0;
    BusReiniciar();
    GsuReiniciar();
    PpuReiniciar();
    ApuReiniciar();
    CpuReiniciar();
}

void SnesMando(int jugador, uint16_t botones) { if (jugador >= 0 && jugador < 2) mando[jugador] = botones; }
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

int SnesCuadro(uint32_t* pantalla, int16_t* audio, int maxAudio)
{
    if (!cargado) return 0;
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
        lineaActual = y;
        int lineaVblank = PpuSobrebarrido() ? 240 : 225;
        bool enVblank = y >= lineaVblank;
        if (y == 0)
        {
            BusFinVblank();
            PpuNuevoCuadro();
            HdmaIniciar();
        }
        if (y == lineaVblank)
        {
            PpuVblank();
            BusLeerMandos();
            BusMarcarNmi();
        }
        BusHvbjoy(enVblank ? 0x80 : 0);
        BusMandoLinea();
        if (y >= 1 && y <= 224 && pantalla) PpuLinea(y - 1, pantalla + (y - 1) * SNES_ANCHO);

        // Interrupción por tiempo (H/V IRQ).
        int modo = BusIrqModo(), puntoIrq = -1;
        if (modo == 1 && BusHtime() <= 339) puntoIrq = BusHtime() * 4 + 14;
        else if (modo == 2 && BusVtime() == y) puntoIrq = 10;
        else if (modo == 3 && BusVtime() == y && BusHtime() <= 339) puntoIrq = BusHtime() * 4 + 14;
        const int HBLANK = 1096, REFRESCO = 538;
        if (puntoIrq >= 0 && puntoIrq < REFRESCO)
        {
            CpuCorrerHasta(inicioLinea + puntoIrq);
            BusDispararIrq();
        }
        // Refresco de la RAM: el procesador queda detenido 40 ciclos en cada línea.
        CpuCorrerHasta(inicioLinea + REFRESCO);
        BusCiclo(40);
        if (puntoIrq >= REFRESCO && puntoIrq < HBLANK)
        {
            CpuCorrerHasta(inicioLinea + puntoIrq);
            BusDispararIrq();
        }
        CpuCorrerHasta(inicioLinea + HBLANK);
        BusHvbjoy((enVblank ? 0x80 : 0) | 0x40);
        if (y < lineaVblank) HdmaLinea();
        if (puntoIrq >= HBLANK)
        {
            CpuCorrerHasta(inicioLinea + puntoIrq);
            BusDispararIrq();
        }
        CpuCorrerHasta(inicioLinea + CICLOS_LINEA);
        GsuCorrerHasta(inicioLinea + CICLOS_LINEA);   // el Super FX corre en paralelo: se lo pone al día cada línea
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
