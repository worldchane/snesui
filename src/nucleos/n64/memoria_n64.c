/* ALLESIU: ver memoria_n64.h. Con N64_DISPOSITIVO_DINAMICO (la consola) la máquina se pide al abrir el núcleo; sin él
   (la PC) es la de siempre, fija, y estas funciones no hacen nada. */
#include <stdlib.h>
#include <string.h>
#include "device/device.h"

#ifdef N64_DISPOSITIVO_DINAMICO
extern struct device* n64_dispositivo;   /* main.c: "struct device g_dev;" con memoria_n64.h */

/* El recompilador de x86-64 apunta a la máquina por su dirección fija (linkage_x64.asm, con direcciones relativas a la
   instrucción) y el código que genera llama a funciones del programa con saltos de ±2 GB. Por eso, si se puede, la
   máquina va en n64_g_dev: una dirección que fija el enlazador (--defsym en config.sh) 256 MB después del principio
   del programa, justo pasado su final y sin memoria detrás. Al abrir el núcleo se pide ahí memoria con permiso de
   ejecución (la prueba JIT 3 mostró que la consola la da); si no se puede, la máquina va donde sea y se usa el
   intérprete. mmap y munmap se buscan por nombre en libkernel (no se suman a las funciones que pide el programa). */
#undef g_dev
extern char n64_g_dev[];
int sceKernelDlsym(int modulo, const char* nombre, void** direccion);

static int fija;   /* la máquina está en n64_g_dev */

static void* (*mapear)(void*, size_t, int, int, int, long);
static int (*desmapear)(void*, size_t);

/* Prepara la máquina (si no estaba). Devuelve 1 si quedó en n64_g_dev: el recompilador se puede usar. */
int n64_dispositivo_preparar(void)
{
    if (n64_dispositivo) return fija;
    if (!mapear && sceKernelDlsym(0x2001, "mmap", (void**)&mapear) != 0) mapear = NULL;
    if (!desmapear && sceKernelDlsym(0x2001, "munmap", (void**)&desmapear) != 0) desmapear = NULL;
    if (mapear && desmapear)
    {
        /* Lectura, escritura y ejecución (1 | 2 | 4); privada y anónima (2 | 0x1000). La dirección, como sugerencia. */
        void* m = mapear(n64_g_dev, sizeof(struct device), 1 | 2 | 4, 2 | 0x1000, -1, 0);
        if (m == (void*)n64_g_dev)
        {
            fija = 1;
            n64_dispositivo = (struct device*)m;   /* la memoria anónima ya viene en cero */
            return 1;
        }
        if (m && m != (void*)-1) desmapear(m, sizeof(struct device));
    }
    fija = 0;
    n64_dispositivo = (struct device*)calloc(1, sizeof(struct device));
    return 0;
}

void n64_dispositivo_pedir(void)
{
    if (!n64_dispositivo) n64_dispositivo_preparar();
}

void n64_dispositivo_soltar(void)
{
    if (fija && desmapear) desmapear(n64_dispositivo, sizeof(struct device));
    else free(n64_dispositivo);
    n64_dispositivo = NULL;
    fija = 0;
}
#else
int n64_dispositivo_preparar(void) { return 1; }   /* en la PC la máquina es fija: el recompilador anda */
void n64_dispositivo_pedir(void) {}
void n64_dispositivo_soltar(void) {}
#endif
