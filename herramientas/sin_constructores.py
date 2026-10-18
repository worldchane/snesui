# Saca los constructores globales de objetos ELF de núcleos (en la consola .init_array tiene que quedar vacío, ver
# LEEME.md): cada sección .init_array pasa a llamarse "nucleo_init" (mismo largo, así se cambia en el lugar) y deja de
# ser del tipo SHT_INIT_ARRAY. El enlazador junta todas las nucleo_init y define solo __start_nucleo_init y
# __stop_nucleo_init; la app llama esas funciones a mano al arrancar (main.cpp). Se puede correr varias veces.
#   python herramientas/sin_constructores.py <objetos.o...>   (o @lista.txt, uno por línea)
import struct, sys

objetos = []
for a in sys.argv[1:]:
    objetos += open(a[1:]).read().splitlines() if a.startswith('@') else [a]

SHT_PROGBITS, SHT_INIT_ARRAY = 1, 14
cambiados = 0
for ruta in [o for o in objetos if o.strip()]:
    d = bytearray(open(ruta, 'rb').read())
    if d[:4] != b'\x7fELF':
        continue
    shoff, = struct.unpack_from('<Q', d, 0x28)
    es, n, si = struct.unpack_from('<HHH', d, 0x3A)
    strtab = struct.unpack_from('<IIQQQQIIQQ', d, shoff + si * es)[4]
    tocado = False
    for i in range(n):
        base = shoff + i * es
        nombre, tipo = struct.unpack_from('<II', d, base)
        if tipo != SHT_INIT_ARRAY:
            continue
        o = strtab + nombre
        if bytes(d[o:o + 12]) != b'.init_array\0':
            sys.exit('%s: constructores con prioridad (%s): no se pueden mover' % (ruta, bytes(d[o:o + 24]).split(b'\0')[0]))
        d[o:o + 11] = b'nucleo_init'
        struct.pack_into('<I', d, base + 4, SHT_PROGBITS)
        tocado = True
    if tocado:
        open(ruta, 'wb').write(d)
        cambiados += 1
if cambiados:
    print('constructores movidos a nucleo_init en %d objetos' % cambiados)
