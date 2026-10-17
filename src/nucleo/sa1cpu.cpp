// El chip SA-1: sus registros y su mapa de memoria (sa1.inc) y su procesador, el mismo 65816 de cpu.cpp compilado otra
// vez con la memoria, el reloj y las interrupciones del SA-1. Corre al doble de velocidad que el de la SNES: un ciclo
// interno son 2 ciclos maestros. Van juntos en un solo archivo para que el compilador pueda integrar los accesos a
// memoria en el procesador (es lo que más se ejecuta en los juegos con este chip).
#include "interno.h"

#include "sa1.inc"

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
