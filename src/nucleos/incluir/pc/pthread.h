/* pthread.h mínimo para compilar Mupen64Plus-Next en la PC (Windows no lo trae): candados y crear/esperar un hilo, lo
 * único que usa (el guardado de estados y el dibujo en otro hilo, que en ALLESIU va apagado). Solo para la PC. */
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef HANDLE pthread_t;
typedef SRWLOCK pthread_mutex_t;
typedef int pthread_attr_t, pthread_mutexattr_t;
#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT

static inline int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a) { (void)a; InitializeSRWLock(m); return 0; }
static inline int pthread_mutex_destroy(pthread_mutex_t* m) { (void)m; return 0; }
static inline int pthread_mutex_lock(pthread_mutex_t* m) { AcquireSRWLockExclusive(m); return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t* m) { ReleaseSRWLockExclusive(m); return 0; }

struct HiloPcInicio { void* (*f)(void*); void* arg; };
static DWORD WINAPI HiloPc(LPVOID p)
{
    struct HiloPcInicio i = *(struct HiloPcInicio*)p;
    HeapFree(GetProcessHeap(), 0, p);
    i.f(i.arg);
    return 0;
}
static inline int pthread_create(pthread_t* t, const pthread_attr_t* a, void* (*f)(void*), void* arg)
{
    (void)a;
    struct HiloPcInicio* i = (struct HiloPcInicio*)HeapAlloc(GetProcessHeap(), 0, sizeof *i);
    i->f = f;
    i->arg = arg;
    *t = CreateThread(NULL, 0, HiloPc, i, 0, NULL);
    return *t ? 0 : -1;
}
static inline int pthread_join(pthread_t t, void** r) { (void)r; WaitForSingleObject(t, INFINITE); CloseHandle(t); return 0; }
