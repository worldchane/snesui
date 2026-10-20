# Solo para la PC (Windows): las variables globales de C++ llevan el nombre decorado ("?s9x_Settings@@3USSettings@@A")
# y los archivos de C del mismo núcleo las piden sin decorar ("s9x_Settings"). En la consola los dos nombres son iguales.
# Escribe, para cada variable global de C++ de los objetos, una opción /alternatename para el enlazador. También el caso
# contrario: una variable de C (sin decorar) que un archivo de C++ pide con el nombre decorado (GLideN64 con las de
# Mupen64Plus, "?n64_rdram_size@@3_KA" → "n64_rdram_size").
#   python herramientas/alternativos_pc.py <prefijo> <salida.txt> <objetos.o...>
import re, struct, sys

prefijo, salida = sys.argv[1], sys.argv[2]
lineas = set()
definidas, pedidas = set(), {}
for ruta in sys.argv[3:]:
    d = open(ruta, 'rb').read()
    if len(d) < 20:
        continue
    ptr, nsym = struct.unpack_from('<II', d, 8)
    tabla = ptr + nsym * 18
    i = 0
    while i < nsym:
        e = d[ptr + i * 18: ptr + i * 18 + 18]
        if e[:4] == b'\0\0\0\0':
            off, = struct.unpack_from('<I', e, 4)
            nombre = d[tabla + off:d.index(b'\0', tabla + off)].decode('latin-1')
        else:
            nombre = e[:8].rstrip(b'\0').decode('latin-1')
        valor, seccion, _, clase, aux = struct.unpack_from('<IhHBB', e, 8)
        comun = seccion == 0 and valor > 0   # variable de C sin valor inicial (símbolo "común"): también es una definición
        # Variable global (no función): "?nombre@@3..." definida en este objeto.
        m = re.match(r'\?(%s_\w+)@@3' % re.escape(prefijo), nombre)
        if clase == 2 and seccion > 0 and m:
            lineas.add('-Wl,/alternatename:%s=%s' % (m.group(1), nombre))
        if clase == 2 and (seccion > 0 or comun):
            definidas.add(nombre)
        if clase == 2 and seccion == 0 and not comun and m:
            pedidas[nombre] = m.group(1)
        i += 1 + aux
# Pedidas con el nombre de C++ y definidas solo con el de C.
for decorado, simple in pedidas.items():
    if decorado not in definidas and simple in definidas:
        lineas.add('-Wl,/alternatename:%s=%s' % (decorado, simple))
open(salida, 'w').write('\n'.join(sorted(lineas)) + ('\n' if lineas else ''))
print('%d nombres alternativos' % len(lineas))
