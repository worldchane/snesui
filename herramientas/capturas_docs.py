"""Saca las capturas de la documentación (docs/capturas) con el simulador y la biblioteca de demostración (pc/demo, ver
herramientas/demo_capturas.py: nombres inventados y carátulas difuminadas).

    python herramientas/demo_capturas.py
    python herramientas/capturas_docs.py [nombre ...]

Cada captura arranca el simulador de cero con unos ajustes y un guion de botones (variable PASOS de pc/simular.cpp).
"""
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

AQUI = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATOS = os.path.join(AQUI, 'pc', 'demo', 'data', 'allesiu')
CRUDAS = os.path.join(AQUI, 'pc', 'salida', 'docs')
DESTINO = os.path.join(AQUI, 'docs', 'capturas')
FUENTE = os.path.join(AQUI, 'assets', 'Inter-SemiBold.ttf')

AJUSTES = dict(proporcion=0, filtro=0, avance=4, rebobinar=1, copiar=0, superfx=1, musica=1, caratulas=0, tema=0, menu=0,
               botones=18, continuar=1, bezels=1, runahead=0, estilos=2)
ORDEN = ['proporcion', 'filtro', 'avance', 'rebobinar', 'copiar', 'superfx', 'musica', 'caratulas', 'tema', 'menu',
         'botones', 'continuar', 'bezels', 'runahead', 'estilos']

MENUS = ['Clásico', 'Vitrina', 'Mosaico', 'Galería', 'Compacto', 'Grande', 'Minimal', 'Etiquetas', 'Lista',
         'Lista detallada', 'Carrusel', 'Escenario']
TEMAS = {1: 'Aurora boreal', 2: 'Noche violeta', 3: 'Aero cristal', 5: 'Atardecer', 7: 'Galaxia', 9: 'Bolsillo verde',
         12: 'Sakura', 13: 'Desierto', 17: 'Cuadros retro'}
BOTONES = [0, 3, 7, 12, 18, 25, 33, 41]

# nombre: (cambios a los ajustes, pasos, opciones). Opciones: 'asistente' (sin haberlo visto), 'novedades' (versión
# anterior), 'vacia' (sin juegos).
CAPTURAS = {
    'biblioteca': ({}, '4.8 DER; 5.0 DER; 5.8 - biblioteca', ''),
    'novedades': ({}, '5.6 - novedades', 'novedades'),
    'asistente_1': ({}, '5.6 - asistente_1', 'asistente'),
    'asistente_2': ({}, '5.0 X; 5.8 - asistente_2', 'asistente'),
    'asistente_3': ({}, '5.0 X; 5.4 X; 6.2 - asistente_3', 'asistente'),
    'asistente_4': ({}, '5.0 X; 5.4 X; 5.8 X; 6.6 - asistente_4', 'asistente'),
    'asistente_5': ({}, '5.0 X; 5.4 X; 5.8 X; 6.2 X; 7.0 - asistente_5', 'asistente'),
    'vacia': ({}, '5.6 - vacia', 'vacia'),
    'pestana_snes': ({}, '4.8 R1; 5.1 DER; 5.8 - pestana_snes', ''),
    'favoritos': ({}, '4.8 L1; 5.0 L1; 5.8 - favoritos', ''),
    'recientes': ({}, '4.8 L1; 5.0 L1; 5.2 L1; 6.0 - recientes', ''),
    'opciones_juego': ({}, '4.8 DER; 5.1 OPT; 5.9 - opciones_juego', ''),
    'buscar': ({}, '4.8 TACTIL; 5.2 DER; 5.35 DER; 5.5 X; 5.65 IZQ; 5.8 IZQ; 5.95 X; 6.8 - buscar', ''),
    'config': ({}, '4.8 L1; 5.6 - config', ''),
    'config_abajo': ({}, '4.8 L1; 5.0 ARR; 5.15 ARR; 5.3 ARR; 5.45 ARR; 5.6 ARR; 5.75 ARR; 6.6 - config_abajo', ''),
    'consolas': ({}, '4.8 L1; 5.0 ABA; 5.15 ABA; 5.4 X; 6.2 - consolas', ''),
    'bios': ({}, '4.8 L1; 5.0 ARR; 5.15 ARR; 5.3 ARR; 5.6 X; 6.4 - bios', ''),
    'acerca': ({}, '4.8 L1; 5.0 ARR; 5.3 X; 6.1 - acerca', ''),
}
for i, n in enumerate(MENUS):
    CAPTURAS[f'menu_{i:02d}'] = ({'menu': i}, '4.8 DER; 5.0 ABA; 5.8 - menu_%02d' % i, '')
for t in TEMAS:
    CAPTURAS[f'tema_{t:02d}'] = ({'tema': t}, '4.8 DER; 5.6 - tema_%02d' % t, '')
for b in BOTONES:
    CAPTURAS[f'botones_{b:02d}'] = ({'botones': b}, '5.6 - botones_%02d' % b, '')


def escribir(nombre, texto):
    with open(os.path.join(DATOS, nombre), 'w', encoding='utf-8', newline='\n') as f:
        f.write(texto)


def preparar(cambios, opcion):
    a = dict(AJUSTES, **cambios)
    escribir('ajustes.txt', ' '.join(f'{k}={a[k]}' for k in ORDEN) + '\n')
    escribir('version.txt', '1.4\n' if opcion == 'novedades' else '1.5\n')
    marca = os.path.join(DATOS, 'asistente.txt')
    if opcion == 'asistente':
        if os.path.exists(marca):
            os.remove(marca)
    else:
        escribir('asistente.txt', 'visto\n')


def sacar(nombre):
    cambios, pasos, opcion = CAPTURAS[nombre]
    env = dict(os.environ, SALIDA_PC='pc/salida/docs', PASOS=pasos)
    if opcion == 'vacia':
        vacia = os.path.join(AQUI, 'pc', 'demo', 'vacia')
        os.makedirs(os.path.join(vacia, 'usb'), exist_ok=True)
        os.makedirs(os.path.join(vacia, 'data', 'allesiu'), exist_ok=True)
        env.update(DATA_PC='pc/demo/vacia/data', USB_PC='pc/demo/vacia/usb')
        global DATOS
        guardado, DATOS = DATOS, os.path.join(vacia, 'data', 'allesiu')
        preparar(cambios, opcion)
        DATOS = guardado
    else:
        env.update(DATA_PC='pc/demo/data', USB_PC='pc/demo/usb')
        preparar(cambios, opcion)
    subprocess.run([os.path.join(AQUI, 'pc', 'salida', 'simular.exe')], cwd=AQUI, env=env, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, timeout=120)
    origen = os.path.join(CRUDAS, nombre + '.png')
    if not os.path.exists(origen):
        print('FALTA', nombre)
        return
    im = Image.open(origen).convert('RGB').resize((1280, 720), Image.LANCZOS)
    im.save(os.path.join(DESTINO, nombre + '.jpg'), quality=88, optimize=True)
    print('lista', nombre)


def mosaico(nombre, piezas, columnas, recorte=None, ancho=640):
    """Junta varias capturas (con su nombre abajo) en una sola imagen."""
    fuente = ImageFont.truetype(FUENTE, 26)
    imgs = []
    for archivo, titulo in piezas:
        ruta = os.path.join(CRUDAS, archivo + '.png')
        if not os.path.exists(ruta):
            continue
        im = Image.open(ruta).convert('RGB')
        if recorte:
            im = im.crop(recorte)
        alto = round(im.height * ancho / im.width)
        imgs.append((im.resize((ancho, alto), Image.LANCZOS), titulo))
    if not imgs:
        return
    w, h = imgs[0][0].size
    pie, sep = 44, 12
    filas = (len(imgs) + columnas - 1) // columnas
    hoja = Image.new('RGB', (columnas * w + (columnas + 1) * sep, filas * (h + pie) + (filas + 1) * sep), (14, 10, 26))
    d = ImageDraw.Draw(hoja)
    for i, (im, titulo) in enumerate(imgs):
        x = sep + (i % columnas) * (w + sep)
        y = sep + (i // columnas) * (h + pie + sep)
        hoja.paste(im, (x, y))
        d.text((x + w / 2, y + h + pie / 2), titulo, font=fuente, fill=(235, 230, 250), anchor='mm')
    hoja.save(os.path.join(DESTINO, nombre + '.jpg'), quality=88, optimize=True)
    print('mosaico', nombre)


def main():
    os.makedirs(CRUDAS, exist_ok=True)
    os.makedirs(DESTINO, exist_ok=True)
    nombres = sys.argv[1:] or list(CAPTURAS)
    for n in nombres:
        sacar(n)
    mosaico('menus', [(f'menu_{i:02d}', n) for i, n in enumerate(MENUS)], 3)
    mosaico('interfaces', [('biblioteca', 'Aero clásico')] + [(f'tema_{t:02d}', n) for t, n in TEMAS.items()], 3)
    # Los botones: la franja de abajo de la pantalla.
    import re
    nombres = re.findall(r'\{"([^"]+)", BF_', open(os.path.join(AQUI, 'src', 'comun', 'estilo.cpp'), encoding='utf-8').read())
    mosaico('botones', [(f'botones_{b:02d}', nombres[b] if b < len(nombres) else f'Estilo {b + 1}') for b in BOTONES], 2,
            recorte=(900, 1030, 1860, 1080),
            ancho=960)
    escribir('ajustes.txt', ' '.join(f'{k}={AJUSTES[k]}' for k in ORDEN) + '\n')


if __name__ == '__main__':
    main()
