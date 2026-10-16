// Datos de la rom cargada, compartidos con el bus de memoria.
#pragma once
#include <stdint.h>

#include "snes.h"

extern uint8_t* romDatos;
extern uint32_t romTam;
extern bool romHiRom;
extern bool romSuperFx;
extern const char* romChip;
extern uint8_t sram[128 * 1024];
extern uint32_t sramTam;

bool RomCargar(const char* ruta, InfoRom& info);
