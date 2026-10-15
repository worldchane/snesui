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
    CrearCarpetas(CARPETA_JUEGOS_CONSOLA);
    AbrirAccesoCompleto("snes");   // para leer el USB

    // Tamaños, y cuáles ya están copiados (mismo nombre y tamaño en la consola).
    uint64_t* tam = (uint64_t*)calloc(nRutas ? nRutas : 1, sizeof(uint64_t));
    bool* hacer = (bool*)calloc(nRutas ? nRutas : 1, sizeof(bool));
    uint64_t libres = 0;
    bool medido = buf && tam && hacer && EspacioLibre(CARPETA_JUEGOS_CONSOLA, &libres);
    if (!medido)
    {
        snprintf(progreso.detalle, sizeof progreso.detalle,
                 "No se pudo medir el espacio libre de la consola. Por seguridad, no se copió nada.");
        progreso.fallo = true;
    }
    else
    {
        // Se eligen los que entran, en orden, dejando siempre la reserva para el sistema.
        uint64_t usado = 0;
        for (int i = 0; i < nRutas; i++)
        {
            char destino[600];
            snprintf(destino, sizeof destino, "%s/%s", CARPETA_JUEGOS_CONSOLA, NombreArchivo(rutas[i]));
            uint64_t t = 0, td = 0;
            if (!ExisteArchivo(rutas[i], &t)) { progreso.errores++; continue; }
            if (ExisteArchivo(destino, &td) && td == t) { progreso.yaEstaban++; continue; }
            if (usado + t + RESERVA_SISTEMA > libres) { progreso.sinEspacio++; continue; }
            usado += t;
            tam[i] = t;
            hacer[i] = true;
        }
        progreso.bytesTotal = usado;
        if (!usado && progreso.sinEspacio)
        {
            char l[32];
            TextoTamano(libres > RESERVA_SISTEMA ? libres - RESERVA_SISTEMA : 0, l, sizeof l);
            snprintf(progreso.detalle, sizeof progreso.detalle,
                     "No hay espacio suficiente en la consola (quedan %s disponibles, sin contar 1 GB que se deja para el sistema).", l);
            progreso.fallo = true;
        }
        for (int i = 0; i < nRutas; i++)
        {
            if (!hacer[i]) { continue; }
            snprintf(progreso.actual, sizeof progreso.actual, "%s", NombreArchivo(rutas[i]));
            char destino[600];
            snprintf(destino, sizeof destino, "%s/%s", CARPETA_JUEGOS_CONSOLA, NombreArchivo(rutas[i]));
            uint64_t antes = progreso.bytesHechos, final = 0;
            bool ok = CopiarArchivo(rutas[i], destino, buf, BUF);
            // Verificación: la copia tiene que tener exactamente el tamaño del original.
            if (ok && (!ExisteArchivo(destino, &final) || final != tam[i])) { ok = false; BorrarArchivo(destino); }
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

bool EmpezarCopia(const char* const* lista, int n)
{
    if (enCurso.load(std::memory_order_acquire) || n <= 0) return false;
    free(rutas);
    rutas = (char(*)[512])malloc(sizeof(*rutas) * n);
    if (!rutas) return false;
    nRutas = n;
    for (int i = 0; i < n; i++) snprintf(rutas[i], sizeof rutas[i], "%s", lista[i]);
    memset(&progreso, 0, sizeof progreso);
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
