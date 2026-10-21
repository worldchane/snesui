// Ver consola.h. Cada núcleo libretro se compila solo si construir.sh lo incluye (define CON_NUCLEO_<nombre>); las
// consolas sin núcleo siguen en la tabla (para las pestañas y carpetas) pero no se pueden abrir.
#include "consola.h"
#include "ajustes.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"
#include "../nucleo/snes.h"
#include "../nucleos/comprimido.h"
#include "nucleo_libretro.h"
#include "registro.h"

// ---------- SNES: el núcleo propio ----------
static int SnesCuadroConsola(uint32_t* imagen, int* ancho, int* alto, int16_t* audio, int maxAudio)
{
    *ancho = SNES_ANCHO;
    *alto = SNES_ALTO;
    return SnesCuadro(imagen, audio, maxAudio);
}
static int SnesFrecuencia() { return 32000; }
static bool SnesCargarConsola(const char*, const char*, uint8_t* d, uint32_t n, char* e) { return SnesCargarMemoria(d, n, e); }

// ---------- núcleos libretro ----------
// NUCLEO(prefijo, nombre) declara las funciones de un núcleo enlazado con ese prefijo (ver nucleos/prefijo.h) y sus dos
// formas de cargar una rom (del archivo o ya en memoria, sacada de un comprimido).
#define NUCLEO(p, nombre)                                                                                                   API_LIBRETRO_DECLARAR(p##_)                                                                                            static const ApiLibretro api_##p = API_LIBRETRO_TABLA(p##_);                                                           static bool Cargar_##p(const char* r, char* e) { Iniciar_##p(); return LibretroCargar(&api_##p, nombre, r, e); }       static bool CargarMemoria_##p(const char* r, const char* i, uint8_t* d, uint32_t n, char* e)                           {                                                                                                                          Iniciar_##p();                                                                                                         return LibretroCargarMemoria(&api_##p, nombre, r, i, d, n, e);                                                     }

#ifdef CON_NUCLEO_GB
// Lo que SameBoy hacía solo al abrir el programa (ver nucleos.sh): sus tablas de sonido y la semilla al azar.
extern "C" void SameBoyIniciar_band_limited_init(void);
extern "C" void SameBoyIniciar_init_seed(void);
static void Iniciar_gb()
{
    static bool iniciado;
    if (iniciado) return;
    SameBoyIniciar_band_limited_init();
    SameBoyIniciar_init_seed();
    iniciado = true;
}
NUCLEO(gb, "SameBoy")
#endif
#ifdef CON_NUCLEO_GBT
static void Iniciar_gbt() {}
NUCLEO(gbt, "Gambatte")
#endif
#ifdef CON_NUCLEO_NES
static void Iniciar_nes() {}
NUCLEO(nes, "FCEUmm")
#endif
#ifdef CON_NUCLEO_MD
static void Iniciar_md() {}
NUCLEO(md, "Genesis Plus GX")
#endif
#ifdef CON_NUCLEO_GBA
static void Iniciar_gba() {}
NUCLEO(gba, "mGBA")
#endif
#ifdef CON_NUCLEO_NGP
static void Iniciar_ngp() {}
NUCLEO(ngp, "Beetle NeoPop")
#endif
#ifdef CON_NUCLEO_A78
static void Iniciar_a78() {}
NUCLEO(a78, "ProSystem")
#endif
#ifdef CON_NUCLEO_LYNX
static void Iniciar_lynx() {}
NUCLEO(lynx, "Handy")
#endif
#ifdef CON_NUCLEO_S9X
// Snes9x: el emulador de SNES de la app (el propio, en src/nucleo, queda como alternativa). La aceleración del Super FX
// de Configuración (×1 a ×4) va a su opción de frecuencia del chip.
static void Iniciar_s9x()
{
    static const char* const VELOCIDAD[] = {"100%", "200%", "300%", "400%"};
    int v = General().superfx;
    LibretroOpcion("snes9x_overclock_superfx", VELOCIDAD[v >= 1 && v <= 4 ? v - 1 : 0]);
}
NUCLEO(s9x, "Snes9x")
#endif
#ifdef CON_NUCLEO_N64
// Mupen64Plus-Next. Sus opciones: por ahora el dibujo por software (Angrylion) y el RSP de bajo nivel; el procesador,
// con el recompilador si la máquina quedó en su dirección fija (n64/memoria_n64.c) y si no, el intérprete en caché (en
// la PC se puede pedir otro con la variable de entorno N64_CPU).
extern "C" int n64_dispositivo_preparar(void);
static void Iniciar_n64()
{
    LibretroOpcion("mupen64plus-rdp-plugin", "angrylion");
    LibretroOpcion("mupen64plus-rsp-plugin", "cxd4");   // Angrylion necesita el RSP de bajo nivel
    // El dibujo en 4 hilos (los de la app: n64/parallel_allesiu.cpp; los de C++ cerraban la app en la consola).
    LibretroOpcion("mupen64plus-angrylion-multithread", "4");
    const char* cpu = getenv("N64_CPU");
    bool recompilador = n64_dispositivo_preparar() != 0;
    LibretroOpcion("mupen64plus-cpucore", cpu ? cpu : recompilador ? "dynamic_recompiler" : "cached_interpreter");
    Registrar("N64: procesador con %s", cpu ? cpu : recompilador ? "el recompilador" : "el intérprete (no se pudo ubicar la máquina)");
}
NUCLEO(n64, "Mupen64Plus-Next")
#endif
#ifdef CON_NUCLEO_ARC
// El arcade se abre por la ruta: FinalBurn Neo lee el zip (y los del juego padre y la BIOS) por su cuenta, y algunos
// sets pasan de los 64 MB que se cargan a memoria en las demás consolas.
API_LIBRETRO_DECLARAR(arc_)
static const ApiLibretro api_arc = API_LIBRETRO_TABLA(arc_);
// Agregada a la copia de su libretro.cpp (nucleos.sh): si al cargar se quedó en su pantalla de error (le faltan archivos
// al set), que la app no deja correr: pierde memoria cada vez y, tras varias, el núcleo se cae.
extern "C" bool arc_ALLESIU_ErrorFBNeo(void);
static bool Cargar_arc(const char* r, char* e)
{
    if (!LibretroCargarRuta(&api_arc, "FinalBurn Neo", r, e)) return false;
    if (!arc_ALLESIU_ErrorFBNeo()) return true;
    ConsolaDe(SIS_ARCADE).Descargar();
    if (LibretroFaltoMemoria()) snprintf(e, 160, "No alcanzó la memoria de la consola para abrir este juego.");
    else snprintf(e, 160, "FinalBurn Neo no pudo abrir este juego: a sus zips les faltan archivos para esta versión del emulador.");
    return false;
}
static bool CargarMemoria_arc(const char* r, const char*, uint8_t* d, uint32_t, char* e) { free(d); return Cargar_arc(r, e); }
#endif

static Consola consolas[NUM_SISTEMAS] = {
    {SIS_SNES, "SNES", "SNES", "snes", "sfc|smc|fig|swc", "Nintendo_-_Super_Nintendo_Entertainment_System", 4.f / 3},
    {SIS_NES, "NES", "NES", "nes", "nes|fds|unf|unif", "Nintendo_-_Nintendo_Entertainment_System", 4.f / 3},
    {SIS_GB, "GB", "Game Boy", "gb", "gb", "Nintendo_-_Game_Boy", 10.f / 9},
    {SIS_GBC, "GBC", "Game Boy Color", "gbc", "gbc", "Nintendo_-_Game_Boy_Color", 10.f / 9},
    {SIS_GBA, "GBA", "Game Boy Advance", "gba", "gba", "Nintendo_-_Game_Boy_Advance", 3.f / 2},
    {SIS_NGP, "NGP", "Neo Geo Pocket", "ngp", "ngp", "SNK_-_Neo_Geo_Pocket", 20.f / 19},
    {SIS_NGPC, "NGPC", "Neo Geo Pocket Color", "ngpc", "ngc|ngpc", "SNK_-_Neo_Geo_Pocket_Color", 20.f / 19},
    {SIS_MD, "MD", "Mega Drive / Genesis", "megadrive", "md|gen|smd|bin", "Sega_-_Mega_Drive_-_Genesis", 4.f / 3},
    {SIS_A2600, "2600", "Atari 2600", "atari2600", "a26", "Atari_-_2600", 4.f / 3},
    {SIS_A7800, "7800", "Atari 7800", "atari7800", "a78", "Atari_-_7800", 4.f / 3},
    {SIS_LYNX, "LYNX", "Atari Lynx", "lynx", "lnx", "Atari_-_Lynx", 160.f / 102},
    {SIS_SMS, "SMS", "Master System", "mastersystem", "sms", "Sega_-_Master_System_-_Mark_III", 4.f / 3},
    {SIS_GG, "GG", "Game Gear", "gamegear", "gg", "Sega_-_Game_Gear", 10.f / 9},
    {SIS_ARCADE, "ARCADE", "Arcade", "arcade", "", "FBNeo_-_Arcade_Games", 4.f / 3},
    {SIS_NEOGEO, "NEO GEO", "Neo Geo", "neogeo", "", "FBNeo_-_Arcade_Games", 4.f / 3},
    {SIS_CPS1, "CPS1", "Capcom CPS1", "cps1", "", "FBNeo_-_Arcade_Games", 4.f / 3},
    {SIS_CPS2, "CPS2", "Capcom CPS2", "cps2", "", "FBNeo_-_Arcade_Games", 4.f / 3},
    {SIS_CPS3, "CPS3", "Capcom CPS3", "cps3", "", "FBNeo_-_Arcade_Games", 4.f / 3},
    {SIS_N64, "N64", "Nintendo 64", "n64", "n64|z64|v64", "Nintendo_-_Nintendo_64", 4.f / 3},
};
static bool armadas;

static void Armar()
{
    if (armadas) return;
    armadas = true;
    Consola& s = consolas[SIS_SNES];
    s.presente = true;
    s.Cargar = SnesCargar;
    s.CargarMemoria = SnesCargarConsola;
    s.Descargar = SnesDescargar;
    s.Reiniciar = SnesReiniciar;
    s.Cuadro = SnesCuadroConsola;
    s.Frecuencia = SnesFrecuencia;
    s.Mando = SnesMando;
    s.MandoConectado = SnesMandoConectado;
    s.SramTam = SnesSramTam;
    s.SramDatos = SnesSramDatos;
    s.SramCambiada = SnesSramCambiada;
    s.EstadoTam = SnesEstadoTam;
    s.GuardarEstado = SnesGuardarEstado;
    s.CargarEstado = SnesCargarEstado;
    s.Diagnostico = SnesDiagnostico;
    s.Anomalia = SnesAnomalia;
// Las consolas que corre cada núcleo.
#define USAR(p, ...)                                                                                                        {                                                                                                                          static const int ks[] = {__VA_ARGS__};                                                                                 for (int k : ks)                                                                                                       {                                                                                                                          LibretroLlenar(consolas[k]);                                                                                           consolas[k].Cargar = Cargar_##p;                                                                                       consolas[k].CargarMemoria = CargarMemoria_##p;                                                                         consolas[k].presente = true;                                                                                       }                                                                                                                  }
#ifdef CON_NUCLEO_GB
    USAR(gb, SIS_GB, SIS_GBC)
#endif
#ifdef CON_NUCLEO_GBT
    USAR(gbt, SIS_GB, SIS_GBC)   // si está Gambatte, GB y GBC van con él: SameBoy es más preciso pero en la PS4 no llega a 60
#endif
#ifdef CON_NUCLEO_NES
    USAR(nes, SIS_NES)
#endif
#ifdef CON_NUCLEO_MD
    USAR(md, SIS_MD, SIS_SMS, SIS_GG)
#endif
#ifdef CON_NUCLEO_GBA
    USAR(gba, SIS_GBA)
#endif
#ifdef CON_NUCLEO_NGP
    USAR(ngp, SIS_NGP, SIS_NGPC)
#endif
#ifdef CON_NUCLEO_A78
    USAR(a78, SIS_A7800)
#endif
#ifdef CON_NUCLEO_LYNX
    USAR(lynx, SIS_LYNX)
#endif
#ifdef CON_NUCLEO_N64
    USAR(n64, SIS_N64)
#endif
#ifdef CON_NUCLEO_S9X
    USAR(s9x, SIS_SNES)   // reemplaza al emulador propio (armado arriba) en la SNES
#endif
#ifdef CON_NUCLEO_ARC
    USAR(arc, SIS_ARCADE, SIS_NEOGEO, SIS_CPS1, SIS_CPS2, SIS_CPS3)
#endif
}

static int activa = SIS_SNES;
void ConsolaActivar(int sistema) { activa = sistema >= 0 && sistema < NUM_SISTEMAS ? sistema : SIS_SNES; }
const Consola& ConsolaActiva() { return ConsolaDe(activa); }

const Consola& ConsolaDe(int sistema)
{
    Armar();
    return consolas[sistema >= 0 && sistema < NUM_SISTEMAS ? sistema : SIS_SNES];
}

const Consola* ConsolaParaRuta(const char* ruta)
{
    Armar();
    const char* punto = strrchr(ruta, '.');
    if (!punto || strlen(punto + 1) > 8) return NULL;
    char ext[10];
    int n = 0;
    for (const char* p = punto + 1; *p && n < 9; p++) ext[n++] = (char)tolower((unsigned char)*p);
    ext[n] = 0;
    for (const Consola& c : consolas)
    {
        if (!c.extensiones[0]) continue;   // el arcade se reconoce por la carpeta, no por la extensión
        // Busca 'ext' entre las extensiones separadas por '|'.
        const char* e = c.extensiones;
        size_t l = strlen(ext);
        while (*e)
        {
            const char* fin = strchr(e, '|');
            size_t len = fin ? (size_t)(fin - e) : strlen(e);
            if (len == l && !strncmp(e, ext, l)) return &c;
            if (!fin) break;
            e = fin + 1;
        }
    }
    return NULL;
}

// ---------- roms comprimidas ----------
static bool TieneExtension(const char* ruta, const char* ext)
{
    const char* p = strrchr(ruta, '.');
    if (!p) return false;
    for (p++; *p && *ext; p++, ext++) if (tolower((unsigned char)*p) != *ext) return false;
    return !*p && !*ext;
}

bool EsComprimido(const char* ruta) { return TieneExtension(ruta, "zip") || TieneExtension(ruta, "7z") || TieneExtension(ruta, "rar"); }

// El comprimido entero en memoria (malloc). Las roms comprimidas de estas consolas son chicas; se pone un tope.
static uint8_t* LeerComprimido(const char* ruta, size_t* n)
{
    uint64_t tam = 0;
    if (!ExisteArchivo(ruta, &tam) || tam < 8 || tam > 128u * 1024 * 1024) return NULL;
    uint8_t* d = (uint8_t*)malloc((size_t)tam);
    FILE* f = AbrirArchivo(ruta, "rb");
    if (!d || !f || fread(d, 1, (size_t)tam, f) != tam) { if (f) fclose(f); free(d); return NULL; }
    fclose(f);
    *n = (size_t)tam;
    return d;
}

// La primera entrada que sea una rom de una consola presente; -1 si ninguna.
static const int MAX_ENTRADAS = 256;
static int Elegir(const EntradaComprimido* e, int n, int* sistema)
{
    for (int i = 0; i < n; i++)
    {
        const Consola* c = ConsolaParaRuta(e[i].nombre);
        if (c && c->presente) { *sistema = c->sistema; return i; }
    }
    return -1;
}

int ConsolaDeComprimido(const char* ruta, char* interno, int max, uint64_t* tam)
{
    // Solo el índice del comprimido: la búsqueda pasa por cientos y no se lee ninguno entero.
    EntradaComprimido* e = (EntradaComprimido*)malloc(sizeof(EntradaComprimido) * MAX_ENTRADAS);
    int k = e ? ComprimidoListarRuta(ruta, e, MAX_ENTRADAS) : -1;
    int sistema = -1, i = k > 0 ? Elegir(e, k, &sistema) : -1;
    if (i >= 0)
    {
        if (interno) snprintf(interno, max, "%s", e[i].nombre);
        if (tam) *tam = e[i].tam;
    }
    free(e);
    return i >= 0 ? sistema : -1;
}

uint8_t* SacarDeComprimido(const char* ruta, char* interno, int max, uint32_t* tam, char* error)
{
    size_t n = 0;
    uint8_t* d = LeerComprimido(ruta, &n);
    if (!d) { snprintf(error, 160, "No se pudo leer el archivo comprimido (¿es más grande que 128 MB?)."); return NULL; }
    EntradaComprimido* e = (EntradaComprimido*)malloc(sizeof(EntradaComprimido) * MAX_ENTRADAS);
    int k = e ? ComprimidoListar(d, n, e, MAX_ENTRADAS) : -1;
    int sistema = -1, i = k > 0 ? Elegir(e, k, &sistema) : -1;
    uint8_t* r = NULL;
    if (k < 0) snprintf(error, 160, "El archivo comprimido está dañado o es de un tipo que no se puede abrir.");
    else if (i < 0) snprintf(error, 160, "Adentro del archivo comprimido no hay ninguna rom de una consola que ALLESIU pueda abrir.");
    else
    {
        size_t t = 0;
        r = ComprimidoSacar(d, n, e[i].indice, &t);
        if (!r) snprintf(error, 160, "No se pudo descomprimir %s.", e[i].nombre);
        else if (t > 0xFFFFFFFFu) { free(r); r = NULL; snprintf(error, 160, "La rom es demasiado grande."); }
        else { *tam = (uint32_t)t; snprintf(interno, max, "%s", e[i].nombre); }
    }
    free(e);
    free(d);
    return r;
}

// ---------- BIOS incluidas (variante personal) ----------
int CopiarBiosIncluidas()
{
#ifdef CON_BIOS_INCLUIDAS
    // Las que usan los núcleos, con el nombre que buscan. En el paquete van sin espacios ni paréntesis (construir.sh).
    static const char* const BIOS[] = {
        "disksys.rom", "gba_bios.bin", "gb_bios.bin", "gbc_bios.bin", "bios_U.sms", "bios_E.sms", "bios_J.sms", "bios.gg",
        "bios_CD_U.bin", "bios_CD_E.bin", "bios_CD_J.bin", "7800 BIOS (U).rom", "7800 BIOS (E).rom", "lynxboot.img", "neogeo.zip",
    };
    char carpeta[160];
    snprintf(carpeta, sizeof carpeta, "%s/bios", CARPETA_DATOS);
    CrearCarpetas(carpeta);
    int copiadas = 0;
    for (const char* b : BIOS)
    {
        char origen[200], destino[256], nombre[64];
        snprintf(nombre, sizeof nombre, "%s", b);
        for (char* p = nombre; *p; p++) if (*p == ' ' || *p == '(' || *p == ')') *p = '_';
        snprintf(origen, sizeof origen, "/app0/assets/misc/%s", nombre);
        snprintf(destino, sizeof destino, "%s/%s", carpeta, b);
        uint64_t tam = 0;
        if (ExisteArchivo(destino) || !ExisteArchivo(origen, &tam) || !tam || tam > 64u * 1024 * 1024) continue;
        uint8_t* d = (uint8_t*)malloc((size_t)tam);
        FILE* f = AbrirArchivo(origen, "rb");
        bool ok = d && f && fread(d, 1, (size_t)tam, f) == tam;
        if (f) fclose(f);
        FILE* g = ok ? AbrirArchivo(destino, "wb") : NULL;
        if (g) { ok = fwrite(d, 1, (size_t)tam, g) == tam; fclose(g); copiadas += ok; }
        free(d);
    }
    return copiadas;
#else
    return 0;
#endif
}

// ---------- consola por el nombre de la carpeta ----------
// Los nombres que la gente usa para las carpetas de cada consola, en minúsculas y sin espacios ni signos.
struct Alias { int sistema; const char* nombre; };
static const Alias ALIAS[] = {
    {SIS_SNES, "snes"}, {SIS_SNES, "sfc"}, {SIS_SNES, "supernintendo"}, {SIS_SNES, "superfamicom"}, {SIS_SNES, "supernes"},
    {SIS_NES, "nes"}, {SIS_NES, "famicom"}, {SIS_NES, "fds"},
    {SIS_GB, "gb"}, {SIS_GB, "gameboy"},
    {SIS_GBC, "gbc"}, {SIS_GBC, "gameboycolor"},
    {SIS_GBA, "gba"}, {SIS_GBA, "gameboyadvance"},
    {SIS_NGP, "ngp"}, {SIS_NGP, "neogeopocket"},
    {SIS_NGPC, "ngpc"}, {SIS_NGPC, "neogeopocketcolor"},
    {SIS_MD, "md"}, {SIS_MD, "genesis"}, {SIS_MD, "megadrive"},
    {SIS_SMS, "sms"}, {SIS_SMS, "mastersystem"},
    {SIS_GG, "gg"}, {SIS_GG, "gamegear"},
    {SIS_A2600, "2600"}, {SIS_A2600, "a2600"}, {SIS_A2600, "atari2600"},
    {SIS_A7800, "7800"}, {SIS_A7800, "a7800"}, {SIS_A7800, "atari7800"},
    {SIS_N64, "n64"}, {SIS_N64, "nintendo64"},
    {SIS_LYNX, "lynx"}, {SIS_LYNX, "atarilynx"},
    {SIS_ARCADE, "arcade"}, {SIS_ARCADE, "mame"}, {SIS_ARCADE, "fbneo"}, {SIS_ARCADE, "fba"}, {SIS_ARCADE, "finalburn"},
    {SIS_ARCADE, "finalburnneo"}, {SIS_ARCADE, "neogeo"}, {SIS_ARCADE, "cps1"}, {SIS_ARCADE, "cps2"}, {SIS_ARCADE, "cps3"},
};

// La consola que dice un nombre de carpeta: alguna palabra igual a un nombre corto ("roms-gba" → gba), o el nombre
// entero que termina en uno largo ("Nintendo - Game Boy Color" → gameboycolor). -1 si no dice ninguna.
static int ConsolaDeNombre(const char* nombre, size_t largo)
{
    char junto[128], palabra[64];
    size_t nj = 0, np = 0;
    int porPalabra = -1;
    for (size_t i = 0; i <= largo; i++)
    {
        char c = i < largo ? (char)tolower((unsigned char)nombre[i]) : 0;
        bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (alnum)
        {
            if (nj < sizeof junto - 1) junto[nj++] = c;
            if (np < sizeof palabra - 1) palabra[np++] = c;
            continue;
        }
        palabra[np] = 0;
        if (np && porPalabra < 0)
            for (const Alias& a : ALIAS)
                if (strlen(a.nombre) <= 4 && !strcmp(palabra, a.nombre)) { porPalabra = a.sistema; break; }
        np = 0;
    }
    junto[nj] = 0;
    // Primero el nombre largo (más específico: "gameboycolor" antes que la palabra "gb" suelta).
    int mejor = -1;
    size_t mejorLargo = 0;
    for (const Alias& a : ALIAS)
    {
        size_t l = strlen(a.nombre);
        if (l > 4 && l <= nj && !strcmp(junto + nj - l, a.nombre) && l > mejorLargo) { mejor = a.sistema; mejorLargo = l; }
    }
    return mejor >= 0 ? mejor : porPalabra;
}

int ConsolaPorCarpeta(const char* carpeta)
{
    // De la carpeta más honda hacia arriba: "USB/roms/GBA/Acción" → GBA. La carpeta de la app (/data/allesiu) no
    // cuenta: su nombre dice "snes" y todo lo copiado a la consola quedaba como juego de SNES.
    const char* tope = carpeta;
    size_t nd = strlen(CARPETA_DATOS);
    if (!strncmp(carpeta, CARPETA_DATOS, nd) && (carpeta[nd] == '/' || !carpeta[nd])) tope = carpeta + nd + (carpeta[nd] ? 1 : 0);
    const char* fin = carpeta + strlen(carpeta);
    while (fin > tope)
    {
        const char* ini = fin;
        while (ini > tope && ini[-1] != '/') ini--;
        int s = ConsolaDeNombre(ini, (size_t)(fin - ini));
        if (s >= 0) return s;
        fin = ini > tope ? ini - 1 : tope;
    }
    return -1;
}
