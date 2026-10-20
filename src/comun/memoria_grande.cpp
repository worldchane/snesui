// Los bloques grandes de memoria (4 MB o más: las roms y tablas de los núcleos de CPS3, N64, arcade...) salen de la
// memoria directa de la consola (varios GB) y no del montón de la librería de C, que comparte con el programa un
// presupuesto chico (la memoria "flexible"): con N64 en el programa, al abrir un juego grande ya no alcanzaba.
// El enlazador redirige malloc, calloc, realloc y free hacia estas (--wrap en construir.sh); lo demás sigue igual.
// Si la memoria directa falla, se usa el montón de siempre. Solo para la consola.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <atomic>

#include <orbis/libkernel.h>

#ifdef MEMORIA_DIRECTA   // con --wrap=malloc,calloc,realloc,free al enlazar (config.sh, ENVOLVER_MEMORIA)
extern "C" {
void* __real_malloc(size_t);
void* __real_calloc(size_t, size_t);
void* __real_realloc(void*, size_t);
void __real_free(void*);
}

static const size_t GRANDE = 4u << 20, PAGINA = 64u << 10;
static const int MAX_BLOQUES = 512;
struct Bloque { uint8_t* p; size_t tam, mapeado; off_t fisica; };
static Bloque bloques[MAX_BLOQUES];
static int nBloques;
static uintptr_t menor = UINTPTR_MAX, mayor;   // rango de los bloques: free de los chicos ni recorre la tabla
static size_t enUso, fallos;
static std::atomic_flag candado = ATOMIC_FLAG_INIT;
static void Tomar() { while (candado.test_and_set(std::memory_order_acquire)) {} }
static void Soltar() { candado.clear(std::memory_order_release); }

// Liberar: por nombre en libkernel (como las demás funciones nuevas: no se suman a las que pide el programa).
static int (*desmapear)(void*, size_t);
static int (*liberarDirecta)(off_t, size_t);
static bool buscadas;
static void Buscar()
{
    if (buscadas) return;
    buscadas = true;
    if (sceKernelDlsym(0x2001, "sceKernelMunmap", (void**)&desmapear) != 0) desmapear = NULL;
    if (sceKernelDlsym(0x2001, "sceKernelReleaseDirectMemory", (void**)&liberarDirecta) != 0) liberarDirecta = NULL;
}

static void* Grande(size_t n)
{
    Buscar();   // afuera del candado: sceKernelDlsym no vuelve a entrar acá, pero así ni se arriesga
    Tomar();
    // Sin forma de devolverla, no se pide (quedaría ocupada para siempre).
    if (!desmapear || !liberarDirecta || nBloques == MAX_BLOQUES) { Soltar(); return NULL; }
    size_t m = (n + PAGINA - 1) & ~(PAGINA - 1);
    off_t fisica = 0;
    void* p = NULL;
    if (sceKernelAllocateDirectMemory(0, 0x180000000u, m, PAGINA, 0 /* WB_ONION: la del procesador */, &fisica) != 0)
    {
        fallos++;
        Soltar();
        return NULL;
    }
    if (sceKernelMapDirectMemory(&p, m, 0x03 /* lectura y escritura del procesador */, 0, fisica, PAGINA) != 0 || !p)
    {
        liberarDirecta(fisica, m);
        fallos++;
        Soltar();
        return NULL;
    }
    bloques[nBloques++] = {(uint8_t*)p, n, m, fisica};
    if ((uintptr_t)p < menor) menor = (uintptr_t)p;
    if ((uintptr_t)p + m > mayor) mayor = (uintptr_t)p + m;
    enUso += m;
    Soltar();
    return p;
}

// El bloque grande que empieza en p (-1 si no es uno de estos). Llamar con el candado tomado.
static int Indice(const void* p)
{
    if ((uintptr_t)p < menor || (uintptr_t)p >= mayor) return -1;
    for (int i = 0; i < nBloques; i++) if (bloques[i].p == p) return i;
    return -1;
}

extern "C" void* __wrap_malloc(size_t n)
{
    if (n >= GRANDE)
        if (void* p = Grande(n)) return p;
    return __real_malloc(n);
}

extern "C" void* __wrap_calloc(size_t a, size_t b)
{
    if (b && a > SIZE_MAX / b) return NULL;
    size_t n = a * b;
    if (n >= GRANDE)
        if (void* p = Grande(n)) { memset(p, 0, n); return p; }
    return __real_calloc(a, b);
}

extern "C" void __wrap_free(void* p)
{
    if (!p) return;
    Tomar();
    int i = Indice(p);
    if (i < 0) { Soltar(); __real_free(p); return; }
    Bloque b = bloques[i];
    bloques[i] = bloques[--nBloques];
    enUso -= b.mapeado;
    Soltar();
    desmapear(b.p, b.mapeado);
    liberarDirecta(b.fisica, b.mapeado);
}

extern "C" void* __wrap_realloc(void* p, size_t n)
{
    if (!p) return __wrap_malloc(n);
    if (!n) { __wrap_free(p); return NULL; }
    Tomar();
    int i = Indice(p);
    size_t antes = i >= 0 ? bloques[i].tam : 0, cabe = i >= 0 ? bloques[i].mapeado : 0;
    if (i >= 0 && n <= cabe) { bloques[i].tam = n; Soltar(); return p; }   // ya entra en lo mapeado
    Soltar();
    if (i < 0)
    {
        // Del montón: sigue en el montón (no se sabe cuánto medía para copiarlo a otro lado).
        return __real_realloc(p, n);
    }
    void* q = __wrap_malloc(n);
    if (!q) return NULL;
    memcpy(q, p, antes < n ? antes : n);
    __wrap_free(p);
    return q;
}

#else
static size_t enUso, fallos;
static int nBloques;
static void Tomar() {}
static void Soltar() {}
#endif

// Para el registro: memoria flexible libre (la del montón y el programa) y la directa que usan los bloques grandes.
extern "C" void MemoriaResumen(char* sale, int max)
{
    static int (*flexibleLibre)(size_t*) = NULL;
    static bool buscada = false;
    if (!buscada)
    {
        buscada = true;
        if (sceKernelDlsym(0x2001, "sceKernelAvailableFlexibleMemorySize", (void**)&flexibleLibre) != 0) flexibleLibre = NULL;
    }
    size_t libre = 0;
    bool hay = flexibleLibre && flexibleLibre(&libre) == 0;
    Tomar();
    size_t uso = enUso, f = fallos;
    int n = nBloques;
    Soltar();
    if (hay)
        snprintf(sale, max, "memoria flexible libre %zu MB; bloques grandes en memoria directa: %d (%zu MB), %zu fallidos",
                 libre >> 20, n, uso >> 20, f);
    else
        snprintf(sale, max, "bloques grandes en memoria directa: %d (%zu MB), %zu fallidos", n, uso >> 20, f);
}
