// Funciones que la librería estándar de C++ pide al sistema y que el programa trae por su cuenta (probado en la PS4 del
// usuario, FW 9.00 + GoldHEN, con Blue Chalk; ver LEEME.md).
// La librería (variables estáticas, excepciones, abort) pide pthread_once, pthread_cond_*, pthread_rwlock_*, raise,
// sigaction, sigprocmask y sched_yield. En la consola del usuario (9.00) los programas que las piden no llegan a abrir
// (CE-41839-5, igual que los ejemplos oficiales de OpenOrbis). Definidas aquí, el programa ya no se las pide.
#include <pthread.h>
#include <signal.h>

extern "C"
{
    int sceKernelUsleep(unsigned);
    static void Ceder(void) { sceKernelUsleep(0); }

    // pthread_once_t es un int: 0 = sin correr, 1 = corriendo, 2 = listo.
    int pthread_once(pthread_once_t* o, void (*f)(void))
    {
        int* a = (int*)o; int esperado = 0;
        if (__atomic_compare_exchange_n(a, &esperado, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) { f(); __atomic_store_n(a, 2, __ATOMIC_RELEASE); return 0; }
        while (__atomic_load_n(a, __ATOMIC_ACQUIRE) != 2) Ceder();
        return 0;
    }

    // Quien espera una condición siempre la vuelve a revisar en un ciclo: soltar el cerrojo un instante y avisar
    // sin hacer nada es válido (POSIX permite despertares sin aviso).
    int pthread_cond_wait(pthread_cond_t*, pthread_mutex_t* m)
    {
        pthread_mutex_unlock(m);
        sceKernelUsleep(100);
        pthread_mutex_lock(m);
        return 0;
    }
    int pthread_cond_broadcast(pthread_cond_t*) { return 0; }

    // Cerrojo de lectura y escritura como uno simple (lo usa el desenrollado de excepciones).
    static int TomarRw(void* l)
    {
        int* a = (int*)l;
        for (;;) { int e = 0; if (__atomic_compare_exchange_n(a, &e, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return 0; Ceder(); }
    }
    int pthread_rwlock_rdlock(pthread_rwlock_t* l) { return TomarRw(l); }
    int pthread_rwlock_wrlock(pthread_rwlock_t* l) { return TomarRw(l); }
    int pthread_rwlock_unlock(pthread_rwlock_t* l) { __atomic_store_n((int*)l, 0, __ATOMIC_RELEASE); return 0; }

    int sched_yield(void) { Ceder(); return 0; }

    // Solo se usan al abortar: el juego se queda detenido ahí.
    int sigaction(int, const struct sigaction*, struct sigaction*) { return 0; }
    int sigprocmask(int, const sigset_t*, sigset_t*) { return 0; }
    int raise(int) { for (;;) sceKernelUsleep(1000000); }
}

// ---------- piezas del runtime de C++ ----------
// Con estas definiciones el programa no trae de la librería de C++ las partes de excepciones, tipos y new/delete:
// los programas que las traen (el juego antes, los ejemplos oficiales) no abren en la consola del usuario.
#include <cstddef>
#include <cstdlib>
#include <new>

void* operator new(std::size_t n) { void* p = malloc(n ? n : 1); if (!p) abort(); return p; }
void* operator new[](std::size_t n) { return operator new(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return malloc(n ? n : 1); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return malloc(n ? n : 1); }
void operator delete(void* p) noexcept { free(p); }
void operator delete[](void* p) noexcept { free(p); }
void operator delete(void* p, std::size_t) noexcept { free(p); }
void operator delete[](void* p, std::size_t) noexcept { free(p); }

extern "C"
{
    // Llamada a una función virtual pura: no debería pasar nunca.
    void __cxa_pure_virtual() { abort(); }
    // Destructores de los estáticos al salir del programa: en la consola el juego no termina así, no hace falta.
    int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
}

// std::stable_sort pide memoria con std::nothrow: definido aquí para no traer new.cpp de la librería.
namespace std { const nothrow_t nothrow{}; }
