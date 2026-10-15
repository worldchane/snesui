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
#include "ajustes.h"
#include "caratula.h"
#include "fondo.h"
#include "juego.h"

static const char* const VERSION = "0.9";
static const char* const AUTOR = "RudolphGamerBkn";   // el creador de SNESUI (Configuración → Acerca de)
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

// ---------- pestañas: todos, recientes y favoritos ----------
// Los recientes (los últimos abiertos, el más nuevo primero) y los favoritos se guardan por clave (nombre de archivo) en
// /data/snes-ps4/recientes.txt y favoritos.txt, una por línea.
enum { V_TODOS, V_RECIENTES, V_FAVORITOS, V_CONFIG, NUM_VISTAS };
static const char* const VISTAS[NUM_VISTAS] = {"Todos", "Recientes", "Favoritos", "Configuración"};
static int vistaActual;
static float vistaSuave;
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

static void ArmarVista()
{
    nVista = 0;
    if (vistaActual == V_CONFIG) { sel = 0; return; }
    if (vistaActual == V_RECIENTES)
    {
        for (int r = 0; r < nRecientes; r++)
            for (int i = 0; i < nJuegos; i++)
                if (!strcmp(juegos[i].clave, recientes[r])) { if (Coincide(juegos[i].nombre)) vista[nVista++] = i; break; }
    }
    else
        for (int i = 0; i < nJuegos; i++)
            if ((vistaActual == V_TODOS || EsFavorito(juegos[i].clave)) && Coincide(juegos[i].nombre)) vista[nVista++] = i;
    if (sel >= nVista) sel = nVista > 0 ? nVista - 1 : 0;
}

void InterfazIniciar()
{
    logo = CargarImagen(RUTA_ASSETS "/logo.png", &logoW, &logoH);
    BibliotecaIniciar();   // la lista guardada al instante; la búsqueda en el USB sigue en segundo plano
    juegos = BibliotecaJuegos(&nJuegos);
    nRecientes = LeerLista("recientes.txt", recientes, MAX_RECIENTES);
    nFavoritos = LeerLista("favoritos.txt", favoritos, MAX_FAVORITOS);
    AjustesLeer();
    ArmarVista();
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
static const int FILAS_PAGINA = 2;    // filas que entran en pantalla (lo que salta L2 / R2)
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

static void Tarjeta(const Juego& j, float x, float y, float f)
{
    float s = 1 + .06f * f;
    float w = CW * s, h = CH * s, cx = x + CW / 2, cy = y + CH / 2;
    float X = cx - w / 2, Y = cy - h / 2;
    Caratula car = ObtenerCaratula(j.clave);

    if (f > .01f) Brillo(cx, cy + 10, 250, car.px ? 0xFFFFFF : j.color1, (car.px ? .22f : .4f) * f);
    Sombra(X, Y + 14 + 6 * f, w, h, 20, 28 + 12 * f, .55f, true);

    // Sin carátula (o mientras se carga): la tarjeta con las iniciales. La carátula aparece encima con un fundido.
    if (!car.px || car.aparece < 1)
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
    if (car.px)
    {
        // La carátula real, con un marco oscuro tipo cartucho.
        float a = car.aparece;
        Caja(X, Y, w, h, 20, 0x0C0A18, a);
        float m = 12;
        // Mantener la proporción de la carátula dentro del hueco.
        float iw = w - 2 * m, ih = h - 2 * m;
        float escala = iw / car.w < ih / car.h ? iw / car.w : ih / car.h;
        float dw = car.w * escala, dh = car.h * escala;
        Imagen(car.px, car.w, car.h, cx - dw / 2, cy - dh / 2, dw, dh, a, true);
    }
    // Favorito: una insignia dorada con estrella en la esquina.
    if (EsFavorito(j.clave))
    {
        Circulo(X + w - 22, Y + 22, 20, C_SNES_B, 1);
        Estrella(X + w - 22, Y + 23, 11, 0x2A1A06, 1);
    }
    // Aro de selección en morado SNES.
    if (f > .01f) Borde(X - 6, Y - 6, w + 12, h + 12, 26, 4, C_ACENTO, .95f * f);
}

static void Biblioteca(float t, float dt, const Mando& m);

// La búsqueda activa, a la derecha de las pestañas; o la pista de cómo buscar.
static void DibujarBusqueda()
{
    // Alineada a la derecha, en la fila de las pestañas.
    float derecha = ANCHO - MARGEN, y = 160;
    if (busqueda[0])
    {
        // "Buscando: «texto»" y el botón ○ dibujado (la letra no trae los símbolos de los botones).
        char t[80];
        snprintf(t, sizeof t, "Buscando: «%s»", busqueda);
        float tw = AnchoTexto(F_CUERPO_M, t), qw = Pista(BM_CIRCULO, "quitar", 0, 0, 1, true);
        float w = tw + qw + 72;
        Caja(derecha - w, y, w, 48, 24, 0x0A0614, .75f);
        Borde(derecha - w, y, w, 48, 24, 2, C_SNES_B, .8f);
        Texto(F_CUERPO_M, t, derecha - w + 20, y + 10, C_TEXTO, 1);
        Pista(BM_CIRCULO, "quitar", derecha - qw - 20, y + 24);
    }
    else if (vistaActual != V_CONFIG) Texto(F_PEQUENA, "Panel táctil: buscar por nombre", derecha, y + 14, C_TEXTO_3, 1, 2);
}

// Las pestañas bajo la cabecera, con L1 y R1 a los costados y una pastilla que se desliza a la elegida.
static void DibujarPestanas()
{
    float y = 160, alto = 48, ancho = 190, x0 = MARGEN + 70;
    DibujarBoton(BM_L1, MARGEN + 24, y + alto / 2);
    Caja(x0 + vistaSuave * ancho, y, ancho - 10, alto, 24, C_ACENTO, .3f);
    Borde(x0 + vistaSuave * ancho, y, ancho - 10, alto, 24, 2, C_ACENTO, .8f);
    for (int i = 0; i < NUM_VISTAS; i++)
        Texto(F_CUERPO_M, VISTAS[i], x0 + i * ancho + (ancho - 10) / 2, y + 10, i == vistaActual ? C_TEXTO : C_TEXTO_2, 1, 1);
    DibujarBoton(BM_R1, x0 + (float)NUM_VISTAS * ancho + 20, y + alto / 2);
}

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

// ---------- pestaña de configuración (vale para todos los juegos) ----------
enum { CF_PROPORCION, CF_FILTRO, CF_AVANCE, CF_REBOBINAR, CF_ACERCA, NUM_CF };
static int selConfig;
static bool viendoAcerca;
static void AcercaDe();
static float selConfigSuave;

static void Configuracion(float dt, const Mando& m)
{
    AjustesGenerales& g = General();
    if (viendoAcerca)
    {
        if (m.pulsados & (B_CIRCULO | B_CRUZ)) { viendoAcerca = false; Sonar(SON_ATRAS); }
        AcercaDe();
        return;
    }
    if (selConfig == CF_ACERCA && (m.pulsados & (B_CRUZ | B_DER))) { viendoAcerca = true; Sonar(SON_ACEPTAR); return; }
    if (m.pulsados & B_ARRIBA) { selConfig = (selConfig + NUM_CF - 1) % NUM_CF; Sonar(SON_MOVER); }
    if (m.pulsados & B_ABAJO) { selConfig = (selConfig + 1) % NUM_CF; Sonar(SON_MOVER); }
    int d = (m.pulsados & (B_DER | B_CRUZ)) ? 1 : (m.pulsados & B_IZQ) ? -1 : 0;
    if (d && selConfig != CF_ACERCA)
    {
        switch (selConfig)
        {
        case CF_PROPORCION: g.proporcion = (g.proporcion + NUM_PROPORCIONES + d) % NUM_PROPORCIONES; break;
        case CF_FILTRO: g.filtro = (g.filtro + NUM_FILTROS + d) % NUM_FILTROS; break;
        case CF_AVANCE: g.avance = 2 + (g.avance - 2 + 3 + d) % 3; break;
        case CF_REBOBINAR: g.rebobinar = !g.rebobinar; break;
        }
        AjustesGuardar();
        Sonar(SON_MOVER);
    }
    Suave(selConfigSuave, (float)selConfig, 18, dt);

    float w = 1180, x = (ANCHO - w) / 2, y = 236, paso = 112, h = 130 + (float)NUM_CF * paso;
    Sombra(x, y, w, h, 28, 40, .5f, true);
    Caja(x, y, w, h, 28, C_PANEL, .92f);
    Texto(F_SUBTITULO, "Configuración general", x + 48, y + 36, C_TEXTO);
    Texto(F_PEQUENA, "Vale para todos los juegos. Cada juego puede tener su propia proporción y filtro desde el menú de pausa (L3 + R3).",
          x + 48, y + 86, C_TEXTO_3);
    float y0 = y + 130;
    Caja(x + 24, y0 + selConfigSuave * paso - 14, w - 48, paso - 12, 18, C_ACENTO, .2f);
    Borde(x + 24, y0 + selConfigSuave * paso - 14, w - 48, paso - 12, 18, 2, C_ACENTO, .7f);
    static const char* const TITULOS[NUM_CF] = {"Proporción de la imagen", "Filtro", "Avance rápido (mantener R2)", "Rebobinar (mantener L2)", "Acerca de SNESUI"};
    static const char* const AYUDAS[NUM_CF] = {
        "Cómo se ajusta la imagen a la pantalla.",
        "Suave, nítido o el aspecto de un televisor antiguo.",
        "Cuántas veces más rápido corre el juego.",
        "Guarda los últimos segundos para volver atrás. Desactivado, la consola trabaja menos.",
        "Versión, autor y página del proyecto.",
    };
    for (int i = 0; i < NUM_CF; i++)
    {
        float fy = y0 + i * paso;
        char v[64];
        switch (i)
        {
        case CF_PROPORCION: snprintf(v, sizeof v, "« %s »", PROPORCIONES[g.proporcion]); break;
        case CF_FILTRO: snprintf(v, sizeof v, "« %s »", FILTROS[g.filtro]); break;
        case CF_AVANCE: snprintf(v, sizeof v, "« ×%d »", g.avance); break;
        case CF_REBOBINAR: snprintf(v, sizeof v, "« %s »", g.rebobinar ? "Activado" : "Desactivado"); break;
        default: snprintf(v, sizeof v, "Ver  »"); break;
        }
        bool elegida = i == selConfig;
        Texto(F_CUERPO_M, TITULOS[i], x + 56, fy + 6, elegida ? C_TEXTO : C_TEXTO_2);
        Texto(F_PEQUENA, AYUDAS[i], x + 56, fy + 46, C_TEXTO_3);
        Texto(F_CUERPO_M, v, x + w - 56, fy + 16, elegida ? C_ACENTO : C_TEXTO_2, 1, 2);
    }
    Texto(F_PEQUENA, "↑ ↓ elegir   ·   ← → cambiar   ·   L1 / R1 otras pestañas", ANCHO / 2, ALTO - 50, C_TEXTO_3, 1, 1);
}

// ---------- Configuración → Acerca de SNESUI ----------
static void AcercaDe()
{
    float w = 1180, x = (ANCHO - w) / 2, y = 250, h = 650;
    Sombra(x, y, w, h, 28, 40, .5f, true);
    Caja(x, y, w, h, 28, C_PANEL, .92f);
    float cx = ANCHO / 2;
    if (logo)
    {
        // El logo tiene fondo negro: va sobre una franja negra de lado a lado (así el negro es parte del diseño).
        float lw = 620, lh = lw * logoH / logoW;
        Caja(x + 20, y + 20, w - 40, lh + 8, 20, 0x000000, 1);
        Imagen(logo, logoW, logoH, cx - lw / 2, y + 24, lw, lh, 1, true);
    }
    char v[64];
    snprintf(v, sizeof v, "Emulador de Super Nintendo para PS4   ·   versión %s", VERSION);
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
        "Carátulas: RetroArch (libretro-thumbnails)",
        "Comparación de exactitud: snes9x y bsnes   ·   Documentación: fullsnes",
        "Herramientas para PS4: OpenOrbis y GoldHEN",
        "Letra Inter (OFL)   ·   Imágenes PNG: stb",
    };
    for (int i = 0; i < 4; i++) Texto(F_PEQUENA, GRACIAS[i], cx, y + 512 + i * 30, C_TEXTO_3, 1, 1);
    Texto(F_PEQUENA, "SNESUI no tiene relación con Nintendo. Usa solo juegos que tengas.", cx, y + h + 26, C_TEXTO_3, 1, 1);
    int b[1] = {BM_CIRCULO};
    const char* tx[1] = {"Volver a la configuración"};
    Pistas(b, tx, 1);
}

// Cuando termina una búsqueda, la lista nueva reemplaza a la anterior y la selección se queda en el mismo juego.
static void TomarListaNueva()
{
    if (!BibliotecaNueva()) return;
    char elegido[96] = "";
    if (nVista > 0 && sel < nVista) snprintf(elegido, sizeof elegido, "%s", juegos[vista[sel]].clave);
    juegos = BibliotecaJuegos(&nJuegos);
    ArmarVista();
    for (int i = 0; i < nVista; i++)
        if (!strcmp(juegos[vista[i]].clave, elegido)) { sel = i; break; }
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

static void TecladoDibujar(float a)
{
    if (a <= .01f) return;
    float kw = 96, kh = 66, sep = 10;
    float w = COLS_TECLADO * (kw + sep) - sep + 64, h = 150 + FILAS_TECLADO * (kh + sep);
    float x = (ANCHO - w) / 2, y = ALTO - h - 100 + (1 - a) * 60;
    Sombra(x, y, w, h, 28, 40, .6f * a);
    Caja(x, y, w, h, 28, 0x150D2C, .97f * a);
    Borde(x, y, w, h, 28, 2, C_ACENTO, .5f * a);
    // Lo escrito, con un cursor que parpadea, y cuántos juegos coinciden.
    Caja(x + 32, y + 28, w - 64, 70, 16, 0x000000, .45f * a);
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
                Caja(kx, yy, kw, kh, 12, elegida ? C_ACENTO : 0xFFFFFF, (elegida ? .9f : .07f) * a);
                char l[2] = {TECLAS[f][c], 0};
                Texto(F_SUBTITULO, l, kx + kw / 2, yy + 14, C_TEXTO, a, 1);
            }
            else if (c == 0 || c == 4 || c == 7)
            {
                // Las acciones ocupan varias columnas: Espacio (4), Borrar (3), Listo (3).
                int ancho = c == 0 ? 4 : 3;
                bool sel2 = tecla / COLS_TECLADO == 4 && Accion(tecla % COLS_TECLADO) == Accion(c);
                float aw = ancho * (kw + sep) - sep;
                Caja(kx, yy, aw, kh, 12, sel2 ? C_ACENTO : 0xFFFFFF, (sel2 ? .9f : .1f) * a);
                static const char* const NOMBRES[3] = {"Espacio", "Borrar", "Listo"};
                Texto(F_CUERPO_M, NOMBRES[Accion(c)], kx + aw / 2, yy + 18, C_TEXTO, a, 1);
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

    // Cabecera: logo con una franja de los cuatro colores de la SNES.
    Texto(F_TITULO, "SNESUI", MARGEN, 58, C_TEXTO);
    float lw = AnchoTexto(F_TITULO, "SNESUI");
    FranjaSnes(MARGEN, 118, lw, 7, 1);
    char v[64];
    snprintf(v, sizeof v, "versión %s", VERSION);
    Texto(F_PEQUENA, v, ANCHO - MARGEN, 70, C_TEXTO_3, 1, 2);
    char cont[48];
    int halladas = 0;
    if (BibliotecaBuscando(&halladas)) snprintf(cont, sizeof cont, "Buscando juegos…  %d", halladas);
    else snprintf(cont, sizeof cont, nJuegos == 1 ? "%d juego" : "%d juegos", nJuegos > 0 ? nJuegos : 0);
    Texto(F_CUERPO, cont, ANCHO - MARGEN, 100, C_TEXTO_2, 1, 2);


    // El teclado en pantalla se lleva los botones mientras está abierto.
    Suave(tecladoVis, teclado ? 1 : 0, 14, dt);
    static const Mando nada = {};
    const Mando& mb = teclado ? nada : m;
    if (teclado) TecladoEntrada(m);

    // Pestañas: L1 / R1.
    int cambio = (mb.pulsados & B_R1) ? 1 : (mb.pulsados & B_L1) ? -1 : 0;
    if (cambio)
    {
        vistaActual = (vistaActual + NUM_VISTAS + cambio) % NUM_VISTAS;
        viendoAcerca = false;
        sel = 0;
        scrollVis = 0;
        ArmarVista();
        Sonar(SON_SECCION);
    }
    Suave(vistaSuave, (float)vistaActual, 14, dt);
    DibujarPestanas();
    if (vistaActual == V_CONFIG)
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
        JuegoAbrir(j.ruta, j.nombre);
        if (vistaActual == V_RECIENTES) { ArmarVista(); sel = 0; }
    }
    if ((mb.pulsados & B_OPTIONS) && nVista > 0)
    {
        CambiarFavorito(juegos[vista[sel]].clave);
        Sonar(SON_ACEPTAR);
        if (vistaActual == V_FAVORITOS) ArmarVista();
    }
    if (mb.pulsados & B_TRIANGULO) { Sonar(BibliotecaBuscando(NULL) ? SON_ERROR : SON_ACEPTAR); BibliotecaBuscar(); }
    // Panel táctil: buscar por nombre. ○ quita la búsqueda.
    if ((mb.pulsados & B_TACTIL) && nJuegos > 0) { teclado = true; Sonar(SON_SECCION); }
    if ((mb.pulsados & B_CIRCULO) && busqueda[0]) { busqueda[0] = 0; sel = 0; scrollVis = 0; ArmarVista(); Sonar(SON_ATRAS); }
    if ((mb.pulsados & B_CUADRADO) && nJuegos > 0)
    {
        static const char* lista[MAX_JUEGOS];
        for (int i = 0; i < nJuegos; i++) lista[i] = juegos[i].clave;
        Sonar(EmpezarDescarga(lista, nJuegos) ? SON_ACEPTAR : SON_ERROR);
        tResultado = 0;
    }
    int antes = sel;
    if (mb.pulsados & B_IZQ) sel--;
    if (mb.pulsados & B_DER) sel++;
    if (mb.pulsados & B_ARRIBA) sel -= COLS;
    if (mb.pulsados & B_ABAJO) sel += COLS;
    // L2 / R2: una página (las filas que entran en pantalla) hacia arriba o abajo.
    if (mb.pulsados & B_L2) sel -= COLS * FILAS_PAGINA;
    if (mb.pulsados & B_R2) sel += COLS * FILAS_PAGINA;
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
        Caja(x, y, w, h, 30, C_PANEL, .85f);
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

    if (nVista <= 0)
    {
        float w = 900, h = 260, x = (ANCHO - w) / 2, y = 380;
        Caja(x, y, w, h, 30, C_PANEL, .85f);
        const char* t = busqueda[0] ? "Ningún juego coincide" : vistaActual == V_RECIENTES ? "Todavía no jugaste nada" : "Todavía no hay favoritos";
        const char* d = busqueda[0] ? "Prueba con otra palabra (panel táctil) o pulsa círculo para ver todos."
                        : vistaActual == V_RECIENTES ? "Los juegos que abras van a aparecer acá, el último primero."
                                                     : "Pulsa OPTIONS sobre un juego para marcarlo con una estrella.";
        Texto(F_SUBTITULO, t, x + w / 2, y + 80, C_TEXTO, 1, 1);
        Texto(F_CUERPO, d, x + w / 2, y + 150, C_TEXTO_2, 1, 1);
        DibujarBusqueda();
        TecladoDibujar(tecladoVis);
        DibujarDescarga(dt);
        return;
    }

    int fila = sel / COLS;
    const float paso = CH + GAPY;
    float objScroll = fila * paso - paso;
    if (objScroll < 0) objScroll = 0;
    int filas = (nVista + COLS - 1) / COLS;
    float maxScroll = filas * paso - (ALTO - Y0 - 110);
    if (maxScroll < 0) maxScroll = 0;
    if (objScroll > maxScroll) objScroll = maxScroll;
    Suave(scrollVis, objScroll, 12, dt);

    Recorte(0, Y0 - 30, ANCHO, ALTO - Y0 - 80);
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
        float bx = ANCHO - 40, by = Y0 - 10, bh = ALTO - Y0 - 110;
        float total = filas * paso, visible = bh;
        float th = bh * (visible / total);
        if (th < 60) th = 60;
        float ty = by + (bh - th) * (maxScroll > 0 ? scrollVis / maxScroll : 0);
        Caja(bx, by, 8, bh, 4, 0xFFFFFF, .08f);
        Caja(bx, ty, 8, th, 4, C_ACENTO, .85f);
    }

    // La letra a la que se saltó, grande en el centro un momento.
    tLetra += dt;
    if (tLetra < .9f)
    {
        float a = tLetra < .6f ? 1 : (.9f - tLetra) / .3f;
        char l[2] = {letraVisible, 0};
        Caja(ANCHO / 2 - 90, ALTO / 2 - 100, 180, 180, 36, 0x0A0614, .82f * a);
        Borde(ANCHO / 2 - 90, ALTO / 2 - 100, 180, 180, 36, 3, C_ACENTO, .8f * a);
        Texto(F_LOGO, l, ANCHO / 2, ALTO / 2 - 88, C_TEXTO, a, 1);
    }

    // Pie: datos del juego + pistas.
    Velo(0, ALTO - 86, ANCHO, 86, 0, 0x120A24, 0, .82f);
    const Juego& j = juegos[vista[sel]];
    char buf[120];
    snprintf(buf, sizeof buf, "%s   ·   %d KB   ·   %s%s", j.mapeo, j.tamKB, j.pal ? "PAL" : "NTSC", j.tieneRam ? "   ·   guarda partida" : "");
    Texto(F_CUERPO_M, j.nombre, MARGEN, ALTO - 70, C_TEXTO);
    char pos[48];
    snprintf(pos, sizeof pos, "%d de %d   ·   ", sel + 1, nVista);
    float pw = AnchoTexto(F_PEQUENA, pos);
    Texto(F_PEQUENA, pos, MARGEN, ALTO - 38, C_ACENTO);
    Texto(F_PEQUENA, buf, MARGEN + pw, ALTO - 38, C_TEXTO_2);
    int b[4] = {BM_CRUZ, BM_OPTIONS, BM_TRIANGULO, BM_CUADRADO};
    const char* tx[4] = {"Jugar", EsFavorito(j.clave) ? "Quitar favorito" : "Favorito", "Buscar juegos", "Bajar carátulas"};
    if (tecladoVis < .5f) Pistas(b, tx, 4, 1 - tecladoVis * 2);   // con el teclado abierto, sus pistas reemplazan a estas
    DibujarBusqueda();
    TecladoDibujar(tecladoVis);
    DibujarDescarga(dt);
}
