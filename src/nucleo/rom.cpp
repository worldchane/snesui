// Carga de roms de SNES y lectura de su cabecera interna.
// Una rom puede traer 512 bytes de más al principio (cabecera que agregaban las copiadoras): se quitan.
// El mapeo LoROM/HiROM dice cómo se ve la rom en la memoria; se detecta probando dónde calza mejor la cabecera interna.
#include "rom.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"

uint8_t* romDatos;
uint32_t romTam;
bool romHiRom;
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
    info.tieneRam = tipo == 1 || tipo == 2 || tipo == 5 || tipo == 6;
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
    // Tamaño de la SRAM según la cabecera (1 KB << valor). Tiene que ser el exacto: algunos juegos (Donkey Kong Country)
    // escriben más allá y revisan que se repita, para detectar copiadoras.
    uint8_t ramCod = d[(hi ? 0xFFC0 : 0x7FC0) + 0x18];
    sramTam = info.tieneRam && ramCod ? (1024u << (ramCod > 7 ? 7 : ramCod)) : 0;
    memset(sram, 0, sizeof sram);
    return true;
}

void SnesAnalizar(const char* ruta, InfoRom& info)
{
    // Solo la cabecera: se leen los primeros 256 KB (alcanzan para 0x7FC0 y 0xFFC0), sin cargar toda la rom.
    memset(&info, 0, sizeof info);
    uint64_t tam64 = 0;
    if (!ExisteArchivo(ruta, &tam64) || tam64 < 0x8000) { snprintf(info.error, sizeof info.error, "rom muy chica"); return; }
    static uint8_t cab[256 * 1024];
    int n = LeerTexto(ruta, (char*)cab, sizeof cab);
    if (n < 0x8000) { snprintf(info.error, sizeof info.error, "no se pudo leer"); return; }
    uint32_t tam = (uint32_t)n;
    if ((tam & 0x3FF) == 512) { memmove(cab, cab + 512, tam - 512); tam -= 512; }
    int pLo = Puntaje(cab, tam, 0x7FC0), pHi = Puntaje(cab, tam, 0xFFC0);
    if (pLo < 0 && pHi < 0) { snprintf(info.error, sizeof info.error, "no parece una rom de SNES"); return; }
    LeerCabecera(cab, tam, pHi > pLo ? 0xFFC0 : 0x7FC0, info);
    info.tamKB = (int)(tam64 / 1024);   // el tamaño real del archivo
}
