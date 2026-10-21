// La mudanza de los datos de versiones anteriores: /data/snes-ps4 → /data/allesiu y las carpetas de cada consola con
// los nombres de Batocera. Se hace al abrir, antes de todo lo demás.
#include "mudanza.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"
#include "../nucleos/comprimido.h"
#include "arcade.h"
#include "consola.h"

// Hasta la 1.4 los datos estaban en /data/snes-ps4 (del tiempo en que era solo de SNES). Al abrir esta versión se
// pasan a /data/allesiu (partidas, estados, carátulas, juegos copiados, BIOS, ajustes) y se corrigen las rutas que
// guarda el índice de la biblioteca. Si la carpeta nueva ya existe (la creó una versión de prueba), se juntan: se mueve
// lo que falta, carpeta por carpeta, sin pisar nada de la nueva. Lo que no se pudo mover queda en la vieja.
struct Nombres { int n, cap; char** v; };
static void AlListarNombre(const char* nombre, bool, void* u)
{
    Nombres& l = *(Nombres*)u;
    if (l.n == l.cap)
    {
        int cap = l.cap ? l.cap * 2 : 64;
        char** v = (char**)realloc(l.v, sizeof(char*) * cap);
        if (!v) return;
        l.v = v;
        l.cap = cap;
    }
    l.v[l.n++] = strdup(nombre);
}
static void SoltarNombres(Nombres& l) { for (int i = 0; i < l.n; i++) free(l.v[i]); free(l.v); l = Nombres(); }
static void AlListarConTipo(const char* nombre, bool carpeta, void* u)
{
    // El tipo va como prefijo ("d" carpeta, "f" archivo) para usar la misma lista.
    char t[300];
    snprintf(t, sizeof t, "%c%s", carpeta ? 'd' : 'f', nombre);
    AlListarNombre(t, carpeta, u);
}
static int migrados, sinMover;
static void Juntar(const char* vieja, const char* nueva, int prof)
{
    Nombres enVieja = {}, enNueva = {};
    ListarCarpeta(vieja, AlListarConTipo, &enVieja);
    ListarCarpeta(nueva, AlListarNombre, &enNueva);
    for (int i = 0; i < enVieja.n; i++)
    {
        const char* nombre = enVieja.v[i] + 1;
        bool carpeta = enVieja.v[i][0] == 'd';
        // El estado del acceso de GoldHEN ("probando", "cerro") era de la versión anterior: se vuelve a probar.
        if (prof == 0 && !strncmp(nombre, "acceso_", 7)) continue;
        bool esta = false;
        for (int k = 0; k < enNueva.n && !esta; k++) esta = !strcmp(enNueva.v[k], nombre);
        char de[600], a[600];
        snprintf(de, sizeof de, "%s/%s", vieja, nombre);
        snprintf(a, sizeof a, "%s/%s", nueva, nombre);
        if (!esta) { if (RenombrarArchivo(de, a)) migrados++; else sinMover++; }
        else if (carpeta && prof < 4) Juntar(de, a, prof + 1);
        else sinMover++;   // el mismo archivo en las dos: queda el de la nueva
    }
    SoltarNombres(enVieja);
    SoltarNombres(enNueva);
}

// Cambia en un archivo de texto de /data/allesiu cada aparición de viejos[i] por nuevos[i] (las rutas del índice de
// la biblioteca).
static void CambiarEnArchivo(const char* archivo, const char* const* viejos, const char* const* nuevos, int n)
{
    char ruta[160];
    snprintf(ruta, sizeof ruta, "%s/%s", CARPETA_DATOS, archivo);
    uint64_t tam = 0;
    if (!ExisteArchivo(ruta, &tam) || !tam || tam > 64u * 1024 * 1024) return;
    char* d = (char*)malloc((size_t)tam + 1);
    FILE* f = d ? AbrirArchivo(ruta, "rb") : NULL;
    bool leido = f && fread(d, 1, (size_t)tam, f) == tam;
    if (f) fclose(f);
    if (!leido) { free(d); return; }
    d[tam] = 0;
    size_t cap = (size_t)tam * 2 + 4096;   // alcanza aunque los nombres nuevos sean más largos en cada línea
    char* sale = (char*)malloc(cap);
    if (!sale) { free(d); return; }
    size_t k = 0;
    bool cambio = false;
    // Muchos reemplazos (los juegos movidos: rutas enteras con su salto de línea): se ordenan y en cada lugar que
    // empieza como ellas se busca la línea entera con búsqueda binaria. Comparar cada letra con miles de rutas tardaba
    // minutos con una biblioteca grande.
    int* orden = NULL;
    size_t prefijo = 0;
    if (n > 32)
    {
        orden = (int*)malloc(sizeof(int) * n);
        if (orden)
        {
            for (int i = 0; i < n; i++) orden[i] = i;
            static const char* const* tabla;
            tabla = viejos;
            qsort(orden, n, sizeof(int), [](const void* a, const void* b) { return strcmp(tabla[*(const int*)a], tabla[*(const int*)b]); });
            // Lo que tienen en común todas (por ejemplo "/data/allesiu/roms/"): solo ahí se busca.
            prefijo = strlen(viejos[0]);
            for (int i = 1; i < n; i++)
            {
                size_t c = 0;
                while (c < prefijo && viejos[i][c] == viejos[0][c]) c++;
                prefijo = c;
            }
        }
    }
    for (const char* p = d; *p && k + 64 < cap;)
    {
        int i = n;
        if (orden)
        {
            if (prefijo && strncmp(p, viejos[0], prefijo) == 0)
            {
                // La línea desde acá (con su salto) contra las rutas ordenadas.
                const char* fin = strchr(p, '\n');
                size_t largo = fin ? (size_t)(fin - p) + 1 : strlen(p);
                int a = 0, b = n;
                while (a < b)
                {
                    int m = (a + b) / 2;
                    int c = strncmp(viejos[orden[m]], p, largo);
                    if (c == 0 && viejos[orden[m]][largo]) c = 1;
                    if (c < 0) a = m + 1; else b = m;
                }
                if (a < n && strlen(viejos[orden[a]]) == largo && !strncmp(viejos[orden[a]], p, largo)) i = orden[a];
            }
        }
        else
        {
            i = 0;
            while (i < n && strncmp(p, viejos[i], strlen(viejos[i]))) i++;
        }
        if (i < n)
        {
            size_t ln = strlen(nuevos[i]);
            memcpy(sale + k, nuevos[i], ln);
            k += ln;
            p += strlen(viejos[i]);
            cambio = true;
        }
        else sale[k++] = *p++;
    }
    free(orden);
    if (cambio && (f = AbrirArchivo(ruta, "wb"))) { fwrite(sale, 1, k, f); fclose(f); }
    free(sale);
    free(d);
}

static void CorregirRutasBiblioteca()
{
    char viejo[64], nuevo[64];
    snprintf(viejo, sizeof viejo, "%s/", CARPETA_DATOS_VIEJA);
    snprintf(nuevo, sizeof nuevo, "%s/", CARPETA_DATOS);
    const char* v[1] = {viejo};
    const char* nu[1] = {nuevo};
    CambiarEnArchivo("biblioteca.txt", v, nu, 1);
}

static char migracion[700];
static void MigrarDatos()
{
    Nombres hay = {};
    if (ListarCarpeta(CARPETA_DATOS_VIEJA, AlListarNombre, &hay) < 0) return;   // no hay carpeta vieja
    bool vacia = hay.n == 0;
    SoltarNombres(hay);
    if (vacia) return;
    if (RenombrarArchivo(CARPETA_DATOS_VIEJA, CARPETA_DATOS))
    {
        char acceso[160];
        snprintf(acceso, sizeof acceso, "%s/acceso_snes.txt", CARPETA_DATOS);
        BorrarArchivo(acceso);
        snprintf(migracion, sizeof migracion, "Datos de %s pasados a %s.", CARPETA_DATOS_VIEJA, CARPETA_DATOS);
    }
    else
    {
        CrearCarpetas(CARPETA_DATOS);
        Juntar(CARPETA_DATOS_VIEJA, CARPETA_DATOS, 0);
        // Lo que no se movió (estaba en las dos) queda aparte, dentro de la nueva: así no se vuelve a revisar en cada
        // arranque y no se pierde nada.
        char restos[160];
        snprintf(restos, sizeof restos, "%s/restos-snes-ps4", CARPETA_DATOS);
        bool apartada = RenombrarArchivo(CARPETA_DATOS_VIEJA, restos);
        snprintf(migracion, sizeof migracion, "Datos de %s juntados con %s: %d movidos, %d quedaron en %s.",
                 CARPETA_DATOS_VIEJA, CARPETA_DATOS, migrados, sinMover, apartada ? restos : CARPETA_DATOS_VIEJA);
    }
    CorregirRutasBiblioteca();
}

// Hasta la 1.4 las carpetas de cada consola tenían nombres propios ("SNES", "MD", "SMS"...; la SNES, sin carpeta, en la
// raíz de guardados y carátulas). Desde la 1.5 llevan los nombres cortos de Batocera y ES-DE ("snes", "megadrive",
// "mastersystem"...), los mismos que se recomiendan para el USB. Se mudan una vez (estructura.txt dice que ya se hizo):
// las carpetas de juegos, guardados y carátulas, las rutas del índice y las claves de favoritos y recientes.
static const char* const CARPETA_ANTES[NUM_SISTEMAS] = {
    "SNES", "NES", "GB", "GBC", "GBA", "NGP", "NGPC", "MD", "A2600", "A7800", "LYNX", "SMS", "GG", "ARCADE", "NEOGEO",
    "CPS1", "CPS2", "CPS3", "N64"};
static const int ESTRUCTURA = 3;

// Mueve los archivos (no las carpetas) de 'de' a 'a', sin pisar los que ya estén.
static void AlListarArchivo(const char* nombre, bool carpeta, void* u) { if (!carpeta) AlListarNombre(nombre, false, u); }
static void MoverArchivos(const char* de, const char* a)
{
    Nombres l = {};
    ListarCarpeta(de, AlListarArchivo, &l);
    if (l.n) CrearCarpetas(a);
    for (int i = 0; i < l.n; i++)
    {
        char x[600], y[600];
        snprintf(x, sizeof x, "%s/%s", de, l.v[i]);
        snprintf(y, sizeof y, "%s/%s", a, l.v[i]);
        if (!ExisteArchivo(y) && RenombrarArchivo(x, y)) migrados++;
        else sinMover++;
    }
    SoltarNombres(l);
}

// Renombra la carpeta 'de' a 'a'. En un disco que no distingue mayúsculas ("NES" y "nes" son la misma) se pasa por un
// nombre intermedio. Si 'a' ya existe, se juntan.
static void MoverCarpeta(const char* de, const char* a)
{
    Nombres hay = {};
    if (ListarCarpeta(de, AlListarNombre, &hay) < 0) return;
    SoltarNombres(hay);
    char medio[600];
    snprintf(medio, sizeof medio, "%s.mudanza", de);
    if (RenombrarArchivo(de, medio))
    {
        if (RenombrarArchivo(medio, a)) { migrados++; return; }
        Juntar(medio, a, 1);
    }
    else Juntar(de, a, 1);
}

// La clave de un favorito o reciente con el nombre de carpeta nuevo ("NES/x" → "nes/x"; la SNES no llevaba: "x" →
// "snes/x"). Las que ya son nuevas quedan igual.
static void ClaveNueva(const char* clave, char* sale, int max)
{
    const char* barra = strchr(clave, '/');
    if (barra)
        for (int s = 0; s < NUM_SISTEMAS; s++)
        {
            size_t l = (size_t)(barra - clave);
            if (l == strlen(ConsolaDe(s).carpeta) && !strncmp(clave, ConsolaDe(s).carpeta, l)) { snprintf(sale, max, "%s", clave); return; }
            if (l == strlen(CARPETA_ANTES[s]) && !strncmp(clave, CARPETA_ANTES[s], l)) { snprintf(sale, max, "%s%s", ConsolaDe(s).carpeta, barra); return; }
        }
    snprintf(sale, max, "%s/%s", ConsolaDe(SIS_SNES).carpeta, clave);
}

static void MigrarLista(const char* archivo)
{
    char r[160];
    snprintf(r, sizeof r, "%s/%s", CARPETA_DATOS, archivo);
    static char txt[64 * 1024], sale[80 * 1024];
    if (LeerTexto(r, txt, sizeof txt) <= 0) return;
    int k = 0;
    for (char* p = txt; *p && k < (int)sizeof sale - 300;)
    {
        char* fin = strchr(p, '\n');
        if (fin) *fin = 0;
        if (*p)
        {
            char c[200];
            ClaveNueva(p, c, sizeof c);
            k += snprintf(sale + k, sizeof sale - k, "%s\n", c);
        }
        if (!fin) break;
        p = fin + 1;
    }
    sale[k] = 0;
    EscribirTexto(r, sale);
}

static void MigrarEstructura()
{
    char ruta[160], t[16] = "";
    snprintf(ruta, sizeof ruta, "%s/estructura.txt", CARPETA_DATOS);
    int version = LeerTexto(ruta, t, sizeof t) > 0 ? atoi(t) : 1;
    if (version >= ESTRUCTURA) return;
    migrados = sinMover = 0;
    char de[200], a[200];
    // 3: los juegos copiados a la consola van en roms/ (como en el USB y en Batocera), no en juegos/.
    snprintf(de, sizeof de, "%s/juegos", CARPETA_DATOS);
    snprintf(a, sizeof a, "%s/roms", CARPETA_DATOS);
    MoverCarpeta(de, a);
    {
        char viejo[64], nuevo[64];
        snprintf(viejo, sizeof viejo, "%s/juegos/", CARPETA_DATOS);
        snprintf(nuevo, sizeof nuevo, "%s/roms/", CARPETA_DATOS);
        const char* v[1] = {viejo};
        const char* nu[1] = {nuevo};
        CambiarEnArchivo("biblioteca.txt", v, nu, 1);
    }
    // 2: las carpetas de cada consola con los nombres cortos de Batocera (antes "SNES", "MD"...; la SNES, sin carpeta).
    if (version < 2)
    {
        static const char* const ZONAS[] = {"roms", "guardados", "caratulas"};
        for (const char* z : ZONAS)
            for (int s = 0; s < NUM_SISTEMAS; s++)
            {
                snprintf(de, sizeof de, "%s/%s/%s", CARPETA_DATOS, z, CARPETA_ANTES[s]);
                snprintf(a, sizeof a, "%s/%s/%s", CARPETA_DATOS, z, ConsolaDe(s).carpeta);
                if (strcmp(de, a)) MoverCarpeta(de, a);
            }
        // La SNES estaba en la raíz de guardados y de carátulas (la lista de carátulas no encontradas se rehace sola).
        snprintf(de, sizeof de, "%s/caratulas/no_encontradas.txt", CARPETA_DATOS);
        BorrarArchivo(de);
        snprintf(de, sizeof de, "%s/guardados", CARPETA_DATOS);
        snprintf(a, sizeof a, "%s/guardados/%s", CARPETA_DATOS, ConsolaDe(SIS_SNES).carpeta);
        MoverArchivos(de, a);
        snprintf(de, sizeof de, "%s/caratulas", CARPETA_DATOS);
        snprintf(a, sizeof a, "%s/caratulas/%s", CARPETA_DATOS, ConsolaDe(SIS_SNES).carpeta);
        MoverArchivos(de, a);
        // Las rutas de los juegos copiados a la consola en el índice de la biblioteca.
        static char viejos[NUM_SISTEMAS][48], nuevos[NUM_SISTEMAS][48];
        const char* v[NUM_SISTEMAS];
        const char* nu[NUM_SISTEMAS];
        for (int s = 0; s < NUM_SISTEMAS; s++)
        {
            snprintf(viejos[s], 48, "/roms/%s/", CARPETA_ANTES[s]);
            snprintf(nuevos[s], 48, "/roms/%s/", ConsolaDe(s).carpeta);
            v[s] = viejos[s];
            nu[s] = nuevos[s];
        }
        CambiarEnArchivo("biblioteca.txt", v, nu, NUM_SISTEMAS);
        MigrarLista("favoritos.txt");
        MigrarLista("recientes.txt");
    }
    char hecho[16];
    snprintf(hecho, sizeof hecho, "%d\n", ESTRUCTURA);
    EscribirTexto(ruta, hecho);
    size_t l = strlen(migracion);
    snprintf(migracion + l, sizeof migracion - l, "%sCarpetas de la consola al formato nuevo (roms/<consola>): %d movidas, %d quedaron.",
             l ? " " : "", migrados, sinMover);
}

// ---------- ordenar los juegos sueltos ----------
// Los copiados por versiones anteriores quedaron sueltos en roms/ (antes no había una carpeta por consola). Cada uno va a
// la carpeta de su consola: la que dice el índice de la biblioteca; si no está en el índice, la del arcade (los juegos
// padre y las BIOS de los arcade, como neogeo.zip) o la que dice su extensión (o la del archivo de adentro, si es un
// comprimido). Lo que no se reconoce queda donde estaba.
struct Suelto { char nombre[256]; };

static int SistemaDelIndice(const char* indice, const char* ruta)
{
    // Cada línea termina en la ruta (último campo) y empieza con el número de la consola.
    size_t lr = strlen(ruta);
    for (const char* p = indice; p && *p;)
    {
        const char* fin = strchr(p, '\n');
        size_t largo = fin ? (size_t)(fin - p) : strlen(p);
        if (largo > lr && p[largo - lr - 1] == '\t' && !strncmp(p + largo - lr, ruta, lr)) return atoi(p);
        p = fin ? fin + 1 : NULL;
    }
    return -1;
}

static int SistemaDeArchivo(const char* ruta, const char* nombre)
{
    if (EsComprimido(nombre))
    {
        if (ArcadeDeRuta(ruta) >= 0) return SIS_ARCADE;
        static EntradaComprimido e[16];
        int n = ComprimidoListarRuta(ruta, e, 16);
        for (int i = 0; i < n; i++)
            if (const Consola* c = ConsolaParaRuta(e[i].nombre)) return c->sistema;
        return -1;
    }
    const Consola* c = ConsolaParaRuta(nombre);
    return c ? c->sistema : -1;
}

// Los archivos de diagnóstico que versiones anteriores dejaban sueltos en /data/allesiu, a /data/allesiu/diagnostico.
static void OrdenarDiagnostico()
{
    static const char* const SUELTOS[] = {"cierre.txt", "foto-en-juego.txt", "prueba_jit.txt", "prueba_jit2.txt",
                                          "prueba_gpu.txt", "prueba_gpu2.txt", "prueba_gpu3.txt"};
    CrearCarpetas(CARPETA_DIAGNOSTICO);
    for (const char* n : SUELTOS)
    {
        char de[200], a[200];
        snprintf(de, sizeof de, "%s/%s", CARPETA_DATOS, n);
        snprintf(a, sizeof a, "%s/%s", CARPETA_DIAGNOSTICO, n);
        if (ExisteArchivo(de) && !ExisteArchivo(a)) RenombrarArchivo(de, a);
    }
}

void MudanzaOrdenarRoms()
{
    char raiz[160];
    snprintf(raiz, sizeof raiz, "%s/roms", CARPETA_DATOS);
    Nombres l = {};
    ListarCarpeta(raiz, AlListarArchivo, &l);
    if (!l.n) { SoltarNombres(l); return; }
    // El índice de la biblioteca, para saber de qué consola es cada uno (ya analizado al buscar).
    char ri[160];
    snprintf(ri, sizeof ri, "%s/biblioteca.txt", CARPETA_DATOS);
    uint64_t tam = 0;
    char* indice = NULL;
    if (ExisteArchivo(ri, &tam) && tam && tam < 64u << 20 && (indice = (char*)malloc((size_t)tam + 1)))
        if (LeerTexto(ri, indice, (int)tam + 1) <= 0) { free(indice); indice = NULL; }
    // Pares de rutas (vieja → nueva) para corregir el índice al final.
    char (*viejas)[300] = (char(*)[300])calloc(l.n, 300);
    char (*nuevas)[300] = (char(*)[300])calloc(l.n, 300);
    int movidos = 0, quedan = 0;
    for (int i = 0; i < l.n && viejas && nuevas; i++)
    {
        char de[600];
        snprintf(de, sizeof de, "%s/%s", raiz, l.v[i]);
        int s = indice ? SistemaDelIndice(indice, de) : -1;
        if (s < 0) s = SistemaDeArchivo(de, l.v[i]);
        if (s < 0 || s >= NUM_SISTEMAS) { quedan++; continue; }
        // Los arcade (también Neo Geo y CPS) van todos juntos: cada juego necesita a su juego padre y su BIOS al lado.
        const char* sub = EsArcade(s) ? ConsolaDe(SIS_ARCADE).carpeta : ConsolaDe(s).carpeta;
        char carpeta[400], a[700];
        snprintf(carpeta, sizeof carpeta, "%s/%s", raiz, sub);
        CrearCarpetas(carpeta);
        snprintf(a, sizeof a, "%s/%s", carpeta, l.v[i]);
        if (ExisteArchivo(a) || !RenombrarArchivo(de, a)) { quedan++; continue; }
        // El índice guarda la ruta como último campo de la línea: con el salto de línea, no se confunde con otra.
        snprintf(viejas[movidos], 300, "%s\n", de);
        snprintf(nuevas[movidos], 300, "%s\n", a);
        movidos++;
    }
    if (movidos)
    {
        const char** v = (const char**)malloc(sizeof(char*) * movidos);
        const char** nu = (const char**)malloc(sizeof(char*) * movidos);
        if (v && nu)
        {
            for (int i = 0; i < movidos; i++) { v[i] = viejas[i]; nu[i] = nuevas[i]; }
            CambiarEnArchivo("biblioteca.txt", v, nu, movidos);
        }
        free(v);
        free(nu);
        size_t ml = strlen(migracion);
        snprintf(migracion + ml, sizeof migracion - ml, "%sJuegos de la consola ordenados en roms/<consola>: %d movidos, %d sin reconocer.",
                 ml ? " " : "", movidos, quedan);
    }
    free(viejas);
    free(nuevas);
    free(indice);
    SoltarNombres(l);
}

void Mudanza()
{
    MigrarDatos();                  // /data/snes-ps4 de las versiones anteriores → /data/allesiu
    CrearCarpetas(CARPETA_DATOS);   // /data/allesiu (guardados, carátulas, ajustes)
    MigrarEstructura();             // carpetas de cada consola con los nombres de Batocera ("snes", "megadrive"...)
    OrdenarDiagnostico();          // cierres y pruebas, a /data/allesiu/diagnostico
}

const char* MudanzaResumen() { return migracion; }
