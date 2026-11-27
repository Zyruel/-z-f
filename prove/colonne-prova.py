# [z]f — cartella di prova per le colonne e il filtro per colonna: file finti con estensioni, date
# di modifica e di creazione, dimensioni e attributi diversi, più qualche cartella.
# Uso:  python -I prove\colonne-prova.py <cartella>
# Le date coprono tutti i valori del filtro: Data futura, Oggi, Ieri, Questa settimana, Settimana
# scorsa, Questo mese, Mese scorso, Quest'anno e alcuni anni prima. Le dimensioni coprono le fasce
# (vuoti, fino a 16 KB, 1 MB, 128 MB, 1 GB; niente oltre, per non riempire il disco: i file grandi
# sono sparsi, quindi non occupano spazio).
import os, sys, ctypes, datetime, random
from ctypes import wintypes
sys.stdout.reconfigure(encoding="utf-8")   # la console di Windows non è UTF-8, e -I ignora PYTHONUTF8

root = sys.argv[1]
os.makedirs(root, exist_ok=True)
rnd = random.Random(7)

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.CreateFileW.restype = wintypes.HANDLE
k32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
                            wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
k32.SetFileTime.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
k32.CloseHandle.argtypes = [wintypes.HANDLE]
k32.SetFileAttributesW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD]
k32.DeviceIoControl.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPVOID, wintypes.DWORD,
                                wintypes.LPVOID, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID]

def filetime(dt):
    """datetime locale → FILETIME (100 ns dal 1601, UTC)"""
    ts = dt.timestamp()
    return ctypes.c_ulonglong(int((ts + 11644473600) * 10_000_000))

def set_times(path, created, modified, folder=False):
    h = k32.CreateFileW(path, 0x100, 7, None, 3, 0x02000000 if folder else 0, None)  # FILE_WRITE_ATTRIBUTES, OPEN_EXISTING
    if h in (None, wintypes.HANDLE(-1).value):
        raise OSError(ctypes.get_last_error(), path)
    c, m = filetime(created), filetime(modified)
    k32.SetFileTime(h, ctypes.byref(c), None, ctypes.byref(m))
    k32.CloseHandle(h)

def sparse(path, size):
    """File della dimensione data senza occupare spazio (FSCTL_SET_SPARSE)"""
    with open(path, "wb") as f:
        pass
    h = k32.CreateFileW(path, 0xC0000000, 7, None, 3, 0, None)
    n = wintypes.DWORD()
    k32.DeviceIoControl(h, 0x900C4, None, 0, None, 0, ctypes.byref(n), None)
    k32.CloseHandle(h)
    with open(path, "r+b") as f:
        f.truncate(size)

now = datetime.datetime.now()
today = now.replace(hour=10, minute=0, second=0, microsecond=0)
monday = today - datetime.timedelta(days=today.weekday())
dates = {
    "futura": today + datetime.timedelta(days=40),
    "oggi": today,
    "ieri": today - datetime.timedelta(days=1),
    "settimana": monday,
    "settimana-scorsa": monday - datetime.timedelta(days=3),
    "mese": today.replace(day=1),
    "mese-scorso": (today.replace(day=1) - datetime.timedelta(days=10)),
    "anno": today.replace(month=1, day=2),
    "2024": datetime.datetime(2024, 6, 1, 12),
    "2019": datetime.datetime(2019, 3, 3, 12),
    "1975": datetime.datetime(1975, 5, 5, 12),
}
sizes = [0, 900, 15_000, 400_000, 30_000_000, 700_000_000, 2_000_000_000]
exts = ["jpg", "png", "txt", "pdf", "docx", "zip", "log", "", "tar.gz", "JPG"]
FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE = 1, 2, 4, 0x20
attrs = [0, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN,
         FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN]

n = 0
keys = list(dates)
for i in range(120):
    ext = exts[i % len(exts)]
    mod = keys[i % len(keys)]
    cre = keys[(i * 3 + 1) % len(keys)]
    size = sizes[(i * 5) % len(sizes)]
    name = f"file {i:03d} {mod}" + (f".{ext}" if ext else "")
    p = os.path.join(root, name)
    if os.path.exists(p):
        k32.SetFileAttributesW(p, 0x80)
        os.remove(p)
    if size > 1_000_000:
        sparse(p, size)
    else:
        with open(p, "wb") as f:
            f.write(b"x" * size)
    set_times(p, dates[cre], dates[mod])
    a = attrs[i % len(attrs)]
    k32.SetFileAttributesW(p, a if a else 0x80)  # 0x80 = FILE_ATTRIBUTE_NORMAL
    n += 1

for name in ["cartella vuota", "cartella con file", ".nascosta"]:
    d = os.path.join(root, name)
    os.makedirs(d, exist_ok=True)
    if name == "cartella con file":
        with open(os.path.join(d, "dentro.txt"), "wb") as f:
            f.write(b"y" * 50_000)
    set_times(d, dates["2019"], dates["ieri"], folder=True)
    if name.startswith("."):
        k32.SetFileAttributesW(d, 0x10 | FILE_ATTRIBUTE_HIDDEN)

print(f"{n} file e 3 cartelle in {root}")
