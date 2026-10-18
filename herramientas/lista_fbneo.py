# Lista de archivos de FinalBurn Neo (núcleo libretro de arcade) a partir de sus Makefiles (src/burner/libretro/
# Makefile.common y Makefile.all): las variables de carpetas, los comodines *.c / *.cpp de cada carpeta, la lista negra
# y los archivos sueltos. Sin CHD, sin 7z y sin Cyclone (ARM). Deja la lista y las carpetas de cabeceras.
#   python herramientas/lista_fbneo.py <carpeta de FBNeo> <lista.txt> <cabeceras.txt>
import glob, os, re, sys

raiz, salida, salida_inc = sys.argv[1], sys.argv[2], sys.argv[3]
lib = os.path.join(raiz, 'src', 'burner', 'libretro')
texto = open(os.path.join(lib, 'Makefile.common'), encoding='latin-1').read() + '\n' + \
        open(os.path.join(lib, 'Makefile.all'), encoding='latin-1').read()
texto = texto.replace('\\\n', ' ')

vars = {'MAIN_FBNEO_DIR': os.path.join(raiz, 'src').replace('\\', '/')}
def expandir(s):
    for _ in range(10):
        s2 = re.sub(r'\$\((\w+)\)', lambda m: vars.get(m.group(1), ''), s)
        if s2 == s:
            break
        s = s2
    return s

# Solo lo que está fuera de los bloques ifeq/ifneq (las opciones que no se usan acá), salvo el else de USE_CYCLONE.
fuentes_c, fuentes_cxx, negra, incluir = [], [], [], []
nivel, en_else_cyclone = 0, False
for linea in texto.split('\n'):
    l = linea.strip()
    if l.startswith(('ifeq', 'ifneq', 'ifdef', 'ifndef')):
        nivel += 1
        en_else_cyclone = False
        continue
    if l == 'else':
        en_else_cyclone = nivel == 1 and 'c68000_intf' in texto
        continue
    if l == 'endif':
        nivel -= 1
        en_else_cyclone = False
        continue
    if nivel and not en_else_cyclone:
        continue
    m = re.match(r'(\w+)\s*(:=|\+=|=)\s*(.*)', l)
    if not m:
        continue
    nombre, op, valor = m.group(1), m.group(2), m.group(3)
    if nombre in ('SOURCES_C', 'SOURCES_CXX', 'BURN_BLACKLIST', 'INCLUDE_DIRS'):
        destino = {'SOURCES_C': fuentes_c, 'SOURCES_CXX': fuentes_cxx, 'BURN_BLACKLIST': negra, 'INCLUDE_DIRS': incluir}[nombre]
        if 'filter-out' in valor:
            ext = '*.c' if nombre == 'SOURCES_C' else '*.cpp'
            for d in expandir('$(FBNEO_SRC_DIRS)').split():
                destino += glob.glob(os.path.join(d, ext))
        else:
            destino += expandir(valor).split()
    else:
        vars[nombre] = expandir(valor) if op == ':=' else (vars.get(nombre, '') + ' ' + expandir(valor) if op == '+=' else valor)
vars['FBNEO_SRC_DIRS'] = expandir('$(FBNEO_CPU_DIRS) $(FBNEO_BURN_DIRS)')

norm = lambda p: os.path.normpath(p).replace('\\', '/')
negra = set(norm(p) for p in negra)
# Los de libretro-common y lo que abre carpetas del sistema van por la capa compartida de SNESUI (archivos.c).
todos = [norm(p) for p in fuentes_c + fuentes_cxx]
todos = [p for p in dict.fromkeys(todos) if p not in negra and '/libretro-common/' not in p and os.path.exists(p)]
open(salida, 'w').write('\n'.join(todos) + '\n')
open(salida_inc, 'w').write('\n'.join(dict.fromkeys(norm(p) for p in incluir)) + '\n')
print('%d archivos, %d carpetas de cabeceras' % (len(todos), len(incluir)))
