// La interfaz del emulador de SNES, con estilo Super Nintendo: fondo morado, los cuatro colores de los botones como
// acentos, y una cuadrícula de carátulas. Si la carátula del juego ya se bajó (de internet, a /data), se muestra; si no,
// una tarjeta con las iniciales. Reutiliza el dibujo en HD del proyecto anterior.
#include "interfaz.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../comun/estilo.h"
#include "../comun/lienzo.h"
#include "../comun/texto.h"
#include "biblioteca.h"
#include "caratula.h"
#include "fondo.h"
#include "juego.h"

static const char* const VERSION = "0.7";

static Juego juegos[MAX_JUEGOS];
static int nJuegos;
static int sel;
static float scrollVis, foco[MAX_JUEGOS];
static float entrada;
static float tResultado = 99;

// ---------- presentación: el logo al abrir ----------
#ifndef RUTA_ASSETS
#define RUTA_ASSETS "/app0/assets"
#endif
static uint32_t* logo;
static int logoW, logoH;
static float tLogo;
static const float DURACION_LOGO = 3.2f;

// Devuelve true mientras se muestra (la biblioteca espera detrás).
static bool Presentacion(float dt, const Mando& m)
{
    if (tLogo >= DURACION_LOGO) return false;
    tLogo += dt;
    if (tLogo > .6f && (m.pulsados & (B_CRUZ | B_OPTIONS)) && tLogo < DURACION_LOGO - .5f) tLogo = DURACION_LOGO - .5f;   // saltar
    Caja(0, 0, ANCHO, ALTO, 0, 0x000000, 1);
    // Aparece en 0,9 s, queda, y se funde en el último medio segundo.
    float a = tLogo < .9f ? Facilidad(tLogo / .9f) : tLogo > DURACION_LOGO - .5f ? (DURACION_LOGO - tLogo) / .5f : 1;
    if (a < 0) a = 0;
    float crece = .94f + .06f * Facilidad(tLogo / DURACION_LOGO < 1 ? tLogo / DURACION_LOGO : 1);
    // Fondo negro puro: el logo trae su propio resplandor y sus bordes ya se funden en negro.
    if (logo)
    {
        float w = 1180 * crece, h = w * logoH / logoW;
        Imagen(logo, logoW, logoH, (ANCHO - w) / 2, (ALTO - h) / 2, w, h, a, true);
    }
    else Texto(F_LOGO, "SNESUI", ANCHO / 2, ALTO / 2 - 70, C_TEXTO, a, 1);
    return true;
}   // tiempo desde que se pidió bajar carátulas (para mostrar el resultado un rato)

// Progreso de la descarga de carátulas: una pastilla arriba al centro con una barra; al terminar, el resultado.
static void DibujarDescarga(float dt)
{
    ProgresoDescarga p = Progreso();
    if (!p.activa && !p.terminada) return;
    if (!p.activa) tResultado += dt;
    float a = p.activa ? 1 : (tResultado < 6 ? 1 : 7 - tResultado);
    if (a <= 0) return;
    float w = 720, h = 76, x = (ANCHO - w) / 2, y = 40;
    Sombra(x, y, w, h, 22, 24, .45f * a);
    Caja(x, y, w, h, 22, C_PANEL, .96f * a);
    char t[200];
    if (p.activa)
    {
        snprintf(t, sizeof t, "Bajando carátulas   %d de %d", p.hechas, p.total);
        Texto(F_CUERPO_M, t, x + 28, y + 12, C_TEXTO, a);
        char corto[120];
        Texto(F_PEQUENA, Acortar(F_PEQUENA, p.detalle, w - 56, corto, sizeof corto), x + 28, y + 44, C_TEXTO_3, a);
        float f = p.total ? (float)p.hechas / p.total : 0;
        Caja(x + 20, y + h - 8, w - 40, 4, 2, 0xFFFFFF, .1f * a);
        Caja(x + 20, y + h - 8, (w - 40) * f, 4, 2, C_ACENTO, a);
    }
    else if (p.fallo)
    {
        Circulo(x + 34, y + 38, 8, C_MAL, a);
        Texto(F_CUERPO_M, "No se pudieron bajar las carátulas", x + 56, y + 12, C_TEXTO, a);
        char corto[160];
        Texto(F_PEQUENA, Acortar(F_PEQUENA, p.detalle, w - 84, corto, sizeof corto), x + 56, y + 44, C_TEXTO_2, a);
    }
    else
    {
        Circulo(x + 34, y + 38, 8, C_BIEN, a);
        snprintf(t, sizeof t, p.bajadas == 1 ? "%d carátula nueva" : "%d carátulas nuevas", p.bajadas);
        Texto(F_CUERPO_M, t, x + 56, y + 12, C_TEXTO, a);
        if (p.sinCaratula) snprintf(t, sizeof t, "%d juegos no tienen carátula en la base de RetroArch", p.sinCaratula);
        else snprintf(t, sizeof t, "Todos los juegos tienen carátula");
        Texto(F_PEQUENA, t, x + 56, y + 44, C_TEXTO_2, a);
    }
}

static const int COLS = 5;
static const float CW = 300, CH = 300, GAPX = 36, GAPY = 56;
static const float X0 = MARGEN, Y0 = 258;

void InterfazIniciar()
{
    logo = CargarImagen(RUTA_ASSETS "/logo.png", &logoW, &logoH);
    nJuegos = BuscarJuegos(juegos, MAX_JUEGOS);
}

// Franja con los cuatro colores de los botones de la Super Famicom (el toque "SNES").
static void FranjaSnes(float x, float y, float w, float h, float a)
{
    uint32_t c[4] = {C_SNES_Y, C_SNES_X, C_SNES_A, C_SNES_B};
    float bw = w / 4;
    for (int i = 0; i < 4; i++) Caja(x + i * bw, y, bw + 1, h, 0, c[i], a);
}

static void Tarjeta(const Juego& j, float x, float y, float f)
{
    float s = 1 + .06f * f;
    float w = CW * s, h = CH * s, cx = x + CW / 2, cy = y + CH / 2;
    float X = cx - w / 2, Y = cy - h / 2;
    Caratula car = ObtenerCaratula(j.clave);

    if (f > .01f) Brillo(cx, cy + 10, 250, car.px ? 0xFFFFFF : j.color1, (car.px ? .22f : .4f) * f);
    Sombra(X, Y + 14 + 6 * f, w, h, 20, 28 + 12 * f, .55f, true);

    if (car.px)
    {
        // La carátula real, con un marco oscuro tipo cartucho.
        Caja(X, Y, w, h, 20, 0x0C0A18, 1);
        float m = 12;
        // Mantener la proporción de la carátula dentro del hueco.
        float iw = w - 2 * m, ih = h - 2 * m;
        float escala = iw / car.w < ih / car.h ? iw / car.w : ih / car.h;
        float dw = car.w * escala, dh = car.h * escala;
        Imagen(car.px, car.w, car.h, cx - dw / 2, cy - dh / 2, dw, dh, 1, true);
    }
    else
    {
        CajaDegradada(X, Y, w, h, 20, j.color1, j.color2, 1);
        FranjaSnes(X + w * .12f, Y + 14, w * .76f, 8, .5f);
        Velo(X, Y, w, h * .5f, 20, 0xFFFFFF, .06f, 0);
        char buf[80];
        Texto(F_INICIAL, j.iniciales, X + 26, Y + h * .30f, 0xFFFFFF, .95f);
        Velo(X, Y + h * .52f, w, h * .48f, 20, 0x05070F, 0, .62f);
        Texto(F_ETIQUETA, Acortar(F_ETIQUETA, j.nombre, w - 48, buf, sizeof buf), X + 26, Y + h - 76, C_TEXTO);
        Texto(F_PEQUENA, j.pal ? "PAL" : "NTSC", X + 26, Y + h - 42, C_TEXTO_2);
    }
    // Aro de selección en morado SNES.
    if (f > .01f) Borde(X - 6, Y - 6, w + 12, h + 12, 26, 4, C_ACENTO, .95f * f);
}

static void Biblioteca(float t, float dt, const Mando& m);

void InterfazCuadro(float t, float dt, const Mando& m)
{
    // Si hay un juego abierto, solo la pantalla de juego (sin gastar en el fondo ni en la biblioteca, que no se ven).
    if (JuegoActivo()) { JuegoCuadro(t, dt, m); return; }
    if (Presentacion(dt, m)) { entrada = 0; return; }
    // Después del logo, la biblioteca aparece desde negro.
    Suave(entrada, 1, 5, dt);
    Biblioteca(t, dt, entrada > .98f ? m : Mando{});
    if (entrada < .995f) Caja(0, 0, ANCHO, ALTO, 0, 0x000000, 1 - entrada);
}

static void Biblioteca(float t, float dt, const Mando& m)
{
    FondoPixel(t);

    // Cabecera: logo con una franja de los cuatro colores de la SNES.
    Texto(F_TITULO, "SNESUI", MARGEN, 58, C_TEXTO);
    float lw = AnchoTexto(F_TITULO, "SNESUI");
    FranjaSnes(MARGEN, 118, lw, 7, 1);
    char v[64];
    snprintf(v, sizeof v, "versión %s", VERSION);
    Texto(F_PEQUENA, v, ANCHO - MARGEN, 70, C_TEXTO_3, 1, 2);
    char cont[48];
    snprintf(cont, sizeof cont, nJuegos == 1 ? "%d juego" : "%d juegos", nJuegos > 0 ? nJuegos : 0);
    Texto(F_CUERPO, cont, ANCHO - MARGEN, 100, C_TEXTO_2, 1, 2);


    if (m.pulsados & B_CRUZ) { Sonar(SON_ACEPTAR); JuegoAbrir(juegos[sel].ruta, juegos[sel].nombre); }
    if (m.pulsados & B_TRIANGULO) { Sonar(SON_ACEPTAR); nJuegos = BuscarJuegos(juegos, MAX_JUEGOS); }
    if ((m.pulsados & B_CUADRADO) && nJuegos > 0)
    {
        static const char* lista[MAX_JUEGOS];
        for (int i = 0; i < nJuegos; i++) lista[i] = juegos[i].clave;
        Sonar(EmpezarDescarga(lista, nJuegos) ? SON_ACEPTAR : SON_ERROR);
        tResultado = 0;
    }
    int antes = sel;
    if (m.pulsados & B_IZQ) sel--;
    if (m.pulsados & B_DER) sel++;
    if (m.pulsados & B_ARRIBA) sel -= COLS;
    if (m.pulsados & B_ABAJO) sel += COLS;
    if (sel < 0) sel = 0;
    if (sel >= nJuegos) sel = nJuegos > 0 ? nJuegos - 1 : 0;
    if (sel != antes) Sonar(SON_MOVER);

    if (nJuegos <= 0)
    {
        float w = 1000, h = 420, x = (ANCHO - w) / 2, y = 330;
        Sombra(x, y + 18, w, h, 30, 50, .5f, true);
        Caja(x, y, w, h, 30, C_PANEL, .8f);
        Borde(x, y, w, h, 30, 1.5f, 0xFFFFFF, .08f);
        FranjaSnes(x + w / 2 - 120, y + 70, 240, 10, .9f);
        Texto(F_TITULO, "Todavía no hay juegos", x + w / 2, y + 170, C_TEXTO, 1, 1);
        Parrafo(F_CUERPO, "Copia tus roms de SNES (.smc o .sfc) en cualquier parte del USB y pulsa triángulo para buscar. "
                          "Después pulsa cuadrado para bajar las carátulas de internet.", x + 80, y + 240, w - 160, C_TEXTO_2);
        Texto(F_PEQUENA, CarpetasDeJuegos(), x + w / 2, y + h - 48, C_TEXTO_3, 1, 1);
        return;
    }

    int fila = sel / COLS;
    const float paso = CH + GAPY;
    float objScroll = fila * paso - paso;
    if (objScroll < 0) objScroll = 0;
    int filas = (nJuegos + COLS - 1) / COLS;
    float maxScroll = filas * paso - (ALTO - Y0 - 110);
    if (maxScroll < 0) maxScroll = 0;
    if (objScroll > maxScroll) objScroll = maxScroll;
    Suave(scrollVis, objScroll, 12, dt);

    Recorte(0, Y0 - 30, ANCHO, ALTO - Y0 - 80);
    for (int i = 0; i < nJuegos; i++)
    {
        Suave(foco[i], i == sel ? 1 : 0, 14, dt);
        float x = X0 + (i % COLS) * (CW + GAPX);
        float y = Y0 + (i / COLS) * paso - scrollVis;
        if (y > ALTO || y + CH < 100) continue;
        if (i != sel) Tarjeta(juegos[i], x, y, foco[i]);
    }
    float sx = X0 + (sel % COLS) * (CW + GAPX), sy = Y0 + (sel / COLS) * paso - scrollVis;
    Tarjeta(juegos[sel], sx, sy, foco[sel]);
    SinRecorte();

    // Pie: datos del juego + pistas.
    Velo(0, ALTO - 86, ANCHO, 86, 0, 0x120A24, 0, .82f);
    const Juego& j = juegos[sel];
    char buf[120];
    snprintf(buf, sizeof buf, "%s   ·   %d KB   ·   %s%s", j.mapeo, j.tamKB, j.pal ? "PAL" : "NTSC", j.tieneRam ? "   ·   guarda partida" : "");
    Texto(F_CUERPO_M, j.nombre, MARGEN, ALTO - 70, C_TEXTO);
    Texto(F_PEQUENA, buf, MARGEN, ALTO - 38, C_TEXTO_2);
    int b[3] = {BM_CRUZ, BM_TRIANGULO, BM_CUADRADO};
    const char* tx[3] = {"Jugar", "Buscar juegos", "Bajar carátulas"};
    Pistas(b, tx, 3);
    DibujarDescarga(dt);
}
