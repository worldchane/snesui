/* vasprintf para Windows (solo PC; en la consola la trae la libc). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
int vasprintf(char **str, const char *fmt, va_list args)
{
    va_list copia;
    va_copy(copia, args);
    int n = _vscprintf(fmt, copia);
    va_end(copia);
    *str = n < 0 ? NULL : (char *)malloc((size_t)n + 1);
    if (!*str) return -1;
    return vsprintf(*str, fmt, args);
}
