// Prueba de memoria ejecutable (para el recompilador de N64). Solo con -DPRUEBA_JIT. Cada parte escribe "devolver 42"
// en código de máquina en un tipo de memoria y lo ejecuta:
//   1) memoria anónima con lectura, escritura y ejecución (mmap RWX; ya se sabe que anda en la consola del usuario);
//   2) memoria fija del programa (un búfer estático, como el caché de código del recompilador de N64 dentro de g_dev)
//      pasada a ejecutable con mprotect;
//   3) memoria RWX pedida cerca del código del programa (a menos de 2 GB: el recompilador salta a sus funciones con
//      saltos relativos de 32 bits).
// Las funciones se buscan por nombre (sceKernelDlsym), así el programa no pide funciones nuevas para abrir. Antes de cada
// parte se anota cuál se está probando: si cierra el programa, la vez siguiente se informa y no se repite.
#include "prueba_jit.h"

#ifdef PRUEBA_JIT
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <orbis/libkernel.h>

#include "../comun/plataforma.h"
#include "registro.h"

namespace {

typedef void* (*FnMmap)(void* dir, size_t tam, int prot, int flags, int fd, long desde);
typedef int (*FnMprotect)(void* dir, size_t tam, int prot);

const int P_LEER = 1, P_ESCRIBIR = 2, P_EJECUTAR = 4;
const int M_ANONIMA = 0x1000, M_PRIVADA = 2;
const size_t TAM = 0x10000;

// "mov eax, 42; ret"
const unsigned char CODIGO[6] = {0xB8, 42, 0, 0, 0, 0xC3};
alignas(16384) unsigned char fija[TAM];   // memoria fija del programa (.bss); 16 KB = la página de la PS4 (más alineación y la PS4 no abre el programa)

char resultado[240];

}   // namespace

const char* PruebaJit()
{
    if (resultado[0]) return resultado;
    FnMmap mapear = NULL;
    FnMprotect proteger = NULL;
    if (sceKernelDlsym(0x2001, "mmap", (void**)&mapear) != 0) mapear = NULL;
    if (sceKernelDlsym(0x2001, "mprotect", (void**)&proteger) != 0) proteger = NULL;
    if (!mapear || !proteger) { snprintf(resultado, sizeof resultado, "JIT: no se encontraron mmap o mprotect."); return resultado; }

    char ruta[160], antes[64] = "";
    snprintf(ruta, sizeof ruta, "%s/prueba_jit2.txt", CARPETA_DATOS);
    LeerTexto(ruta, antes, sizeof antes);
    int cerro = antes[0] == 'p' ? antes[9] - '0' : 0;   // "probando N": la parte N cerró el programa
    int saltar = antes[0] == 'c' ? antes[6] - '0' : 0;  // "cerro  N": ya se sabe, no se repite
    if (cerro) { saltar = cerro; snprintf(antes, sizeof antes, "cerro  %d\n", cerro); EscribirTexto(ruta, antes); }

    const char* que[4] = {"", "", "", ""};
    static char textos[4][64];
    for (int parte = 1; parte <= 3; parte++)
    {
        if (parte == saltar) { snprintf(textos[parte], 64, "cerró el programa"); que[parte] = textos[parte]; continue; }
        char marca[32];
        snprintf(marca, sizeof marca, "probando %d\n", parte);
        EscribirTexto(ruta, marca);
        int valor = -1;
        void* donde = NULL;
        if (parte == 1)
        {
            void* m = mapear(NULL, TAM, P_LEER | P_ESCRIBIR | P_EJECUTAR, M_PRIVADA | M_ANONIMA, -1, 0);
            if (m != (void*)-1 && m) { donde = m; memcpy(m, CODIGO, sizeof CODIGO); valor = ((int (*)())m)(); }
        }
        else if (parte == 2)
        {
            int r = proteger(fija, TAM, P_LEER | P_ESCRIBIR | P_EJECUTAR);
            Registrar("Prueba JIT 2: mprotect de la memoria fija = %d", r);
            if (r == 0) { donde = fija; memcpy(fija, CODIGO, sizeof CODIGO); valor = ((int (*)())(void*)fija)(); }
        }
        else
        {
            // Cerca del código: a 256 MB de esta función (redondeado a 2 MB), como sugerencia (sin MAP_FIXED).
            uintptr_t aqui = (uintptr_t)(void*)&PruebaJit;
            void* pedido = (void*)(((aqui + (256u << 20)) + 0x1FFFFF) & ~(uintptr_t)0x1FFFFF);
            void* m = mapear(pedido, TAM, P_LEER | P_ESCRIBIR | P_EJECUTAR, M_PRIVADA | M_ANONIMA, -1, 0);
            if (m != (void*)-1 && m)
            {
                donde = m;
                intptr_t dist = (intptr_t)m - (intptr_t)aqui;
                Registrar("Prueba JIT 3: pedida %p, dada %p (a %lld MB del código)", pedido, m, (long long)(dist >> 20));
                memcpy(m, CODIGO, sizeof CODIGO);
                valor = ((int (*)())m)();
                if (valor == 42 && (dist > 0x7FFF0000LL || dist < -0x7FFF0000LL)) valor = -2;   // anda, pero lejos
            }
        }
        snprintf(textos[parte], 64, valor == 42 ? "FUNCIONA" : valor == -2 ? "anda pero lejos del código" : "no funciona");
        que[parte] = textos[parte];
        Registrar("Prueba JIT %d: %s (%p)", parte, que[parte], donde);
    }
    if (!saltar) EscribirTexto(ruta, "terminada\n");
    else { snprintf(antes, sizeof antes, "cerro  %d\n", saltar); EscribirTexto(ruta, antes); }
    snprintf(resultado, sizeof resultado, "JIT · RWX: %s · memoria fija: %s · cerca del código: %s", que[1], que[2], que[3]);
    Registrar("%s", resultado);
    return resultado;
}
#else
const char* PruebaJit() { return ""; }
#endif
