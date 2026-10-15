// plataforma.h para el simulador de PC: el tiempo y el mando los maneja simular.cpp; /data y /mnt/usb0 se cambian por
// carpetas dentro de pc/ (pc/data y pc/usb).
#include "../src/comun/plataforma.h"

#include <atomic>
#include <filesystem>
#include <stdio.h>
#include <string.h>
#include <string>
#include <chrono>
#include <thread>

double tiempoSimulado;   // lo avanza simular.cpp
double Segundos() { return tiempoSimulado; }

bool Lanzar(void* (*f)(void*), void* arg, const char*)
{
    std::thread(f, arg).detach();
    return true;
}

static int nTrab;
static std::atomic<int> generacion{0}, pendientes{0};
static void (*tareaF)(int, int, void*);
static void* tareaArg;

static void* Trabajador(void* a)
{
    int i = (int)(intptr_t)a, vista = 0;
    for (;;)
    {
        int g;
        while ((g = generacion.load()) == vista) std::this_thread::yield();
        vista = g;
        tareaF(i + 1, nTrab + 1, tareaArg);
        pendientes.fetch_sub(1);
    }
}

void IniciarTrabajadores(int n)
{
    for (int i = 0; i < n; i++) Lanzar(Trabajador, (void*)(intptr_t)i, "");
    nTrab = n;
}
int Partes() { return nTrab + 1; }
void EnParalelo(void (*f)(int, int, void*), void* arg)
{
    tareaF = f;
    tareaArg = arg;
    pendientes = nTrab;
    generacion++;
    f(0, nTrab + 1, arg);
    while (pendientes.load() > 0) std::this_thread::yield();
}

void IniciarMando() {}
void LeerMando(Mando&, float) {}
void IniciarAudio() {}
bool AudioListo() { return true; }
void Sonar(Sonido) {}
void SonidosActivos(bool) {}
uint32_t LeerMando2(bool* c) { if (c) *c = false; return 0; }
void AudioJuego(const int16_t*, int) {}
void AudioJuegoVaciar() {}

static std::string Ruta(const char* r)
{
    std::string s = r;
    if (s.rfind("/data", 0) == 0) return "pc/data" + s.substr(5);
    if (s.rfind("/mnt/usb0", 0) == 0) return "pc/usb" + s.substr(9);
    if (s.rfind("/", 0) == 0) return "pc/no-existe" + s;
    return s;   // ruta relativa: tal cual (pruebas)
}

FILE* AbrirArchivo(const char* r, const char* modo) { return fopen(Ruta(r).c_str(), modo); }

bool CrearCarpeta(const char* r) { return std::filesystem::create_directories(Ruta(r)); }
bool EspacioLibre(const char* r, uint64_t* libres, uint64_t* total)
{
    std::error_code e;
    auto sp = std::filesystem::space(Ruta(r), e);
    if (e) return false;
    *libres = sp.available;
    if (total) *total = sp.capacity;
    return true;
}
bool BorrarArchivo(const char* r) { std::error_code e; return std::filesystem::remove(Ruta(r), e); }
bool CrearCarpetas(const char* r) { std::error_code e; std::filesystem::create_directories(Ruta(r), e); return std::filesystem::is_directory(Ruta(r)); }

// Red con Winsock (la misma lógica de descarga que en la consola).
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
bool RedIniciar(char* detalle, int max)
{
    WSADATA w;
    bool ok = WSAStartup(MAKEWORD(2, 2), &w) == 0;
    snprintf(detalle, max, ok ? "Red lista." : "Winsock no inició.");
    return ok;
}
int RedConectar(const char* host, int puerto)
{
    addrinfo pista = {}, *res = NULL;
    pista.ai_family = AF_INET;
    pista.ai_socktype = SOCK_STREAM;
    char p[8];
    snprintf(p, sizeof p, "%d", puerto);
    if (getaddrinfo(host, p, &pista, &res) != 0 || !res) return -1;
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    DWORD espera = 15000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&espera, sizeof espera);
    bool ok = connect(s, res->ai_addr, (int)res->ai_addrlen) == 0;
    freeaddrinfo(res);
    if (!ok) { closesocket(s); return -1; }
    return (int)s;
}
int RedEnviar(int s, const void* d, int n) { return send((SOCKET)s, (const char*)d, n, 0); }
int RedRecibir(int s, void* d, int n) { return recv((SOCKET)s, (char*)d, n, 0); }
void RedCerrar(int s) { closesocket((SOCKET)s); }

bool ExisteArchivo(const char* r, uint64_t* tam)
{
    FILE* f = fopen(Ruta(r).c_str(), "rb");
    if (!f) return false;
    if (tam) { _fseeki64(f, 0, SEEK_END); *tam = (uint64_t)_ftelli64(f); }
    fclose(f);
    return true;
}

int ErrorListado() { return 2; }
int AbrirAccesoCompleto(const char*, bool) { return 1; }
void CerrarAccesoCompleto() {}
const char* AccesoDetalle() { return "(PC)"; }
void Dormir(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

int ListarCarpeta(const char* r, void (*f)(const char*, bool, void*), void* u)
{
#ifdef SIN_LISTADO
    return -1;
#endif
    std::error_code ec;
    std::filesystem::directory_iterator it(Ruta(r), ec);
    if (ec) return -1;
    int n = 0;
    for (auto& e : it)
    {
        std::string nombre = e.path().filename().string();
        f(nombre.c_str(), e.is_directory(), u);
        n++;
    }
    return n;
}

int LeerTexto(const char* r, char* buf, int max)
{
    FILE* f = fopen(Ruta(r).c_str(), "rb");
    if (!f) return -1;
    int n = (int)fread(buf, 1, max - 1, f);
    fclose(f);
    buf[n] = 0;
    return n;
}

bool EscribirTexto(const char* r, const char* t)
{
    FILE* f = fopen(Ruta(r).c_str(), "wb");
    if (!f) return false;
    fputs(t, f);
    fclose(f);
    return true;
}
