# Compara capturas: referencia (izquierda) y emulador propio (derecha), cuadro por cuadro, y mide la diferencia.
# Uso: python -I pc/comparar.py <carpeta referencia> <carpeta propia> <salida.png> [desde] [hasta]
import sys, glob, os
from PIL import Image, ImageChops, ImageDraw
ref, mia, out = sys.argv[1], sys.argv[2], sys.argv[3]
desde = int(sys.argv[4]) if len(sys.argv) > 4 else 0
hasta = int(sys.argv[5]) if len(sys.argv) > 5 else 99999
pares = []
for f in sorted(glob.glob(os.path.join(ref, 'juego_*.png'))):
    n = int(os.path.basename(f)[6:10])
    m = os.path.join(mia, os.path.basename(f))
    if desde <= n <= hasta and os.path.exists(m): pares.append((n, f, m))
W, H = 256, 224
hoja = Image.new('RGB', (W * 2 + 8, (H + 18) * len(pares)), (40, 40, 40))
d = ImageDraw.Draw(hoja)
for k, (n, f, m) in enumerate(pares):
    a = Image.open(f).convert('RGB').resize((W, H)); b = Image.open(m).convert('RGB')
    dif = ImageChops.difference(a, b).convert('L')
    pct = sum(1 for v in dif.getdata() if v > 40) * 100 / (W * H)
    y = k * (H + 18)
    d.text((4, y + 2), 'cuadro %d   diferente: %.0f%%' % (n, pct), fill=(255, 255, 0))
    hoja.paste(a, (0, y + 18)); hoja.paste(b, (W + 8, y + 18))
    print(n, '%.1f%%' % pct)
hoja.save(out)
