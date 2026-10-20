// Adaptador de los núcleos libretro a la tabla Consola (consola.h). Hay un solo juego abierto a la vez: el núcleo activo
// queda en 'api' y las funciones de abajo hablan con él. La app pide los cuadros desde el hilo de emulación; el núcleo
// llama de vuelta (imagen, sonido, mando) dentro de retro_run.
#include "nucleo_libretro.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"
#include "registro.h"

static const ApiLibretro* api;
static const char* nombreNucleo = "";
static bool iniciado;
static uint8_t* rom;             // la rom en memoria (algunos núcleos guardan el puntero mientras corre)
static char rutaRom[512];
static unsigned formato = RETRO_PIXEL_FORMAT_0RGB1555;
static float aspecto;   // el que informa el núcleo (0 = no informó)
static int frecuencia = 48000;
static uint32_t cuadros;

// Lo que deja el núcleo durante retro_run.
static uint32_t* destino;
static int destinoAncho, destinoAlto;
static int16_t* sonido;
static int sonidoMax, sonidoN;
static uint16_t botones[2];
static bool conectado[2] = {true, false};

// Copia de la batería para saber si el juego la cambió.
static uint8_t* sramCopia;
static size_t sramCopiaTam;

static char carpetaBios[160], carpetaGuardados[160];

// La rom con su ruta partida, para los núcleos que la piden con GET_GAME_INFO_EXT (así la toman de la memoria en vez de
// abrir el archivo, que en la consola está afuera del encierro del sistema).
static char infoDir[512], infoNombre[256], infoExt[16];
static retro_game_info_ext infoExt_;

// Los núcleos abren sus archivos (BIOS, por ejemplo) con esta, la misma de la app (ver nucleos/archivos.c).
// Alias de archivos: el núcleo pide una ruta y se abre otra. Lo usa el arcade cuando el zip no tiene el nombre corto
// que FinalBurn Neo necesita (ver ArcadePrepararAlias): se le pasa "carpeta/<nombre corto>.zip" y se abre el archivo real.
struct Alias { char virtual_[600], real[600]; };
static Alias alias[8];
static int nAlias;
static bool MismaRuta(const char* a, const char* b)
{
    for (; *a && *b; a++, b++)
    {
        char x = *a == 0x5C ? '/' : (*a >= 'A' && *a <= 'Z' ? *a + 32 : *a);   // la barra invertida de la PC, y sin mayúsculas
        char y = *b == 0x5C ? '/' : (*b >= 'A' && *b <= 'Z' ? *b + 32 : *b);
        if (x != y) return false;
    }
    return !*a && !*b;
}
void LibretroAliasLimpiar() { nAlias = 0; }
void LibretroAlias(const char* virtual_, const char* real)
{
    if (nAlias == 8) return;
    snprintf(alias[nAlias].virtual_, sizeof alias[0].virtual_, "%s", virtual_);
    snprintf(alias[nAlias].real, sizeof alias[0].real, "%s", real);
    nAlias++;
}
extern "C" FILE* NucleoAbrirArchivo(const char* ruta, const char* modo)
{
    for (int i = 0; i < nAlias; i++)
        if (MismaRuta(ruta, alias[i].virtual_)) return AbrirArchivo(alias[i].real, modo);
    return AbrirArchivo(ruta, modo);
}

// Mientras se abre un juego se anota también lo informativo (dónde buscó cada archivo, qué le faltó), salvo lo que se
// repite por cada botón o cada rom encontrada: sirve para entender en el registro por qué un juego no abrió.
static bool anotarTodo = false;
static bool faltoMemoria = false;   // el núcleo dijo que no pudo pedir memoria (en esta carga)

// Opciones de núcleo fijadas por la app (consola.cpp): clave y valor, como las del archivo de opciones de RetroArch.
struct Opcion { char clave[64], valor[64]; };
static Opcion opciones[16];
static int nOpciones;
void LibretroOpcion(const char* clave, const char* valor)
{
    int i = 0;
    while (i < nOpciones && strcmp(opciones[i].clave, clave)) i++;
    if (i == nOpciones) { if (nOpciones == 16) return; nOpciones++; }
    snprintf(opciones[i].clave, sizeof opciones[i].clave, "%s", clave);
    snprintf(opciones[i].valor, sizeof opciones[i].valor, "%s", valor);
}

static void Log(enum retro_log_level nivel, const char* fmt, ...)
{
    if (nivel < RETRO_LOG_WARN && !anotarTodo) return;
    char t[300];
    va_list a;
    va_start(a, fmt);
    vsnprintf(t, sizeof t, fmt, a);
    va_end(a);
    if (nivel < RETRO_LOG_WARN && (strstr(t, "nSwitchCode") || strstr(t, "Using ROM with known crc"))) return;
    size_t n = strlen(t);
    while (n && (t[n - 1] == '\n' || t[n - 1] == '\r')) t[--n] = 0;
    if (strstr(t, "ailed to allocate") || strstr(t, "ut of memory") || strstr(t, "err=7)")) faltoMemoria = true;
    Registrar("Núcleo %s: %s", nombreNucleo, t);
}

static bool Entorno(unsigned cmd, void* dato)
{
    switch (cmd)
    {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    {
        unsigned f = *(const unsigned*)dato;
        if (f > RETRO_PIXEL_FORMAT_RGB565) return false;
        formato = f;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: ((retro_log_callback*)dato)->log = Log; return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: *(const char**)dato = carpetaBios; return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char**)dato = carpetaGuardados; return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool*)dato = true; return true;
    case RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE: *(unsigned*)dato = 48000; return true;   // la de la PS4
    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
        // Las opciones que la app le fijó al núcleo (LibretroOpcion); las demás, por defecto.
        retro_variable* v = (retro_variable*)dato;
        for (int i = 0; i < nOpciones; i++)
            if (v && v->key && !strcmp(v->key, opciones[i].clave)) { v->value = opciones[i].valor; return true; }
        return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool*)dato = false; return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *(unsigned*)dato = 0; return true;   // el modo más simple
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    {
        const retro_game_geometry* g = (const retro_game_geometry*)dato;
        if (g->aspect_ratio > 0) aspecto = g->aspect_ratio;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    {
        const retro_system_av_info* av = (const retro_system_av_info*)dato;
        if (av->timing.sample_rate > 1000) frecuencia = (int)(av->timing.sample_rate + .5);
        return true;
    }
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: return true;
    case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT: *(const retro_game_info_ext**)dato = &infoExt_; return true;
    case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE: return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE: *(unsigned*)dato = RETRO_LANGUAGE_SPANISH; return true;
    default: return false;
    }
}

static inline uint32_t De565(uint16_t p)
{
    uint32_t r = p >> 11, g = (p >> 5) & 63, b = p & 31;
    return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
}
static inline uint32_t De1555(uint16_t p)
{
    uint32_t r = (p >> 10) & 31, g = (p >> 5) & 31, b = p & 31;
    return (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

static void Video(const void* datos, unsigned w, unsigned h, size_t pitch)
{
    if (!datos || !destino) return;   // cuadro repetido: queda el anterior
    if (w > (unsigned)CONSOLA_MAX_ANCHO) w = CONSOLA_MAX_ANCHO;
    if (h > (unsigned)CONSOLA_MAX_ALTO) h = CONSOLA_MAX_ALTO;
    destinoAncho = (int)w;
    destinoAlto = (int)h;
    for (unsigned y = 0; y < h; y++)
    {
        const uint8_t* fila = (const uint8_t*)datos + y * pitch;
        uint32_t* sal = destino + (size_t)y * w;
        if (formato == RETRO_PIXEL_FORMAT_XRGB8888)
            for (unsigned x = 0; x < w; x++) sal[x] = ((const uint32_t*)fila)[x] & 0xFFFFFF;
        else if (formato == RETRO_PIXEL_FORMAT_RGB565)
            for (unsigned x = 0; x < w; x++) sal[x] = De565(((const uint16_t*)fila)[x]);
        else
            for (unsigned x = 0; x < w; x++) sal[x] = De1555(((const uint16_t*)fila)[x]);
    }
}

static void Audio1(int16_t l, int16_t r)
{
    if (!sonido || sonidoN >= sonidoMax) return;
    sonido[sonidoN * 2] = l;
    sonido[sonidoN * 2 + 1] = r;
    sonidoN++;
}

static size_t AudioLote(const int16_t* d, size_t n)
{
    if (sonido)
    {
        size_t cabe = (size_t)(sonidoMax - sonidoN);
        size_t k = n < cabe ? n : cabe;
        memcpy(sonido + sonidoN * 2, d, k * 4);
        sonidoN += (int)k;
    }
    return n;
}

static void Sondeo() {}

static int16_t EstadoEntrada(unsigned puerto, unsigned disp, unsigned, unsigned id)
{
    if (puerto > 1 || !conectado[puerto] || (disp & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD) return 0;
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t)botones[puerto];
    return id < 16 ? (botones[puerto] >> id) & 1 : 0;
}

// ---------- la tabla Consola ----------
// Abre la rom ya leída ('datos', malloc: queda a cargo de este módulo). 'interno' es el nombre del archivo dentro del
// comprimido 'ruta', o NULL si la rom es el archivo 'ruta' mismo.
static bool Abrir(const char* ruta, const char* interno, uint8_t* datos, size_t tam, char* error)
{
    snprintf(carpetaBios, sizeof carpetaBios, "%s/bios", CARPETA_DATOS);
    snprintf(carpetaGuardados, sizeof carpetaGuardados, "%s/guardados", CARPETA_DATOS);
    if (!iniciado)
    {
        api->set_environment(Entorno);
        api->set_video_refresh(Video);
        api->set_audio_sample(Audio1);
        api->set_audio_sample_batch(AudioLote);
        api->set_input_poll(Sondeo);
        api->set_input_state(EstadoEntrada);
        api->init();
        iniciado = true;
    }
    free(rom);
    rom = datos;
    // Carpeta, nombre sin extensión y extensión (en minúsculas) para GET_GAME_INFO_EXT. Si la rom viene de un comprimido,
    // el nombre y la extensión son los del archivo de adentro (algunos núcleos sacan la región del nombre).
    snprintf(infoDir, sizeof infoDir, "%s", ruta);
    char* barra = strrchr(infoDir, '/');
    const char* archivo = barra ? barra + 1 : ruta;
    if (barra) *barra = 0; else infoDir[0] = 0;
    if (interno)
    {
        const char* b = strrchr(interno, '/');
        archivo = b ? b + 1 : interno;
        snprintf(rutaRom, sizeof rutaRom, "%s/%s", infoDir, archivo);
    }
    else snprintf(rutaRom, sizeof rutaRom, "%s", ruta);
    snprintf(infoNombre, sizeof infoNombre, "%s", archivo);
    char* punto = strrchr(infoNombre, '.');
    infoExt[0] = 0;
    if (punto)
    {
        *punto = 0;
        int k = 0;
        for (const char* p = punto + 1; *p && k < (int)sizeof infoExt - 1; p++) infoExt[k++] = (char)(*p >= 'A' && *p <= 'Z' ? *p + 32 : *p);
        infoExt[k] = 0;
    }
    infoExt_ = {};
    infoExt_.full_path = rutaRom;
    infoExt_.dir = infoDir;
    infoExt_.name = infoNombre;
    infoExt_.ext = infoExt;
    infoExt_.data = rom;
    infoExt_.size = (size_t)tam;
    infoExt_.persistent_data = true;   // la rom queda en memoria mientras dura el juego
    if (interno)
    {
        static char archivoInterno[256];
        snprintf(archivoInterno, sizeof archivoInterno, "%s", interno);
        infoExt_.file_in_archive = true;
        infoExt_.full_path = NULL;
        infoExt_.archive_path = ruta;
        infoExt_.archive_file = archivoInterno;
    }
#ifdef _WIN32
    // Simulador de PC: los núcleos compilados para Windows separan las carpetas con '\' (FinalBurn Neo busca los zips
    // en la carpeta del juego cortando por ahí). plataforma_pc.cpp las vuelve a '/'.
    for (char* c = rutaRom; *c; c++) if (*c == '/') *c = 0x5C;
#endif
    retro_game_info gi = {rutaRom, rom, (size_t)tam, NULL};
    anotarTodo = true;
    faltoMemoria = false;
    bool cargado = api->load_game(&gi);
    anotarTodo = false;
    if (!cargado)
    {
        snprintf(error, 160, faltoMemoria ? "No alcanzó la memoria de la consola para abrir este juego con %s." :
                 "El núcleo de %s no pudo abrir esta rom (¿está dañada o es de otra consola?).", nombreNucleo);
        return false;
    }
    retro_system_av_info av = {};
    api->get_system_av_info(&av);
    frecuencia = av.timing.sample_rate > 1000 ? (int)(av.timing.sample_rate + .5) : 48000;
    aspecto = av.geometry.aspect_ratio > 0 ? av.geometry.aspect_ratio
            : av.geometry.base_height ? (float)av.geometry.base_width / av.geometry.base_height : 0;
    api->set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    api->set_controller_port_device(1, RETRO_DEVICE_JOYPAD);
    // Copia de la batería tal como quedó al cargar (la app la llena después con el archivo de la partida).
    free(sramCopia);
    sramCopia = NULL;
    sramCopiaTam = 0;
    cuadros = 0;
    return true;
}

static bool Cargar(const char* ruta, char* error)
{
    uint64_t tam = 0;
    if (!ExisteArchivo(ruta, &tam) || tam == 0 || tam > 64u * 1024 * 1024)
    {
        snprintf(error, 160, "No se pudo abrir la rom o es demasiado grande.");
        return false;
    }
    uint8_t* d = (uint8_t*)malloc((size_t)tam);
    FILE* f = AbrirArchivo(ruta, "rb");
    bool leida = d && f && fread(d, 1, (size_t)tam, f) == tam;
    if (f) fclose(f);
    if (!leida) { free(d); snprintf(error, 160, "No se pudo leer la rom."); return false; }
    return Abrir(ruta, NULL, d, (size_t)tam, error);
}

static void Descargar()
{
    if (!api || !iniciado) return;
    api->unload_game();
    api->deinit();
    iniciado = false;
    free(rom);
    rom = NULL;
    free(sramCopia);
    sramCopia = NULL;
    sramCopiaTam = 0;
}

static void Reiniciar() { api->reset(); }
// Trucos (códigos Game Genie, Action Replay...): cada núcleo entiende los de su consola.
static void TrucosBorrar() { if (api && iniciado) api->cheat_reset(); }
static void Truco(unsigned i, bool activo, const char* codigo) { if (api && iniciado) api->cheat_set(i, activo, codigo); }

static int Cuadro(uint32_t* imagen, int* ancho, int* alto, int16_t* audio, int maxAudio)
{
    destino = imagen;
    sonido = audio;
    sonidoMax = maxAudio;
    sonidoN = 0;
    api->run();
    destino = NULL;
    sonido = NULL;
    cuadros++;
    *ancho = destinoAncho ? destinoAncho : 1;
    *alto = destinoAlto ? destinoAlto : 1;
    return sonidoN;
}

static int Frecuencia() { return frecuencia; }
static float Aspecto() { return aspecto; }
static void Mando(int j, uint16_t b) { if (j >= 0 && j < 2) botones[j] = b; }
static void MandoConectado(int j, bool si) { if (j >= 0 && j < 2) conectado[j] = si; }

// Sin juego abierto (la carga falló, o el núcleo ya se cerró): no hay partida.
static uint32_t SramTam() { return api && iniciado ? (uint32_t)api->get_memory_size(RETRO_MEMORY_SAVE_RAM) : 0; }
static uint8_t* SramDatos() { return api && iniciado ? (uint8_t*)api->get_memory_data(RETRO_MEMORY_SAVE_RAM) : NULL; }

static bool SramCambiada()
{
    size_t n = api->get_memory_size(RETRO_MEMORY_SAVE_RAM);
    const uint8_t* d = (const uint8_t*)api->get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!n || !d) return false;
    if (sramCopiaTam != n)
    {
        free(sramCopia);
        sramCopia = (uint8_t*)malloc(n);
        sramCopiaTam = sramCopia ? n : 0;
        if (sramCopia) memcpy(sramCopia, d, n);
        return false;   // la primera vez solo se toma la copia
    }
    if (!memcmp(sramCopia, d, n)) return false;
    memcpy(sramCopia, d, n);
    return true;
}

// Los estados llevan delante el nombre del núcleo y el tamaño, para no cargar el de otro núcleo.
struct CabeceraEstado { char marca[8]; char nucleo[24]; uint32_t tam; };

static uint32_t EstadoTam() { return (uint32_t)(sizeof(CabeceraEstado) + api->serialize_size()); }

static uint32_t GuardarEstado(uint8_t* buf, uint32_t max)
{
    size_t n = api->serialize_size();
    if (sizeof(CabeceraEstado) + n > max) return 0;
    CabeceraEstado c = {};
    memcpy(c.marca, "SNESUIL", 8);
    snprintf(c.nucleo, sizeof c.nucleo, "%s", nombreNucleo);
    c.tam = (uint32_t)n;
    memcpy(buf, &c, sizeof c);
    if (!api->serialize(buf + sizeof c, n)) return 0;
    return (uint32_t)(sizeof c + n);
}

static bool CargarEstado(const uint8_t* buf, uint32_t n)
{
    if (n < sizeof(CabeceraEstado)) return false;
    CabeceraEstado c;
    memcpy(&c, buf, sizeof c);
    if (memcmp(c.marca, "SNESUIL", 8) || strncmp(c.nucleo, nombreNucleo, sizeof c.nucleo) || c.tam != n - sizeof c) return false;
    return api->unserialize(buf + sizeof c, c.tam);
}

static int Diagnostico(char* out, int max)
{
    retro_system_info si = {};
    api->get_system_info(&si);
    return snprintf(out, max, "Núcleo: %s %s\nRom: %s\nCuadros emulados: %u\nImagen: %d x %d, sonido %d Hz\n",
                    si.library_name ? si.library_name : "?", si.library_version ? si.library_version : "", rutaRom,
                    cuadros, destinoAncho, destinoAlto, frecuencia);
}

static bool Anomalia(char*, int) { return false; }

void LibretroLlenar(Consola& c)
{
    c.Descargar = Descargar;
    c.Reiniciar = Reiniciar;
    c.TrucosBorrar = TrucosBorrar;
    c.Truco = Truco;
    c.Cuadro = Cuadro;
    c.Frecuencia = Frecuencia;
    c.Aspecto = Aspecto;
    c.Mando = Mando;
    c.MandoConectado = MandoConectado;
    c.SramTam = SramTam;
    c.SramDatos = SramDatos;
    c.SramCambiada = SramCambiada;
    c.EstadoTam = EstadoTam;
    c.GuardarEstado = GuardarEstado;
    c.CargarEstado = CargarEstado;
    c.Diagnostico = Diagnostico;
    c.Anomalia = Anomalia;
}

bool LibretroCargar(const ApiLibretro* a, const char* nombre, const char* ruta, char* error)
{
    if (api != a && iniciado) Descargar();   // otro núcleo quedó abierto
    api = a;
    nombreNucleo = nombre;
    return Cargar(ruta, error);
}

bool LibretroCargarRuta(const ApiLibretro* a, const char* nombre, const char* ruta, char* error)
{
    if (api != a && iniciado) Descargar();
    api = a;
    nombreNucleo = nombre;
    return Abrir(ruta, NULL, NULL, 0, error);
}

bool LibretroCargarMemoria(const ApiLibretro* a, const char* nombre, const char* ruta, const char* interno, uint8_t* datos,
                           uint32_t tam, char* error)
{
    if (api != a && iniciado) Descargar();
    api = a;
    nombreNucleo = nombre;
    return Abrir(ruta, interno, datos, tam, error);
}

bool LibretroFaltoMemoria() { return faltoMemoria; }
