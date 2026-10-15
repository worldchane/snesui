// Ver biblioteca.h. Recorre los USB y /data buscando roms de SNES (.smc/.sfc/.fig/.swc) en todas sus carpetas, lee la
// cabecera de cada una (solo las nuevas o cambiadas) y guarda la lista en un índice.
#include "biblioteca.h"

#include <atomic>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../comun/plataforma.h"
#include "../nucleo/snes.h"

static const char* const RAICES[] = {
    "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3", "/data/snes-ps4/juegos",
};
static const int MAX_PROFUNDIDAD = 8, MAX_CARPETAS = 4000;

const char* CarpetasDeJuegos() { return "Busco en todos los USB (en la raíz y en todas sus carpetas) y en /data/snes-ps4/juegos"; }

static const char* Ext(const char* n) { const char* p = strrchr(n, '.'); return p ? p + 1 : ""; }
static bool EsRom(const char* n) { const char* e = Ext(n); return !strcasecmp(e, "smc") || !strcasecmp(e, "sfc") || !strcasecmp(e, "fig") || !strcasecmp(e, "swc"); }

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

// ---------- las dos listas ----------
// 'activa' es la que muestra la interfaz (solo se lee); 'nueva' la arma el hilo de búsqueda. Al terminar se cambian
// (en el hilo principal, con BibliotecaNueva): la vieja queda para la búsqueda siguiente.
struct Lista { Juego* j; int n, cap; };
static Lista activa, nueva;
static std::atomic<bool> buscando{false}, hayNueva{false};
static std::atomic<int> encontrados{0};
static bool iniciada;
static uint32_t HashRuta(const char* s) { uint32_t h = 2166136261u; for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u; return h; }

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

// Índice por ruta de la lista activa (tabla hash): para reusar lo ya analizado sin volver a leer la rom.
static int* tabla;
static int tablaTam;
static void ArmarTabla()
{
    int t = 1;
    while (t < activa.n * 2 + 16) t <<= 1;
    if (t != tablaTam) { free(tabla); tabla = (int*)malloc(sizeof(int) * t); tablaTam = tabla ? t : 0; }
    if (!tabla) return;
    for (int i = 0; i < tablaTam; i++) tabla[i] = -1;
    for (int i = 0; i < activa.n; i++)
    {
        uint32_t k = HashRuta(activa.j[i].ruta) & (tablaTam - 1);
        while (tabla[k] >= 0) k = (k + 1) & (tablaTam - 1);
        tabla[k] = i;
    }
}
static const Juego* BuscarAnterior(const char* ruta)
{
    if (!tabla) return NULL;
    for (uint32_t k = HashRuta(ruta) & (tablaTam - 1); tabla[k] >= 0; k = (k + 1) & (tablaTam - 1))
        if (!strcmp(activa.j[tabla[k]].ruta, ruta)) return &activa.j[tabla[k]];
    return NULL;
}


// Completa lo que se calcula del nombre (clave, iniciales, colores).
static void Completar(Juego& j)
{
    const char* barra = strrchr(j.ruta, '/');
    snprintf(j.archivo, sizeof j.archivo, "%s", barra ? barra + 1 : j.ruta);
    snprintf(j.clave, sizeof j.clave, "%s", j.archivo);
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
            const Juego& c = l.j[h[k]];
            if (c.bytes == j.bytes && !strcmp(c.archivo, j.archivo)) { j.copiado = true; break; }
        }
    }
    free(h);
}

// ---------- recorrido del USB ----------
struct Busq { int prof, vistas; char carpeta[1024]; };

static bool Saltar(const char* n) { return n[0] == '.' || n[0] == '$' || !strcasecmp(n, "System Volume Information"); }

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
    if (!EsRom(nombre) || !Crecer(nueva)) return;
    Juego& j = nueva.j[nueva.n];
    memset(&j, 0, sizeof j);
    snprintf(j.ruta, sizeof j.ruta, "%s/%s", b.carpeta, nombre);
    uint64_t bytes = 0;
    if (!ExisteArchivo(j.ruta, &bytes)) return;
    // Si ya estaba en el índice con el mismo tamaño, se reusa sin leer la rom.
    const Juego* antes = BuscarAnterior(j.ruta);
    if (antes && antes->bytes == bytes) j = *antes;
    else
    {
        InfoRom info;
        SnesAnalizar(j.ruta, info);
        // El nombre del archivo es más legible que el interno (que viene en mayúsculas y cortado a 21 letras).
        NombreLimpio(nombre, j.nombre, sizeof j.nombre);
        if (!j.nombre[0] && info.valida) snprintf(j.nombre, sizeof j.nombre, "%s", info.titulo);
        snprintf(j.mapeo, sizeof j.mapeo, "%s", info.mapeo);
        j.bytes = bytes;
        j.tamKB = info.tamKB; j.pal = info.pal; j.tieneRam = info.tieneRam; j.valida = info.valida;
        Completar(j);
    }
    nueva.n++;
    encontrados.store(nueva.n, std::memory_order_relaxed);
}

static int PorNombre(const void* a, const void* b) { return strcasecmp(((const Juego*)a)->nombre, ((const Juego*)b)->nombre); }

// ---------- índice guardado ----------
// Una línea por juego, separada por tabuladores: bytes, válida, PAL, SRAM, KB, mapeo, nombre, ruta.
static const char* const CABECERA = "SNESUI-BIBLIOTECA 1";

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
        k += (size_t)snprintf(txt + k, cap - k, "%llu\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n", (unsigned long long)j.bytes, j.valida,
                              j.pal, j.tieneRam, j.tamKB, j.mapeo, j.nombre, j.ruta);
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
    if (LeerTexto(r, txt, (int)tam + 1) <= 0 || strncmp(txt, CABECERA, strlen(CABECERA))) { free(txt); return; }
    char* p = strchr(txt, '\n');
    while (p && *++p)
    {
        char* fin = strchr(p, '\n');
        if (fin) *fin = 0;
        // Partir la línea en sus 8 campos.
        char* c[8];
        int n = 0;
        c[n++] = p;
        for (char* q = p; *q && n < 8; q++) if (*q == '\t') { *q = 0; c[n++] = q + 1; }
        if (n == 8 && Crecer(activa))
        {
            Juego& j = activa.j[activa.n];
            memset(&j, 0, sizeof j);
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
static void* HiloBusqueda(void*)
{
    nueva.n = 0;
    encontrados.store(0, std::memory_order_relaxed);
    ArmarTabla();
    static Busq b;
    memset(&b, 0, sizeof b);
    // Para ver el USB hay que salir del encierro del sistema con GoldHEN.
    AbrirAccesoCompleto("snes");
    bool algo = false;
    for (const char* c : RAICES)
    {
        snprintf(b.carpeta, sizeof b.carpeta, "%s", c);
        if (ListarCarpeta(c, AlEncontrar, &b) >= 0) algo = true;
    }
    CerrarAccesoCompleto();
    if (nueva.n > 1) qsort(nueva.j, nueva.n, sizeof(Juego), PorNombre);
    MarcarCopiados(nueva);
    // Si no se pudo abrir ninguna carpeta (sin acceso, o la variante sin listado), se conserva la lista anterior.
    if (algo)
    {
        GuardarIndice(nueva);
        hayNueva.store(true, std::memory_order_release);
    }
    buscando.store(false, std::memory_order_release);
    return NULL;
}

void BibliotecaBuscar()
{
    if (buscando.load(std::memory_order_acquire) || hayNueva.load(std::memory_order_acquire)) return;
    buscando.store(true, std::memory_order_release);
    if (!Lanzar(HiloBusqueda, NULL, "biblioteca")) buscando.store(false, std::memory_order_release);
}

void BibliotecaIniciar()
{
    if (iniciada) return;
    iniciada = true;
    LeerIndice();
    MarcarCopiados(activa);
    BibliotecaBuscar();
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
    activa = nueva;
    nueva = vieja;
    hayNueva.store(false, std::memory_order_release);
    return true;
}

const Juego* BibliotecaJuegos(int* n)
{
    *n = activa.n;
    return activa.j;
}
