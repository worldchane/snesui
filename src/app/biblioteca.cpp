// Ver biblioteca.h. Recorre los USB y /data buscando roms de las consolas que trae el programa (por extensión, ver
// consola.h) en todas sus carpetas, lee la cabecera de cada una (solo las nuevas o cambiadas) y guarda la lista en un
// índice.
#include "biblioteca.h"

#include <atomic>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../comun/plataforma.h"
#include "../nucleo/snes.h"
#include "arcade.h"
#include "bios.h"
#include "caratula.h"
#include "metadatos.h"
#include "consola.h"
#include "registro.h"

static const char* const RAICES[] = {
    "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3", "/data/allesiu/roms",
};
static const int MAX_PROFUNDIDAD = 8, MAX_CARPETAS = 20000;

const char* CarpetasDeJuegos() { return "Busco en todos los USB (en la raíz y en todas sus carpetas) y en /data/allesiu/roms"; }

static const char* Ext(const char* n) { const char* p = strrchr(n, '.'); return p ? p + 1 : ""; }
// La consola de la rom, o -1 si no es una rom de una consola que el programa pueda abrir.
static int SistemaDe(const char* n)
{
    const Consola* c = ConsolaParaRuta(n);
    return c && c->presente ? c->sistema : -1;
}

static uint32_t Hsv(float h, float s, float v)
{
    float r, g, b, f = h * 6 - (int)(h * 6);
    float p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch ((int)(h * 6) % 6)
    {
    case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; break;
    }
    return (uint32_t)(r * 255) << 16 | (uint32_t)(g * 255) << 8 | (uint32_t)(b * 255);
}

static void ColoresE(Juego& j)
{
    uint32_t h = 2166136261u;
    for (const char* p = j.nombre; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
    float t = (h % 1000) / 1000.f;
    j.color1 = Hsv(t, .60f, .64f);
    j.color2 = Hsv(t + .07f, .72f, .28f);
}

static void Iniciales(Juego& j)
{
    int n = 0; bool ini = true;
    for (const char* p = j.nombre; *p && n < 2; p++)
    {
        unsigned char c = *p;
        if (isalnum(c)) { if (ini) j.iniciales[n++] = toupper(c); ini = false; }
        else ini = true;
    }
    if (!n) j.iniciales[n++] = '?';
    j.iniciales[n] = 0;
}

static void NombreLimpio(const char* archivo, char* out, int max)
{
    char tmp[128];
    snprintf(tmp, sizeof tmp, "%s", archivo);
    char* punto = strrchr(tmp, '.'); if (punto) *punto = 0;
    int n = 0, prof = 0;
    for (const char* p = tmp; *p && n < max - 1; p++)
    {
        char c = *p;
        if (c == '(' || c == '[') { prof++; continue; }
        if (c == ')' || c == ']') { if (prof) prof--; continue; }
        if (prof) continue;
        if (c == '_' || c == '.') c = ' ';
        if (c == ' ' && (n == 0 || out[n - 1] == ' ')) continue;
        out[n++] = c;
    }
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = 0;
}

// ---------- las listas ----------
// 'activa' es la que muestra la interfaz (solo la toca el hilo principal). El hilo de búsqueda arma 'nueva' y, cada
// tanto mientras busca y al terminar, deja una copia ordenada en 'entrega'; el hilo principal la cambia por 'activa'
// (BibliotecaNueva). Así los juegos van apareciendo mientras se busca. 'previa' es una copia de 'activa' al empezar,
// para reusar lo ya analizado sin tocar la de la interfaz.
struct Lista { Juego* j; int n, cap; };
static Lista activa, nueva, previa, entrega;
static std::atomic<bool> buscando{false}, hayNueva{false};
static std::atomic<int> encontrados{0};
static bool iniciada;
static uint32_t HashRuta(const char* s) { uint32_t h = 2166136261u; for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u; return h; }

static bool Copiar(Lista& dst, const Lista& src);
static bool Crecer(Lista& l)
{
    if (l.n < l.cap) return true;
    if (l.cap >= MAX_JUEGOS) return false;
    int cap = l.cap ? l.cap * 2 : 256;
    if (cap > MAX_JUEGOS) cap = MAX_JUEGOS;
    Juego* j = (Juego*)realloc(l.j, (size_t)cap * sizeof(Juego));
    if (!j) return false;
    l.j = j;
    l.cap = cap;
    return true;
}

// Copia el contenido de una lista en otra (agrandándola si hace falta).
static bool Copiar(Lista& dst, const Lista& src)
{
    if (dst.cap < src.n)
    {
        Juego* j = (Juego*)realloc(dst.j, (size_t)(src.n ? src.n : 1) * sizeof(Juego));
        if (!j) return false;
        dst.j = j;
        dst.cap = src.n;
    }
    if (src.n) memcpy(dst.j, src.j, (size_t)src.n * sizeof(Juego));
    dst.n = src.n;
    return true;
}

// Índice por ruta de la lista previa (tabla hash): para reusar lo ya analizado sin volver a leer la rom.
static int* tabla;
static int tablaTam;
static void ArmarTabla()
{
    int t = 1;
    while (t < previa.n * 2 + 16) t <<= 1;
    if (t != tablaTam) { free(tabla); tabla = (int*)malloc(sizeof(int) * t); tablaTam = tabla ? t : 0; }
    if (!tabla) return;
    for (int i = 0; i < tablaTam; i++) tabla[i] = -1;
    for (int i = 0; i < previa.n; i++)
    {
        uint32_t k = HashRuta(previa.j[i].ruta) & (tablaTam - 1);
        while (tabla[k] >= 0) k = (k + 1) & (tablaTam - 1);
        tabla[k] = i;
    }
}
static const Juego* BuscarAnterior(const char* ruta)
{
    if (!tabla) return NULL;
    for (uint32_t k = HashRuta(ruta) & (tablaTam - 1); tabla[k] >= 0; k = (k + 1) & (tablaTam - 1))
        if (!strcmp(previa.j[tabla[k]].ruta, ruta)) return &previa.j[tabla[k]];
    return NULL;
}


// Completa lo que se calcula del nombre (clave, iniciales, colores).
static void Completar(Juego& j)
{
    const char* barra = strrchr(j.ruta, '/');
    snprintf(j.archivo, sizeof j.archivo, "%s", barra ? barra + 1 : j.ruta);
    // La clave (carátula, favoritos, recientes) es el nombre del archivo sin extensión con la carpeta de su consola
    // delante ("nes/<nombre>"), porque dos consolas pueden tener juegos con el mismo nombre.
    snprintf(j.clave, sizeof j.clave, "%s/%s", ConsolaDe(j.sistema).carpeta, j.archivo);
    char* ext = strrchr(j.clave, '.');
    if (ext) *ext = 0;
    Iniciales(j);
    ColoresE(j);
    j.enUsb = !strncmp(j.ruta, "/mnt/usb", 8);
}

// Marca los juegos del USB que ya tienen una copia igual (mismo archivo y tamaño) en la consola.
static void MarcarCopiados(Lista& l)
{
    int t = 1;
    while (t < l.n * 2 + 16) t <<= 1;
    int* h = (int*)malloc(sizeof(int) * t);
    if (!h) return;
    for (int i = 0; i < t; i++) h[i] = -1;
    for (int i = 0; i < l.n; i++)
    {
        l.j[i].copiado = false;
        l.j[i].tambienEnUsb = false;
        if (l.j[i].enUsb) continue;
        uint32_t k = HashRuta(l.j[i].archivo) & (t - 1);
        while (h[k] >= 0) k = (k + 1) & (t - 1);
        h[k] = i;
    }
    for (int i = 0; i < l.n; i++)
    {
        Juego& j = l.j[i];
        if (!j.enUsb) continue;
        for (uint32_t k = HashRuta(j.archivo) & (t - 1); h[k] >= 0; k = (k + 1) & (t - 1))
        {
            Juego& c = l.j[h[k]];
            if (c.bytes == j.bytes && !strcmp(c.archivo, j.archivo)) { j.copiado = true; c.tambienEnUsb = true; break; }
        }
    }
    free(h);
}

// Cada juego, una sola vez: los que son de la misma consola y se llaman igual (sin la extensión) son el mismo juego,
// aunque estén en dos USB, en el USB y en la consola, en dos carpetas o uno comprimido y otro no. Se muestra el mejor:
// el de la consola antes que el del USB, y el suelto antes que el comprimido. Las regiones distintas ("(USA)",
// "(Europe)") se llaman distinto y siguen apareciendo por separado. Devuelve cuántos ocultó por cada causa.
struct Repetidos { int variosUsb, usbYConsola, otraCarpeta; };
static int PuntosCopia(const Juego& j) { return (!j.enUsb ? 4 : 0) + (!EsComprimido(j.archivo) ? 2 : 0); }
static uint32_t HashJuego(const Juego& j)
{
    uint32_t h = 2166136261u ^ (uint32_t)j.sistema;
    const char* fin = strrchr(j.archivo, '.');
    for (const char* p = j.archivo; *p && p != fin; p++) h = (h ^ (unsigned char)tolower((unsigned char)*p)) * 16777619u;
    return h;
}
static bool MismoJuego(const Juego& a, const Juego& b)
{
    if (a.sistema != b.sistema) return false;
    const char* fa = strrchr(a.archivo, '.');
    const char* fb = strrchr(b.archivo, '.');
    size_t la = fa ? (size_t)(fa - a.archivo) : strlen(a.archivo), lb = fb ? (size_t)(fb - b.archivo) : strlen(b.archivo);
    return la == lb && !strncasecmp(a.archivo, b.archivo, la);
}
// La ruta sin el "/mnt/usbN" del principio (el mismo USB puede aparecer montado dos veces).
static const char* RutaEnUsb(const char* r) { return !strncmp(r, "/mnt/usb", 8) && r[8] && r[9] == '/' ? r + 9 : r; }

static Repetidos MarcarRepetidos(Lista& l)
{
    Repetidos cuenta = {0, 0, 0};
    int t = 1;
    while (t < l.n * 2 + 16) t <<= 1;
    int* h = (int*)malloc(sizeof(int) * t);   // la copia elegida de cada juego
    if (!h) return cuenta;
    for (int i = 0; i < t; i++) h[i] = -1;
    for (int i = 0; i < l.n; i++) l.j[i].repetido = false;
    for (int i = 0; i < l.n; i++)
    {
        Juego& j = l.j[i];
        uint32_t k = HashJuego(j) & (t - 1);
        while (h[k] >= 0 && !MismoJuego(l.j[h[k]], j)) k = (k + 1) & (t - 1);
        if (h[k] < 0) { h[k] = i; continue; }
        // Ya había otra copia: queda la mejor y la otra se oculta.
        Juego& otra = l.j[h[k]];
        Juego* oculto = &j;
        if (PuntosCopia(j) > PuntosCopia(otra)) { oculto = &otra; h[k] = i; }
        Juego& quedo = oculto == &j ? otra : j;
        oculto->repetido = true;
        if (oculto->enUsb && quedo.enUsb && !strcmp(RutaEnUsb(oculto->ruta), RutaEnUsb(quedo.ruta))) cuenta.variosUsb++;
        else if (oculto->enUsb != quedo.enUsb) cuenta.usbYConsola++;
        else cuenta.otraCarpeta++;
    }
    free(h);
    return cuenta;
}

// ---------- recorrido del USB ----------
struct Busq { int prof, vistas, nuevos; bool rapida; char carpeta[1024]; };

// Carpetas que no tienen juegos: las ocultas, las del sistema y las de BIOS (sus zips no son juegos; las BIOS se buscan
// aparte, ver bios.cpp).
static bool Saltar(const char* n)
{
    return n[0] == '.' || n[0] == '$' || !strcasecmp(n, "System Volume Information") || !strcasecmp(n, "bios");
}

static int PorNombre(const void* a, const void* b);
static void MarcarCopiados(Lista& l);

// Deja en 'entrega' una copia ordenada de lo encontrado hasta ahora, si la interfaz ya tomó la anterior. Mientras
// busca, cada 1,5 s; al final, siempre (espera a que la interfaz tome la anterior).
static double ultimaEntrega;
static void Entregar(bool final)
{
    if (!final && Segundos() - ultimaEntrega < 1.5) return;
    if (hayNueva.load(std::memory_order_acquire))
    {
        if (!final) return;
        while (hayNueva.load(std::memory_order_acquire)) Dormir(5);
    }
    if (!Copiar(entrega, nueva)) return;
    if (entrega.n > 1) qsort(entrega.j, entrega.n, sizeof(Juego), PorNombre);
    MarcarCopiados(entrega);
    MarcarRepetidos(entrega);
    ultimaEntrega = Segundos();
    hayNueva.store(true, std::memory_order_release);
}

static void AlEncontrar(const char* nombre, bool carpeta, void* u)
{
    Busq& b = *(Busq*)u;
    if (carpeta)
    {
        if (Saltar(nombre) || b.prof >= MAX_PROFUNDIDAD || b.vistas >= MAX_CARPETAS) return;
        size_t l = strlen(b.carpeta);
        if (l + strlen(nombre) + 2 >= sizeof b.carpeta) return;
        snprintf(b.carpeta + l, sizeof b.carpeta - l, "/%s", nombre);
        b.prof++; b.vistas++;
        ListarCarpeta(b.carpeta, AlEncontrar, u);
        b.prof--; b.carpeta[l] = 0;
        return;
    }
    // Las BIOS de las colecciones ("[BIOS] SNK Neo Geo Pocket Color (World).zip") no son juegos: abiertas, pantalla negra.
    if (!strncmp(nombre, "[BIOS]", 6)) return;
    // Los datos de los juegos (descripción, año...) que arman Batocera, ES-DE y los programas de datos. En la búsqueda
    // rápida solo si todavía no hay ninguno guardado.
    if (!strcasecmp(nombre, "gamelist.xml"))
    {
        if (!b.rapida || !MetadatosHay())
        {
            char r[1100];
            snprintf(r, sizeof r, "%s/%s", b.carpeta, nombre);
            MetadatosLeerGamelist(r);
        }
        return;
    }
    // Una BIOS suelta en cualquier carpeta (no solo en bios/): se copia a la consola y no se lista como juego. En la
    // búsqueda rápida, solo las que no están en el índice (las ya conocidas como juego no se vuelven a mirar).
    {
        char r[1100];
        snprintf(r, sizeof r, "%s/%s", b.carpeta, nombre);
        if ((!b.rapida || !BuscarAnterior(r)) && BiosProbar(r, nombre)) return;
    }
    bool comprimido = EsComprimido(nombre);
    int sistema = comprimido ? 0 : SistemaDe(nombre);
    // ".bin" es muy común: suelto, solo cuenta como Mega Drive en una carpeta de Mega Drive o si trae la firma "SEGA" de
    // sus cartuchos (en 0x100). Adentro de un comprimido no hace falta (ver consola.cpp).
    if (sistema == SIS_MD && !strcasecmp(Ext(nombre), "bin") && ConsolaPorCarpeta(b.carpeta) != SIS_MD)
    {
        char r[1100], firma[4] = {0};
        snprintf(r, sizeof r, "%s/%s", b.carpeta, nombre);
        FILE* f = AbrirArchivo(r, "rb");
        bool sega = f && !fseek(f, 0x100, SEEK_SET) && fread(firma, 1, 4, f) == 4 && !memcmp(firma, "SEGA", 4);
        if (f) fclose(f);
        if (!sega) return;
    }
    if (sistema < 0 || !Crecer(nueva)) return;
    Juego& j = nueva.j[nueva.n];
    memset(&j, 0, sizeof j);
    j.sistema = sistema;
    snprintf(j.ruta, sizeof j.ruta, "%s/%s", b.carpeta, nombre);
    // Búsqueda rápida (al abrir la app): lo que ya está en el índice con la misma ruta se reusa sin preguntarle nada al
    // USB (cada consulta allá es lenta). La completa (triángulo) revisa además que no haya cambiado de tamaño.
    const Juego* antes = BuscarAnterior(j.ruta);
    // Un arcade que quedó sin reconocer (en "Arcade" con el nombre del zip) pero cuyo nombre la tabla sí conoce: se vuelve
    // a revisar (pasaba cuando la tabla todavía no estaba cargada; ver ArcadeIniciar en main.cpp).
    // (también los de otro nombre: ahora se reconocen por lo que tienen adentro)
    if (antes && antes->sistema == SIS_ARCADE && antes->estado == ARC_DESCONOCIDO)
        antes = NULL;
    // Un comprimido copiado a la consola que quedó como SNES: la carpeta de la app contaba como carpeta "de SNES" (ver
    // ConsolaPorCarpeta). Se vuelve a revisar (está en el disco de la consola: es rápido).
    if (antes && antes->sistema == SIS_SNES && comprimido && !strncmp(antes->ruta, CARPETA_DATOS, strlen(CARPETA_DATOS)))
        antes = NULL;
    uint64_t bytes = 0;
    if (antes && b.rapida) j = *antes;
    else if (!ExisteArchivo(j.ruta, &bytes)) return;
    else if (antes && antes->bytes == bytes) j = *antes;
    else
    {
        b.nuevos++;
        InfoRom info;
        memset(&info, 0, sizeof info);
        int deCarpeta = comprimido ? ConsolaPorCarpeta(b.carpeta) : -1;
        // Arcade: un comprimido en una carpeta de arcade, o cuyo nombre es el de un juego arcade conocido (y sus archivos
        // lo confirman). Las BIOS (neogeo.zip, pgm.zip...) no son juegos: no se listan.
        char base[128];
        snprintf(base, sizeof base, "%s", nombre);
        if (strrchr(base, '.')) *strrchr(base, '.') = 0;
        // En una carpeta de arcade ("arcade", "neogeo", "cps2"...) un zip con otro nombre ("<Título> (Neo Geo).zip") se
        // reconoce por lo que tiene adentro.
        bool hayArcade = ConsolaDe(SIS_ARCADE).presente, carpetaArcade = EsArcade(deCarpeta);
        int arc = comprimido && hayArcade && (carpetaArcade || deCarpeta < 0) ? ArcadeBuscar(base) : -1;
        if (arc < 0 && comprimido && hayArcade && carpetaArcade) arc = ArcadeIdentificar(j.ruta);
        int estado = ARC_SIN_REVISAR;
        if (arc >= 0)
        {
            char explicacion[160];
            estado = ArcadeRevisar(j.ruta, explicacion);
            if (!carpetaArcade && estado == ARC_OTRA_VERSION) arc = -1;   // casi nada coincide: no era arcade
        }
        if (arc >= 0 || (carpetaArcade && hayArcade))
        {
            if (arc >= 0 && ArcadeEsBios(arc)) return;
            sistema = arc >= 0 ? ArcadePlaca(arc) : SIS_ARCADE;   // Neo Geo, CPS1, CPS2 y CPS3 tienen pestaña propia
            j.sistema = sistema;
            j.estado = (int8_t)(arc >= 0 ? estado : ARC_DESCONOCIDO);
            info.valida = true;
            info.tamKB = (int)(bytes / 1024);
            if (arc >= 0) snprintf(info.titulo, sizeof info.titulo, "%s", ArcadeNombre(arc));
        }
        else if (deCarpeta >= 0 && ConsolaDe(deCarpeta).presente)
        {
            // Comprimido en una carpeta que ya dice la consola ("GBA", "roms-gba", "Game Boy Advance"...): no se abre
            // (abrir cientos en el USB es lento). La rom de adentro se busca al jugar.
            sistema = deCarpeta;
            j.sistema = sistema;
            info.valida = true;
            info.tamKB = (int)(bytes / 1024);
        }
        else if (comprimido)
        {
            // Comprimido: la consola es la de la rom de adentro (si no hay ninguna, no es un juego).
            uint64_t tam = 0;
            sistema = ConsolaDeComprimido(j.ruta, NULL, 0, &tam);
            // Sin rom de consola adentro: puede ser un juego arcade con otro nombre (se reconoce por su contenido).
            int a = sistema < 0 && ConsolaDe(SIS_ARCADE).presente ? ArcadeIdentificar(j.ruta) : -1;
            if (a >= 0 && ArcadeEsBios(a)) return;
            if (a >= 0)
            {
                char explicacion[160];
                sistema = ArcadePlaca(a);
                j.estado = (int8_t)ArcadeRevisar(j.ruta, explicacion);
                snprintf(info.titulo, sizeof info.titulo, "%s", ArcadeNombre(a));
                tam = bytes;
            }
            if (sistema < 0) return;
            j.sistema = sistema;
            info.valida = true;
            info.tamKB = (int)(tam / 1024);
        }
        else if (sistema == SIS_SNES) SnesAnalizar(j.ruta, info);
        else { info.valida = true; info.tamKB = (int)(bytes / 1024); }   // las otras consolas no se analizan
        // El nombre del archivo es más legible que el interno (que viene en mayúsculas y cortado a 21 letras).
        NombreLimpio(nombre, j.nombre, sizeof j.nombre);
        // Arcade: el título real del juego en vez del nombre corto del zip.
        int ja = EsArcade(sistema) ? ArcadeBuscar(info.titulo) : -1;
        if (ja >= 0) snprintf(j.nombre, sizeof j.nombre, "%s", ArcadeTitulo(ja));
        if (!j.nombre[0] && info.valida) snprintf(j.nombre, sizeof j.nombre, "%s", info.titulo);
        snprintf(j.mapeo, sizeof j.mapeo, "%s", info.mapeo);
        j.bytes = bytes;
        j.tamKB = info.tamKB; j.pal = info.pal; j.tieneRam = info.tieneRam; j.valida = info.valida;
        Completar(j);
    }
    nueva.n++;
    encontrados.store(nueva.n, std::memory_order_relaxed);
    Entregar(false);
}

static int PorNombre(const void* a, const void* b) { return strcasecmp(((const Juego*)a)->nombre, ((const Juego*)b)->nombre); }

// ---------- índice guardado ----------
// Una línea por juego, separada por tabuladores: consola, bytes, válida, PAL, SRAM, KB, mapeo, nombre, ruta. El índice
// de la versión 1 (solo SNES) no tenía la consola.
static const char* const CABECERA = "SNESUI-BIBLIOTECA 3", *const CABECERA_2 = "SNESUI-BIBLIOTECA 2",
                         *const CABECERA_1 = "SNESUI-BIBLIOTECA 1";

static void RutaIndice(char* r, int max) { snprintf(r, max, "%s/biblioteca.txt", CARPETA_DATOS); }

static void GuardarIndice(const Lista& l)
{
    size_t cap = (size_t)l.n * 700 + 64;
    char* txt = (char*)malloc(cap);
    if (!txt) return;
    size_t k = (size_t)snprintf(txt, cap, "%s\n", CABECERA);
    for (int i = 0; i < l.n; i++)
    {
        const Juego& j = l.j[i];
        k += (size_t)snprintf(txt + k, cap - k, "%d\t%d\t%llu\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n", j.sistema, j.estado,
                              (unsigned long long)j.bytes, j.valida, j.pal, j.tieneRam, j.tamKB, j.mapeo, j.nombre, j.ruta);
    }
    char r[160];
    RutaIndice(r, sizeof r);
    FILE* f = AbrirArchivo(r, "wb");
    if (f) { fwrite(txt, 1, k, f); fclose(f); }
    free(txt);
}

static void LeerIndice()
{
    char r[160];
    RutaIndice(r, sizeof r);
    uint64_t tam = 0;
    if (!ExisteArchivo(r, &tam) || tam == 0 || tam > 32u * 1024 * 1024) return;
    char* txt = (char*)malloc((size_t)tam + 1);
    if (!txt) return;
    if (LeerTexto(r, txt, (int)tam + 1) <= 0) { free(txt); return; }
    int campos;
    if (!strncmp(txt, CABECERA, strlen(CABECERA))) campos = 10;
    else if (!strncmp(txt, CABECERA_2, strlen(CABECERA_2))) campos = 9;
    else if (!strncmp(txt, CABECERA_1, strlen(CABECERA_1))) campos = 8;
    else { free(txt); return; }
    char* p = strchr(txt, '\n');
    while (p && *++p)
    {
        char* fin = strchr(p, '\n');
        if (fin) *fin = 0;
        // Partir la línea en sus campos (la versión 1 no tiene el primero, la consola: es SNES).
        char* c0[10];
        int n = 0;
        c0[n++] = p;
        for (char* q = p; *q && n < campos; q++) if (*q == '\t') { *q = 0; c0[n++] = q + 1; }
        char** c = c0 + (campos - 8);   // los 8 campos de siempre, después de la consola y el estado
        int sistema = campos >= 9 ? atoi(c0[0]) : SIS_SNES;
        int estado = campos == 10 ? atoi(c0[1]) : 0;
        if (n == campos && sistema >= 0 && sistema < NUM_SISTEMAS && ConsolaDe(sistema).presente && Crecer(activa))
        {
            Juego& j = activa.j[activa.n];
            memset(&j, 0, sizeof j);
            j.sistema = sistema;
            j.estado = (int8_t)estado;
            j.bytes = strtoull(c[0], NULL, 10);
            j.valida = atoi(c[1]); j.pal = atoi(c[2]); j.tieneRam = atoi(c[3]); j.tamKB = atoi(c[4]);
            snprintf(j.mapeo, sizeof j.mapeo, "%s", c[5]);
            snprintf(j.nombre, sizeof j.nombre, "%s", c[6]);
            snprintf(j.ruta, sizeof j.ruta, "%s", c[7]);
            Completar(j);
            activa.n++;
        }
        if (!fin) break;
        p = fin;
    }
    free(txt);
}

// ---------- búsqueda en segundo plano ----------
static bool proximaRapida;   // la próxima búsqueda es la rápida (ver AlEncontrar)
static std::atomic<int> caratulasCopiadas{0};   // de la última búsqueda, para el aviso (BibliotecaCaratulasCopiadas)
static bool proximaInsiste;  // la pidió el usuario (triángulo): se vuelve a pedir el acceso aunque antes haya cerrado el programa

static void* HiloBusqueda(void*)
{
    double t0 = Segundos();
    ultimaEntrega = t0;
    nueva.n = 0;
    encontrados.store(0, std::memory_order_relaxed);
    Copiar(previa, activa);   // la interfaz no la cambia mientras tanto: solo cambia con una entrega, y no hay ninguna pendiente
    ArmarTabla();
    static Busq b;
    memset(&b, 0, sizeof b);
    b.rapida = proximaRapida;
    MetadatosEmpezar();
    BiosRevisar();   // qué BIOS ya están en la consola (la búsqueda copia las que encuentre y falten)
    // Para ver el USB hay que salir del encierro del sistema con GoldHEN.
    if (AbrirAccesoCompleto("snes", proximaInsiste) <= 0) Registrar("Sin acceso al USB: %s", AccesoDetalle());
    bool algo = false;
    for (const char* c : RAICES)
    {
        snprintf(b.carpeta, sizeof b.carpeta, "%s", c);
        if (ListarCarpeta(c, AlEncontrar, &b) >= 0) algo = true;
    }
    MetadatosTerminar();
    // Las BIOS que haya en las carpetas de BIOS del USB y falten en la consola (con el mismo acceso abierto).
    int bios = BiosBuscarEnUsb();
    if (bios) Registrar("BIOS nuevas copiadas a la consola: %d", bios);
    // Las carátulas que el usuario ya tenga en el USB, para los juegos que no tengan una.
    int car = CaratulasDelUsb(nueva.j, nueva.n);
    if (car) Registrar("Carátulas copiadas del USB: %d", car);
    caratulasCopiadas.store(car, std::memory_order_release);
    CerrarAccesoCompleto();
    if (nueva.n > 1) qsort(nueva.j, nueva.n, sizeof(Juego), PorNombre);
    Registrar("Búsqueda %s: %d carpetas, %d juegos (%d nuevos o cambiados), %.1f s", b.rapida ? "rápida" : "completa",
              b.vistas, nueva.n, b.nuevos, Segundos() - t0);
    MarcarCopiados(nueva);
    Repetidos rep = MarcarRepetidos(nueva);
    if (rep.variosUsb + rep.usbYConsola + rep.otraCarpeta)
        Registrar("Juegos repetidos ocultos: %d por el mismo USB montado dos veces, %d en el USB y en la consola, %d en otra "
                  "carpeta o formato", rep.variosUsb, rep.usbYConsola, rep.otraCarpeta);
    // Si no se pudo abrir ninguna carpeta (sin acceso, o la variante sin listado), se conserva la lista anterior (y se
    // espera a que la interfaz tome una entrega parcial que haya quedado, para que no quede a medias).
    if (algo)
    {
        GuardarIndice(nueva);
        Entregar(true);
    }
    buscando.store(false, std::memory_order_release);
    return NULL;
}

// ---------- USB conectados ----------
// Un hilo mira cada 5 s qué USB hay (solo mientras la biblioteca está a la vista y no se está buscando). La primera
// vez anota los que hay; después avisa si apareció o se fue alguno.
static std::atomic<bool> vigilar{false};
static std::atomic<int> cambioUsb{0};   // 1 = se conectó uno, -1 = se sacó uno (lo toma BibliotecaCambioUsb)
static void* HiloUsb(void*)
{
    static const char* const USB[] = {"/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3"};
    int antes = -1;
    for (;;)
    {
        Dormir(5000);
        if (!vigilar.load(std::memory_order_acquire) || buscando.load(std::memory_order_acquire)) continue;
        int hay = 0;
        AbrirAccesoCompleto("snes");
        for (int i = 0; i < 4; i++) if (ListarCarpeta(USB[i], [](const char*, bool, void*) {}, NULL) >= 0) hay |= 1 << i;
        CerrarAccesoCompleto();
        if (antes >= 0 && hay != antes) cambioUsb.store((hay & ~antes) ? 1 : -1, std::memory_order_release);
        antes = hay;
    }
    return NULL;
}

void BibliotecaVigilarUsb(bool si)
{
    static bool lanzado;
    if (!lanzado) lanzado = Lanzar(HiloUsb, NULL, "usb");
    vigilar.store(si, std::memory_order_release);
}

int BibliotecaCambioUsb() { return cambioUsb.exchange(0, std::memory_order_acq_rel); }

int BibliotecaCaratulasCopiadas() { return caratulasCopiadas.exchange(0, std::memory_order_acq_rel); }

void BibliotecaBuscar() { BibliotecaBuscarDe(false, true); }

void BibliotecaBuscarDe(bool rapida, bool insistir)
{
    if (buscando.load(std::memory_order_acquire) || hayNueva.load(std::memory_order_acquire)) return;
    buscando.store(true, std::memory_order_release);
    proximaRapida = rapida;
    proximaInsiste = insistir;
    if (!Lanzar(HiloBusqueda, NULL, "biblioteca")) buscando.store(false, std::memory_order_release);
}

void BibliotecaIniciar()
{
    if (iniciada) return;
    iniciada = true;
    LeerIndice();
    MarcarCopiados(activa);
    MarcarRepetidos(activa);
    BibliotecaBuscarDe(true);   // al abrir: la rápida (el índice ya tiene casi todo)
}

bool BibliotecaBuscando(int* n)
{
    if (n) *n = encontrados.load(std::memory_order_relaxed);
    return buscando.load(std::memory_order_acquire);
}

bool BibliotecaNueva()
{
    if (!hayNueva.load(std::memory_order_acquire)) return false;
    Lista vieja = activa;
    activa = entrega;
    entrega = vieja;
    hayNueva.store(false, std::memory_order_release);
    return true;
}

const Juego* BibliotecaJuegos(int* n)
{
    *n = activa.n;
    return activa.j;
}
