// Perfilador de muestreo (solo Windows): corre un juego como probar_snes y, desde otro hilo, detiene el hilo del
// emulador cada ~0,5 ms y anota en qué función estaba. Al final muestra las funciones donde más tiempo pasa.
// Uso: pc/salida/perfil.exe <rom> <cuadros>
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "../src/nucleo/snes.h"
#include "../src/nucleo/interno.h"
extern Cpu sa1cpu;

// Acceso a archivos que el núcleo pide a la plataforma (como en probar_snes).
bool ExisteArchivo(const char* r, uint64_t* tam)
{
    FILE* f = fopen(r, "rb");
    if (!f) return false;
    if (tam) { _fseeki64(f, 0, SEEK_END); *tam = (uint64_t)_ftelli64(f); }
    fclose(f);
    return true;
}
FILE* AbrirArchivo(const char* r, const char* m) { return fopen(r, m); }
int LeerTexto(const char* r, char* buf, int max) { FILE* f = fopen(r, "rb"); if (!f) return -1; int n = (int)fread(buf, 1, max - 1, f); fclose(f); buf[n < 0 ? 0 : n] = 0; return n; }

int main(int argc, char** argv)
{
    if (argc < 3) { printf("uso: perfil <rom> <cuadros>\n"); return 1; }
    char error[160];
    if (!SnesCargar(argv[1], error)) { printf("no cargó: %s\n", error); return 1; }
    int cuadros = atoi(argv[2]);

    HANDLE proceso = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proceso, NULL, TRUE);
    HANDLE hilo;
    DuplicateHandle(proceso, GetCurrentThread(), proceso, &hilo, 0, FALSE, DUPLICATE_SAME_ACCESS);

    std::atomic<bool> fin{false};
    std::vector<DWORD64> muestras;
    muestras.reserve(1 << 20);
    std::map<uint32_t, int> pcSa1;   // dónde está el procesador del SA-1 en cada muestra (si el cartucho lo tiene)
    std::thread muestreador([&] {
        timeBeginPeriod(1);
        while (!fin.load())
        {
            if (SuspendThread(hilo) != (DWORD)-1)
            {
                CONTEXT c = {};
                c.ContextFlags = CONTEXT_CONTROL;
                if (GetThreadContext(hilo, &c)) muestras.push_back(c.Rip);
                if (sa1Presente) pcSa1[(uint32_t)sa1cpu.pbr << 16 | sa1cpu.pc]++;
                ResumeThread(hilo);
            }
            Sleep(0);
            for (volatile int k = 0; k < 20000; k++) {}
        }
    });

    static uint32_t pantalla[256 * 224];
    static int16_t audio[4096];
    for (int i = 0; i < cuadros; i++)
    {
        SnesMando(0, (uint16_t)((i % 180 < 6) ? 1 << 3 : 0));
        SnesCuadro(pantalla, audio, 2048);
    }
    fin = true;
    muestreador.join();

    std::map<std::string, int> porFuncion;
    char buf[sizeof(SYMBOL_INFO) + 256];
    for (DWORD64 a : muestras)
    {
        SYMBOL_INFO* s = (SYMBOL_INFO*)buf;
        s->SizeOfStruct = sizeof(SYMBOL_INFO);
        s->MaxNameLen = 255;
        DWORD64 desp = 0;
        porFuncion[SymFromAddr(proceso, a, &desp, s) ? s->Name : "?"]++;
    }
    std::vector<std::pair<int, std::string>> orden;
    for (auto& p : porFuncion) orden.push_back({p.second, p.first});
    std::sort(orden.rbegin(), orden.rend());
    printf("%zu muestras\n", muestras.size());
    for (size_t i = 0; i < orden.size() && i < 25; i++)
        printf("%5.1f%%  %s\n", orden[i].first * 100.0 / muestras.size(), orden[i].second.c_str());
    if (!pcSa1.empty())
    {
        std::vector<std::pair<int, uint32_t>> o;
        for (auto& p : pcSa1) o.push_back({p.second, p.first});
        std::sort(o.rbegin(), o.rend());
        printf("procesador del SA-1:\n");
        for (size_t i = 0; i < o.size() && i < 12; i++) printf("%5.1f%%  %02X:%04X\n", o[i].first * 100.0 / muestras.size(), o[i].second >> 16, o[i].second & 0xFFFF);
    }
    return 0;
}
