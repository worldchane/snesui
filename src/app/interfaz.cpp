// La interfaz del emulador de SNES, con estilo SNES: fondo morado, los cuatro colores de los botones como
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
#include "copiar.h"
#include "fondo.h"
#include "registro.h"
#include "juego.h"

static const char* const VERSION = "1.2";
// Número de compilación (lo genera construir.sh): commit y fecha; en el simulador de PC, "pc".
#ifdef CON_COMPILACION
#include "compilacion.h"
#else
#define COMPILACION "pc"
#endif
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
                if (!juegos[i].copiado && !strcmp(juegos[i].clave, recientes[r])) { if (Coincide(juegos[i].nombre)) vista[nVista++] = i; break; }
    }
    else
        for (int i = 0; i < nJuegos; i++)
            if (!juegos[i].copiado && (vistaActual == V_TODOS || EsFavorito(juegos[i].clave)) && Coincide(juegos[i].nombre))
                vista[nVista++] = i;
    if (sel >= nVista) sel = nVista > 0 ? nVista - 1 : 0;
}

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
    ArmarVista();
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
        Texto(F_ETIQUETA, Acortar(F_ETIQUETA, j.nombre, w - 48, buf, sizeof buf), X + 26, Y + h - 84, C_TEXTO);
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
    // Dónde está: ícono de USB (ámbar) o de disco duro de la consola (turquesa), abajo a la izquierda.
    Caja(X + 12, Y + h - 46, 62, 38, 19, 0x0A0614, .85f);
    IconoLugar(j.enUsb, X + 43, Y + h - 27, 1.15f, 1);
    // Aro de selección en morado SNES.
    if (f > .01f) Borde(X - 6, Y - 6, w + 12, h + 12, 26, 4, C_ACENTO, .95f * f);
}

static void Biblioteca(float t, float dt, const Mando& m);
static void DibujarCopia(float dt);
static void VentanaDibujar(float dt);

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
    RegistroCuadro();
    // Si hay un juego abierto, solo la pantalla de juego (sin gastar en el fondo ni en la biblioteca, que no se ven).
    if (JuegoActivo()) { JuegoCuadro(t, dt, m); return; }
    if (Presentacion(dt, m)) { entrada = 0; return; }
    // Después del logo, la biblioteca aparece desde negro.
    Suave(entrada, 1, 5, dt);
    Biblioteca(t, dt, entrada > .98f ? m : Mando{});
    DibujarCopia(dt);
    VentanaDibujar(dt);
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
        if (juegos[i].enUsb && !juegos[i].copiado)
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
        if (!juegos[i].enUsb && !juegos[i].tambienEnUsb)
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
}

static void CopiarPendientes()
{
    static const char* lista[MAX_JUEGOS];
    int n = PendientesUsb(lista, NULL);
    if (n && EmpezarCopia(lista, n, CARPETA_JUEGOS_CONSOLA, RESERVA_SISTEMA)) copiaVista = false;
}

// Lo que se puede hacer con el juego elegido, según dónde está. Copiar solo ese juego va primero.
enum { JA_A_CONSOLA, JA_A_USB, JA_BORRAR, JA_FAVORITO, JA_ADMINISTRAR, JA_CANCELAR };
static int AccionesJuego(int* ac)
{
    int n = 0;
    if (elegido.enUsb) ac[n++] = JA_A_CONSOLA;
    else
    {
        if (!elegido.otraEnUsb) ac[n++] = JA_A_USB;   // si ya está también en el USB, no hace falta
        ac[n++] = JA_BORRAR;
    }
    ac[n++] = JA_FAVORITO;
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
        case JA_FAVORITO:
            CambiarFavorito(elegido.clave);
            if (vistaActual == V_FAVORITOS) ArmarVista();
            Sonar(SON_ACEPTAR);
            ventana = VN_NADA;
            break;
        case JA_A_CONSOLA:
            if (EmpezarCopia(r, 1, CARPETA_JUEGOS_CONSOLA, RESERVA_SISTEMA)) { copiaVista = false; Sonar(SON_ACEPTAR); }
            else { Avisar("Ya hay una copia en curso: espera a que termine.", C_AVISO); Sonar(SON_ERROR); }
            ventana = VN_NADA;
            break;
        case JA_A_USB:
        {
            char usb[64];
            if (CopiaEnCurso()) { Avisar("Ya hay una copia en curso: espera a que termine.", C_AVISO); Sonar(SON_ERROR); }
            else if (!CarpetaUsb(usb, sizeof usb)) { Avisar("No hay un USB conectado.", C_MAL); Sonar(SON_ERROR); }
            else if (EmpezarCopia(r, 1, usb, RESERVA_USB)) { copiaVista = false; Sonar(SON_ACEPTAR); }
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
    Sombra(x, y, w, h, 28, 50, .6f * a);
    Caja(x, y, w, h, 28, 0x1A1033, .98f * a);
    Borde(x, y, w, h, 28, 2, C_ACENTO, .5f * a);
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
            case JA_FAVORITO: op[i] = EsFavorito(elegido.clave) ? "Quitar de favoritos" : "Agregar a favoritos"; break;
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
                                     "SNESUI siempre deja 1 GB libre para el sistema."
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
        float w = 720, h = 76, x = (ANCHO - w) / 2, y = d.activa ? 130 : 40;
        Sombra(x, y, w, h, 22, 24, .45f);
        Caja(x, y, w, h, 22, C_PANEL, .96f);
        char t[160], a1[32], a2[32];
        TextoTamano(p.bytesHechos, a1, sizeof a1);
        TextoTamano(p.bytesTotal, a2, sizeof a2);
        if (p.borrando) snprintf(t, sizeof t, "Borrando juegos %s   %d de %d", p.alUsb ? "del USB" : "de la consola", p.hechos, p.total);
        else snprintf(t, sizeof t, "Copiando %s   %d de %d   ·   %s de %s", p.alUsb ? "al USB" : "a la consola",
                      p.hechos + 1 > p.total ? p.total : p.hechos + 1, p.total, a1, a2);
        Texto(F_CUERPO_M, t, x + 28, y + 12, C_TEXTO, 1);
        char corto[120];
        Texto(F_PEQUENA, Acortar(F_PEQUENA, p.actual, w - 56, corto, sizeof corto), x + 28, y + 44, C_TEXTO_3, 1);
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
        Sombra(x, y, w, 56, 28, 20, .4f * a);
        Caja(x, y, w, 56, 28, C_PANEL, .96f * a);
        Circulo(x + 30, y + 28, 7, avisoColor, a);
        Texto(F_CUERPO_M, aviso, x + 50, y + 14, C_TEXTO, a);
    }
}

// ---------- pestaña de configuración (vale para todos los juegos) ----------
enum { CF_PROPORCION, CF_FILTRO, CF_AVANCE, CF_REBOBINAR, CF_SUPERFX, CF_COPIAR_AUTO, CF_ADMINISTRAR, CF_ACERCA, NUM_CF };
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
    if (selConfig == CF_ADMINISTRAR && (m.pulsados & (B_CRUZ | B_DER))) { ventana = VN_ADMINISTRAR; opVentana = 0; tMedida = 99; Sonar(SON_ACEPTAR); return; }
    if (m.pulsados & B_ARRIBA) { selConfig = (selConfig + NUM_CF - 1) % NUM_CF; Sonar(SON_MOVER); }
    if (m.pulsados & B_ABAJO) { selConfig = (selConfig + 1) % NUM_CF; Sonar(SON_MOVER); }
    int d = (m.pulsados & (B_DER | B_CRUZ)) ? 1 : (m.pulsados & B_IZQ) ? -1 : 0;
    if (d && selConfig != CF_ACERCA && selConfig != CF_ADMINISTRAR)
    {
        switch (selConfig)
        {
        case CF_PROPORCION: g.proporcion = (g.proporcion + NUM_PROPORCIONES + d) % NUM_PROPORCIONES; break;
        case CF_FILTRO: g.filtro = (g.filtro + NUM_FILTROS + d) % NUM_FILTROS; break;
        case CF_AVANCE: g.avance = 2 + (g.avance - 2 + 3 + d) % 3; break;
        case CF_REBOBINAR: g.rebobinar = !g.rebobinar; break;
        case CF_SUPERFX: g.superfx = 1 + (g.superfx - 1 + 4 + d) % 4; break;
        case CF_COPIAR_AUTO: g.copiarAuto = !g.copiarAuto; break;
        }
        AjustesGuardar();
        Sonar(SON_MOVER);
    }
    Suave(selConfigSuave, (float)selConfig, 18, dt);

    float w = 1180, x = (ANCHO - w) / 2, y = 200, paso = 84, h = 124 + (float)NUM_CF * paso;
    Sombra(x, y, w, h, 28, 40, .5f, true);
    Caja(x, y, w, h, 28, C_PANEL, .92f);
    Texto(F_SUBTITULO, "Configuración general", x + 48, y + 36, C_TEXTO);
    Texto(F_PEQUENA, "Vale para todos los juegos. Cada juego puede tener su propia proporción y filtro desde el menú de pausa (L3 + R3).",
          x + 48, y + 86, C_TEXTO_3);
    float y0 = y + 124;
    Caja(x + 24, y0 + selConfigSuave * paso - 14, w - 48, paso - 12, 18, C_ACENTO, .2f);
    Borde(x + 24, y0 + selConfigSuave * paso - 14, w - 48, paso - 12, 18, 2, C_ACENTO, .7f);
    static const char* const TITULOS[NUM_CF] = {"Proporción de la imagen", "Filtro", "Avance rápido (mantener R2)", "Rebobinar (mantener L2)",
                                                "Velocidad del Super FX", "Copiar juegos nuevos del USB automáticamente", "Copiar o borrar todos los juegos", "Acerca de SNESUI"};
    static const char* const AYUDAS[NUM_CF] = {
        "Cómo se ajusta la imagen a la pantalla.",
        "Suave, nítido o el aspecto de un televisor antiguo.",
        "Cuántas veces más rápido corre el juego.",
        "Guarda los últimos segundos para volver atrás. Desactivado, la consola trabaja menos.",
        "Más rápido: los juegos 3D con este chip se ven más fluidos (la consola original va a ×1).",
        "Los juegos que aparezcan en el USB se copian solos a la consola, si hay espacio.",
        NULL,   // el espacio libre (se calcula)
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
        case CF_SUPERFX: snprintf(v, sizeof v, g.superfx == 1 ? "« ×1 (original) »" : "« ×%d »", g.superfx); break;
        case CF_COPIAR_AUTO: snprintf(v, sizeof v, "« %s »", g.copiarAuto ? "Sí" : "No"); break;
        default: snprintf(v, sizeof v, "Ver  »"); break;
        }
        bool elegida = i == selConfig;
        char libre[96];
        const char* ayuda = AYUDAS[i];
        if (!ayuda) { TextoLibre(libre, sizeof libre); ayuda = libre; }
        Texto(F_CUERPO_M, TITULOS[i], x + 56, fy + 2, elegida ? C_TEXTO : C_TEXTO_2);
        Texto(F_PEQUENA, ayuda, x + 56, fy + 38, C_TEXTO_3);
        Texto(F_CUERPO_M, v, x + w - 56, fy + 12, elegida ? C_ACENTO : C_TEXTO_2, 1, 2);
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
    char v[128];
    snprintf(v, sizeof v, "Emulador de SNES para PS4   ·   versión %s   ·   compilación %s", VERSION, COMPILACION);
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
    // Después de copiar o borrar: buscar de nuevo apenas termine la búsqueda en curso.
    if (releer && !BibliotecaBuscando(NULL)) { releer = false; BibliotecaBuscar(); }
    if (!BibliotecaNueva()) return;
    char elegidoClave[96] = "";
    if (nVista > 0 && sel < nVista) snprintf(elegidoClave, sizeof elegidoClave, "%s", juegos[vista[sel]].clave);
    juegos = BibliotecaJuegos(&nJuegos);
    ArmarVista();
    for (int i = 0; i < nVista; i++)
        if (!strcmp(juegos[vista[i]].clave, elegidoClave)) { sel = i; break; }
    // Copia automática: los juegos nuevos del USB pasan a la consola (si hay espacio; el hilo de copia lo verifica).
    if (General().copiarAuto && !CopiaEnCurso()) CopiarPendientes();
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
    char cont[48];
    int halladas = 0;
    if (BibliotecaBuscando(&halladas)) snprintf(cont, sizeof cont, "Buscando juegos…  %d", halladas);
    else
    {
        // Los que están en el USB y también en la consola cuentan una sola vez.
        int distintos = 0;
        for (int i = 0; i < nJuegos; i++) if (!juegos[i].copiado) distintos++;
        snprintf(cont, sizeof cont, distintos == 1 ? "%d juego" : "%d juegos", distintos);
    }
    Texto(F_CUERPO, cont, ANCHO - MARGEN, 100, C_TEXTO_2, 1, 2);


    // El teclado en pantalla y las ventanas se llevan los botones mientras están abiertos.
    Suave(tecladoVis, teclado ? 1 : 0, 14, dt);
    MedirEspacio(dt);
    static const Mando nada = {};
    const Mando& mb = teclado || ventana != VN_NADA ? nada : m;
    if (ventana != VN_NADA) VentanaEntrada(m);
    else if (teclado) TecladoEntrada(m);

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
    if ((mb.pulsados & B_OPTIONS) && nVista > 0) { AbrirMenuJuego(juegos[vista[sel]]); Sonar(SON_SECCION); }
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
    snprintf(buf, sizeof buf, "       %s   ·   %s   ·   %d KB   ·   %s%s", j.enUsb ? "En el USB" : "En la consola", j.mapeo, j.tamKB,
             j.pal ? "PAL" : "NTSC", j.tieneRam ? "   ·   guarda partida" : "");
    Texto(F_CUERPO_M, j.nombre, MARGEN, ALTO - 70, C_TEXTO);
    char pos[48];
    snprintf(pos, sizeof pos, "%d de %d   ·   ", sel + 1, nVista);
    float pw = AnchoTexto(F_PEQUENA, pos);
    Texto(F_PEQUENA, pos, MARGEN, ALTO - 38, C_ACENTO);
    Texto(F_PEQUENA, buf, MARGEN + pw, ALTO - 38, C_TEXTO_2);
    IconoLugar(j.enUsb, MARGEN + pw + 16, ALTO - 26, .6f, 1);
    int b[4] = {BM_CRUZ, BM_OPTIONS, BM_TRIANGULO, BM_CUADRADO};
    const char* tx[4] = {"Jugar", "Opciones", "Buscar juegos", "Bajar carátulas"};
    // Con el teclado o una ventana abiertos, sus pistas reemplazan a estas.
    float tapa = tecladoVis > ventanaVis ? tecladoVis : ventanaVis;
    if (tapa < .5f) Pistas(b, tx, 4, 1 - tapa * 2);
    DibujarBusqueda();
    TecladoDibujar(tecladoVis);
    DibujarDescarga(dt);
}
