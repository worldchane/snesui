# Arma el renombrar.h de un núcleo libretro: un "#define nombre <prefijo>_nombre" por cada símbolo global que definen sus
# objetos (ELF de la PS4). Compilado otra vez con -include renombrar.h, el núcleo no comparte ningún nombre con los demás
# núcleos ni con la app (ver src/nucleos/nucleos.sh).
#   python herramientas/simbolos_nucleo.py <prefijo> <renombrar.h> <objetos.o...>   (o @lista.txt, uno por línea)
import struct, sys

prefijo, salida, objetos, fuentes = sys.argv[1], sys.argv[2], [], []
for a in sys.argv[3:]:
    if a.startswith('--fuentes='):
        fuentes.append(a[10:])
    else:
        objetos += open(a[1:]).read().splitlines() if a.startswith('@') else [a]

# Los nombres que el propio núcleo borra con #undef (por ejemplo "#define RefreshAddr otra ... #undef RefreshAddr") no se
# pueden renombrar con un #define: el #undef se lo llevaría. Se dejan como están.
import os, re
redefinidos = set()
for raiz in fuentes:
    for carpeta, _, archivos in os.walk(raiz):
        for f in archivos:
            if f.rsplit('.', 1)[-1].lower() in ('c', 'h', 'cpp', 'cxx', 'hxx', 'cc', 'hpp', 'inc'):
                try:
                    texto = open(os.path.join(carpeta, f), encoding='latin-1').read()
                except OSError:
                    continue
                redefinidos.update(re.findall(r'^\s*#\s*undef\s+([A-Za-z_]\w*)', texto, re.M))
objetos = [o for o in objetos if o.strip()]
NO_TOCAR = ('SameBoyIniciar_',)   # los llama la app con su nombre (consola.cpp)

def simbolos(ruta):
    d = open(ruta, 'rb').read()
    if d[:4] != b'\x7fELF':
        return set()
    shoff, = struct.unpack_from('<Q', d, 0x28)
    es, n, _ = struct.unpack_from('<HHH', d, 0x3A)
    secs = [struct.unpack_from('<IIQQQQIIQQ', d, shoff + i * es) for i in range(n)]
    r = set()
    for s in secs:
        if s[1] != 2:   # SHT_SYMTAB
            continue
        tabla = secs[s[6]]
        for k in range(1, s[5] // 24):
            nombre, info, _, shndx = struct.unpack_from('<IBBH', d, s[4] + k * 24)
            if (info >> 4) not in (1, 2) or shndx == 0:   # global o débil, y definido acá
                continue
            o = tabla[4] + nombre
            t = d[o:d.index(b'\0', o)].decode()
            if t.startswith('_Z'):
                c = Cpp(t)
                if c:
                    r.add(c)
            elif t and (t[0].isalpha() or t[0] == '_'):
                r.add(t)
    return r

# De un nombre de C++ (_ZN6CMikie5ResetEv, _Z9lynx_initv...) saca el primer identificador: la clase o la función libre.
# Renombrar ese identificador en el código basta para que todos sus símbolos cambien. Se saltean los de la biblioteca de
# C++ (std, operadores) y las tablas que arma el compilador (_ZT*, _ZGV*).
def Cpp(t):
    s = t[2:]
    if s[:1] in ('T', 'G'):
        return None
    if s[:1] == 'N':
        s = s[1:]
        while s[:1] in ('K', 'V', 'r', 'L'):
            s = s[1:]
    elif s[:1] == 'L':
        s = s[1:]
    if not s[:1].isdigit():
        return None
    k = 0
    while s[k].isdigit():
        k += 1
    n = int(s[:k])
    ident = s[k:k + n]
    if ident.startswith('__') or ident in ('std',):
        return None
    return ident

todos = set()
for o in objetos:
    todos |= simbolos(o)
p = prefijo + '_'
lineas = ['/* Generado por herramientas/simbolos_nucleo.py: no editar. */']
for t in sorted(todos):
    if t.startswith(p) or t.startswith(NO_TOCAR) or t == 'main' or t in redefinidos:
        continue
    lineas.append('#define %s %s%s' % (t, p, t))
open(salida, 'w').write('\n'.join(lineas) + '\n')
print('%s: %d nombres renombrados' % (salida, len(lineas) - 1))
