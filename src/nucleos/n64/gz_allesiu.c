/* Las funciones gz* de zlib (archivos .gz) en su lugar: solo las usa la caché de texturas en alta resolución de
 * GLideN64, que ALLESIU no usa, y las de zlib piden read y write al sistema. Acá siempre fallan sin tocar el disco. */
#include <stddef.h>
#include <zlib.h>

gzFile gzopen(const char* ruta, const char* modo) { (void)ruta; (void)modo; return NULL; }
int gzclose(gzFile f) { (void)f; return Z_STREAM_ERROR; }
int gzread(gzFile f, voidp buf, unsigned len) { (void)f; (void)buf; (void)len; return -1; }
int gzwrite(gzFile f, voidpc buf, unsigned len) { (void)f; (void)buf; (void)len; return 0; }
z_off_t gzseek(gzFile f, z_off_t desde, int origen) { (void)f; (void)desde; (void)origen; return -1; }
int gzeof(gzFile f) { (void)f; return 1; }
