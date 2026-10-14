// El APU: el subsistema de sonido de la SNES. Tiene su propio procesador (SPC700, de 8 bits) con 64 KB de memoria, su
// ROM de arranque (IPL), dos temporizadores y el DSP (que genera el sonido). El juego le sube su música y efectos por
// cuatro puertos compartidos y después le manda órdenes; por eso el APU tiene que funcionar o muchos juegos ni arrancan.
// Esta versión corre el SPC700 y su ROM real (así el juego sube su programa y avanza); el DSP guarda sus registros y los
// temporizadores andan. La síntesis del sonido en sí se completa en el siguiente paso (por ahora silencio).
#include "interno.h"
#include <stdio.h>

extern uint64_t ciclosMaestros;

static uint8_t aram[64 * 1024];
static uint8_t puerto[4];      // lo que el SPC700 le deja al CPU principal (lee en 0x2140-0x2143)
static uint8_t puertoIn[4];    // lo que el CPU principal le deja al SPC700

// ROM de arranque del SPC700 (IPL), 64 bytes. Es la misma de todas las SNES: recibe el programa del juego por los puertos
// y salta a él. Sin ella el juego no puede subir su música.
static const uint8_t IPL[64] = {
    0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
    0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4, 0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
    0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB, 0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
    0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD, 0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF,
};

// SPC700: registros.
static struct { uint8_t a, x, y, sp, psw; uint16_t pc; uint64_t ciclos; bool iplVisible; } spc;
enum { P_C = 1, P_Z = 2, P_I = 4, P_H = 8, P_B = 16, P_P = 32, P_V = 64, P_N = 128 };

// Temporizadores y DSP.
static uint8_t control;        // 0xF1: habilita temporizadores y ROM
static uint8_t dspDir;
static uint8_t dsp[128];
static struct { uint8_t objetivo, contadorInterno, salida; bool on; int acum; } tmr[3];

#include "dsp.inc"

// ---------- memoria del SPC700 ----------
static uint8_t spcLeer(uint16_t a)
{
    if (a >= 0xFFC0 && spc.iplVisible) return IPL[a - 0xFFC0];
    switch (a)
    {
    case 0xF2: return dspDir;
    case 0xF3: return dsp[dspDir & 0x7F];
    case 0xF4: case 0xF5: case 0xF6: case 0xF7: return puertoIn[a - 0xF4];
    case 0xFD: { uint8_t v = tmr[0].salida & 0xF; tmr[0].salida = 0; return v; }
    case 0xFE: { uint8_t v = tmr[1].salida & 0xF; tmr[1].salida = 0; return v; }
    case 0xFF: { uint8_t v = tmr[2].salida & 0xF; tmr[2].salida = 0; return v; }
    }
    return aram[a];
}

static void spcEscribir(uint16_t a, uint8_t v)
{
    switch (a)
    {
    case 0xF1:
        control = v;
        for (int i = 0; i < 3; i++) if ((v & (1 << i)) && !tmr[i].on) { tmr[i].contadorInterno = 0; tmr[i].salida = 0; }
        for (int i = 0; i < 3; i++) tmr[i].on = v & (1 << i);
        spc.iplVisible = v & 0x80;
        if (v & 0x10) { puertoIn[0] = puertoIn[1] = 0; }   // limpiar puertos 0/1
        if (v & 0x20) { puertoIn[2] = puertoIn[3] = 0; }
        return;
    case 0xF2: dspDir = v; return;
    case 0xF3: if (!(dspDir & 0x80)) DspEscribir(dspDir, v); return;
    case 0xF4: case 0xF5: case 0xF6: case 0xF7: puerto[a - 0xF4] = v; return;   // al CPU principal
    case 0xFA: tmr[0].objetivo = v; return;
    case 0xFB: tmr[1].objetivo = v; return;
    case 0xFC: tmr[2].objetivo = v; return;
    }
    aram[a] = v;
}

static void Temporizadores(int ciclos)
{
    // Los temporizadores 0 y 1 van a 8 kHz; el 2 a 64 kHz. Cuando el contador interno llega al objetivo, sube la salida.
    for (int i = 0; i < 3; i++)
    {
        if (!tmr[i].on) continue;
        tmr[i].acum += ciclos;
        int div = (i == 2) ? 16 : 128;   // ciclos del SPC (1,024 MHz) por paso
        while (tmr[i].acum >= div)
        {
            tmr[i].acum -= div;
            if (++tmr[i].contadorInterno == (tmr[i].objetivo ? tmr[i].objetivo : 256))
            {
                tmr[i].contadorInterno = 0;
                tmr[i].salida = (tmr[i].salida + 1) & 0xF;
            }
        }
    }
}

// ---------- intérprete del SPC700 ----------
#include "spc700.inc"

// Ciclos de cada instrucción del SPC700 (los saltos tomados suman 2 más).
static const uint8_t SPC_CICLOS[256] = {
    2,8,4,5,3,4,3,6,2,6,5,4,5,4,6,8,
    2,8,4,5,4,5,5,6,5,5,6,5,2,2,4,6,
    2,8,4,5,3,4,3,6,2,6,5,4,5,4,5,4,
    2,8,4,5,4,5,5,6,5,5,6,5,2,2,3,8,
    2,8,4,5,3,4,3,6,2,6,4,4,5,4,6,6,
    2,8,4,5,4,5,5,6,5,5,4,5,2,2,4,3,
    2,8,4,5,3,4,3,6,2,6,4,4,5,4,5,5,
    2,8,4,5,4,5,5,6,5,5,5,5,2,2,3,6,
    2,8,4,5,3,4,3,6,2,6,5,4,5,2,4,5,
    2,8,4,5,4,5,5,6,5,5,5,5,2,2,12,5,
    3,8,4,5,3,4,3,6,2,6,4,4,5,2,4,4,
    2,8,4,5,4,5,5,6,5,5,5,5,2,2,3,4,
    3,8,4,5,4,5,4,7,2,5,6,4,5,2,4,9,
    2,8,4,5,5,6,6,7,4,5,5,5,2,2,6,3,
    2,8,4,5,3,4,3,6,2,4,5,3,4,3,4,3,
    2,8,4,5,4,5,5,6,3,4,5,4,2,2,4,3,
};

// Largo de las instrucciones de salto condicional (para saber si el salto se tomó); 0 = no es salto condicional.
static inline int LargoSalto(uint8_t op)
{
    if ((op & 0x1F) == 0x10) return 2;          // BPL BMI BVC BVS BCC BCS BNE BEQ
    if ((op & 0x0F) == 0x03) return 3;          // BBS / BBC
    if (op == 0x2E || op == 0xDE || op == 0x6E) return 3;   // CBNE dp, CBNE dp+X, DBNZ dp
    if (op == 0xFE) return 2;                   // DBNZ Y
    return 0;
}

static void spcCorrerHasta(uint64_t ciclosSpc)
{
    while (spc.ciclos < ciclosSpc)
    {
        uint64_t antes = spc.ciclos;
        uint16_t pc0 = spc.pc;
        uint8_t op = (pc0 >= 0xFFC0 && spc.iplVisible) ? IPL[pc0 - 0xFFC0] : aram[pc0];
        Spc700Paso();
        int c = SPC_CICLOS[op];
        int largo = LargoSalto(op);
        if (largo && spc.pc != (uint16_t)(pc0 + largo)) c += 2;
        spc.ciclos = antes + c;
        Temporizadores(c);
        // El DSP saca una muestra cada 32 ciclos del SPC700.
        acumDsp += c;
        while (acumDsp >= 32) { acumDsp -= 32; DspMuestra(); }
    }
}

// Sincroniza el APU con el reloj maestro (el SPC700 va a 1,024 MHz; el maestro a 21,477 MHz).
static void Sincronizar()
{
    spcCorrerHasta(ciclosMaestros * 1024000ull / 21477272ull);
}

// ---------- lo que ve el CPU principal ----------
uint8_t ApuLeer(uint16_t p)
{
    Sincronizar();
    return puerto[p & 3];
}

void ApuEscribir(uint16_t p, uint8_t v)
{
    Sincronizar();
    puertoIn[p & 3] = v;
}

void ApuReiniciar()
{
    memset(aram, 0, sizeof aram);
    memset(puerto, 0, sizeof puerto);
    memset(puertoIn, 0, sizeof puertoIn);
    memset(dsp, 0, sizeof dsp);
    memset(tmr, 0, sizeof tmr);
    DspReiniciar();
    memset(&spc, 0, sizeof spc);
    spc.sp = 0xEF;
    spc.iplVisible = true;
    spc.pc = 0xFFC0;   // la ROM de arranque empieza acá
    control = 0x80;
    dspDir = 0;
}

int ApuCuadro(int16_t* audio, int maxAudio, bool pal)
{
    (void)pal;
    Sincronizar();
    // Las muestras que generó el DSP durante el cuadro (unas 533 a 60 Hz).
    int n = nMuestras < maxAudio ? nMuestras : maxAudio;
    if (audio) memcpy(audio, muestras, (size_t)n * 2 * sizeof(int16_t));
    nMuestras = 0;
    return n;
}

// Depuración: estado del SPC700 y los puertos.
void ApuVolcado(char* out, int max)
{
    snprintf(out, max, "SPC pc=%04X cyc=%llu  CPU->APU=%02X %02X %02X %02X  APU->CPU=%02X %02X %02X %02X  ipl=%d",
             spc.pc, (unsigned long long)spc.ciclos, puertoIn[0], puertoIn[1], puertoIn[2], puertoIn[3],
             puerto[0], puerto[1], puerto[2], puerto[3], spc.iplVisible);
}

void ApuEstado(Serial& s)
{
    s(aram); s(puerto); s(puertoIn); s(spc); s(control); s(dspDir); s(dsp); s(tmr);
    s(voces); s(ruido); s(contadorRuido); s(ecoPos); s(ecoHist); s(ecoHistPos); s(acumDsp);
    if (s.cargar) nMuestras = 0;
}
