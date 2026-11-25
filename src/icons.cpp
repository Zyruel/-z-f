// [z]f — icone della shell caricate su un thread dedicato e messe in un atlante BGRA.
//
// Un thread dedicato chiede le icone alla shell (SHGetFileInfo) e le rasterizza.
// Quasi tutte si risolvono per estensione (SHGFI_USEFILEATTRIBUTES: il disco non
// viene toccato e la shell le tiene già in cache). Solo i tipi con un'icona propria
// per ogni file (exe, lnk, ico, url, ...) e le unità usano il percorso vero.
// Il thread UI non aspetta mai: finché l'icona non c'è, disegna il segnaposto.
// Due code e due thread: le icone per estensione (veloci, in cache nella shell) e quelle che
// leggono il file o l'unità (.lnk, .exe, unità: lente su una condivisione lenta o scollegata).
// Così un collegamento che non risponde non ferma le icone di tutto il resto. Il secondo thread
// parte alla prima richiesta che gli spetta.
// Le icone stanno in un atlante BGRA sulla GPU; nello shader le UV con x >= 1
// indicano l'atlante delle icone invece di quello dei glifi.

struct IconReq {
    std::wstring key;   // chiave di cache: ".txt", "<dir>", oppure percorso completo
    std::wstring path;  // nome finto (per estensione) o percorso reale
    DWORD attr;
    bool byAttr;        // SHGFI_USEFILEATTRIBUTES
    bool linkOverlay;
    int size;
    uint32_t gen;
};

struct IconRes {
    std::wstring key;
    std::vector<uint32_t> px; // size*size, BGRA non premoltiplicato
    int size;
    uint32_t gen;
    bool ok;
    double ms;
};

struct IconEntry {
    uint8_t state = 0; // 0 = da chiedere, 1 = in coda, 2 = pronta, 3 = fallita
    int slot = -1;
    double readyAt = 0; // per la dissolvenza d'ingresso
};

enum { IQ_EXT = 0, IQ_FILE = 1, IQ_COUNT = 2 }; // per estensione; dal file o dall'unità

static struct IconSys {
    std::unordered_map<std::wstring, IconEntry> cache; // solo thread UI
    CRITICAL_SECTION lock;
    std::vector<IconReq> queue[IQ_COUNT]; // protetto da lock; il thread prende dal fondo (le più recenti = visibili ora)
    std::vector<IconRes> results; // protetto da lock
    HANDLE wake[IQ_COUNT] = {}, thread[IQ_COUNT] = {};
    HANDLE shellReady = nullptr;  // il thread IQ_EXT ha fatto la prima SHGetFileInfo
    uint32_t gen = 1;
    int size = 16;
    int nextSlot = 0;
    // statistiche
    int loaded = 0, failed = 0;
    double totalMs = 0, firstAt = 0;
} I;

static const int ICON_QUEUE_MAX = 256;

static void iconRaster(HDC dc, uint32_t* bits, int S, HICON h, std::vector<uint32_t>& out) {
    // Si disegna l'icona due volte, su nero e su bianco: dalla differenza si ricava l'alfa.
    // Funziona per qualsiasi icona (con alfa, con maschera, vecchie a 4 bit).
    std::vector<uint32_t> black((size_t)S * S);
    for (int pass = 0; pass < 2; pass++) {
        uint32_t fill = pass ? 0x00FFFFFFu : 0;
        for (int i = 0; i < S * S; i++) bits[i] = fill;
        GdiFlush();
        DrawIconEx(dc, 0, 0, h, S, S, 0, nullptr, DI_NORMAL);
        GdiFlush();
        if (!pass) memcpy(black.data(), bits, (size_t)S * S * 4);
    }
    out.resize((size_t)S * S);
    for (int i = 0; i < S * S; i++) {
        uint32_t b = black[i], w = bits[i];
        int bg = (b >> 8) & 0xFF, wg = (w >> 8) & 0xFF;
        int a = 255 - (wg - bg);
        if (a <= 0) { out[i] = 0; continue; }
        if (a > 255) a = 255;
        int r = std::min(255, (int)((b >> 16) & 0xFF) * 255 / a);
        int g = std::min(255, bg * 255 / a);
        int bl = std::min(255, (int)(b & 0xFF) * 255 / a);
        out[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
    }
}

static DWORD WINAPI iconThread(void* p) {
    int q = (int)(INT_PTR)p;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); // richiesto da SHGetFileInfo
    // La prima SHGetFileInfo prepara le icone di sistema della shell; due thread che la fanno
    // insieme falliscono (provato: "<dir>" e altre restavano col segnaposto). La fa il primo
    // thread, il secondo aspetta.
    if (q == IQ_EXT) {
        SHFILEINFOW fi = {};
        SHGetFileInfoW(L"file", FILE_ATTRIBUTE_NORMAL, &fi, sizeof(fi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
        SetEvent(I.shellReady);
    } else {
        WaitForSingleObject(I.shellReady, INFINITE);
    }
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = nullptr;
    uint32_t* bits = nullptr;
    int bmpSize = 0;
    // Un thread STA deve smistare i messaggi: la shell può creare finestre nascoste su questo
    // thread (estensioni caricate leggendo .lnk e simili), e chi manda loro un messaggio
    // resterebbe bloccato finché non rispondiamo.
    auto pump = []() {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
    };
    for (;;) {
        DWORD w = MsgWaitForMultipleObjectsEx(1, &I.wake[q], INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        pump();
        if (w != WAIT_OBJECT_0) continue; // solo messaggi
        for (;;) {
            pump();
            IconReq r;
            EnterCriticalSection(&I.lock);
            bool have = !I.queue[q].empty();
            if (have) { r = std::move(I.queue[q].back()); I.queue[q].pop_back(); }
            LeaveCriticalSection(&I.lock);
            if (!have) break;

            double t0 = nowMs();
            IconRes res;
            res.key = r.key;
            res.size = r.size;
            res.gen = r.gen;
            res.ok = false;
            if (bmpSize != r.size) {
                if (bmp) DeleteObject(bmp);
                BITMAPINFO bi = {};
                bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
                bi.bmiHeader.biWidth = r.size;
                bi.bmiHeader.biHeight = -r.size;
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;
                bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
                SelectObject(dc, bmp);
                bmpSize = r.size;
            }
            UINT flags = SHGFI_ICON | (r.size <= GetSystemMetrics(SM_CXSMICON) ? SHGFI_SMALLICON : SHGFI_LARGEICON);
            if (r.byAttr) flags |= SHGFI_USEFILEATTRIBUTES;
            if (r.linkOverlay) flags |= SHGFI_LINKOVERLAY;
            SHFILEINFOW fi = {};
            // Cartelle virtuali ("::{CLSID}", Questo PC): SHGetFileInfo non le accetta come
            // percorso, vuole il loro PIDL
            PIDLIST_ABSOLUTE pidl = nullptr;
            if (!r.byAttr && r.path.compare(0, 2, L"::") == 0 &&
                SUCCEEDED(SHParseDisplayName(r.path.c_str(), nullptr, &pidl, 0, nullptr)))
                flags |= SHGFI_PIDL;
            DWORD_PTR got = SHGetFileInfoW(pidl ? (LPCWSTR)pidl : r.path.c_str(), r.attr, &fi, sizeof(fi), flags);
            CoTaskMemFree(pidl);
            if (got && fi.hIcon) {
                iconRaster(dc, bits, r.size, fi.hIcon, res.px);
                DestroyIcon(fi.hIcon);
                res.ok = true;
            }
            res.ms = nowMs() - t0;

            EnterCriticalSection(&I.lock);
            bool wasEmpty = I.results.empty();
            I.results.push_back(std::move(res));
            LeaveCriticalSection(&I.lock);
            if (wasEmpty) PostMessageW(A.hwnd, WM_APP_ICON, 0, 0); // un messaggio per lotto, non per icona
        }
    }
    return 0;
}

static void iconThreadStart(int q) {
    if (I.thread[q]) return;
    I.wake[q] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    I.thread[q] = CreateThread(nullptr, 0, iconThread, (void*)(INT_PTR)q, 0, nullptr);
}

static void iconInit() {
    InitializeCriticalSection(&I.lock);
    I.shellReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    iconThreadStart(IQ_EXT); // IQ_FILE alla prima richiesta (iconForKey)
}

// L'atlante non vale più (altra dimensione, o device nuovo dopo una GPU persa): tutte le icone si
// richiedono al primo disegno
static void iconsForget() {
    I.gen++;           // i risultati in arrivo per l'atlante vecchio vengono scartati
    I.cache.clear();
    I.nextSlot = 0;
    EnterCriticalSection(&I.lock);
    for (auto& q : I.queue) q.clear();
    LeaveCriticalSection(&I.lock);
}

static void iconSetSize(int size) {
    if (size == I.size && !I.cache.empty()) return;
    I.size = std::max(8, std::min(size, 64));
    iconsForget();
}

static void iconUpload(int slot, const std::vector<uint32_t>& px) {
    if (!G.iconAtlas || slot < 0) return;
    int per = ICON_ATLAS / I.size;
    int x = (slot % per) * I.size, y = (slot / per) * I.size;
    D3D11_BOX box = { (UINT)x, (UINT)y, 0, (UINT)(x + I.size), (UINT)(y + I.size), 1 };
    G.ctx->UpdateSubresource(G.iconAtlas, 0, &box, px.data(), (UINT)I.size * 4, 0);
}

// Righe dell'atlante occupate (per copiarlo sul nuovo device nel passaggio WARP → GPU)
static int iconRowsUsed() {
    int per = ICON_ATLAS / I.size;
    return ((I.nextSlot + per - 1) / per) * I.size;
}

static void onIconResults() {
    std::vector<IconRes> res;
    EnterCriticalSection(&I.lock);
    res.swap(I.results);
    LeaveCriticalSection(&I.lock);
    int per = ICON_ATLAS / I.size;
    for (IconRes& r : res) {
        if (r.gen != I.gen || r.size != I.size) continue;
        auto it = I.cache.find(r.key);
        if (it == I.cache.end()) continue;
        IconEntry& e = it->second;
        I.totalMs += r.ms;
        if (!r.ok) { e.state = 3; I.failed++; continue; }
        if (I.nextSlot >= per * per) { // atlante pieno: si ricomincia (raro: 4096 icone a 16 px)
            for (auto& kv : I.cache) if (kv.second.state == 2) { kv.second.state = 0; kv.second.slot = -1; }
            I.nextSlot = 0;
        }
        e.slot = I.nextSlot++;
        e.state = 2;
        e.readyAt = nowMs();
        iconUpload(e.slot, r.px);
        I.loaded++;
        if (!I.firstAt) I.firstAt = processAgeMs();
    }
    A.dirty = true;
}

// Tipi con un'icona propria per ogni file (la shell la legge dal file)
static bool iconPerFile(const wchar_t* ext) {
    static const wchar_t* k[] = { L".exe", L".lnk", L".ico", L".url", L".cur", L".ani", L".cpl", L".scr", L".msc",
                                  L".appref-ms", nullptr };
    for (int i = 0; k[i]; i++) if (!_wcsicmp(ext, k[i])) return true;
    return false;
}

// Da dove viene l'icona di una voce, la stessa in elenco (qui), in griglia (thIconKeyFor) e
// nell'anteprima (pvBigIcon): ICON_REAL dal percorso vero (le unità e i tipi con un'icona per file),
// ICON_DIR quella di cartella, ICON_EXT quella del tipo. Dentro uno zip il file non c'è sul disco:
// per estensione.
enum { ICON_EXT, ICON_DIR, ICON_REAL };
static int iconSource(const wchar_t* name, DWORD attr, bool drive, bool inZip) {
    if (drive) return ICON_REAL;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) return ICON_DIR;
    const wchar_t* dot = wcsrchr(name, L'.');
    return dot && iconPerFile(dot) && !inZip ? ICON_REAL : ICON_EXT;
}

// Un'unità di Questo PC, con l'icona sua (in background: può toccare l'unità). Un percorso di rete
// lì è una cartella, come in Explorer: niente rete per chiederne l'icona.
static bool iconDrive(const Listing* l, const Entry& e) { return l->isDrives && e.size != DRIVE_NETPLACE; }

static const IconEntry* iconForKey(const std::wstring& key, const std::wstring& path, DWORD attr, bool byAttr, bool overlay);

// Icona per una voce della lista (accodando la richiesta se manca), o nullptr
static const IconEntry* iconFor(const Listing* l, const Entry& e) {
    const wchar_t* name = l->name(e);
    std::wstring key, path;
    bool byAttr = true, overlay = false;
    DWORD attr = FILE_ATTRIBUTE_NORMAL;
    const wchar_t* dot = wcsrchr(name, L'.');
    int src = iconSource(name, e.attr, iconDrive(l, e), l->zipLen != 0);
    if (src == ICON_REAL) { // un'unità: "C:\" (la cartella di Questo PC è vuota)
        key = path = joinPath(l->path, name);
        byAttr = false;
        overlay = dot && (!_wcsicmp(dot, L".lnk") || !_wcsicmp(dot, L".url"));
    } else if (src == ICON_DIR) {
        key = L"<dir>";
        path = L"cartella";
        attr = FILE_ATTRIBUTE_DIRECTORY;
    } else {
        key = dot ? dot : L".";
        for (auto& c : key) c = (wchar_t)towlower(c);
        path = L"file" + key;
    }
    return iconForKey(key, path, attr, byAttr, overlay);
}

// Ritorna l'icona per una chiave di cache (accodando la richiesta se manca), o nullptr
static const IconEntry* iconForKey(const std::wstring& key, const std::wstring& path, DWORD attr, bool byAttr, bool overlay) {
    IconEntry& ce = I.cache[key];
    if (ce.state == 2) return &ce;
    if (ce.state == 0) {
        ce.state = 1;
        IconReq r{ key, path, attr, byAttr, overlay, I.size, I.gen };
        int q = byAttr ? IQ_EXT : IQ_FILE;
        iconThreadStart(q);
        std::wstring dropped;
        EnterCriticalSection(&I.lock);
        std::vector<IconReq>& qu = I.queue[q];
        qu.push_back(std::move(r));
        if ((int)qu.size() > ICON_QUEUE_MAX) { // scorrimento veloce: si scartano le richieste più vecchie
            dropped = qu.front().key;
            qu.erase(qu.begin());
        }
        LeaveCriticalSection(&I.lock);
        if (I.wake[q]) SetEvent(I.wake[q]);
        if (!dropped.empty()) {
            auto it = I.cache.find(dropped);
            if (it != I.cache.end() && it->second.state == 1) it->second.state = 0; // verrà richiesta se torna visibile
        }
    }
    return nullptr;
}

static void drawIcon(float x, float y, const IconEntry& ie, uint32_t tint) {
    int per = ICON_ATLAS / I.size;
    float k = 1.0f / ICON_ATLAS;
    float u = (float)((ie.slot % per) * I.size), v = (float)((ie.slot / per) * I.size);
    // u + 1: nello shader indica l'atlante delle icone
    quad(x, y, x + I.size, y + I.size, 1.0f + u * k, v * k, 1.0f + (u + I.size) * k, (v + I.size) * k, tint);
}
