/* ALLESIU: ver memoria_n64.h. Con N64_DISPOSITIVO_DINAMICO (la consola) la máquina se pide al abrir el núcleo; sin él
   (la PC) es la de siempre, fija, y estas funciones no hacen nada. */
#include <stdlib.h>
#include "device/device.h"

#ifdef N64_DISPOSITIVO_DINAMICO
extern struct device* n64_dispositivo;   /* main.c: "struct device g_dev;" con memoria_n64.h */

void n64_dispositivo_pedir(void)
{
    if (!n64_dispositivo) n64_dispositivo = (struct device*)calloc(1, sizeof(struct device));
}

void n64_dispositivo_soltar(void)
{
    free(n64_dispositivo);
    n64_dispositivo = NULL;
}

/* El recompilador de x86-64 (linkage_x64.asm) apunta a la máquina por su dirección fija; con el intérprete (el que usa
   la app) no se ejecuta, pero tiene que enlazar. */
#undef g_dev
char n64_g_dev[64];
#else
void n64_dispositivo_pedir(void) {}
void n64_dispositivo_soltar(void) {}
#endif
