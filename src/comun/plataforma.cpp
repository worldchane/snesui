// Ver plataforma.h. Sin variables globales con constructores (.init_array debe quedar vacío): solo arreglos y tipos simples.
#include "plataforma.h"

#include <atomic>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifndef SIN_LISTADO
#include <dirent.h>
#endif

#include <orbis/AudioOut.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include <SDL2/SDL.h>

// ---------- tiempo ----------
// Con el contador de SDL: gettimeofday de la librería de C pediría clock_gettime, que no está probada en la consola.
static uint64_t inicio;

double Segundos()
{
    if (!inicio) inicio = SDL_GetPerformanceCounter();
    return (double)(SDL_GetPerformanceCounter() - inicio) / (double)SDL_GetPerformanceFrequency();
}

// ---------- hilos ----------
bool Lanzar(void* (*f)(void*), void* arg, const char* nombre)
{
    // Igual que lo que funcionó en la consola: sin atributos y sin despegar el hilo.
    OrbisPthread h;
    return scePthreadCreate(&h, NULL, f, arg, nombre) == 0;
}

// Sincronización con contadores atómicos (sin semáforos: los de la librería de C piden al sistema funciones que no
// están probadas en la consola). Quien espera da vueltas un momento y después duerme de a ratos cortos.
static const int MAX_TRAB = 8;
static std::atomic<int> generacion{0}, pendientes{0};
static int nTrab;
static void (*tareaF)(int, int, void*);
static void* tareaArg;

static void Esperar(int& vueltas)
{
    if (++vueltas < 4000) __builtin_ia32_pause();
    else sceKernelUsleep(50);
}

static void* Trabajador(void* a)
{
    int i = (int)(intptr_t)a, vista = 0;
    for (;;)
    {
        int g, vueltas = 0;
        while ((g = generacion.load(std::memory_order_acquire)) == vista) Esperar(vueltas);
        vista = g;
        tareaF(i + 1, nTrab + 1, tareaArg);
        pendientes.fetch_sub(1, std::memory_order_release);
    }
    return NULL;
}

void IniciarTrabajadores(int n)
{
    if (n > MAX_TRAB) n = MAX_TRAB;
    for (int i = 0; i < n; i++)
    {
        if (!Lanzar(Trabajador, (void*)(intptr_t)i, "trabajador")) break;
        nTrab = i + 1;
    }
}

int Partes() { return nTrab + 1; }

void EnParalelo(void (*f)(int, int, void*), void* arg)
{
    tareaF = f;
    tareaArg = arg;
    pendientes.store(nTrab, std::memory_order_relaxed);
    generacion.fetch_add(1, std::memory_order_release);
    f(0, nTrab + 1, arg);
    int vueltas = 0;
    while (pendientes.load(std::memory_order_acquire) > 0) Esperar(vueltas);
}

// ---------- mando ----------
static int32_t pad = -1;
static int32_t usuario1 = -1;
static uint32_t antes;
static float repEspera[4];   // tiempo hasta la próxima repetición de cada dirección

void IniciarMando()
{
    OrbisUserServiceInitializeParams up;
    up.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&up);
    int32_t usuario = -1;
    sceUserServiceGetInitialUser(&usuario);
    usuario1 = usuario;
    scePadInit();
    pad = scePadOpen(usuario, 0, 0, NULL);
}

static float Eje(uint8_t v)
{
    float f = (v - 128) / 127.f;
    if (f > 1) f = 1;
    if (f < -1) f = -1;
    const float muerta = .22f;   // zona muerta, y el resto reescalado para que el movimiento empiece suave
    if (fabsf(f) < muerta) return 0;
    return (f - (f > 0 ? muerta : -muerta)) / (1 - muerta);
}

// Los botones de la consola a los bits B_*.
static uint32_t Botones(uint32_t b)
{
    uint32_t a = 0;
    if (b & ORBIS_PAD_BUTTON_UP) a |= B_ARRIBA;
    if (b & ORBIS_PAD_BUTTON_DOWN) a |= B_ABAJO;
    if (b & ORBIS_PAD_BUTTON_LEFT) a |= B_IZQ;
    if (b & ORBIS_PAD_BUTTON_RIGHT) a |= B_DER;
    if (b & ORBIS_PAD_BUTTON_CROSS) a |= B_CRUZ;
    if (b & ORBIS_PAD_BUTTON_CIRCLE) a |= B_CIRCULO;
    if (b & ORBIS_PAD_BUTTON_SQUARE) a |= B_CUADRADO;
    if (b & ORBIS_PAD_BUTTON_TRIANGLE) a |= B_TRIANGULO;
    if (b & ORBIS_PAD_BUTTON_L1) a |= B_L1;
    if (b & ORBIS_PAD_BUTTON_R1) a |= B_R1;
    if (b & ORBIS_PAD_BUTTON_L2) a |= B_L2;
    if (b & ORBIS_PAD_BUTTON_R2) a |= B_R2;
    if (b & ORBIS_PAD_BUTTON_OPTIONS) a |= B_OPTIONS;
    if (b & ORBIS_PAD_BUTTON_L3) a |= B_L3;
    if (b & ORBIS_PAD_BUTTON_R3) a |= B_R3;
    if (b & ORBIS_PAD_BUTTON_TOUCH_PAD) a |= B_TACTIL;
    return a;
}

// Jugador 2: en la PS4 cada mando pertenece a un usuario con la sesión iniciada. Cada segundo se busca otro usuario
// conectado (distinto del que abrió el programa) y se abre su mando.
static int32_t pad2 = -1, usuario2 = -1;
static double proximaBusqueda;

uint32_t LeerMando2(bool* conectado)
{
    if (Segundos() >= proximaBusqueda)
    {
        proximaBusqueda = Segundos() + 1;
        OrbisUserServiceLoginUserIdList lista;
        memset(&lista, 0xFF, sizeof lista);
        int32_t otro = -1;
        if (sceUserServiceGetLoginUserIdList(&lista) == 0)
            for (int i = 0; i < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; i++)
                if (lista.userId[i] != ORBIS_USER_SERVICE_USER_ID_INVALID && lista.userId[i] != usuario1) { otro = lista.userId[i]; break; }
        if (otro != usuario2)
        {
            if (pad2 >= 0) scePadClose(pad2);
            pad2 = otro != -1 ? scePadOpen(otro, 0, 0, NULL) : -1;
            usuario2 = otro;
        }
    }
    OrbisPadData d;
    memset(&d, 0, sizeof d);
    bool ok = pad2 >= 0 && scePadReadState(pad2, &d) == 0 && d.connected;
    if (conectado) *conectado = ok;
    if (!ok) return 0;
    uint32_t a = Botones(d.buttons);
    // El stick izquierdo también mueve.
    int lx = d.leftStick.x - 128, ly = d.leftStick.y - 128;
    if (ly < -64) a |= B_ARRIBA;
    if (ly > 64) a |= B_ABAJO;
    if (lx < -64) a |= B_IZQ;
    if (lx > 64) a |= B_DER;
    return a;
}

void LeerMando(Mando& m, float dt)
{
    OrbisPadData d;
    memset(&d, 0, sizeof d);
    m.conectado = pad >= 0 && scePadReadState(pad, &d) == 0 && d.connected;
    uint32_t a = Botones(d.buttons);
    m.lx = m.conectado ? Eje(d.leftStick.x) : 0;
    m.ly = m.conectado ? Eje(d.leftStick.y) : 0;
    m.rx = m.conectado ? Eje(d.rightStick.x) : 0;
    m.ry = m.conectado ? Eje(d.rightStick.y) : 0;
    // El stick izquierdo inclinado más de la mitad cuenta como cruceta.
    if (m.ly < -.5f) a |= B_ARRIBA;
    if (m.ly > .5f) a |= B_ABAJO;
    if (m.lx < -.5f) a |= B_IZQ;
    if (m.lx > .5f) a |= B_DER;

    m.pulsados = a & ~antes;
    // Repetición: tras 0,38 s mantenido, cada 0,075 s (como el menú de la consola).
    for (int i = 0; i < 4; i++)
    {
        uint32_t bit = 1u << i;
        if (!(a & bit)) continue;
        if (!(antes & bit)) { repEspera[i] = .38f; continue; }
        repEspera[i] -= dt;
        if (repEspera[i] <= 0) { m.pulsados |= bit; repEspera[i] += .075f; }
    }
    m.apretados = a;
    antes = a;
}

// ---------- sonidos de la interfaz ----------
// El hilo de sonido toma los pedidos de una cola circular y los mezcla como voces senoidales suaves.
static int32_t puertoAudio = -1;
static std::atomic<unsigned> colaEsc{0};
static int cola[32];
static std::atomic<bool> sonidosSi{true};

struct Voz { float f0, f1, fase, t, dur, amp, ataque; float retardo; };

// ---------- sonido del juego ----------
// El emulador deja muestras estéreo a 32 kHz en un anillo (un solo escritor: el hilo de emulación; un solo lector: el
// hilo de sonido). El hilo de sonido las pasa a 48 kHz interpolando, y ajusta un poco la velocidad según lo lleno que
// esté el anillo, para no quedarse sin muestras ni acumular retraso.
static const uint32_t ANILLO = 16384;   // muestras estéreo (~0,5 s)
static int16_t anillo[ANILLO * 2];
static std::atomic<uint32_t> anilloEsc{0}, anilloLee{0};

void AudioJuego(const int16_t* estereo, int n)
{
    uint32_t e = anilloEsc.load(std::memory_order_relaxed), l = anilloLee.load(std::memory_order_acquire);
    for (int i = 0; i < n && e - l < ANILLO - 1; i++, e++)
    {
        anillo[(e % ANILLO) * 2] = estereo[i * 2];
        anillo[(e % ANILLO) * 2 + 1] = estereo[i * 2 + 1];
    }
    anilloEsc.store(e, std::memory_order_release);
}

static std::atomic<int> frecJuego{32000};
void AudioJuegoFrecuencia(int hz) { frecJuego.store(hz > 1000 && hz <= 192000 ? hz : 32000, std::memory_order_relaxed); }

void AudioJuegoVaciar() { anilloLee.store(anilloEsc.load(std::memory_order_acquire), std::memory_order_release); }

static void* HiloAudio(void*)
{
    double posJuego = 0;   // parte fraccionaria entre la muestra de 32 kHz actual y la siguiente
    static int16_t buf[256 * 2];
    static Voz voces[8];
    unsigned leidos = 0;
    const float SR = 48000.f;
    for (;;)
    {
        unsigned hasta = colaEsc.load(std::memory_order_acquire);
        for (; leidos != hasta; leidos++)
        {
            Voz nuevas[2];
            int n = 0;
            switch (cola[leidos & 31])
            {
            case SON_MOVER:   nuevas[n++] = {1180, 1320, 0, 0, .055f, .10f, .002f, 0}; break;
            case SON_SECCION: nuevas[n++] = {740, 990, 0, 0, .09f, .11f, .003f, 0}; break;
            case SON_ACEPTAR: nuevas[n++] = {880, 880, 0, 0, .09f, .12f, .002f, 0};
                              nuevas[n++] = {1320, 1320, 0, 0, .16f, .11f, .002f, .06f}; break;
            case SON_ATRAS:   nuevas[n++] = {700, 470, 0, 0, .11f, .12f, .002f, 0}; break;
            case SON_ERROR:   nuevas[n++] = {230, 200, 0, 0, .22f, .14f, .004f, 0}; break;
            case SON_INICIO:  nuevas[n++] = {523, 523, 0, 0, .5f, .09f, .01f, 0};
                              nuevas[n++] = {784, 784, 0, 0, .6f, .08f, .01f, .12f}; break;
            }
            for (int k = 0; k < n; k++)
            {
                int libre = 0;   // la voz libre, o la más vieja
                for (int v = 0; v < 8; v++)
                {
                    if (voces[v].t >= voces[v].dur) { libre = v; break; }
                    if (voces[v].t > voces[libre].t) libre = v;
                }
                voces[libre] = nuevas[k];
            }
        }
        for (int i = 0; i < 256; i++)
        {
            float s = 0;
            for (int v = 0; v < 8; v++)
            {
                Voz& z = voces[v];
                if (z.t >= z.dur) continue;
                if (z.retardo > 0) { z.retardo -= 1 / SR; continue; }
                float p = z.t / z.dur;
                float f = z.f0 + (z.f1 - z.f0) * p;
                z.fase += f / SR;
                if (z.fase >= 1) z.fase -= 1;
                float env = z.t < z.ataque ? z.t / z.ataque : expf(-5.f * (z.t - z.ataque) / z.dur);
                // Seno con un poco del segundo armónico: suena más "de vidrio" que un seno puro.
                float w = sinf(z.fase * 6.2831853f) + .25f * sinf(z.fase * 12.566371f);
                s += w * env * z.amp;
                z.t += 1 / SR;
            }
            // Sonido del juego: su frecuencia (32 kHz en la SNES) → 48 kHz con interpolación lineal.
            float jl = 0, jr = 0;
            uint32_t l = anilloLee.load(std::memory_order_relaxed), e = anilloEsc.load(std::memory_order_acquire);
            uint32_t lleno = e - l;
            if (lleno >= 2)
            {
                int16_t* a = anillo + (l % ANILLO) * 2;
                int16_t* b = anillo + ((l + 1) % ANILLO) * 2;
                float f = (float)posJuego;
                jl = (a[0] + (b[0] - a[0]) * f) / 32768.f;
                jr = (a[1] + (b[1] - a[1]) * f) / 32768.f;
                // Objetivo: 50 ms guardados (1.600 muestras a 32 kHz). Más llenas → un poco más rápido; menos → más lento.
                int hz = frecJuego.load(std::memory_order_relaxed);
                double objetivo = hz / 20.0;
                double ajuste = 1 + ((double)lleno - objetivo) / objetivo * .005;
                if (ajuste < .99) ajuste = .99;
                if (ajuste > 1.01) ajuste = 1.01;
                posJuego += hz / 48000.0 * ajuste;
                uint32_t avanza = (uint32_t)posJuego;
                posJuego -= avanza;
                if (avanza > lleno - 1) avanza = lleno - 1;
                anilloLee.store(l + avanza, std::memory_order_release);
            }
            float L = s + jl, R = s + jr;
            if (L > 1) L = 1;
            if (L < -1) L = -1;
            if (R > 1) R = 1;
            if (R < -1) R = -1;
            buf[i * 2] = (int16_t)(L * 32000);
            buf[i * 2 + 1] = (int16_t)(R * 32000);
        }
        sceAudioOutOutput(puertoAudio, buf);   // espera a que la consola pida el bloque siguiente
    }
    return NULL;
}

void IniciarAudio()
{
    sceAudioOutInit();
    puertoAudio = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000,
                                  ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (puertoAudio >= 0 && !Lanzar(HiloAudio, NULL, "sonido")) puertoAudio = -1;
}

bool AudioListo() { return puertoAudio >= 0; }
void SonidosActivos(bool si) { sonidosSi = si; }

void Sonar(Sonido s)
{
    if (puertoAudio < 0 || !sonidosSi) return;
    unsigned i = colaEsc.load(std::memory_order_relaxed);
    cola[i & 31] = s;
    colaEsc.store(i + 1, std::memory_order_release);
}

// ---------- archivos ----------
bool CrearCarpeta(const char* ruta) { return mkdir(ruta, 0777) == 0; }

bool CrearCarpetas(const char* ruta)
{
    char r[512];
    snprintf(r, sizeof r, "%s", ruta);
    for (char* p = r + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(r, 0777); *p = '/'; }
    mkdir(r, 0777);
    // Comprobar escribiendo de verdad (stat no está entre las funciones probadas).
    char prueba[600];
    snprintf(prueba, sizeof prueba, "%s/.prueba", r);
    FILE* f = fopen(prueba, "wb");
    if (!f) return false;
    fclose(f);
    return true;
}

// ---------- red ----------
// libSceNet se carga en el momento con sceKernelLoadStartModule y sus funciones se buscan por nombre (sceKernelDlsym):
// así el programa no depende de ella para abrir.
#ifndef SIN_RED
struct DireccionIn { uint8_t largo, familia; uint16_t puerto; uint32_t ip; uint8_t cero[8]; };
static struct
{
    bool listo, probado;
    int (*Init)();
    int (*PoolCreate)(const char*, int, int);
    int (*ResolverCreate)(const char*, int, int);
    int (*ResolverStartNtoa)(int, const char*, uint32_t*, int, int, int);
    int (*ResolverDestroy)(int);
    int (*Socket)(const char*, int, int, int);
    int (*Connect)(int, const void*, uint32_t);
    int (*Send)(int, const void*, size_t, int);
    int (*Recv)(int, void*, size_t, int);
    int (*SocketClose)(int);
    int (*Setsockopt)(int, int, int, const void*, uint32_t);
    int pool;
} red;
static char redDetalle[160];
#endif

bool RedIniciar(char* detalle, int max)
{
#ifdef SIN_RED
    snprintf(detalle, max, "Esta versión no usa la red.");
    return false;
#else
    if (red.probado) { snprintf(detalle, max, "%s", redDetalle); return red.listo; }
    red.probado = true;
    static const char* const RUTAS[] = {"libSceNet.sprx", "/system/common/lib/libSceNet.sprx"};
    int h = -1;
    for (const char* r : RUTAS)
    {
        h = (int)sceKernelLoadStartModule(r, 0, NULL, 0, NULL, NULL);
        if (h >= 0) break;
    }
    if (h < 0)
    {
        snprintf(redDetalle, sizeof redDetalle, "La consola no dejó cargar la red (código 0x%X).", (unsigned)h);
        snprintf(detalle, max, "%s", redDetalle);
        return false;
    }
    struct { const char* nombre; void** f; } tabla[] = {
        {"sceNetInit", (void**)&red.Init}, {"sceNetPoolCreate", (void**)&red.PoolCreate},
        {"sceNetResolverCreate", (void**)&red.ResolverCreate}, {"sceNetResolverStartNtoa", (void**)&red.ResolverStartNtoa},
        {"sceNetResolverDestroy", (void**)&red.ResolverDestroy}, {"sceNetSocket", (void**)&red.Socket},
        {"sceNetConnect", (void**)&red.Connect}, {"sceNetSend", (void**)&red.Send}, {"sceNetRecv", (void**)&red.Recv},
        {"sceNetSocketClose", (void**)&red.SocketClose}, {"sceNetSetsockopt", (void**)&red.Setsockopt},
    };
    for (auto& t : tabla)
        if (sceKernelDlsym(h, t.nombre, t.f) != 0 || !*t.f)
        {
            snprintf(redDetalle, sizeof redDetalle, "Falta la función %s en la red de la consola.", t.nombre);
            snprintf(detalle, max, "%s", redDetalle);
            return false;
        }
    red.Init();   // si ya estaba iniciada devuelve error: no importa
    red.pool = red.PoolCreate("snesred", 64 * 1024, 0);
    if (red.pool < 0)
    {
        snprintf(redDetalle, sizeof redDetalle, "No se pudo preparar la red (código 0x%X).", (unsigned)red.pool);
        snprintf(detalle, max, "%s", redDetalle);
        return false;
    }
    red.listo = true;
    snprintf(redDetalle, sizeof redDetalle, "Red lista.");
    snprintf(detalle, max, "%s", redDetalle);
    return true;
#endif
}

int RedConectar(const char* host, int puerto)
{
#ifdef SIN_RED
    (void)host; (void)puerto;
    return -1;
#else
    if (!red.listo) return -1;
    int res = red.ResolverCreate("snesdns", red.pool, 0);
    if (res < 0) return -1;
    uint32_t ip = 0;
    int r = red.ResolverStartNtoa(res, host, &ip, 0, 0, 0);
    red.ResolverDestroy(res);
    if (r < 0 || !ip) return -1;
    int s = red.Socket("snessock", 2 /* AF_INET */, 1 /* SOCK_STREAM */, 0);
    if (s < 0) return -1;
    int espera = 15 * 1000 * 1000;   // 15 s para recibir o enviar (en microsegundos)
    red.Setsockopt(s, 0xFFFF /* SOL_SOCKET */, 0x1006 /* SO_RCVTIMEO */, &espera, sizeof espera);
    red.Setsockopt(s, 0xFFFF, 0x1005 /* SO_SNDTIMEO */, &espera, sizeof espera);
    DireccionIn d;
    memset(&d, 0, sizeof d);
    d.largo = sizeof d;
    d.familia = 2;
    d.puerto = (uint16_t)((puerto >> 8) | ((puerto & 0xFF) << 8));
    d.ip = ip;
    if (red.Connect(s, &d, sizeof d) < 0) { red.SocketClose(s); return -1; }
    return s;
#endif
}

int RedEnviar(int s, const void* datos, int n)
{
#ifdef SIN_RED
    (void)s; (void)datos; (void)n;
    return -1;
#else
    return red.Send(s, datos, (size_t)n, 0);
#endif
}

int RedRecibir(int s, void* datos, int n)
{
#ifdef SIN_RED
    (void)s; (void)datos; (void)n;
    return -1;
#else
    return red.Recv(s, datos, (size_t)n, 0);
#endif
}

void RedCerrar(int s)
{
#ifndef SIN_RED
    if (red.listo && s >= 0) red.SocketClose(s);
#else
    (void)s;
#endif
}

FILE* AbrirArchivo(const char* ruta, const char* modo) { return fopen(ruta, modo); }

bool ExisteArchivo(const char* ruta, uint64_t* tam)
{
    // fopen + fseek en vez de stat: solo funciones ya probadas.
    FILE* f = fopen(ruta, "rb");
    if (!f) return false;
    if (tam)
    {
        fseeko(f, 0, SEEK_END);
        *tam = (uint64_t)ftello(f);
    }
    fclose(f);
    return true;
}

static int errorListado;

// ---------- acceso completo con GoldHEN ----------
// Como el SDK de plugins de GoldHEN (licencia MIT, github.com/GoldHEN/GoldHEN_Plugins_SDK): la llamada al sistema 500
// la atiende GoldHEN; la orden 2 (JAILBREAK) quita el encierro y guarda en 'respaldo' lo que había antes.
// Se hace con la llamada indirecta (número 0, el número real va en rdi): no pide funciones nuevas al sistema.
extern "C" long LlamadaSistema(long numero, long a, long b);
__asm__(
    ".globl LlamadaSistema\n"
    "LlamadaSistema:\n"
    "  movq $0, %rax\n"
    "  movq %rcx, %r10\n"
    "  syscall\n"
    "  jnb 1f\n"
    "  negq %rax\n"          // error: devuelve -errno
    "1:\n"
    "  retq\n");

// ---------- espacio libre y borrar archivos ----------
// Con llamadas directas al sistema (las de FreeBSD, la base del sistema de la PS4), por el mismo camino que la de
// GoldHEN: no se piden funciones nuevas. statfs = 396, unlink = 10.
bool EspacioLibre(const char* ruta, uint64_t* libres, uint64_t* total)
{
    // Los primeros campos de struct statfs de FreeBSD; el resto (nombres del montaje) no se usa.
    struct
    {
        uint32_t version, tipo;
        uint64_t banderas, bloque, bloqueEs, bloques, bloquesLibres;
        int64_t disponibles;
        uint64_t archivos;
        int64_t archivosLibres;
        uint8_t resto[1024];
    } st;
    memset(&st, 0, sizeof st);
    if (LlamadaSistema(396, (long)ruta, (long)&st) != 0) return false;
    // Que los números tengan sentido (si no, mejor no copiar nada).
    if (st.bloque < 512 || st.bloque > (1u << 24) || (st.bloque & (st.bloque - 1)) || st.disponibles < 0 ||
        (uint64_t)st.disponibles > st.bloques || st.bloques == 0)
        return false;
    *libres = (uint64_t)st.disponibles * st.bloque;
    if (total) *total = st.bloques * st.bloque;
    return true;
}

bool BorrarArchivo(const char* ruta) { return LlamadaSistema(10, (long)ruta, 0) == 0; }

static char accesoDetalle[120];
static uint64_t respaldo[16];   // struct jailbreak_backup del SDK (72 bytes): cómo estaba el encierro
static bool afuera;
static char rutaAcceso[96];
const char* AccesoDetalle() { return accesoDetalle; }

static int AbrirAccesoInterno(const char* quien, bool insistir)
{
    // Antes de probar se anota "probando" y apenas funciona, "ok". Si quedó "probando", eso cerró el programa: al volver
    // a abrirlo no se repite solo (sí si el usuario insiste).
    snprintf(rutaAcceso, sizeof rutaAcceso, "%s/acceso_%s.txt", CARPETA_DATOS, quien);
    char antes[32] = "";
    LeerTexto(rutaAcceso, antes, sizeof antes);
    if (!insistir && (!strncmp(antes, "probando", 8) || !strncmp(antes, "cerro", 5)))
    {
        EscribirTexto(rutaAcceso, "cerro\n");
        snprintf(accesoDetalle, sizeof accesoDetalle, "Pedir acceso a GoldHEN cerró el programa una vez. Pulsa triángulo para intentarlo de nuevo.");
        return -1;
    }
    EscribirTexto(rutaAcceso, "probando\n");
    long version = LlamadaSistema(500, 0, 0);   // orden 0: versión del SDK de GoldHEN
    long r = LlamadaSistema(500, 2, (long)respaldo);
    afuera = r == 0;
    if (afuera) snprintf(accesoDetalle, sizeof accesoDetalle, "GoldHEN dio acceso al USB (SDK 0x%lX).", version);
    else snprintf(accesoDetalle, sizeof accesoDetalle, "GoldHEN no dio acceso al USB (código %ld, SDK 0x%lX).", r, version);
    char txt[160];
    snprintf(txt, sizeof txt, "%s\n%s\n", afuera ? "ok" : "fallo", accesoDetalle);
    EscribirTexto(rutaAcceso, txt);
    return afuera ? 1 : 0;
}

// Varios hilos pueden pedir el acceso a la vez (la búsqueda de juegos en segundo plano y abrir un juego): se cuenta
// cuántos lo están usando y se vuelve al encierro recién cuando lo suelta el último.
static std::atomic_flag candado = ATOMIC_FLAG_INIT;
static int usuariosAcceso;
static void Tomar() { while (candado.test_and_set(std::memory_order_acquire)) sceKernelUsleep(200); }
static void Soltar() { candado.clear(std::memory_order_release); }

int AbrirAccesoCompleto(const char* quien, bool insistir)
{
    Tomar();
    int r;
    if (afuera) { usuariosAcceso++; r = 1; }
    else
    {
        r = AbrirAccesoInterno(quien, insistir);
        usuariosAcceso = afuera ? 1 : 0;
    }
    Soltar();
    return r;
}

void CerrarAccesoCompleto()
{
    Tomar();
    if (afuera && --usuariosAcceso <= 0)
    {
        LlamadaSistema(500, 3, (long)respaldo);   // orden 3: UNJAILBREAK, vuelve a como estaba
        afuera = false;
        usuariosAcceso = 0;
    }
    Soltar();
}

int ErrorListado() { return errorListado; }

void Dormir(int ms) { sceKernelUsleep((unsigned)ms * 1000); }

int ListarCarpeta(const char* ruta, void (*f)(const char*, bool, void*), void* u)
{
#ifdef SIN_LISTADO
    (void)ruta; (void)f; (void)u;
    return -1;
#else
    DIR* d = opendir(ruta);
    if (!d) { errorListado = errno; return -1; }
    int n = 0;
    char completa[1024];
    while (struct dirent* e = readdir(d))
    {
        if (e->d_name[0] == '.' && (!e->d_name[1] || (e->d_name[1] == '.' && !e->d_name[2]))) continue;
        bool carpeta = e->d_type == DT_DIR;
        if (e->d_type == DT_UNKNOWN)   // algunos sistemas de archivos no lo dicen: se prueba abrirla
        {
            snprintf(completa, sizeof completa, "%s/%s", ruta, e->d_name);
            if (DIR* s = opendir(completa)) { carpeta = true; closedir(s); }
        }
        f(e->d_name, carpeta, u);
        n++;
    }
    closedir(d);
    return n;
#endif
}

int LeerTexto(const char* ruta, char* buf, int max)
{
    FILE* f = fopen(ruta, "rb");
    if (!f) return -1;
    int n = (int)fread(buf, 1, max - 1, f);
    fclose(f);
    if (n < 0) n = 0;
    buf[n] = 0;
    return n;
}

bool EscribirTexto(const char* ruta, const char* texto)
{
    FILE* f = fopen(ruta, "wb");
    if (!f) return false;
    size_t n = strlen(texto);
    bool ok = fwrite(texto, 1, n, f) == n;
    fclose(f);
    return ok;
}
