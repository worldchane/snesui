// Ver copiar.h.
#include "registro.h"
#include "copiar.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comun/plataforma.h"
#include "arcade.h"
#include "biblioteca.h"
#include "consola.h"

static ProgresoCopia progreso;
static std::atomic<bool> enCurso{false};
static char (*rutas)[512];
static int nRutas, capRutas;
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
    // El primer USB que se pueda abrir. Los juegos copiados van a su carpeta roms (cada uno en la de su consola).
    static const char* const USB[] = {"/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3"};
    AbrirAccesoCompleto("snes");
    bool hay = false;
    for (const char* u : USB)
    {
        uint64_t libres = 0;
        if (ListarCarpeta(u, [](const char*, bool, void*) {}, NULL) >= 0 && EspacioLibre(u, &libres))
        {
            snprintf(out, max, "%s/roms", u);
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
    // Un juego arcade se copia con lo que necesita para abrir: el zip del juego del que es versión (clon) y el de su BIOS
    // (neogeo.zip...), si están en la misma carpeta. Si no, la copia de la consola no abre (FinalBurn Neo los busca en
    // la carpeta del juego).
    for (int i = 0, original = nRutas; i < original; i++)
    {
        int j = ArcadeDeRuta(rutas[i]);
        if (j < 0) continue;
        int padre = ArcadePadre(j);
        const int extra[3] = {padre, ArcadeBios(j), padre >= 0 ? ArcadeBios(padre) : -1};
        for (int e : extra)
        {
            if (e < 0 || nRutas >= capRutas) continue;
            char carpeta[512], r[600];
            snprintf(carpeta, sizeof carpeta, "%s", rutas[i]);
            char* barra = strrchr(carpeta, '/');
            if (!barra) continue;
            *barra = 0;
            if (!ArcadeZipDe(carpeta, e, r, sizeof r)) continue;   // con su nombre corto o reconocido por contenido
            bool repetido = false;
            for (int k = 0; k < nRutas && !repetido; k++) repetido = !strcmp(rutas[k], r);
            if (repetido) continue;
            snprintf(rutas[nRutas++], sizeof rutas[0], "%s", r);
            progreso.total++;
        }
    }

    // Cada juego en la carpeta de su consola, en la consola (juegos/snes, juegos/gba...) y en el USB (roms/snes...); los
    // arcade, con sus BIOS y juegos originales, todos en arcade.
    char (*carpetaDe)[300] = (char(*)[300])calloc(nRutas ? nRutas : 1, 300);
    int nLista = 0;
    const Juego* lista = BibliotecaJuegos(&nLista);
    const char* arcade = ConsolaDe(SIS_ARCADE).carpeta;
    for (int i = 0; carpetaDe && i < nRutas; i++)
    {
        const char* sub = NULL;
        if (EsComprimido(rutas[i]) && ArcadeDeRuta(rutas[i]) >= 0) sub = arcade;
        for (int k = 0; !sub && k < nLista; k++)
            if (!strcmp(lista[k].ruta, rutas[i])) sub = EsArcade(lista[k].sistema) ? arcade : ConsolaDe(lista[k].sistema).carpeta;
        if (sub) { snprintf(carpetaDe[i], 300, "%s/%s", destino, sub); CrearCarpetas(carpetaDe[i]); }
        else snprintf(carpetaDe[i], 300, "%s", destino);
    }

    // Tamaños, y cuáles ya están copiados (mismo nombre y tamaño en la consola: en la carpeta de su sistema o, de las
    // versiones anteriores, directamente en juegos/).
    uint64_t* tam = (uint64_t*)calloc(nRutas ? nRutas : 1, sizeof(uint64_t));
    bool* hacer = (bool*)calloc(nRutas ? nRutas : 1, sizeof(bool));
    uint64_t libres = 0;
    bool medido = buf && tam && hacer && carpetaDe && EspacioLibre(destino, &libres);
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
            char dst[600], antes[600];
            snprintf(dst, sizeof dst, "%s/%s", carpetaDe[i], NombreArchivo(rutas[i]));
            snprintf(antes, sizeof antes, "%s/%s", destino, NombreArchivo(rutas[i]));
            uint64_t t = 0, td = 0, ta = 0;
            if (!ExisteArchivo(rutas[i], &t)) { progreso.errores++; continue; }
            if ((ExisteArchivo(dst, &td) && td == t) || (ExisteArchivo(antes, &ta) && ta == t)) { progreso.yaEstaban++; continue; }
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
            snprintf(dst, sizeof dst, "%s/%s", carpetaDe[i], NombreArchivo(rutas[i]));
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
                Registrar("ERROR: no se pudo copiar %s a %s (copiado %llu de %llu bytes)", rutas[i], destino,
                          (unsigned long long)final, (unsigned long long)tam[i]);
            }
            progreso.hechos++;
        }
    }
    CerrarAccesoCompleto();
    Registrar("Copia a %s: %d copiados, %d ya estaban, %d sin espacio, %d con error, %llu MB libres antes%s%s", destino,
              progreso.copiados, progreso.yaEstaban, progreso.sinEspacio, progreso.errores, (unsigned long long)(libres >> 20),
              progreso.fallo ? ". Falló: " : "", progreso.fallo ? progreso.detalle : "");
    free(buf);
    free(tam);
    free(hacer);
    free(carpetaDe);
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
    return ConsolaParaRuta(r) != NULL || EsComprimido(r);   // solo roms (por la extensión) de alguna consola
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
        else
        {
            progreso.errores++;
            Registrar("ERROR: no se pudo borrar %s%s", rutas[i], EsRomBorrable(rutas[i]) ? "" : " (fuera de las carpetas permitidas)");
        }
        progreso.hechos++;
    }
    CerrarAccesoCompleto();
    if (progreso.errores) snprintf(progreso.detalle, sizeof progreso.detalle, "%d juegos no se pudieron borrar.", progreso.errores);
    Registrar("Borrado: %d borrados, %d con error", progreso.copiados, progreso.errores);
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
    nRutas = capRutas = n;
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
    capRutas = n * 4;   // lugar para los zips que necesitan los arcade (ver HiloCopia)
    rutas = (char(*)[512])malloc(sizeof(*rutas) * capRutas);
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
