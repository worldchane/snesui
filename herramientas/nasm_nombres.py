# Pasa el renombrar.h de un núcleo (#define nombre prefijo_nombre) a %define de NASM, para su código en ensamblador.
# Sin los nombres que en ensamblador son registros o palabras de NASM: "rsp" es en C el RSP de la N64 y en ensamblador
# el registro de la pila.
#   python herramientas/nasm_nombres.py <renombrar.h> <salida.inc>
import re, sys

reservados = set()
for r in 'ax bx cx dx si di sp bp'.split():
    reservados |= {'r' + r, 'e' + r, r}
for r in 'abcd':
    reservados |= {r + 'l', r + 'h'}
reservados |= {'sil', 'dil', 'spl', 'bpl'}
for i in range(8, 16):
    reservados |= {'r%d' % i, 'r%dd' % i, 'r%dw' % i, 'r%db' % i}
for i in range(16):
    reservados |= {'xmm%d' % i, 'ymm%d' % i}
reservados |= set('cs ds es fs gs ss rip eip byte word dword qword tword oword ptr rel abs section segment global extern '
                  'align times default bits use64 struc endstruc'.split())

salida = []
for linea in open(sys.argv[1], encoding='latin-1'):
    m = re.match(r'#define (\w+) (\w+)\s*$', linea)
    if m and m.group(1).lower() not in reservados:
        salida.append('%%define %s %s' % (m.group(1), m.group(2)))
open(sys.argv[2], 'w').write('\n'.join(salida) + '\n')
