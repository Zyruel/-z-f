# [z]f — cartella di prova per la vista a griglia: migliaia di immagini finte di misure diverse,
# più i casi difficili (file rotti, vuoti, troncati, PDF, cartelle, nomi lunghi).
# Uso:  python prove\griglia-prova.py <cartella> [scala]
#   scala 1 (predefinita) = circa 3.000 file; 3 = circa 9.000.
# Serve Pillow (pip install pillow).
import os, sys, random
sys.stdout.reconfigure(encoding="utf-8")   # la console di Windows non è UTF-8, e -I ignora PYTHONUTF8
from PIL import Image, ImageDraw

root = sys.argv[1]
scale = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
os.makedirs(root, exist_ok=True)
rnd = random.Random(42)

def base(w, h, noisy):
    """Sfondo a gradiente con rumore (come una foto) o a tinte piatte (come uno screenshot)."""
    g = Image.linear_gradient('L').resize((w, h))
    r = rnd.randrange(256)
    img = Image.merge('RGB', (g, g.rotate(90).resize((w, h)), Image.new('L', (w, h), r)))
    if noisy:
        n = Image.effect_noise((w, h), 40).convert('RGB')
        img = Image.blend(img, n, 0.25)
    return img

def stamp(img, i, color):
    """Un rettangolo colorato e il numero: le voci si distinguono a colpo d'occhio."""
    im = img.copy()
    d = ImageDraw.Draw(im)
    w, h = im.size
    s = min(w, h)
    x, y = rnd.randrange(max(1, w - s // 2)), rnd.randrange(max(1, h - s // 2))
    d.rectangle([x, y, x + s // 2, y + s // 2], fill=color)
    d.text((x + 10, y + 10), str(i), fill=(255, 255, 255))
    return im

def col():
    return (rnd.randrange(256), rnd.randrange(256), rnd.randrange(256))

def exif_orient(o):
    ex = Image.Exif()
    ex[274] = o
    return ex.tobytes()

def n(k):
    return max(1, int(k * scale))

count = 0
def made(name):
    global count
    count += 1
    if count % 200 == 0:
        print(count, name, flush=True)

# Foto del telefono: 4032x3024, alcune ruotate con l'EXIF
b = base(4032, 3024, True)
for i in range(n(400)):
    im = stamp(b, i, col())
    name = 'foto_%05d.jpg' % i
    if i % 5 == 0:
        im.save(os.path.join(root, name), quality=88, exif=exif_orient(6))
    else:
        im.save(os.path.join(root, name), quality=88)
    made(name)

# Immagini medie 1920x1080
b = base(1920, 1080, True)
for i in range(n(1600)):
    name = 'img_%05d.jpg' % i
    stamp(b, i, col()).save(os.path.join(root, name), quality=85)
    made(name)

# Screenshot PNG 1920x1080 a tinte piatte
b = base(1920, 1080, False)
for i in range(n(600)):
    name = 'screenshot_%05d.png' % i
    stamp(b, i, col()).save(os.path.join(root, name), optimize=False)
    made(name)

# Icone e immagini piccole con trasparenza
for i in range(n(200)):
    s = rnd.choice([16, 32, 48, 64, 128, 256, 512])
    im = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(im).ellipse([s // 8, s // 8, s - s // 8, s - s // 8], fill=col() + (255,))
    name = 'icona_%04d_%dpx.png' % (i, s)
    im.save(os.path.join(root, name))
    made(name)

# Panorami e immagini molto alte: lo spazio della voce resta fisso
for i in range(n(20)):
    stamp(base(8000, 2000, True), i, col()).save(os.path.join(root, 'panorama_%03d.jpg' % i), quality=85)
    stamp(base(1080, 4000, False), i, col()).save(os.path.join(root, 'lunga_%03d.png' % i))
    made('panorama')

# Altri formati
b = base(1200, 900, True)
for i in range(n(10)):
    stamp(b, i, col()).save(os.path.join(root, 'altro_%03d.bmp' % i))
    stamp(b, i, col()).save(os.path.join(root, 'altro_%03d.gif' % i))
    stamp(b, i, col()).save(os.path.join(root, 'altro_%03d.tif' % i))
    stamp(b, i, col()).save(os.path.join(root, 'altro_%03d.webp' % i), quality=80)
    stamp(b, i, col()).save(os.path.join(root, 'documento_%03d.pdf' % i))
    made('altro')

# Casi difficili: rotti, vuoti, troncati, estensione sbagliata, nome lunghissimo
for i in range(5):
    with open(os.path.join(root, 'rotta_%d.jpg' % i), 'wb') as f:
        f.write(os.urandom(5000))
    open(os.path.join(root, 'vuota_%d.png' % i), 'wb').close()
with open(os.path.join(root, 'foto_00001.jpg'), 'rb') as f:
    data = f.read()
with open(os.path.join(root, 'troncata.jpg'), 'wb') as f:
    f.write(data[:len(data) // 3])
with open(os.path.join(root, 'in_realta_un_png.jpg'), 'wb') as f:
    with open(os.path.join(root, 'screenshot_00000.png'), 'rb') as g:
        f.write(g.read())
long = 'un nome molto lungo che non sta in una riga e nemmeno in due righe della griglia ' * 2
stamp(b, 0, col()).save(os.path.join(root, long.strip() + '.jpg'), quality=80)

# File che non sono immagini e qualche cartella
for i in range(n(30)):
    with open(os.path.join(root, 'nota_%03d.txt' % i), 'w', encoding='utf-8') as f:
        f.write('nota %d\n' % i)
    with open(os.path.join(root, 'archivio_%03d.zip' % i), 'wb') as f:
        f.write(b'PK\x05\x06' + b'\0' * 18)
for i in range(n(20)):
    os.makedirs(os.path.join(root, 'cartella_%03d' % i), exist_ok=True)

print('fatto:', count, 'gruppi; cartella', root)
