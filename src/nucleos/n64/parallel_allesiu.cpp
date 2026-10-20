// ALLESIU: reemplazo de parallel_al.cpp de Angrylion (el dibujo de N64 por software repartido en varios hilos). El
// original usa std::thread y condition_variable: en la consola std::thread cierra el programa y las condiciones son de
// mentira (sistema_ps4.cpp). Acá los hilos son los de la app (Lanzar: scePthreadCreate) y se avisan con contadores
// atómicos: el que espera da vueltas un rato (los trabajos llegan varias veces por cuadro) y después duerme de a poco.
#include <stdint.h>

#include <atomic>

#include "parallel_al.h"

bool Lanzar(void* (*f)(void*), void* arg, const char* nombre);   // src/comun/plataforma.cpp (pc/plataforma_pc.cpp)

#ifdef _WIN32
#include <thread>
static void Dormir() { std::this_thread::yield(); }
#else
extern "C" int sceKernelUsleep(unsigned);
static void Dormir() { sceKernelUsleep(100); }
#endif

static void Pausa() { __builtin_ia32_pause(); }

static const uint32_t MAX_HILOS = 4;   // el de la emulación y 3 más (la consola deja 6 núcleos a los juegos)
static uint32_t hilos = 1;
static void (*tarea)(uint32_t);
static std::atomic<uint32_t> generacion{0}, terminados{0}, vivos{0};
static std::atomic<bool> seguir{false};

// Espera a que se cumpla 'listo': vueltas cortas primero, después dormir de a poco.
template <class F> static void Esperar(F listo)
{
    for (int i = 0; !listo(); i++)
    {
        if (i < 20000) Pausa();
        else Dormir();
    }
}

static void* Trabajador(void* arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg, vista = 0;
    vivos.fetch_add(1, std::memory_order_acq_rel);
    for (;;)
    {
        Esperar([&] { return generacion.load(std::memory_order_acquire) != vista; });
        vista = generacion.load(std::memory_order_acquire);
        if (!seguir.load(std::memory_order_acquire)) break;
        tarea(id);
        terminados.fetch_add(1, std::memory_order_acq_rel);
    }
    vivos.fetch_sub(1, std::memory_order_acq_rel);
    return nullptr;
}

extern "C" void parallel_alinit(uint32_t num)
{
    if (seguir.load()) return;
    if (num == 0 || num > MAX_HILOS) num = MAX_HILOS;
    hilos = 1;
    seguir.store(true, std::memory_order_release);
    for (uint32_t i = 1; i < num; i++)
        if (Lanzar(Trabajador, (void*)(uintptr_t)i, "angrylion")) hilos++;
        else break;
    // Que todos estén esperando antes del primer trabajo.
    Esperar([] { return vivos.load(std::memory_order_acquire) == hilos - 1; });
}

extern "C" void parallel_run(void task(uint32_t))
{
    if (hilos == 1) { task(0); return; }
    tarea = task;
    terminados.store(0, std::memory_order_release);
    generacion.fetch_add(1, std::memory_order_acq_rel);
    task(0);   // la parte 0 en este hilo (el de la emulación)
    Esperar([] { return terminados.load(std::memory_order_acquire) == hilos - 1; });
}

extern "C" uint32_t parallel_num_workers(void) { return hilos; }

extern "C" void parallel_close(void)
{
    if (!seguir.load()) return;
    seguir.store(false, std::memory_order_release);
    generacion.fetch_add(1, std::memory_order_acq_rel);
    Esperar([] { return vivos.load(std::memory_order_acquire) == 0; });
    hilos = 1;
}
