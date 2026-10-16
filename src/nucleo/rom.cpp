// Carga de roms de SNES y lectura de su cabecera interna.
// Una rom puede traer 512 bytes de más al principio (cabecera que agregaban las copiadoras): se quitan.
// El mapeo LoROM/HiROM dice cómo se ve la rom en la memoria; se detecta probando dónde calza mejor la cabecera interna.
#include "rom.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"
#include "interno.h"

uint8_t* romDatos;
uint32_t romTam;
bool romHiRom;
bool romSuperFx;
uint8_t dspMapa;
const char* romChip = "";   // chip especial del cartucho (para el diagnóstico): "Super FX", "DSP-1"...
uint8_t sram[128 * 1024];
uint32_t sramTam;

static int Puntaje(const uint8_t* d, uint32_t tam, uint32_t base)
{
    // Cuánto "parece" una cabecera válida la que está en 'base': nombre legible, y el vector de reinicio apunta a la BIOS
    // del juego (>= 0x8000). Más puntaje = más probable que ese sea el mapeo correcto.
    if (base + 0x40 > tam) return -1;
    const uint8_t* h = d + base;
    int p = 0;
    for (int i = 0; i < 21; i++)
    {
        uint8_t c = h[i];
        if (c >= 0x20 && c < 0x7F) p++;
        else if (c == 0) p += 0;
        else p -= 3;
    }
    uint16_t reset = h[0x3C] | h[0x3D] << 8;
    if (reset >= 0x8000) p += 8;
    uint16_t suma = h[0x1E] | h[0x1F] << 8, comp = h[0x1C] | h[0x1D] << 8;
    if ((suma ^ comp) == 0xFFFF) p += 8;
    return p;
}

static void LeerCabecera(const uint8_t* d, uint32_t tam, uint32_t base, InfoRom& info)
{
    const uint8_t* h = d + base;
    int n = 0;
    for (int i = 0; i < 21; i++)
    {
        uint8_t c = h[i];
        if (c == 0) break;
        info.titulo[n++] = (c >= 0x20 && c < 0x7F) ? (char)c : ' ';
    }
    while (n > 0 && info.titulo[n - 1] == ' ') n--;
    info.titulo[n] = 0;
    snprintf(info.mapeo, sizeof info.mapeo, "%s", (h[0x15] & 1) ? "HiROM" : "LoROM");
    info.tamKB = (int)(tam / 1024);
    uint8_t pais = h[0x19];
    info.pal = pais >= 2 && pais <= 12 && pais != 7;   // Europa/PAL
    uint8_t tipo = h[0x16];
    uint8_t lo = tipo & 0x0F;   // la cifra baja: 1-2 RAM (y batería) sin chip; 4-6 RAM o batería con chip
    info.tieneRam = lo == 1 || lo == 2 || lo == 4 || lo == 5 || lo == 6;
    info.valida = true;
}

bool RomCargar(const char* ruta, InfoRom& info)
{
    memset(&info, 0, sizeof info);
    uint64_t tam64 = 0;
    if (!ExisteArchivo(ruta, &tam64) || tam64 < 0x8000)
    {
        snprintf(info.error, sizeof info.error, "No se pudo abrir la rom o es demasiado chica.");
        return false;
    }
    if (tam64 > 8u * 1024 * 1024) { snprintf(info.error, sizeof info.error, "La rom es demasiado grande (más de 8 MB)."); return false; }
    uint32_t tam = (uint32_t)tam64;
    uint8_t* d = (uint8_t*)malloc(tam);
    FILE* f = AbrirArchivo(ruta, "rb");
    if (!d || !f || fread(d, 1, tam, f) != tam)
    {
        if (f) fclose(f);
        free(d);
        snprintf(info.error, sizeof info.error, "No se pudo leer la rom.");
        return false;
    }
    fclose(f);
    // Quitar la cabecera de copiadora (512 bytes) si la hay.
    if ((tam & 0x3FF) == 512) { memmove(d, d + 512, tam - 512); tam -= 512; }

    // Probar LoROM (cabecera en 0x7FC0) y HiROM (0xFFC0): se queda el de más puntaje.
    int pLo = Puntaje(d, tam, 0x7FC0);
    int pHi = Puntaje(d, tam, 0xFFC0);
    bool hi = pHi > pLo;
    if (pLo < 0 && pHi < 0) { free(d); snprintf(info.error, sizeof info.error, "No parece una rom de SNES (sin cabecera válida)."); return false; }
    LeerCabecera(d, tam, hi ? 0xFFC0 : 0x7FC0, info);

    free(romDatos);
    romDatos = d;
    romTam = tam;
    romHiRom = hi;
    // Tamaño de la SRAM según la cabecera (1 KB << valor). Tiene que ser el exacto: algunos juegos
    // escriben más allá y revisan que se repita, para detectar copiadoras.
    const uint8_t* h = d + (hi ? 0xFFC0 : 0x7FC0);
    uint8_t ramCod = h[0x18];
    sramTam = info.tieneRam && ramCod ? (1024u << (ramCod > 7 ? 7 : ramCod)) : 0;
    memset(sram, 0, sizeof sram);

    // Super FX: mapa LoROM (0x20/0x30) y tipo de cartucho 0x13..0x1A. La RAM del chip va en el mismo búfer que la
    // SRAM; su tamaño sale de la cabecera extendida (0x7FBD) y, si no la hay, 64 KB. Con batería (0x15, 0x1A) se guarda.
    uint8_t tipo = h[0x16];
    bool superfx = !hi && (h[0x15] & 0xEF) == 0x20 && tipo >= 0x13 && tipo <= 0x1A;
    romSuperFx = superfx;
    if (superfx)
    {
        uint8_t exp = (h[0x1A] == 0x33) ? h[-3] : 0;   // 0x7FBD: tamaño de la RAM de expansión
        uint32_t ram = exp >= 1 && exp <= 7 ? (1024u << exp) : 64 * 1024;
        bool bateria = tipo == 0x15 || tipo == 0x1A;
        sramTam = bateria ? ram : 0;
        info.tieneRam = bateria;
        GsuConfigurar(true, ram, tipo == 0x1A || ram > 64 * 1024 || tam > 1024 * 1024);
    }
    else GsuConfigurar(false, 0, false);
    romChip = superfx ? "Super FX" : "";

    // DSP (NEC µPD7725): tipo 0x03..0x05 sin otro coprocesador. Hay cuatro programas distintos; cuál es sale del título
    // (como hace bsnes). El lugar de sus registros depende de la placa: HiROM, LoROM sin RAM, LoROM con RAM.
    dsp1Presente = false;
    // SA-1: coprocesador 3 (tipo 0x34/0x35), mapa 0x23. La BW-RAM es la RAM del cartucho.
    sa1Presente = !superfx && (tipo >> 4) == 3 && (tipo & 0x0F) >= 3;
    if (sa1Presente) romChip = "SA-1";
    // OBC1: coprocesador 2 (tipo 0x25), con 8 KB de RAM a batería.
    obc1Presente = !superfx && (tipo >> 4) == 2 && (tipo & 0x0F) >= 3;
    if (obc1Presente) { romChip = "OBC1"; if (!sramTam) sramTam = 8 * 1024; }
    if (!superfx && (tipo & 0xF0) == 0 && (tipo & 0x0F) >= 3 && (tipo & 0x0F) <= 5)
    {
        // Huella (FNV-1a) del título de la cabecera, sin los espacios del final: identifica los pocos cartuchos que
        // traen otro programa en el chip (DSP-2, DSP-3, DSP-4).
        int largo = 21;
        while (largo > 0 && (h[largo - 1] == ' ' || h[largo - 1] == 0)) largo--;
        uint32_t huella = 0x811C9DC5;
        for (int i = 0; i < largo; i++) huella = (huella ^ h[i]) * 0x01000193;
        if (huella == 0x50AC5A2D) romChip = "DSP-2 (todavía no soportado)";
        else if (huella == 0x84C00A36 || huella == 0xC5DD26D3) romChip = "DSP-4 (todavía no soportado)";
        else if (h[0] == 0xBB && h[1] == 0xC4) romChip = "DSP-3 (todavía no soportado)";
        else
        {
            romChip = "DSP-1";
            dsp1Presente = true;
            if (hi) dspMapa = DSP_HIROM;
            else if (!sramTam) dspMapa = DSP_LOROM_30;
            else dspMapa = tam > 1024 * 1024 ? DSP_LOROM_60 : DSP_LOROM_20;
        }
    }
    return true;
}

void SnesAnalizar(const char* ruta, InfoRom& info)
{
    // Solo la cabecera: los primeros 64 KB (alcanzan para 0x7FC0 y 0xFFC0) más los 512 bytes que agregan algunas
    // copiadoras al principio. No hace falta leer más (con miles de roms en el USB, esto es lo que más tarda).
    memset(&info, 0, sizeof info);
    uint64_t tam64 = 0;
    if (!ExisteArchivo(ruta, &tam64) || tam64 < 0x8000) { snprintf(info.error, sizeof info.error, "rom muy chica"); return; }
    static uint8_t cab[0x10000 + 512 + 1];
    int n = LeerTexto(ruta, (char*)cab, sizeof cab);
    if (n < 0x8000) { snprintf(info.error, sizeof info.error, "no se pudo leer"); return; }
    uint32_t tam = (uint32_t)n;
    // La cabecera de copiadora se reconoce por el tamaño del archivo entero (no por lo leído, que está recortado).
    if ((tam64 & 0x3FF) == 512) { memmove(cab, cab + 512, tam - 512); tam -= 512; }
    int pLo = Puntaje(cab, tam, 0x7FC0), pHi = Puntaje(cab, tam, 0xFFC0);
    if (pLo < 0 && pHi < 0) { snprintf(info.error, sizeof info.error, "no parece una rom de SNES"); return; }
    LeerCabecera(cab, tam, pHi > pLo ? 0xFFC0 : 0x7FC0, info);
    info.tamKB = (int)(tam64 / 1024);   // el tamaño real del archivo
}
