// Ver estilo.h.
#include "estilo.h"

#include <math.h>

#include "lienzo.h"
#include "plataforma.h"
#include "texto.h"

void Suave(float& v, float objetivo, float rapidez, float dt)
{
    v += (objetivo - v) * (1 - expf(-rapidez * dt));
    if (fabsf(objetivo - v) < 1e-4f) v = objetivo;
}

float Facilidad(float t)
{
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return 1 - powf(1 - t, 3);   // sale rápido y frena al llegar
}

// ---------- estilos de los botones del mando (pistas de abajo, ventanas) ----------
// Cada estilo combina la forma del fondo, el relleno, el color de los símbolos, el grosor del trazo y el tamaño.
enum { BF_CIRCULO, BF_REDONDEADO, BF_CUADRADO, BF_PILDORA, BF_NADA };
enum { BR_TRANSLUCIDO, BR_OSCURO, BR_CLARO, BR_COLOR, BR_CONTORNO, BR_VIDRIO, BR_DEGRADADO };
enum { BC_CLASICO, BC_VIVO, BC_BLANCO, BC_NEGRO, BC_TEMA, BC_DORADO, BC_PASTEL };
struct EstiloBoton { const char* nombre; uint8_t forma, relleno, color; float grosor, tam; bool brillo; };
static const EstiloBoton BOTONES[] = {
    {"Original", BF_CIRCULO, BR_TRANSLUCIDO, BC_CLASICO, 2.6f, 1, false},
    {"Clásico vivo", BF_CIRCULO, BR_TRANSLUCIDO, BC_VIVO, 2.8f, 1, false},
    {"Blanco puro", BF_CIRCULO, BR_TRANSLUCIDO, BC_BLANCO, 2.6f, 1, false},
    {"Botón de color", BF_CIRCULO, BR_COLOR, BC_BLANCO, 2.8f, 1, false},
    {"Botón oscuro", BF_CIRCULO, BR_OSCURO, BC_CLASICO, 2.6f, 1, false},
    {"Botón claro", BF_CIRCULO, BR_CLARO, BC_CLASICO, 2.8f, 1, false},
    {"Contorno", BF_CIRCULO, BR_CONTORNO, BC_CLASICO, 2.4f, 1, false},
    {"Vidrio", BF_CIRCULO, BR_VIDRIO, BC_CLASICO, 2.6f, 1, false},
    {"Relieve", BF_CIRCULO, BR_DEGRADADO, BC_CLASICO, 2.8f, 1, false},
    {"Neón", BF_CIRCULO, BR_OSCURO, BC_VIVO, 2.6f, 1, true},
    {"Fino", BF_CIRCULO, BR_TRANSLUCIDO, BC_CLASICO, 1.8f, 1, false},
    {"Grueso", BF_CIRCULO, BR_TRANSLUCIDO, BC_CLASICO, 3.6f, 1, false},
    {"Grande", BF_CIRCULO, BR_TRANSLUCIDO, BC_CLASICO, 3, 1.18f, false},
    {"Chico", BF_CIRCULO, BR_TRANSLUCIDO, BC_CLASICO, 2.2f, .85f, false},
    {"Tecla redondeada", BF_REDONDEADO, BR_TRANSLUCIDO, BC_CLASICO, 2.6f, 1, false},
    {"Tecla oscura", BF_REDONDEADO, BR_OSCURO, BC_BLANCO, 2.6f, 1, false},
    {"Tecla clara", BF_REDONDEADO, BR_CLARO, BC_NEGRO, 2.6f, 1, false},
    {"Tecla de color", BF_REDONDEADO, BR_COLOR, BC_BLANCO, 2.8f, 1, false},
    {"Tecla vidrio", BF_REDONDEADO, BR_VIDRIO, BC_BLANCO, 2.6f, 1, false},
    {"Tecla relieve", BF_REDONDEADO, BR_DEGRADADO, BC_VIVO, 2.8f, 1, false},
    {"Tecla contorno", BF_REDONDEADO, BR_CONTORNO, BC_VIVO, 2.4f, 1, false},
    {"Tecla neón", BF_REDONDEADO, BR_OSCURO, BC_VIVO, 2.6f, 1, true},
    {"Cuadro", BF_CUADRADO, BR_TRANSLUCIDO, BC_CLASICO, 2.6f, 1, false},
    {"Cuadro oscuro", BF_CUADRADO, BR_OSCURO, BC_VIVO, 2.6f, 1, false},
    {"Cuadro claro", BF_CUADRADO, BR_CLARO, BC_CLASICO, 2.8f, 1, false},
    {"Cuadro de color", BF_CUADRADO, BR_COLOR, BC_NEGRO, 2.8f, 1, false},
    {"Cuadro contorno", BF_CUADRADO, BR_CONTORNO, BC_BLANCO, 2.2f, 1, false},
    {"Píldora", BF_PILDORA, BR_TRANSLUCIDO, BC_CLASICO, 2.6f, 1, false},
    {"Píldora oscura", BF_PILDORA, BR_OSCURO, BC_VIVO, 2.6f, 1, false},
    {"Píldora de color", BF_PILDORA, BR_COLOR, BC_BLANCO, 2.8f, 1, false},
    {"Píldora vidrio", BF_PILDORA, BR_VIDRIO, BC_CLASICO, 2.6f, 1, false},
    {"Solo símbolo", BF_NADA, BR_TRANSLUCIDO, BC_CLASICO, 3, 1, false},
    {"Solo símbolo vivo", BF_NADA, BR_TRANSLUCIDO, BC_VIVO, 3.2f, 1.1f, false},
    {"Solo símbolo blanco", BF_NADA, BR_TRANSLUCIDO, BC_BLANCO, 3, 1, false},
    {"Solo símbolo con brillo", BF_NADA, BR_TRANSLUCIDO, BC_VIVO, 3, 1, true},
    {"Color del tema", BF_CIRCULO, BR_TRANSLUCIDO, BC_TEMA, 2.6f, 1, false},
    {"Tema en contorno", BF_CIRCULO, BR_CONTORNO, BC_TEMA, 2.4f, 1, false},
    {"Tecla del tema", BF_REDONDEADO, BR_OSCURO, BC_TEMA, 2.6f, 1, false},
    {"Tema con brillo", BF_CIRCULO, BR_OSCURO, BC_TEMA, 2.6f, 1, true},
    {"Dorado", BF_CIRCULO, BR_OSCURO, BC_DORADO, 2.6f, 1, false},
    {"Dorado relieve", BF_REDONDEADO, BR_DEGRADADO, BC_DORADO, 2.8f, 1, false},
    {"Pastel", BF_CIRCULO, BR_CLARO, BC_PASTEL, 2.8f, 1, false},
    {"Pastel suave", BF_CIRCULO, BR_TRANSLUCIDO, BC_PASTEL, 2.6f, 1, false},
    {"Tecla pastel", BF_REDONDEADO, BR_CLARO, BC_PASTEL, 2.8f, 1, false},
    {"Negro sobre blanco", BF_CIRCULO, BR_CLARO, BC_NEGRO, 2.8f, 1, false},
    {"Blanco sobre negro", BF_CIRCULO, BR_OSCURO, BC_BLANCO, 2.8f, 1, false},
    {"Monocromo fino", BF_CIRCULO, BR_CONTORNO, BC_BLANCO, 1.8f, 1, false},
    {"Arcade", BF_CIRCULO, BR_COLOR, BC_NEGRO, 3.2f, 1.12f, true},
    {"Retro", BF_CUADRADO, BR_DEGRADADO, BC_BLANCO, 3, 1, false},
    {"Burbuja", BF_CIRCULO, BR_VIDRIO, BC_VIVO, 2.8f, 1.1f, true},
};
static const int N_BOTONES = (int)(sizeof BOTONES / sizeof BOTONES[0]);
static int botonElegido;
int NumEstilosBoton() { return N_BOTONES; }
void BotonesElegir(int i) { botonElegido = i >= 0 && i < N_BOTONES ? i : 0; }
int BotonesElegido() { return botonElegido; }
const char* NombreEstiloBoton(int i) { return BOTONES[i >= 0 && i < N_BOTONES ? i : 0].nombre; }

// Color del símbolo de cada botón según el estilo.
static uint32_t ColorSimbolo(int b, int modo)
{
    static const uint32_t CLASICO[4] = {0x86B4FF, 0xFF7C8A, 0x6FE6BE, 0xF59BDD};   // cruz, círculo, triángulo, cuadrado
    static const uint32_t VIVO[4] = {0x3D8BFF, 0xFF3B4E, 0x22D69A, 0xFF5FD2};
    static const uint32_t PASTEL[4] = {0x9BB8F0, 0xF0A0AA, 0x9FDCC4, 0xE8B4DC};
    int k = b == BM_CRUZ ? 0 : b == BM_CIRCULO ? 1 : b == BM_TRIANGULO ? 2 : 3;
    switch (modo)
    {
    case BC_VIVO: return VIVO[k];
    case BC_BLANCO: return 0xFFFFFF;
    case BC_NEGRO: return 0x18161F;
    case BC_TEMA: return C_ACENTO;
    case BC_DORADO: return 0xF2C14E;
    case BC_PASTEL: return PASTEL[k];
    default: return CLASICO[k];
    }
}

// El fondo del botón (centrado en cx, cy; w × h).
static void FondoBoton(const EstiloBoton& e, float cx, float cy, float w, float h, uint32_t colorBoton, float a)
{
    if (e.forma == BF_NADA) return;
    float r = e.forma == BF_CIRCULO || e.forma == BF_PILDORA ? h / 2 : e.forma == BF_REDONDEADO ? h * .3f : h * .1f;
    float x = cx - w / 2, y = cy - h / 2;
    switch (e.relleno)
    {
    case BR_OSCURO: Caja(x, y, w, h, r, 0x0E0C16, .85f * a); break;
    case BR_CLARO: Caja(x, y, w, h, r, 0xF4F2FA, .95f * a); break;
    case BR_COLOR: Caja(x, y, w, h, r, colorBoton, .95f * a); break;
    case BR_CONTORNO: Borde(x, y, w, h, r, 1.6f, colorBoton, .9f * a); break;
    case BR_VIDRIO: CajaVidrio(x, y, w, h, r, 0xFFFFFF, .3f, a); Borde(x, y, w, h, r, 1.2f, 0xFFFFFF, .5f * a); break;
    case BR_DEGRADADO:
        CajaDegradada(x, y, w, h, r, 0x5A5470, 0x1E1A2A, a);
        Velo(x, y, w, h * .45f, r, 0xFFFFFF, .22f * a, 0);
        break;
    default: Caja(x, y, w, h, r, 0xFFFFFF, .13f * a); break;
    }
}

float DibujarBoton(int b, float cx, float cy, float a)
{
    const EstiloBoton& e = BOTONES[botonElegido];
    // Color del texto de las teclas con nombre (L1, R1, OPTIONS): el del estilo si es blanco o negro; si no, el del tema
    // (sobre un fondo claro, oscuro).
    uint32_t tinta = e.color == BC_BLANCO || e.color == BC_NEGRO ? ColorSimbolo(BM_CRUZ, e.color)
                   : e.relleno == BR_CLARO ? 0x18161F : e.color == BC_TEMA ? C_ACENTO : e.color == BC_DORADO ? 0xF2C14E : C_TEXTO;
    if (b == BM_L1 || b == BM_R1 || b == BM_OPTIONS)
    {
        const char* t = b == BM_L1 ? "L1" : b == BM_R1 ? "R1" : "OPTIONS";
        float w = AnchoTexto(F_BOTON, t) + 22, h = 30;
        EstiloBoton tecla = e;
        if (tecla.forma == BF_CIRCULO) tecla.forma = BF_PILDORA;
        if (tecla.forma == BF_NADA) { tecla.forma = BF_REDONDEADO; tecla.relleno = BR_CONTORNO; }
        FondoBoton(tecla, cx, cy, w, h, e.relleno == BR_COLOR ? 0x6A6480 : tinta, a);
        Texto(F_BOTON, t, cx, cy - AltoLinea(F_BOTON) / 2, e.relleno == BR_COLOR ? 0xFFFFFF : tinta, a, 1);
        return w;
    }
    if (b == BM_TACTIL)
    {
        // El panel táctil del DualShock 4: un rectángulo ancho de esquinas redondas, con la marca de un dedo.
        const float w = 42, h = 24;
        Caja(cx - w / 2, cy - h / 2, w, h, 7, e.relleno == BR_CLARO ? 0xF4F2FA : 0xFFFFFF, (e.relleno == BR_CLARO ? .95f : .13f) * a);
        uint32_t c = e.relleno == BR_CLARO ? 0x18161F : e.color == BC_BLANCO ? 0xFFFFFF : 0xC9C2E8;
        Borde(cx - w / 2, cy - h / 2, w, h, 7, 2, c, a);
        Circulo(cx + 5, cy - 1, 4, c, a);
        return w;
    }
    const float r = 17 * e.tam, s = e.tam, g = e.grosor * e.tam;
    uint32_t colorBoton = ColorSimbolo(b, e.color == BC_VIVO ? BC_VIVO : BC_CLASICO);
    uint32_t c = ColorSimbolo(b, e.color);
    if (e.relleno == BR_COLOR && (e.color == BC_CLASICO || e.color == BC_VIVO)) c = 0xFFFFFF;   // sobre su color, en blanco
    if (e.brillo) Brillo(cx, cy, r * 1.9f, e.relleno == BR_COLOR ? colorBoton : c, .45f * a);
    FondoBoton(e, cx, cy, e.forma == BF_PILDORA ? r * 2.6f : r * 2, r * 2, e.relleno == BR_CONTORNO ? c : colorBoton, a);
    switch (b)
    {
    case BM_CRUZ:
        Linea(cx - 6.5f * s, cy - 6.5f * s, cx + 6.5f * s, cy + 6.5f * s, g, c, a);
        Linea(cx - 6.5f * s, cy + 6.5f * s, cx + 6.5f * s, cy - 6.5f * s, g, c, a);
        break;
    case BM_CIRCULO: Aro(cx, cy, 7.5f * s, g, c, a); break;
    case BM_CUADRADO: Borde(cx - 7 * s, cy - 7 * s, 14 * s, 14 * s, 1.5f * s, g, c, a); break;
    case BM_TRIANGULO:
    {
        float x0 = cx - 8 * s, x1 = cx + 8 * s, yb = cy + 5.5f * s, yt = cy - 8 * s;
        Linea(x0, yb, x1, yb, g, c, a);
        Linea(x0, yb, cx, yt, g, c, a);
        Linea(x1, yb, cx, yt, g, c, a);
        break;
    }
    }
    return e.forma == BF_PILDORA ? r * 2.6f : 2 * r;
}

void Capsula(float x, float y, float w, float h, float a)
{
    const EstiloBoton& e = BOTONES[botonElegido];
    // La forma: redonda (círculo y píldora), redondeada, cuadrada; "solo símbolo" queda como un contorno redondeado.
    float r = e.forma == BF_CUADRADO ? h * .12f : e.forma == BF_REDONDEADO || e.forma == BF_NADA ? h * .3f : h / 2;
    uint32_t c = e.color == BC_TEMA || e.color == BC_CLASICO || e.color == BC_VIVO ? C_ACENTO
               : e.color == BC_DORADO ? 0xF2C14E : e.color == BC_PASTEL ? 0xC9B8F0 : e.color == BC_NEGRO ? 0x18161F : 0xFFFFFF;
    if (e.brillo) Brillo(x + w / 2, y + h / 2, w * .6f, c, .35f * a);
    switch (e.forma == BF_NADA ? BR_CONTORNO : e.relleno)
    {
    case BR_OSCURO: Caja(x, y, w, h, r, 0x0E0C16, .85f * a); Borde(x, y, w, h, r, 2, c, .8f * a); break;
    case BR_CLARO: Caja(x, y, w, h, r, 0xF4F2FA, .9f * a); Borde(x, y, w, h, r, 2, c, .6f * a); break;
    case BR_COLOR: Caja(x, y, w, h, r, c, .55f * a); break;
    case BR_CONTORNO: Borde(x, y, w, h, r, 2.5f, c, .9f * a); break;
    case BR_VIDRIO: CajaVidrio(x, y, w, h, r, c, .3f, a); Borde(x, y, w, h, r, 1.5f, 0xFFFFFF, .5f * a); break;
    case BR_DEGRADADO:
        CajaDegradada(x, y, w, h, r, 0x5A5470, 0x1E1A2A, a);
        Velo(x, y, w, h * .45f, r, 0xFFFFFF, .22f * a, 0);
        Borde(x, y, w, h, r, 1.5f, c, .7f * a);
        break;
    default: Caja(x, y, w, h, r, c, .3f * a); Borde(x, y, w, h, r, 2, c, .8f * a); break;   // el de siempre
    }
}

uint32_t TextoCapsula()
{
    const EstiloBoton& e = BOTONES[botonElegido];
    bool claroColor = e.color == BC_DORADO || e.color == BC_PASTEL || (e.color != BC_TEMA && e.color != BC_CLASICO && e.color != BC_VIVO && e.color != BC_NEGRO);
    bool claro = e.forma != BF_NADA && (e.relleno == BR_CLARO || (e.relleno == BR_COLOR && claroColor));
    return claro ? 0x1A1626 : C_TEXTO;
}

// Lo que mide un botón de ancho con el estilo elegido (para alinear las pistas).
static float AnchoBoton(int b)
{
    if (b == BM_L1 || b == BM_R1 || b == BM_OPTIONS) return AnchoTexto(F_BOTON, b == BM_OPTIONS ? "OPTIONS" : "L1") + 22;
    if (b == BM_TACTIL) return 42;
    const EstiloBoton& e = BOTONES[botonElegido];
    return 17 * e.tam * (e.forma == BF_PILDORA ? 2.6f : 2);
}

float Pista(int b, const char* texto, float x, float cy, float a, bool medir)
{
    float wb = AnchoBoton(b);
    if (!medir)
    {
        DibujarBoton(b, x + wb / 2, cy, a);
        Texto(F_CUERPO_M, texto, x + wb + 12, cy - AltoLinea(F_CUERPO_M) / 2, C_TEXTO_2, a);
    }
    return wb + 12 + AnchoTexto(F_CUERPO_M, texto);
}

void Pistas(const int* botones, const char* const* textos, int n, float a) { PistasEn(botones, textos, n, a, 1012); }

void PistasEn(const int* botones, const char* const* textos, int n, float a, float cy, float derecha)
{
    const float sep = 40;
    float total = 0;
    for (int i = 0; i < n; i++) total += Pista(botones[i], textos[i], 0, 0, a, true) + (i ? sep : 0);
    float x = (derecha < 0 ? ANCHO - MARGEN : derecha) - total;
    for (int i = 0; i < n; i++)
    {
        x += Pista(botones[i], textos[i], x, cy, a) + sep;
    }
}

float Pastilla(const char* texto, float x, float y, uint32_t fondo, float alfaFondo, uint32_t color, float a)
{
    float w = AnchoTexto(F_PEQUENA, texto) + 28, h = 36;
    Caja(x, y, w, h, h / 2, fondo, alfaFondo * a);
    Texto(F_PEQUENA, texto, x + 14, y + (h - AltoLinea(F_PEQUENA)) / 2, color, a);
    return w;
}

// El fondo se calcula en una imagen de 480 × 270 (repartida entre los núcleos) y se agranda a la pantalla con filtro:
// degradado vertical, tres luces grandes que se mueven lentamente y una viñeta arriba y abajo.
struct Luz { float x, y, r2inv; float c[3]; float a; };
struct DatosFondo { Luz luces[3]; float arriba[3], abajo[3]; };
static uint32_t fondo[FONDO_W * FONDO_H];

static void Componentes(uint32_t c, float* f) { f[0] = (c >> 16 & 255); f[1] = (c >> 8 & 255); f[2] = (c & 255); }

static void PintarFondoParte(int parte, int partes, void* arg)
{
    const DatosFondo& d = *(const DatosFondo*)arg;
    int y0 = FONDO_H * parte / partes, y1 = FONDO_H * (parte + 1) / partes;
    for (int y = y0; y < y1; y++)
    {
        float ty = (y + .5f) / FONDO_H;
        float base[3];
        for (int k = 0; k < 3; k++) base[k] = d.arriba[k] + (d.abajo[k] - d.arriba[k]) * ty;
        // Viñeta: oscurece el 17 % de arriba y el 24 % de abajo.
        float vin = ty < .17f ? .35f * (1 - ty / .17f) : ty > .76f ? .6f * (ty - .76f) / .24f : 0;
        uint32_t* fila = fondo + y * FONDO_W;
        for (int x = 0; x < FONDO_W; x++)
        {
            float c[3] = {base[0], base[1], base[2]};
            for (int l = 0; l < 3; l++)
            {
                const Luz& L = d.luces[l];
                float dx = x + .5f - L.x, dy = y + .5f - L.y;
                float t = 1 - (dx * dx + dy * dy) * L.r2inv;
                if (t <= 0) continue;
                float w = t * t * L.a;
                for (int k = 0; k < 3; k++) c[k] += (L.c[k] - c[k]) * w;
            }
            uint32_t v = 0;
            for (int k = 0; k < 3; k++)
            {
                float f = c[k] * (1 - vin) + .5f;
                v = v << 8 | (uint32_t)(f > 255 ? 255 : f);
            }
            fila[x] = v;
        }
    }
}

void FondoAnimado(float t, bool animado)
{
    if (!animado) t = 0;
    DatosFondo d;
    Componentes(C_FONDO_ARRIBA, d.arriba);
    Componentes(C_FONDO_ABAJO, d.abajo);
    // Posición (en la pantalla de 1920 × 1080), radio, color y fuerza de cada luz; cada una con su propio ritmo.
    const float luces[3][5] = {
        {420 + 260 * sinf(t * .11f), 260 + 140 * cosf(t * .09f), 820, (float)0x6A2DBF, .22f},
        {1560 + 220 * cosf(t * .08f + 1), 860 + 120 * sinf(t * .13f), 760, (float)0x00A89A, .12f},
        {1180 + 300 * sinf(t * .06f + 2), 160 + 90 * cosf(t * .1f + 1), 640, (float)0x9B3FD0, .13f},
    };
    for (int i = 0; i < 3; i++)
    {
        Luz& L = d.luces[i];
        L.x = luces[i][0] / 4; L.y = luces[i][1] / 4;
        float r = luces[i][2] / 4;
        L.r2inv = 1 / (r * r);
        Componentes((uint32_t)luces[i][3], L.c);
        L.a = luces[i][4];
    }
    EnParalelo(PintarFondoParte, &d);
    FondoReducido(fondo);
}

float Radio(float r) { return r * temaActual->radio; }

void Panel(float x, float y, float w, float h, float radio, float a)
{
    const Tema& t = *temaActual;
    float r = Radio(radio);
    switch (t.material)
    {
    case MT_VIDRIO:
        Sombra(x, y + 10, w, h, r, 34, .28f * a, true);
        CajaVidrio(x, y, w, h, r, t.panel, .15f, a);
        Velo(x, y, w, h * .45f, r, 0xFFFFFF, .14f * a, 0);
        Borde(x, y, w, h, r, 1.5f, 0xFFFFFF, .38f * a);
        break;
    case MT_NEON:
        Caja(x, y, w, h, r, t.panel, .88f * a);
        Borde(x - 3, y - 3, w + 6, h + 6, r + 3, 6, t.acento, .16f * a);
        Borde(x, y, w, h, r, 2, t.acento, .95f * a);
        break;
    case MT_DEGRADADO:
        Sombra(x, y + 14, w, h, r, 36, .4f * a, true);
        CajaDegradada(x, y, w, h, r, t.panelClaro, t.panel, .95f * a);
        break;
    case MT_PLANO:
        Caja(x, y, w, h, r, t.panel, a);
        break;
    case MT_CONTORNO:
        Caja(x, y, w, h, r, t.panel, .62f * a);
        Borde(x, y, w, h, r, 1.5f, t.texto3, .9f * a);
        break;
    case MT_PAPEL:
        Caja(x + 8, y + 10, w, h, r, 0x000000, .16f * a);
        Caja(x, y, w, h, r, t.panel, a);
        Borde(x, y, w, h, r, 1.5f, t.texto3, .35f * a);
        break;
    case MT_METAL:
        Sombra(x, y + 14, w, h, r, 30, .45f * a, true);
        CajaDegradada(x, y, w, h, r, t.panelClaro, t.panel, a);
        Velo(x, y, w, h * .3f, r, 0xFFFFFF, .16f * a, 0);
        Borde(x, y, w, h, r, 1.5f, t.texto3, .5f * a);
        break;
    default:
        Sombra(x, y + 18, w, h, r, 40, .5f * a, true);
        Caja(x, y, w, h, r, t.panel, .94f * a);
        Borde(x, y, w, h, r, 1.5f, 0xFFFFFF, .06f * a);
        break;
    }
}
