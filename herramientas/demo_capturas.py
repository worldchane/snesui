"""Arma pc/demo: una biblioteca de demostración para las capturas de la documentación (docs/capturas).

Los juegos tienen nombres inventados (en lo público no van nombres de juegos) y las carátulas son las del simulador
(pc/data/allesiu/caratulas) muy difuminadas: se ve que hay una carátula, pero no de qué juego es. La descarga
automática de carátulas queda apagada, para que no se bajen las verdaderas.

    python herramientas/demo_capturas.py
"""
import os
import random
import shutil

from PIL import Image, ImageFilter

AQUI = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEMO = os.path.join(AQUI, 'pc', 'demo')
DATOS = os.path.join(DEMO, 'data', 'allesiu')
USB = os.path.join(DEMO, 'usb')
CARATULAS_REALES = os.path.join(AQUI, 'pc', 'data', 'allesiu', 'caratulas')
BIOS_REALES = os.path.join(AQUI, 'pc', 'data', 'allesiu', 'bios')

# Consola (carpeta), extensión y juegos inventados (sin tildes: el simulador de Windows no lee bien esos nombres de archivo). Los que terminan en '*' quedan sin carátula (vidrio con iniciales).
JUEGOS = {
    'snes': ('sfc', ['Aventura del Bosque', 'Carrera Estelar', 'Caballeros de Cristal', 'El Dragon Final', 'Galaxia Rota',
                     'Isla de los Piratas', 'Leyenda del Viento', 'Mundo Pixel', 'Ninja de la Luna', 'Golpe de Hierro*',
                     'Reino de Arena', 'Robot Gigante', 'Selva Perdida', 'Torre Encantada', 'Volcan Rojo']),
    'nes': ('nes', ['Castillo Oscuro', 'Hermanos Saltarines', 'Laberinto 8 Bits', 'Patrulla Espacial', 'Tanque Valiente*']),
    'gb': ('gb', ['Bloques Locos', 'Joven Explorador', 'Mina Profunda*']),
    'gbc': ('gbc', ['Criaturas de Bolsillo', 'Jardin Secreto', 'Pista Turbo']),
    'gba': ('gba', ['Academia de Magos', 'Escuadra Alfa', 'Heroes del Futuro', 'Torneo Final*']),
    'megadrive': ('md', ['Erizo Veloz', 'Calles de Neon', 'Guerreros del Desierto', 'Rayo Azul']),
    'mastersystem': ('sms', ['Alex y el Tesoro', 'Fortaleza Lunar']),
    'gamegear': ('gg', ['Surf en la Playa*', 'Agente Secreto']),
}
GENEROS = ['Plataformas', 'Aventura', 'Rol', 'Carreras', 'Lucha', 'Acción', 'Puzle', 'Deportes']
DESCRIPCIONES = [
    'Un juego de demostración para mostrar cómo se ve la descripción en las vistas de lista. ALLESIU la lee del '
    'gamelist.xml que dejes junto a tus juegos (el de Batocera, ES-DE o un programa de datos).',
    'Recorre mundos llenos de secretos, junta tesoros y enfrenta a los jefes de cada nivel. Texto de ejemplo: los '
    'nombres de estos juegos son inventados.',
    'Hasta dos jugadores a la vez. Las vistas Lista y Lista detallada muestran la descripción, el año, el género y '
    'cuántos jugadores admite.',
]


def carátulas_reales():
    todas = []
    for raiz, _, archivos in os.walk(CARATULAS_REALES):
        todas += [os.path.join(raiz, a) for a in archivos if a.lower().endswith('.png')]
    return sorted(todas)


def difuminada(origen, destino):
    im = Image.open(origen).convert('RGB')
    im.thumbnail((300, 300))
    # Muy difuminada (no se lee ningún texto ni se reconoce el dibujo) y un poco más clara.
    im = im.filter(ImageFilter.GaussianBlur(max(im.size) / 16))
    im.save(destino)


def main():
    if os.path.isdir(DEMO):
        shutil.rmtree(DEMO)
    reales = carátulas_reales()
    if not reales:
        print('no hay carátulas en pc/data/allesiu/caratulas')
        return
    random.seed(15)
    os.makedirs(DATOS)
    k = 0
    for consola, (ext, juegos) in JUEGOS.items():
        carpeta = os.path.join(USB, 'roms', consola)
        os.makedirs(carpeta)
        os.makedirs(os.path.join(DATOS, 'caratulas', consola), exist_ok=True)
        lista = []
        for j in juegos:
            sin = j.endswith('*')
            nombre = j.rstrip('*')
            with open(os.path.join(carpeta, f'{nombre}.{ext}'), 'wb') as f:
                f.write(bytes(64 * 1024))
            if not sin:
                difuminada(reales[k % len(reales)], os.path.join(DATOS, 'caratulas', consola, nombre + '.png'))
                k += 1
            lista.append(nombre)
        # Un gamelist.xml con datos inventados (para las vistas con descripción).
        with open(os.path.join(carpeta, 'gamelist.xml'), 'w', encoding='utf-8') as f:
            f.write('<?xml version="1.0"?>\n<gameList>\n')
            for i, nombre in enumerate(lista):
                f.write(f'  <game>\n    <path>./{nombre}.{ext}</path>\n    <name>{nombre}</name>\n'
                        f'    <desc>{DESCRIPCIONES[i % len(DESCRIPCIONES)]}</desc>\n'
                        f'    <releasedate>{1989 + i % 10}0101T000000</releasedate>\n'
                        f'    <developer>Estudio de ejemplo</developer>\n    <genre>{GENEROS[i % len(GENEROS)]}</genre>\n'
                        f'    <players>{1 + i % 2}</players>\n  </game>\n')
            f.write('</gameList>\n')
    # Datos de la consola: ya configurada (sin asistente), estilos por defecto y sin bajar carátulas.
    textos = {
        'ajustes.txt': 'proporcion=0 filtro=0 avance=4 rebobinar=1 copiar=0 superfx=1 musica=1 caratulas=0 tema=0 menu=0 '
                       'botones=18 continuar=1 bezels=1 runahead=0 estilos=2\n',
        'asistente.txt': 'visto\n', 'version.txt': '1.5\n', 'estructura.txt': '3\n',
        'favoritos.txt': 'snes/Carrera Estelar\nsnes/Leyenda del Viento\ngba/Escuadra Alfa\n',
        'recientes.txt': 'snes/Leyenda del Viento\nmegadrive/Erizo Veloz\nnes/Castillo Oscuro\n',
    }
    for n, t in textos.items():
        with open(os.path.join(DATOS, n), 'w', encoding='utf-8', newline='\n') as f:   # la app compara sin '\r'
            f.write(t)
    # Las BIOS del simulador (solo en esta PC: pc/ no se sube), para que la pantalla de BIOS muestre algunas.
    if os.path.isdir(BIOS_REALES):
        shutil.copytree(BIOS_REALES, os.path.join(DATOS, 'bios'))
    print('demo lista:', sum(len(j) for _, j in JUEGOS.values()), 'juegos,', k, 'carátulas difuminadas')


if __name__ == '__main__':
    main()
