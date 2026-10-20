// Carátulas que el usuario ya tiene en el USB: se copian a la consola (caratulas/<consola>/) para los juegos de la
// biblioteca que no tengan una. Se reconocen las carpetas que arman otros programas, además de la propia:
//   caratulas/<consola>/<juego>.png          la de ALLESIU (también covers/ o boxart/)
//   thumbnails/<Sistema>/Named_Boxarts/       RetroArch ("Nintendo - Super Nintendo Entertainment System")
//   downloaded_media/<consola>/covers/        ES-DE
//   roms/<consola>/images/<juego>-thumb.png   Batocera (también -image, -boxart)
// El nombre se compara solo por letras y números (RetroArch cambia & * / : por _, y cada uno escribe distinto los
// espacios y signos), con el nombre del archivo del juego o con el que muestra la biblioteca.
#include "caratula.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../comun/plataforma.h"
#include "biblioteca.h"
#include "consola.h"
#include "registro.h"

struct Imagen { char clave[96]; int sistema; char ruta[512]; };
static Imagen* imagenes;
static int nImagenes, capImagenes;

static void Normalizar(const char* s, size_t largo, char* sale, int max)
{
    int n = 0;
    for (size_t i = 0; i < largo && s[i] && n < max - 1; i++)
    {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c)) sale[n++] = (char)tolower(c);
    }
    sale[n] = 0;
}

static bool EsImagen(const char* nombre)
{
    const char* e = strrchr(nombre, '.');
    return e && (!strcasecmp(e, ".png") || !strcasecmp(e, ".jpg") || !strcasecmp(e, ".jpeg"));
}

// La consola de una carpeta de RetroArch por el nombre de su repositorio de carátulas ("Nintendo - Game Boy").
static int ConsolaDeThumbnails(const char* carpeta)
{
    char a[128], b[128];
    Normalizar(carpeta, strlen(carpeta), a, sizeof a);
    for (int s = 0; s < NUM_SISTEMAS; s++)
    {
        Normalizar(ConsolaDe(s).caratulas, strlen(ConsolaDe(s).caratulas), b, sizeof b);
        if (!strcmp(a, b) && !EsArcade(s)) return s;
    }
    return !strcmp(a, "fbnearcadegames") || !strcmp(a, "mame") ? SIS_ARCADE : -1;
}

static void Agregar(const char* carpeta, const char* nombre, int sistema)
{
    if (!EsImagen(nombre)) return;
    // Sin la extensión ni los agregados de Batocera; las que no son la caja se saltan.
    const char* punto = strrchr(nombre, '.');
    size_t largo = (size_t)(punto - nombre);
    static const char* const OTRAS[] = {"-marquee", "-fanart", "-manual", "-video", "-wheel", "-titleshot", "-map", "-bezel"};
    for (const char* o : OTRAS)
    {
        size_t lo = strlen(o);
        if (largo > lo && !strncasecmp(nombre + largo - lo, o, lo)) return;
    }
    static const char* const CAJA[] = {"-thumb", "-image", "-boxart", "-box"};
    for (const char* c : CAJA)
    {
        size_t lc = strlen(c);
        if (largo > lc && !strncasecmp(nombre + largo - lc, c, lc)) { largo -= lc; break; }
    }
    if (nImagenes == capImagenes)
    {
        int cap = capImagenes ? capImagenes * 2 : 1024;
        Imagen* v = (Imagen*)realloc(imagenes, sizeof(Imagen) * cap);
        if (!v) return;
        imagenes = v;
        capImagenes = cap;
    }
    Imagen& im = imagenes[nImagenes];
    Normalizar(nombre, largo, im.clave, sizeof im.clave);
    if (!im.clave[0]) return;
    im.sistema = sistema;
    snprintf(im.ruta, sizeof im.ruta, "%s/%s", carpeta, nombre);
    nImagenes++;
}

// Recorre 'carpeta' (y sus subcarpetas, hasta 'prof' niveles) agregando las imágenes. La consola sale del nombre de la
// carpeta (o la de RetroArch, si 'retroarch').
struct Recorrido { char carpeta[400]; int prof, sistema; bool retroarch; };
static void AlListar(const char* nombre, bool esCarpeta, void* u)
{
    Recorrido& r = *(Recorrido*)u;
    if (!esCarpeta) { Agregar(r.carpeta, nombre, r.sistema); return; }
    if (r.prof <= 0) return;
    Recorrido sub = r;
    snprintf(sub.carpeta, sizeof sub.carpeta, "%s/%s", r.carpeta, nombre);
    sub.prof--;
    if (r.retroarch)
    {
        // thumbnails/<Sistema>/Named_Boxarts: solo las cajas (no las capturas ni las pantallas de título).
        int s = ConsolaDeThumbnails(nombre);
        if (s >= 0) sub.sistema = s;
        else if (strcasecmp(nombre, "Named_Boxarts")) return;
    }
    else
    {
        int s = ConsolaPorCarpeta(nombre);
        if (s >= 0) sub.sistema = s;
    }
    ListarCarpeta(sub.carpeta, AlListar, &sub);
}

static void Recorrer(const char* carpeta, int prof, int sistema, bool retroarch)
{
    static Recorrido r;
    memset(&r, 0, sizeof r);
    snprintf(r.carpeta, sizeof r.carpeta, "%s", carpeta);
    r.prof = prof;
    r.sistema = sistema;
    r.retroarch = retroarch;
    ListarCarpeta(r.carpeta, AlListar, &r);
}

// ES-DE: downloaded_media/<consola>/covers. Batocera: roms/<consola>/images.
struct Raiz { char carpeta[300]; const char* sub; };
static void AlListarRaiz(const char* nombre, bool esCarpeta, void* u)
{
    Raiz& r = *(Raiz*)u;
    int s = esCarpeta ? ConsolaPorCarpeta(nombre) : -1;
    if (s < 0) return;
    char c[600];
    snprintf(c, sizeof c, "%s/%s/%s", r.carpeta, nombre, r.sub);
    Recorrer(c, 0, s, false);
}

static int PorClave(const void* a, const void* b) { return strcmp(((const Imagen*)a)->clave, ((const Imagen*)b)->clave); }

static const Imagen* Buscar(const char* clave, int sistema)
{
    int a = 0, b = nImagenes;
    while (a < b) { int m = (a + b) / 2; if (strcmp(imagenes[m].clave, clave) < 0) a = m + 1; else b = m; }
    const Imagen* sinConsola = NULL;
    for (int i = a; i < nImagenes && !strcmp(imagenes[i].clave, clave); i++)
    {
        if (imagenes[i].sistema == sistema || (EsArcade(sistema) && EsArcade(imagenes[i].sistema))) return &imagenes[i];
        if (imagenes[i].sistema < 0 && !sinConsola) sinConsola = &imagenes[i];
    }
    return sinConsola;
}

static bool Copiar(const char* de, const char* a)
{
    // Hasta 4 MB (lo que lee el cargador de carátulas).
    uint64_t t = 0;
    if (!ExisteArchivo(de, &t) || !t || t > 4u << 20) return false;
    FILE* f = AbrirArchivo(de, "rb");
    if (!f) return false;
    uint8_t* d = (uint8_t*)malloc((size_t)t);
    bool ok = d && fread(d, 1, (size_t)t, f) == t;
    fclose(f);
    FILE* g = ok ? AbrirArchivo(a, "wb") : NULL;
    ok = g && fwrite(d, 1, (size_t)t, g) == t;
    if (g) fclose(g);
    if (!ok && g) BorrarArchivo(a);
    free(d);
    return ok;
}

int CaratulasDelUsb(const Juego* juegos, int n)
{
    // ¿Falta alguna? (si no, ni se recorre el USB).
    int faltan = 0;
    for (int i = 0; i < n && !faltan; i++) faltan += !HayCaratula(juegos[i].clave);
    if (!faltan) return 0;
    nImagenes = 0;
    static const char* const USB[] = {"/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3"};
    for (const char* u : USB)
    {
        char c[300];
        static const char* const PROPIAS[] = {"caratulas", "Caratulas", "carátulas", "covers", "Covers", "boxart", "Boxart"};
        for (const char* p : PROPIAS) { snprintf(c, sizeof c, "%s/%s", u, p); Recorrer(c, 2, -1, false); }
        snprintf(c, sizeof c, "%s/thumbnails", u);
        Recorrer(c, 2, -1, true);
        static Raiz r;
        snprintf(r.carpeta, sizeof r.carpeta, "%s/downloaded_media", u);
        r.sub = "covers";
        ListarCarpeta(r.carpeta, AlListarRaiz, &r);
        snprintf(r.carpeta, sizeof r.carpeta, "%s/roms", u);
        r.sub = "images";
        ListarCarpeta(r.carpeta, AlListarRaiz, &r);
    }
    if (!nImagenes) return 0;
    qsort(imagenes, nImagenes, sizeof(Imagen), PorClave);
    int copiadas = 0;
    for (int i = 0; i < n; i++)
    {
        const Juego& j = juegos[i];
        if (HayCaratula(j.clave)) continue;
        // Primero por el nombre del archivo; si no, por el nombre que muestra la biblioteca.
        char clave[96];
        const char* punto = strrchr(j.archivo, '.');
        Normalizar(j.archivo, punto ? (size_t)(punto - j.archivo) : strlen(j.archivo), clave, sizeof clave);
        const Imagen* im = Buscar(clave, j.sistema);
        if (!im)
        {
            Normalizar(j.nombre, strlen(j.nombre), clave, sizeof clave);
            im = Buscar(clave, j.sistema);
        }
        if (!im) continue;
        char destino[600];
        CaratulaRuta(j.clave, destino, sizeof destino);
        char carpeta[600];
        snprintf(carpeta, sizeof carpeta, "%s", destino);
        char* barra = strrchr(carpeta, '/');
        if (barra) { *barra = 0; CrearCarpetas(carpeta); }
        if (Copiar(im->ruta, destino)) copiadas++;
    }
    free(imagenes);
    imagenes = NULL;
    nImagenes = capImagenes = 0;
    return copiadas;
}
