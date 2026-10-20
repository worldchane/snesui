"""Traduce /data/allesiu/cierre.txt (lo que anota la app al caerse: ver CapturarCierres en plataforma.cpp) a nombres de
funciones, con el mapa del ejecutable de esa compilación.

    python herramientas/cierre.py cierre.txt obj/prueba39/eboot.map
"""
import bisect
import re
import sys


def leer_mapa(ruta):
    """Las funciones de .text del mapa de lld: (dirección, tamaño, nombre), ordenadas."""
    funciones = []
    en_texto = False
    for linea in open(ruta, encoding='utf-8', errors='replace'):
        partes = linea.split(None, 4)   # dirección, carga, tamaño, alineación y el resto (sección, archivo o símbolo)
        if len(partes) < 5:
            continue
        nombre = partes[4].strip()
        if nombre.startswith('.') and ' ' not in nombre:
            en_texto = nombre == '.text'
            continue
        # Solo los símbolos (no los archivos "x.o:(.text...)" ni las líneas QUAD o de asignación).
        if not en_texto or ':(' in nombre or nombre.startswith('QUAD') or ' = ' in nombre:
            continue
        try:
            dir_ = int(partes[0], 16)
            tam = int(partes[2], 16)
        except ValueError:
            continue
        funciones.append((dir_, tam, nombre))
    funciones.sort()
    return funciones


def buscar(funciones, inicios, dir_):
    i = bisect.bisect_right(inicios, dir_) - 1
    if i < 0:
        return None
    f = funciones[i]
    return f'{f[2]} +0x{dir_ - f[0]:x}'


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return
    funciones = leer_mapa(sys.argv[2])
    inicios = [f[0] for f in funciones]
    ref_mapa = next((f[0] for f in funciones if f[2].startswith('CapturarCierres(')), None)
    if ref_mapa is None:
        print('el mapa no tiene CapturarCierres(): ¿es el de la misma compilación?')
        return
    base = None
    for linea in open(sys.argv[1], encoding='utf-8', errors='replace'):
        linea = linea.strip()
        m = re.match(r'cierre senal=(\d+) dir=(0x[0-9a-f]+)(?: rip=(0x[0-9a-f]+))? ref=(0x[0-9a-f]+)', linea)
        if m:
            senal, dir_, ref = int(m[1]), int(m[2], 16), int(m[4], 16)
            base = ref - ref_mapa
            nombres = {4: 'SIGILL', 8: 'SIGFPE', 10: 'SIGBUS', 11: 'SIGSEGV'}
            print(f'\n== cierre: {nombres.get(senal, senal)}, dirección 0x{dir_:x}', end='')
            n = buscar(funciones, inicios, dir_ - base) if 0 <= dir_ - base < 0x4000000 else None
            print(f' (es código: {n})' if n else '')
            continue
        if linea.startswith('contexto') and base is not None:
            for k, d in re.findall(r'(\d+):(0x[0-9a-f]+)', linea):
                print(f'   registro {k}: {buscar(funciones, inicios, int(d, 16) - base)}')
            continue
        if (linea.startswith('pila') or linea.startswith('0x')) and base is not None:   # la pila sigue en varias líneas
            for d in re.findall(r'0x[0-9a-f]+', linea):
                n = buscar(funciones, inicios, int(d, 16) - base)
                if n:
                    print(f'   venía de: {n}')


if __name__ == '__main__':
    main()
