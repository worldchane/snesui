# Arma assets/misc/arcade.bin: la tabla de los juegos arcade que conoce la versión de FinalBurn Neo que lleva ALLESIU,
# sacada de sus DAT (FBNeo/dats: Arcade y Neo Geo). Por cada juego: el nombre del zip, el título, de qué juego es clon,
# qué BIOS usa, el año, el fabricante y los archivos que lleva (nombre, tamaño y CRC). La app la lee al abrir
# (src/app/arcade.cpp): sin punteros adentro, solo desplazamientos, así no agrega relocaciones al programa.
#   python herramientas/generar_arcade.py <carpeta de FBNeo> <salida.bin>
#
# Formato (todo little endian):
#   "ALLESIU-ARCADE1" + 0      16 bytes
#   u32 juegos, u32 archivos, u32 bytes de texto
#   juegos[]:   u32 nombre, u32 titulo, u32 fabricante, i32 padre, i32 bios, u16 año, u8 marcas, u8 0,
#               u32 primer archivo, u32 cuántos archivos          (32 bytes; ordenados por nombre)
#   archivos[]: u32 nombre, u32 tamaño, u32 crc, u8 marcas, 3 × 0  (16 bytes)
#   texto: cadenas terminadas en 0
# Marcas del juego: 1 = es una BIOS (no es un juego), y la placa: 2 = Neo Geo, 4 = CPS1, 8 = CPS2, 16 = CPS3. Marcas del archivo: 1 = viene del padre o la BIOS
# (merge), 2 = no se exige (no hay volcado, o FinalBurn Neo lo marca opcional).
import os, struct, sys
import xml.etree.ElementTree as ET

raiz, salida = sys.argv[1], sys.argv[2]
dats = os.path.join(raiz, 'dats')
fuentes = sorted([(n, 'Neogeo only' in n) for n in os.listdir(dats) if n.endswith('.dat') and ('Arcade only' in n or 'Neogeo only' in n)], key=lambda f: not f[1])   # Neo Geo primero (sus juegos también están en el de arcade)

juegos = {}
for nombre_dat, es_neogeo in fuentes:
    for g in ET.parse(os.path.join(dats, nombre_dat)).getroot().iter('game'):
        n = g.get('name')
        if n in juegos:
            continue
        roms = []
        for r in g.iter('rom'):
            roms.append((r.get('name'), int(r.get('size') or 0), int(r.get('crc') or '0', 16),
                         (1 if r.get('merge') else 0) | (2 if r.get('status') == 'nodump' else 0)))
        anio = (g.findtext('year') or '0')[:4]
        fuente = g.get('sourcefile') or ''
        placa = 2 if es_neogeo or fuente.startswith('neogeo/') else 4 if fuente == 'capcom/d_cps1.cpp' else                 8 if fuente == 'capcom/d_cps2.cpp' else 16 if fuente.startswith('cps3/') else 0
        juegos[n] = dict(titulo=g.findtext('description') or n, fab=g.findtext('manufacturer') or '',
                         padre=g.get('cloneof'), bios=g.get('romof'), anio=int(anio) if anio.isdigit() else 0,
                         esbios=g.get('isbios') == 'yes', placa=placa, roms=roms)

# Los archivos que FinalBurn Neo marca como opcionales en su código (BRF_OPT: chips PAL/PLD y similares que no hacen
# falta para jugar). El DAT no lo dice; sin esto el chequeo pediría archivos que el emulador no exige.
import re
opcionales = set()
patron = re.compile(r'\{\s*"([^"]+)"\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*,[^}]*BRF_OPT')
for carpeta, _, archivos_fuente in os.walk(os.path.join(raiz, 'src', 'burn', 'drv')):
    for f in archivos_fuente:
        if f.endswith('.cpp'):
            for m in patron.finditer(open(os.path.join(carpeta, f), encoding='latin-1').read()):
                opcionales.add((int(m.group(2), 16), int(m.group(3), 16)))
for j in juegos.values():
    j['roms'] = [(n, t, c, m | (2 if (t, c) in opcionales else 0)) for n, t, c, m in j['roms']]
print('%d archivos opcionales (BRF_OPT)' % len(opcionales))

orden = sorted(juegos)
indice = {n: i for i, n in enumerate(orden)}
texto = bytearray()
cadenas = {}
def cad(s):
    if s not in cadenas:
        cadenas[s] = len(texto)
        texto.extend(s.encode('utf-8') + b'\0')
    return cadenas[s]

tabla_j, tabla_a = bytearray(), bytearray()
narch = 0
for n in orden:
    j = juegos[n]
    padre = indice.get(j['padre'], -1) if j['padre'] else -1
    # La BIOS: el "romof" cuando no es el padre (si lo es, la BIOS es la del padre).
    bios = j['bios'] if j['bios'] and j['bios'] != j['padre'] else (juegos[j['padre']]['bios'] if j['padre'] in juegos else None)
    bios = indice.get(bios, -1) if bios and bios != n else -1
    marcas = (1 if j['esbios'] else 0) | j['placa']
    tabla_j += struct.pack('<IIIiiHBBII', cad(n), cad(j['titulo']), cad(j['fab']), padre, bios, j['anio'], marcas, 0,
                           narch, len(j['roms']))
    for rn, tam, crc, m in j['roms']:
        tabla_a += struct.pack('<IIIB3x', cad(rn), tam, crc, m)
        narch += 1

with open(salida, 'wb') as f:
    f.write(b'ALLESIU-ARCADE1\0')
    f.write(struct.pack('<III', len(orden), narch, len(texto)))
    f.write(tabla_j)
    f.write(tabla_a)
    f.write(texto)
print('%s: %d juegos (%d de Neo Geo), %d archivos, %d KB' % (salida, len(orden), sum(1 for j in juegos.values() if j['placa'] == 2),
                                                       narch, os.path.getsize(salida) // 1024))
