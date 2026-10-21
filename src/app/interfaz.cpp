// La interfaz del emulador de SNES, con estilo SNES: fondo morado, los cuatro colores de los botones como
// acentos, y una cuadrícula de carátulas. Si la carátula del juego ya se bajó (de internet, a /data), se muestra; si no,
// una tarjeta con las iniciales. Reutiliza el dibujo en HD del proyecto anterior.
#include "interfaz.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../comun/musica.h"
#include "prueba_jit.h"
#include "../comun/estilo.h"
#include "../comun/lienzo.h"
#include "../comun/texto.h"
#include "biblioteca.h"
#include "asistente.h"
#include "bios.h"
#include "respaldo.h"
#include "metadatos.h"
#include "ajustes.h"
#include "arcade.h"
#include "caratula.h"
#include "consola.h"
#include "copiar.h"
#include "fondo.h"
#include "registro.h"
#include "juego.h"

static const char* const VERSION = "1.5";
// Número de compilación (lo genera construir.sh): commit y fecha; en el simulador de PC, "pc".
#ifdef CON_COMPILACION
#include "compilacion.h"
#else
#define COMPILACION "pc"
#endif
static const char* const AUTOR = "RudolphGamerBkn";   // el creador de ALLESIU (Configuración → Acerca de)
static const char* const PAGINA = "github.com/worldchane/snesui";

static const Juego* juegos;   // la lista de la biblioteca (ver biblioteca.h; cambia cuando termina una búsqueda)
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
    else Texto(F_LOGO, "ALLESIU", ANCHO / 2, ALTO / 2 - 70, C_TEXTO, a, 1);
    return true;
}   // tiempo desde que se pidió bajar carátulas (para mostrar el resultado un rato)

// Las pastillas de progreso (carátulas y copia) van arriba a la derecha, en la franja del logo: ahí no tapan las
// pestañas ni los juegos. La de carátulas en la esquina; la de la copia, a su izquierda si están las dos.
static const float PASTILLA_W = 560, PASTILLA_H = 66, PASTILLA_Y = 14;

// Progreso de la descarga de carátulas: una pastilla con una barra; al terminar, el resultado.
static void DibujarDescarga(float dt)
{
    ProgresoDescarga p = Progreso();
    if (!p.activa && !p.terminada) return;
    // La automática no avisa si no hubo red ni si no había nada que bajar.
    if (p.automatica && !p.activa && (p.fallo || !p.bajadas)) return;
    if (!p.activa) tResultado += dt;
    float a = p.activa ? 1 : (tResultado < 6 ? 1 : 7 - tResultado);
    if (a <= 0) return;
    float w = PASTILLA_W, h = PASTILLA_H, x = ANCHO - MARGEN - w, y = PASTILLA_Y;
    Panel(x, y, w, h, 22, a);
    char t[200];
    if (p.activa)
    {
        // Mientras se baja la lista de nombres del servidor (puede tardar unos segundos), sin un "0 de N" que parezca trabado.
        if (p.leyendoLista && !p.hechas) snprintf(t, sizeof t, "Preparando las carátulas…");
        else snprintf(t, sizeof t, "Bajando carátulas   %d de %d", p.hechas, p.total);
        Texto(F_CUERPO_M, t, x + 28, y + 6, C_TEXTO, a);
        char corto[120];
        Texto(F_PEQUENA, Acortar(F_PEQUENA, p.detalle, w - 56, corto, sizeof corto), x + 28, y + 37, C_TEXTO_3, a);
        float f = p.total ? (float)p.hechas / p.total : 0;
        Caja(x + 20, y + h - 8, w - 40, 4, 2, 0xFFFFFF, .1f * a);
        Caja(x + 20, y + h - 8, (w - 40) * f, 4, 2, C_ACENTO, a);
    }
    else if (p.fallo)
    {
        Circulo(x + 34, y + 33, 8, C_MAL, a);
        Texto(F_CUERPO_M, "No se pudieron bajar las carátulas", x + 56, y + 6, C_TEXTO, a);
        char corto[160];
        Texto(F_PEQUENA, Acortar(F_PEQUENA, p.detalle, w - 84, corto, sizeof corto), x + 56, y + 37, C_TEXTO_2, a);
    }
    else
    {
        Circulo(x + 34, y + 33, 8, C_BIEN, a);
        snprintf(t, sizeof t, p.bajadas == 1 ? "%d carátula nueva" : "%d carátulas nuevas", p.bajadas);
        Texto(F_CUERPO_M, t, x + 56, y + 6, C_TEXTO, a);
        if (p.sinCaratula) snprintf(t, sizeof t, "%d juegos no tienen carátula en la base de RetroArch", p.sinCaratula);
        else snprintf(t, sizeof t, "Todos los juegos tienen carátula");
        Texto(F_PEQUENA, t, x + 56, y + 37, C_TEXTO_2, a);
    }
}

// Medidas de la cuadrícula: las fija el estilo de menú (MenuElegir). Al empezar, 5 columnas de tarjetas con la forma de
// las cajas de SNES (apaisadas, 1,38:1) y 3 filas en pantalla.
static int COLS = 5;
static float CW = 300, CH = 218, GAPX = 36, GAPY = 50;
static float X0 = MARGEN, Y0 = 178;
static const float PIE = 46;   // alto de la barra de abajo (datos del juego y botones en una línea), pegada al borde

// ---------- pestañas: todos, recientes y favoritos ----------
// Los recientes (los últimos abiertos, el más nuevo primero) y los favoritos se guardan por clave (nombre de archivo) en
// /data/allesiu/recientes.txt y favoritos.txt, una por línea.
enum { V_TODOS, V_RECIENTES, V_FAVORITOS, V_CONFIG, NUM_VISTAS };
static const char* const VISTAS[NUM_VISTAS] = {"Todos", "Recientes", "Favoritos", "Configuración"};
// Las pestañas: Todos, una por cada consola que tiene juegos (si hay más de una), Recientes, Favoritos y Configuración.
// 'vistaActual' es la posición en esta lista; Tipo() dice qué es y Filtro() la consola (-1 = todas).
struct Pestana { int tipo, sistema; const char* nombre; };
static Pestana pestanas[NUM_VISTAS + NUM_SISTEMAS];
static int nPestanas, vistaActual;
static float pastillaX, pastillaW, tiraDesde;   // la pastilla de la pestaña elegida y el desplazamiento de la tira
static int Tipo() { return nPestanas ? pestanas[vistaActual].tipo : V_TODOS; }
static int Filtro() { return nPestanas ? pestanas[vistaActual].sistema : -1; }
static int vista[MAX_JUEGOS], nVista;   // índices de 'juegos' que se muestran
static const int MAX_RECIENTES = 15, MAX_FAVORITOS = 300;
static char recientes[MAX_RECIENTES][96], favoritos[MAX_FAVORITOS][96];
static int nRecientes, nFavoritos;

static int LeerLista(const char* archivo, char (*l)[96], int max)
{
    char ruta[160];
    snprintf(ruta, sizeof ruta, "%s/%s", CARPETA_DATOS, archivo);
    static char txt[32 * 1024];
    if (LeerTexto(ruta, txt, sizeof txt) <= 0) return 0;
    int n = 0;
    for (char* p = txt; *p && n < max;)
    {
        char* fin = strchr(p, '\n');
        if (fin) *fin = 0;
        if (*p) snprintf(l[n++], 96, "%s", p);
        if (!fin) break;
        p = fin + 1;
    }
    return n;
}

static void GuardarLista(const char* archivo, char (*l)[96], int n)
{
    char ruta[160];
    snprintf(ruta, sizeof ruta, "%s/%s", CARPETA_DATOS, archivo);
    static char txt[32 * 1024];
    int k = 0;
    for (int i = 0; i < n && k < (int)sizeof txt - 100; i++) k += snprintf(txt + k, sizeof txt - k, "%s\n", l[i]);
    txt[k] = 0;
    EscribirTexto(ruta, txt);
}

static bool EsFavorito(const char* clave)
{
    for (int i = 0; i < nFavoritos; i++) if (!strcmp(favoritos[i], clave)) return true;
    return false;
}

static void CambiarFavorito(const char* clave)
{
    for (int i = 0; i < nFavoritos; i++)
        if (!strcmp(favoritos[i], clave))
        {
            memmove(favoritos[i], favoritos[i + 1], (size_t)(nFavoritos - i - 1) * sizeof favoritos[0]);
            nFavoritos--;
            GuardarLista("favoritos.txt", favoritos, nFavoritos);
            return;
        }
    if (nFavoritos < MAX_FAVORITOS) snprintf(favoritos[nFavoritos++], 96, "%s", clave);
    GuardarLista("favoritos.txt", favoritos, nFavoritos);
}

static void AnotarReciente(const char* clave)
{
    int i = 0;
    while (i < nRecientes && strcmp(recientes[i], clave)) i++;
    if (i == nRecientes && nRecientes < MAX_RECIENTES) nRecientes++;
    if (i >= MAX_RECIENTES) i = MAX_RECIENTES - 1;
    memmove(recientes[1], recientes[0], (size_t)i * sizeof recientes[0]);   // correr los anteriores un lugar
    snprintf(recientes[0], 96, "%s", clave);
    GuardarLista("recientes.txt", recientes, nRecientes);
}

// Búsqueda por nombre: el texto escrito con el teclado en pantalla. Coincide si aparece en cualquier parte del nombre,
// sin distinguir mayúsculas.
static char busqueda[32];
static bool Coincide(const char* nombre)
{
    if (!busqueda[0]) return true;
    for (const char* p = nombre; *p; p++)
    {
        const char *a = p, *b = busqueda;
        while (*a && *b)
        {
            char x = *a, y = *b;
            if (x >= 'a' && x <= 'z') x -= 32;
            if (y >= 'a' && y <= 'z') y -= 32;
            if (x != y) break;
            a++, b++;
        }
        if (!*b) return true;
    }
    return false;
}

// Rehace las pestañas (cuando cambia la lista de juegos) sin perder la elegida.
static void ArmarPestanas()
{
    int tipo = Tipo(), sistema = Filtro();
    bool hay[NUM_SISTEMAS] = {};
    int distintas = 0;
    for (int i = 0; i < nJuegos; i++)
        if (Visible(juegos[i]) && juegos[i].sistema >= 0 && juegos[i].sistema < NUM_SISTEMAS && !hay[juegos[i].sistema])
            hay[juegos[i].sistema] = true, distintas++;
    nPestanas = 0;
    pestanas[nPestanas++] = {V_TODOS, -1, VISTAS[V_TODOS]};
    if (distintas > 1)
        for (int k = 0; k < NUM_SISTEMAS; k++)
            if (hay[k]) pestanas[nPestanas++] = {V_TODOS, k, ConsolaDe(k).nombre};
    for (int v = V_RECIENTES; v < NUM_VISTAS; v++) pestanas[nPestanas++] = {v, -1, VISTAS[v]};
    vistaActual = 0;
    for (int i = 0; i < nPestanas; i++)
        if (pestanas[i].tipo == tipo && pestanas[i].sistema == sistema) vistaActual = i;
}

static void ArmarVista()
{
    nVista = 0;
    if (!nPestanas) ArmarPestanas();
    int filtro = Filtro();
    if (Tipo() == V_CONFIG) { sel = 0; return; }
    if (Tipo() == V_RECIENTES)
    {
        for (int r = 0; r < nRecientes; r++)
            for (int i = 0; i < nJuegos; i++)
                if (Visible(juegos[i]) && !strcmp(juegos[i].clave, recientes[r])) { if (Coincide(juegos[i].nombre)) vista[nVista++] = i; break; }
    }
    else
        for (int i = 0; i < nJuegos; i++)
            if (Visible(juegos[i]) && (Tipo() == V_TODOS || EsFavorito(juegos[i].clave)) && (filtro < 0 || juegos[i].sistema == filtro) &&
                Coincide(juegos[i].nombre))
                vista[nVista++] = i;
    if (sel >= nVista) sel = nVista > 0 ? nVista - 1 : 0;
}

static void MenuElegir(int i);

void InterfazIniciar()
{
    static char version[64];
    snprintf(version, sizeof version, "%s (compilación %s)", VERSION, COMPILACION);
    RegistroIniciar(version);
    logo = CargarImagen(RUTA_ASSETS "/logo.png", &logoW, &logoH);
    BibliotecaIniciar();   // la lista guardada al instante; la búsqueda en el USB sigue en segundo plano
    juegos = BibliotecaJuegos(&nJuegos);
    nRecientes = LeerLista("recientes.txt", recientes, MAX_RECIENTES);
    nFavoritos = LeerLista("favoritos.txt", favoritos, MAX_FAVORITOS);
    AjustesLeer();
    MetadatosCargar();
    TemaElegir(General().tema);   // los estilos guardados
    MenuElegir(General().menu);
    BotonesElegir(General().botones);
    AsistenteIniciar(VERSION);   // la primera vez, el asistente; con una versión nueva, las novedades
    ArmarVista();
    // La música de la biblioteca: todas las canciones en orden aleatorio (distinto cada vez que se abre la app).
    MusicaEncender(General().musica != 0, (unsigned)(Segundos() * 1000));
}

// Íconos de dónde está un juego, centrados en (cx, cy); s = escala (1 ≈ 30 px de ancho).
//   USB: un pendrive acostado (cuerpo, conector y los dos agujeros del conector).
//   Consola: un disco duro (caja, la franja de abajo y la luz).
static void IconoUsb(float cx, float cy, float s, uint32_t c, float a)
{
    float x = cx - 15 * s, y = cy - 7 * s;
    Caja(x, y, 19 * s, 14 * s, 3 * s, c, a);                        // cuerpo
    Caja(x + 19 * s, y + 3 * s, 10 * s, 8 * s, 1.5f * s, c, a);     // conector
    Caja(x + 22 * s, y + 5 * s, 2.2f * s, 2 * s, 0, 0x0A0614, a);   // agujeros
    Caja(x + 25.5f * s, y + 5 * s, 2.2f * s, 2 * s, 0, 0x0A0614, a);
    Caja(x + 4 * s, y + 5.5f * s, 9 * s, 3 * s, 1.5f * s, 0x0A0614, .45f * a);   // etiqueta
}
static void IconoDisco(float cx, float cy, float s, uint32_t c, float a)
{
    float x = cx - 13 * s, y = cy - 9 * s;
    Caja(x, y, 26 * s, 18 * s, 3.5f * s, c, a);                       // caja
    Caja(x + 3 * s, y + 11.5f * s, 20 * s, 2.2f * s, 1 * s, 0x0A0614, .55f * a);   // franja
    Circulo(x + 20.5f * s, y + 5.5f * s, 1.8f * s, 0x0A0614, .8f * a); // luz (apagada)
    Circulo(x + 15.5f * s, y + 5.5f * s, 1.8f * s, 0x0A0614, .8f * a);
}
static void IconoLugar(bool usb, float cx, float cy, float s, float a)
{
    if (usb) IconoUsb(cx, cy, s, C_AVISO, a);
    else IconoDisco(cx, cy, s, C_ACENTO_2, a);
}

// Estrella dibujada con líneas (la letra no trae el símbolo).
static void Estrella(float cx, float cy, float r, uint32_t c, float a)
{
    float px[5], py[5];
    for (int i = 0; i < 5; i++)
    {
        float ang = -1.5708f + i * 1.2566f;
        px[i] = cx + r * cosf(ang);
        py[i] = cy + r * sinf(ang);
    }
    for (int i = 0; i < 5; i++) Linea(px[i], py[i], px[(i + 2) % 5], py[(i + 2) % 5], r * .32f, c, a);
}

// Navegación rápida.
static int FILAS_PAGINA = 3;          // filas que entran en pantalla (según el estilo de menú)
static float esperaLetra;             // repetición del salto de letra con el stick derecho
static char letraVisible;             // la letra a la que se saltó (se muestra grande un momento)
static float tLetra = 99;

// La letra con la que se agrupa un juego: A–Z, o '#' si empieza con un número o un signo.
static char Letra(const char* nombre)
{
    for (const char* p = nombre; *p; p++)
    {
        char c = *p;
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        if (c >= 'A' && c <= 'Z') return c;
        if (c >= '0' && c <= '9') return '#';
    }
    return '#';
}

// Franja con los cuatro colores de los botones de la Super Famicom (el toque "SNES").
static void FranjaSnes(float x, float y, float w, float h, float a)
{
    uint32_t c[4] = {C_SNES_Y, C_SNES_X, C_SNES_A, C_SNES_B};
    float bw = w / 4;
    for (int i = 0; i < 4; i++) Caja(x + i * bw, y, bw + 1, h, 0, c[i], a);
}

// ---------- estilos de menú: cómo se ordenan los juegos ----------
// Cada estilo combina una forma (cuadrícula, lista, carrusel, destacado), la proporción de las tarjetas, cómo se ven y
// qué hace la elegida. La cuadrícula calcula el tamaño de las tarjetas para que entren las columnas y filas pedidas.
enum FormaMenu { MF_CUADRICULA, MF_LISTA, MF_CARRUSEL, MF_DESTACADO };
enum TarjetaMenu { TJ_NORMAL, TJ_LIMPIA, TJ_TITULO, TJ_POLAROID, TJ_MARCO };
enum { EF_CRECE = 1, EF_ELEVA = 2, EF_BRILLA = 4, EF_ARO = 8 };
struct EstiloMenu { const char* nombre; uint8_t forma, cols, filas; float proporcion; uint8_t tarjeta, efecto; bool descripcion; };
static const float CAJA = 1.38f, VERTICAL = .72f, CUADRADA = 1.f;
static const EstiloMenu MENUS[] = {
    {"Clásico", MF_CUADRICULA, 5, 3, CAJA, TJ_NORMAL, EF_CRECE | EF_BRILLA | EF_ARO},
    {"Vitrina", MF_CUADRICULA, 4, 2, CAJA, TJ_TITULO, EF_CRECE | EF_ARO},
    {"Mosaico", MF_CUADRICULA, 7, 4, CAJA, TJ_LIMPIA, EF_CRECE | EF_ARO},
    {"Galería", MF_CUADRICULA, 4, 3, CAJA, TJ_MARCO, EF_BRILLA},
    {"Compacto", MF_CUADRICULA, 6, 4, CAJA, TJ_NORMAL, EF_ARO},
    {"Grande", MF_CUADRICULA, 3, 2, CAJA, TJ_NORMAL, EF_CRECE | EF_BRILLA | EF_ARO},
    {"Minimal", MF_CUADRICULA, 5, 3, CAJA, TJ_LIMPIA, EF_ELEVA},
    {"Etiquetas", MF_CUADRICULA, 5, 3, CAJA, TJ_TITULO, EF_ARO},
    {"Lista", MF_LISTA, 1, 9, CAJA, TJ_NORMAL, EF_BRILLA, true},
    {"Lista detallada", MF_LISTA, 1, 7, CAJA, TJ_MARCO, EF_ARO, true},
    {"Carrusel", MF_CARRUSEL, 1, 1, CAJA, TJ_NORMAL, EF_BRILLA | EF_ARO},
    {"Escenario", MF_CARRUSEL, 1, 1, CAJA, TJ_TITULO, EF_BRILLA | EF_ARO},
};
static const int NUM_MENUS = (int)(sizeof MENUS / sizeof MENUS[0]);
static const EstiloMenu* menu = &MENUS[0];
static int menuElegido;
static float zonaX = MARGEN, zonaW = ANCHO - 2 * MARGEN;   // donde va la cuadrícula (en el destacado, a la derecha)

static void MenuElegir(int i)
{
    menuElegido = i >= 0 && i < NUM_MENUS ? i : 0;
    menu = &MENUS[menuElegido];
    const float arriba = 178, alto = ALTO - arriba - PIE - 34;
    Y0 = arriba;
    if (menu->forma == MF_LISTA) { COLS = 1; FILAS_PAGINA = menu->filas; return; }
    if (menu->forma == MF_CARRUSEL) { COLS = 1; FILAS_PAGINA = 1; return; }
    zonaX = MARGEN;
    zonaW = ANCHO - 2 * MARGEN;
    if (menu->forma == MF_DESTACADO) { zonaX = MARGEN + 700; zonaW = ANCHO - MARGEN - zonaX; }
    COLS = menu->cols;
    FILAS_PAGINA = menu->filas;
    float titulo = menu->tarjeta == TJ_TITULO ? 40 : 0;
    GAPX = COLS >= 7 ? 22 : 34;
    GAPY = (COLS >= 7 ? 26 : 44) + titulo;
    CW = (zonaW - (COLS - 1) * GAPX) / COLS;
    CH = CW / menu->proporcion;
    if (FILAS_PAGINA * (CH + GAPY) > alto)
    {
        CH = alto / FILAS_PAGINA - GAPY;
        CW = CH * menu->proporcion;
    }
    // Centrada en su zona.
    X0 = zonaX + (zonaW - (COLS * CW + (COLS - 1) * GAPX)) / 2;
}

// La carátula (o las iniciales si no hay) dentro de un rectángulo, con la proporción de la carátula.
static void CaratulaEn(const Juego& j, float X, float Y, float w, float h, float radio, float margen, bool marcoOscuro)
{
    Caratula car = ObtenerCaratula(j.clave);
    if (!car.px || car.aparece < 1)
    {
        // Vidrio difuminado (en todos los temas) con un toque del color del juego.
        CajaVidrio(X, Y, w, h, radio, j.color1, .12f, 1);
        Velo(X, Y, w, h * .5f, radio, 0xFFFFFF, .1f, 0);
        Borde(X, Y, w, h, radio, 1.5f, 0xFFFFFF, .45f);
        if (h > 120) FranjaSnes(X + w * .12f, Y + 14, w * .76f, 8, .5f);
        Velo(X, Y, w, h * .5f, radio, 0xFFFFFF, .06f, 0);
        char buf[80];
        if (h > 90) Texto(F_INICIAL, j.iniciales, X + w * .09f, Y + h * .12f, 0xFFFFFF, .95f);
        Velo(X, Y + h * .52f, w, h * .48f, radio, 0x05070F, 0, .62f);
        if (h > 120) Texto(F_ETIQUETA, Acortar(F_ETIQUETA, j.nombre, w - 48, buf, sizeof buf), X + 26, Y + h - 82, 0xFFFFFF);
    }
    if (car.px)
    {
        float a = car.aparece;
        if (marcoOscuro)
        {
            // El marco de la caja: siempre de vidrio difuminado, teñido con el color de los paneles del tema.
            // Vidrio transparente: poco tinte (se ve el paisaje detrás), un reflejo de luz arriba y un borde fino.
            CajaVidrio(X, Y, w, h, radio, C_PANEL, .07f, a);
            Velo(X, Y, w, h * .5f, radio, 0xFFFFFF, .1f * a, 0);
            Borde(X, Y, w, h, radio, 1.5f, 0xFFFFFF, .45f * a);
        }
        float iw = w - 2 * margen, ih = h - 2 * margen;
        float escala = iw / car.w < ih / car.h ? iw / car.w : ih / car.h;
        float dw = car.w * escala, dh = car.h * escala;
        Imagen(car.px, car.w, car.h, X + w / 2 - dw / 2, Y + h / 2 - dh / 2, dw, dh, a, true);
    }
}

// Insignias de la tarjeta: favorito arriba a la derecha, dónde está (USB o consola) y la consola abajo.
static void Insignias(const Juego& j, float X, float Y, float w, float h, bool chicas)
{
    float e = chicas ? .7f : 1;
    if (EsFavorito(j.clave))
    {
        Circulo(X + w - 22 * e, Y + 22 * e, 20 * e, C_SNES_B, 1);
        Estrella(X + w - 22 * e, Y + 23 * e, 11 * e, 0x2A1A06, 1);
    }
    if (chicas) return;
    Caja(X + 12, Y + h - 46, 62, 38, 19, C_INSIGNIA, .85f);
    IconoLugar(j.enUsb, X + 43, Y + h - 27, 1.15f, 1);
    const char* n = ConsolaDe(j.sistema).nombre;
    // Arcade con el set incompleto (le faltan archivos, el juego padre o la BIOS): la etiqueta en ámbar.
    bool aviso = EsArcade(j.sistema) && j.estado > ARC_COMPLETO;
    float tw = AnchoTexto(F_PEQUENA, n) + 28;
    Caja(X + w - 12 - tw, Y + h - 46, tw, 38, 19, aviso ? C_AVISO : C_INSIGNIA, aviso ? .9f : .85f);
    Texto(F_PEQUENA, n, X + w - 12 - tw / 2, Y + h - 40, aviso ? 0x1A1206 : C_TEXTO, 1, 1);
}

// Una tarjeta de 'tw' × 'th' con la esquina en (x, y). f: cuánto está elegida (0 a 1, animado).
static void TarjetaEn(const Juego& j, float x, float y, float tw, float th, float f)
{
    int ef = menu->efecto;
    float s = ef & EF_CRECE ? 1 + .06f * f : 1;
    float sube = ef & EF_ELEVA ? -14 * f : 0;
    float w = tw * s, h = th * s, cx = x + tw / 2, cy = y + th / 2 + sube;
    float X = cx - w / 2, Y = cy - h / 2, r = Radio(20);
    bool chicas = w < 200;
    Caratula car = ObtenerCaratula(j.clave);
    if ((ef & EF_BRILLA) && f > .01f) Brillo(cx, cy + 10, w * .85f, car.px ? 0xFFFFFF : j.color1, (car.px ? .22f : .4f) * f);
    Sombra(X, Y + 14 + (6 + (sube < 0 ? 10 : 0)) * f, w, h, r, 28 + 12 * f, .55f, true);
    switch (menu->tarjeta)
    {
    case TJ_LIMPIA:
        CaratulaEn(j, X, Y, w, h, r, 0, false);
        Insignias(j, X, Y, w, h, true);
        break;
    case TJ_POLAROID:
    {
        // Marco claro con el nombre abajo, adentro del marco.
        float pie = h * .2f;
        Caja(X, Y, w, h, Radio(8), 0xF6F3EC, 1);
        CaratulaEn(j, X + 10, Y + 10, w - 20, h - pie - 14, Radio(4), 0, false);
        char buf[80];
        Texto(F_PEQUENA, Acortar(F_PEQUENA, j.nombre, w - 24, buf, sizeof buf), X + w / 2, Y + h - pie / 2 - 12, 0x2A2632, 1, 1);
        if (EsFavorito(j.clave)) Insignias(j, X, Y, w, h, true);
        break;
    }
    case TJ_MARCO:
        CaratulaEn(j, X, Y, w, h, r, 12, true);
        Borde(X, Y, w, h, r, 2, C_ACENTO_2, .7f);
        Insignias(j, X, Y, w, h, chicas);
        break;
    default:   // normal y con título
        CaratulaEn(j, X, Y, w, h, r, 12, true);
        Insignias(j, X, Y, w, h, chicas);
        break;
    }
    if (menu->tarjeta == TJ_TITULO)
    {
        char buf[80];
        Texto(F_PEQUENA, Acortar(F_PEQUENA, j.nombre, tw, buf, sizeof buf), x + tw / 2, y + th + 10, f > .5f ? C_TEXTO : C_TEXTO_2, 1, 1);
    }
    // Aro de selección.
    if ((ef & EF_ARO) && f > .01f) Borde(X - 6, Y - 6, w + 12, h + 12, r + 6, 4, C_ACENTO, .95f * f);
    // Sin ningún efecto visible (la lista compacta no tiene): al menos un contorno suave.
    if (!(ef & (EF_ARO | EF_BRILLA | EF_CRECE | EF_ELEVA)) && f > .01f) Borde(X - 3, Y - 3, w + 6, h + 6, r + 3, 2, C_ACENTO, .8f * f);
}

static void Tarjeta(const Juego& j, float x, float y, float f) { TarjetaEn(j, x, y, CW, CH, f); }

// Los datos de un juego en una línea ("Game Boy · 128 KB", el mapeo de la SNES, el fabricante del arcade...).
static void TextoDatos(const Juego& j, char* buf, int max)
{
    if (j.sistema == SIS_SNES)
        snprintf(buf, max, "%s   ·   %d KB   ·   %s%s", j.mapeo, j.tamKB, j.pal ? "PAL" : "NTSC", j.tieneRam ? "   ·   guarda partida" : "");
    else if (EsArcade(j.sistema))
    {
        int ja = ArcadeBuscar(ArcadeNombreDeRuta(j.ruta));
        snprintf(buf, max, "%s   ·   %s %d   ·   %s", ConsolaDe(j.sistema).nombreLargo, ja >= 0 ? ArcadeFabricante(ja) : "", ja >= 0 ? ArcadeAnio(ja) : 0,
                 j.estado == ARC_COMPLETO ? "Completo" : j.estado > ARC_COMPLETO ? ArcadeEstadoTexto(j.estado) : "");
    }
    else snprintf(buf, max, "%s   ·   %d KB", ConsolaDe(j.sistema).nombreLargo, j.tamKB);
}

// El juego elegido en grande, con su nombre y datos debajo (lista y destacado).
static void FichaGrande(const Juego& j, float x, float y, float w, float hMax)
{
    float h = w / menu->proporcion;
    if (h > hMax) { h = hMax; w = h * menu->proporcion; }
    Sombra(x, y + 18, w, h, Radio(24), 44, .55f, true);
    CaratulaEn(j, x, y, w, h, Radio(24), menu->tarjeta == TJ_LIMPIA ? 0 : 14, menu->tarjeta != TJ_LIMPIA);
    if (EsFavorito(j.clave)) Insignias(j, x, y, w, h, false);
    char corto[128], buf[160];
    Texto(F_SUBTITULO, Acortar(F_SUBTITULO, j.nombre, w + 200, corto, sizeof corto), x, y + h + 26, C_TEXTO);
    TextoDatos(j, buf, sizeof buf);
    Texto(F_PEQUENA, Acortar(F_PEQUENA, buf, w + 200, corto, sizeof corto), x, y + h + 80, C_TEXTO_2);
    Texto(F_PEQUENA, j.enUsb ? "En el USB" : "En la consola", x, y + h + 112, C_TEXTO_3);
}

// La ficha con descripción (lista y lista detallada): carátula, nombre, datos y lo que diga su gamelist.xml.
static void FichaDescripcion(const Juego& j, float x, float y, float w, float hMax)
{
    float h = w / menu->proporcion;
    if (h > 380) { h = 380; w = h * menu->proporcion; }
    Sombra(x, y + 16, w, h, Radio(22), 40, .5f, true);
    CaratulaEn(j, x, y, w, h, Radio(22), menu->tarjeta == TJ_LIMPIA ? 0 : 12, menu->tarjeta != TJ_LIMPIA);
    if (EsFavorito(j.clave)) Insignias(j, x, y, w, h, false);
    float ancho = ANCHO - MARGEN - x;
    char corto[128], buf[200];
    float ty = y + h + 22;
    Texto(F_SUBTITULO, Acortar(F_SUBTITULO, j.nombre, ancho, corto, sizeof corto), x, ty, C_TEXTO);
    ty += 54;
    TextoDatos(j, buf, sizeof buf);
    Texto(F_PEQUENA, Acortar(F_PEQUENA, buf, ancho, corto, sizeof corto), x, ty, C_TEXTO_2);
    ty += 34;
    const Metadato* md = MetadatoDe(j.archivo);
    if (md)
    {
        // Año · desarrollador · género · jugadores (los que haya).
        int n = 0;
        buf[0] = 0;
        const char* partes[4] = {md->anio, md->desarrollador, md->genero, md->jugadores};
        for (int k = 0; k < 4; k++)
        {
            if (!partes[k][0]) continue;
            n += snprintf(buf + n, sizeof buf - n, "%s%s%s", n ? "   ·   " : "", k == 3 ? "Jugadores: " : "", partes[k]);
            if (n >= (int)sizeof buf) break;
        }
        if (buf[0]) { Texto(F_PEQUENA, Acortar(F_PEQUENA, buf, ancho, corto, sizeof corto), x, ty, C_ACENTO_2); ty += 40; }
        if (md->descripcion[0])
        {
            // Hasta donde entre (unas 7 líneas): se corta en una palabra y termina en "…".
            static char d[520];
            snprintf(d, sizeof d, "%s", md->descripcion);
            if (strlen(md->descripcion) > 500) { char* e = strrchr(d, ' '); if (e) strcpy(e, "…"); }
            Recorte(x, ty, ancho, y + hMax + 120 - ty);
            Parrafo(F_PEQUENA, d, x, ty, ancho, C_TEXTO_2);
            SinRecorte();
        }
    }
    else
        Parrafo(F_PEQUENA, "Sin descripción. Si tienes un gamelist.xml (de Batocera, ES-DE o un programa de datos) junto a tus juegos en el "
                           "USB, la app lo lee al buscar juegos (triángulo).", x, ty, ancho, C_TEXTO_3);
}

static float listaVis;   // desplazamiento animado de la lista (en filas)

static void DibujarLista(float dt)
{
    const float alto = ALTO - Y0 - PIE - 34, fila = alto / FILAS_PAGINA, ancho = 960;
    float objetivo = sel - FILAS_PAGINA / 2;
    if (objetivo > nVista - FILAS_PAGINA) objetivo = (float)(nVista - FILAS_PAGINA);
    if (objetivo < 0) objetivo = 0;
    Suave(listaVis, objetivo, 12, dt);
    Panel(MARGEN - 12, Y0 - 12, ancho + 24, alto + 24, 24, .75f);   // detrás de las filas: se leen sobre cualquier fondo
    Recorte(0, Y0 - 10, MARGEN + ancho + 30, alto + 20);
    for (int i = (int)listaVis - 1; i < listaVis + FILAS_PAGINA + 1; i++)
    {
        if (i < 0 || i >= nVista) continue;
        const Juego& j = juegos[vista[i]];
        float y = Y0 + (i - listaVis) * fila;
        Suave(foco[i], i == sel ? 1 : 0, 14, dt);
        float f = foco[i];
        if (f > .01f)
        {
            Caja(MARGEN, y + 3, ancho, fila - 6, Radio(16), C_ACENTO, .22f * f);
            if (menu->efecto & EF_ARO) Borde(MARGEN, y + 3, ancho, fila - 6, Radio(16), 2, C_ACENTO, .8f * f);
            if (menu->efecto & EF_BRILLA) Brillo(MARGEN + 80, y + fila / 2, 160, C_ACENTO, .25f * f);
        }
        // Miniatura con la proporción del estilo, el nombre y la consola.
        float mh = fila - 18, mw = mh * menu->proporcion;
        if (mw > 150) { mw = 150; mh = mw / menu->proporcion; }
        CaratulaEn(j, MARGEN + 14, y + (fila - mh) / 2, mw, mh, Radio(8), 0, false);
        char corto[128];
        float tx = MARGEN + 30 + mw;
        Texto(fila > 80 ? F_CUERPO_M : F_CUERPO, Acortar(F_CUERPO_M, j.nombre, ancho - mw - 230, corto, sizeof corto), tx,
              y + fila / 2 - (fila > 80 ? 30 : 16), i == sel ? C_TEXTO : C_TEXTO_2);
        if (fila > 64) Texto(F_PEQUENA, ConsolaDe(j.sistema).nombreLargo, tx, y + fila / 2 + 6, C_TEXTO_3);
        if (EsFavorito(j.clave)) { Circulo(MARGEN + ancho - 40, y + fila / 2, 14, C_SNES_B, 1); Estrella(MARGEN + ancho - 40, y + fila / 2 + 1, 8, 0x2A1A06, 1); }
        IconoLugar(j.enUsb, MARGEN + ancho - 90, y + fila / 2, .7f, 1);
    }
    SinRecorte();
    // A la derecha, el elegido en grande.
    if (menu->descripcion) FichaDescripcion(juegos[vista[sel]], MARGEN + ancho + 60, Y0 + 4, ANCHO - MARGEN - (MARGEN + ancho + 60), alto - 200);
    else FichaGrande(juegos[vista[sel]], MARGEN + ancho + 80, Y0 + 10, ANCHO - MARGEN - (MARGEN + ancho + 80), alto - 200);
}

static float carruselVis;   // posición animada del carrusel (en juegos)

static void DibujarCarrusel(float dt)
{
    Suave(carruselVis, (float)sel, 10, dt);
    float hCentro = 470, wCentro = hCentro * menu->proporcion;
    if (wCentro > 760) { wCentro = 760; hCentro = wCentro / menu->proporcion; }
    float cx = ANCHO / 2, cy = Y0 + 40 + hCentro / 2;
    // De los de los costados hacia el centro, así el elegido queda encima.
    int lejos = 6;
    for (int d = lejos; d >= 0; d--)
        for (int lado = -1; lado <= 1; lado += 2)
        {
            if (d == 0 && lado == 1) continue;
            int i = (int)floorf(carruselVis + .5f) + d * lado;
            if (i < 0 || i >= nVista) continue;
            float k = i - carruselVis, ak = fabsf(k);
            float esc = 1 - .2f * ak;
            if (esc < .42f) esc = .42f;
            float w = wCentro * esc, h = hCentro * esc;
            float x = cx + k * wCentro * .62f - w / 2;
            float y = cy - h / 2 + ak * 18;
            Suave(foco[i], i == sel ? 1 : 0, 14, dt);
            TarjetaEn(juegos[vista[i]], x, y, w, h, foco[i]);
            if (ak > .5f) Caja(x, y, w, h, Radio(20), 0x000000, (ak > 3 ? .6f : ak * .17f));   // los lejanos, más oscuros
        }
    const Juego& j = juegos[vista[sel]];
    char corto[128], buf[160];
    float ty = cy + hCentro / 2 + 60;
    Texto(F_TITULO, Acortar(F_TITULO, j.nombre, ANCHO - 2 * MARGEN, corto, sizeof corto), cx, ty, C_TEXTO, 1, 1);
    TextoDatos(j, buf, sizeof buf);
    Texto(F_CUERPO, Acortar(F_CUERPO, buf, ANCHO - 2 * MARGEN, corto, sizeof corto), cx, ty + 64, C_TEXTO_2, 1, 1);
}

static void Biblioteca(float t, float dt, const Mando& m);
static void DibujarCopia(float dt);
static void VentanaDibujar(float dt);
static void EligeCaratulaDibujar(float dt);

// La búsqueda activa, a la derecha de las pestañas; o la pista de cómo buscar.
static void DibujarBusqueda()
{
    // Alineada a la derecha, en la fila de las pestañas.
    float derecha = ANCHO - MARGEN, y = 96;
    if (busqueda[0])
    {
        // "Buscando: «texto»" y el botón ○ dibujado (la letra no trae los símbolos de los botones).
        char t[80];
        snprintf(t, sizeof t, "Buscando: «%s»", busqueda);
        float tw = AnchoTexto(F_CUERPO_M, t), qw = Pista(BM_CIRCULO, "quitar", 0, 0, 1, true);
        float w = tw + qw + 72;
        Caja(derecha - w, y, w, 48, 24, C_INSIGNIA, .75f);
        Borde(derecha - w, y, w, 48, 24, 2, C_SNES_B, .8f);
        Texto(F_CUERPO_M, t, derecha - w + 20, y + 10, C_TEXTO, 1);
        Pista(BM_CIRCULO, "quitar", derecha - qw - 20, y + 24);
    }
    // (la pista de cómo buscar está en la barra de abajo: el panel táctil)
}

// Las pestañas bajo la cabecera, con L1 y R1 a los costados y una pastilla que se desliza a la elegida. Cada una mide lo
// que su nombre; si no entran todas (muchas consolas), la tira se corre para que la elegida quede a la vista y las que
// quedan fuera se desvanecen en los bordes.
static void DibujarPestanas(float dt)
{
    float y = 96, alto = 48, x0 = MARGEN + 70, separa = 10;
    float limite = ANCHO - MARGEN - 470;   // a la derecha va la búsqueda
    float px[NUM_VISTAS + NUM_SISTEMAS], pw[NUM_VISTAS + NUM_SISTEMAS], total = 0;
    for (int i = 0; i < nPestanas; i++)
    {
        pw[i] = AnchoTexto(F_CUERPO_M, pestanas[i].nombre) + 64;
        if (pw[i] < 150) pw[i] = 150;
        px[i] = total;
        total += pw[i] + separa;
    }
    float ventana = limite - x0 - 60;
    float objetivo = 0;
    if (total > ventana && nPestanas)
    {
        objetivo = px[vistaActual] + pw[vistaActual] / 2 - ventana / 2;
        if (objetivo < 0) objetivo = 0;
        if (objetivo > total - ventana) objetivo = total - ventana;
    }
    Suave(tiraDesde, objetivo, 14, dt);
    if (nPestanas)
    {
        Suave(pastillaX, px[vistaActual], 14, dt);
        Suave(pastillaW, pw[vistaActual], 14, dt);
    }
    DibujarBoton(BM_L1, MARGEN + 24, y + alto / 2);
    float fin = total > ventana ? x0 + ventana : x0 + total;
    Capsula(x0 + pastillaX - tiraDesde, y, pastillaW, alto, 1);   // con el estilo de los botones (círculo lo cambia)
    for (int i = 0; i < nPestanas; i++)
    {
        float cx = x0 + px[i] - tiraDesde + pw[i] / 2;
        // Desvanecida si el texto se sale de la ventana de la tira (así no pisa los botones L1 y R1).
        float tw = (pw[i] - 64) / 2, a = 1;
        if (cx - tw < x0) a = 1 - (x0 - (cx - tw)) / 50;
        if (cx + tw > fin) a = 1 - (cx + tw - fin) / 50;
        if (a <= 0) continue;
        Texto(F_CUERPO_M, pestanas[i].nombre, cx, y + 10, i == vistaActual ? C_TEXTO : C_TEXTO_2, a, 1);
    }
    DibujarBoton(BM_R1, fin + 20, y + alto / 2);
}

static void Avisar(const char* t, uint32_t c);

void InterfazCuadro(float t, float dt, const Mando& m)
{
    RegistroCuadro();
    MusicaActiva(!JuegoActivo());   // la música de la biblioteca se apaga durante el juego
    // Si hay un juego abierto, solo la pantalla de juego (sin gastar en el fondo ni en la biblioteca, que no se ven).
    if (JuegoActivo()) { BibliotecaVigilarUsb(false); JuegoCuadro(t, dt, m); return; }
    if (Presentacion(dt, m)) { entrada = 0; return; }
    // Al terminar cada búsqueda: las carátulas que falten, solas (si está activado y no hay otra descarga).
    {
        static bool buscabaAntes = true;
        bool busca = BibliotecaBuscando(NULL);
        if (buscabaAntes && !busca && General().caratulasAuto && !Progreso().activa && nJuegos > 0)
        {
            static const char* lista[MAX_JUEGOS];
            for (int i = 0; i < nJuegos; i++) lista[i] = juegos[i].clave;
            EmpezarDescarga(lista, nJuegos, true);
        }
        buscabaAntes = busca;
    }
    // USB conectado o sacado: aviso y búsqueda rápida (los juegos nuevos aparecen; los del USB que se fue, desaparecen).
    BibliotecaVigilarUsb(true);
    if (int cambio = BibliotecaCambioUsb())
    {
        Avisar(cambio > 0 ? "USB conectado: buscando juegos…" : "USB desconectado", cambio > 0 ? C_ACENTO_2 : C_TEXTO_2);
        Sonar(SON_SECCION);
        BibliotecaBuscarDe(true);
    }
    {
        char t2[160];
        bool ok2 = false;
        if (RespaldoTerminado(t2, sizeof t2, &ok2)) { Avisar(t2, ok2 ? C_BIEN : C_AVISO); Sonar(ok2 ? SON_ACEPTAR : SON_ERROR); }
    }
    if (int car = BibliotecaCaratulasCopiadas())
    {
        char t2[96];
        snprintf(t2, sizeof t2, car == 1 ? "1 carátula copiada del USB" : "%d carátulas copiadas del USB", car);
        Avisar(t2, C_BIEN);
    }
    // Después del logo, la biblioteca aparece desde negro.
    Suave(entrada, 1, 5, dt);
    // El asistente de primera vez (o las novedades) va encima de la biblioteca y se lleva los botones.
    bool asistente = AsistenteActivo();
    Biblioteca(t, dt, entrada > .98f && !asistente ? m : Mando{});
    DibujarCopia(dt);
    VentanaDibujar(dt);
    EligeCaratulaDibujar(dt);
    if (asistente) AsistenteCuadro(dt, entrada > .98f ? m : Mando{});
    if (entrada < .995f) Caja(0, 0, ANCHO, ALTO, 0, 0x000000, 1 - entrada);
}

// ---------- copiar a la consola: ventanas, avisos y progreso ----------
enum { VN_NADA, VN_JUEGO, VN_BORRAR, VN_COPIAR_TODOS, VN_TODOS_AL_USB, VN_ADMINISTRAR, VN_BORRAR_CONSOLA, VN_BORRAR_USB };
static int ventana = VN_NADA, opVentana;
static float ventanaVis;
struct Elegido { char ruta[512], archivo[96], nombre[64], clave[96]; uint64_t bytes; bool enUsb, otraEnUsb; };
static Elegido elegido;
static char aviso[160];
static uint32_t avisoColor;
static float tAviso = 99;
static bool releer;            // volver a buscar juegos apenas se pueda (después de copiar o borrar)
static bool copiaVista = true; // ya se atendió el final de la última copia
static bool copiaAutomatica;   // la empezó la app sola (copia automática): sin aviso si no había nada que copiar
static uint64_t libreConsola;
static bool libreMedido;
static float tMedida = 99;

static void Avisar(const char* t, uint32_t c) { snprintf(aviso, sizeof aviso, "%s", t); avisoColor = c; tAviso = 0; }

static void MedirEspacio(float dt)
{
    tMedida += dt;
    if (tMedida < 1) return;   // una vez por segundo alcanza
    tMedida = 0;
    libreMedido = EspacioConsola(&libreConsola);
}

static void TextoLibre(char* out, int max)
{
    char t[32];
    if (!libreMedido) { snprintf(out, max, "Espacio libre en la consola: no se pudo medir"); return; }
    TextoTamano(libreConsola, t, sizeof t);
    snprintf(out, max, "Espacio libre en la consola: %s", t);
}

// Los juegos del USB que todavía no están en la consola (lo que copiaría "Copiar todos").
static int PendientesUsb(const char** rutas, uint64_t* bytes)
{
    int n = 0;
    uint64_t b = 0;
    for (int i = 0; i < nJuegos; i++)
        if (juegos[i].enUsb && Visible(juegos[i]))
        {
            if (rutas) rutas[n] = juegos[i].ruta;
            b += juegos[i].bytes;
            n++;
        }
    if (bytes) *bytes = b;
    return n;
}

// Los juegos de la consola que todavía no están en un USB (lo que copiaría "Copiar todos al USB").
static int PendientesConsola(const char** rutas, uint64_t* bytes)
{
    int n = 0;
    uint64_t b = 0;
    for (int i = 0; i < nJuegos; i++)
        if (!juegos[i].enUsb && !juegos[i].tambienEnUsb && !juegos[i].repetido)
        {
            if (rutas) rutas[n] = juegos[i].ruta;
            b += juegos[i].bytes;
            n++;
        }
    if (bytes) *bytes = b;
    return n;
}

// Todos los juegos de un lugar (para borrar todos), y cuántos de ellos no tienen copia en el otro lugar (se perderían).
static int TodosDe(bool usb, const char** rutas, uint64_t* bytes, int* sinCopia)
{
    int n = 0, sc = 0;
    uint64_t b = 0;
    for (int i = 0; i < nJuegos; i++)
    {
        const Juego& j = juegos[i];
        if (j.enUsb != usb) continue;
        if (rutas) rutas[n] = j.ruta;
        b += j.bytes;
        if (usb ? !j.copiado : !j.tambienEnUsb) sc++;
        n++;
    }
    if (bytes) *bytes = b;
    if (sinCopia) *sinCopia = sc;
    return n;
}

static void BorrarTodos(bool usb)
{
    static const char* lista[MAX_JUEGOS];
    int n = TodosDe(usb, lista, NULL, NULL);
    if (n && EmpezarBorrado(lista, n)) copiaVista = false;
}

// El USB de destino: se busca al abrir la ventana (hace falta el acceso de GoldHEN para verlo).
static char usbCarpeta[64];
static bool usbHay;
static uint64_t usbLibre;
static void BuscarUsb() { usbHay = CarpetaUsb(usbCarpeta, sizeof usbCarpeta, &usbLibre); }

static void CopiarConsolaAlUsb()
{
    static const char* lista[MAX_JUEGOS];
    int n = PendientesConsola(lista, NULL);
    if (n && usbHay && EmpezarCopia(lista, n, usbCarpeta, RESERVA_USB)) copiaVista = false;
    copiaAutomatica = false;
}

static void CopiarPendientes()
{
    static const char* lista[MAX_JUEGOS];
    int n = PendientesUsb(lista, NULL);
    if (n && EmpezarCopia(lista, n, CARPETA_JUEGOS_CONSOLA, RESERVA_SISTEMA)) copiaVista = false;
    copiaAutomatica = false;   // la copia automática lo vuelve a poner después de llamar
}

// Lo que se puede hacer con el juego elegido, según dónde está. Copiar solo ese juego va primero.
enum { JA_A_CONSOLA, JA_A_USB, JA_BORRAR, JA_ELEGIR_CARATULA, JA_CARATULAS, JA_ADMINISTRAR, JA_CANCELAR };
static int AccionesJuego(int* ac)
{
    int n = 0;
    if (elegido.enUsb) ac[n++] = JA_A_CONSOLA;
    else
    {
        if (!elegido.otraEnUsb) ac[n++] = JA_A_USB;   // si ya está también en el USB, no hace falta
        ac[n++] = JA_BORRAR;
    }
    ac[n++] = JA_ELEGIR_CARATULA;
    ac[n++] = JA_CARATULAS;
    ac[n++] = JA_ADMINISTRAR;
    ac[n++] = JA_CANCELAR;
    return n;
}

static void AbrirMenuJuego(const Juego& j)
{
    snprintf(elegido.ruta, sizeof elegido.ruta, "%s", j.ruta);
    snprintf(elegido.archivo, sizeof elegido.archivo, "%s", j.archivo);
    snprintf(elegido.nombre, sizeof elegido.nombre, "%s", j.nombre);
    snprintf(elegido.clave, sizeof elegido.clave, "%s", j.clave);
    elegido.bytes = j.bytes;
    elegido.enUsb = j.enUsb;
    elegido.otraEnUsb = j.tambienEnUsb;
    ventana = VN_JUEGO;
    opVentana = 0;
    tMedida = 99;
}

// ---------- elegir la carátula a mano ----------
// Las del servidor más parecidas al nombre, en una cuadrícula de 6 × 2: X la elige, □ quita la carátula, ○ vuelve.
static bool eligiendoCaratula;
static int selCaratula;
static float eligeVis;

static void EligeCaratulaEntrada(const Mando& m)
{
    int total = 0;
    bool fin = false, sinRed = false;
    CaratulaManualEstado(&total, &fin, &sinRed);
    if (m.pulsados & B_CIRCULO) { eligiendoCaratula = false; CaratulaManualCerrar(); Sonar(SON_ATRAS); return; }
    if (m.pulsados & B_CUADRADO)
    {
        CaratulaQuitar(elegido.clave);
        Avisar("Carátula quitada", C_TEXTO_2);
        eligiendoCaratula = false;
        CaratulaManualCerrar();
        Sonar(SON_ATRAS);
        return;
    }
    if (total > 0)
    {
        int antes = selCaratula;
        if (m.pulsados & B_IZQ) selCaratula--;
        if (m.pulsados & B_DER) selCaratula++;
        if (m.pulsados & B_ARRIBA) selCaratula -= 6;
        if (m.pulsados & B_ABAJO) selCaratula += 6;
        if (selCaratula < 0) selCaratula = 0;
        if (selCaratula >= total) selCaratula = total - 1;
        if (selCaratula != antes) Sonar(SON_MOVER);
        if ((m.pulsados & B_CRUZ) && CaratulaManualOpcion(selCaratula)->px)
        {
            bool ok = CaratulaManualElegir(selCaratula);
            Avisar(ok ? "Carátula cambiada" : "No se pudo guardar la carátula", ok ? C_BIEN : C_MAL);
            eligiendoCaratula = false;
            CaratulaManualCerrar();
            Sonar(ok ? SON_ACEPTAR : SON_ERROR);
        }
    }
}

static void EligeCaratulaDibujar(float dt)
{
    Suave(eligeVis, eligiendoCaratula ? 1 : 0, 14, dt);
    if (eligeVis <= .01f || !eligiendoCaratula) return;
    float a = eligeVis;
    Velo(0, 0, ANCHO, ALTO, 0, 0x05030C, .6f * a, .6f * a);
    float w = 1660, h = 760, x = (ANCHO - w) / 2, y = 150;
    Panel(x, y, w, h, 28, a);
    char corto[128];
    Texto(F_SUBTITULO, "Elegir la carátula", x + 48, y + 30, C_TEXTO, a);
    Texto(F_PEQUENA, Acortar(F_PEQUENA, elegido.nombre, w - 96, corto, sizeof corto), x + 48, y + 80, C_TEXTO_3, a);
    int total = 0;
    bool fin = false, sinRed = false;
    int listas = CaratulaManualEstado(&total, &fin, &sinRed);
    if (fin && total == 0)
    {
        Texto(F_CUERPO, sinRed ? "No hay conexión con el servidor de carátulas." : "No encontré carátulas parecidas en el servidor.",
              x + w / 2, y + h / 2 - 20, C_TEXTO_2, a, 1);
    }
    else if (!total) Texto(F_CUERPO, "Buscando carátulas parecidas…", x + w / 2, y + h / 2 - 20, C_TEXTO_2, a, 1);
    const float cw = 240, ch = 230, gx = 26, gy = 64, x0 = x + (w - (6 * cw + 5 * gx)) / 2, y0 = y + 130;
    for (int i = 0; i < total; i++)
    {
        float cx = x0 + (i % 6) * (cw + gx), cy = y0 + (i / 6) * (ch + gy);
        const OpcionCaratula* o = CaratulaManualOpcion(i);
        bool el = i == selCaratula;
        CajaVidrio(cx, cy, cw, ch, Radio(14), C_PANEL, .5f, a);
        if (o && o->px)
        {
            float esc = (cw - 16) / o->w < (ch - 16) / o->h ? (cw - 16) / o->w : (ch - 16) / o->h;
            float dw = o->w * esc, dh = o->h * esc;
            Imagen(o->px, o->w, o->h, cx + cw / 2 - dw / 2, cy + ch / 2 - dh / 2, dw, dh, a, true);
        }
        else if (i >= listas) Texto(F_PEQUENA, "Bajando…", cx + cw / 2, cy + ch / 2 - 10, C_TEXTO_3, a, 1);
        if (el) Borde(cx - 5, cy - 5, cw + 10, ch + 10, Radio(18), 4, C_ACENTO, a);
        if (o) Texto(F_PEQUENA, Acortar(F_PEQUENA, o->nombre, cw, corto, sizeof corto), cx + cw / 2, cy + ch + 10, el ? C_TEXTO : C_TEXTO_3, a, 1);
    }
    static const int b[3] = {BM_CRUZ, BM_CUADRADO, BM_CIRCULO};
    static const char* const t[3] = {"Usar esta", "Quitar la carátula", "Volver"};
    PistasEn(b, t, 3, a, y + h - 40, x + w - 48);   // dentro de la ventana
}

static void VentanaEntrada(const Mando& m)
{
    int acciones[8];
    int opciones = ventana == VN_JUEGO ? AccionesJuego(acciones) : ventana == VN_ADMINISTRAR ? 5 : 2;
    if (m.pulsados & B_ARRIBA) { opVentana = (opVentana + opciones - 1) % opciones; Sonar(SON_MOVER); }
    if (m.pulsados & B_ABAJO) { opVentana = (opVentana + 1) % opciones; Sonar(SON_MOVER); }
    if (m.pulsados & (B_CIRCULO | B_OPTIONS)) { ventana = VN_NADA; Sonar(SON_ATRAS); return; }
    if (!(m.pulsados & B_CRUZ)) return;
    if (ventana == VN_JUEGO)
    {
        const char* r[1] = {elegido.ruta};
        switch (acciones[opVentana])
        {
        case JA_ELEGIR_CARATULA:
            if (CaratulaManualEmpezar(elegido.clave)) { eligiendoCaratula = true; selCaratula = 0; Sonar(SON_ACEPTAR); }
            else { Avisar("Espera a que termine la descarga de carátulas.", C_AVISO); Sonar(SON_ERROR); }
            ventana = VN_NADA;
            break;
        case JA_CARATULAS:
        {
            static const char* lista[MAX_JUEGOS];
            for (int i = 0; i < nJuegos; i++) lista[i] = juegos[i].clave;
            OlvidarNoEncontradas();   // a pedido: también las que antes no estaban en el servidor
            Sonar(EmpezarDescarga(lista, nJuegos) ? SON_ACEPTAR : SON_ERROR);
            tResultado = 0;
            ventana = VN_NADA;
            break;
        }
        case JA_A_CONSOLA:
            if (EmpezarCopia(r, 1, CARPETA_JUEGOS_CONSOLA, RESERVA_SISTEMA)) { copiaVista = false; copiaAutomatica = false; Sonar(SON_ACEPTAR); }
            else { Avisar("Ya hay una copia en curso: espera a que termine.", C_AVISO); Sonar(SON_ERROR); }
            ventana = VN_NADA;
            break;
        case JA_A_USB:
        {
            char usb[64];
            if (CopiaEnCurso()) { Avisar("Ya hay una copia en curso: espera a que termine.", C_AVISO); Sonar(SON_ERROR); }
            else if (!CarpetaUsb(usb, sizeof usb)) { Avisar("No hay un USB conectado.", C_MAL); Sonar(SON_ERROR); }
            else if (EmpezarCopia(r, 1, usb, RESERVA_USB)) { copiaVista = false; copiaAutomatica = false; Sonar(SON_ACEPTAR); }
            ventana = VN_NADA;
            break;
        }
        case JA_BORRAR: ventana = VN_BORRAR; opVentana = 1; Sonar(SON_ACEPTAR); break;                     // empieza en "Cancelar"
        case JA_ADMINISTRAR: ventana = VN_ADMINISTRAR; opVentana = 0; tMedida = 99; Sonar(SON_ACEPTAR); break;
        default: ventana = VN_NADA; Sonar(SON_ATRAS); break;
        }
        return;
    }
    if (ventana == VN_ADMINISTRAR)
    {
        // Cada acción abre su confirmación, que empieza en "Cancelar".
        switch (opVentana)
        {
        case 0: ventana = VN_COPIAR_TODOS; opVentana = 1; tMedida = 99; break;
        case 1: BuscarUsb(); ventana = VN_TODOS_AL_USB; opVentana = 1; break;
        case 2: ventana = VN_BORRAR_CONSOLA; opVentana = 1; break;
        case 3: ventana = VN_BORRAR_USB; opVentana = 1; break;
        default: ventana = VN_NADA; Sonar(SON_ATRAS); return;
        }
        Sonar(SON_ACEPTAR);
        return;
    }
    if (ventana == VN_BORRAR_CONSOLA || ventana == VN_BORRAR_USB)
    {
        if (opVentana == 0)
        {
            if (CopiaEnCurso()) { Avisar("Ya hay una copia o un borrado en curso: espera a que termine.", C_AVISO); Sonar(SON_ERROR); }
            else { BorrarTodos(ventana == VN_BORRAR_USB); Sonar(SON_ACEPTAR); }
        }
        else Sonar(SON_ATRAS);
        ventana = VN_NADA;
        return;
    }
    if (ventana == VN_BORRAR)
    {
        if (opVentana == 0)
        {
            if (BorrarDeConsola(elegido.ruta)) { Avisar("Juego borrado de la consola.", C_BIEN); releer = true; Sonar(SON_ACEPTAR); }
            else { Avisar("No se pudo borrar el juego de la consola.", C_MAL); Sonar(SON_ERROR); }
        }
        else Sonar(SON_ATRAS);
        ventana = VN_NADA;
        return;
    }
    // Copiar todos (del USB a la consola, o de la consola al USB).
    if (opVentana == 0)
    {
        if (CopiaEnCurso()) { Avisar("Ya hay una copia en curso: espera a que termine.", C_AVISO); Sonar(SON_ERROR); }
        else if (ventana == VN_TODOS_AL_USB && !usbHay) { Avisar("No hay un USB conectado.", C_MAL); Sonar(SON_ERROR); }
        else if (ventana == VN_TODOS_AL_USB) { CopiarConsolaAlUsb(); Sonar(SON_ACEPTAR); }
        else { CopiarPendientes(); Sonar(SON_ACEPTAR); }
    }
    else Sonar(SON_ATRAS);
    ventana = VN_NADA;
}

static void VentanaDibujar(float dt)
{
    Suave(ventanaVis, ventana != VN_NADA ? 1 : 0, 16, dt);
    float a = ventanaVis;
    if (a <= .01f || ventana == VN_NADA) return;
    Velo(0, 0, ANCHO, ALTO, 0, 0x05030C, .6f * a, .6f * a);
    int acciones[8];
    int nAcc = AccionesJuego(acciones);
    float w = 860, h = ventana == VN_JUEGO ? 230 + nAcc * 62.f : ventana == VN_ADMINISTRAR ? 230 + 5 * 62.f
                     : (ventana == VN_COPIAR_TODOS || ventana == VN_TODOS_AL_USB || ventana == VN_BORRAR_CONSOLA || ventana == VN_BORRAR_USB) ? 560 : 470;
    float x = (ANCHO - w) / 2, y = (ALTO - h) / 2 + (1 - a) * 40;
    Panel(x, y, w, h, 28, a);
    char t[200], tam[32], libre[96];
    TextoLibre(libre, sizeof libre);
    const char* op[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    int n = 0;
    if (ventana == VN_JUEGO)
    {
        char corto[96];
        Texto(F_SUBTITULO, Acortar(F_SUBTITULO, elegido.nombre, w - 96, corto, sizeof corto), x + 48, y + 40, C_TEXTO, a);
        TextoTamano(elegido.bytes, tam, sizeof tam);
        snprintf(t, sizeof t, "%s   ·   %s", elegido.enUsb ? "En el USB" : "En la consola", tam);
        IconoLugar(elegido.enUsb, x + 66, y + 106, .9f, a);
        Texto(F_CUERPO, t, x + 96, y + 92, elegido.enUsb ? C_AVISO : C_ACENTO_2, a);
        Texto(F_PEQUENA, libre, x + 48, y + 132, C_TEXTO_3, a);
        for (int i = 0; i < nAcc; i++)
            switch (acciones[i])
            {
            case JA_A_CONSOLA: op[i] = "Copiar solo este juego a la consola"; break;
            case JA_A_USB: op[i] = "Copiar este juego al USB"; break;
            case JA_BORRAR: op[i] = "Borrar de la consola"; break;
            case JA_ELEGIR_CARATULA: op[i] = "Elegir la carátula..."; break;
            case JA_CARATULAS: op[i] = "Bajar las carátulas que faltan"; break;
            case JA_ADMINISTRAR: op[i] = "Copiar o borrar todos los juegos..."; break;
            default: op[i] = "Cancelar"; break;
            }
        n = nAcc;
    }
    else if (ventana == VN_BORRAR)
    {
        Texto(F_SUBTITULO, "¿Borrar de la consola?", x + 48, y + 40, C_TEXTO, a);
        Parrafo(F_CUERPO, elegido.otraEnUsb ? "Se borra solo la copia de la consola. El juego sigue en el USB y tus partidas no se tocan."
                                            : "No hay otra copia de este juego en el USB: para volver a jugarlo tendrás que copiarlo otra vez. Tus partidas no se tocan.",
                x + 48, y + 100, w - 96, C_TEXTO_2, a);
        op[0] = "Borrar";
        op[1] = "Cancelar";
        n = 2;
    }
    else if (ventana == VN_ADMINISTRAR)
    {
        uint64_t bc = 0, bu = 0;
        int nc = TodosDe(false, NULL, &bc, NULL), nu = TodosDe(true, NULL, &bu, NULL);
        char tc[32], tu[32];
        TextoTamano(bc, tc, sizeof tc);
        TextoTamano(bu, tu, sizeof tu);
        Texto(F_SUBTITULO, "Copiar o borrar todos los juegos", x + 48, y + 40, C_TEXTO, a);
        IconoLugar(false, x + 66, y + 104, .8f, a);
        snprintf(t, sizeof t, nc == 1 ? "En la consola: %d juego (%s)" : "En la consola: %d juegos (%s)", nc, tc);
        Texto(F_CUERPO, t, x + 96, y + 90, C_ACENTO_2, a);
        IconoLugar(true, x + 66, y + 140, .8f, a);
        snprintf(t, sizeof t, nu == 1 ? "En el USB: %d juego (%s)" : "En el USB: %d juegos (%s)", nu, tu);
        Texto(F_CUERPO, t, x + 96, y + 126, C_AVISO, a);
        op[0] = "Copiar todos los juegos del USB a la consola";
        op[1] = "Copiar todos los juegos de la consola al USB";
        op[2] = "Borrar todos los juegos de la consola";
        op[3] = "Borrar todos los juegos del USB";
        op[4] = "Cancelar";
        n = 5;
    }
    else if (ventana == VN_BORRAR_CONSOLA || ventana == VN_BORRAR_USB)
    {
        bool usb = ventana == VN_BORRAR_USB;
        uint64_t bytes = 0;
        int sinCopia = 0, cuantos = TodosDe(usb, NULL, &bytes, &sinCopia);
        TextoTamano(bytes, tam, sizeof tam);
        Texto(F_SUBTITULO, "¿Estás seguro?", x + 48, y + 40, C_TEXTO, a);
        snprintf(t, sizeof t, usb ? "Se van a borrar los %d juegos del USB (%s)." : "Se van a borrar los %d juegos de la consola (%s).", cuantos, tam);
        Texto(F_CUERPO_M, t, x + 48, y + 100, C_TEXTO, a);
        Texto(F_PEQUENA, "Solo se borran las roms de SNES. Tus partidas guardadas, estados y carátulas no se tocan.", x + 48, y + 142, C_TEXTO_3, a);
        float ay = y + 188, ah = 150;
        Caja(x + 32, ay, w - 64, ah, 16, C_MAL, .1f * a);
        Borde(x + 32, ay, w - 64, ah, 16, 2, C_MAL, .7f * a);
        Texto(F_CUERPO_M, "Esta acción no se puede deshacer", x + 56, ay + 16, C_MAL, a);
        char nota[200];
        if (!cuantos) snprintf(nota, sizeof nota, usb ? "No hay juegos en el USB: no hay nada para borrar." : "No hay juegos en la consola: no hay nada para borrar.");
        else if (sinCopia)
            snprintf(nota, sizeof nota, usb ? "%d de estos juegos no están copiados en la consola: si los borras, se pierden."
                                            : "%d de estos juegos no están copiados en el USB: si los borras, se pierden.", sinCopia);
        else snprintf(nota, sizeof nota, usb ? "Todos estos juegos tienen una copia en la consola, así que los vas a poder seguir jugando."
                                             : "Todos estos juegos tienen una copia en el USB, así que los vas a poder volver a copiar.");
        Parrafo(F_PEQUENA, nota, x + 56, ay + 58, w - 112, sinCopia ? C_TEXTO : C_TEXTO_2, a);
        static char si[64];
        snprintf(si, sizeof si, cuantos == 1 ? "Sí, borrar 1 juego" : "Sí, borrar %d juegos", cuantos);
        op[0] = si;
        op[1] = "Cancelar";
        n = 2;
    }
    else if (ventana == VN_TODOS_AL_USB)
    {
        uint64_t bytes = 0;
        int pend = PendientesConsola(NULL, &bytes);
        TextoTamano(bytes, tam, sizeof tam);
        Texto(F_SUBTITULO, "Copiar los juegos de la consola al USB", x + 48, y + 40, C_TEXTO, a);
        snprintf(t, sizeof t, pend == 1 ? "%d juego por copiar   ·   %s" : "%d juegos por copiar   ·   %s", pend, tam);
        Texto(F_CUERPO_M, t, x + 48, y + 100, C_TEXTO, a);
        char lu[32], tu[120];
        TextoTamano(usbLibre, lu, sizeof lu);
        if (usbHay) snprintf(tu, sizeof tu, "Espacio libre en el USB: %s   (se copia a %s)", lu, usbCarpeta);
        else snprintf(tu, sizeof tu, "No hay un USB conectado.");
        Texto(F_CUERPO, tu, x + 48, y + 142, usbHay ? C_TEXTO_2 : C_MAL, a);
        bool alcanza = usbHay && bytes + RESERVA_USB <= usbLibre;
        float ay = y + 188, ah = 150;
        Caja(x + 32, ay, w - 64, ah, 16, C_AVISO, .1f * a);
        Borde(x + 32, ay, w - 64, ah, 16, 2, C_AVISO, .6f * a);
        Texto(F_CUERPO_M, "Antes de copiar, verifica la capacidad de tu USB", x + 56, ay + 16, C_AVISO, a);
        const char* nota = !usbHay ? "Conecta un USB a la consola y vuelve a abrir esta ventana."
                         : !pend ? "Todos los juegos de la consola ya están en el USB: no hay nada para copiar."
                         : alcanza ? "Entran todos. Los juegos se copian a la carpeta SNES del USB y la consola los sigue teniendo."
                                   : "No entran todos: se copiarán solo los que entren. Si quieres copiarlos todos, usa un USB con más espacio.";
        Parrafo(F_PEQUENA, nota, x + 56, ay + 58, w - 112, C_TEXTO_2, a);
        op[0] = "Copiar";
        op[1] = "Cancelar";
        n = 2;
    }
    else
    {
        uint64_t bytes = 0;
        int pend = PendientesUsb(NULL, &bytes);
        TextoTamano(bytes, tam, sizeof tam);
        Texto(F_SUBTITULO, "Copiar los juegos del USB a la consola", x + 48, y + 40, C_TEXTO, a);
        snprintf(t, sizeof t, pend == 1 ? "%d juego por copiar   ·   %s" : "%d juegos por copiar   ·   %s", pend, tam);
        Texto(F_CUERPO_M, t, x + 48, y + 100, C_TEXTO, a);
        Texto(F_CUERPO, libre, x + 48, y + 142, C_TEXTO_2, a);
        bool alcanza = libreMedido && bytes + RESERVA_SISTEMA <= libreConsola;
        // Aviso destacado: que el usuario revise la capacidad de su consola antes de copiar.
        float ay = y + 188, ah = 150;
        Caja(x + 32, ay, w - 64, ah, 16, C_AVISO, .1f * a);
        Borde(x + 32, ay, w - 64, ah, 16, 2, C_AVISO, .6f * a);
        Texto(F_CUERPO_M, "Antes de copiar, verifica la capacidad de tu consola", x + 56, ay + 16, C_AVISO, a);
        const char* nota = !pend ? "Todos los juegos del USB ya están en la consola: no hay nada para copiar."
                         : !libreMedido ? "No se pudo medir el espacio libre: por seguridad, no se va a copiar nada."
                         : alcanza ? "Entran todos. Igual revisa que te quede espacio para tus juegos y aplicaciones de PS4: "
                                     "ALLESIU siempre deja 1 GB libre para el sistema."
                                   : "No entran todos: se copiarán solo los que entren, dejando 1 GB libre para el sistema. "
                                     "Si quieres copiarlos todos, libera espacio en la consola primero.";
        Parrafo(F_PEQUENA, nota, x + 56, ay + 58, w - 112, C_TEXTO_2, a);
        op[0] = "Copiar";
        op[1] = "Cancelar";
        n = 2;
    }
    float oy = y + h - 64 - n * 62;
    for (int i = 0; i < n; i++)
    {
        bool sel2 = i == opVentana;
        Caja(x + 32, oy + i * 62, w - 64, 52, 14, sel2 ? C_ACENTO : 0xFFFFFF, (sel2 ? .35f : .05f) * a);
        if (sel2) Borde(x + 32, oy + i * 62, w - 64, 52, 14, 2, C_ACENTO, .8f * a);
        Texto(F_CUERPO_M, op[i], x + 56, oy + i * 62 + 12, sel2 ? C_TEXTO : C_TEXTO_2, a);
    }
    int b[2] = {BM_CRUZ, BM_CIRCULO};
    const char* tx[2] = {"Aceptar", "Cancelar"};
    Pistas(b, tx, 2, a);
}

// La copia en curso (arriba al centro, debajo de la descarga de carátulas si las dos están activas), su resultado, y
// los avisos breves.
static void DibujarCopia(float dt)
{
    ProgresoCopia p = ProgresoDeCopia();
    // Al terminar: avisar el resultado y volver a buscar para que aparezcan las copias.
    if (p.terminada && !copiaVista && p.borrando)
    {
        copiaVista = true;
        char t[160];
        const char* lugar = p.alUsb ? "del USB" : "de la consola";
        if (p.copiados == 1 && !p.errores) snprintf(t, sizeof t, "1 juego borrado %s.", lugar);
        else if (p.copiados && !p.errores) snprintf(t, sizeof t, "%d juegos borrados %s.", p.copiados, lugar);
        else if (p.copiados) snprintf(t, sizeof t, "%d borrados; %d no se pudieron borrar.", p.copiados, p.errores);
        else snprintf(t, sizeof t, "No se pudo borrar ningún juego.");
        Avisar(t, p.errores ? (p.copiados ? C_AVISO : C_MAL) : C_BIEN);
        if (p.copiados) releer = true;
    }
    // La automática sin nada nuevo que copiar no avisa (pasaba después de cada búsqueda y tapaba la pantalla).
    if (p.terminada && !copiaVista && copiaAutomatica && !p.copiados && !p.errores && !p.sinEspacio && !p.fallo)
        copiaVista = true;
    if (p.terminada && !copiaVista)
    {
        copiaVista = true;
        char t[200], b1[32];
        TextoTamano(p.bytesHechos, b1, sizeof b1);
        if (p.fallo && !p.copiados) snprintf(t, sizeof t, "%s", p.detalle);
        else if (p.copiados == 1 && !p.sinEspacio && !p.errores) snprintf(t, sizeof t, "1 juego copiado %s (%s).", p.alUsb ? "al USB" : "a la consola", b1);
        else if (p.copiados && !p.sinEspacio && !p.errores) snprintf(t, sizeof t, "%d juegos copiados %s (%s).", p.copiados, p.alUsb ? "al USB" : "a la consola", b1);
        else if (p.copiados) snprintf(t, sizeof t, "%d copiados; %d no entraron por espacio; %d con error.", p.copiados, p.sinEspacio, p.errores);
        else if (p.yaEstaban) snprintf(t, sizeof t, p.alUsb ? "Ya estaba en el USB." : "Ya estaban todos en la consola.");
        else snprintf(t, sizeof t, "%s", p.detalle[0] ? p.detalle : "No se copió ningún juego.");
        Avisar(t, p.copiados && !p.errores && !p.sinEspacio ? C_BIEN : p.copiados ? C_AVISO : C_MAL);
        if (p.copiados) releer = true;
    }
    if (p.activa)
    {
        ProgresoDescarga d = Progreso();
        float w = PASTILLA_W, h = PASTILLA_H, x = ANCHO - MARGEN - w - (d.activa ? w + 16 : 0), y = PASTILLA_Y;
        Panel(x, y, w, h, 22, 1);
        char t[160], a1[32], a2[32];
        TextoTamano(p.bytesHechos, a1, sizeof a1);
        TextoTamano(p.bytesTotal, a2, sizeof a2);
        if (p.borrando) snprintf(t, sizeof t, "Borrando juegos %s   %d de %d", p.alUsb ? "del USB" : "de la consola", p.hechos, p.total);
        else snprintf(t, sizeof t, "Copiando %s   %d de %d   ·   %s de %s", p.alUsb ? "al USB" : "a la consola",
                      p.hechos + 1 > p.total ? p.total : p.hechos + 1, p.total, a1, a2);
        Texto(F_CUERPO_M, t, x + 28, y + 6, C_TEXTO, 1);
        char corto[120];
        Texto(F_PEQUENA, Acortar(F_PEQUENA, p.actual, w - 56, corto, sizeof corto), x + 28, y + 37, C_TEXTO_3, 1);
        float f = p.borrando ? (p.total ? (float)p.hechos / p.total : 0) : p.bytesTotal ? (float)((double)p.bytesHechos / p.bytesTotal) : 0;
        Caja(x + 20, y + h - 8, w - 40, 4, 2, 0xFFFFFF, .1f);
        Caja(x + 20, y + h - 8, (w - 40) * f, 4, 2, C_ACENTO_2, 1);
    }
    // Aviso breve, abajo al centro (sobre el pie).
    tAviso += dt;
    if (tAviso < 5)
    {
        float a = tAviso < .2f ? tAviso / .2f : tAviso > 4.5f ? (5 - tAviso) / .5f : 1;
        float w = AnchoTexto(F_CUERPO_M, aviso) + 80;
        float x = (ANCHO - w) / 2, y = ALTO - 170;
        Panel(x, y, w, 56, 28, a);
        Circulo(x + 30, y + 28, 7, avisoColor, a);
        Texto(F_CUERPO_M, aviso, x + 50, y + 14, C_TEXTO, a);
    }
}

// ---------- pestaña de configuración (vale para todos los juegos) ----------
enum { CF_PROPORCION, CF_FILTRO, CF_POR_CONSOLA, CF_AVANCE, CF_REBOBINAR, CF_CONTINUAR, CF_BEZELS, CF_RUNAHEAD, CF_SUPERFX, CF_MUSICA, CF_COPIAR_AUTO, CF_CARATULAS_AUTO, CF_ADMINISTRAR, CF_BIOS, CF_RESPALDAR, CF_RESTAURAR, CF_ASISTENTE, CF_ACERCA, NUM_CF };
static int selConfig;
static bool viendoAcerca, viendoBios, viendoConsolas;
static void PantallaConsolas(const Mando& m);
static void AcercaDe();
static void PantallaBios();
static float selConfigSuave, primeraConfigSuave;
static int primeraConfig;   // la primera fila que se ve (la lista se desplaza)
static const int FILAS_CONFIG = 9;
// Las filas de la configuración en pantalla, por secciones: los números negativos son los títulos de sección.
static const char* const SECCIONES[] = {"Imagen", "Juego", "Biblioteca", "Almacenamiento", "Sistema"};
static const int ORDEN_CONFIG[] = {
    -1, CF_PROPORCION, CF_FILTRO, CF_POR_CONSOLA, CF_BEZELS,
    -2, CF_AVANCE, CF_REBOBINAR, CF_CONTINUAR, CF_RUNAHEAD, CF_SUPERFX,
    -3, CF_MUSICA, CF_CARATULAS_AUTO, CF_COPIAR_AUTO,
    -4, CF_ADMINISTRAR, CF_RESPALDAR, CF_RESTAURAR, CF_BIOS,
    -5, CF_ASISTENTE, CF_ACERCA,
};
static const int FILAS_ORDEN = (int)(sizeof ORDEN_CONFIG / sizeof ORDEN_CONFIG[0]);
static int FilaConfig(int id) { for (int r = 0; r < FILAS_ORDEN; r++) if (ORDEN_CONFIG[r] == id) return r; return 0; }
// La opción anterior o siguiente en pantalla (saltando los títulos; da la vuelta).
static int OtraConfig(int id, int d)
{
    int r = FilaConfig(id);
    do r = (r + d + FILAS_ORDEN) % FILAS_ORDEN; while (ORDEN_CONFIG[r] < 0);
    return ORDEN_CONFIG[r];
}

static void Configuracion(float dt, const Mando& m)
{
    AjustesGenerales& g = General();
    if (viendoAcerca)
    {
        if (m.pulsados & (B_CIRCULO | B_CRUZ)) { viendoAcerca = false; Sonar(SON_ATRAS); }
        AcercaDe();
        return;
    }
    if (viendoBios)
    {
        if (m.pulsados & (B_CIRCULO | B_CRUZ)) { viendoBios = false; Sonar(SON_ATRAS); }
        if (m.pulsados & B_TRIANGULO) { Sonar(BibliotecaBuscando(NULL) ? SON_ERROR : SON_ACEPTAR); BibliotecaBuscar(); }
        PantallaBios();
        return;
    }
    if (selConfig == CF_ACERCA && (m.pulsados & (B_CRUZ | B_DER))) { viendoAcerca = true; Sonar(SON_ACEPTAR); return; }
    if (selConfig == CF_BIOS && (m.pulsados & (B_CRUZ | B_DER))) { BiosRevisar(); viendoBios = true; Sonar(SON_ACEPTAR); return; }
    if (selConfig == CF_ASISTENTE && (m.pulsados & (B_CRUZ | B_DER))) { AsistenteAbrir(); Sonar(SON_ACEPTAR); return; }
    if (viendoConsolas) { PantallaConsolas(m); return; }
    // Respaldo al USB; restaurar pide confirmar (pulsar X otra vez en 4 s): reemplaza las partidas de la consola.
    static double confirmarRestaurar = -10;
    if (selConfig == CF_RESPALDAR && (m.pulsados & B_CRUZ))
    {
        if (RespaldoEmpezar(false)) { Avisar("Respaldando partidas en el USB…", C_ACENTO_2); Sonar(SON_ACEPTAR); }
        else Sonar(SON_ERROR);
        return;
    }
    if (selConfig == CF_RESTAURAR && (m.pulsados & B_CRUZ))
    {
        if (Segundos() - confirmarRestaurar > 4) { confirmarRestaurar = Segundos(); Avisar("Pulsa X otra vez para restaurar (reemplaza las partidas de la consola)", C_AVISO); Sonar(SON_MOVER); }
        else if (RespaldoEmpezar(true)) { confirmarRestaurar = -10; Avisar("Restaurando el respaldo del USB…", C_ACENTO_2); Sonar(SON_ACEPTAR); }
        else Sonar(SON_ERROR);
        return;
    }
    if (selConfig == CF_POR_CONSOLA && (m.pulsados & (B_CRUZ | B_DER))) { viendoConsolas = true; Sonar(SON_ACEPTAR); return; }
    if (selConfig == CF_ADMINISTRAR && (m.pulsados & (B_CRUZ | B_DER))) { ventana = VN_ADMINISTRAR; opVentana = 0; tMedida = 99; Sonar(SON_ACEPTAR); return; }
    if (m.pulsados & B_ARRIBA) { selConfig = OtraConfig(selConfig, -1); Sonar(SON_MOVER); }
    if (m.pulsados & B_ABAJO) { selConfig = OtraConfig(selConfig, 1); Sonar(SON_MOVER); }
    int d = (m.pulsados & (B_DER | B_CRUZ)) ? 1 : (m.pulsados & B_IZQ) ? -1 : 0;
    if (d && selConfig != CF_ACERCA && selConfig != CF_ADMINISTRAR && selConfig != CF_BIOS && selConfig != CF_ASISTENTE && selConfig != CF_POR_CONSOLA && selConfig != CF_RESPALDAR && selConfig != CF_RESTAURAR)
    {
        switch (selConfig)
        {
        case CF_PROPORCION: g.proporcion = (g.proporcion + NUM_PROPORCIONES + d) % NUM_PROPORCIONES; break;
        case CF_FILTRO: g.filtro = (g.filtro + NUM_FILTROS + d) % NUM_FILTROS; break;
        case CF_AVANCE: g.avance = 2 + (g.avance - 2 + 3 + d) % 3; break;
        case CF_REBOBINAR: g.rebobinar = !g.rebobinar; break;
        case CF_CONTINUAR: g.continuar = !g.continuar; break;
        case CF_BEZELS: g.bezels = !g.bezels; break;
        case CF_RUNAHEAD: g.runahead = !g.runahead; break;
        case CF_SUPERFX: g.superfx = 1 + (g.superfx - 1 + 4 + d) % 4; break;
        case CF_COPIAR_AUTO: g.copiarAuto = !g.copiarAuto; break;
        case CF_CARATULAS_AUTO: g.caratulasAuto = !g.caratulasAuto; break;
        case CF_MUSICA: g.musica = !g.musica; MusicaEncender(g.musica != 0, (unsigned)(Segundos() * 1000)); break;
        }
        AjustesGuardar();
        Sonar(SON_MOVER);
    }
    // La fila elegida siempre a la vista.
    int filaSel = FilaConfig(selConfig);
    if (filaSel - 1 < primeraConfig) primeraConfig = filaSel - 1 < 0 ? 0 : filaSel - 1;   // con su título a la vista
    if (filaSel >= primeraConfig + FILAS_CONFIG) primeraConfig = filaSel - FILAS_CONFIG + 1;
    Suave(selConfigSuave, (float)filaSel, 18, dt);
    Suave(primeraConfigSuave, (float)primeraConfig, 18, dt);

    float w = 1180, x = (ANCHO - w) / 2, y = 176, paso = 78, h = 124 + (float)FILAS_CONFIG * paso;
    Panel(x, y, w, h, 28, 1);
    Texto(F_SUBTITULO, "Configuración general", x + 48, y + 36, C_TEXTO);
    Texto(F_PEQUENA, "Vale para todos los juegos. Cada juego puede tener su propia proporción y filtro desde el menú de pausa (L3 + R3).",
          x + 48, y + 86, C_TEXTO_3);
    float y0 = y + 124 - primeraConfigSuave * paso;
    Caja(x + 24, y0 + selConfigSuave * paso - 8, w - 48, paso - 4, 18, C_ACENTO, .2f);
    Borde(x + 24, y0 + selConfigSuave * paso - 8, w - 48, paso - 4, 18, 2, C_ACENTO, .7f);
    static const char* const TITULOS[NUM_CF] = {"Proporción de la imagen", "Filtro", "Imagen por consola", "Avance rápido (mantener R2)", "Rebobinar (mantener L2)", "Continuar donde quedé", "Marcos decorativos (bezels)", "Menos retraso de los controles",
                                                "Velocidad del Super FX", "Música en la biblioteca", "Copiar juegos nuevos del USB automáticamente", "Bajar carátulas automáticamente", "Copiar o borrar todos los juegos", "BIOS", "Respaldar partidas al USB", "Restaurar el respaldo del USB", "Asistente de inicio", "Acerca de ALLESIU"};
    static const char* const AYUDAS[NUM_CF] = {
        "Cómo se ajusta la imagen a la pantalla.",
        "Suave, nítido o el aspecto de un televisor antiguo.",
        "Proporción y filtro propios para cada consola (si no, usan los de arriba).",
        "Cuántas veces más rápido corre el juego.",
        "Guarda los últimos segundos para volver atrás. Desactivado, la consola trabaja menos.",
        "Al salir de un juego se guarda dónde ibas; al abrirlo de nuevo sigue desde ahí.",
        "Imágenes alrededor del juego: bezels/<consola>/<juego>.png o default.png en el USB.",
        "Run-ahead de un cuadro: responde antes, pero la consola trabaja el doble (no en arcade ni N64).",
        "Más rápido: los juegos 3D con este chip se ven más fluidos (la consola original va a ×1).",
        "Diez canciones originales de ALLESIU, en orden aleatorio. Se apaga sola mientras juegas.",
        "Los juegos que aparezcan en el USB se copian solos a la consola, si hay espacio.",
        "Si la consola tiene internet, las carátulas que falten se bajan solas después de buscar juegos.",
        NULL,   // el espacio libre (se calcula)
        NULL,   // cuántas BIOS hay (se calcula)
        "Copia tus partidas, estados, favoritos y ajustes a la carpeta ALLESIU-respaldo del USB.",
        "Vuelve a copiar a la consola lo que esté en ALLESIU-respaldo (por ejemplo, después de reinstalar).",
        "Vuelve a mostrar la ayuda del principio: el USB, cómo jugar, carpetas, BIOS y controles.",
        "Versión, autor y página del proyecto.",
    };
    for (int r = 0; r < FILAS_ORDEN; r++)
    {
        float fy = y0 + r * paso;
        // Solo las filas que entran en el panel (las de los bordes, mientras se desplaza, también).
        if (fy < y + 124 - paso * .5f || fy > y + 124 + (FILAS_CONFIG - .5f) * paso) continue;
        if (ORDEN_CONFIG[r] < 0)
        {
            // Título de sección.
            const char* sec = SECCIONES[-ORDEN_CONFIG[r] - 1];
            Texto(F_ETIQUETA, sec, x + 56, fy + 26, C_ACENTO);
            Linea(x + 72 + AnchoTexto(F_ETIQUETA, sec), fy + 40, x + w - 56, fy + 40, 1.5f, C_ACENTO, .35f);
            continue;
        }
        int i = ORDEN_CONFIG[r];
        char v[64];
        switch (i)
        {
        case CF_PROPORCION: snprintf(v, sizeof v, "« %s »", PROPORCIONES[g.proporcion]); break;
        case CF_FILTRO: snprintf(v, sizeof v, "« %s »", FILTROS[g.filtro]); break;
        case CF_AVANCE: snprintf(v, sizeof v, "« ×%d »", g.avance); break;
        case CF_REBOBINAR: snprintf(v, sizeof v, "« %s »", g.rebobinar ? "Activado" : "Desactivado"); break;
        case CF_CONTINUAR: snprintf(v, sizeof v, "« %s »", g.continuar ? "Sí" : "No"); break;
        case CF_BEZELS: snprintf(v, sizeof v, "« %s »", g.bezels ? "Sí" : "No"); break;
        case CF_RUNAHEAD: snprintf(v, sizeof v, "« %s »", g.runahead ? "Sí" : "No"); break;
        case CF_SUPERFX: snprintf(v, sizeof v, g.superfx == 1 ? "« ×1 (original) »" : "« ×%d »", g.superfx); break;
        case CF_COPIAR_AUTO: snprintf(v, sizeof v, "« %s »", g.copiarAuto ? "Sí" : "No"); break;
        case CF_CARATULAS_AUTO: snprintf(v, sizeof v, "« %s »", g.caratulasAuto ? "Sí" : "No"); break;
        case CF_MUSICA: snprintf(v, sizeof v, "« %s »", g.musica ? "Sí" : "No"); break;
        default: snprintf(v, sizeof v, "Ver  »"); break;
        }
        bool elegida = i == selConfig;
        char libre[96];
        const char* ayuda = AYUDAS[i];
        if (!ayuda && i == CF_BIOS)
        {
            int hay = 0;
            for (int k = 0; k < BiosCuantas(); k++) hay += BiosEstado(k) == BIOS_BIEN;
            snprintf(libre, sizeof libre, "%d de %d BIOS conocidas en la consola. Ponlas en la carpeta bios del USB, con cualquier nombre.", hay, BiosCuantas());
            ayuda = libre;
        }
        if (!ayuda) { TextoLibre(libre, sizeof libre); ayuda = libre; }
        Texto(F_CUERPO_M, TITULOS[i], x + 56, fy + 2, elegida ? C_TEXTO : C_TEXTO_2);
        Texto(F_PEQUENA, ayuda, x + 56, fy + 38, C_TEXTO_3);
        Texto(F_CUERPO_M, v, x + w - 56, fy + 12, elegida ? C_ACENTO : C_TEXTO_2, 1, 2);
    }
    Texto(F_PEQUENA, "↑ ↓ elegir   ·   ← → cambiar   ·   L1 / R1 otras pestañas", ANCHO / 2, ALTO - 50, C_TEXTO_3, 1, 1);
}

// ---------- Configuración → Imagen por consola ----------
// Una fila por consola (las que trae este programa) con su proporción y su filtro: ↑ ↓ eligen la consola, ← → la
// columna, X cambia el valor (General = usa el de la configuración general), ○ vuelve.
static int selConsolaCfg, colConsolaCfg;
static void PantallaConsolas(const Mando& m)
{
    int lista[NUM_SISTEMAS], n = 0;
    for (int s = 0; s < NUM_SISTEMAS; s++) if (ConsolaDe(s).presente) lista[n++] = s;
    if (m.pulsados & B_CIRCULO) { viendoConsolas = false; Sonar(SON_ATRAS); return; }
    if (m.pulsados & B_ARRIBA) { selConsolaCfg = (selConsolaCfg + n - 1) % n; Sonar(SON_MOVER); }
    if (m.pulsados & B_ABAJO) { selConsolaCfg = (selConsolaCfg + 1) % n; Sonar(SON_MOVER); }
    if (m.pulsados & (B_IZQ | B_DER)) { colConsolaCfg ^= 1; Sonar(SON_MOVER); }
    if (m.pulsados & B_CRUZ)
    {
        AjustesConsola& c = DeConsola(lista[selConsolaCfg]);
        int& v = colConsolaCfg ? c.filtro : c.proporcion;
        int maximo = colConsolaCfg ? NUM_FILTROS : NUM_PROPORCIONES;
        v = v + 1 >= maximo ? -1 : v + 1;   // General → cada valor → General
        AjustesConsolasGuardar();
        Sonar(SON_MOVER);
    }
    float w = 1180, x = (ANCHO - w) / 2, y = 168, fila = 44, h = 190 + n * fila;
    if (h > ALTO - y - 70) { fila = (ALTO - y - 260) / n; h = 190 + n * fila; }
    Panel(x, y, w, h, 28, 1);
    Texto(F_SUBTITULO, "Imagen por consola", x + 48, y + 30, C_TEXTO);
    Texto(F_PEQUENA, "General = la de la configuración general. Cada juego puede cambiarla además desde el menú de pausa (L3 + R3).",
          x + 48, y + 80, C_TEXTO_3);
    Texto(F_PEQUENA, "Proporción", x + 560, y + 120, colConsolaCfg == 0 ? C_ACENTO : C_TEXTO_3);
    Texto(F_PEQUENA, "Filtro", x + 880, y + 120, colConsolaCfg == 1 ? C_ACENTO : C_TEXTO_3);
    for (int i = 0; i < n; i++)
    {
        const AjustesConsola& c = DeConsola(lista[i]);
        float fy = y + 156 + i * fila;
        bool el = i == selConsolaCfg;
        if (el) Caja(x + 24, fy - 4, w - 48, fila - 2, Radio(12), C_ACENTO, .18f);
        Texto(F_CUERPO, ConsolaDe(lista[i]).nombreLargo, x + 56, fy + 4, el ? C_TEXTO : C_TEXTO_2);
        const char* pr = c.proporcion >= 0 ? PROPORCIONES[c.proporcion] : "General";
        const char* fi = c.filtro >= 0 ? FILTROS[c.filtro] : "General";
        Texto(F_CUERPO, pr, x + 560, fy + 4, el && colConsolaCfg == 0 ? C_ACENTO : c.proporcion >= 0 ? C_TEXTO : C_TEXTO_3);
        Texto(F_CUERPO, fi, x + 880, fy + 4, el && colConsolaCfg == 1 ? C_ACENTO : c.filtro >= 0 ? C_TEXTO : C_TEXTO_3);
    }
    static const int b[3] = {BM_CRUZ, BM_CIRCULO};
    static const char* const t[2] = {"Cambiar", "Volver"};
    PistasEn(b, t, 2, 1, ALTO - 45);
}

// ---------- Configuración → BIOS ----------
// Cada BIOS conocida: si está en la consola (verde), si hay un archivo con su nombre que no es el correcto (ámbar) o si
// falta (gris; roja si sin ella la consola no corre).
static void PantallaBios()
{
    int n = BiosCuantas();
    float w = 1180, x = (ANCHO - w) / 2, y = 168, fila = 40, h = 196 + n * fila;
    Panel(x, y, w, h, 28, 1);
    Texto(F_SUBTITULO, "BIOS", x + 48, y + 30, C_TEXTO);
    Texto(F_PEQUENA, "Copia tus BIOS a la carpeta bios del USB, con cualquier nombre (sueltas o en un zip). Al buscar juegos, la",
          x + 48, y + 80, C_TEXTO_3);
    Texto(F_PEQUENA, "app las reconoce por su contenido y las pasa a la consola con el nombre correcto.", x + 48, y + 108, C_TEXTO_3);
    float fy = y + 160;
    for (int i = 0; i < n; i++, fy += fila)
    {
        const BiosConocida& b = BiosDe(i);
        int e = BiosEstado(i);
        uint32_t color = e == BIOS_BIEN ? C_BIEN : e == BIOS_DISTINTA ? C_AVISO : b.necesaria ? C_MAL : C_TEXTO_3;
        Circulo(x + 66, fy + 14, 9, color, 1);
        Texto(F_CUERPO_M, b.descripcion, x + 92, fy, e == BIOS_BIEN ? C_TEXTO : C_TEXTO_2);
        const char* que = e == BIOS_BIEN ? "Lista" : e == BIOS_DISTINTA ? "No es la correcta" : b.necesaria ? "Falta (necesaria)" : "Falta (opcional)";
        Texto(F_PEQUENA, b.archivo, x + 620, fy + 6, C_TEXTO_3);
        Texto(F_CUERPO_M, que, x + w - 56, fy, color == C_TEXTO_3 ? C_TEXTO_2 : color, 1, 2);
    }
    static const int boton[2] = {BM_TRIANGULO, BM_CIRCULO};
    const char* const texto[2] = {BibliotecaBuscando(NULL) ? "Buscando…" : "Buscar en el USB ahora", "Volver"};
    PistasEn(boton, texto, 2, 1, ALTO - 45);
}

// ---------- Configuración → Acerca de SNESUI ----------
static void AcercaDe()
{
    float w = 1180, x = (ANCHO - w) / 2, y = 250, h = 650;
    Panel(x, y, w, h, 28, 1);
    float cx = ANCHO / 2;
    if (logo)
    {
        // El logo tiene fondo negro: va sobre una franja negra de lado a lado (así el negro es parte del diseño).
        float lw = 620, lh = lw * logoH / logoW;
        Caja(x + 20, y + 20, w - 40, lh + 8, 20, 0x000000, 1);
        Imagen(logo, logoW, logoH, cx - lw / 2, y + 24, lw, lh, 1, true);
    }
    char v[128];
    snprintf(v, sizeof v, "Emulador de consolas clásicas para PS4   ·   versión %s   ·   compilación %s", VERSION, COMPILACION);
    Texto(F_CUERPO, v, cx, y + 270, C_TEXTO_2, 1, 1);
    Texto(F_PEQUENA, "CREADO POR", cx, y + 318, C_TEXTO_3, 1, 1);
    Texto(F_TITULO, AUTOR, cx, y + 344, C_TEXTO, 1, 1);
    float lw = AnchoTexto(F_TITULO, AUTOR);
    FranjaSnes(cx - lw / 2, y + 400, lw, 6, 1);

    char web[96];
    snprintf(web, sizeof web, "Descargas y novedades:  %s", PAGINA);
    Texto(F_CUERPO_M, web, cx, y + 424, C_ACENTO, 1, 1);
    Texto(F_ETIQUETA, "Agradecimientos", cx, y + 470, C_TEXTO_2, 1, 1);
    static const char* const GRACIAS[] = {
        "Emuladores: Snes9x · FCEUmm · Gambatte · mGBA · Genesis Plus GX · Beetle NeoPop · ProSystem · Handy · FinalBurn Neo",
        "Carátulas: RetroArch (libretro-thumbnails)   ·   Documentación de la SNES: fullsnes",
        "Herramientas para PS4: OpenOrbis y GoldHEN",
        "Letra Inter (OFL)   ·   Imágenes PNG: stb",
    };
    for (int i = 0; i < 4; i++) Texto(F_PEQUENA, GRACIAS[i], cx, y + 512 + i * 30, C_TEXTO_3, 1, 1);
    Texto(F_PEQUENA, "ALLESIU no tiene relación con Nintendo, Sega, SNK, Atari ni Capcom. Usa solo juegos que tengas.", cx, y + h + 26, C_TEXTO_3, 1, 1);
    int b[1] = {BM_CIRCULO};
    const char* tx[1] = {"Volver a la configuración"};
    Pistas(b, tx, 1);
}

// Cuando termina una búsqueda, la lista nueva reemplaza a la anterior y la selección se queda en el mismo juego.
static void TomarListaNueva()
{
    // Después de copiar o borrar: buscar de nuevo apenas termine la búsqueda en curso.
    if (releer && !BibliotecaBuscando(NULL)) { releer = false; BibliotecaBuscarDe(false); }
    if (!BibliotecaNueva()) return;
    char elegidoClave[96] = "";
    if (nVista > 0 && sel < nVista) snprintf(elegidoClave, sizeof elegidoClave, "%s", juegos[vista[sel]].clave);
    juegos = BibliotecaJuegos(&nJuegos);
    ArmarPestanas();
    ArmarVista();
    for (int i = 0; i < nVista; i++)
        if (!strcmp(juegos[vista[i]].clave, elegidoClave)) { sel = i; break; }
    // Copia automática: los juegos nuevos del USB pasan a la consola (si hay espacio; el hilo de copia lo verifica).
    if (General().copiarAuto && !CopiaEnCurso()) { CopiarPendientes(); copiaAutomatica = true; }
}

// ---------- teclado en pantalla (búsqueda por nombre) ----------
static bool teclado;
static float tecladoVis;
static int tecla;   // fila * 10 + columna; la última fila son las acciones
static const int FILAS_TECLADO = 5, COLS_TECLADO = 10;
static const char* const TECLAS[4] = {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-'&!"};
enum { AC_ESPACIO, AC_BORRAR, AC_LISTO };
static int Accion(int col) { return col < 4 ? AC_ESPACIO : col < 7 ? AC_BORRAR : AC_LISTO; }

static void Escribir(char c)
{
    size_t n = strlen(busqueda);
    if (c == '\b') { if (n) busqueda[n - 1] = 0; }
    else if (n + 1 < sizeof busqueda && !(c == ' ' && (n == 0 || busqueda[n - 1] == ' '))) { busqueda[n] = c; busqueda[n + 1] = 0; }
    sel = 0;
    scrollVis = 0;
    ArmarVista();
}

static void TecladoEntrada(const Mando& m)
{
    int f = tecla / COLS_TECLADO, c = tecla % COLS_TECLADO;
    if (m.pulsados & B_IZQ) c = (c + COLS_TECLADO - 1) % COLS_TECLADO;
    if (m.pulsados & B_DER) c = (c + 1) % COLS_TECLADO;
    if (m.pulsados & B_ARRIBA) f = (f + FILAS_TECLADO - 1) % FILAS_TECLADO;
    if (m.pulsados & B_ABAJO) f = (f + 1) % FILAS_TECLADO;
    if (f * COLS_TECLADO + c != tecla) { tecla = f * COLS_TECLADO + c; Sonar(SON_MOVER); }
    if (m.pulsados & B_CUADRADO) { Escribir('\b'); Sonar(SON_ATRAS); }
    if (m.pulsados & (B_CIRCULO | B_TACTIL | B_OPTIONS)) { teclado = false; Sonar(SON_ATRAS); return; }
    if (!(m.pulsados & B_CRUZ)) return;
    if (f < 4) { Escribir(TECLAS[f][c]); Sonar(SON_ACEPTAR); return; }
    switch (Accion(c))
    {
    case AC_ESPACIO: Escribir(' '); Sonar(SON_ACEPTAR); break;
    case AC_BORRAR: Escribir('\b'); Sonar(SON_ATRAS); break;
    default: teclado = false; Sonar(SON_ACEPTAR); break;
    }
}

// Una tecla con la forma y el color del estilo de botones: entera la elegida, tenue las demás.
static void Tecla(float x, float y, float w, float h, bool elegida, float a)
{
    Capsula(x, y, w, h, (elegida ? 1 : .3f) * a);
    if (elegida) Velo(x, y, w, h * .5f, h * .3f, 0xFFFFFF, .18f * a, 0);
}

static void TecladoDibujar(float a)
{
    if (a <= .01f) return;
    float kw = 96, kh = 66, sep = 10;
    float w = COLS_TECLADO * (kw + sep) - sep + 64, h = 150 + FILAS_TECLADO * (kh + sep);
    float x = (ANCHO - w) / 2, y = ALTO - h - 100 + (1 - a) * 60;
    // Con el material de la interfaz elegida (L2) y las teclas con la forma y el color del estilo de botones (○).
    Sombra(x, y, w, h, Radio(28), 40, .5f * a);
    Panel(x, y, w, h, 28, a);
    Caja(x, y, w, h, Radio(28), C_PANEL, (TemaClaro() ? .35f : .45f) * a);   // que las teclas se lean sobre cualquier fondo
    // Lo escrito, con un cursor que parpadea, y cuántos juegos coinciden.
    Caja(x + 32, y + 28, w - 64, 70, Radio(16), TemaClaro() ? 0xFFFFFF : 0x000000, (TemaClaro() ? .55f : .4f) * a);
    char t[64];
    snprintf(t, sizeof t, "%s%s", busqueda, ((int)(Segundos() * 2) & 1) ? "" : "|");
    Texto(F_SUBTITULO, busqueda[0] || ((int)(Segundos() * 2) & 1) == 0 ? t : "Escribe el nombre del juego", x + 56, y + 44,
          busqueda[0] ? C_TEXTO : C_TEXTO_3, a);
    char cuenta[48];
    snprintf(cuenta, sizeof cuenta, nVista == 1 ? "%d juego" : "%d juegos", nVista);
    Texto(F_CUERPO_M, cuenta, x + w - 56, y + 50, nVista ? C_ACENTO : C_MAL, a, 2);
    float ky = y + 124;
    for (int f = 0; f < FILAS_TECLADO; f++)
        for (int c = 0; c < COLS_TECLADO; c++)
        {
            float kx = x + 32 + c * (kw + sep), yy = ky + f * (kh + sep);
            bool elegida = tecla == f * COLS_TECLADO + c;
            if (f < 4)
            {
                Tecla(kx, yy, kw, kh, elegida, a);
                char l[2] = {TECLAS[f][c], 0};
                Texto(F_SUBTITULO, l, kx + kw / 2, yy + 14, elegida ? TextoCapsula() : C_TEXTO, a, 1);
            }
            else if (c == 0 || c == 4 || c == 7)
            {
                // Las acciones ocupan varias columnas: Espacio (4), Borrar (3), Listo (3).
                int ancho = c == 0 ? 4 : 3;
                bool sel2 = tecla / COLS_TECLADO == 4 && Accion(tecla % COLS_TECLADO) == Accion(c);
                float aw = ancho * (kw + sep) - sep;
                Tecla(kx, yy, aw, kh, sel2, a);
                static const char* const NOMBRES[3] = {"Espacio", "Borrar", "Listo"};
                Texto(F_CUERPO_M, NOMBRES[Accion(c)], kx + aw / 2, yy + 18, sel2 ? TextoCapsula() : C_TEXTO, a, 1);
            }
        }
    int b[3] = {BM_CRUZ, BM_CUADRADO, BM_CIRCULO};
    const char* tx[3] = {"Escribir", "Borrar", "Cerrar"};
    Pistas(b, tx, 3, a);
}

static void Biblioteca(float t, float dt, const Mando& m)
{
    FondoPixel(t);
    TomarListaNueva();

    // Cabecera: el logo de SNESUI, chico, arriba a la izquierda (si no se pudo cargar, el nombre en texto).
    if (logo)
    {
        float lh = 64, lw = lh * logoW / logoH;
        // El logo es claro sobre negro (se suma al fondo): en los temas claros va sobre una pastilla oscura.
        if (TemaClaro()) Caja(MARGEN - 34, 12, lw + 34, lh + 12, Radio(22), 0x1B1A2C, .88f);
        ImagenSumada(logo, logoW, logoH, MARGEN - 18, 18, lw, lh, 1);
    }
    else
    {
        Texto(F_TITULO, "ALLESIU", MARGEN, 20, C_TEXTO);
        FranjaSnes(MARGEN, 80, AnchoTexto(F_TITULO, "ALLESIU"), 7, 1);
    }
    char cont[48];
    int halladas = 0;
    if (BibliotecaBuscando(&halladas)) snprintf(cont, sizeof cont, "Buscando juegos…  %d", halladas);
    else
    {
        // Los que están en el USB y también en la consola cuentan una sola vez.
        int distintos = 0;
        for (int i = 0; i < nJuegos; i++) if (Visible(juegos[i])) distintos++;
        snprintf(cont, sizeof cont, distintos == 1 ? "%d juego" : "%d juegos", distintos);
    }
    Texto(F_CUERPO, cont, ANCHO - MARGEN, 36, C_TEXTO_2, 1, 2);


    // El teclado en pantalla y las ventanas se llevan los botones mientras están abiertos.
    Suave(tecladoVis, teclado ? 1 : 0, 14, dt);
    MedirEspacio(dt);
    static const Mando nada = {};
    const Mando& mb = teclado || ventana != VN_NADA || eligiendoCaratula ? nada : m;
    if (eligiendoCaratula) EligeCaratulaEntrada(m);
    else if (ventana != VN_NADA) VentanaEntrada(m);
    else if (teclado) TecladoEntrada(m);

    // Pestañas: L1 / R1.
    int cambio = (mb.pulsados & B_R1) ? 1 : (mb.pulsados & B_L1) ? -1 : 0;
    if (cambio)
    {
        vistaActual = (vistaActual + nPestanas + cambio) % nPestanas;
        viendoAcerca = viendoBios = viendoConsolas = false;
        sel = 0;
        scrollVis = 0;
        ArmarVista();
        Sonar(SON_SECCION);
    }
    DibujarPestanas(dt);
    if (Tipo() == V_CONFIG)
    {
        Configuracion(dt, mb);
        DibujarDescarga(dt);
        return;
    }

    if ((mb.pulsados & B_CRUZ) && nVista > 0)
    {
        const Juego& j = juegos[vista[sel]];
        Sonar(SON_ACEPTAR);
        AnotarReciente(j.clave);
        JuegoAbrir(j.ruta, j.nombre, j.sistema);
        if (Tipo() == V_RECIENTES) { ArmarVista(); sel = 0; }
    }
    if ((mb.pulsados & B_OPTIONS) && nVista > 0) { AbrirMenuJuego(juegos[vista[sel]]); Sonar(SON_SECCION); }
    if (mb.pulsados & B_TRIANGULO) { Sonar(BibliotecaBuscando(NULL) ? SON_ERROR : SON_ACEPTAR); BibliotecaBuscar(); }
    // Panel táctil: buscar por nombre. ○ quita la búsqueda.
    if ((mb.pulsados & B_TACTIL) && nJuegos > 0) { teclado = true; Sonar(SON_SECCION); }
    if ((mb.pulsados & B_CIRCULO) && busqueda[0]) { busqueda[0] = 0; sel = 0; scrollVis = 0; ArmarVista(); Sonar(SON_ATRAS); }
    else if (mb.pulsados & B_CIRCULO)
    {
        AjustesGenerales& g = General();
        g.botones = (BotonesElegido() + 1) % NumEstilosBoton();
        BotonesElegir(g.botones);
        AjustesGuardar();
        char t[96];
        snprintf(t, sizeof t, "Botones %d de %d:  %s", g.botones + 1, NumEstilosBoton(), NombreEstiloBoton(g.botones));
        Avisar(t, C_ACENTO);
        Sonar(SON_SECCION);
    }
    if ((mb.pulsados & B_R3) && nVista > 1)
    {
        static uint32_t semilla = 0;
        if (!semilla) semilla = (uint32_t)(Segundos() * 1000) | 1;
        semilla ^= semilla << 13; semilla ^= semilla >> 17; semilla ^= semilla << 5;
        int nuevo = (int)(semilla % (uint32_t)nVista);
        if (nuevo == sel) nuevo = (nuevo + 1) % nVista;
        sel = nuevo;
        Avisar("Juego al azar", C_ACENTO);
        Sonar(SON_SECCION);
    }
    // L3: la canción siguiente (si la música estaba apagada, la enciende).
    if (mb.pulsados & B_L3)
    {
        AjustesGenerales& g = General();
        if (!g.musica) { g.musica = 1; AjustesGuardar(); MusicaEncender(true, (unsigned)(Segundos() * 1000)); }
        int c = MusicaSiguiente();
        char t[96];
        snprintf(t, sizeof t, "Música %d de %d:  %s", c + 1, NUM_CANCIONES, MusicaNombre(c));
        Avisar(t, C_ACENTO);
    }
    if ((mb.pulsados & B_CUADRADO) && nVista > 0 && sel < nVista)
    {
        const Juego& j = juegos[vista[sel]];
        CambiarFavorito(j.clave);
        bool es = EsFavorito(j.clave);
        Avisar(es ? "Agregado a favoritos" : "Quitado de favoritos", es ? C_SNES_B : C_TEXTO_2);
        Sonar(es ? SON_ACEPTAR : SON_ATRAS);
        if (Tipo() == V_FAVORITOS) { ArmarVista(); if (sel >= nVista) sel = nVista > 0 ? nVista - 1 : 0; }
    }
    int antes = sel;
    int dx = ((mb.pulsados & B_DER) ? 1 : 0) - ((mb.pulsados & B_IZQ) ? 1 : 0);
    int dy = ((mb.pulsados & B_ABAJO) ? 1 : 0) - ((mb.pulsados & B_ARRIBA) ? 1 : 0);
    if (menu->forma == MF_LISTA) sel += dy + dx * FILAS_PAGINA;        // ← → saltan una página
    else if (menu->forma == MF_CARRUSEL) sel += dx + dy * 5;           // ↑ ↓ saltan de a 5
    else sel += dx + dy * COLS;
#ifdef PRUEBA_JIT
    // Pruebas para el N64: memoria ejecutable los primeros 15 segundos, después la de la GPU otros 15.
    static float tJit;
    if (tJit < 30) { tJit += dt; Avisar(tJit < 15 ? PruebaJit() : PruebaGpu(), C_BIEN); }
#endif
    // L2 / R2: el estilo de interfaz y el de menú (mientras se eligen; queda guardado en la configuración).
    if (mb.pulsados & (B_L2 | B_R2))
    {
        AjustesGenerales& g = General();
        char t[96];
        if (mb.pulsados & B_L2)
        {
            g.tema = (TemaElegido() + 1) % NUM_TEMAS;
            TemaElegir(g.tema);
            snprintf(t, sizeof t, "Interfaz %d de %d:  %s", g.tema + 1, NUM_TEMAS, TemaDe(g.tema).nombre);
        }
        else
        {
            g.menu = (menuElegido + 1) % NUM_MENUS;
            MenuElegir(g.menu);
            scrollVis = listaVis = 0;
            carruselVis = (float)sel;
            snprintf(t, sizeof t, "Menú %d de %d:  %s", g.menu + 1, NUM_MENUS, MENUS[g.menu].nombre);
        }
        AjustesGuardar();
        Avisar(t, C_ACENTO);
        Sonar(SON_SECCION);
    }
    if (sel < 0) sel = 0;
    if (sel >= nVista) sel = nVista > 0 ? nVista - 1 : 0;
    // Stick derecho a los costados: al primer juego de la letra siguiente o anterior (se repite si se mantiene).
    int dirLetra = mb.rx > .6f ? 1 : mb.rx < -.6f ? -1 : 0;
    if (!dirLetra) esperaLetra = 0;
    else if ((esperaLetra -= dt) <= 0 && nVista > 0)
    {
        esperaLetra = .32f;
        char actual = Letra(juegos[vista[sel]].nombre);
        int i = sel;
        if (dirLetra > 0) { while (i < nVista - 1 && Letra(juegos[vista[i]].nombre) == actual) i++; }
        else
        {
            // Atrás: primero salir de la letra actual, después ir al primero de la anterior.
            if (i > 0 && Letra(juegos[vista[i - 1]].nombre) == actual) { while (i > 0 && Letra(juegos[vista[i - 1]].nombre) == actual) i--; }
            else if (i > 0)
            {
                char previa = Letra(juegos[vista[i - 1]].nombre);
                i--;
                while (i > 0 && Letra(juegos[vista[i - 1]].nombre) == previa) i--;
            }
        }
        sel = i;
        letraVisible = Letra(juegos[vista[sel]].nombre);
        tLetra = 0;
    }
    if (sel != antes) Sonar(SON_MOVER);

    if (nJuegos <= 0 && BibliotecaBuscando(NULL))
    {
        float w = 900, h = 220, x = (ANCHO - w) / 2, y = 400;
        Panel(x, y, w, h, 30, 1);
        int n = 0;
        BibliotecaBuscando(&n);
        char t[64];
        snprintf(t, sizeof t, "Buscando juegos en el USB…  %d", n);
        Texto(F_SUBTITULO, t, x + w / 2, y + 70, C_TEXTO, 1, 1);
        Texto(F_CUERPO, "La primera vez tarda un poco. Después la biblioteca aparece al instante.", x + w / 2, y + 140, C_TEXTO_2, 1, 1);
        return;
    }
    if (nJuegos <= 0)
    {
        float w = 1000, h = 500, x = (ANCHO - w) / 2, y = 290;   // alto para las 4 líneas del texto y la nota de abajo
        Panel(x, y, w, h, 30, 1);
        Borde(x, y, w, h, 30, 1.5f, 0xFFFFFF, .08f);
        FranjaSnes(x + w / 2 - 120, y + 70, 240, 10, .9f);
        Texto(F_TITULO, "Todavía no hay juegos", x + w / 2, y + 160, C_TEXTO, 1, 1);
        Parrafo(F_CUERPO, "Copia tus juegos al USB (lo más ordenado: roms/snes, roms/gba, roms/megadrive, roms/arcade...) y pulsa "
                          "triángulo para buscar. Las BIOS van en la carpeta bios. Las carátulas se bajan solas si hay internet. "
                          "La ayuda completa está en Configuración → Asistente de inicio.", x + 80, y + 230, w - 160, C_TEXTO_2);
        Texto(F_PEQUENA, CarpetasDeJuegos(), x + w / 2, y + h - 48, C_TEXTO_3, 1, 1);
        return;
    }

    if (nVista <= 0)
    {
        float w = 900, h = 260, x = (ANCHO - w) / 2, y = 380;
        Panel(x, y, w, h, 30, 1);
        const char* t = busqueda[0] ? "Ningún juego coincide" : Tipo() == V_RECIENTES ? "Todavía no jugaste nada" : "Todavía no hay favoritos";
        const char* d = busqueda[0] ? "Prueba con otra palabra (panel táctil) o pulsa círculo para ver todos."
                        : Tipo() == V_RECIENTES ? "Los juegos que abras van a aparecer acá, el último primero."
                                                     : "Pulsa OPTIONS sobre un juego para marcarlo con una estrella.";
        Texto(F_SUBTITULO, t, x + w / 2, y + 80, C_TEXTO, 1, 1);
        Texto(F_CUERPO, d, x + w / 2, y + 150, C_TEXTO_2, 1, 1);
        DibujarBusqueda();
        TecladoDibujar(tecladoVis);
        DibujarDescarga(dt);
        return;
    }

    if (menu->forma == MF_LISTA) DibujarLista(dt);
    else if (menu->forma == MF_CARRUSEL) DibujarCarrusel(dt);
    else
    {
    if (menu->forma == MF_DESTACADO) FichaGrande(juegos[vista[sel]], MARGEN, Y0 + 10, 600, ALTO - Y0 - PIE - 260);
    int fila = sel / COLS;
    const float paso = CH + GAPY;
    float objScroll = fila * paso - paso;
    if (objScroll < 0) objScroll = 0;
    int filas = (nVista + COLS - 1) / COLS;
    float maxScroll = filas * paso - (ALTO - Y0 - PIE - 30);
    if (maxScroll < 0) maxScroll = 0;
    if (objScroll > maxScroll) objScroll = maxScroll;
    Suave(scrollVis, objScroll, 12, dt);

    // Hasta poco después de la última fila que entra (que no se asome otra cortada arriba de la barra de abajo).
    Recorte(menu->forma == MF_DESTACADO ? zonaX - 30 : 0, Y0 - 30, ANCHO, FILAS_PAGINA * (CH + GAPY) + 10);
    for (int i = 0; i < nVista; i++)
    {
        Suave(foco[i], i == sel ? 1 : 0, 14, dt);
        float x = X0 + (i % COLS) * (CW + GAPX);
        float y = Y0 + (i / COLS) * paso - scrollVis;
        if (y > ALTO || y + CH < 100) continue;
        if (i != sel) Tarjeta(juegos[vista[i]], x, y, foco[i]);
    }
    float sx = X0 + (sel % COLS) * (CW + GAPX), sy = Y0 + (sel / COLS) * paso - scrollVis;
    Tarjeta(juegos[vista[sel]], sx, sy, foco[sel]);
    SinRecorte();

    // Barra de posición a la derecha (solo si no entran todos en pantalla).
    if (filas > FILAS_PAGINA)
    {
        float bx = ANCHO - 40, by = Y0 - 10, bh = ALTO - Y0 - PIE - 30;
        float total = filas * paso, visible = bh;
        float th = bh * (visible / total);
        if (th < 60) th = 60;
        float ty = by + (bh - th) * (maxScroll > 0 ? scrollVis / maxScroll : 0);
        Caja(bx, by, 8, bh, 4, 0xFFFFFF, .08f);
        Caja(bx, ty, 8, th, 4, C_ACENTO, .85f);
    }
    }

    // La letra a la que se saltó, grande en el centro un momento.
    tLetra += dt;
    if (tLetra < .9f)
    {
        float a = tLetra < .6f ? 1 : (.9f - tLetra) / .3f;
        char l[2] = {letraVisible, 0};
        Panel(ANCHO / 2 - 90, ALTO / 2 - 100, 180, 180, 36, a);
        Borde(ANCHO / 2 - 90, ALTO / 2 - 100, 180, 180, 36, 3, C_ACENTO, .8f * a);
        Texto(F_LOGO, l, ANCHO / 2, ALTO / 2 - 88, C_TEXTO, a, 1);
    }

    // Pie, en una sola línea: el nombre del juego y sus datos a la izquierda (recortados para no llegar a los botones), y
    // los botones a la derecha.
    // Una franja de vidrio difuminado detrás de la línea de abajo, apenas del alto de las letras y los botones.
    {
        float fx = MARGEN - 28, fw = ANCHO - 2 * fx, fh = PIE - 6, fy = ALTO - PIE + 2;
        CajaVidrio(fx, fy, fw, fh, fh / 2, C_PANEL, TemaClaro() ? .35f : .45f, 1);
        Borde(fx, fy, fw, fh, fh / 2, 1.2f, 0xFFFFFF, .22f);
    }
    const Juego& j = juegos[vista[sel]];
    int b[5] = {BM_CRUZ, BM_OPTIONS, BM_TRIANGULO, BM_CUADRADO, BM_TACTIL};
    const char* tx[5] = {"Jugar", "Opciones", "Actualizar", EsFavorito(j.clave) ? "Quitar favorito" : "Favorito", "Buscar"};
    float anchoPistas = 4 * 40;
    for (int i = 0; i < 5; i++) anchoPistas += Pista(b[i], tx[i], 0, 0, 1, true);
    float limite = ANCHO - MARGEN - anchoPistas - 40, cy = ALTO - PIE / 2;
    char corto[128], buf[160];
    const char* nom = Acortar(F_CUERPO_M, j.nombre, (limite - MARGEN) * .55f, corto, sizeof corto);
    Texto(F_CUERPO_M, nom, MARGEN, cy - AltoLinea(F_CUERPO_M) / 2, C_TEXTO);
    float x = MARGEN + AnchoTexto(F_CUERPO_M, nom) + 24;
    char pos[48];
    snprintf(pos, sizeof pos, "%d de %d", sel + 1, nVista);
    Texto(F_PEQUENA, pos, x, cy - AltoLinea(F_PEQUENA) / 2, C_ACENTO);
    x += AnchoTexto(F_PEQUENA, pos) + 18;
    IconoLugar(j.enUsb, x + 12, cy, .6f, 1);
    x += 34;
    TextoDatos(j, buf, sizeof buf);
    if (limite - x > 60)
        Texto(F_PEQUENA, Acortar(F_PEQUENA, buf, limite - x, corto, sizeof corto), x, cy - AltoLinea(F_PEQUENA) / 2, C_TEXTO_2);
    // Con el teclado o una ventana abiertos, sus pistas reemplazan a estas.
    float tapa = tecladoVis > ventanaVis ? tecladoVis : ventanaVis;
    if (tapa < .5f) PistasEn(b, tx, 5, 1 - tapa * 2, ALTO - PIE / 2);
    DibujarBusqueda();
    TecladoDibujar(tecladoVis);
    DibujarDescarga(dt);
}
