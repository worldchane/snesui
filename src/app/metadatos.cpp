// Ver metadatos.h. El índice se guarda como texto, una línea por juego:
//   clave \t año \t desarrollador \t género \t jugadores \t descripción
// (la clave es el nombre del archivo sin extensión, solo letras y números en minúsculas, como se compara al buscar).
#include "metadatos.h"

#include <atomic>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"
#include "registro.h"

struct Entrada { const char* clave; Metadato m; };
struct Tabla { char* texto; Entrada* e; int n; };
static std::atomic<Tabla*> actual{nullptr};
static Tabla* anterior;   // la que se reemplazó: se libera en el próximo cambio (la interfaz pudo estar leyéndola)

static void Clave(const char* s, size_t largo, char* out, int max)
{
    int n = 0;
    for (size_t i = 0; i < largo && s[i] && n < max - 1; i++)
    {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c)) out[n++] = (char)tolower(c);
    }
    out[n] = 0;
}

static int PorClave(const void* a, const void* b) { return strcmp(((const Entrada*)a)->clave, ((const Entrada*)b)->clave); }

// Arma la tabla a partir del texto (que pasa a ser de la tabla: los campos apuntan adentro).
static Tabla* Armar(char* texto)
{
    int lineas = 0;
    for (char* p = texto; *p; p++) lineas += *p == '\n';
    Tabla* t = (Tabla*)calloc(1, sizeof(Tabla));
    if (!t) { free(texto); return NULL; }
    t->texto = texto;
    t->e = (Entrada*)malloc(sizeof(Entrada) * (lineas + 1));
    if (!t->e) { free(texto); free(t); return NULL; }
    for (char* p = texto; *p;)
    {
        char* fin = strchr(p, '\n');
        if (fin) *fin = 0;
        char* campo[6];
        int k = 0;
        campo[k++] = p;
        for (char* q = p; *q && k < 6; q++) if (*q == '\t') { *q = 0; campo[k++] = q + 1; }
        if (k == 6 && campo[0][0])
        {
            Entrada& e = t->e[t->n++];
            e.clave = campo[0];
            e.m = {campo[1], campo[2], campo[3], campo[4], campo[5]};
        }
        if (!fin) break;
        p = fin + 1;
    }
    qsort(t->e, t->n, sizeof(Entrada), PorClave);
    return t;
}

static void Usar(Tabla* t)
{
    if (!t) return;
    Tabla* vieja = actual.exchange(t, std::memory_order_acq_rel);
    if (anterior) { free(anterior->texto); free(anterior->e); free(anterior); }
    anterior = vieja;
}

static void RutaIndice(char* r, int max) { snprintf(r, max, "%s/metadatos.txt", CARPETA_DATOS); }

void MetadatosCargar()
{
    char r[160];
    RutaIndice(r, sizeof r);
    uint64_t tam = 0;
    if (!ExisteArchivo(r, &tam) || !tam || tam > 64u << 20) return;
    char* txt = (char*)malloc((size_t)tam + 1);
    if (!txt) return;
    if (LeerTexto(r, txt, (int)tam + 1) <= 0) { free(txt); return; }
    Usar(Armar(txt));
}

bool MetadatosHay() { Tabla* t = actual.load(std::memory_order_acquire); return t && t->n; }

const Metadato* MetadatoDe(const char* archivo)
{
    Tabla* t = actual.load(std::memory_order_acquire);
    if (!t || !t->n) return NULL;
    const char* punto = strrchr(archivo, '.');
    char c[96];
    Clave(archivo, punto ? (size_t)(punto - archivo) : strlen(archivo), c, sizeof c);
    int a = 0, b = t->n;
    while (a < b) { int m = (a + b) / 2; if (strcmp(t->e[m].clave, c) < 0) a = m + 1; else b = m; }
    return a < t->n && !strcmp(t->e[a].clave, c) ? &t->e[a].m : NULL;
}

// ---------- lectura de los gamelist.xml (en la búsqueda) ----------
static char* nuevo;            // las líneas encontradas en esta búsqueda
static size_t nNuevo, capNuevo;
static int juegosNuevos;

static void Agregar(const char* s, size_t n)
{
    if (nNuevo + n + 1 > capNuevo)
    {
        size_t cap = capNuevo ? capNuevo * 2 : 1 << 20;
        while (cap < nNuevo + n + 1) cap *= 2;
        char* v = (char*)realloc(nuevo, cap);
        if (!v) return;
        nuevo = v;
        capNuevo = cap;
    }
    memcpy(nuevo + nNuevo, s, n);
    nNuevo += n;
    nuevo[nNuevo] = 0;
}

// El contenido de <etiqueta>…</etiqueta> dentro de [ini, fin), con las entidades de XML resueltas y sin saltos de línea
// ni tabuladores (separan los campos del índice). Vacío si no está.
static void Etiqueta(const char* ini, const char* fin, const char* nombre, char* out, int max)
{
    out[0] = 0;
    char abre[32], cierra[32];
    snprintf(abre, sizeof abre, "<%s>", nombre);
    snprintf(cierra, sizeof cierra, "</%s>", nombre);
    const char* a = strstr(ini, abre);
    if (!a || a >= fin) return;
    a += strlen(abre);
    const char* b = strstr(a, cierra);
    if (!b || b > fin) return;
    int n = 0;
    for (const char* p = a; p < b && n < max - 1; p++)
    {
        char c = *p;
        if (c == '&')
        {
            static const struct { const char* e; char c; } ENT[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
            bool hecho = false;
            for (auto& x : ENT)
                if (!strncmp(p, x.e, strlen(x.e))) { out[n++] = x.c; p += strlen(x.e) - 1; hecho = true; break; }
            if (hecho) continue;
            if (p[1] == '#')
            {
                // &#233; o &#xE9;: solo los de un byte (los demás se saltan).
                const char* q = p + 2;
                long v = *q == 'x' ? strtol(q + 1, (char**)&q, 16) : strtol(q, (char**)&q, 10);
                if (*q == ';') { if (v >= 32 && v < 127) out[n++] = (char)v; p = q; continue; }
            }
        }
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ' && n > 0 && out[n - 1] == ' ') continue;
        out[n++] = c;
    }
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = 0;
}

void MetadatosEmpezar()
{
    nNuevo = 0;
    juegosNuevos = 0;
    if (nuevo) nuevo[0] = 0;
}

void MetadatosLeerGamelist(const char* ruta)
{
    uint64_t tam = 0;
    if (!ExisteArchivo(ruta, &tam) || !tam || tam > 32u << 20) return;
    char* xml = (char*)malloc((size_t)tam + 1);
    if (!xml) return;
    FILE* f = AbrirArchivo(ruta, "rb");
    bool ok = f && fread(xml, 1, (size_t)tam, f) == tam;
    if (f) fclose(f);
    if (!ok) { free(xml); return; }
    xml[tam] = 0;
    int antes = juegosNuevos;
    for (const char* p = strstr(xml, "<game"); p; p = strstr(p, "<game"))
    {
        // "<game>" o "<game id=...>", no "<gameList>".
        if (p[5] != '>' && p[5] != ' ') { p += 5; continue; }
        const char* fin = strstr(p, "</game>");
        if (!fin) break;
        static char ruta2[512], anio[16], dev[96], gen[96], jug[16], desc[700];
        Etiqueta(p, fin, "path", ruta2, sizeof ruta2);
        Etiqueta(p, fin, "desc", desc, sizeof desc);
        Etiqueta(p, fin, "releasedate", anio, sizeof anio);
        anio[4] = 0;   // "19920801T000000" → "1992"
        Etiqueta(p, fin, "developer", dev, sizeof dev);
        if (!dev[0]) Etiqueta(p, fin, "publisher", dev, sizeof dev);
        Etiqueta(p, fin, "genre", gen, sizeof gen);
        Etiqueta(p, fin, "players", jug, sizeof jug);
        p = fin + 7;
        if (!ruta2[0] || (!desc[0] && !anio[0] && !dev[0] && !gen[0])) continue;
        const char* base = strrchr(ruta2, '/');
        base = base ? base + 1 : ruta2;
        const char* punto = strrchr(base, '.');
        char clave[96];
        Clave(base, punto ? (size_t)(punto - base) : strlen(base), clave, sizeof clave);
        if (!clave[0]) continue;
        char linea[1100];
        int n = snprintf(linea, sizeof linea, "%s\t%s\t%s\t%s\t%s\t%s\n", clave, anio, dev, gen, jug, desc);
        if (n > 0) Agregar(linea, n < (int)sizeof linea ? (size_t)n : sizeof linea - 1);
        juegosNuevos++;
    }
    free(xml);
    if (juegosNuevos > antes) Registrar("Datos de juegos leídos de %s: %d", ruta, juegosNuevos - antes);
}

void MetadatosTerminar()
{
    if (!juegosNuevos || !nuevo) return;
    char r[160];
    RutaIndice(r, sizeof r);
    FILE* f = AbrirArchivo(r, "wb");
    if (f) { fwrite(nuevo, 1, nNuevo, f); fclose(f); }
    char* copia = (char*)malloc(nNuevo + 1);
    if (!copia) return;
    memcpy(copia, nuevo, nNuevo + 1);
    Usar(Armar(copia));
}
