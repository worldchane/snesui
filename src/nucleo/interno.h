// Estado interno compartido por las partes del núcleo. No usar desde la interfaz.
#pragma once
#include <stdint.h>
#include <string.h>

#include "snes.h"

// El procesador 65816 de la SNES. A y los índices X/Y son de 16 bits, pero según unos bits de estado (M y X) se usan
// como de 8 bits. En "modo emulación" (E=1, como al encender) se comporta como el 6502 de la NES.
struct Cpu
{
    uint16_t a, x, y, s, d;   // acumulador, índices, pila, página directa
    uint16_t pc;
    uint8_t pbr, dbr;         // banco del programa y de los datos
    uint8_t p;                // banderas: C Z I D X M V N (bits 0..7)
    bool e;                   // modo emulación
    bool parado;              // STP: el procesador se detuvo
    bool esperando;           // WAI: espera una interrupción
    uint64_t ciclos;
};

// Banderas de estado (registro P).
enum { F_C = 1, F_Z = 2, F_I = 4, F_D = 8, F_X = 16, F_M = 32, F_V = 64, F_N = 128 };

extern Cpu cpu;
extern uint8_t wram[128 * 1024];

// El bus: toda lectura/escritura de la SNES pasa por acá (dirección de 24 bits: banco << 16 | desplazamiento).
uint8_t BusLeer(uint32_t dir);
void BusEscribir(uint32_t dir, uint8_t v);
// Un ciclo del bus avanza el reloj; el procesador y la imagen se miden en estos ciclos maestros.
void BusCiclo(int ciclos);

// Cuánto tarda un acceso del procesador según la dirección (en ciclos maestros): la RAM y la rom lenta 8, los registros
// 6, el puerto del mando 12, y la rom rápida (FastROM, bancos 0x80-0xFF con $420D) 6.
extern bool memselRapido;
static inline int VelocidadAcceso(uint32_t a)
{
    uint8_t banco = a >> 16;
    uint16_t off = (uint16_t)a;
    if (banco & 0x40) return ((banco & 0x80) && memselRapido) ? 6 : 8;
    if (off & 0x8000) return ((banco & 0x80) && memselRapido) ? 6 : 8;
    if (off < 0x2000) return 8;
    if (off < 0x4000) return 6;
    if (off < 0x4200) return 12;
    if (off < 0x6000) return 6;
    return 8;
}

// Pedidos de interrupción (bus.cpp): la IRQ por tiempo queda pedida hasta que se lee $4211; la NMI una vez.
extern bool irqLinea, nmiPendiente;

void CpuReiniciar();
void CpuInterrupcion(bool nmi);   // dispara NMI (vblank) o IRQ
// Corre instrucciones hasta llegar al ciclo maestro 'hasta'.
void CpuCorrerHasta(uint64_t hasta);

// Registros de hardware, atendidos por ppu.cpp / apu.cpp / dma (bus.cpp).
uint8_t PpuLeer(uint16_t reg, uint8_t abierto);
void PpuEscribir(uint16_t reg, uint8_t v);
void PpuReiniciar();
// Dibuja la línea 'y' (0..223) en el búfer de la pantalla.
void PpuLinea(int y, uint32_t* fila);
void PpuNuevoCuadro();

uint8_t ApuLeer(uint16_t puerto);     // los 4 puertos que el CPU ve en 0x2140-0x2143
void ApuEscribir(uint16_t puerto, uint8_t v);
void ApuReiniciar();
int ApuCuadro(int16_t* audio, int maxAudio, bool pal);

extern uint16_t mando[2];

// Guardar y cargar el estado completo (estados rápidos). Cada parte pasa sus variables por el mismo Serial: al guardar se
// copian al búfer, al cargar se leen de él, y sin búfer solo se mide el tamaño. Así el orden es siempre el mismo.
struct Serial
{
    uint8_t* datos;   // NULL = solo medir
    uint32_t pos, max;
    bool cargar, ok;
    void Bloque(void* v, uint32_t n)
    {
        if (datos)
        {
            if (pos + n > max) { ok = false; return; }
            if (cargar) memcpy(v, datos + pos, n);
            else memcpy(datos + pos, v, n);
        }
        pos += n;
    }
    template <class T> void operator()(T& v) { Bloque(&v, sizeof v); }
};
void CpuEstado(Serial& s);
void BusEstado(Serial& s);
void PpuEstado(Serial& s);
void ApuEstado(Serial& s);
extern bool sramCambiada;

// Super FX (superfx.cpp).
extern bool gsuPresente, irqGsu, gsuCorre;
void GsuConfigurar(bool presente, uint32_t ramTam, bool gsu2);
void GsuReiniciar();
void GsuCorrerHasta(uint64_t hasta);
uint8_t GsuLeer(uint16_t dir);
void GsuEscribir(uint16_t dir, uint8_t v);
void GsuEstado(Serial& s);
bool GsuTieneRom();
void GsuContadores(uint64_t* arranques, uint64_t* paradas, uint64_t* instrucciones, uint64_t* ocupado);
int GsuDiagnostico(char* out, int max);
extern uint32_t gsuConsultasSfr;
extern uint32_t diagNmi, diagIrq;   // interrupciones atendidas por el procesador (cpu.cpp)
bool GsuTieneRam();   // el juego escribió en la SRAM (hay que guardarla en el archivo)   // botones de cada jugador (ver SnesMando)
