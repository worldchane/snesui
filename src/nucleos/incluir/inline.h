/* Algunos núcleos (Genesis Plus GX, NeoPop) esperan INLINE definido por su Makefile (con un espacio: no entra en una
 * opción -D del script). */
#ifndef INLINE
#define INLINE static inline
#endif
