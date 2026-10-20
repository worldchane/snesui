// Detector de BIOS: ver bios.h. Los tamaños y CRC-32 son los de la lista de BIOS de libretro (los mismos archivos que
// piden sus emuladores).
#include "bios.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../comun/plataforma.h"
#include "../nucleos/comprimido.h"
#include "consola.h"
#include "registro.h"

// neogeo.zip no tiene un CRC fijo (hay varias versiones del set): se reconoce por los archivos de adentro.
static const uint32_t ZIP_NEOGEO = 0xFFFFFFFFu;

static const BiosConocida BIOS[] = {
    {SIS_GBA, "gba_bios.bin", 16384, 0x81977335, "Game Boy Advance", false},
    {SIS_GB, "gb_bios.bin", 256, 0x59c8598e, "Game Boy", false},
    {SIS_GBC, "gbc_bios.bin", 2304, 0x41884e46, "Game Boy Color", false},
    {SIS_NES, "disksys.rom", 8192, 0x5e607dcf, "Famicom Disk System", true},
    {SIS_SMS, "bios_U.sms", 8192, 0x0072ed54, "Master System (América)", false},
    {SIS_SMS, "bios_E.sms", 8192, 0x0072ed54, "Master System (Europa)", false},
    {SIS_SMS, "bios_J.sms", 8192, 0x48d44a13, "Master System (Japón)", false},
    {SIS_GG, "bios.gg", 1024, 0x0ebea9d4, "Game Gear", false},
    {SIS_MD, "bios_CD_U.bin", 131072, 0x9f6f6276, "Sega CD (América)", false},
    {SIS_MD, "bios_CD_E.bin", 131072, 0x529ac15a, "Mega CD (Europa)", false},
    {SIS_MD, "bios_CD_J.bin", 131072, 0x9d2da8f2, "Mega CD (Japón)", false},
    {SIS_A7800, "7800 BIOS (U).rom", 4096, 0x5d13730c, "Atari 7800 (NTSC)", false},
    {SIS_A7800, "7800 BIOS (E).rom", 16384, 0xd5b61170, "Atari 7800 (PAL)", false},
    {SIS_LYNX, "lynxboot.img", 512, 0x0d973c9d, "Atari Lynx", true},
    {SIS_NEOGEO, "neogeo.zip", 0, ZIP_NEOGEO, "Neo Geo (neogeo.zip)", true},
};
static const int N = (int)(sizeof BIOS / sizeof BIOS[0]);
static int estado[N];

int BiosCuantas() { return N; }
const BiosConocida& BiosDe(int i) { return BIOS[i]; }
int BiosEstado(int i) { return i >= 0 && i < N ? estado[i] : BIOS_FALTA; }

static uint32_t Crc32(const uint8_t* d, size_t n)
{
    static uint32_t tabla[256];
    if (!tabla[1])
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            tabla[i] = c;
        }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = tabla[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// El archivo entero a memoria (solo los chicos: las BIOS sueltas miden a lo más unos cientos de KB).
static uint8_t* Leer(const char* ruta, uint64_t max, size_t* tam)
{
    uint64_t t = 0;
    if (!ExisteArchivo(ruta, &t) || !t || t > max) return NULL;
    FILE* f = AbrirArchivo(ruta, "rb");
    if (!f) return NULL;
    uint8_t* d = (uint8_t*)malloc((size_t)t);
    bool ok = d && fread(d, 1, (size_t)t, f) == t;
    fclose(f);
    if (!ok) { free(d); return NULL; }
    *tam = (size_t)t;
    return d;
}

static bool Escribir(const char* ruta, const uint8_t* d, size_t n)
{
    FILE* f = AbrirArchivo(ruta, "wb");
    if (!f) return false;
    bool ok = fwrite(d, 1, n, f) == n;
    fclose(f);
    if (!ok) BorrarArchivo(ruta);
    return ok;
}

// ¿Es el set de BIOS de Neo Geo? (trae la rom de la placa y la de las letras.)
static bool EsZipNeoGeo(const char* ruta)
{
    static EntradaComprimido e[64];
    int n = ComprimidoListarRuta(ruta, e, 64);
    bool lo = false, sfix = false;
    for (int i = 0; i < n; i++)
    {
        const char* b = strrchr(e[i].nombre, '/');
        b = b ? b + 1 : e[i].nombre;
        if (!strcasecmp(b, "000-lo.lo")) lo = true;
        if (!strcasecmp(b, "sfix.sfix")) sfix = true;
    }
    return lo && sfix;
}

static void RutaBios(int i, char* r, int max) { snprintf(r, max, "%s/bios/%s", CARPETA_DATOS, BIOS[i].archivo); }

void BiosRevisar()
{
    for (int i = 0; i < N; i++)
    {
        char r[200];
        RutaBios(i, r, sizeof r);
        uint64_t t = 0;
        if (!ExisteArchivo(r, &t)) { estado[i] = BIOS_FALTA; continue; }
        if (BIOS[i].crc == ZIP_NEOGEO) { estado[i] = EsZipNeoGeo(r) ? BIOS_BIEN : BIOS_DISTINTA; continue; }
        size_t n = 0;
        uint8_t* d = t == BIOS[i].tam ? Leer(r, BIOS[i].tam, &n) : NULL;
        estado[i] = d && Crc32(d, n) == BIOS[i].crc ? BIOS_BIEN : BIOS_DISTINTA;
        free(d);
    }
}

int BiosDeConsola(int sistema, int* conocidas)
{
    int hay = 0, c = 0;
    for (int i = 0; i < N; i++)
        if (BIOS[i].sistema == sistema) { c++; hay += estado[i] == BIOS_BIEN; }
    if (conocidas) *conocidas = c;
    return hay;
}

// ---------- búsqueda en el USB ----------
struct Busqueda { char carpeta[300]; int copiadas, prof; };

// Copia 'd' como la BIOS que corresponda (si es una y falta en la consola). Las que tienen el mismo contenido (la de
// Master System de América y la de Europa son iguales) se copian todas.
static int Instalar(const uint8_t* d, size_t n, const char* deDonde)
{
    int copiadas = 0;
    uint32_t crc = 0;
    bool calculado = false;
    for (int i = 0; i < N; i++)
    {
        if (BIOS[i].crc == ZIP_NEOGEO || BIOS[i].tam != n || estado[i] == BIOS_BIEN) continue;
        if (!calculado) { crc = Crc32(d, n); calculado = true; }
        if (crc != BIOS[i].crc) continue;
        char r[200];
        RutaBios(i, r, sizeof r);
        if (Escribir(r, d, n))
        {
            estado[i] = BIOS_BIEN;
            copiadas++;
            Registrar("BIOS copiada: %s (%s), desde %s", BIOS[i].archivo, BIOS[i].descripcion, deDonde);
        }
    }
    return copiadas;
}

// ¿Puede ser una BIOS por el tamaño? (para no leer cada archivo de la carpeta).
static bool TamDeBios(uint64_t t)
{
    for (int i = 0; i < N; i++) if (BIOS[i].tam == t) return true;
    return false;
}

static void AlListarBios(const char* nombre, bool esCarpeta, void* u)
{
    Busqueda& b = *(Busqueda*)u;
    char ruta[600];
    snprintf(ruta, sizeof ruta, "%s/%s", b.carpeta, nombre);
    if (esCarpeta)
    {
        // Una carpeta adentro ("bios/gba", "bios/Sega"...): un nivel más.
        if (b.prof >= 1) return;
        Busqueda sub = b;
        snprintf(sub.carpeta, sizeof sub.carpeta, "%s", ruta);
        sub.prof++;
        sub.copiadas = 0;
        ListarCarpeta(sub.carpeta, AlListarBios, &sub);
        b.copiadas += sub.copiadas;
        return;
    }
    uint64_t t = 0;
    if (!ExisteArchivo(ruta, &t)) return;
    if (EsComprimido(nombre))
    {
        // neogeo.zip (con cualquier nombre que tenga los archivos de la placa).
        int ng = -1;
        for (int i = 0; i < N; i++) if (BIOS[i].crc == ZIP_NEOGEO) ng = i;
        if (ng >= 0 && estado[ng] != BIOS_BIEN && t < 8u << 20 && EsZipNeoGeo(ruta))
        {
            size_t n = 0;
            uint8_t* d = Leer(ruta, 8u << 20, &n);
            char r[200];
            RutaBios(ng, r, sizeof r);
            if (d && Escribir(r, d, n))
            {
                estado[ng] = BIOS_BIEN;
                b.copiadas++;
                Registrar("BIOS copiada: neogeo.zip, desde %s", ruta);
            }
            free(d);
            return;
        }
        // Un paquete de BIOS comprimido: se mira el índice y solo si trae alguna que falte se abre entero.
        static EntradaComprimido e[512];
        int n = ComprimidoListarRuta(ruta, e, 512);
        bool sirve = false;
        for (int k = 0; k < n && !sirve; k++)
            for (int i = 0; i < N; i++)
                if (BIOS[i].tam == e[k].tam && BIOS[i].crc == e[k].crc && estado[i] != BIOS_BIEN) sirve = true;
        if (!sirve || t > 64u << 20) return;
        size_t tz = 0;
        uint8_t* z = Leer(ruta, 64u << 20, &tz);
        if (!z) return;
        for (int k = 0; k < n; k++)
        {
            if (!TamDeBios(e[k].tam)) continue;
            size_t tb = 0;
            uint8_t* d = ComprimidoSacar(z, tz, e[k].indice, &tb);
            if (d) b.copiadas += Instalar(d, tb, ruta);
            free(d);
        }
        free(z);
        return;
    }
    if (!TamDeBios(t)) return;
    size_t n = 0;
    uint8_t* d = Leer(ruta, t, &n);
    if (d) b.copiadas += Instalar(d, n, ruta);
    free(d);
}

bool BiosProbar(const char* ruta, const char* nombre)
{
    const char* e = strrchr(nombre, '.');
    if (!e) return false;
    static const char* const EXT[] = {".rom", ".bin", ".img", ".sms", ".gg", ".gb", ".gbc", ".gba", ".bios"};
    bool sirve = false;
    for (const char* x : EXT) if (!strcasecmp(e, x)) sirve = true;
    uint64_t t = 0;
    if (!sirve || !ExisteArchivo(ruta, &t) || !TamDeBios(t)) return false;
    size_t n = 0;
    uint8_t* d = Leer(ruta, t, &n);
    if (!d) return false;
    uint32_t crc = Crc32(d, n);
    bool es = false;
    for (int i = 0; i < N; i++) if (BIOS[i].tam == n && BIOS[i].crc == crc) es = true;
    if (es) Instalar(d, n, ruta);
    free(d);
    return es;
}

int BiosBuscarEnUsb()
{
    char destino[160];
    snprintf(destino, sizeof destino, "%s/bios", CARPETA_DATOS);
    CrearCarpetas(destino);
    BiosRevisar();
    static const char* const USB[] = {"/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3"};
    static const char* const CARPETAS[] = {"bios", "BIOS", "Bios", "roms/bios", "roms/BIOS", "system", "System"};
    int copiadas = 0;
    for (const char* u : USB)
        for (const char* c : CARPETAS)
        {
            static Busqueda b;
            memset(&b, 0, sizeof b);
            snprintf(b.carpeta, sizeof b.carpeta, "%s/%s", u, c);
            if (ListarCarpeta(b.carpeta, AlListarBios, &b) >= 0) copiadas += b.copiadas;
        }
    return copiadas;
}
