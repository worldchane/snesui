// Ver rebobinar.h. La historia vive en un anillo de bytes: cada entrada es la diferencia comprimida entre un estado y el
// anterior. Se guarda completo solo el último estado ("actual"); para ir atrás se le aplica la diferencia más nueva
// (actual XOR diferencia = estado anterior) y se la saca del anillo. Cuando el anillo se llena, se descartan las más
// viejas.
//
// Compresión de la diferencia: secuencias de [ceros: varint][largo: varint][bytes distintos de cero], que es muy chica
// porque entre dos cuadros casi todos los bytes son iguales (la diferencia da cero).
#include "rebobinar.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "consola.h"

static const uint32_t CAPACIDAD = 16 * 1024 * 1024;
static const int MAX_ENTRADAS = 8192;
static const float SEGUNDOS_POR_ENTRADA = 2 / 60.f;   // se captura cada 2 cuadros

static uint8_t* anillo;
static uint32_t cabeza, usados;   // cabeza = donde empieza la entrada más vieja
struct Entrada { uint32_t pos, largo; };
static Entrada entradas[MAX_ENTRADAS];
static int primera, cuantas;      // índice de la más vieja y cantidad
static uint8_t *actual, *nuevo, *comprimido;
static uint32_t tam;
static bool hayActual;

bool RebobinarIniciar()
{
    uint32_t t = ConsolaActiva().EstadoTam();
    if (!anillo) anillo = (uint8_t*)malloc(CAPACIDAD);
    if (t != tam || !actual)
    {
        free(actual); free(nuevo); free(comprimido);
        tam = t;
        actual = (uint8_t*)malloc(tam);
        nuevo = (uint8_t*)malloc(tam);
        comprimido = (uint8_t*)malloc(tam + tam / 64 + 64);   // el peor caso, sin ceros
    }
    cabeza = usados = 0;
    primera = cuantas = 0;
    hayActual = false;
    return anillo && actual && nuevo && comprimido;
}

static inline uint8_t* Varint(uint8_t* o, uint32_t v)
{
    while (v >= 0x80) { *o++ = (uint8_t)(v | 0x80); v >>= 7; }
    *o++ = (uint8_t)v;
    return o;
}

// Copia desde/hacia el anillo dando la vuelta al final.
static void EscribirAnillo(uint32_t pos, const uint8_t* d, uint32_t n)
{
    uint32_t a = CAPACIDAD - pos < n ? CAPACIDAD - pos : n;
    memcpy(anillo + pos, d, a);
    if (a < n) memcpy(anillo, d + a, n - a);
}
static inline uint8_t ByteAnillo(uint32_t pos) { return anillo[pos % CAPACIDAD]; }

void RebobinarCapturar()
{
    if (!anillo || !actual) return;
    if (!ConsolaActiva().GuardarEstado(nuevo, tam)) return;
    if (!hayActual) { memcpy(actual, nuevo, tam); hayActual = true; return; }

    // Diferencia nuevo XOR actual, comprimida.
    uint8_t* o = comprimido;
    uint32_t i = 0;
    while (i < tam)
    {
        uint32_t z = i;
        while (z < tam && nuevo[z] == actual[z]) z++;
        uint32_t l = z;
        while (l < tam && nuevo[l] != actual[l]) l++;
        o = Varint(o, z - i);
        o = Varint(o, l - z);
        for (uint32_t k = z; k < l; k++) *o++ = nuevo[k] ^ actual[k];
        i = l;
    }
    uint32_t n = (uint32_t)(o - comprimido);
    if (n > CAPACIDAD / 4) return;

    // Hacer lugar descartando las más viejas.
    while (cuantas > 0 && (usados + n > CAPACIDAD || cuantas >= MAX_ENTRADAS))
    {
        Entrada& e = entradas[primera];
        usados -= e.largo;
        cabeza = (e.pos + e.largo) % CAPACIDAD;
        primera = (primera + 1) % MAX_ENTRADAS;
        cuantas--;
    }
    if (cuantas == 0) cabeza = 0, usados = 0;
    uint32_t pos = (cabeza + usados) % CAPACIDAD;
    EscribirAnillo(pos, comprimido, n);
    entradas[(primera + cuantas) % MAX_ENTRADAS] = {pos, n};
    cuantas++;
    usados += n;
    memcpy(actual, nuevo, tam);
}

bool RebobinarAtras()
{
    if (!hayActual) return false;
    if (cuantas == 0)
    {
        // Sin más historia: al menos volver al estado más viejo que quedó.
        ConsolaActiva().CargarEstado(actual, tam);
        return false;
    }
    int ultima = (primera + cuantas - 1) % MAX_ENTRADAS;
    Entrada e = entradas[ultima];
    // Descomprimir aplicando el XOR sobre 'actual': queda el estado anterior.
    uint32_t p = e.pos, fin = e.pos + e.largo, i = 0;
    auto leerVarint = [&]() {
        uint32_t v = 0;
        int s = 0;
        for (;;)
        {
            uint8_t b = ByteAnillo(p++);
            v |= (uint32_t)(b & 0x7F) << s;
            if (!(b & 0x80)) break;
            s += 7;
        }
        return v;
    };
    while (p < fin && i < tam)
    {
        i += leerVarint();
        uint32_t l = leerVarint();
        for (uint32_t k = 0; k < l && i < tam; k++) actual[i++] ^= ByteAnillo(p++);
    }
    cuantas--;
    usados -= e.largo;
    ConsolaActiva().CargarEstado(actual, tam);
    return true;
}

float RebobinarSegundos() { return cuantas * SEGUNDOS_POR_ENTRADA; }
