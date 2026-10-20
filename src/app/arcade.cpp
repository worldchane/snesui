// Ver arcade.h. La tabla se lee entera a memoria (unos 4 MB) la primera vez que hace falta y se busca por nombre con
// búsqueda binaria (viene ordenada).
#include "arcade.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"
#include "../nucleos/comprimido.h"
#include "consola.h"
#include "nucleo_libretro.h"

#include <atomic>

#ifndef RUTA_ASSETS
#define RUTA_ASSETS "/app0/assets"
#endif

#pragma pack(push, 1)
struct JuegoArcade { uint32_t nombre, titulo, fabricante; int32_t padre, bios; uint16_t anio; uint8_t marcas, cero; uint32_t primero, cuantos; };
struct ArchivoArcade { uint32_t nombre, tam, crc; uint8_t marcas, cero[3]; };
#pragma pack(pop)
static_assert(sizeof(JuegoArcade) == 32 && sizeof(ArchivoArcade) == 16, "formato de arcade.bin");

static uint8_t* datos;
static const JuegoArcade* juegos;
static const ArchivoArcade* archivos;
static const char* texto;
static uint32_t nJuegos, nArchivos, tamTexto;
static bool intentado;

bool ArcadeIniciar()
{
    if (intentado) return datos != NULL;
    intentado = true;
    const char* ruta = RUTA_ASSETS "/misc/arcade.bin";
    uint64_t tam = 0;
    if (!ExisteArchivo(ruta, &tam) || tam < 28 || tam > 64u * 1024 * 1024) return false;
    uint8_t* d = (uint8_t*)malloc((size_t)tam);
    FILE* f = AbrirArchivo(ruta, "rb");
    bool ok = d && f && fread(d, 1, (size_t)tam, f) == tam;
    if (f) fclose(f);
    if (!ok || memcmp(d, "ALLESIU-ARCADE1", 16)) { free(d); return false; }
    memcpy(&nJuegos, d + 16, 4);
    memcpy(&nArchivos, d + 20, 4);
    memcpy(&tamTexto, d + 24, 4);
    uint64_t esperado = 28 + (uint64_t)nJuegos * sizeof(JuegoArcade) + (uint64_t)nArchivos * sizeof(ArchivoArcade) + tamTexto;
    if (esperado != tam) { free(d); return false; }
    datos = d;
    juegos = (const JuegoArcade*)(d + 28);
    archivos = (const ArchivoArcade*)(d + 28 + nJuegos * sizeof(JuegoArcade));
    texto = (const char*)(d + 28 + nJuegos * sizeof(JuegoArcade) + nArchivos * sizeof(ArchivoArcade));
    return true;
}

static const char* Texto(uint32_t off) { return off < tamTexto ? texto + off : ""; }
static bool Valido(int j) { return datos && j >= 0 && (uint32_t)j < nJuegos; }

int ArcadeBuscar(const char* nombre)
{
    if (!ArcadeIniciar()) return -1;
    int a = 0, b = (int)nJuegos - 1;
    while (a <= b)
    {
        int m = (a + b) / 2;
        int c = strcmp(Texto(juegos[m].nombre), nombre);
        if (!c) return m;
        if (c < 0) a = m + 1; else b = m - 1;
    }
    return -1;
}

const char* ArcadeNombre(int j) { return Valido(j) ? Texto(juegos[j].nombre) : ""; }
const char* ArcadeTitulo(int j) { return Valido(j) ? Texto(juegos[j].titulo) : ""; }
const char* ArcadeFabricante(int j) { return Valido(j) ? Texto(juegos[j].fabricante) : ""; }
int ArcadeAnio(int j) { return Valido(j) ? juegos[j].anio : 0; }
int ArcadePadre(int j) { return Valido(j) ? juegos[j].padre : -1; }
int ArcadeBios(int j) { return Valido(j) ? juegos[j].bios : -1; }
bool ArcadeEsBios(int j) { return Valido(j) && (juegos[j].marcas & 1); }
bool ArcadeEsNeoGeo(int j) { return Valido(j) && (juegos[j].marcas & 2); }

const char* ArcadeEstadoTexto(int e)
{
    switch (e)
    {
    case ARC_COMPLETO: return "Completo";
    case ARC_FALTAN_ARCHIVOS: return "Faltan archivos";
    case ARC_FALTA_PADRE: return "Falta el juego principal";
    case ARC_FALTA_BIOS: return "Falta la BIOS";
    case ARC_OTRA_VERSION: return "Set de otra versión";
    case ARC_DESCONOCIDO: return "Juego desconocido";
    default: return "";
    }
}

// ¿Existe el zip (o 7z) de un juego en la carpeta 'carpeta' (o, si es una BIOS, en la carpeta de BIOS)?
// ---------- reconocer un juego por su contenido ----------
// FinalBurn Neo sabe qué juego es por el nombre corto del zip ("<nombre corto>.zip"). Muchas colecciones los traen con el
// título ("<Título del juego> (Neo Geo).zip"): se los reconoce por las huellas (CRC) de los archivos de adentro, que están en la tabla.
static uint64_t* porCrc;   // (crc << 32) | juego, ordenado: de cada huella, los juegos que la usan
static uint32_t nPorCrc;
static int CompararU64(const void* a, const void* b)
{
    uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
    return x < y ? -1 : x > y;
}
static bool ArmarPorCrc()
{
    if (porCrc) return true;
    if (!ArcadeIniciar()) return false;
    uint64_t* p = (uint64_t*)malloc(sizeof(uint64_t) * (nArchivos ? nArchivos : 1));
    if (!p) return false;
    uint32_t n = 0;
    for (uint32_t j = 0; j < nJuegos; j++)
        for (uint32_t k = 0; k < juegos[j].cuantos; k++)
        {
            const ArchivoArcade& a = archivos[juegos[j].primero + k];
            // Solo los archivos propios: los que vienen del juego principal o de la BIOS (marca 1) están en la lista de
            // cada juego que los usa (los de la BIOS de Neo Geo, en unos 250) y no dicen qué juego es.
            if (a.crc && !(a.marcas & 3)) p[n++] = ((uint64_t)a.crc << 32) | j;
        }
    qsort(p, n, sizeof(uint64_t), CompararU64);
    nPorCrc = n;
    porCrc = p;
    return true;
}

int ArcadeIdentificar(const char* ruta)
{
    static std::atomic_flag ocupado = ATOMIC_FLAG_INIT;   // la búsqueda y la apertura de un juego pueden llamarla a la vez
    while (ocupado.test_and_set(std::memory_order_acquire)) {}
    int elegido = -1;
    static const int MAX = 2048;
    EntradaComprimido* e = ArmarPorCrc() ? (EntradaComprimido*)malloc(sizeof(EntradaComprimido) * MAX) : NULL;
    int n = e ? ComprimidoListarRuta(ruta, e, MAX) : -1;
    static int cand[256], cuenta[256];
    int nc = 0;
    for (int i = 0; i < n; i++)
    {
        if (!e[i].crc) continue;
        // La primera huella igual (búsqueda binaria) y todas las que siguen.
        uint64_t clave = (uint64_t)e[i].crc << 32;
        uint32_t a = 0, b = nPorCrc;
        while (a < b) { uint32_t m = (a + b) / 2; if (porCrc[m] < clave) a = m + 1; else b = m; }
        for (uint32_t k = a; k < nPorCrc && (porCrc[k] >> 32) == e[i].crc; k++)
        {
            int j = (int)(porCrc[k] & 0xFFFFFFFF), c = 0;
            while (c < nc && cand[c] != j) c++;
            if (c == nc) { if (nc == 256) continue; cand[nc] = j; cuenta[nc++] = 0; }
            cuenta[c]++;
        }
    }
    // Entre los candidatos (los que tienen algún archivo propio en el zip), el que mejor lo explica: la mayor parte de
    // su lista completa (propios y heredados del juego principal) está en el zip y casi nada del zip le sobra. Así una
    // versión (con sus archivos y los del original) gana al original, al que le faltan los que la versión cambia; a
    // igual, el juego principal (un zip "merged" trae todas las versiones).
    static uint32_t enZip[2048], deJuego[512];
    int nz = 0;
    for (int i = 0; i < n; i++) if (e[i].crc) enZip[nz++] = e[i].crc;
    qsort(enZip, nz, sizeof(uint32_t), [](const void* a, const void* b) {
        uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
        return x < y ? -1 : x > y;
    });
    auto Esta = [](const uint32_t* v, int n, uint32_t crc) {
        int a = 0, b = n;
        while (a < b) { int m = (a + b) / 2; if (v[m] < crc) a = m + 1; else b = m; }
        return a < n && v[a] == crc;
    };
    double mejor = -1;
    for (int c = 0; c < nc; c++)
    {
        const JuegoArcade& g = juegos[cand[c]];
        int total = 0, presentes = 0, nj = 0;
        for (uint32_t k = 0; k < g.cuantos; k++)
        {
            const ArchivoArcade& a = archivos[g.primero + k];
            if ((a.marcas & 2) || !a.crc) continue;
            total++;
            if (Esta(enZip, nz, a.crc)) presentes++;
            if (nj < 512) deJuego[nj++] = a.crc;
        }
        if (!total || presentes * 2 < total) continue;
        qsort(deJuego, nj, sizeof(uint32_t), [](const void* a, const void* b) {
            uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
            return x < y ? -1 : x > y;
        });
        int sobran = 0;
        for (int i = 0; i < nz; i++) if (!Esta(deJuego, nj, enZip[i])) sobran++;
        double puntaje = (double)presentes / total - .5 * sobran / (nz ? nz : 1) + (g.padre < 0 ? 1e-6 : 0);
        if (puntaje > mejor) { mejor = puntaje; elegido = cand[c]; }
    }
    free(e);
    ocupado.clear(std::memory_order_release);
    return elegido;
}

int ArcadeDeRuta(const char* ruta)
{
    int j = ArcadeBuscar(ArcadeNombreDeRuta(ruta));
    return j >= 0 ? j : ArcadeIdentificar(ruta);
}

// El zip (o 7z) del juego 'j' en 'carpeta': con su nombre corto o, si no, uno de otro nombre que sea ese juego (la
// carpeta se recorre una vez y se recuerda). Las BIOS también en /data/allesiu/bios. Deja la ruta en 'sale'.
struct RecorridoArcade { int n; char nombres[256][200]; };
static void AlListarArcade(const char* nombre, bool esCarpeta, void* u)
{
    RecorridoArcade& r = *(RecorridoArcade*)u;
    if (esCarpeta || r.n == 256 || !EsComprimido(nombre)) return;
    snprintf(r.nombres[r.n++], sizeof r.nombres[0], "%s", nombre);
}

static bool BuscarZipDe(const char* carpeta, int j, bool bios, char* sale, int max)
{
    static const char* const EXT[] = {"zip", "7z"};
    for (const char* e : EXT)
    {
        snprintf(sale, max, "%s/%s.%s", carpeta, ArcadeNombre(j), e);
        if (ExisteArchivo(sale)) return true;
        if (bios)
        {
            snprintf(sale, max, "%s/bios/%s.%s", CARPETA_DATOS, ArcadeNombre(j), e);
            if (ExisteArchivo(sale)) return true;
        }
    }
    static std::atomic_flag ocupado = ATOMIC_FLAG_INIT;
    while (ocupado.test_and_set(std::memory_order_acquire)) {}
    struct Visto { int juego; char ruta[600]; };
    static Visto vistos[256];
    static int nVistos;
    static char recorrida[600];
    if (strcmp(recorrida, carpeta))
    {
        snprintf(recorrida, sizeof recorrida, "%s", carpeta);
        nVistos = 0;
        static RecorridoArcade r;
        r.n = 0;
        ListarCarpeta(carpeta, AlListarArcade, &r);
        for (int i = 0; i < r.n && nVistos < 256; i++)
        {
            char ruta[600];
            snprintf(ruta, sizeof ruta, "%s/%s", carpeta, r.nombres[i]);
            if (ArcadeBuscar(ArcadeNombreDeRuta(ruta)) >= 0) continue;   // ya tiene nombre corto: lo encontró arriba
            int id = ArcadeIdentificar(ruta);
            if (id < 0) continue;
            vistos[nVistos].juego = id;
            snprintf(vistos[nVistos++].ruta, sizeof vistos[0].ruta, "%s", ruta);
        }
    }
    bool hay = false;
    for (int i = 0; i < nVistos && !hay; i++)
        if (vistos[i].juego == j) { snprintf(sale, max, "%s", vistos[i].ruta); hay = true; }
    ocupado.clear(std::memory_order_release);
    return hay;
}

bool ArcadeZipDe(const char* carpeta, int j, char* sale, int max) { return BuscarZipDe(carpeta, j, false, sale, max); }

static bool HayZip(const char* carpeta, int j, bool bios)
{
    char r[700];
    return BuscarZipDe(carpeta, j, bios, r, sizeof r);
}

bool ArcadePrepararAlias(const char* ruta, char* rutaNucleo, int max)
{
    LibretroAliasLimpiar();
    snprintf(rutaNucleo, max, "%s", ruta);
    int j = ArcadeDeRuta(ruta);
    if (j < 0) return false;
    char carpeta[600];
    snprintf(carpeta, sizeof carpeta, "%s", ruta);
    char* barra = strrchr(carpeta, '/');
    if (!barra) return false;
    *barra = 0;
    const char* ext = strrchr(ruta, '.');
    // El juego: con su nombre corto (si ya lo tiene, queda igual).
    snprintf(rutaNucleo, max, "%s/%s%s", carpeta, ArcadeNombre(j), ext ? ext : ".zip");
    if (strcmp(rutaNucleo, ruta)) LibretroAlias(rutaNucleo, ruta);
    // El juego principal y las BIOS que tengan otro nombre en la misma carpeta.
    int padre = ArcadePadre(j);
    const int extra[3] = {padre, ArcadeBios(j), padre >= 0 ? ArcadeBios(padre) : -1};
    for (int e : extra)
    {
        if (e < 0) continue;
        char real[600], virt[700];
        if (!BuscarZipDe(carpeta, e, true, real, sizeof real)) continue;
        const char* x = strrchr(real, '.');
        snprintf(virt, sizeof virt, "%s/%s%s", carpeta, ArcadeNombre(e), x ? x : ".zip");
        if (strcmp(virt, real) && strncmp(real, CARPETA_DATOS, strlen(CARPETA_DATOS))) LibretroAlias(virt, real);
    }
    return true;
}

int ArcadeRevisar(const char* ruta, char* explicacion)
{
    explicacion[0] = 0;
    if (!ArcadeIniciar()) return ARC_SIN_REVISAR;
    // El nombre del juego es el del archivo sin la carpeta ni la extensión.
    char carpeta[600], nombre[128];
    snprintf(carpeta, sizeof carpeta, "%s", ruta);
    char* barra = strrchr(carpeta, '/');
    char* barra2 = strrchr(carpeta, 0x5C);   // la barra invertida (rutas de Windows, en el simulador)
    if (barra2 > barra) barra = barra2;
    snprintf(nombre, sizeof nombre, "%s", barra ? barra + 1 : ruta);
    if (barra) *barra = 0; else snprintf(carpeta, sizeof carpeta, ".");
    char* punto = strrchr(nombre, '.');
    if (punto) *punto = 0;
    int j = ArcadeBuscar(nombre);
    if (j < 0) j = ArcadeIdentificar(ruta);   // otro nombre: por lo que tiene adentro
    if (j < 0)
    {
        snprintf(explicacion, 160, "\"%s\" no es un juego de esta versión del emulador arcade (FinalBurn Neo).", nombre);
        return ARC_DESCONOCIDO;
    }
    static const int MAX = 2048;
    EntradaComprimido* e = (EntradaComprimido*)malloc(sizeof(EntradaComprimido) * MAX);
    int n = e ? ComprimidoListarRuta(ruta, e, MAX) : -1;
    if (n < 0) { free(e); snprintf(explicacion, 160, "No se pudo leer el archivo comprimido."); return ARC_FALTAN_ARCHIVOS; }
    // Cada archivo que el juego necesita: está en su zip (por la huella), o viene del padre o la BIOS (y su zip está).
    const JuegoArcade& g = juegos[j];
    int faltan = 0, propios = 0, encontrados = 0;
    bool necesitaPadre = false, necesitaBios = false;
    const char* primeroQueFalta = NULL;
    for (uint32_t k = 0; k < g.cuantos; k++)
    {
        const ArchivoArcade& a = archivos[g.primero + k];
        if (a.marcas & 2) continue;   // sin volcado: no se exige
        bool esta = false;
        for (int i = 0; i < n && !esta; i++) esta = e[i].crc == a.crc && e[i].tam == a.tam;
        if (esta) { encontrados++; continue; }
        if (a.marcas & 1)
        {
            // Viene del padre o de la BIOS: alcanza con que su zip esté (FinalBurn Neo lo busca ahí).
            if (g.padre >= 0) necesitaPadre = true; else necesitaBios = true;
            continue;
        }
        propios++;
        faltan++;
        if (!primeroQueFalta) primeroQueFalta = Texto(a.nombre);
    }
    free(e);
    if (faltan)
    {
        // Si casi nada coincide, seguro es un set armado para otra versión (de MAME o de FinalBurn Neo).
        if (encontrados * 4 < (int)g.cuantos)
        {
            snprintf(explicacion, 160, "Este zip es de otra versión del juego (otro emulador): casi ningún archivo coincide. Hay que rearmarlo para FinalBurn Neo.");
            return ARC_OTRA_VERSION;
        }
        snprintf(explicacion, 160, "Le falta%s %d archivo%s (por ejemplo \"%s\").", faltan == 1 ? "" : "n", faltan, faltan == 1 ? "" : "s",
                 primeroQueFalta);
        return ARC_FALTAN_ARCHIVOS;
    }
    if (necesitaPadre && !HayZip(carpeta, g.padre, false))
    {
        snprintf(explicacion, 160, "Es una versión de \"%s\": pon %s.zip en la misma carpeta.", ArcadeTitulo(g.padre), ArcadeNombre(g.padre));
        return ARC_FALTA_PADRE;
    }
    int bios = g.bios;
    if ((necesitaBios || bios >= 0) && bios >= 0 && !HayZip(carpeta, bios, true))
    {
        snprintf(explicacion, 160, "Necesita la BIOS %s.zip: ponla en la misma carpeta o en /data/allesiu/bios.", ArcadeNombre(bios));
        return ARC_FALTA_BIOS;
    }
    (void)propios;
    return ARC_COMPLETO;
}

const char* ArcadeNombreDeRuta(const char* ruta)
{
    static char n[128];
    const char* b = strrchr(ruta, '/');
    const char* b2 = strrchr(ruta, 0x5C);   // la barra invertida (rutas de Windows, en el simulador)
    if (b2 > b) b = b2;
    snprintf(n, sizeof n, "%s", b ? b + 1 : ruta);
    char* p = strrchr(n, '.');
    if (p) *p = 0;
    return n;
}

int ArcadePlaca(int j)
{
    if (!Valido(j)) return SIS_ARCADE;
    uint8_t m = juegos[j].marcas;
    return (m & 2) ? SIS_NEOGEO : (m & 4) ? SIS_CPS1 : (m & 8) ? SIS_CPS2 : (m & 16) ? SIS_CPS3 : SIS_ARCADE;
}
