// El chip DSP-1 (NEC µPD7725 con el programa de Nintendo) que traen algunos cartuchos: hace la matemática 3D
// (perspectiva del suelo en modo 7, rotaciones, proyección de objetos). La SNES le escribe un comando y sus datos por
// un registro de 8 bits (DR) y lee los resultados; el registro de estado (SR) dice si está listo.
//
// Se emula lo que calcula cada comando (como bsnes y snes9x cuando no tienen el programa del chip), con la misma
// aritmética de 16 bits que el chip, siguiendo la investigación de Overload, The Dumper, Neviksti y Andreas Naive.
// Las tablas que usa el chip (seno, recíprocos, raíces, potencias de 2 y coeficientes) se calculan con fórmulas al
// arrancar: dan los mismos valores que las del chip, sin copiar sus datos.
#include "interno.h"

#include <math.h>
#include <string.h>

bool dsp1Presente;

// ---------- tablas ----------
static int16_t tablaSeno[256];      // 32768·sin(2π·i/256), truncado (el mínimo, -32767)
static int16_t tablaMul[256];       // i·π, truncado: la pendiente entre dos puntos de la tabla de seno
static int16_t tablaPot[0x41];      // [0x22+k] = 2^k (k 0..14), [0x31] = 32767, [0x32+k] = 2^(14-k)
static int16_t tablaInversa[128];   // primera aproximación de 1/x para x en [0.5, 1)
static int16_t tablaRaiz[65];       // √(p/64)·32767
static int16_t taylor[5];           // [0] 5/24·(π/4)^4, [1] (π/4)^2/2, [3] π/4, [4] (π/4)^3/3
// Límite del ángulo cenital según el exponente de la altura (constantes del chip).
static const int16_t MAX_AZS[16] = {0x38b4, 0x38b7, 0x38ba, 0x38be, 0x38c0, 0x38c4, 0x38c7, 0x38ca,
                                    0x38ce, 0x38d0, 0x38d4, 0x38d7, 0x38da, 0x38dd, 0x38e0, 0x38e4};

static void PrepararTablas()
{
    static bool listo;
    if (listo) return;
    listo = true;
    for (int i = 0; i < 256; i++)
    {
        double s = 32768.0 * ::sin(2 * M_PI * i / 256);
        int v = (int)s;   // truncado hacia cero
        tablaSeno[i] = (int16_t)(v < -32767 ? -32767 : v > 32767 ? 32767 : v);
        tablaMul[i] = (int16_t)(int)(i * M_PI + 1e-4);   // (113·π y 226·π quedan a una cienmilésima del entero)
    }
    for (int k = 0; k <= 14; k++) { tablaPot[0x22 + k] = (int16_t)(1 << k); tablaPot[0x32 + k] = (int16_t)(1 << (14 - k)); }
    tablaPot[0x31] = 32767;
    for (int k = 0; k < 128; k++)
    {
        double v = floor(536870912.0 / (0x4000 + 128 * k) + 0.5);
        tablaInversa[k] = (int16_t)(v > 32767 ? 32767 : v);
    }
    for (int p = 0; p < 65; p++)
    {
        double v = floor(sqrt(p / 64.0) * 32767 + 1e-4);
        tablaRaiz[p] = (int16_t)(v > 32767 ? 32767 : v);
    }
    double c = M_PI / 4;
    taylor[0] = (int16_t)floor(32768 * 5.0 / 24 * c * c * c * c + 0.5);
    taylor[1] = (int16_t)floor(32768 * c * c / 2 + 0.5);
    taylor[3] = (int16_t)floor(32768 * c + 0.5);
    taylor[4] = (int16_t)floor(32768 * c * c * c / 3 + 0.5);
}

// ---------- matemática del chip (aritmética de 16 bits, como el µPD7725) ----------
static int16_t Seno(int16_t a)
{
    if (a < 0) { if (a == -32768) return 0; return (int16_t)-Seno((int16_t)-a); }
    int32_t s = tablaSeno[a >> 8] + (tablaMul[a & 0xff] * tablaSeno[0x40 + (a >> 8)] >> 15);
    return (int16_t)(s > 32767 ? 32767 : s);
}

static int16_t Coseno(int16_t a)
{
    if (a < 0) { if (a == -32768) return -32768; a = (int16_t)-a; }
    int32_t s = tablaSeno[0x40 + (a >> 8)] - (tablaMul[a & 0xff] * tablaSeno[a >> 8] >> 15);
    return (int16_t)(s < -32768 ? -32767 : s);
}

// 1/(c·2^e) como coeficiente normalizado (entre 1/2 y 1) y exponente: aproximación de la tabla y dos pasos de Newton.
static void Inversa(int16_t c, int16_t e, int16_t& ic, int16_t& ie)
{
    if (c == 0) { ic = 0x7fff; ie = 0x002f; return; }
    int16_t signo = 1;
    if (c < 0) { if (c < -32767) c = -32767; c = (int16_t)-c; signo = -1; }
    while (c < 0x4000) { c = (int16_t)(c << 1); e--; }
    if (c == 0x4000)
    {
        // (así lo hace el chip: con signo positivo el resultado es 0x7fff; con negativo, -0x4000 y un exponente más)
        if (signo == 1) ic = 0x7fff;
        else { ic = -0x4000; e--; }
    }
    else
    {
        int16_t i = tablaInversa[(c - 0x4000) >> 7];
        i = (int16_t)((int16_t)(i + (-i * (c * i >> 15) >> 15)) << 1);
        i = (int16_t)((int16_t)(i + (-i * (c * i >> 15) >> 15)) << 1);
        ic = (int16_t)(i * signo);
    }
    ie = (int16_t)(1 - e);
}

static int16_t DesnormalizarRecortar(int16_t c, int16_t e)
{
    if (e > 0) { if (c > 0) return 32767; if (c < 0) return -32767; return c; }
    if (e < 0) return 0x31 + e < 0 ? 0 : (int16_t)(c * tablaPot[0x31 + e] >> 15);
    return c;
}

// m (entre -1 y 1) como c·2^e con |c| >= 1/2.
static void Normalizar(int16_t m, int16_t& c, int16_t& e)
{
    int16_t i = 0x4000, n = 0;
    if (m < 0) while ((m & i) && i) { i >>= 1; n++; }
    else while (!(m & i) && i) { i >>= 1; n++; }
    c = n > 0 ? (int16_t)(m * tablaPot[0x21 + n] << 1) : m;
    e = (int16_t)(e - n);
}

static void NormalizarDoble(int32_t producto, int16_t& c, int16_t& e)
{
    int16_t n = (int16_t)(producto & 0x7fff), m = (int16_t)(producto >> 15);
    int16_t i = 0x4000, k = 0;
    if (m < 0) while ((m & i) && i) { i >>= 1; k++; }
    else while (!(m & i) && i) { i >>= 1; k++; }
    if (k > 0)
    {
        c = (int16_t)(m * tablaPot[0x21 + k] << 1);
        if (k < 15) c = (int16_t)(c + (n * tablaPot[0x40 - k] >> 15));
        else
        {
            i = 0x4000;
            if (m < 0) while ((n & i) && i) { i >>= 1; k++; }
            else while (!(n & i) && i) { i >>= 1; k++; }
            if (k > 15) c = (int16_t)(n * tablaPot[0x12 + k] << 1);
            else c = (int16_t)(c + n);
        }
    }
    else c = m;
    e = k;
}

static int16_t DesplazarDerecha(int16_t c, int16_t e) { return 0x31 + e < 0 || 0x31 + e > 0x40 ? 0 : (int16_t)(c * tablaPot[0x31 + e] >> 15); }

// ---------- estado del chip ----------
static struct
{
    // Comunicación
    uint8_t sr;          // estado: RQM (listo), DRS (falta el byte alto), DRC (8 bits)
    uint16_t dr;         // registro de datos
    uint8_t fase;        // 0 esperando comando, 1 leyendo datos, 2 escribiendo resultados
    uint8_t comando;
    bool congelado;      // comandos 0x1a/0x2a/0x3a: el chip se queda sin responder
    int16_t contador;
    int16_t entrada[7], salida[16];
    // Matrices de actitud y datos de la proyección (los usan los comandos siguientes)
    int16_t matA[3][3], matB[3][3], matC[3][3];
    int16_t centroX, centroY, centroZ, centroZ_C, centroZ_E;
    int16_t vOffset, les, c_les, e_les;
    int16_t senAas, cosAas, senAzs, cosAzs, senAZS, cosAZS;
    int16_t secAZS_C1, secAZS_E1, secAZS_C2, secAZS_E2;
    int16_t nx, ny, nz, gx, gy, gz, hx, hy, vx, vy, vz;
} d;

enum { SR_DRC = 0x04, SR_DRS = 0x10, SR_RQM = 0x80 };

// ---------- comandos ----------
static void Multiplicar(int16_t* in, int16_t* out) { out[0] = (int16_t)(in[0] * in[1] >> 15); }
static void Multiplicar2(int16_t* in, int16_t* out) { out[0] = (int16_t)((in[0] * in[1] >> 15) + 1); }
static void CmdInversa(int16_t* in, int16_t* out) { Inversa(in[0], in[1], out[0], out[1]); }

static void Triangulo(int16_t* in, int16_t* out)
{
    out[0] = (int16_t)(Seno(in[0]) * in[1] >> 15);
    out[1] = (int16_t)(Coseno(in[0]) * in[1] >> 15);
}

static void Radio(int16_t* in, int16_t* out)
{
    int32_t r = (in[0] * in[0] + in[1] * in[1] + in[2] * in[2]) << 1;
    out[0] = (int16_t)r;
    out[1] = (int16_t)(r >> 16);
}

static void Rango(int16_t* in, int16_t* out) { out[0] = (int16_t)((in[0] * in[0] + in[1] * in[1] + in[2] * in[2] - in[3] * in[3]) >> 15); }
static void Rango2(int16_t* in, int16_t* out) { out[0] = (int16_t)(((in[0] * in[0] + in[1] * in[1] + in[2] * in[2] - in[3] * in[3]) >> 15) + 1); }

static void Distancia(int16_t* in, int16_t* out)
{
    int32_t r = in[0] * in[0] + in[1] * in[1] + in[2] * in[2];
    if (r == 0) { out[0] = 0; return; }
    int16_t c, e;
    NormalizarDoble(r, c, e);
    if (e & 1) c = (int16_t)(c * 0x4000 >> 15);
    int16_t pos = (int16_t)(c * 0x0040 >> 15);
    int16_t n1 = tablaRaiz[pos], n2 = tablaRaiz[pos + 1];
    int16_t dist = (int16_t)(((n2 - n1) * (c & 0x1ff) >> 9) + n1);
    out[0] = (int16_t)(dist >> (e >> 1));
}

static void Rotar(int16_t* in, int16_t* out)
{
    int16_t s = Seno(in[0]), c = Coseno(in[0]);
    out[0] = (int16_t)((in[2] * s >> 15) + (in[1] * c >> 15));
    out[1] = (int16_t)((in[2] * c >> 15) - (in[1] * s >> 15));
}

static void Polar(int16_t* in, int16_t* out)
{
    int16_t az = in[0], ay = in[1], ax = in[2], x1 = in[3], y1 = in[4], z1 = in[5];
    int16_t x = (int16_t)((y1 * Seno(az) >> 15) + (x1 * Coseno(az) >> 15));
    int16_t y = (int16_t)((y1 * Coseno(az) >> 15) - (x1 * Seno(az) >> 15));
    x1 = x; y1 = y;
    int16_t z = (int16_t)((x1 * Seno(ay) >> 15) + (z1 * Coseno(ay) >> 15));
    x = (int16_t)((x1 * Coseno(ay) >> 15) - (z1 * Seno(ay) >> 15));
    out[0] = x; z1 = z;
    y = (int16_t)((z1 * Seno(ax) >> 15) + (y1 * Coseno(ax) >> 15));
    z = (int16_t)((z1 * Coseno(ax) >> 15) - (y1 * Seno(ax) >> 15));
    out[1] = y; out[2] = z;
}

// Matriz de actitud (rotación Z, luego Y, luego X) por la escala S/2. Hay tres (A, B y C) para tres objetos.
static void Actitud(int16_t* in, int16_t m[3][3])
{
    int16_t s = (int16_t)(in[0] >> 1);
    int16_t sz = Seno(in[1]), cz = Coseno(in[1]), sy = Seno(in[2]), cy = Coseno(in[2]), sx = Seno(in[3]), cx = Coseno(in[3]);
    int scz = s * cz >> 15, ssz = s * sz >> 15;   // (los productos intermedios no se truncan a 16 bits)
    m[0][0] = (int16_t)(scz * cy >> 15);
    m[0][1] = (int16_t)((ssz * cx >> 15) + ((scz * sx >> 15) * sy >> 15));
    m[0][2] = (int16_t)((ssz * sx >> 15) - ((scz * cx >> 15) * sy >> 15));
    m[1][0] = (int16_t)-(ssz * cy >> 15);
    m[1][1] = (int16_t)((scz * cx >> 15) - ((ssz * sx >> 15) * sy >> 15));
    m[1][2] = (int16_t)((scz * sx >> 15) + ((ssz * cx >> 15) * sy >> 15));
    m[2][0] = (int16_t)(s * sy >> 15);
    m[2][1] = (int16_t)-((s * sx >> 15) * cy >> 15);
    m[2][2] = (int16_t)((s * cx >> 15) * cy >> 15);
}
static void ActitudA(int16_t* in, int16_t*) { Actitud(in, d.matA); }
static void ActitudB(int16_t* in, int16_t*) { Actitud(in, d.matB); }
static void ActitudC(int16_t* in, int16_t*) { Actitud(in, d.matC); }

// Coordenadas globales → del objeto (con la matriz traspuesta), y al revés.
static void Objetivo(int16_t* in, int16_t* out, int16_t m[3][3])
{
    for (int j = 0; j < 3; j++)
        out[j] = (int16_t)((m[0][j] * in[0] >> 15) + (m[1][j] * in[1] >> 15) + (m[2][j] * in[2] >> 15));
}
static void Subjetivo(int16_t* in, int16_t* out, int16_t m[3][3])
{
    for (int j = 0; j < 3; j++)
        out[j] = (int16_t)((m[j][0] * in[0] >> 15) + (m[j][1] * in[1] >> 15) + (m[j][2] * in[2] >> 15));
}
static void ObjetivoA(int16_t* i, int16_t* o) { Objetivo(i, o, d.matA); }
static void ObjetivoB(int16_t* i, int16_t* o) { Objetivo(i, o, d.matB); }
static void ObjetivoC(int16_t* i, int16_t* o) { Objetivo(i, o, d.matC); }
static void SubjetivoA(int16_t* i, int16_t* o) { Subjetivo(i, o, d.matA); }
static void SubjetivoB(int16_t* i, int16_t* o) { Subjetivo(i, o, d.matB); }
static void SubjetivoC(int16_t* i, int16_t* o) { Subjetivo(i, o, d.matC); }

// Producto con la primera columna de la matriz (la dirección "adelante" del objeto).
static void Escalar(int16_t* in, int16_t* out, int16_t m[3][3]) { out[0] = (int16_t)((in[0] * m[0][0] + in[1] * m[1][0] + in[2] * m[2][0]) >> 15); }
static void EscalarA(int16_t* i, int16_t* o) { Escalar(i, o, d.matA); }
static void EscalarB(int16_t* i, int16_t* o) { Escalar(i, o, d.matB); }
static void EscalarC(int16_t* i, int16_t* o) { Escalar(i, o, d.matC); }

// Nuevos ángulos de actitud después de girar un poco (DeltaU, DeltaF, DeltaL).
static void Girar(int16_t* in, int16_t* out)
{
    int16_t az = in[0], ax = in[1], ay = in[2], u = in[3], f = in[4], l = in[5];
    int16_t cSec, eSec, cSin, c, e;
    int16_t sAy = Seno(ay), cAy = Coseno(ay);
    Inversa(Coseno(ax), 0, cSec, eSec);
    // Alrededor de Z
    NormalizarDoble(u * cAy - f * sAy, c, e);
    e = (int16_t)(eSec - e);
    Normalizar((int16_t)(c * cSec >> 15), c, e);
    out[0] = (int16_t)(az + DesnormalizarRecortar(c, e));
    // Alrededor de X
    out[1] = (int16_t)(ax + (u * sAy >> 15) + (f * cAy >> 15));
    // Alrededor de Y
    NormalizarDoble(u * cAy + f * sAy, c, e);
    e = (int16_t)(eSec - e);
    Normalizar(Seno(ax), cSin, e);
    Normalizar((int16_t)-(c * (cSec * cSin >> 15) >> 15), c, e);
    out[2] = (int16_t)(ay + DesnormalizarRecortar(c, e) + l);
}

// Prepara la proyección (la cámara del suelo en modo 7): devuelve la línea del horizonte, el centro imaginario y el
// punto del suelo en el centro de la pantalla; guarda lo que usan Raster, Objetivo y Proyectar.
static void Parametro(int16_t* in, int16_t* out)
{
    int16_t fx = in[0], fy = in[1], fz = in[2], lfe = in[3], les = in[4], aas = in[5], azs = in[6];
    int16_t cSec, c, e;
    int16_t azsRec = azs;   // ángulo cenital recortado

    d.les = les;
    d.e_les = 0;
    Normalizar(les, d.c_les, d.e_les);

    d.senAas = Seno(aas); d.cosAas = Coseno(aas);
    d.senAzs = Seno(azs); d.cosAzs = Coseno(azs);

    // Vector normal a la pantalla, horizontal y vertical
    d.nx = (int16_t)(d.senAzs * -d.senAas >> 15);
    d.ny = (int16_t)(d.senAzs * d.cosAas >> 15);
    d.nz = (int16_t)(d.cosAzs * 0x7fff >> 15);
    d.hx = (int16_t)(d.cosAas * 0x7fff >> 15);
    d.hy = (int16_t)(d.senAas * 0x7fff >> 15);
    d.vx = (int16_t)(d.cosAzs * -d.senAas >> 15);
    d.vy = (int16_t)(d.cosAzs * d.cosAas >> 15);
    d.vz = (int16_t)(-d.senAzs * 0x7fff >> 15);

    // Centro de proyección y centro de la pantalla
    d.centroX = (int16_t)(fx + (int16_t)(lfe * d.nx >> 15));
    d.centroY = (int16_t)(fy + (int16_t)(lfe * d.ny >> 15));
    d.centroZ = (int16_t)(fz + (int16_t)(lfe * d.nz >> 15));
    d.gx = (int16_t)(d.centroX - (int16_t)(les * d.nx >> 15));
    d.gy = (int16_t)(d.centroY - (int16_t)(les * d.ny >> 15));
    d.gz = (int16_t)(d.centroZ - (int16_t)(les * d.nz >> 15));

    e = 0;
    Normalizar(d.centroZ, c, e);
    d.centroZ_C = c;
    d.centroZ_E = e;

    // Recorte del ángulo cenital
    int16_t maxAzs = MAX_AZS[-e];
    if (azsRec < 0) { maxAzs = (int16_t)-maxAzs; if (azsRec < maxAzs + 1) azsRec = (int16_t)(maxAzs + 1); }
    else if (azsRec > maxAzs) azsRec = maxAzs;

    d.senAZS = Seno(azsRec);
    d.cosAZS = Coseno(azsRec);

    // Separación del punto del suelo en el centro de la pantalla
    Inversa(d.cosAZS, 0, d.secAZS_C1, d.secAZS_E1);
    Normalizar((int16_t)(c * d.secAZS_C1 >> 15), c, e);
    e = (int16_t)(e + d.secAZS_E1);
    c = (int16_t)(DesnormalizarRecortar(c, e) * d.senAZS >> 15);
    d.centroX = (int16_t)(d.centroX + (c * d.senAas >> 15));
    d.centroY = (int16_t)(d.centroY - (c * d.cosAas >> 15));
    out[2] = d.centroX;
    out[3] = d.centroY;

    // Línea del centro imaginario y corrección cuando el ángulo quedó recortado
    int16_t vof = 0;
    if (azs != azsRec || azs == maxAzs)
    {
        if (azs == -32768) azs = -32767;
        c = (int16_t)(azs - maxAzs);
        if (c >= 0) c--;
        int16_t aux = (int16_t)~(c << 2);
        c = (int16_t)(aux * taylor[4] >> 15);
        c = (int16_t)((c * aux >> 15) + taylor[3]);
        vof = (int16_t)(vof - ((c * aux >> 15) * les >> 15));
        c = (int16_t)(aux * aux >> 15);
        aux = (int16_t)((c * taylor[0] >> 15) + taylor[1]);
        d.cosAZS = (int16_t)(d.cosAZS + ((c * aux >> 15) * d.cosAZS >> 15));
    }
    out[0] = vof;

    d.vOffset = (int16_t)(les * d.cosAZS >> 15);

    // Línea del horizonte
    Inversa(d.senAZS, 0, cSec, e);
    Normalizar(d.vOffset, c, e);
    Normalizar((int16_t)(c * cSec >> 15), c, e);
    if (c == -32768) { c >>= 1; e++; }
    out[1] = DesnormalizarRecortar((int16_t)-c, e);

    Inversa(d.cosAZS, 0, d.secAZS_C2, d.secAZS_E2);
}

// Matriz del modo 7 para la línea de pantalla Vs (se pide línea por línea, en modo continuo).
static void Raster(int16_t* in, int16_t* out)
{
    int16_t c, e, c1, e1;
    Inversa((int16_t)((in[0] * d.senAzs >> 15) + d.vOffset), 7, c, e);
    e = (int16_t)(e + d.centroZ_E);
    c1 = (int16_t)(c * d.centroZ_C >> 15);
    e1 = (int16_t)(e + d.secAZS_E2);
    Normalizar(c1, c, e);
    c = DesnormalizarRecortar(c, e);
    out[0] = (int16_t)(c * d.cosAas >> 15);
    out[2] = (int16_t)(c * d.senAas >> 15);
    Normalizar((int16_t)(c1 * d.secAZS_C2 >> 15), c, e1);
    c = DesnormalizarRecortar(c, e1);
    out[1] = (int16_t)(c * -d.senAas >> 15);
    out[3] = (int16_t)(c * d.cosAas >> 15);
}

// Punto del suelo que se ve en el punto (H, V) de la pantalla.
static void Blanco(int16_t* in, int16_t* out)
{
    int16_t h = in[0], v = in[1];
    int16_t c, e, c1, e1;
    Inversa((int16_t)((v * d.senAzs >> 15) + d.vOffset), 8, c, e);
    e = (int16_t)(e + d.centroZ_E);
    c1 = (int16_t)(c * d.centroZ_C >> 15);
    e1 = (int16_t)(e + d.secAZS_E1);
    h = (int16_t)(h << 8);
    Normalizar(c1, c, e);
    c = (int16_t)(DesnormalizarRecortar(c, e) * h >> 15);
    out[0] = (int16_t)(d.centroX + (c * d.cosAas >> 15));
    out[1] = (int16_t)(d.centroY - (c * d.senAas >> 15));
    v = (int16_t)(v << 8);
    Normalizar((int16_t)(c1 * d.secAZS_C1 >> 15), c, e1);
    c = (int16_t)(DesnormalizarRecortar(c, e1) * v >> 15);
    out[0] = (int16_t)(out[0] + (c * -d.senAas >> 15));
    out[1] = (int16_t)(out[1] + (c * d.cosAas >> 15));
}

// Posición en pantalla (H, V) y tamaño (M, 0x100 = 1:1) de un objeto en (X, Y, Z).
static void Proyectar(int16_t* in, int16_t* out)
{
    int16_t e4 = 0, ey = 0, e3 = 0, e2 = 0;
    int16_t px, py, pz;
    NormalizarDoble((int32_t)in[0] - d.gx, px, e4);
    NormalizarDoble((int32_t)in[1] - d.gy, py, ey);
    NormalizarDoble((int32_t)in[2] - d.gz, pz, e3);
    px >>= 1; e4--;   // para que los productos no desborden
    py >>= 1; ey--;
    pz >>= 1; e3--;
    int16_t refE = ey < e3 ? ey : e3;
    if (e4 < refE) refE = e4;
    px = DesplazarDerecha(px, (int16_t)(e4 - refE));
    py = DesplazarDerecha(py, (int16_t)(ey - refE));
    pz = DesplazarDerecha(pz, (int16_t)(e3 - refE));

    int16_t c12 = (int16_t)((int16_t)-(px * d.nx >> 15) + (int16_t)-(py * d.ny >> 15) + (int16_t)-(pz * d.nz >> 15));
    int32_t aux4 = c12;
    refE = (int16_t)(16 - refE);
    if (refE >= 0) aux4 <<= refE; else aux4 >>= -refE;
    if (aux4 == -1) aux4 = 0;
    aux4 >>= 1;

    int32_t aux = (uint16_t)d.les + aux4;   // Les menos el producto con la normal de la pantalla
    int16_t c10;
    NormalizarDoble(aux, c10, e2);
    e2 = (int16_t)(15 - e2);
    int16_t c4;
    Inversa(c10, 0, c4, e4);
    int16_t c2 = (int16_t)(c4 * d.c_les >> 15);   // factor de escala

    int16_t c, e;
    // H
    e = 0;
    Normalizar((int16_t)((int16_t)((int16_t)(px * d.hx >> 15) + (int16_t)(py * d.hy >> 15)) * c2 >> 15), c, e);
    out[0] = DesnormalizarRecortar(c, (int16_t)(d.e_les - e2 + refE + e));
    // V
    e = 0;
    int16_t c24 = (int16_t)((int16_t)(px * d.vx >> 15) + (int16_t)(py * d.vy >> 15) + (int16_t)(pz * d.vz >> 15));
    Normalizar((int16_t)(c24 * c2 >> 15), c, e);
    out[1] = DesnormalizarRecortar(c, (int16_t)(d.e_les - e2 + refE + e));
    // M
    Normalizar(c2, c, e4);
    out[2] = DesnormalizarRecortar(c, (int16_t)(e4 + d.e_les - e2 - 7));
}

static void PruebaMemoria(int16_t*, int16_t* out) { out[0] = 0; }
static void TamanoMemoria(int16_t*, int16_t* out) { out[0] = 0x0100; }
static void VolcarMemoria(int16_t*, int16_t* out) { memset(out, 0, 16 * sizeof *out); }   // (no se usa en juegos)

// Comandos: función, cantidad de datos de entrada y de resultados (en palabras de 16 bits).
struct Comando { void (*f)(int16_t*, int16_t*); uint8_t entradas; uint16_t salidas; };
static const Comando COMANDOS[0x40] = {
    {Multiplicar, 2, 1}, {ActitudA, 4, 0}, {Parametro, 7, 4}, {SubjetivoA, 3, 3},      // 00
    {Triangulo, 2, 2}, {ActitudA, 4, 0}, {Proyectar, 3, 3}, {PruebaMemoria, 1, 1},
    {Radio, 3, 2}, {ObjetivoA, 3, 3}, {Raster, 1, 4}, {EscalarA, 3, 1},
    {Rotar, 3, 2}, {ObjetivoA, 3, 3}, {Blanco, 2, 2}, {PruebaMemoria, 1, 1},
    {CmdInversa, 2, 2}, {ActitudB, 4, 0}, {Parametro, 7, 4}, {SubjetivoB, 3, 3},       // 10
    {Girar, 6, 3}, {ActitudB, 4, 0}, {Proyectar, 3, 3}, {VolcarMemoria, 1, 1024},
    {Rango, 4, 1}, {ObjetivoB, 3, 3}, {nullptr, 0, 0}, {EscalarB, 3, 1},
    {Polar, 6, 3}, {ObjetivoB, 3, 3}, {Blanco, 2, 2}, {VolcarMemoria, 1, 1024},
    {Multiplicar2, 2, 1}, {ActitudC, 4, 0}, {Parametro, 7, 4}, {SubjetivoC, 3, 3},     // 20
    {Triangulo, 2, 2}, {ActitudC, 4, 0}, {Proyectar, 3, 3}, {TamanoMemoria, 1, 1},
    {Distancia, 3, 1}, {ObjetivoC, 3, 3}, {nullptr, 0, 0}, {EscalarC, 3, 1},
    {Rotar, 3, 2}, {ObjetivoC, 3, 3}, {Blanco, 2, 2}, {TamanoMemoria, 1, 1},
    {CmdInversa, 2, 2}, {ActitudA, 4, 0}, {Parametro, 7, 4}, {SubjetivoA, 3, 3},       // 30
    {Girar, 6, 3}, {ActitudA, 4, 0}, {Proyectar, 3, 3}, {VolcarMemoria, 1, 1024},
    {Rango2, 4, 1}, {ObjetivoA, 3, 3}, {nullptr, 0, 0}, {EscalarA, 3, 1},
    {Polar, 6, 3}, {ObjetivoA, 3, 3}, {Blanco, 2, 2}, {VolcarMemoria, 1, 1024},
};

// El resultado de VolcarMemoria tiene 1024 palabras: solo se entregan ceros (ningún juego lo usa).
static int16_t SalidaPalabra(int i) { return i < 16 ? d.salida[i] : 0; }

// ---------- comunicación con la SNES ----------
// Cada lectura o escritura del registro de datos mueve un byte (primero el bajo, luego el alto): con eso avanza la
// máquina de estados del chip. El chip responde al instante.
static void Paso(bool leer, uint8_t& dato)
{
    if (!(d.sr & SR_RQM)) return;
    if (leer) dato = (d.sr & SR_DRS) ? (uint8_t)(d.dr >> 8) : (uint8_t)d.dr;
    else if (d.sr & SR_DRS) d.dr = (uint16_t)((d.dr & 0x00ff) | (dato << 8));
    else d.dr = (uint16_t)((d.dr & 0xff00) | dato);

    switch (d.fase)
    {
    case 0:   // esperando comando
        d.comando = (uint8_t)d.dr;
        if (!(d.comando & 0xc0))
        {
            if (d.comando == 0x1a || d.comando == 0x2a || d.comando == 0x3a) d.congelado = true;
            else { d.contador = 0; d.fase = 1; d.sr &= ~SR_DRC; }
        }
        break;
    case 1:   // leyendo los datos del comando
        d.sr ^= SR_DRS;
        if (!(d.sr & SR_DRS))
        {
            d.entrada[d.contador++] = (int16_t)d.dr;
            const Comando& c = COMANDOS[d.comando];
            if (d.contador >= c.entradas)
            {
                c.f(d.entrada, d.salida);
                if (c.salidas) { d.contador = 0; d.dr = (uint16_t)SalidaPalabra(0); d.fase = 2; }
                else { d.dr = 0x0080; d.fase = 0; d.sr |= SR_DRC; }
            }
        }
        break;
    default:  // entregando los resultados
        d.sr ^= SR_DRS;
        if (!(d.sr & SR_DRS))
        {
            ++d.contador;
            const Comando& c = COMANDOS[d.comando];
            if (d.contador >= c.salidas)
            {
                if (d.comando == 0x0a && d.dr != 0x8000)
                {
                    // Raster en modo continuo: sigue con la línea siguiente hasta que la SNES escribe 0x8000.
                    d.entrada[0]++;
                    c.f(d.entrada, d.salida);
                    d.contador = 0;
                    d.dr = (uint16_t)SalidaPalabra(0);
                }
                else { d.dr = 0x0080; d.fase = 0; d.sr |= SR_DRC; }
            }
            else d.dr = (uint16_t)SalidaPalabra(d.contador);
        }
        break;
    }
    if (d.congelado) d.sr &= ~SR_RQM;
}

void Dsp1Reiniciar()
{
    PrepararTablas();
    memset(&d, 0, sizeof d);
    d.sr = SR_DRC | SR_RQM;
    d.dr = 0x0080;
}

uint8_t Dsp1Leer(bool estado)
{
    if (estado) return d.sr;   // byte alto del registro de estado (RQM = listo)
    uint8_t v = 0;
    Paso(true, v);
    return v;
}

void Dsp1Escribir(bool estado, uint8_t v)
{
    if (estado) return;   // el registro de estado no se escribe
    Paso(false, v);
}

void Dsp1Estado(Serial& s) { s(d); }

#ifdef GSU_TRAZA
// Herramientas de PC: las tablas generadas, para compararlas con las del chip.
const int16_t* Dsp1Tabla(int cual)
{
    PrepararTablas();
    switch (cual)
    {
    case 0: return tablaSeno;
    case 1: return tablaMul;
    case 2: return tablaPot;
    case 3: return tablaInversa;
    case 4: return tablaRaiz;
    default: return taylor;
    }
}
#endif
