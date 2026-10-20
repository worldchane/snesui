# Genera src/comun/gl_funciones.cpp: una función por cada una de OpenGL ES 2 (GLES2/gl2.h) y de EGL (EGL/egl.h) del
# toolchain de OpenOrbis, que llama a un puntero, y GlCargar(), que llena los punteros buscando cada función por nombre
# (en la consola, en el módulo Piglet con sceKernelDlsym). Así el programa usa OpenGL ES sin pedirle funciones nuevas al
# sistema para abrir.
#   python herramientas/generar_gl.py <gl2.h> <egl.h> <salida.cpp>
import re, sys

def prototipos(ruta, api, entrada, prefijo):
    texto = open(ruta, encoding='latin-1').read()
    # "GL_APICALL const GLubyte *GL_APIENTRY glGetString (GLenum name);": el * puede ir pegado a GL_APIENTRY.
    patron = r'%s\s+(.+?)\s*%s\s+(%s\w+)\s*\(([^)]*)\);' % (api, entrada, prefijo)
    return [(ret.strip(), nombre, args.strip()) for ret, nombre, args in re.findall(patron, texto)]

gl = prototipos(sys.argv[1], 'GL_APICALL', 'GL_APIENTRY', 'gl')
egl = prototipos(sys.argv[2], 'EGLAPI', 'EGLAPIENTRY', 'egl')
lineas = [
    '// Generado por herramientas/generar_gl.py desde GLES2/gl2.h y EGL/egl.h: no editar.',
    '// Cada función de OpenGL ES 2 y de EGL llama a un puntero que llena GlCargar (ver gl_funciones.h).',
    '#include "gl_funciones.h"',
    '',
    '#include <EGL/egl.h>',
    '#include <GLES2/gl2.h>',
    '',
]
cargas = []
for (ret, nombre, args), entrada in [(p, 'GL_APIENTRY') for p in gl] + [(p, 'EGLAPIENTRY') for p in egl]:
    lista = [] if args in ('', 'void') else [a.strip() for a in args.split(',')]
    nombres = [re.search(r'(\w+)\s*(\[\s*\d*\s*\])?$', a).group(1) for a in lista]
    tipo_args = args if lista else 'void'
    lineas.append('static %s (%s* p_%s)(%s);' % (ret, entrada, nombre, tipo_args))
    cuerpo = ('return ' if ret != 'void' else '') + 'p_%s(%s);' % (nombre, ', '.join(nombres))
    lineas.append('extern "C" %s %s %s(%s) { %s }' % (ret, entrada, nombre, tipo_args, cuerpo))
    cargas.append('    p_%s = (%s (%s*)(%s))buscar("%s");' % (nombre, ret, entrada, tipo_args, nombre))
    cargas.append('    if (!p_%s) faltan++;' % nombre)
lineas += ['', 'int GlCargar(void* (*buscar)(const char* nombre))', '{', '    int faltan = 0;'] + cargas + ['    return faltan;', '}', '']
open(sys.argv[3], 'w', encoding='utf-8').write('\n'.join(lineas))
print('%d funciones de OpenGL ES 2 y %d de EGL' % (len(gl), len(egl)))
