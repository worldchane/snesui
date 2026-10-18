/* Reemplazo de la capa de archivos de libretro-common (streams/file_stream.c, vfs/vfs_implementation.c y
 * file/file_path_io.c) para todos los núcleos. La original pide al sistema funciones que SNESUI no usa (stat, fstat,
 * rename, rmdir, ftruncate, unlink...), y en la consola cada función nueva es un riesgo de que el programa no abra. Esta
 * va sobre FILE* y abre con NucleoAbrirArchivo (nucleo_libretro.cpp), la misma función de la app: así las rutas
 * /data y del USB valen igual que en el resto del programa (también en el simulador de PC). Borrar, renombrar o crear
 * carpetas no se permite a los núcleos (la app guarda las partidas por su cuenta). */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <streams/file_stream.h>

FILE* NucleoAbrirArchivo(const char* ruta, const char* modo);

struct RFILE
{
    FILE* f;
    char* ruta;
    bool error;
};

void filestream_vfs_init(const struct retro_vfs_interface_info* vfs_info) { (void)vfs_info; }

RFILE* filestream_open(const char* path, unsigned mode, unsigned hints)
{
    (void)hints;
    if (!path || !*path) return NULL;
    const char* m = "rb";
    if ((mode & RETRO_VFS_FILE_ACCESS_READ_WRITE) == RETRO_VFS_FILE_ACCESS_READ_WRITE)
        m = (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) ? "r+b" : "w+b";
    else if (mode & RETRO_VFS_FILE_ACCESS_WRITE)
        m = (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) ? "r+b" : "wb";
    FILE* f = NucleoAbrirArchivo(path, m);
    if (!f) return NULL;
    RFILE* r = (RFILE*)calloc(1, sizeof *r);
    if (!r) { fclose(f); return NULL; }
    r->f = f;
    r->ruta = strdup(path);
    return r;
}

int filestream_close(RFILE* s)
{
    if (!s) return -1;
    int e = fclose(s->f);
    free(s->ruta);
    free(s);
    return e;
}

int64_t filestream_read(RFILE* s, void* data, int64_t len)
{
    if (!s || len < 0) return -1;
    size_t n = fread(data, 1, (size_t)len, s->f);
    if (n < (size_t)len && ferror(s->f)) s->error = true;
    return (int64_t)n;
}

int64_t filestream_write(RFILE* s, const void* data, int64_t len)
{
    if (!s || len < 0) return -1;
    size_t n = fwrite(data, 1, (size_t)len, s->f);
    if (n < (size_t)len) s->error = true;
    return (int64_t)n;
}

int64_t filestream_seek(RFILE* s, int64_t offset, int seek_position)
{
    if (!s) return -1;
    int w = seek_position == RETRO_VFS_SEEK_POSITION_CURRENT ? SEEK_CUR : seek_position == RETRO_VFS_SEEK_POSITION_END ? SEEK_END : SEEK_SET;
    if (fseek(s->f, (long)offset, w)) { s->error = true; return -1; }
    return (int64_t)ftell(s->f);
}

int64_t filestream_tell(RFILE* s) { return s ? (int64_t)ftell(s->f) : -1; }
void filestream_rewind(RFILE* s) { if (s) { rewind(s->f); s->error = false; } }

int64_t filestream_get_size(RFILE* s)
{
    if (!s) return -1;
    long p = ftell(s->f);
    if (fseek(s->f, 0, SEEK_END)) return -1;
    long t = ftell(s->f);
    fseek(s->f, p, SEEK_SET);
    return (int64_t)t;
}

int64_t filestream_truncate(RFILE* s, int64_t length) { (void)s; (void)length; return -1; }
int filestream_getc(RFILE* s) { return s ? fgetc(s->f) : EOF; }
int filestream_putc(RFILE* s, int c) { return s ? fputc(c, s->f) : EOF; }
char* filestream_gets(RFILE* s, char* buf, size_t len) { return s ? fgets(buf, (int)len, s->f) : NULL; }
int filestream_eof(RFILE* s) { return s ? feof(s->f) : 1; }
int filestream_error(RFILE* s) { return s ? (s->error || ferror(s->f)) : 1; }
int filestream_flush(RFILE* s) { return s ? fflush(s->f) : -1; }
const char* filestream_get_path(RFILE* s) { return s ? s->ruta : NULL; }
libretro_vfs_implementation_file* filestream_get_vfs_handle(RFILE* s) { (void)s; return NULL; }

int filestream_vprintf(RFILE* s, const char* format, va_list args) { return s ? vfprintf(s->f, format, args) : -1; }
int filestream_printf(RFILE* s, const char* format, ...)
{
    va_list a;
    va_start(a, format);
    int n = filestream_vprintf(s, format, a);
    va_end(a);
    return n;
}
int filestream_vscanf(RFILE* s, const char* format, va_list* args) { return s ? vfscanf(s->f, format, *args) : -1; }
int filestream_scanf(RFILE* s, const char* format, ...)
{
    va_list a;
    va_start(a, format);
    int n = filestream_vscanf(s, format, &a);
    va_end(a);
    return n;
}

char* filestream_getline(RFILE* s)
{
    size_t cap = 256, n = 0;
    char* t = (char*)malloc(cap);
    if (!t) return NULL;
    int c;
    while ((c = filestream_getc(s)) != EOF && c != '\n')
    {
        if (n + 1 >= cap)
        {
            char* u = (char*)realloc(t, cap * 2);
            if (!u) { free(t); return NULL; }
            t = u;
            cap *= 2;
        }
        t[n++] = (char)c;
    }
    t[n] = 0;
    return t;
}

int64_t filestream_read_file(const char* path, void** buf, int64_t* len)
{
    *buf = NULL;
    if (len) *len = 0;
    RFILE* s = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0);
    if (!s) return 0;
    int64_t t = filestream_get_size(s);
    char* d = t >= 0 ? (char*)malloc((size_t)t + 1) : NULL;
    if (!d || filestream_read(s, d, t) != t) { free(d); filestream_close(s); return 0; }
    d[t] = 0;   /* como la original: termina en cero por si es texto */
    filestream_close(s);
    *buf = d;
    if (len) *len = t;
    return 1;
}

bool filestream_write_file(const char* path, const void* data, int64_t size)
{
    RFILE* s = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE, 0);
    if (!s) return false;
    int64_t n = filestream_write(s, data, size);
    filestream_close(s);
    return n == size;
}

bool filestream_exists(const char* path)
{
    RFILE* s = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0);
    if (!s) return false;
    filestream_close(s);
    return true;
}

int filestream_delete(const char* path) { (void)path; return -1; }
int filestream_rename(const char* a, const char* b) { (void)a; (void)b; return -1; }

/* De file_path_io.c. */
bool path_is_valid(const char* path) { return filestream_exists(path); }
bool path_is_directory(const char* path) { (void)path; return false; }
bool path_mkdir(const char* dir) { (void)dir; return false; }
int32_t path_get_size(const char* path)
{
    RFILE* s = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0);
    if (!s) return -1;
    int64_t t = filestream_get_size(s);
    filestream_close(s);
    return (int32_t)t;
}

/* De file/file_path.c (lo único que usan los núcleos; el archivo original pide la hora local y setlocale, que traen
 * más funciones del sistema). */
size_t fill_pathname_join(char* out_path, const char* dir, const char* path, size_t size)
{
    if (!size) return 0;
    size_t n = 0;
    if (out_path != dir) n = (size_t)snprintf(out_path, size, "%s", dir ? dir : "");
    else n = strlen(out_path);
    if (n >= size) return n;
    if (n && out_path[n - 1] != '/' && out_path[n - 1] != '\\' && n + 1 < size) out_path[n++] = '/', out_path[n] = 0;
    return n + (size_t)snprintf(out_path + n, size - n, "%s", path ? path : "");
}

/* De file/file_path.c: lo de rutas que usan algunos núcleos. */
const char* path_basename(const char* path)
{
    const char* a = strrchr(path, '/');
    const char* b = strrchr(path, 0x5C);   /* la barra invertida */
    if (b > a) a = b;
    return a ? a + 1 : path;
}

char* path_remove_extension(char* path)
{
    char* punto = path && *path ? strrchr(path_basename(path), '.') : NULL;
    if (!punto) return NULL;
    *punto = 0;
    return path;
}

void fill_pathname_resolve_relative(char* s, const char* in_refpath, const char* in_path, size_t len)
{
    if (!len) return;
    if (in_path[0] == '/' || (in_path[0] && in_path[1] == ':')) { snprintf(s, len, "%s", in_path); return; }
    const char* base = path_basename(in_refpath);
    size_t n = (size_t)(base - in_refpath);
    if (n >= len) n = len - 1;
    memmove(s, in_refpath, n);
    s[n] = 0;
    snprintf(s + n, len - n, "%s", in_path);
}

/* De streams/file_stream_transforms.c: la forma "estilo fopen" (rfopen, rfread...). */
RFILE* rfopen(const char* path, const char* mode)
{
    unsigned m = RETRO_VFS_FILE_ACCESS_READ;
    if (strchr(mode, 'r')) m = strchr(mode, '+') ? RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING : RETRO_VFS_FILE_ACCESS_READ;
    else if (strchr(mode, 'w')) m = strchr(mode, '+') ? RETRO_VFS_FILE_ACCESS_READ_WRITE : RETRO_VFS_FILE_ACCESS_WRITE;
    else if (strchr(mode, 'a')) m = RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
    RFILE* r = filestream_open(path, m, 0);
    if (r && strchr(mode, 'a')) filestream_seek(r, 0, RETRO_VFS_SEEK_POSITION_END);
    return r;
}
int rfclose(RFILE* s) { return filestream_close(s); }
int64_t rftell(RFILE* s) { return filestream_tell(s); }
int64_t rfseek(RFILE* s, int64_t offset, int origin)
{
    int p = origin == SEEK_CUR ? RETRO_VFS_SEEK_POSITION_CURRENT : origin == SEEK_END ? RETRO_VFS_SEEK_POSITION_END : RETRO_VFS_SEEK_POSITION_START;
    return filestream_seek(s, offset, p) < 0 ? -1 : 0;
}
int64_t rfread(void* buffer, size_t elem_size, size_t elem_count, RFILE* s)
{
    if (!elem_size) return 0;
    int64_t n = filestream_read(s, buffer, (int64_t)(elem_size * elem_count));
    return n < 0 ? 0 : n / (int64_t)elem_size;
}
int64_t rfwrite(void const* buffer, size_t elem_size, size_t elem_count, RFILE* s)
{
    if (!elem_size) return 0;
    int64_t n = filestream_write(s, buffer, (int64_t)(elem_size * elem_count));
    return n < 0 ? 0 : n / (int64_t)elem_size;
}
char* rfgets(char* buf, int maxCount, RFILE* s) { return filestream_gets(s, buf, (size_t)maxCount); }
int rfgetc(RFILE* s) { return filestream_getc(s); }
int rfputc(int c, RFILE* s) { return filestream_putc(s, c); }
int64_t rfflush(RFILE* s) { return filestream_flush(s); }
int rferror(RFILE* s) { return filestream_error(s); }
int rfeof(RFILE* s) { return filestream_eof(s); }
int rfprintf(RFILE* s, const char* format, ...)
{
    va_list a;
    va_start(a, format);
    int n = filestream_vprintf(s, format, a);
    va_end(a);
    return n;
}

#if defined(__FreeBSD__)
/* Hora y "locale" para los núcleos en la consola (ver prefijo.h): sin zona horaria (todo en UTC) y siempre el locale
 * "C". Las de la libc leerían archivos del sistema. */
#include <errno.h>
#include <locale.h>
#include <sys/stat.h>
#include <time.h>

int nucleo_fstat(int fd, struct stat* st) { (void)fd; (void)st; errno = ENOSYS; return -1; }

/* Días desde 1970-01-01 a una fecha y al revés (algoritmo de Howard Hinnant, "days from civil"). */
static int64_t DiasDesdeFecha(int64_t a, unsigned m, unsigned d)
{
    a -= m <= 2;
    int64_t era = (a >= 0 ? a : a - 399) / 400;
    unsigned ae = (unsigned)(a - era * 400);
    unsigned dia = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned de = ae * 365 + ae / 4 - ae / 100 + dia;
    return era * 146097 + (int64_t)de - 719468;
}

struct tm* nucleo_gmtime_r(const time_t* t, struct tm* r)
{
    int64_t s = (int64_t)*t, dias = s / 86400, resto = s % 86400;
    if (resto < 0) { resto += 86400; dias--; }
    r->tm_hour = (int)(resto / 3600);
    r->tm_min = (int)(resto / 60 % 60);
    r->tm_sec = (int)(resto % 60);
    r->tm_wday = (int)((dias % 7 + 11) % 7);   /* 1970-01-01 fue jueves */
    int64_t z = dias + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned de = (unsigned)(z - era * 146097);
    unsigned ae = (de - de / 1460 + de / 36524 - de / 146096) / 365;
    unsigned dia = de - (365 * ae + ae / 4 - ae / 100);
    unsigned mp = (5 * dia + 2) / 153;
    unsigned d = dia - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    int64_t a = (int64_t)ae + era * 400 + (m <= 2);
    r->tm_year = (int)(a - 1900);
    r->tm_mon = (int)m - 1;
    r->tm_mday = (int)d;
    r->tm_yday = (int)(dias - DiasDesdeFecha(a, 1, 1));
    r->tm_isdst = 0;
    return r;
}
struct tm* nucleo_gmtime(const time_t* t) { static struct tm r; return nucleo_gmtime_r(t, &r); }
struct tm* nucleo_localtime_r(const time_t* t, struct tm* r) { return nucleo_gmtime_r(t, r); }
struct tm* nucleo_localtime(const time_t* t) { return nucleo_gmtime(t); }

time_t nucleo_timegm(struct tm* tm)
{
    int64_t a = tm->tm_year + 1900LL + tm->tm_mon / 12, m = tm->tm_mon % 12;
    if (m < 0) { m += 12; a--; }
    int64_t s = DiasDesdeFecha(a, (unsigned)m + 1, 1) * 86400 + (int64_t)(tm->tm_mday - 1) * 86400 + tm->tm_hour * 3600LL +
                tm->tm_min * 60LL + tm->tm_sec;
    time_t t = (time_t)s;
    nucleo_gmtime_r(&t, tm);   /* como mktime: deja la fecha normalizada */
    return t;
}
time_t nucleo_mktime(struct tm* tm) { return nucleo_timegm(tm); }

size_t nucleo_strftime(char* s, size_t max, const char* formato, const struct tm* tm)
{
    (void)formato;   /* solo se usa para fechas en textos: siempre "AAAA-MM-DD HH:MM:SS" */
    int n = snprintf(s, max, "%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
                     tm->tm_min, tm->tm_sec);
    return n > 0 && (size_t)n < max ? (size_t)n : 0;
}

static char localeFalso;
locale_t nucleo_newlocale(int mask, const char* nombre, locale_t base) { (void)mask; (void)nombre; (void)base; return (locale_t)(void*)&localeFalso; }
locale_t nucleo_uselocale(locale_t l) { (void)l; return LC_GLOBAL_LOCALE; }
void nucleo_freelocale(locale_t l) { (void)l; }
char* nucleo_setlocale(int categoria, const char* nombre) { (void)categoria; (void)nombre; return (char*)"C"; }
#endif

/* De file/file_path.c: "<dir>/<path><last><ext>". */
size_t fill_pathname_join_special_ext(char* out_path, const char* dir, const char* path, const char* last, const char* ext, size_t size)
{
    size_t n = fill_pathname_join(out_path, dir, path, size);
    if (n < size) n += (size_t)snprintf(out_path + n, size - n, "%s%s", last ? last : "", ext ? ext : "");
    return n;
}
