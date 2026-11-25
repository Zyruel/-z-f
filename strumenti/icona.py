# Disegna l'icona di [z]f e scrive src\z-f.ico.  Uso:  python strumenti\icona.py [--anteprima cartella]
#
# "[z]" nel blu d'accento su un quadrato arrotondato col fondo di [z]f: i colori sono C_ACCENT e
# C_BG di src\theme.h (se cambiano lì, si cambiano qui e si rigenera). Disegno geometrico, senza
# font, così non ci sono questioni di licenza. Serve Pillow (pip install pillow): è uno strumento
# usato una volta, come strumenti\firma.cpp, non una dipendenza di [z]f. Il file .ico si committa.
#
# Le misure piccole (16 e 20 px) hanno un disegno a pixel fatto a mano: ridotto da quello grande,
# "[z]" diventerebbe una macchia. Dalla 24 in su la geometria è la stessa, con i bordi agganciati
# ai pixel della misura finale; si disegna a 4× e si riduce, così diagonale e angoli sono sfumati.
# Sotto 256 le immagini sono BMP a 32 bit (le legge qualunque Windows), la 256 è PNG (più piccola).

import io
import os
import struct
import sys

from PIL import Image, ImageDraw

FONDO = (0x1B, 0x1C, 0x1E, 255)    # C_BG
ACCENTO = (0x4A, 0xA3, 0xFF, 255)  # C_ACCENT
MISURE = [16, 20, 24, 32, 40, 48, 64, 256]
SS = 4  # sovracampionamento (riduzione con BOX: media esatta, senza aloni scuri)

# Disegni a pixel delle misure piccole: "#" è l'accento, il resto è fondo
PIXEL = {
    16: [
        "................",
        "................",
        ".###........###.",
        ".##..........##.",
        ".##..######..##.",
        ".##..######..##.",
        ".##......##..##.",
        ".##.....##...##.",
        ".##...##.....##.",
        ".##..##......##.",
        ".##..######..##.",
        ".##..######..##.",
        ".##..........##.",
        ".###........###.",
        "................",
        "................",
    ],
    20: [
        "....................",
        "....................",
        "....................",
        "..####........####..",
        "..##............##..",
        "..##............##..",
        "..##..########..##..",
        "..##..########..##..",
        "..##.......###..##..",
        "..##.....###....##..",
        "..##....###.....##..",
        "..##..###.......##..",
        "..##..########..##..",
        "..##..########..##..",
        "..##............##..",
        "..##............##..",
        "..####........####..",
        "....................",
        "....................",
        "....................",
    ],
}


def fondo(s, inset):
    """Quadrato arrotondato a 4×, con l'angolo in proporzione alla misura."""
    S = s * SS
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = round(s * 0.22) * SS
    d.rounded_rectangle([inset * SS, inset * SS, S - inset * SS - 1, S - inset * SS - 1], r, fill=FONDO)
    return img, d


def piccola(s):
    img, d = fondo(s, 0)
    righe = PIXEL[s]
    for y, riga in enumerate(righe):
        for x, c in enumerate(riga):
            if c == "#":
                d.rectangle([x * SS, y * SS, (x + 1) * SS - 1, (y + 1) * SS - 1], fill=ACCENTO)
    return img.resize((s, s), Image.BOX)


def grande(s):
    inset = round(s / 32) if s >= 32 else 0
    img, d = fondo(s, inset)
    R = lambda f: round(f * s)  # agganciato ai pixel della misura finale
    t = max(2, R(0.085))        # spessore dei tratti
    # Parentesi: asta verticale e due grazie orizzontali
    bx0, bx1 = R(0.20), R(0.20) + R(0.16)
    by0, by1 = R(0.20), s - R(0.20)
    # z minuscola, centrata in verticale come "[z]" in un logo
    zx0, zx1 = R(0.37), s - R(0.37)
    zh = R(0.32)
    zy0 = (s - zh) // 2
    zy1 = zy0 + zh
    rett = []
    for sx in (False, True):  # sinistra, poi la speculare a destra
        m = (lambda x: s - x) if sx else (lambda x: x)
        xa, xb = sorted((m(bx0), m(bx1)))
        xs = sorted((m(bx0), m(bx0 + t)))
        rett += [(xs[0], by0, xs[1], by1), (xa, by0, xb, by0 + t), (xa, by1 - t, xb, by1)]
    rett += [(zx0, zy0, zx1, zy0 + t), (zx0, zy1 - t, zx1, zy1)]
    for x0, y0, x1, y1 in rett:
        d.rectangle([x0 * SS, y0 * SS, x1 * SS - 1, y1 * SS - 1], fill=ACCENTO)
    # Diagonale della z: dall'angolo in alto a destra a quello in basso a sinistra, spessore come i tratti
    dx = t * 1.35
    d.polygon([((zx1 - dx) * SS, (zy0 + t) * SS), (zx1 * SS, (zy0 + t) * SS),
               ((zx0 + dx) * SS, (zy1 - t) * SS), (zx0 * SS, (zy1 - t) * SS)], fill=ACCENTO)
    return img.resize((s, s), Image.BOX)


def bmp32(img):
    """Immagine di un .ico in formato BMP: BGRA dal basso, poi la maschera AND (tutta a zero)."""
    s = img.size[0]
    px = img.load()
    righe = []
    for y in range(s - 1, -1, -1):
        r = bytearray()
        for x in range(s):
            R_, G, B, A = px[x, y]
            r += bytes((B, G, R_, A))
        righe.append(bytes(r))
    maschera = b"\0" * (((s + 31) // 32) * 4 * s)
    testa = struct.pack("<IiiHHIIiiII", 40, s, s * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    return testa + b"".join(righe) + maschera


def png(img):
    b = io.BytesIO()
    img.save(b, "PNG", optimize=True)
    return b.getvalue()


def main():
    qui = os.path.dirname(os.path.abspath(__file__))
    uscita = os.path.join(qui, "..", "src", "z-f.ico")
    immagini = {s: (piccola(s) if s in PIXEL else grande(s)) for s in MISURE}
    dati = [png(immagini[s]) if s >= 256 else bmp32(immagini[s]) for s in MISURE]
    voci, pos = b"", 6 + 16 * len(MISURE)
    for s, b in zip(MISURE, dati):
        voci += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(b), pos)
        pos += len(b)
    with open(uscita, "wb") as f:
        f.write(struct.pack("<HHH", 0, 1, len(MISURE)) + voci + b"".join(dati))
    print("scritto", os.path.normpath(uscita), pos, "byte")

    # Anteprima: ogni misura ingrandita a pixel pieni, una accanto all'altra
    if "--anteprima" in sys.argv:
        cartella = sys.argv[sys.argv.index("--anteprima") + 1]
        os.makedirs(cartella, exist_ok=True)
        k = 6
        larg = sum(min(s, 64) * k + 24 for s in MISURE) + 24
        tela = Image.new("RGBA", (larg, 64 * k + 48), (0x80, 0x80, 0x80, 255))
        x = 24
        for s in MISURE:
            im = immagini[s] if s <= 64 else immagini[s].resize((64, 64), Image.BOX)
            im = im.resize((im.size[0] * k, im.size[1] * k), Image.NEAREST)
            tela.alpha_composite(im, (x, 24))
            x += im.size[0] + 24
        tela.save(os.path.join(cartella, "icona-anteprima.png"))
        immagini[256].save(os.path.join(cartella, "icona-256.png"))
        print("anteprima in", cartella)


if __name__ == "__main__":
    main()
