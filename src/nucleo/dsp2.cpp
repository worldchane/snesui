// El DSP-2: el mismo chip NEC del DSP-1 con otro programa, que ayuda a dibujar con mapas de bits de 4 bits por pixel
// (pasar a tiles, superponer con un color transparente, dar vuelta, escalar) y multiplica. La SNES le escribe un
// comando y sus datos por el registro de datos y lee el resultado por el mismo registro.
// Se emula lo que calcula cada comando (como snes9x), según la investigación de Overload y Kris Bleakley. El escalado
// usa el mismo punto fijo que el chip.
#include "interno.h"

#include <string.h>

bool dsp2Presente;

static struct
{
    bool esperaComando;
    uint8_t comando;
    int entrada, cuantosEntran;     // datos recibidos y cuántos hacen falta
    int salida, cuantosSalen;
    uint8_t datos[512], resultado[512];
    uint8_t transparente;
    bool tieneLargo5, tieneLargo6, tieneLargoD;
    int largo5, largo6, largoEntD, largoSalD;
} d;

// 01: 8 filas de 8 pixeles (4 bits, de a 2 por byte) a un tile de 4 planos.
static void ATile()
{
    const uint8_t* p = d.datos;
    uint8_t* a = d.resultado;        // planos 0 y 1
    uint8_t* b = d.resultado + 16;   // planos 2 y 3
    for (int fila = 0; fila < 8; fila++, p += 4)
    {
        for (int plano = 0; plano < 4; plano++)
        {
            // Bit 'plano' de los 8 pixeles de la fila: en cada byte, el pixel alto (bit plano+4) y el bajo (bit plano).
            uint8_t v = 0;
            for (int k = 0; k < 4; k++)
            {
                v |= (uint8_t)((p[k] >> (plano + 4) & 1) << (7 - 2 * k));
                v |= (uint8_t)((p[k] >> plano & 1) << (6 - 2 * k));
            }
            if (plano < 2) a[fila * 2 + plano] = v;
            else b[fila * 2 + plano - 2] = v;
        }
    }
}

// 05: superponer el segundo mapa sobre el primero; donde el segundo tiene el color transparente se ve el primero.
static void Superponer()
{
    uint8_t c = d.transparente & 0x0F;
    const uint8_t* p1 = d.datos;
    const uint8_t* p2 = d.datos + d.largo5;
    for (int i = 0; i < d.largo5; i++)
    {
        uint8_t a = p1[i], b = p2[i];
        d.resultado[i] = (uint8_t)((((b >> 4) == c) ? (a & 0xF0) : (b & 0xF0)) | (((b & 0x0F) == c) ? (a & 0x0F) : (b & 0x0F)));
    }
}

// 06: dar vuelta un mapa de izquierda a derecha.
static void DarVuelta()
{
    for (int i = 0, j = d.largo6 - 1; i < d.largo6; i++, j--)
        d.resultado[j] = (uint8_t)(d.datos[i] << 4 | d.datos[i] >> 4);
}

// 09: producto de dos números de 16 bits.
static void Multiplicar()
{
    uint32_t r = (uint32_t)(d.datos[0] | d.datos[1] << 8) * (uint32_t)(d.datos[2] | d.datos[3] << 8);
    for (int i = 0; i < 4; i++) d.resultado[i] = (uint8_t)(r >> 8 * i);
}

// 0D: escalar una fila de pixeles de largoEntD a largoSalD (en pixeles), con el punto fijo del chip (16 bits de fracción).
static void Escalar()
{
    uint32_t paso = d.largoEntD <= d.largoSalD ? 0x10000u
                                               : ((uint32_t)d.largoEntD << 17) / (((uint32_t)d.largoSalD << 1) + 1);
    uint8_t px[512];
    uint32_t pos = 0;
    for (int i = 0; i < d.largoSalD * 2; i++, pos += paso)
    {
        int j = (int)(pos >> 16);
        px[i] = (j & 1) ? (d.datos[(j >> 1) & 511] & 0x0F) : (d.datos[(j >> 1) & 511] >> 4);
    }
    for (int i = 0; i < d.largoSalD; i++) d.resultado[i] = (uint8_t)(px[i * 2] << 4 | px[i * 2 + 1]);
}

void Dsp2Reiniciar()
{
    memset(&d, 0, sizeof d);
    d.esperaComando = true;
}

uint8_t Dsp2Leer(bool estado)
{
    if (estado) return 0x80;   // siempre listo
    if (!d.cuantosSalen) return 0xFF;
    uint8_t v = d.resultado[d.salida++ & 511];
    if (d.salida >= d.cuantosSalen) d.cuantosSalen = 0;
    return v;
}

void Dsp2Escribir(bool estado, uint8_t v)
{
    if (estado) return;
    if (d.esperaComando)
    {
        d.comando = v;
        d.entrada = 0;
        d.esperaComando = false;
        switch (v)
        {
        case 0x01: d.cuantosEntran = 32; break;
        case 0x03: case 0x05: case 0x06: d.cuantosEntran = 1; break;
        case 0x09: d.cuantosEntran = 4; break;
        case 0x0D: d.cuantosEntran = 2; break;
        default: d.cuantosEntran = 0; break;
        }
    }
    else d.datos[d.entrada++ & 511] = v;
    if (d.entrada != d.cuantosEntran) return;

    // Llegaron todos los datos: se corre el comando (los de largo variable reciben primero el largo).
    d.esperaComando = true;
    d.salida = 0;
    switch (d.comando)
    {
    case 0x01: d.cuantosSalen = 32; ATile(); break;
    case 0x03: d.transparente = d.datos[0]; break;
    case 0x05:
        if (d.tieneLargo5) { d.tieneLargo5 = false; d.cuantosSalen = d.largo5; Superponer(); }
        else
        {
            d.largo5 = d.datos[0];
            d.entrada = 0;
            d.cuantosEntran = 2 * d.largo5;
            d.tieneLargo5 = true;
            if (v) d.esperaComando = false;
        }
        break;
    case 0x06:
        if (d.tieneLargo6) { d.tieneLargo6 = false; d.cuantosSalen = d.largo6; DarVuelta(); }
        else
        {
            d.largo6 = d.datos[0];
            d.entrada = 0;
            d.cuantosEntran = d.largo6;
            d.tieneLargo6 = true;
            if (v) d.esperaComando = false;
        }
        break;
    case 0x09: d.cuantosSalen = 4; Multiplicar(); break;
    case 0x0D:
        if (d.tieneLargoD) { d.tieneLargoD = false; d.cuantosSalen = d.largoSalD; Escalar(); }
        else
        {
            d.largoEntD = d.datos[0];
            d.largoSalD = d.datos[1];
            d.entrada = 0;
            d.cuantosEntran = (d.largoEntD + 1) >> 1;
            d.tieneLargoD = true;
            if (v) d.esperaComando = false;
        }
        break;
    }
}

void Dsp2Estado(Serial& s) { s(d); }
