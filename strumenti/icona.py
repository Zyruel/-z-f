# Disegna l'icona di [z]f e scrive src\z-f.ico.  Uso:  python strumenti\icona.py [--anteprima cartella]
#
# "[z]" nel blu d'accento su un quadrato arrotondato col fondo di [z]f: i colori si leggono da
# C_ACCENT e C_BG di src\theme.h, così stanno in un posto solo (se cambiano lì, si rigenera).
# Disegno geometrico, senza font, così non ci sono questioni di licenza. Serve Pillow (pip install pillow): è uno strumento
# usato una volta, come strumenti\firma.cpp, non una dipendenza di [z]f. Il file .ico si committa.
#
# Le misure piccole (16 e 20 px) hanno un disegno a pixel fatto a mano: ridotto da quello grande,
# "[z]" diventerebbe una macchia. Dalla 24 in su la geometria è la stessa, con i bordi agganciati
# ai pixel della misura finale; si disegna a 4× e si riduce, così diagonale e angoli sono sfumati.
# Ci sono le misure di ogni DPI comune (100% 16/32, 125% 20/40, 150% 24/48, 175% 28/56, 200% 32/64,
# 225% 36/72): una misura che manca Windows la ricava riducendo un'altra, e i bordi agganciati ai
# pixel diventano sfumati con un alone (prove\icona-misure.cpp). Tutte in PNG: il file pesa 8,7 KB
# invece dei 62 in BMP a 32 bit; Windows legge i PNG a ogni misura da Vista, e la prima
# decodifica costa all'avvio circa 1,6 ms in più, dopo il primo frame (ARCHITETTURA).

import io
import os
import re
import struct
import sys

from PIL import Image, ImageDraw

QUI = os.path.dirname(os.path.abspath(__file__))


def colore(nome):
    """Un colore di src\\theme.h, scritto lì come rgba(0xRR, 0xGG, 0xBB)."""
    with open(os.path.join(QUI, "..", "src", "theme.h"), encoding="utf-8") as f:
        m = re.search(r"\b" + nome + r"\s*=\s*rgba\(\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*\)", f.read())
    if not m:
        sys.exit(nome + " non trovato in src\\theme.h")
    return tuple(int(x, 16) for x in m.groups()) + (255,)


FONDO = colore("C_BG")
ACCENTO = colore("C_ACCENT")
MISURE = [16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72, 256]
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


def png(img):
    b = io.BytesIO()
    img.save(b, "PNG", optimize=True)
    return b.getvalue()


def main():
    uscita = os.path.join(QUI, "..", "src", "z-f.ico")
    immagini = {s: (piccola(s) if s in PIXEL else grande(s)) for s in MISURE}
    dati = [png(immagini[s]) for s in MISURE]
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
