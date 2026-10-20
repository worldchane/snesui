// Ver musica.h. Cada canción es una receta: tonalidad, modo, tempo, dos progresiones de acordes (partes A y B) y el
// estilo de cada instrumento. La melodía se compone al elegir la canción con reglas sencillas (notas del acorde en los
// tiempos fuertes, pasos cortos por la escala entre medio, cada frase termina en la tónica) a partir de una semilla
// fija, así que siempre suena igual. Forma de 16 compases que se repite: A, A' (otro final), B, A.
#include "musica.h"

#include <atomic>
#include <math.h>
#include <string.h>

namespace {

const float SR = 48000.f;
const int PASOS = 64;   // una frase: 4 compases de 16 semicorcheas

enum { M_MAYOR, M_MENOR, M_DORICO, M_MIXOLIDIO };
const int ESCALAS[4][7] = {{0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 10}};
enum { O_PULSO12, O_PULSO25, O_PULSO50, O_TRIANGULO, O_DIENTE };

struct Receta
{
    const char* nombre;
    int bpm, tonica, modo;          // tónica en MIDI (57 = La)
    int progA[4], progB[4];         // grado del acorde de cada compás (0 = I ... 6 = VII)
    int arpegio, bajo, bateria;     // estilos (ver más abajo)
    int ondaMelodia, ondaArpegio;
    unsigned semilla;
    float swing;                    // 0 = parejo; 0,2 = las semicorcheas de a contratiempo un poco más tarde
};

// Las diez canciones de ALLESIU.
const Receta RECETAS[NUM_CANCIONES] = {
    {"Luna violeta", 92, 57, M_MENOR, {0, 5, 2, 6}, {3, 4, 0, 4}, 0, 0, 3, O_PULSO25, O_PULSO12, 0x1A2B3C, 0.f},
    {"Atardecer pixelado", 108, 60, M_MAYOR, {0, 4, 5, 3}, {3, 0, 3, 4}, 1, 1, 0, O_PULSO50, O_PULSO25, 0x51F00D, .12f},
    {"Bosque de 8 bits", 120, 62, M_DORICO, {0, 3, 0, 4}, {6, 3, 6, 4}, 2, 3, 0, O_PULSO25, O_TRIANGULO, 0xB05C3E, 0.f},
    {"Ciudad estelar", 126, 55, M_MENOR, {0, 6, 5, 4}, {5, 6, 0, 4}, 0, 3, 1, O_DIENTE, O_PULSO12, 0xC17A11, 0.f},
    {"Brisa de arcade", 132, 64, M_MIXOLIDIO, {0, 6, 3, 0}, {3, 4, 6, 4}, 1, 0, 1, O_PULSO12, O_PULSO50, 0xA4C4DE, 0.f},
    {"Cielo lila", 84, 65, M_MAYOR, {0, 2, 3, 4}, {5, 3, 1, 4}, 3, 2, 3, O_TRIANGULO, O_PULSO25, 0x0C1E70, .18f},
    {"Ruta costera", 116, 59, M_MAYOR, {0, 5, 3, 4}, {1, 4, 0, 5}, 2, 1, 0, O_PULSO50, O_PULSO12, 0xC057A1, .1f},
    {"Cristal", 100, 61, M_MENOR, {0, 3, 6, 2}, {5, 6, 4, 4}, 1, 2, 2, O_TRIANGULO, O_PULSO12, 0xC21574, 0.f},
    {"Gran aventura", 138, 58, M_MAYOR, {0, 3, 4, 0}, {5, 3, 1, 4}, 0, 3, 1, O_PULSO25, O_PULSO50, 0xAD7E07, 0.f},
    {"Medianoche retro", 96, 52, M_DORICO, {0, 0, 3, 4}, {6, 6, 3, 4}, 3, 0, 2, O_PULSO12, O_TRIANGULO, 0x3D17E5, .15f},
};

// Ritmos de la melodía por compás (1 = empieza una nota). Los de cierre dejan notas largas.
const char* const RITMOS[6] = {"1000101010001000", "1010100010101010", "1001001010001000", "1000001010101000",
                               "1010001000101000", "1000100010001000"};
const char* const CIERRES[3] = {"1000100010000000", "1010100010000000", "1000001010000000"};

struct Cancion
{
    const Receta* r;
    int melodiaA[PASOS], melodiaA2[PASOS], melodiaB[PASOS];   // nota MIDI, -1 = silencio, -2 = sigue la anterior
};

unsigned rng;
int Azar(int n) { rng = rng * 1103515245u + 12345u; return (int)((rng >> 16) % (unsigned)n); }

int Nota(const Receta& r, int grado)   // grado de la escala (puede pasar de 7) → nota MIDI
{
    int o = grado >= 0 ? grado / 7 : (grado - 6) / 7;
    return r.tonica + 12 * o + ESCALAS[r.modo][grado - 7 * o];
}

bool DelAcorde(int grado, int acorde) { int d = ((grado - acorde) % 7 + 7) % 7; return d == 0 || d == 2 || d == 4; }

// Compone una frase de 4 compases sobre la progresión 'prog'. 'desde' = compás desde el que se compone (lo anterior se
// deja como está, para las variaciones).
void Componer(const Receta& r, const int* prog, int* mel, int desde)
{
    int actual = 9;   // grado de la escala (una octava arriba de la tónica, más o menos)
    for (int c = desde; c < 4; c++)
    {
        const char* ritmo = c == 3 ? CIERRES[Azar(3)] : RITMOS[Azar(6)];
        for (int p = 0; p < 16; p++)
        {
            int i = c * 16 + p;
            if (ritmo[p] != '1') { mel[i] = -2; continue; }
            int acorde = prog[c];
            if (c == 3 && p >= 8)
            {
                // Cierre: la tónica más cercana.
                int t = (actual / 7) * 7;
                actual = actual - t > 3 ? t + 7 : t;
            }
            else if (p % 8 == 0)
            {
                // Tiempo fuerte: la nota del acorde más cercana (hacia arriba o abajo al azar).
                int sube = Azar(2) ? 1 : -1;
                for (int k = 0; k < 4 && !DelAcorde(actual, acorde); k++) actual += sube;
            }
            else
            {
                static const int PASO[8] = {-2, -1, -1, 0, 1, 1, 2, 3};
                actual += PASO[Azar(8)];
            }
            if (actual < 5) actual += 2;
            if (actual > 16) actual -= 3;
            mel[i] = Nota(r, actual);
        }
    }
    // Algún silencio corto para que respire (no en el primer tiempo de cada compás).
    for (int k = 0; k < 2; k++)
    {
        int i = Azar(PASOS);
        if (i % 16 && i % 16 < 12 && mel[i] == -2) mel[i] = -1;
    }
}

void Armar(Cancion& c, const Receta& r)
{
    c.r = &r;
    rng = r.semilla;
    Componer(r, r.progA, c.melodiaA, 0);
    memcpy(c.melodiaA2, c.melodiaA, sizeof c.melodiaA);
    Componer(r, r.progA, c.melodiaA2, 3);   // A': el mismo comienzo, otro final
    Componer(r, r.progB, c.melodiaB, 0);
}

// ---------- síntesis ----------
struct Voz { float frec, fase, amp, env, decae; int onda; bool sonando; };
struct Golpe { float t, fase; bool activo; };

float Onda(int onda, float fase)
{
    switch (onda)
    {
    case O_PULSO12: return fase < .125f ? .8f : -.2f;
    case O_PULSO25: return fase < .25f ? .7f : -.3f;
    case O_PULSO50: return fase < .5f ? .5f : -.5f;
    case O_TRIANGULO: return fase < .5f ? fase * 4 - 1 : 3 - fase * 4;
    default: return fase * 2 - 1;
    }
}

float Frec(int midi) { return 440.f * powf(2.f, (midi - 69) / 12.f); }

Cancion cancion;
std::atomic<int> pedida{0}, elegida{-1};
std::atomic<bool> activa{true};
float volumen = 0, volumenObjetivo = 1;
long muestra;            // muestras desde el comienzo de la canción
int pasoAnterior = -1;
Voz melodia, arpegio, bajo;
Golpe bombo, caja, platillo;
unsigned ruido = 22222;
float filtroM, filtroA, filtroB;   // suavizado de las ondas cuadradas (filtro de un polo)
const int ECO = 48000;   // 1 s: el eco más largo (3 semicorcheas a 60 por minuto son 0,75 s)
float ecoL[ECO], ecoR[ECO];
int ecoPos;

void Tocar(Voz& v, int midi, float amp, float decae, int onda)
{
    v.frec = Frec(midi);
    v.amp = amp;
    v.env = 1;
    v.decae = decae;
    v.onda = onda;
    v.sonando = true;
}

int GradoAcorde(const Receta& r, int acorde, int k) { return acorde + 2 * k; }   // k = 0, 1, 2: tónica, tercera, quinta

// Lo que pasa al empezar cada semicorchea.
void Paso(int paso)
{
    const Receta& r = *cancion.r;
    int frase = (paso / PASOS) % 4, i = paso % PASOS, compas = i / 16, p = i % 16;
    const int* prog = frase == 2 ? r.progB : r.progA;
    const int* mel = frase == 0 || frase == 3 ? cancion.melodiaA : frase == 1 ? cancion.melodiaA2 : cancion.melodiaB;
    int acorde = prog[compas];

    if (mel[i] >= 0) Tocar(melodia, mel[i], .55f, 1.8f, r.ondaMelodia);
    else if (mel[i] == -1) melodia.sonando = false;

    // Arpegio (0: sube 1-3-5-8, 1: sube y baja, 2: quebrado 1-5-3-5, 3: acorde suave cada dos tiempos).
    if (r.arpegio == 3)
    {
        if (p % 8 == 0) Tocar(arpegio, Nota(r, GradoAcorde(r, acorde, (p / 8) % 2 ? 2 : 1)), .32f, 1.2f, r.ondaArpegio);
    }
    else
    {
        static const int ORDEN[3][4] = {{0, 1, 2, 3}, {0, 1, 2, 1}, {0, 2, 1, 2}};
        int k = ORDEN[r.arpegio][p % 4];
        int nota = k == 3 ? Nota(r, acorde) + 12 : Nota(r, GradoAcorde(r, acorde, k));
        Tocar(arpegio, nota, .26f, 9.f, r.ondaArpegio);
    }

    // Bajo (0: corcheas con salto de octava, 1: tónica y quinta en 1 y 3, 2: caminando por negras, 3: sincopado).
    int nb = Nota(r, acorde) - 12;
    if (nb > r.tonica - 5) nb -= 12;
    switch (r.bajo)
    {
    case 0: if (p % 2 == 0) Tocar(bajo, nb + ((p / 2) % 2 ? 12 : 0), .6f, 5.f, O_TRIANGULO); break;
    case 1: if (p == 0 || p == 8) Tocar(bajo, nb + (p == 8 ? Nota(r, acorde + 4) - Nota(r, acorde) : 0), .65f, 2.5f, O_TRIANGULO); break;
    case 2: if (p % 4 == 0) { static const int G[4] = {0, 2, 4, 5}; Tocar(bajo, nb + Nota(r, acorde + G[p / 4]) - Nota(r, acorde), .6f, 3.f, O_TRIANGULO); } break;
    default: if (p == 0 || p == 3 || p == 6 || p == 8 || p == 11 || p == 14) Tocar(bajo, nb + (p == 6 || p == 14 ? 12 : 0), .62f, 4.f, O_TRIANGULO); break;
    }

    // Batería (0: rock, 1: bombo en negras, 2: medio tiempo, 3: suave).
    bool b = false, c = false, h = false;
    switch (r.bateria)
    {
    case 0: b = p == 0 || p == 8 || p == 10; c = p == 4 || p == 12; h = p % 2 == 0; break;
    case 1: b = p % 4 == 0; c = p == 4 || p == 12; h = p % 4 == 2; break;
    case 2: b = p == 0 || p == 10; c = p == 8; h = p % 2 == 0; break;
    default: b = p == 0; h = p == 4 || p == 12; c = compas == 3 && p == 14; break;
    }
    // La primera frase entra sin batería los dos primeros compases (una introducción).
    if (paso < 32) b = c = h = false;
    if (b) bombo = {0, 0, true};
    if (c) caja = {0, 0, true};
    if (h) platillo = {0, 0, true};
}

float Voz1(Voz& v, float& filtro)
{
    if (!v.sonando && v.env < .001f) return 0;
    if (!v.sonando) v.env *= .9990f;   // soltar rápido
    v.fase += v.frec / SR;
    if (v.fase >= 1) v.fase -= 1;
    float s = Onda(v.onda, v.fase) * v.amp * v.env;
    v.env *= 1 - v.decae / SR * .35f;
    filtro += (s - filtro) * .35f;
    return filtro;
}

float Ruido()
{
    ruido ^= ruido << 13; ruido ^= ruido >> 17; ruido ^= ruido << 5;
    return (int)(ruido & 0xFFFF) / 32768.f - 1;
}

float Bateria()
{
    float s = 0;
    if (bombo.activo)
    {
        float f = 45 + 95 * expf(-bombo.t * 30);
        bombo.fase += f / SR;
        s += sinf(bombo.fase * 6.2831853f) * expf(-bombo.t * 14) * .9f;
        bombo.t += 1 / SR;
        if (bombo.t > .4f) bombo.activo = false;
    }
    if (caja.activo)
    {
        caja.fase += 190 / SR;
        s += (Ruido() * .55f + sinf(caja.fase * 6.2831853f) * .35f) * expf(-caja.t * 22) * .5f;
        caja.t += 1 / SR;
        if (caja.t > .3f) caja.activo = false;
    }
    if (platillo.activo)
    {
        static float antes;
        float n = Ruido(), agudo = n - antes;   // solo lo agudo del ruido
        antes = n;
        s += agudo * expf(-platillo.t * 70) * .18f;
        platillo.t += 1 / SR;
        if (platillo.t > .1f) platillo.activo = false;
    }
    return s;
}

void Empezar(int c)
{
    memset(&melodia, 0, sizeof melodia);
    memset(&arpegio, 0, sizeof arpegio);
    memset(&bajo, 0, sizeof bajo);
    bombo.activo = caja.activo = platillo.activo = false;
    memset(ecoL, 0, sizeof ecoL);
    memset(ecoR, 0, sizeof ecoR);
    muestra = 0;
    pasoAnterior = -1;
    if (c >= 0) Armar(cancion, RECETAS[c]);
    elegida.store(c, std::memory_order_release);
}

}   // namespace

namespace {
// Orden aleatorio: la canción siguiente, distinta de la actual.
std::atomic<bool> aleatoria{false};
unsigned azarOrden = 1;
int OtraCancion(int actual)
{
    azarOrden = azarOrden * 1664525u + 1013904223u;
    int c = (int)((azarOrden >> 8) % (NUM_CANCIONES - 1));
    return c >= actual && actual >= 0 ? c + 1 : c;
}
const int VUELTAS = 2;   // cada canción suena dos veces la forma completa (A A' B A) antes de pasar a otra
}   // namespace

void MusicaEncender(bool si, unsigned semilla)
{
    aleatoria.store(si, std::memory_order_relaxed);
    if (!si) { MusicaElegir(-1); return; }
    if (MusicaElegida() >= 0) return;   // ya suena
    azarOrden = semilla | 1;
    MusicaElegir(OtraCancion(-1));
}

int MusicaSiguiente()
{
    int c = (MusicaElegida() + 1) % NUM_CANCIONES;
    MusicaElegir(c);
    return c;
}

const char* MusicaNombre(int c) { return c >= 0 && c < NUM_CANCIONES ? RECETAS[c].nombre : "Sin música"; }
void MusicaElegir(int c) { pedida.store(c >= -1 && c < NUM_CANCIONES ? c : -1, std::memory_order_release); }
int MusicaElegida() { return pedida.load(std::memory_order_acquire); }
void MusicaActiva(bool si) { activa.store(si, std::memory_order_relaxed); }

void MusicaMezclar(float* L, float* R, int n)
{
    int p = pedida.load(std::memory_order_acquire);
    if (p != elegida.load(std::memory_order_relaxed)) { Empezar(p); volumen = 0; }
    if (p < 0) return;
    const Receta& r = *cancion.r;
    volumenObjetivo = activa.load(std::memory_order_relaxed) ? 1.f : 0.f;
    if (volumen <= 0 && volumenObjetivo <= 0) return;   // en silencio y pausada: no avanza
    const float porPaso = SR * 60.f / r.bpm / 4;
    int retardo = (int)(porPaso * 3);   // eco a 3 semicorcheas
    if (retardo >= ECO) retardo = ECO - 1;
    for (int k = 0; k < n; k++)
    {
        // Semicorcheas con swing: las de contratiempo empiezan un poco más tarde.
        float t = muestra / porPaso;
        int paso = (int)t;
        if (paso % 2 == 1 && t - paso < r.swing) paso--;
        if (paso != pasoAnterior)
        {
            // Terminadas sus vueltas, otra canción al azar (entra desde silencio: ver el fundido de abajo).
            if (paso >= VUELTAS * 4 * PASOS && aleatoria.load(std::memory_order_relaxed))
            {
                int otra = OtraCancion(elegida.load(std::memory_order_relaxed));
                pedida.store(otra, std::memory_order_release);
                Empezar(otra);
                volumen = 0;
                return;
            }
            pasoAnterior = paso;
            Paso(paso);
        }
        muestra++;
        float m = Voz1(melodia, filtroM), a = Voz1(arpegio, filtroA), b = Voz1(bajo, filtroB), d = Bateria();
        int lee = (ecoPos - retardo + ECO) % ECO;
        float el = ecoL[lee], er = ecoR[lee];
        ecoL[ecoPos] = (m + a * .5f) * .5f + er * .35f;   // eco cruzado: va y viene entre los dos lados
        ecoR[ecoPos] = (m * .5f + a) * .5f + el * .35f;
        ecoPos = (ecoPos + 1) % ECO;
        float l = m * .9f + a * .7f + b + d + el * .45f, rr = m * .9f + a * 1.f + b + d + er * .45f;
        volumen += (volumenObjetivo - volumen) * (1 / (SR * .4f));   // sube o baja en algo menos de un segundo
        // En el modo aleatorio, el último compás baja de a poco hasta el cambio de canción.
        float fundido = 1;
        if (aleatoria.load(std::memory_order_relaxed))
        {
            float queda = (VUELTAS * 4 * PASOS * porPaso - muestra) / (porPaso * 16);
            if (queda < 1) fundido = queda > 0 ? queda : 0;
        }
        L[k] += l * .16f * volumen * fundido;
        R[k] += rr * .16f * volumen * fundido;
    }
}
