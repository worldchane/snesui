// Ver copiar.h.
#include "copiar.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"
#include "biblioteca.h"

static ProgresoCopia progreso;
static std::atomic<bool> enCurso{false};
static char (*rutas)[512];
static int nRutas;
static char destino[256];
static uint64_t reserva;

ProgresoCopia ProgresoDeCopia() { return progreso; }
bool CopiaEnCurso() { return enCurso.load(std::memory_order_acquire); }

bool EspacioConsola(uint64_t* libres)
{
    CrearCarpetas(CARPETA_JUEGOS_CONSOLA);
    return EspacioLibre(CARPETA_JUEGOS_CONSOLA, libres);
}

void TextoTamano(uint64_t bytes, char* out, int max)
{
    double mb = bytes / (1024.0 * 1024.0);
    if (mb < 1000) snprintf(out, max, "%.0f MB", mb < 1 && bytes ? 1.0 : mb);
    else
    {
        // Con coma decimal, como se escribe en español.
        snprintf(out, max, "%.1f GB", mb / 1024.0);
        for (char* p = out; *p; p++) if (*p == '.') *p = ',';
    }
}

bool CarpetaUsb(char* out, int max, uint64_t* libresUsb)
{
    // El primer USB que se pueda abrir. Los juegos copiados van a su carpeta SNES (la búsqueda la recorre igual).
    static const char* const USB[] = {"/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3"};
    AbrirAccesoCompleto("snes");
    bool hay = false;
    for (const char* u : USB)
    {
        uint64_t libres = 0;
        if (ListarCarpeta(u, [](const char*, bool, void*) {}, NULL) >= 0 && EspacioLibre(u, &libres))
        {
            snprintf(out, max, "%s/SNES", u);
            if (libresUsb) *libresUsb = libres;
            hay = true;
            break;
        }
    }
    CerrarAccesoCompleto();
    return hay;
}

bool BorrarDeConsola(const char* ruta)
{
    size_t n = strlen(CARPETA_JUEGOS_CONSOLA);
    if (strncmp(ruta, CARPETA_JUEGOS_CONSOLA, n) || ruta[n] != '/') return false;   // nunca fuera de esa carpeta
    return BorrarArchivo(ruta);
}

static const char* NombreArchivo(const char* ruta)
{
    const char* b = strrchr(ruta, '/');
    return b ? b + 1 : ruta;
}

// Copia un archivo en bloques de 1 MB, sumando al progreso. false si falló (la copia incompleta se borra).
static bool CopiarArchivo(const char* origen, const char* destino, uint8_t* buf, int tamBuf)
{
    FILE* in = AbrirArchivo(origen, "rb");
    if (!in) return false;
    FILE* out = AbrirArchivo(destino, "wb");
    if (!out) { fclose(in); return false; }
    bool ok = true;
    for (;;)
    {
        size_t n = fread(buf, 1, tamBuf, in);
        if (n == 0) break;
        if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
        progreso.bytesHechos += n;
    }
    fclose(in);
    if (fclose(out) != 0) ok = false;
    if (!ok) BorrarArchivo(destino);
    return ok;
}

static void* HiloCopia(void*)
{
    static const int BUF = 1024 * 1024;
    uint8_t* buf = (uint8_t*)malloc(BUF);
    AbrirAccesoCompleto("snes");   // para leer y escribir el USB
    CrearCarpetas(destino);

    // Tamaños, y cuáles ya están copiados (mismo nombre y tamaño en la consola).
    uint64_t* tam = (uint64_t*)calloc(nRutas ? nRutas : 1, sizeof(uint64_t));
    bool* hacer = (bool*)calloc(nRutas ? nRutas : 1, sizeof(bool));
    uint64_t libres = 0;
    bool medido = buf && tam && hacer && EspacioLibre(destino, &libres);
    if (!medido)
    {
        snprintf(progreso.detalle, sizeof progreso.detalle,
                 progreso.alUsb ? "No se pudo medir el espacio libre del USB. Por seguridad, no se copió nada."
                                : "No se pudo medir el espacio libre de la consola. Por seguridad, no se copió nada.");
        progreso.fallo = true;
    }
    else
    {
        // Se eligen los que entran, en orden, dejando siempre la reserva para el sistema.
        uint64_t usado = 0;
        for (int i = 0; i < nRutas; i++)
        {
            char dst[600];
            snprintf(dst, sizeof dst, "%s/%s", destino, NombreArchivo(rutas[i]));
            uint64_t t = 0, td = 0;
            if (!ExisteArchivo(rutas[i], &t)) { progreso.errores++; continue; }
            if (ExisteArchivo(dst, &td) && td == t) { progreso.yaEstaban++; continue; }
            if (usado + t + reserva > libres) { progreso.sinEspacio++; continue; }
            usado += t;
            tam[i] = t;
            hacer[i] = true;
        }
        progreso.bytesTotal = usado;
        if (!usado && progreso.sinEspacio)
        {
            char l[32];
            TextoTamano(libres > reserva ? libres - reserva : 0, l, sizeof l);
            if (progreso.alUsb)
                snprintf(progreso.detalle, sizeof progreso.detalle, "No hay espacio suficiente en el USB (quedan %s disponibles).", l);
            else
                snprintf(progreso.detalle, sizeof progreso.detalle,
                         "No hay espacio suficiente en la consola (quedan %s disponibles, sin contar 1 GB que se deja para el sistema).", l);
            progreso.fallo = true;
        }
        for (int i = 0; i < nRutas; i++)
        {
            if (!hacer[i]) { continue; }
            snprintf(progreso.actual, sizeof progreso.actual, "%s", NombreArchivo(rutas[i]));
            char dst[600];
            snprintf(dst, sizeof dst, "%s/%s", destino, NombreArchivo(rutas[i]));
            uint64_t antes = progreso.bytesHechos, final = 0;
            bool ok = CopiarArchivo(rutas[i], dst, buf, BUF);
            // Verificación: la copia tiene que tener exactamente el tamaño del original.
            if (ok && (!ExisteArchivo(dst, &final) || final != tam[i])) { ok = false; BorrarArchivo(dst); }
            if (ok) progreso.copiados++;
            else
            {
                progreso.errores++;
                progreso.bytesHechos = antes + tam[i];   // para que la barra siga avanzando
                snprintf(progreso.detalle, sizeof progreso.detalle, "No se pudo copiar %s.", NombreArchivo(rutas[i]));
            }
            progreso.hechos++;
        }
    }
    CerrarAccesoCompleto();
    free(buf);
    free(tam);
    free(hacer);
    progreso.activa = false;
    progreso.terminada = true;
    enCurso.store(false, std::memory_order_release);
    return NULL;
}

// ---------- borrar varios ----------
static bool EsRomBorrable(const char* r)
{
    size_t n = strlen(CARPETA_JUEGOS_CONSOLA);
    bool lugar = (!strncmp(r, CARPETA_JUEGOS_CONSOLA, n) && r[n] == '/') || !strncmp(r, "/mnt/usb", 8);
    const char* e = strrchr(r, '.');
    if (!lugar || !e || strstr(r, "/../")) return false;
    char ext[8] = "";
    for (int i = 0; i < 7 && e[1 + i]; i++) ext[i] = (char)(e[1 + i] | 0x20), ext[i + 1] = 0;
    return !strcmp(ext, "sfc") || !strcmp(ext, "smc") || !strcmp(ext, "fig") || !strcmp(ext, "swc");
}

static void* HiloBorrado(void*)
{
    AbrirAccesoCompleto("snes");   // para el USB
    for (int i = 0; i < nRutas; i++)
    {
        snprintf(progreso.actual, sizeof progreso.actual, "%s", NombreArchivo(rutas[i]));
        uint64_t t = 0;
        ExisteArchivo(rutas[i], &t);
        if (EsRomBorrable(rutas[i]) && BorrarArchivo(rutas[i])) { progreso.copiados++; progreso.bytesHechos += t; }
        else progreso.errores++;
        progreso.hechos++;
    }
    CerrarAccesoCompleto();
    if (progreso.errores) snprintf(progreso.detalle, sizeof progreso.detalle, "%d juegos no se pudieron borrar.", progreso.errores);
    progreso.activa = false;
    progreso.terminada = true;
    enCurso.store(false, std::memory_order_release);
    return NULL;
}

static bool Preparar(const char* const* lista, int n)
{
    free(rutas);
    rutas = (char(*)[512])malloc(sizeof(*rutas) * n);
    if (!rutas) return false;
    nRutas = n;
    for (int i = 0; i < n; i++) snprintf(rutas[i], sizeof rutas[i], "%s", lista[i]);
    memset(&progreso, 0, sizeof progreso);
    progreso.total = n;
    progreso.activa = true;
    return true;
}

bool EmpezarBorrado(const char* const* lista, int n)
{
    if (enCurso.load(std::memory_order_acquire) || n <= 0 || !Preparar(lista, n)) return false;
    progreso.borrando = true;
    progreso.alUsb = !strncmp(lista[0], "/mnt/usb", 8);
    enCurso.store(true, std::memory_order_release);
    if (!Lanzar(HiloBorrado, NULL, "borrar")) { enCurso.store(false, std::memory_order_release); progreso.activa = false; return false; }
    return true;
}

bool EmpezarCopia(const char* const* lista, int n, const char* carpeta, uint64_t res)
{
    if (enCurso.load(std::memory_order_acquire) || n <= 0) return false;
    free(rutas);
    rutas = (char(*)[512])malloc(sizeof(*rutas) * n);
    if (!rutas) return false;
    nRutas = n;
    for (int i = 0; i < n; i++) snprintf(rutas[i], sizeof rutas[i], "%s", lista[i]);
    memset(&progreso, 0, sizeof progreso);
    snprintf(destino, sizeof destino, "%s", carpeta);
    reserva = res;
    progreso.alUsb = !strncmp(carpeta, "/mnt/usb", 8);
    progreso.total = n;
    progreso.activa = true;
    enCurso.store(true, std::memory_order_release);
    if (!Lanzar(HiloCopia, NULL, "copiar"))
    {
        enCurso.store(false, std::memory_order_release);
        progreso.activa = false;
        return false;
    }
    return true;
}
