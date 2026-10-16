// El procesador del chip SA-1: el mismo 65816 de cpu.cpp, compilado otra vez con la memoria, el reloj y las
// interrupciones del SA-1 (ver sa1.cpp). Corre al doble de velocidad que el de la SNES: un ciclo interno son 2 ciclos
// maestros.
#include "interno.h"

// Lo que el procesador usa, en su versión del SA-1 (sa1.cpp).
extern uint64_t sa1Ciclos;
extern bool sa1Nmi, sa1Irq;
uint8_t Sa1Leer(uint32_t dir);
void Sa1Escribir(uint32_t dir, uint8_t v);
void Sa1Ciclo(int ciclos);
int Sa1Costo(uint32_t dir);

#define CPU_SA1
#define CICLO_INTERNO 2
#define cpu sa1cpu
#define ciclosMaestros sa1Ciclos
#define BusLeer Sa1Leer
#define BusEscribir Sa1Escribir
#define BusCiclo Sa1Ciclo
#define VelocidadAcceso Sa1Costo
#define CpuReiniciar Sa1CpuReiniciar
#define CpuInterrupcion Sa1CpuInterrupcion
#define CpuCorrerHasta Sa1CpuCorrerHasta
#define CpuEstado Sa1CpuEstado
#define CpuPc Sa1CpuPc
#define diagNmi sa1DiagNmi
#define diagIrq sa1DiagIrq
#define apuLogOn sa1ApuLogOn
#define nmiPendiente sa1Nmi
#define irqLinea sa1Irq
#define irqGsu false
#define irqSa1 false
#define irqReprogramada false

#include "cpu.cpp"
