# Rearma juegos arcade para ALLESIU: lee tus zips (de cualquier versión de MAME o FinalBurn Neo, sueltos en una o
# varias carpetas), busca cada archivo por su huella (CRC y tamaño) y arma en otra carpeta los zips que espera la versión
# de FinalBurn Neo que lleva ALLESIU (la tabla assets/misc/arcade.bin). Los arma "non-merged": cada juego en un zip que
# se basta solo (con lo que comparte con su juego principal), y las BIOS (neogeo.zip, pgm.zip...) en zips aparte.
#   python herramientas/reparar_arcade.py <carpeta de salida> <carpeta con tus zips> [otra carpeta...]
# Al final dice cuántos juegos quedaron completos y cuántos no se pudieron armar (y qué les falta, en faltantes.txt).
import os, struct, sys, zipfile, zlib

AQUI = os.path.dirname(os.path.abspath(__file__))
TABLA = os.path.join(AQUI, '..', 'assets', 'misc', 'arcade.bin')

def leer_tabla():
    d = open(TABLA, 'rb').read()
    assert d[:16] == b'ALLESIU-ARCADE1\0', 'arcade.bin no válido'
    nj, na, nt = struct.unpack_from('<III', d, 16)
    oj = 28
    oa = oj + nj * 32
    ot = oa + na * 16
    def txt(o):
        return d[ot + o:d.index(b'\0', ot + o)].decode('utf-8')
    juegos = []
    for i in range(nj):
        nom, tit, fab, padre, bios, anio, marcas, _, prim, cuant = struct.unpack_from('<IIIiiHBBII', d, oj + i * 32)
        arch = []
        for k in range(cuant):
            an, tam, crc, m = struct.unpack_from('<IIIB', d, oa + (prim + k) * 16)
            arch.append((txt(an), tam, crc, m))
        juegos.append(dict(nombre=txt(nom), titulo=txt(tit), padre=padre, bios=bios, esbios=bool(marcas & 1), archivos=arch))
    return juegos

def main():
    if len(sys.argv) < 3:
        print(__doc__ if __doc__ else 'uso: reparar_arcade.py <salida> <carpeta> [carpeta...]'); return
    salida, entradas = sys.argv[1], sys.argv[2:]
    os.makedirs(salida, exist_ok=True)
    juegos = leer_tabla()
    # Índice de todo lo que hay en tus zips: (crc, tamaño) → (zip, nombre adentro).
    hay = {}
    nzips = 0
    for carpeta in entradas:
        for raiz, _, nombres in os.walk(carpeta):
            for n in nombres:
                if not n.lower().endswith('.zip'):
                    continue
                ruta = os.path.join(raiz, n)
                try:
                    with zipfile.ZipFile(ruta) as z:
                        for info in z.infolist():
                            if not info.is_dir():
                                hay.setdefault((info.CRC, info.file_size), []).append((ruta, info.filename))
                    nzips += 1
                except (zipfile.BadZipFile, OSError):
                    print('dañado, se saltea:', ruta)
    print('%d zips leídos, %d archivos distintos' % (nzips, len(hay)))

    completos, incompletos = 0, []
    for j in juegos:
        # Los archivos de la BIOS van en el zip de la BIOS, no en el de cada juego.
        de_bios = set()
        if j['bios'] >= 0 and not j['esbios']:
            de_bios = {(c, t) for _, t, c, _ in juegos[j['bios']]['archivos']}
        necesarios = [(n, t, c) for n, t, c, m in j['archivos'] if not (m & 2) and ((c, t) not in de_bios or j['esbios'])]
        if not necesarios:
            continue
        faltan = [n for n, t, c in necesarios if (c, t) not in hay]
        if faltan:
            if len(faltan) < len(necesarios):   # algo había: vale la pena avisar
                incompletos.append((j['nombre'], j['titulo'], faltan))
            continue
        # Cada archivo, de la primera copia que se pueda leer y tenga la huella correcta (hay zips dañados en las
        # colecciones: esos se saltean y se prueba con otra copia). Si alguno no se consigue, el juego no se arma.
        datos_de, mal = {}, []
        for n, t, c in necesarios:
            if n in datos_de:
                continue
            for ruta, interno in hay[(c, t)]:
                try:
                    with zipfile.ZipFile(ruta) as ze:
                        datos = ze.read(interno)
                except Exception:
                    continue
                if zlib.crc32(datos) & 0xFFFFFFFF == c:
                    datos_de[n] = datos
                    break
            else:
                mal.append(n)
        if mal:
            incompletos.append((j['nombre'], j['titulo'], ['%s (dañado)' % n for n in mal]))
            continue
        destino = os.path.join(salida, j['nombre'] + '.zip')
        with zipfile.ZipFile(destino, 'w', zipfile.ZIP_DEFLATED) as zs:
            for n, datos in datos_de.items():
                zs.writestr(n, datos)
        completos += 1
    with open(os.path.join(salida, 'faltantes.txt'), 'w', encoding='utf-8') as f:
        for nombre, titulo, faltan in incompletos:
            f.write('%s (%s): faltan %s\n' % (nombre, titulo, ', '.join(faltan)))
    print('%d juegos y BIOS armados en %s; %d incompletos (ver faltantes.txt)' % (completos, salida, len(incompletos)))

main()
