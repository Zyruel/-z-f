# [z]f — rotellina e Ctrl+rotellina mandate a una finestra di prova, per PID.
# Uso:  python -I prove\rotella.py <pid> <scatti> [ctrl|semplice] [x y]
#   scatti: positivi verso l'alto (zoom avanti), negativi verso il basso;
#   ctrl (predefinito) tiene Ctrl, semplice no;
#   x y: il punto, in pixel dentro la finestra (dall'angolo in alto a sinistra della parte
#   interna); senza, il centro della finestra.
# Perché: lo strumento che controlla lo schermo non tiene Ctrl insieme alla rotellina, e la
# rotellina va dove sta il mouse. Qui il messaggio (WM_MOUSEWHEEL, come lo manda Windows) arriva
# alla finestra di [z]f di quel PID senza muovere mouse né tastiera: funziona anche se la finestra
# non è davanti. Il PID si legge con tasklist prima e dopo prove\dev.bat (vedi /prova).
import ctypes, sys
from ctypes import wintypes as W

WM_MOUSEWHEEL, MK_CONTROL, WHEEL_DELTA = 0x020A, 0x0008, 120
sys.stdout.reconfigure(encoding="utf-8")   # "giù" anche nella console di Bash

if len(sys.argv) < 3:
    sys.exit("uso: rotella.py <pid> <scatti> [ctrl|semplice] [x y]")
pid, scatti = int(sys.argv[1]), int(sys.argv[2])
resto = sys.argv[3:]
ctrl = True
if resto and resto[0] in ("ctrl", "semplice"):
    ctrl = resto.pop(0) == "ctrl"
punto = (int(resto[0]), int(resto[1])) if len(resto) >= 2 else None

u = ctypes.windll.user32
trovate = []

@ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
def cerca(h, _):
    p = W.DWORD()
    u.GetWindowThreadProcessId(h, ctypes.byref(p))
    nome = ctypes.create_unicode_buffer(16)
    u.GetClassNameW(h, nome, 16)
    if p.value == pid and nome.value in ("z-f", "z-f-dev"):   # la finestra principale (z-f-dev nella build di prova), non la vista figlia
        trovate.append(h)
    return True

u.EnumWindows(cerca, 0)
if not trovate:
    sys.exit("nessuna finestra di [z]f con PID %d" % pid)
h = trovate[0]

if punto:   # dentro la finestra → schermo: WM_MOUSEWHEEL porta coordinate dello schermo
    pt = W.POINT(*punto)
    u.ClientToScreen(h, ctypes.byref(pt))
    x, y = pt.x, pt.y
else:
    r = W.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    x, y = (r.left + r.right) // 2, (r.top + r.bottom) // 2

delta = WHEEL_DELTA if scatti > 0 else -WHEEL_DELTA
wp = ((delta & 0xFFFF) << 16) | (MK_CONTROL if ctrl else 0)
lp = ((y & 0xFFFF) << 16) | (x & 0xFFFF)
for _ in range(abs(scatti)):
    u.PostMessageW(h, WM_MOUSEWHEEL, wp, lp)
print("%d scatti %s%s a 0x%X in (%d, %d)" % (abs(scatti), "su" if scatti > 0 else "giù",
      " con Ctrl" if ctrl else "", h, x, y))
