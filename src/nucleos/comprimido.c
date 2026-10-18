/* Ver comprimido.h. Todo se lee desde memoria: ninguna de las tres librerías abre archivos (así no piden funciones del
 * sistema nuevas y el archivo se lee con las mismas funciones que el resto de la app). */
#include "comprimido.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "miniz.h"

#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"

#define DMC_UNRAR_HEADER_FILE_ONLY
#include "dmc_unrar.c"

int ComprimidoTipo(const uint8_t* d, size_t n)
{
    if (n >= 4 && d[0] == 'P' && d[1] == 'K' && d[2] == 3 && d[3] == 4) return COMPRIMIDO_ZIP;
    if (n >= 6 && !memcmp(d, "7z\xBC\xAF\x27\x1C", 6)) return COMPRIMIDO_7Z;
    if (n >= 6 && !memcmp(d, "Rar!\x1A\x07", 6)) return COMPRIMIDO_RAR;
    return COMPRIMIDO_NINGUNO;
}

static void Copiar(char* dst, const char* src)
{
    size_t k = 0;
    for (; src[k] && k < 255; k++) dst[k] = src[k] == '\\' ? '/' : src[k];
    dst[k] = 0;
}

/* ---------- zip ---------- */
static int ZipListarAbierto(mz_zip_archive* z, EntradaComprimido* out, int max)
{
    int k = 0;
    mz_uint total = mz_zip_reader_get_num_files(z);
    for (mz_uint i = 0; i < total && k < max; i++)
    {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(z, i, &st) || st.m_is_directory) continue;
        Copiar(out[k].nombre, st.m_filename);
        out[k].tam = st.m_uncomp_size;
        out[k].crc = (uint32_t)st.m_crc32;
        out[k].indice = (int)i;
        k++;
    }
    return k;
}

static int ZipListar(const uint8_t* d, size_t n, EntradaComprimido* out, int max)
{
    mz_zip_archive z;
    memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_mem(&z, d, n, 0)) return -1;
    int k = ZipListarAbierto(&z, out, max);
    mz_zip_reader_end(&z);
    return k;
}

static uint8_t* ZipSacar(const uint8_t* d, size_t n, int indice, size_t* tam)
{
    mz_zip_archive z;
    memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_mem(&z, d, n, 0)) return NULL;
    void* r = mz_zip_reader_extract_to_heap(&z, (mz_uint)indice, tam, 0);
    mz_zip_reader_end(&z);
    if (!r) return NULL;
    /* miniz reserva con su propio malloc: es el mismo de la libc (MZ_MALLOC = malloc), se puede liberar con free. */
    return (uint8_t*)r;
}

/* ---------- 7z ---------- */
typedef struct
{
    ISeekInStream vt;
    const uint8_t* d;
    size_t n, pos;
} MemoriaEntrada;

static SRes MemLeer(ISeekInStreamPtr p, void* buf, size_t* size)
{
    MemoriaEntrada* m = (MemoriaEntrada*)(void*)p;
    size_t k = m->n - m->pos < *size ? m->n - m->pos : *size;
    memcpy(buf, m->d + m->pos, k);
    m->pos += k;
    *size = k;
    return SZ_OK;
}

static SRes MemBuscar(ISeekInStreamPtr p, Int64* pos, ESzSeek origen)
{
    MemoriaEntrada* m = (MemoriaEntrada*)(void*)p;
    Int64 base = origen == SZ_SEEK_CUR ? (Int64)m->pos : origen == SZ_SEEK_END ? (Int64)m->n : 0;
    Int64 nueva = base + *pos;
    if (nueva < 0 || nueva > (Int64)m->n) return SZ_ERROR_PARAM;
    m->pos = (size_t)nueva;
    *pos = nueva;
    return SZ_OK;
}

typedef struct
{
    MemoriaEntrada mem;
    CLookToRead2 look;
    Byte buf[1 << 16];
    CSzArEx db;
} Abierto7z;

static const ISzAlloc asignar = {SzAlloc, SzFree};

// Abre el 7z que lee 'fuente' (memoria o archivo). 'a' (calloc) queda a cargo: se libera si falla.
static Abierto7z* Abrir7zDesde(ISeekInStreamPtr fuente, Abierto7z* a)
{
    static bool crc;
    if (!crc) { CrcGenerateTable(); crc = true; }
    LookToRead2_CreateVTable(&a->look, False);
    a->look.realStream = fuente;
    a->look.buf = a->buf;
    a->look.bufSize = sizeof a->buf;
    LookToRead2_INIT(&a->look);
    SzArEx_Init(&a->db);
    if (SzArEx_Open(&a->db, &a->look.vt, &asignar, &asignar) != SZ_OK)
    {
        SzArEx_Free(&a->db, &asignar);
        free(a);
        return NULL;
    }
    return a;
}

static Abierto7z* Abrir7z(const uint8_t* d, size_t n)
{
    Abierto7z* a = (Abierto7z*)calloc(1, sizeof *a);
    if (!a) return NULL;
    a->mem.vt.Read = MemLeer;
    a->mem.vt.Seek = MemBuscar;
    a->mem.d = d;
    a->mem.n = n;
    return Abrir7zDesde(&a->mem.vt, a);
}

static void Cerrar7z(Abierto7z* a)
{
    SzArEx_Free(&a->db, &asignar);
    free(a);
}

static int SieteListarAbierto(Abierto7z* a, EntradaComprimido* out, int max)
{
    int k = 0;
    for (UInt32 i = 0; i < a->db.NumFiles && k < max; i++)
    {
        if (SzArEx_IsDir(&a->db, i)) continue;
        UInt16 nombre[256];
        size_t l = SzArEx_GetFileNameUtf16(&a->db, i, NULL);
        if (l > 256) continue;
        SzArEx_GetFileNameUtf16(&a->db, i, nombre);
        /* De UTF-16 a UTF-8 (los nombres de roms son casi siempre ASCII). */
        char t[256];
        size_t o = 0;
        for (size_t c = 0; nombre[c] && o < 250; c++)
        {
            UInt32 u = nombre[c];
            if (u < 0x80) t[o++] = (char)u;
            else if (u < 0x800) { t[o++] = (char)(0xC0 | u >> 6); t[o++] = (char)(0x80 | (u & 63)); }
            else { t[o++] = (char)(0xE0 | u >> 12); t[o++] = (char)(0x80 | ((u >> 6) & 63)); t[o++] = (char)(0x80 | (u & 63)); }
        }
        t[o] = 0;
        Copiar(out[k].nombre, t);
        out[k].tam = SzArEx_GetFileSize(&a->db, i);
        out[k].crc = SzBitWithVals_Check(&a->db.CRCs, i) ? a->db.CRCs.Vals[i] : 0;
        out[k].indice = (int)i;
        k++;
    }
    return k;
}

static int SieteListar(const uint8_t* d, size_t n, EntradaComprimido* out, int max)
{
    Abierto7z* a = Abrir7z(d, n);
    if (!a) return -1;
    int k = SieteListarAbierto(a, out, max);
    Cerrar7z(a);
    return k;
}

static uint8_t* SieteSacar(const uint8_t* d, size_t n, int indice, size_t* tam)
{
    Abierto7z* a = Abrir7z(d, n);
    if (!a) return NULL;
    UInt32 bloque = 0xFFFFFFFF;
    Byte* salida = NULL;
    size_t salidaTam = 0, desde = 0, hecho = 0;
    uint8_t* r = NULL;
    if (SzArEx_Extract(&a->db, &a->look.vt, (UInt32)indice, &bloque, &salida, &salidaTam, &desde, &hecho, &asignar, &asignar) == SZ_OK)
    {
        /* 'salida' es el bloque entero (puede traer varios archivos): se copia solo el pedido. */
        r = (uint8_t*)malloc(hecho ? hecho : 1);
        if (r) { memcpy(r, salida + desde, hecho); *tam = hecho; }
    }
    SzFree(NULL, salida);
    Cerrar7z(a);
    return r;
}

/* ---------- rar ---------- */
static int RarListarAbierto(dmc_unrar_archive* a, EntradaComprimido* out, int max)
{
    int k = 0;
    dmc_unrar_size_t total = dmc_unrar_get_file_count(a);
    for (dmc_unrar_size_t i = 0; i < total && k < max; i++)
    {
        if (dmc_unrar_file_is_directory(a, i)) continue;
        char t[256];
        if (!dmc_unrar_get_filename(a, i, t, sizeof t)) continue;
        const dmc_unrar_file* st = dmc_unrar_get_file_stat(a, i);
        Copiar(out[k].nombre, t);
        out[k].tam = st ? st->uncompressed_size : 0;
        out[k].crc = st ? (uint32_t)st->crc : 0;
        out[k].indice = (int)i;
        k++;
    }
    return k;
}

static int RarListar(const uint8_t* d, size_t n, EntradaComprimido* out, int max)
{
    dmc_unrar_archive a;
    if (dmc_unrar_archive_init(&a) != DMC_UNRAR_OK) return -1;
    if (dmc_unrar_archive_open_mem(&a, d, n) != DMC_UNRAR_OK) { dmc_unrar_archive_close(&a); return -1; }
    int k = RarListarAbierto(&a, out, max);
    dmc_unrar_archive_close(&a);
    return k;
}

static uint8_t* RarSacar(const uint8_t* d, size_t n, int indice, size_t* tam)
{
    dmc_unrar_archive a;
    if (dmc_unrar_archive_init(&a) != DMC_UNRAR_OK) return NULL;
    if (dmc_unrar_archive_open_mem(&a, d, n) != DMC_UNRAR_OK) { dmc_unrar_archive_close(&a); return NULL; }
    uint8_t* r = NULL;
    const dmc_unrar_file* st = dmc_unrar_get_file_stat(&a, (dmc_unrar_size_t)indice);
    if (st && st->uncompressed_size < 256u * 1024 * 1024)
    {
        size_t t = (size_t)st->uncompressed_size;
        r = (uint8_t*)malloc(t ? t : 1);
        dmc_unrar_size_t hecho = 0;
        if (r && dmc_unrar_extract_file_to_mem(&a, (dmc_unrar_size_t)indice, r, t, &hecho, true) == DMC_UNRAR_OK)
            *tam = (size_t)hecho;
        else { free(r); r = NULL; }
    }
    dmc_unrar_archive_close(&a);
    return r;
}

int ComprimidoListar(const uint8_t* d, size_t n, EntradaComprimido* out, int max)
{
    switch (ComprimidoTipo(d, n))
    {
    case COMPRIMIDO_ZIP: return ZipListar(d, n, out, max);
    case COMPRIMIDO_7Z: return SieteListar(d, n, out, max);
    case COMPRIMIDO_RAR: return RarListar(d, n, out, max);
    default: return -1;
    }
}

uint8_t* ComprimidoSacar(const uint8_t* d, size_t n, int indice, size_t* tam)
{
    *tam = 0;
    switch (ComprimidoTipo(d, n))
    {
    case COMPRIMIDO_ZIP: return ZipSacar(d, n, indice, tam);
    case COMPRIMIDO_7Z: return SieteSacar(d, n, indice, tam);
    case COMPRIMIDO_RAR: return RarSacar(d, n, indice, tam);
    default: return NULL;
    }
}

/* ---------- listar sin leer el archivo entero ---------- */
/* Para la biblioteca: solo se lee el índice de cada comprimido (en zip y 7z está al final; en rar, los encabezados de
 * cada archivo), con lecturas sueltas. Así buscar en cientos de comprimidos del USB es rápido. */
FILE* NucleoAbrirArchivo(const char* ruta, const char* modo);

static size_t ZipLeerArchivo(void* opaco, mz_uint64 desde, void* buf, size_t n)
{
    FILE* f = (FILE*)opaco;
    if (fseek(f, (long)desde, SEEK_SET)) return 0;
    return fread(buf, 1, n, f);
}

typedef struct
{
    ISeekInStream vt;
    FILE* f;
} ArchivoEntrada;

static SRes ArchLeer(ISeekInStreamPtr p, void* buf, size_t* size)
{
    ArchivoEntrada* a = (ArchivoEntrada*)(void*)p;
    *size = fread(buf, 1, *size, a->f);
    return SZ_OK;
}

static SRes ArchBuscar(ISeekInStreamPtr p, Int64* pos, ESzSeek origen)
{
    ArchivoEntrada* a = (ArchivoEntrada*)(void*)p;
    int w = origen == SZ_SEEK_CUR ? SEEK_CUR : origen == SZ_SEEK_END ? SEEK_END : SEEK_SET;
    if (fseek(a->f, (long)*pos, w)) return SZ_ERROR_READ;
    *pos = ftell(a->f);
    return SZ_OK;
}

static void* RarAbrirNada(const char* ruta) { (void)ruta; return NULL; }
static void RarCerrarNada(void* opaco) { (void)opaco; }   /* el archivo lo cierra ComprimidoListarRuta */
static dmc_unrar_size_t RarLeer(void* opaco, void* buf, dmc_unrar_size_t n) { return fread(buf, 1, (size_t)n, (FILE*)opaco); }
static bool RarBuscar(void* opaco, dmc_unrar_offset_t desde, int origen) { return fseek((FILE*)opaco, (long)desde, origen) == 0; }
static dmc_unrar_offset_t RarPosicion(void* opaco) { return ftell((FILE*)opaco); }
static dmc_unrar_io_handler rarArchivo = {RarAbrirNada, RarCerrarNada, RarLeer, RarBuscar, RarPosicion};

int ComprimidoListarRuta(const char* ruta, EntradaComprimido* out, int max)
{
    FILE* f = NucleoAbrirArchivo(ruta, "rb");
    if (!f) return -1;
    uint8_t cabeza[8] = {0};
    size_t leidos = fread(cabeza, 1, sizeof cabeza, f);
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);
    int k = -1;
    switch (ComprimidoTipo(cabeza, leidos))
    {
    case COMPRIMIDO_ZIP:
    {
        mz_zip_archive z;
        memset(&z, 0, sizeof z);
        z.m_pRead = ZipLeerArchivo;
        z.m_pIO_opaque = f;
        if (tam > 0 && mz_zip_reader_init(&z, (mz_uint64)tam, 0))
        {
            k = ZipListarAbierto(&z, out, max);
            mz_zip_reader_end(&z);
        }
        break;
    }
    case COMPRIMIDO_7Z:
    {
        ArchivoEntrada e;
        e.vt.Read = ArchLeer;
        e.vt.Seek = ArchBuscar;
        e.f = f;
        Abierto7z* a = (Abierto7z*)calloc(1, sizeof *a);
        if (a) a = Abrir7zDesde(&e.vt, a);
        if (a)
        {
            k = SieteListarAbierto(a, out, max);
            Cerrar7z(a);
        }
        break;
    }
    case COMPRIMIDO_RAR:
    {
        dmc_unrar_archive a;
        if (dmc_unrar_archive_init(&a) == DMC_UNRAR_OK)
        {
            a.io.funcs = &rarArchivo;
            a.io.opaque = f;
            a.io.size = (dmc_unrar_size_t)tam;
            if (dmc_unrar_archive_open(&a) == DMC_UNRAR_OK) k = RarListarAbierto(&a, out, max);
            dmc_unrar_archive_close(&a);
        }
        break;
    }
    }
    fclose(f);
    return k;
}
