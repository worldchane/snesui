// El DSP de sonido de la SNES (S-DSP). Se incluye dentro de apu.cpp (usa aram y dsp[]).
// 8 voces que tocan muestras guardadas en la RAM del APU en formato BRR (bloques de 9 bytes: 1 de cabecera y 16 muestras
// de 4 bits), cada una con su tono, su envolvente (ADSR o ganancia) y su volumen; más ruido, modulación de tono y eco.
// Saca una muestra estéreo cada 32 ciclos del SPC700 (32.000 por segundo). Sigue el funcionamiento descrito por bsnes y
// la documentación de la comunidad (fullsnes); la interpolación es lineal (la consola usa una gaussiana: suena un poco
// más brillante, pero muy parecido).

enum { MODO_ATAQUE, MODO_DECAIMIENTO, MODO_SOSTEN, MODO_LIBERACION };

struct VozDsp
{
    uint16_t bloque;          // dirección del bloque BRR que se está tocando
    int nibble;               // muestra dentro del bloque (0..15)
    int16_t hist[2];          // las dos últimas muestras decodificadas (para los filtros BRR)
    int16_t actual, anterior; // para interpolar
    uint32_t posicion;        // parte fraccionaria del avance (12 bits)
    int env;                  // envolvente 0..0x7FF
    int modo;
    int contadorEnv;
    bool activa;
    int salida;               // última salida (para la modulación de tono y OUTX)
};

static VozDsp voces[8];
static int16_t muestras[8192];   // salida estéreo intercalada, del cuadro en curso
static int nMuestras;
static uint16_t ruido = 0x4000;
static int contadorRuido;
static int ecoPos;
static int16_t ecoHist[2][8];
static int ecoHistPos;
static int acumDsp;

// Cada cuánto (en muestras) avanza una envolvente o el ruido, según su "velocidad" de 0 a 31 (0 = nunca).
static const int PERIODO[32] = {0, 2048, 1536, 1280, 1024, 768, 640, 512, 384, 320, 256, 192, 160, 128, 96, 80,
                                64, 48, 40, 32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1};

static inline int Satura16(int v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }
static inline int8_t R8(int r) { return (int8_t)dsp[r]; }

// Decodifica la siguiente muestra BRR de la voz.
static int16_t DecodificarBrr(VozDsp& v, int nv)
{
    uint8_t cab = aram[v.bloque];
    int corrimiento = cab >> 4, filtro = (cab >> 2) & 3;
    uint8_t byte = aram[(uint16_t)(v.bloque + 1 + (v.nibble >> 1))];
    int s = (v.nibble & 1) ? (byte & 0xF) : (byte >> 4);
    s = (int16_t)(s << 12) >> 12;                       // con signo, 4 bits
    s = (s << corrimiento) >> 1;
    if (corrimiento >= 0xD) s = (s >> 25) << 11;        // -2048 o 0
    int p1 = v.hist[0], p2 = v.hist[1] >> 1;
    switch (filtro)
    {
    case 1: s += p1 >> 1; s += (-p1) >> 5; break;
    case 2: s += p1; s -= p2; s += p2 >> 4; s += (p1 * -3) >> 6; break;
    case 3: s += p1; s -= p2; s += (p1 * -13) >> 7; s += (p2 * 3) >> 4; break;
    }
    s = Satura16(s);
    s = (int16_t)(s << 1);
    v.hist[1] = v.hist[0];
    v.hist[0] = (int16_t)s;

    // Avanzar; al final del bloque: repetir desde el punto de bucle o terminar.
    if (++v.nibble >= 16)
    {
        v.nibble = 0;
        if (cab & 1)   // último bloque
        {
            dsp[0x7C] |= 1 << nv;   // ENDX
            if (cab & 2)
            {
                uint16_t dir = (uint16_t)(dsp[0x5D] * 0x100 + dsp[nv * 0x10 + 4] * 4);
                v.bloque = aram[(uint16_t)(dir + 2)] | aram[(uint16_t)(dir + 3)] << 8;
            }
            else { v.modo = MODO_LIBERACION; v.env = 0; v.activa = false; }
        }
        else v.bloque += 9;
    }
    return (int16_t)s;
}

static void Encender(int nv)
{
    VozDsp& v = voces[nv];
    uint16_t dir = (uint16_t)(dsp[0x5D] * 0x100 + dsp[nv * 0x10 + 4] * 4);
    v.bloque = aram[dir] | aram[(uint16_t)(dir + 1)] << 8;
    v.nibble = 0;
    v.hist[0] = v.hist[1] = 0;
    v.actual = v.anterior = 0;
    v.posicion = 0;
    v.env = 0;
    v.modo = MODO_ATAQUE;
    v.contadorEnv = 0;
    v.activa = true;
    dsp[0x7C] &= ~(1 << nv);
}

static void Envolvente(VozDsp& v, int nv)
{
    uint8_t adsr1 = dsp[nv * 0x10 + 5], adsr2 = dsp[nv * 0x10 + 6], gain = dsp[nv * 0x10 + 7];
    if (v.modo == MODO_LIBERACION) { v.env -= 8; if (v.env < 0) v.env = 0; return; }
    int velocidad, paso = 0;
    if (adsr1 & 0x80)   // ADSR
    {
        switch (v.modo)
        {
        case MODO_ATAQUE: velocidad = (adsr1 & 0xF) * 2 + 1; paso = velocidad == 31 ? 1024 : 32; break;
        case MODO_DECAIMIENTO: velocidad = ((adsr1 >> 4) & 7) * 2 + 16; paso = -(((v.env - 1) >> 8) + 1); break;
        default: velocidad = adsr2 & 0x1F; paso = -(((v.env - 1) >> 8) + 1); break;
        }
    }
    else if (!(gain & 0x80)) { v.env = (gain & 0x7F) * 16; return; }   // ganancia directa
    else
    {
        velocidad = gain & 0x1F;
        switch ((gain >> 5) & 3)
        {
        case 0: paso = -32; break;                                   // baja lineal
        case 1: paso = -(((v.env - 1) >> 8) + 1); break;            // baja exponencial
        case 2: paso = 32; break;                                    // sube lineal
        case 3: paso = v.env < 0x600 ? 32 : 8; break;                // sube "doblada"
        }
    }
    if (!PERIODO[velocidad]) return;
    if (++v.contadorEnv < PERIODO[velocidad]) return;
    v.contadorEnv = 0;
    v.env += paso;
    if (v.env > 0x7FF) { v.env = 0x7FF; if (v.modo == MODO_ATAQUE) v.modo = MODO_DECAIMIENTO; }
    if (v.env < 0) v.env = 0;
    if ((adsr1 & 0x80) && v.modo == MODO_DECAIMIENTO && (v.env >> 8) <= (adsr2 >> 5)) v.modo = MODO_SOSTEN;
}

static void DspMuestra()
{
    uint8_t flg = dsp[0x6C];
    // Ruido: un registro de desplazamiento de 15 bits que avanza a la velocidad de FLG.
    if (PERIODO[flg & 0x1F] && ++contadorRuido >= PERIODO[flg & 0x1F])
    {
        contadorRuido = 0;
        int fb = (ruido << 13) ^ (ruido << 14);
        ruido = (uint16_t)((fb & 0x4000) | (ruido >> 1));
    }

    int sumaL = 0, sumaR = 0, ecoL = 0, ecoR = 0;
    for (int nv = 0; nv < 8; nv++)
    {
        VozDsp& v = voces[nv];
        if (!v.activa && v.env == 0) { v.salida = 0; dsp[nv * 0x10 + 8] = 0; dsp[nv * 0x10 + 9] = 0; continue; }
        int tono = (dsp[nv * 0x10 + 2] | dsp[nv * 0x10 + 3] << 8) & 0x3FFF;
        if (nv > 0 && (dsp[0x2D] & (1 << nv))) tono += ((voces[nv - 1].salida >> 5) * tono) >> 10;   // modulación
        if (tono < 0) tono = 0;
        if (tono > 0x3FFF) tono = 0x3FFF;
        // Avanzar por la muestra: 0x1000 = una muestra por cada muestra de salida.
        v.posicion += tono;
        while (v.posicion >= 0x1000)
        {
            v.posicion -= 0x1000;
            v.anterior = v.actual;
            v.actual = v.activa ? DecodificarBrr(v, nv) : 0;
        }
        int s = v.anterior + (((v.actual - v.anterior) * (int)v.posicion) >> 12);
        if (dsp[0x3D] & (1 << nv)) s = (int16_t)(ruido << 1);   // ruido en lugar de la muestra
        Envolvente(v, nv);
        int salida = (s * v.env) >> 11;
        v.salida = salida;
        dsp[nv * 0x10 + 8] = (uint8_t)(v.env >> 4);      // ENVX
        dsp[nv * 0x10 + 9] = (uint8_t)(salida >> 8);     // OUTX
        int l = (salida * R8(nv * 0x10 + 0)) >> 7, r = (salida * R8(nv * 0x10 + 1)) >> 7;
        sumaL += l; sumaR += r;
        if (dsp[0x4D] & (1 << nv)) { ecoL += l; ecoR += r; }
    }

    // Eco: un búfer circular en la RAM del APU, filtrado con 8 coeficientes (FIR) y realimentado.
    uint16_t base = (uint16_t)(dsp[0x6D] * 0x100);
    int largo = (dsp[0x7D] & 0xF) ? (dsp[0x7D] & 0xF) * 0x800 : 4;
    uint16_t dir = (uint16_t)(base + ecoPos);
    int16_t entL = (int16_t)(aram[dir] | aram[(uint16_t)(dir + 1)] << 8) >> 1;
    int16_t entR = (int16_t)(aram[(uint16_t)(dir + 2)] | aram[(uint16_t)(dir + 3)] << 8) >> 1;
    ecoHist[0][ecoHistPos] = entL;
    ecoHist[1][ecoHistPos] = entR;
    int firL = 0, firR = 0;
    for (int k = 0; k < 8; k++)
    {
        int idx = (ecoHistPos + 1 + k) & 7;
        firL += (ecoHist[0][idx] * R8(k * 0x10 + 0xF)) >> 6;
        firR += (ecoHist[1][idx] * R8(k * 0x10 + 0xF)) >> 6;
    }
    ecoHistPos = (ecoHistPos + 1) & 7;
    firL = Satura16(firL);
    firR = Satura16(firR);
    if (!(flg & 0x20))   // escritura del eco habilitada
    {
        int wl = Satura16(ecoL + ((firL * R8(0x0D)) >> 7)) & ~1;
        int wr = Satura16(ecoR + ((firR * R8(0x0D)) >> 7)) & ~1;
        aram[dir] = (uint8_t)wl; aram[(uint16_t)(dir + 1)] = (uint8_t)(wl >> 8);
        aram[(uint16_t)(dir + 2)] = (uint8_t)wr; aram[(uint16_t)(dir + 3)] = (uint8_t)(wr >> 8);
    }
    ecoPos += 4;
    if (ecoPos >= largo) ecoPos = 0;

    int L = Satura16(((sumaL * R8(0x0C)) >> 7) + ((firL * R8(0x2C)) >> 7));
    int R = Satura16(((sumaR * R8(0x1C)) >> 7) + ((firR * R8(0x3C)) >> 7));
    if (flg & 0x40) L = R = 0;   // silencio
    if (nMuestras < (int)(sizeof muestras / sizeof muestras[0]) / 2)
    {
        muestras[nMuestras * 2] = (int16_t)L;
        muestras[nMuestras * 2 + 1] = (int16_t)R;
        nMuestras++;
    }
}

// Escritura de un registro del DSP (por el puerto 0xF2/0xF3 del SPC700).
static void DspEscribir(uint8_t r, uint8_t v)
{
    r &= 0x7F;
    dsp[r] = v;
    switch (r)
    {
    case 0x4C: for (int i = 0; i < 8; i++) if (v & (1 << i)) Encender(i); break;                       // KON
    case 0x5C: for (int i = 0; i < 8; i++) if (v & (1 << i)) voces[i].modo = MODO_LIBERACION; break;   // KOFF
    case 0x7C: dsp[0x7C] = 0; break;                                                                   // ENDX: escribir lo borra
    case 0x6C: if (v & 0x80) for (int i = 0; i < 8; i++) { voces[i].modo = MODO_LIBERACION; voces[i].env = 0; voces[i].activa = false; } break;
    }
}

static void DspReiniciar()
{
    memset(voces, 0, sizeof voces);
    memset(ecoHist, 0, sizeof ecoHist);
    nMuestras = 0;
    ruido = 0x4000;
    contadorRuido = ecoPos = ecoHistPos = acumDsp = 0;
    dsp[0x6C] = 0xE0;
}
