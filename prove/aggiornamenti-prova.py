# [z]f — server locale che fa da GitHub per provare l'aggiornamento (update.cpp).
#
#   python prove\aggiornamenti-prova.py <cartella> [porta]
#
# Nella cartella: z-f-<versione>.exe e la sua firma z-f-<versione>.exe.sig (build\firma.exe).
# Lo script scrive releases.json (con dimensione e impronta SHA-256 veri, come l'API di GitHub)
# e serve la cartella su http://127.0.0.1:<porta> (8766 se manca). Poi:
#
#   <copia della versione vecchia>\z-f.exe --new-window --aggiorna-da=http://127.0.0.1:8766
#
# Il controllo parte dopo 2 s; nel registro accanto all'exe compaiono "aggiornamento: controllo" e
# "aggiornamento: download" con lo stato (3 pronto, 4 a mano, 5 errore). Mai sulla copia di
# build\z-f.exe in uso: l'exe viene sostituito.
# Un server minimo e non http.server: su questo PC quello di Python mandava 3 KB al secondo.
import hashlib, json, os, re, socket, sys, threading

root = sys.argv[1]
port = int(sys.argv[2]) if len(sys.argv) > 2 else 8766
base = "http://127.0.0.1:%d/" % port
releases = []
for name in sorted(os.listdir(root)):
    m = re.fullmatch(r"z-f-(.+)\.exe", name)
    if not m:
        continue
    data = open(os.path.join(root, name), "rb").read()
    ver = m.group(1)
    assets = [{"name": name, "size": len(data), "digest": "sha256:" + hashlib.sha256(data).hexdigest(),
               "browser_download_url": base + name}]
    if os.path.exists(os.path.join(root, name + ".sig")):
        assets.append({"name": name + ".sig", "size": 64, "digest": None, "browser_download_url": base + name + ".sig"})
    releases.append({"tag_name": "v" + ver, "prerelease": "-" in ver, "draft": False,
                     "html_url": base + "pagina-" + ver, "assets": assets})
json.dump(releases, open(os.path.join(root, "releases.json"), "w"), indent=1)
print("releases.json:", [r["tag_name"] for r in releases], flush=True)

def handle(c):
    try:
        req = b""
        while b"\r\n\r\n" not in req:
            d = c.recv(4096)
            if not d:
                return
            req += d
        path = os.path.basename(req.split(b" ")[1].decode().split("?")[0])
        full = os.path.join(root, path)
        if os.path.isfile(full):
            data = open(full, "rb").read()
            c.sendall(b"HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % len(data) + data)
        else:
            c.sendall(b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
        print("GET", path, flush=True)
    finally:
        c.close()

s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", port))
s.listen(8)
print("pronto su", base, flush=True)
while True:
    conn, _ = s.accept()
    threading.Thread(target=handle, args=(conn,), daemon=True).start()
