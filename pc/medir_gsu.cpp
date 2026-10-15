// Peor caso del Super FX: el chip corriendo sin parar todo el cuadro (dibujo, memoria y cálculo), a 21 MHz, para medir
// cuánto tarda la emulación por cuadro. Uso: medir_gsu
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/nucleo/interno.h"
#include "../src/nucleo/rom.h"
#include "../src/comun/plataforma.h"

bool ExisteArchivo(const char*, uint64_t*) { return false; }
FILE* AbrirArchivo(const char* r, const char* m) { return fopen(r, m); }
int LeerTexto(const char*, char*, int) { return -1; }

int main()
{
    romTam = 1024 * 1024;
    romDatos = (uint8_t*)calloc(1, romTam);
    // Programa en 00:8000 (desde la caché): bucle de PLOT, suma, guardado en RAM, multiplicación y salto.
    const uint8_t prog[] = {
        0x02,                    // CACHE
        0xA4, 0x00,              // IBT R4,#0
        0x4C,                    // loop: PLOT
        0x23, 0x53,              // WITH R3: ADD R3
        0x34,                    // STW (R4)
        0xD4,                    // INC R4
        0x22, 0x83,              // WITH R2: MULT R3
        0x05, (uint8_t)-9,       // BRA loop
        0x01,                    // NOP (hueco del salto)
    };
    memcpy(romDatos, prog, sizeof prog);
    GsuConfigurar(true, 64 * 1024, true);
    GsuEscribir(0x3034, 0);      // PBR
    GsuEscribir(0x3037, 0xA0);   // CFGR: sin IRQ, multiplicación rápida
    GsuEscribir(0x3039, 1);      // CLSR: 21 MHz
    GsuEscribir(0x303A, 0x19);   // SCMR: 4 bpp, alto 128, la rom y la RAM son del chip
    GsuEscribir(0x301E, 0x00);
    GsuEscribir(0x301F, 0x80);   // R15 = 0x8000: arranca
    const uint64_t CUADRO = 357366;   // ciclos maestros por cuadro (NTSC)
    auto t0 = std::chrono::steady_clock::now();
    uint64_t t = 0;
    for (int i = 0; i < 600; i++) { t += CUADRO; GsuCorrerHasta(t); }
    printf("SFR=%02X%02X R15=%02X%02X R4=%02X%02X\n", GsuLeer(0x3031), GsuLeer(0x3030), GsuLeer(0x301F), GsuLeer(0x301E), GsuLeer(0x3009), GsuLeer(0x3008));
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("600 cuadros con el Super FX a pleno (21 MHz): %.2f s  ->  %.2f ms por cuadro\n", s, s * 1000 / 600);
    return 0;
}
