// Ver respaldo.h. Copia archivo por archivo (las carpetas, hasta 3 niveles), sin las carátulas ni los juegos (ocupan
// mucho y se vuelven a conseguir solos).
#include "respaldo.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"
#include "copiar.h"
#include "registro.h"

static std::atomic<bool> enCurso{false}, terminado{false};
static bool restaurando, exito;
static int copiados, fallidos;
static char resultado[160];

static bool CopiarArchivo(const char* de, const char* a)
{
    uint64_t tam = 0;
    if (!ExisteArchivo(de, &tam) || tam > 256u << 20) return false;
    FILE* f = AbrirArchivo(de, "rb");
    if (!f) return false;
    FILE* g = AbrirArchivo(a, "wb");
    if (!g) { fclose(f); return false; }
    static uint8_t buf[256 * 1024];
    bool ok = true;
    for (uint64_t hecho = 0; hecho < tam && ok;)
    {
        size_t n = fread(buf, 1, sizeof buf, f);
        if (!n) { ok = false; break; }
        ok = fwrite(buf, 1, n, g) == n;
        hecho += n;
    }
    fclose(f);
    fclose(g);
    if (!ok) BorrarArchivo(a);
    return ok;
}

struct Copia { char de[400], a[400]; int prof; };
static void AlListar(const char* nombre, bool carpeta, void* u)
{
    const Copia& c = *(const Copia*)u;
    Copia sub = c;
    snprintf(sub.de, sizeof sub.de, "%s/%s", c.de, nombre);
    snprintf(sub.a, sizeof sub.a, "%s/%s", c.a, nombre);
    if (carpeta)
    {
        if (c.prof >= 3) return;
        sub.prof++;
        CrearCarpetas(sub.a);
        ListarCarpeta(sub.de, AlListar, &sub);
        return;
    }
    if (CopiarArchivo(sub.de, sub.a)) copiados++;
    else fallidos++;
}

static void CopiarCarpeta(const char* de, const char* a)
{
    static Copia c;
    snprintf(c.de, sizeof c.de, "%s", de);
    snprintf(c.a, sizeof c.a, "%s", a);
    c.prof = 0;
    CrearCarpetas(a);
    ListarCarpeta(c.de, AlListar, &c);
}

static void* Hilo(void*)
{
    copiados = fallidos = 0;
    char usb[160];
    exito = false;
    if (!CarpetaUsb(usb, sizeof usb))   // <usb>/roms
        snprintf(resultado, sizeof resultado, "No hay un USB conectado.");
    else
    {
        char* barra = strrchr(usb, '/');
        if (barra) *barra = 0;
        AbrirAccesoCompleto("snes");
        char respaldo[200];
        snprintf(respaldo, sizeof respaldo, "%s/ALLESIU-respaldo", usb);
        static const char* const ARCHIVOS[] = {"favoritos.txt", "recientes.txt", "ajustes.txt", "ajustes_consolas.txt"};
        char de[300], a[300];
        if (!restaurando)
        {
            snprintf(de, sizeof de, "%s/guardados", CARPETA_DATOS);
            snprintf(a, sizeof a, "%s/guardados", respaldo);
            CopiarCarpeta(de, a);
            for (const char* n : ARCHIVOS)
            {
                snprintf(de, sizeof de, "%s/%s", CARPETA_DATOS, n);
                snprintf(a, sizeof a, "%s/%s", respaldo, n);
                if (ExisteArchivo(de)) { if (CopiarArchivo(de, a)) copiados++; else fallidos++; }
            }
        }
        else
        {
            snprintf(de, sizeof de, "%s/guardados", respaldo);
            uint64_t t;
            bool hay = ListarCarpeta(de, [](const char*, bool, void*) {}, NULL) >= 0 || ExisteArchivo(de, &t);
            if (!hay) snprintf(resultado, sizeof resultado, "No hay un respaldo en el USB (carpeta ALLESIU-respaldo).");
            else
            {
                snprintf(a, sizeof a, "%s/guardados", CARPETA_DATOS);
                CopiarCarpeta(de, a);
                for (const char* n : ARCHIVOS)
                {
                    snprintf(de, sizeof de, "%s/%s", respaldo, n);
                    snprintf(a, sizeof a, "%s/%s", CARPETA_DATOS, n);
                    if (ExisteArchivo(de)) { if (CopiarArchivo(de, a)) copiados++; else fallidos++; }
                }
            }
        }
        CerrarAccesoCompleto();
        if (!resultado[0])
        {
            exito = copiados > 0 && !fallidos;
            snprintf(resultado, sizeof resultado, fallidos ? "%s: %d archivos copiados, %d con error." : "%s: %d archivos copiados.",
                     restaurando ? "Respaldo restaurado" : "Partidas respaldadas en el USB", copiados, fallidos);
            if (!copiados && !fallidos) snprintf(resultado, sizeof resultado, "No había nada que copiar.");
        }
        Registrar("%s", resultado);
    }
    terminado.store(true, std::memory_order_release);
    enCurso.store(false, std::memory_order_release);
    return NULL;
}

bool RespaldoEmpezar(bool restaurar)
{
    if (enCurso.load()) return false;
    restaurando = restaurar;
    resultado[0] = 0;
    terminado = false;
    enCurso = true;
    if (!Lanzar(Hilo, NULL, "respaldo")) { enCurso = false; return false; }
    return true;
}

bool RespaldoEnCurso() { return enCurso.load(std::memory_order_acquire); }

bool RespaldoTerminado(char* texto, int max, bool* ok)
{
    if (!terminado.exchange(false, std::memory_order_acq_rel)) return false;
    snprintf(texto, max, "%s", resultado);
    if (ok) *ok = exito;
    return true;
}
