// Datos de la rom cargada, compartidos con el bus de memoria.
#pragma once
#include <stdint.h>

#include "snes.h"

extern uint8_t* romDatos;
extern uint32_t romTam;
extern uint32_t romMascara;   // romTam - 1 si es potencia de 2 (casi siempre); si no, 0
// Posición en la rom de una dirección que puede pasarse del tamaño (se repite, como en el cartucho). Con una máscara en
// lugar de una división: es el acceso más frecuente del emulador.
static inline uint32_t RomPos(uint32_t a) { return romMascara ? (a & romMascara) : (romTam ? a % romTam : 0); }
extern bool romHiRom;
extern bool romSuperFx;
extern const char* romChip;
extern uint8_t sram[128 * 1024];
extern uint32_t sramTam;

bool RomCargar(const char* ruta, InfoRom& info);
// La rom ya en memoria (por ejemplo, sacada de un .zip): 'd' (malloc) queda a cargo del núcleo, también si falla.
bool RomCargarDatos(uint8_t* d, uint32_t tam, InfoRom& info);
