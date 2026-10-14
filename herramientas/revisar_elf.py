# Revisa un eboot.elf antes de mandarlo a la consola: qué funciones pide al sistema (importaciones) y si tiene
# constructores globales (.init_array). Con FW 9.00 + GoldHEN, los programas que importan funciones no probadas o traen el
# runtime de excepciones de C++ no abren (CE-41839-5); ver LEEME.md.
#   python herramientas/revisar_elf.py obj/eboot.elf [otro.elf para comparar]
import struct, sys

def leer(ruta):
    d = open(ruta, 'rb').read()
    shoff, = struct.unpack_from('<Q', d, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x3A)
    secs = [struct.unpack_from('<IIQQQQIIQQ', d, shoff + i * shentsize) for i in range(shnum)]
    nombres = secs[shstrndx]
    def nom(off, tabla): o = tabla[4] + off; return d[o:d.index(b'\0', o)].decode()
    por_nombre = {nom(s[0], nombres): s for s in secs}
    imp = set()
    for s in secs:
        if s[1] != 11: continue                      # SHT_DYNSYM
        strtab = secs[s[6]]
        for k in range(1, s[5] // 24):
            st_name, st_info, st_other, st_shndx = struct.unpack_from('<IBBH', d, s[4] + k * 24)
            if st_shndx == 0 and st_name: imp.add(nom(st_name, strtab))
    init = por_nombre.get('.init_array')
    return imp, (init[5] // 8 if init else 0)

imp, init = leer(sys.argv[1])
print('importa', len(imp), 'funciones; constructores globales:', init)
if len(sys.argv) > 2:
    otro, _ = leer(sys.argv[2])
    nuevas = sorted(imp - otro)
    print('importa y el otro no:', ', '.join(nuevas) if nuevas else 'ninguna')
else:
    print(', '.join(sorted(imp)))
