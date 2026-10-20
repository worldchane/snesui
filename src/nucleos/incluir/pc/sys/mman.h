/* sys/mman.h mínimo para compilar Mupen64Plus-Next en la PC (Windows no lo trae): mprotect y mmap anónimo encima de
 * VirtualProtect y VirtualAlloc. Lo usa el recompilador para dar permiso de ejecución a su memoria. Solo para la PC. */
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <stddef.h>
#include <windows.h>

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_FAILED ((void*)-1)

static inline DWORD ProteccionWindows(int prot)
{
    if (prot & PROT_EXEC) return (prot & PROT_WRITE) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    if (prot & PROT_WRITE) return PAGE_READWRITE;
    return prot & PROT_READ ? PAGE_READONLY : PAGE_NOACCESS;
}
static inline int mprotect(void* dir, size_t tam, int prot)
{
    DWORD antes;
    return VirtualProtect(dir, tam, ProteccionWindows(prot), &antes) ? 0 : -1;
}
static inline void* mmap(void* dir, size_t tam, int prot, int flags, int fd, long desde)
{
    (void)flags; (void)fd; (void)desde;
    void* p = VirtualAlloc(dir, tam, MEM_COMMIT | MEM_RESERVE, ProteccionWindows(prot));
    return p ? p : MAP_FAILED;
}
static inline int munmap(void* dir, size_t tam) { (void)tam; return VirtualFree(dir, 0, MEM_RELEASE) ? 0 : -1; }
