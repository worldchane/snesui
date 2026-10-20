# Lista de archivos y opciones de Mupen64Plus-Next (N64) a partir de su Makefile.common, con la configuración de ALLESIU
# (sin make en esta PC: se evalúa acá lo que usa ese Makefile: variables, ifeq/ifneq/ifdef/ifndef/else, findstring,
# filter y wildcard). Deja tres archivos: la lista de fuentes C/C++, la de ensamblador (NASM) y las opciones.
#   python herramientas/lista_mupen.py <carpeta de mupen64plus-libretro-nx> <lista.txt> <nasm.txt> <opciones.txt>
import glob, os, re, sys

raiz, salida, salida_nasm, salida_opc = sys.argv[1:5]
# La configuración: como "unix" con OpenGL ES 2 (el de la PS4, Piglet), el recompilador x86-64, Angrylion (dibujo por
# software, para las primeras pruebas) con el RSP de bajo nivel CXD4 (Angrylion no anda con el RSP por HLE: sin él, el
# núcleo cae en GLideN64 y falla). Sin paraLLEl (necesita Vulkan).
vars = {
    'ROOT_DIR': raiz.replace('\\', '/'), 'LIBRETRO_DIR': raiz.replace('\\', '/') + '/libretro', 'platform': 'unix', 'GLES': '1', 'GLES3': '0', 'EGL': '0',
    'WITH_DYNAREC': 'x86_64', 'HAVE_THR_AL': '1', 'LLE': '1', 'HAVE_PARALLEL_RSP': '0', 'HAVE_PARALLEL_RDP': '0',
    'SYSTEM_MINIZIP': '0', 'SYSTEM_LIBPNG': '0', 'SYSTEM_XXHASH': '0', 'SYSTEM_ZLIB': '0', 'HAVE_NEON': '0',
    'DYNAFLAGS': '', 'INCFLAGS': '', 'COREFLAGS': '', 'CPUFLAGS': '', 'GLFLAGS': '', 'CFLAGS': '', 'CXXFLAGS': '',
    'LDFLAGS': '', 'SOURCES_C': '', 'SOURCES_CXX': '', 'SOURCES_ASM': '', 'SOURCES_NASM': '',
}

def expandir(s, prof=0):
    if prof > 20:
        return s
    # Funciones de make primero (pueden tener variables adentro).
    def funcion(m):
        nombre, args = m.group(1), m.group(2)
        if nombre == 'findstring':
            a, b = [expandir(x, prof + 1) for x in args.split(',', 1)]
            return a if a.strip() and a.strip() in b else ''
        if nombre == 'filter':
            a, b = [expandir(x, prof + 1).split() for x in args.split(',', 1)]
            return ' '.join(x for x in b if x in a)
        if nombre == 'wildcard':
            return ' '.join(sorted(p.replace('\\', '/') for pat in expandir(args, prof + 1).split() for p in glob.glob(pat)))
        if nombre == 'shell':
            return ''
        return ''
    anterior = None
    while anterior != s:
        anterior = s
        s = re.sub(r'\$\((findstring|filter|wildcard|shell) ([^()]*(?:\([^()]*\)[^()]*)*)\)', funcion, s)
        s = re.sub(r'\$\((\w+)\)', lambda m: vars.get(m.group(1), ''), s)
        s = re.sub(r'\$\(CURDIR\)', raiz, s)
    return s

def condicion(l):
    m = re.match(r'(ifeq|ifneq)\s*\((.*)\)\s*$', l)
    if m:
        a, b = expandir(m.group(2)).split(',', 1)
        igual = a.strip() == b.strip()
        return igual if m.group(1) == 'ifeq' else not igual
    m = re.match(r'(ifdef|ifndef)\s+(\w+)', l)
    if m:
        hay = bool(vars.get(m.group(2), ''))
        return hay if m.group(1) == 'ifdef' else not hay
    raise ValueError(l)

texto = open(os.path.join(raiz, 'Makefile.common'), encoding='latin-1').read().replace('\\\n', ' ')
pila = []   # por nivel: (activo, ya hubo una rama verdadera)
for linea in texto.split('\n'):
    l = linea.strip()
    if not l or l.startswith('#'):
        continue
    activo = all(a for a, _ in pila)
    if re.match(r'(ifeq|ifneq|ifdef|ifndef)\b', l):
        c = condicion(l) if activo else False
        pila.append((c, c))
        continue
    if l.startswith('else'):
        resto = l[4:].strip()
        _, hubo = pila.pop()
        padre = all(a for a, _ in pila)
        if hubo or not padre:
            pila.append((False, True))
        elif resto:
            c = condicion(resto)
            pila.append((c, c))
        else:
            pila.append((True, True))
        continue
    if l == 'endif':
        pila.pop()
        continue
    if not activo:
        continue
    m = re.match(r'([\w.]+)\s*(\+=|:=|\?=|=)\s*(.*)', l)
    if not m:
        continue
    nombre, op, valor = m.groups()
    if op == '+=':
        vars[nombre] = (vars.get(nombre, '') + ' ' + expandir(valor)).strip()
    elif op == '?=':
        if not vars.get(nombre):
            vars[nombre] = expandir(valor)
    else:
        vars[nombre] = expandir(valor)

norm = lambda p: os.path.normpath(p).replace('\\', '/')
fuentes = [norm(p) for p in (vars['SOURCES_C'] + ' ' + vars['SOURCES_CXX']).split()]
fuentes = list(dict.fromkeys(p for p in fuentes if os.path.exists(p)))
faltan = [p for p in (vars['SOURCES_C'] + ' ' + vars['SOURCES_CXX']).split() if not os.path.exists(norm(p))]
nasm = [norm(p) for p in vars['SOURCES_NASM'].split()]
opciones = ' '.join([vars['INCFLAGS'], vars['COREFLAGS'], vars['GLFLAGS'], vars['DYNAFLAGS'], vars['CFLAGS']])
if vars['LLE'] == '1':
    opciones += ' -DHAVE_LLE'   # en el Makefile (no en Makefile.common)
opciones += (' -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -D__LIBRETRO__ -DUSE_FILE32API -DM64P_PLUGIN_API'
             ' -DM64P_CORE_PROTOTYPES -D_ENDUSER_RELEASE -DSINC_LOWER_QUALITY -DTXFILTER_LIB -D__VEC4_OPT -DMUPENPLUSAPI -fcommon')
open(salida, 'w').write('\n'.join(fuentes) + '\n')
open(salida_nasm, 'w').write('\n'.join(nasm) + '\n')
open(salida_opc, 'w').write(re.sub(r'\s+', ' ', opciones).strip() + '\n')
print('%d fuentes (%d C++), %d de ensamblador, %d que no existen%s' % (
    len(fuentes), sum(p.endswith('.cpp') for p in fuentes), len(nasm), len(faltan), (': ' + ' '.join(faltan[:5])) if faltan else ''))
