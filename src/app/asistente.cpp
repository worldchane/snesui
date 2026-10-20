// Ver asistente.h. Pasos del asistente: bienvenida, el USB (juegos por consola), cómo jugar (desde el USB o copiando a
// la consola), el orden de las carpetas, las BIOS, las carátulas, música y estilo, y los controles. Se avanza con X o →,
// se vuelve con ○ o ←; ↑ ↓ eligen entre las opciones del paso.
#include "asistente.h"

#include <stdio.h>
#include <string.h>

#include "../comun/estilo.h"
#include "../comun/lienzo.h"
#include "../comun/musica.h"
#include "../comun/texto.h"
#include "ajustes.h"
#include "biblioteca.h"
#include "bios.h"
#include "consola.h"

enum { P_BIENVENIDA, P_USB, P_JUGAR, P_CARPETAS, P_BIOS, P_CARATULAS, P_ESTILO, P_CONTROLES, NUM_PASOS };
static bool abierto, novedades;
static int paso, opcion;
static float vis, pasoSuave;

static void RutaMarca(const char* archivo, char* r, int max) { snprintf(r, max, "%s/%s", CARPETA_DATOS, archivo); }

void AsistenteIniciar(const char* version)
{
    char r[160], antes[32] = "";
    RutaMarca("asistente.txt", r, sizeof r);
    bool visto = LeerTexto(r, antes, sizeof antes) > 0;
    char rv[160], vAntes[32] = "";
    RutaMarca("version.txt", rv, sizeof rv);
    LeerTexto(rv, vAntes, sizeof vAntes);
    char* fin = strchr(vAntes, '\n');
    if (fin) *fin = 0;
    // La primera vez: el asistente. Si ya lo vio y la versión cambió: las novedades.
    if (!visto) { abierto = true; paso = 0; }
    else if (strcmp(vAntes, version)) novedades = true;
    char t[48];
    snprintf(t, sizeof t, "%s\n", version);
    EscribirTexto(rv, t);
}

void AsistenteAbrir() { abierto = true; paso = 0; opcion = 0; vis = 0; }
bool AsistenteActivo() { return abierto || novedades; }

static void Terminar()
{
    abierto = false;
    char r[160];
    RutaMarca("asistente.txt", r, sizeof r);
    EscribirTexto(r, "visto\n");
}

// ---------- contenido de cada paso ----------
static const float PW = 1280, PH = 760;
static float px0() { return (ANCHO - PW) / 2; }
static const float PY = 150;

static void Titulo(const char* t, const char* sub)
{
    Texto(F_TITULO, t, px0() + 60, PY + 50, C_TEXTO);
    if (sub) Texto(F_CUERPO, sub, px0() + 60, PY + 116, C_TEXTO_2);
}

// Opciones elegibles (↑ ↓), una debajo de la otra.
static void Opciones(const char* const* textos, const char* const* ayudas, int n, float y)
{
    for (int i = 0; i < n; i++)
    {
        float fy = y + i * 104, x = px0() + 60, w = PW - 120;
        bool el = i == opcion;
        Caja(x, fy, w, 88, Radio(18), el ? C_ACENTO : 0xFFFFFF, el ? .22f : .05f);
        if (el) Borde(x, fy, w, 88, Radio(18), 2, C_ACENTO, .9f);
        Circulo(x + 40, fy + 44, 12, el ? C_ACENTO : C_TEXTO_3, 1);
        if (el) Circulo(x + 40, fy + 44, 6, C_TEXTO, 1);
        Texto(F_CUERPO_M, textos[i], x + 72, fy + 14, el ? C_TEXTO : C_TEXTO_2);
        if (ayudas && ayudas[i]) Texto(F_PEQUENA, ayudas[i], x + 72, fy + 52, C_TEXTO_3);
    }
}

static int OpcionesDe(int p) { return p == P_JUGAR ? 3 : p == P_CARATULAS ? 2 : p == P_ESTILO ? 2 : 0; }

// Al confirmar un paso con opciones: aplica lo elegido.
static void Aplicar(int p)
{
    AjustesGenerales& g = General();
    if (p == P_JUGAR) { if (opcion == 1) g.copiarAuto = 1; else if (opcion == 0) g.copiarAuto = 0; }
    if (p == P_CARATULAS) g.caratulasAuto = opcion == 0;
    if (p == P_ESTILO) { g.musica = opcion == 0; MusicaEncender(g.musica != 0, (unsigned)(Segundos() * 1000)); }
    AjustesGuardar();
}

// La opción que corresponde a lo que ya está configurado (al entrar a un paso).
static int OpcionActual(int p)
{
    const AjustesGenerales& g = General();
    if (p == P_JUGAR) return g.copiarAuto ? 1 : 0;
    if (p == P_CARATULAS) return g.caratulasAuto ? 0 : 1;
    if (p == P_ESTILO) return g.musica ? 0 : 1;
    return 0;
}

static void PasoUsb()
{
    int n = 0, hallados = 0;
    const Juego* j = BibliotecaJuegos(&n);
    bool buscando = BibliotecaBuscando(&hallados);
    int porConsola[NUM_SISTEMAS] = {}, enUsb = 0;
    // Todos los del USB (también los que ya tienen una copia en la consola: siguen estando en el USB).
    for (int i = 0; i < n; i++) if (j[i].enUsb) { porConsola[EsArcade(j[i].sistema) ? SIS_ARCADE : j[i].sistema]++; enUsb++; }
    char t[160];
    if (buscando) snprintf(t, sizeof t, "Buscando juegos en el USB…  %d encontrados", hallados);
    else if (enUsb) snprintf(t, sizeof t, enUsb == 1 ? "Encontré 1 juego en el USB:" : "Encontré %d juegos en el USB:", enUsb);
    else snprintf(t, sizeof t, "No encontré juegos en el USB.");
    Titulo("Tu USB", t);
    if (!enUsb && !buscando)
    {
        Parrafo(F_CUERPO, "Conecta un USB (formato exFAT o FAT32) con tus juegos. Pueden estar en cualquier carpeta, pero lo más ordenado es "
                          "una carpeta roms con una carpeta por consola (en el paso siguiente te muestro cómo). Cuando lo conectes, pulsa "
                          "triángulo en la biblioteca para buscar.", px0() + 60, PY + 190, PW - 120, C_TEXTO_2);
        return;
    }
    // Una tarjeta por consola con juegos, en filas de 4.
    int k = 0;
    for (int s = 0; s < NUM_SISTEMAS; s++)
    {
        if (!porConsola[s]) continue;
        float x = px0() + 60 + (k % 4) * 296, y = PY + 190 + (k / 4) * 120;
        if (y > PY + PH - 140) break;
        Caja(x, y, 276, 100, Radio(16), 0xFFFFFF, .06f);
        Texto(F_SUBTITULO, ConsolaDe(s).nombre, x + 24, y + 14, C_TEXTO);
        snprintf(t, sizeof t, porConsola[s] == 1 ? "1 juego" : "%d juegos", porConsola[s]);
        Texto(F_PEQUENA, t, x + 24, y + 62, C_ACENTO_2);
        k++;
    }
}

static void PasoCarpetas()
{
    Titulo("Orden de las carpetas", "Así encuentro todo más rápido (igual que en Batocera y otros emuladores):");
    static const char* const LINEAS[] = {
        "USB/",
        "    roms/",
        "        snes/   nes/   gb/   gbc/   gba/   megadrive/   mastersystem/   gamegear/",
        "        ngp/   ngpc/   atari7800/   lynx/   arcade/   neogeo/   cps1/   cps2/   cps3/",
        "    bios/           tus BIOS, con cualquier nombre (las reconozco por su contenido)",
        "    caratulas/      opcional: carátulas propias en caratulas/<consola>/<juego>.png",
    };
    for (int i = 0; i < 6; i++) Texto(F_CUERPO_M, LINEAS[i], px0() + 80, PY + 200 + i * 52, i < 4 ? C_TEXTO : C_TEXTO_2);
    // ¿Ya está ordenado? (cuántos de los juegos del USB están en una carpeta que dice su consola)
    int n = 0, ordenados = 0, total = 0;
    const Juego* j = BibliotecaJuegos(&n);
    for (int i = 0; i < n; i++)
    {
        if (!j[i].enUsb) continue;
        total++;
        char carpeta[512];
        snprintf(carpeta, sizeof carpeta, "%s", j[i].ruta);
        char* b = strrchr(carpeta, '/');
        if (b) *b = 0;
        int s = ConsolaPorCarpeta(carpeta);
        if (s == j[i].sistema || (EsArcade(s) && EsArcade(j[i].sistema))) ordenados++;
    }
    const char* msj = !total ? "Cuando conectes un USB con juegos te digo si ya está ordenado."
                    : ordenados * 10 >= total * 9 ? "Tu USB ya está ordenado así. ¡Perfecto!"
                                                  : "Tu USB no sigue este orden, pero igual encuentro tus juegos: ordenarlo es opcional.";
    Texto(F_CUERPO, msj, px0() + 60, PY + PH - 120, ordenados * 10 >= total * 9 && total ? C_BIEN : C_TEXTO_2);
}

static void PasoBios()
{
    Titulo("BIOS", "Algunas consolas las necesitan o funcionan mejor con ellas:");
    BiosRevisar();
    int n = BiosCuantas();
    for (int i = 0; i < n; i++)
    {
        const BiosConocida& b = BiosDe(i);
        int e = BiosEstado(i);
        float x = px0() + 60 + (i % 2) * 590, y = PY + 180 + (i / 2) * 50;
        uint32_t c = e == BIOS_BIEN ? C_BIEN : b.necesaria ? C_MAL : C_TEXTO_3;
        Circulo(x + 12, y + 16, 8, c, 1);
        Texto(F_CUERPO, b.descripcion, x + 34, y, e == BIOS_BIEN ? C_TEXTO : C_TEXTO_2);
    }
    Texto(F_PEQUENA, "Ponlas en la carpeta bios del USB (sueltas o en un zip, con cualquier nombre): las copio solas a la consola al buscar juegos.",
          px0() + 60, PY + PH - 90, C_TEXTO_3);
}

static void PasoControles()
{
    Titulo("Controles", "En la biblioteca:");
    struct { int b; const char* t; } F[] = {
        {BM_CRUZ, "Jugar"}, {BM_CIRCULO, "Estilo de los botones (o quitar la búsqueda)"}, {BM_CUADRADO, "Agregar o quitar de favoritos"},
        {BM_TRIANGULO, "Buscar juegos en el USB"}, {BM_L1, "Pestaña anterior (consolas, favoritos...)"}, {BM_R1, "Pestaña siguiente"},
        {BM_OPTIONS, "Opciones del juego (copiar, borrar, carátulas)"}, {BM_TACTIL, "Buscar por nombre"},
    };
    for (int i = 0; i < 8; i++)
    {
        float x = px0() + 60 + (i % 2) * 590, y = PY + 190 + (i / 2) * 70;
        DibujarBoton(F[i].b, x + 40, y + 18);
        Texto(F_CUERPO, F[i].t, x + 90, y + 4, C_TEXTO_2);
    }
    Texto(F_PEQUENA, "L2 y R2 cambian el estilo de la interfaz y del menú.   Jugando: L3 + R3 abre el menú; R2 acelera y L2 rebobina.",
          px0() + 60, PY + PH - 110, C_TEXTO_3);
}

static void DibujarPaso(int p)
{
    switch (p)
    {
    case P_BIENVENIDA:
        Titulo("¡Bienvenido a ALLESIU!", "Emulador de consolas clásicas para PS4. Te ayudo a dejarlo listo en un minuto.");
        Parrafo(F_CUERPO, "Juega SNES, NES, Game Boy, Game Boy Color, Game Boy Advance, Mega Drive, Master System, Game Gear, Neo Geo Pocket, "
                          "Atari 7800, Lynx y arcade (Neo Geo y CPS). Tus partidas se guardan solas en la consola.",
                px0() + 60, PY + 200, PW - 120, C_TEXTO_2);
        Texto(F_CUERPO, "Idioma: Español", px0() + 60, PY + 400, C_TEXTO);
        Texto(F_PEQUENA, "Puedes volver a abrir este asistente cuando quieras desde Configuración.", px0() + 60, PY + PH - 100, C_TEXTO_3);
        break;
    case P_USB: PasoUsb(); break;
    case P_JUGAR:
    {
        Titulo("¿Cómo quieres jugar?", "Lo puedes cambiar después en Configuración.");
        static const char* const T[3] = {"Jugar desde el USB", "Copiar los juegos a la consola", "Decidir después"};
        static const char* const A[3] = {"Los juegos quedan en el USB: no ocupan espacio en la consola, pero el USB tiene que estar conectado.",
                                         "Se copian solos a la consola (si hay espacio): después puedes jugar sin el USB.",
                                         "Puedes copiar juego por juego con OPTIONS."};
        Opciones(T, A, 3, PY + 200);
        break;
    }
    case P_CARPETAS: PasoCarpetas(); break;
    case P_BIOS: PasoBios(); break;
    case P_CARATULAS:
    {
        Titulo("Carátulas", "Si tienes carátulas en el USB (de RetroArch, ES-DE o Batocera) las copio solas.");
        static const char* const T[2] = {"Bajar las que falten de internet", "No bajar de internet"};
        static const char* const A[2] = {"Si la consola tiene internet, después de cada búsqueda (base de carátulas de RetroArch).",
                                         "Solo las del USB. Puedes bajarlas cuando quieras con OPTIONS."};
        Opciones(T, A, 2, PY + 200);
        break;
    }
    case P_ESTILO:
    {
        Titulo("Música y estilo", "En la biblioteca, L2 cambia la interfaz (20 estilos) y R2 el menú (12 formas).");
        static const char* const T[2] = {"Con música en la biblioteca", "Sin música"};
        static const char* const A[2] = {"Diez canciones originales de ALLESIU, en orden aleatorio. Se apaga sola mientras juegas.", NULL};
        Opciones(T, A, 2, PY + 200);
        break;
    }
    case P_CONTROLES: PasoControles(); break;
    }
}

// ---------- novedades ----------
static void DibujarNovedades(float a)
{
    Velo(0, 0, ANCHO, ALTO, 0, 0x05030C, .55f * a, .55f * a);
    float x = px0(), y = PY;
    Panel(x, y, PW, PH, 30, a);
    Texto(F_TITULO, "Novedades", x + 60, y + 50, C_TEXTO, a);
    static const char* const N[] = {
        "Estilos: 21 interfaces (L2), 12 formas de menú (R2) y 50 estilos de botones (círculo).",
        "Cuadrado agrega o quita favoritos directo. Las carátulas se bajan solas si hay internet.",
        "BIOS reconocidas por su contenido, con cualquier nombre y en cualquier carpeta del USB.",
        "Carátulas propias desde el USB (RetroArch, ES-DE, Batocera) y descripciones de tus gamelist.xml.",
        "Carpetas ordenadas: roms/<consola> en la consola y en el USB (los juegos copiados se ordenaron solos).",
        "Arcade: más juegos reconocidos y CPS3 abre (mejor uso de la memoria). SNES con Snes9x.",
    };
    for (int i = 0; i < 6; i++)
    {
        Circulo(x + 76, y + 186 + i * 84, 7, C_ACENTO, a);
        Parrafo(F_CUERPO, N[i], x + 100, y + 168 + i * 84, PW - 160, C_TEXTO_2, a);
    }
    static const int b[1] = {BM_CRUZ};
    static const char* const t[1] = {"Continuar"};
    PistasEn(b, t, 1, a, y + PH - 50, x + PW - 60);   // dentro del panel
}

bool AsistenteCuadro(float dt, const Mando& m)
{
    if (novedades)
    {
        Suave(vis, 1, 10, dt);
        DibujarNovedades(vis);
        if (m.pulsados & (B_CRUZ | B_CIRCULO)) { novedades = false; vis = 0; Sonar(SON_ACEPTAR); }
        return true;
    }
    if (!abierto) return false;
    Suave(vis, 1, 10, dt);
    int n = OpcionesDe(paso);
    if (n && (m.pulsados & B_ARRIBA)) { opcion = (opcion + n - 1) % n; Sonar(SON_MOVER); }
    if (n && (m.pulsados & B_ABAJO)) { opcion = (opcion + 1) % n; Sonar(SON_MOVER); }
    if (m.pulsados & (B_CRUZ | B_DER))
    {
        if (n) Aplicar(paso);
        if (paso == NUM_PASOS - 1) { Terminar(); Sonar(SON_ACEPTAR); return true; }
        paso++;
        opcion = OpcionActual(paso);
        Sonar(SON_ACEPTAR);
    }
    else if ((m.pulsados & (B_CIRCULO | B_IZQ)) && paso > 0) { paso--; opcion = OpcionActual(paso); Sonar(SON_ATRAS); }
    Suave(pasoSuave, (float)paso, 12, dt);
    float a = vis;
    Velo(0, 0, ANCHO, ALTO, 0, 0x05030C, .45f * a, .45f * a);
    Panel(px0(), PY, PW, PH, 30, a);
    Opacidad(a);
    DibujarPaso(paso);
    // Puntos de avance abajo y las pistas.
    for (int i = 0; i < NUM_PASOS; i++)
        Circulo(ANCHO / 2 - (NUM_PASOS - 1) * 14 + i * 28, PY + PH - 34, i == paso ? 7 : 5, i == paso ? C_ACENTO : C_TEXTO_3, 1);
    int b[2] = {BM_CIRCULO, BM_CRUZ};
    const char* t[2] = {"Atrás", paso == NUM_PASOS - 1 ? "Empezar" : "Siguiente"};
    PistasEn(paso ? b : b + 1, paso ? t : t + 1, paso ? 2 : 1, 1, ALTO - 60);
    Opacidad(1);
    return true;
}
