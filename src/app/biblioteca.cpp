// Ver biblioteca.h. Recorre los USB y /data buscando roms de SNES (.smc/.sfc), lee el nombre interno de cada una y
// arma su carátula. Igual que en el proyecto de PS2 (misma búsqueda recursiva y mismo acceso con GoldHEN).
#include "biblioteca.h"

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

struct Busq { Juego* lista; int max, n, prof, vistas; int* prog; char carpeta[1024]; };

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
        if (b.prog) *b.prog = b.vistas;
        ListarCarpeta(b.carpeta, AlEncontrar, u);
        b.prof--; b.carpeta[l] = 0;
        return;
    }
    if (b.n >= b.max || !EsRom(nombre)) return;
    Juego& j = b.lista[b.n];
    memset(&j, 0, sizeof j);
    snprintf(j.ruta, sizeof j.ruta, "%s/%s", b.carpeta, nombre);
    for (int i = 0; i < b.n; i++) if (!strcasecmp(b.lista[i].ruta, j.ruta)) return;
    snprintf(j.archivo, sizeof j.archivo, "%s", nombre);
    InfoRom info;
    SnesAnalizar(j.ruta, info);
    // El nombre del archivo es más legible que el interno (que viene en mayúsculas y cortado a 21 letras).
    NombreLimpio(nombre, j.nombre, sizeof j.nombre);
    if (!j.nombre[0] && info.valida) snprintf(j.nombre, sizeof j.nombre, "%s", info.titulo);
    snprintf(j.clave, sizeof j.clave, "%s", nombre);
    char* ext = strrchr(j.clave, '.');
    if (ext) *ext = 0;
    snprintf(j.mapeo, sizeof j.mapeo, "%s", info.mapeo);
    j.tamKB = info.tamKB; j.pal = info.pal; j.tieneRam = info.tieneRam; j.valida = info.valida;
    Iniciales(j);
    ColoresE(j);
    b.n++;
}

static int PorNombre(const void* a, const void* b) { return strcasecmp(((const Juego*)a)->nombre, ((const Juego*)b)->nombre); }

int BuscarJuegos(Juego* lista, int max, int* progreso)
{
    static Busq b;
    memset(&b, 0, sizeof b);
    b.lista = lista; b.max = max; b.prog = progreso;
    // Igual que en el proyecto de PS2: para ver el USB hay que salir del encierro del sistema con GoldHEN.
    AbrirAccesoCompleto("snes");
    for (const char* c : RAICES)
    {
        snprintf(b.carpeta, sizeof b.carpeta, "%s", c);
        ListarCarpeta(c, AlEncontrar, &b);
    }
    CerrarAccesoCompleto();
#ifdef SIN_LISTADO
    return -1;
#else
    qsort(lista, b.n, sizeof(Juego), PorNombre);
    return b.n;
#endif
}
