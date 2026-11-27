# [z]f — cartella di prova per il pannello di anteprima: file finti di ogni tipo.
# Uso:  python prove\anteprima-prova.py <cartella>
# Il JPEG ruotato (EXIF), lo scatto e la WebP servono Pillow; senza, si saltano.
import os, struct, zlib, random, sys
sys.stdout.reconfigure(encoding="utf-8")   # la console di Windows non è UTF-8, e -I ignora PYTHONUTF8

root = sys.argv[1]
os.makedirs(root, exist_ok=True)

def w(name, data):
    with open(os.path.join(root, name), 'wb') as f:
        f.write(data)

def png(wd, ht, fn, alpha=False):
    rows = []
    for y in range(ht):
        r = bytearray([0])
        for x in range(wd):
            r += bytes(fn(x, y))
        rows.append(bytes(r))
    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', wd, ht, 8, 6 if alpha else 2, 0, 0, 0)) + \
        chunk(b'IDAT', zlib.compress(b''.join(rows), 6)) + chunk(b'IEND', b'')

def bmp(wd, ht):
    rowsz = (wd * 3 + 3) & ~3
    data = bytearray()
    for y in range(ht):
        r = bytearray()
        for x in range(wd):
            r += bytes((x % 256, y % 256, 128))
        data += r + b'\0' * (rowsz - wd * 3)
    return b'BM' + struct.pack('<IHHI', 54 + len(data), 0, 0, 54) + \
        struct.pack('<IiiHHIIiiII', 40, wd, ht, 1, 24, 0, len(data), 2835, 2835, 0, 0) + bytes(data)

# Immagini
w('foto_grande.png', png(4000, 3000, lambda x, y: (x * 255 // 4000, y * 255 // 3000, 255 if (x // 200 + y // 200) % 2 else 80)))
w('icona_trasparente.png', png(64, 64, lambda x, y: (255, 120, 0, 255 if (x - 32) ** 2 + (y - 32) ** 2 < 900 else 0), alpha=True))
w('disegno.bmp', bmp(300, 200))
w('rotta.jpg', b'non sono un jpeg' * 10)

# Testo
lines = ['Riga %05d: àèìòù €  il testo prosegue con parole qualsiasi per vedere come va a capo nel pannello stretto' % i
         for i in range(12000)]
w('testo_lungo.txt', '\r\n'.join(lines).encode('utf-8'))  # oltre 256 KB
w('esempio.c', b'#include <stdio.h>\n\nint main(void) {\n\tfor (int i = 0; i < 10; i++) {\n\t\tprintf("%d\\n", i);\n\t}\n\treturn 0;\n}\n')
w('dati.json', b'{"nome": "prova", "valori": [1, 2, 3], "annidato": {"a": true, "b": null}}\n')
w('minimizzato.js', ('var a=' + ','.join(str(i) for i in range(30000)) + ';').encode('ascii'))  # una riga sola
w('vecchio_ansi.txt', 'Perché così è più città — “virgolette”\r\nSeconda riga\r\n'.encode('cp1252'))
w('unicode16.txt', '﻿Testo in UTF-16\r\nこんにちは\r\n'.encode('utf-16-le'))
w('senza_bom.dat16', 'Questo è UTF-16 senza BOM, riconosciuto dagli zeri\r\n'.encode('utf-16-le') * 20)
w('LEGGIMI', b'File senza estensione\nma di testo.\n')
w('servizio.log', b''.join(b'2026-11-25 08:%02d:%02d INFO evento %d\n' % (i // 60 % 60, i % 60, i) for i in range(500)))
w('vuoto.txt', b'')
w('vuoto_sconosciuto', b'')

# Binari
random.seed(1)
w('binario.bin', bytes(random.getrandbits(8) for _ in range(50000)))
w('sconosciuto.xyz', bytes(random.getrandbits(8) for _ in range(5000)))

# Cartelle
d = os.path.join(root, 'molti_file')
os.makedirs(os.path.join(d, 'sottocartella'), exist_ok=True)
for i in range(3000):
    open(os.path.join(d, 'file_%04d.txt' % i), 'wb').write(b'x')
os.makedirs(os.path.join(root, 'cartella_vuota'), exist_ok=True)
sub = os.path.join(root, 'progetto')
os.makedirs(os.path.join(sub, 'src'), exist_ok=True)
for n in ('main.cpp', 'README.md', 'build.bat', 'icona.png'):
    open(os.path.join(sub, n), 'wb').write(b'contenuto')

try:
    from PIL import Image, ImageDraw
except ImportError:
    Image = None
if Image:
    # 6000 x 4000 con orientamento EXIF 6: la freccia gialla deve puntare in alto
    im = Image.new('RGB', (6000, 4000), (40, 90, 160))
    dr = ImageDraw.Draw(im)
    for x in range(0, 6000, 300):
        dr.line([(x, 0), (x, 4000)], fill=(200, 200, 200), width=6)
    dr.polygon([(500, 2000), (2000, 800), (2000, 3200)], fill=(255, 220, 0))
    dr.rectangle([2000, 1600, 5000, 2400], fill=(255, 220, 0))
    exif = Image.Exif()
    exif[274] = 6
    im.save(os.path.join(root, 'telefono_ruotata.jpg'), quality=90, exif=exif.tobytes())
    im.resize((1500, 1000)).save(os.path.join(root, 'scatto.jpg'), quality=85)
    im2 = Image.new('RGBA', (800, 600), (0, 0, 0, 0))
    ImageDraw.Draw(im2).ellipse([100, 100, 700, 500], fill=(80, 200, 120, 200))
    im2.save(os.path.join(root, 'trasparente.webp'), 'WEBP')
print('ok:', root)
