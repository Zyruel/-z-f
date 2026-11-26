// [z]f — vista a griglia: miniature e vista ricordata per cartella (viste.txt).
//
// Da dove vengono le miniature (misure in ARCHITETTURA.md):
//  1. la cache delle miniature di Windows (IThumbnailCache con WTS_INCACHEONLY): 0,3 ms se c'è.
//     L'elemento della shell si crea dalla cartella già aperta (IShellFolder, una volta per thread):
//     dal percorso intero costerebbe 2,3 ms, perché la shell lo risolve livello per livello;
//  2. se non c'è, le immagini con WIC: pvImage dell'anteprima (JPEG ridotti già in decodifica, EXIF);
//  3. video e documenti con la shell (IShellItemImageFactory, SIIGBF_THUMBNAILONLY), che usa i suoi
//     gestori, quasi sempre fuori dal nostro processo: lenta (100–400 ms), quindi su un thread suo;
//  4. altrimenti l'icona grande della shell (pvBigIcon), larga metà del riquadro.
// Le miniature fatte con WIC si chiedono poi alla shell, a riposo e a bassa priorità: così entrano
// nella cache di Windows e la volta dopo costano 0,3 ms (anche in Explorer). I file solo online
// (OneDrive) non si leggono mai, per non scaricarli: per loro solo la cache di Windows.
//
// Thread: uno per la cache di Windows e le icone, metà dei core per WIC, uno per la shell, due per
// le cartelle di rete e le icone delle unità di rete e ottiche (una lettura bloccata lì non ferma le
// cartelle locali). Le code le possiede il
// thread UI e le rifà quando cambiano le voci volute: prima le visibili, poi una schermata nella
// direzione dello scorrimento, poi una dall'altra parte. Una richiesta che nessuno vuole più non
// parte; se la cartella cambia, quelle in corso smettono prima (contatore di generazione, come
// nell'anteprima). WIC e shell si caricano solo alla prima miniatura, sui thread.
//
// Memoria: un atlante di kThAtlas² sulla GPU (64 MB, creato al primo uso della griglia e liberato
// quando nessuna scheda è più in griglia), con posti grandi quanto il riquadro; in RAM le copie dei
// pixel, fino a kThRamMax. Se ne vanno prima quelle usate meno di recente. Il thread UI carica sulla
// GPU al massimo kThUploadMs per frame, il resto al frame dopo. Lo spazio di una voce è fisso: una
// miniatura che arriva non sposta niente, entra con la dissolvenza delle icone.

static const int kThAtlas = 4096;                 // lato dell'atlante (BGRA: 64 MB)
static const size_t kThRamMax = (size_t)96 << 20; // copie dei pixel in RAM
static const double kThUploadMs = 0.25;           // caricamenti sulla GPU per frame, al massimo
static const size_t kThMaxEntries = 20000;        // voci ricordate (le vuote si tolgono oltre)
static const size_t kThWarmMax = 1000;            // miniature da far entrare nella cache di Windows
static const int kViewsMax = 500;                 // cartelle ricordate in viste.txt

// GUID scritti qui, come in preview.cpp: le librerie che li definiscono cambiano tra MSVC e MinGW
static const GUID kClsidThumbCache = { 0x50ef4544, 0xac9f, 0x4a8e, { 0xb2, 0x1b, 0x8a, 0x26, 0x18, 0x0d, 0xb1, 0x3f } };
static const GUID kIidThumbCache = { 0xf676c15d, 0x596a, 0x4ce2, { 0x82, 0x34, 0x33, 0x99, 0x6f, 0x44, 0x5d, 0xb1 } };
static const GUID kIidImageFactory = { 0xbcc18b79, 0xba16, 0x442f, { 0x80, 0xc4, 0x8a, 0x59, 0xc3, 0x0c, 0x46, 0x3b } };
static const GUID kIidShellItem = { 0x43826d1e, 0xe718, 0x42ee, { 0xbc, 0x55, 0xa1, 0xe2, 0x61, 0xc3, 0x7b, 0xfe } };
static const GUID kIidShellFolder = { 0x000214e6, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

enum { TK_NONE, TK_IMAGE, TK_SHELL };              // che miniatura può avere un file
enum { THS_NEW, THS_READY, THS_NONE };                // stato: da fare, pronta, niente (si usa l'icona)
enum : uint8_t { SRC_CACHE, SRC_WIC, SRC_SHELL, SRC_ICON };  // fonte (anche indice delle statistiche in F12)
enum { POOL_FAST, POOL_WIC, POOL_SHELL, POOL_NET, POOL_N };
enum { TR_OK, TR_MISS, TR_FAIL, TR_DROP };         // DROP: superata, non provata

// Tipi che la shell sa rendere in miniatura, se c'è il gestore (video sempre, documenti a volte)
static const wchar_t* const kShellThumbExt[] = { L".mp4", L".m4v", L".mkv", L".avi", L".mov", L".wmv", L".webm",
    L".mpg", L".mpeg", L".3gp", L".mts", L".m2ts", L".flv", L".pdf", L".docx", L".xlsx", L".pptx", L".odt", L".psd",
    L".svg", L".epub", L".3mf", L".stl", L".dng", L".cr2", L".cr3", L".nef", L".arw", nullptr };

struct ThumbEntry {
    uint8_t state = THS_NEW;
    uint8_t src = SRC_CACHE;     // prossima fonte da provare
    uint8_t kind = TK_NONE;
    bool inZip = false;          // dentro uno zip: si comincia da WIC (thRestart)
    bool online = false;         // solo online: niente letture
    bool queued = false;         // in una coda o su un thread
    bool shown = false;          // già vista: tornando sulla GPU non rifà la dissolvenza
    int slot = -1;               // posto nell'atlante
    int w = 0, h = 0;
    double readyAt = 0;          // caricata sulla GPU: inizio della dissolvenza
    uint64_t lastUse = 0;        // frame dell'ultimo disegno
    uint64_t mtime = 0, size = 0;// il file è cambiato: si rifà
    std::vector<uint32_t> px;    // copia in RAM, BGRA non premoltiplicato
};

struct ThumbReq {
    std::wstring key, dir, name; // percorso = joinPath(dir, name) ("C:\" per un'unità, con dir vuota)
    uint32_t attr = 0;
    int box = 0, src = SRC_CACHE;
    bool icon = false, image = false, local = true;
    bool drive = false;          // un'unità di Questo PC (iconDrive: non un percorso di rete)
    bool slowDrive = false;      // l'unità è di rete o ottica (il tipo letto con la lista, non da A.drives)
    bool inZip = false;          // la voce è dentro uno zip: l'icona si sceglie come la chiave (thIconKeyFor)
    uint64_t gen = 0, sizeGen = 0;
};

struct ThumbRes {
    std::wstring key, dir, name;
    uint32_t attr = 0;
    bool icon = false, local = true;
    int src = SRC_CACHE, box = 0, outcome = TR_FAIL;
    uint64_t gen = 0, sizeGen = 0;
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    double ms = 0;
};

struct ThumbWarm { std::wstring dir, name; int box; };
struct ThumbPool { std::deque<ThumbReq> q; HANDLE sem = nullptr; int threads = 0; };

// Generazione più recente (cartella): un thread superato lo vede e smette prima
static volatile LONG64 g_thLatest = 0;
static bool thSuperseded(uint64_t gen) { return (uint64_t)InterlockedCompareExchange64(&g_thLatest, 0, 0) != gen; }

static struct Thumbs {
    std::unordered_map<std::wstring, ThumbEntry> map;   // miniature, per percorso (solo thread UI)
    std::unordered_map<std::wstring, ThumbEntry> icons; // icone grandi, per tipo (solo thread UI)
    CRITICAL_SECTION lock;
    bool started = false;
    ThumbPool pool[POOL_N];          // protetto da lock
    std::vector<ThumbRes> results;   // protetto da lock
    std::deque<ThumbWarm> warm;      // protetto da lock
    uint64_t gen = 1, sizeGen = 1;
    const Listing* forList = nullptr;
    uint64_t forListGen = 0;
    std::wstring forPath = L"\x01";  // cartella della prima schermata (una ricarica non la rimisura)
    int forBox = 0;
    bool active = false, remote = false, needSync = false;
    int first = -1, last = -1, dir = 1;
    float lastScroll = 0;
    uint64_t frame = 0;
    int per = 0;                     // posti per riga dell'atlante
    std::vector<ThumbEntry*> owner;  // posto → voce
    std::vector<int> freeSlots;
    size_t ram = 0;
    double frameUpMs = 0;
    double fsStart = 0;
    bool fsDone = true;
} TH;

// --- lavoro sui thread ----------------------------------------------------------------------
// Stato di un thread: la cartella aperta nella shell (gli elementi si creano da lì) e la cache
struct ThCtx {
    std::wstring dir = L"\x01";
    IShellFolder* sf = nullptr;
    PIDLIST_ABSOLUTE pidl = nullptr;
    IThumbnailCache* tc = nullptr;
};

static IShellItem* thItem(ThCtx& c, const std::wstring& dir, const std::wstring& name) {
    if (c.dir != dir) {
        if (c.sf) { c.sf->Release(); c.sf = nullptr; }
        if (c.pidl) { CoTaskMemFree(c.pidl); c.pidl = nullptr; }
        c.dir = dir;
        if (SUCCEEDED(SHParseDisplayName(dir.c_str(), nullptr, &c.pidl, 0, nullptr)) && c.pidl &&
            FAILED(SHBindToObject(nullptr, c.pidl, nullptr, kIidShellFolder, (void**)&c.sf)))
            c.sf = nullptr;
    }
    if (!c.sf) return nullptr;
    PIDLIST_RELATIVE child = nullptr;
    if (FAILED(c.sf->ParseDisplayName(nullptr, nullptr, (LPWSTR)name.c_str(), nullptr, &child, nullptr)) || !child) return nullptr;
    IShellItem* it = nullptr;
    if (FAILED(SHCreateItemWithParent(c.pidl, nullptr, (PCUITEMID_CHILD)child, kIidShellItem, (void**)&it))) it = nullptr;
    CoTaskMemFree(child);
    return it;
}

// Pixel di una HBITMAP della shell → BGRA non premoltiplicato. alpha: può avere un'alfa vera
// (premoltiplicata); se è tutta a zero il bitmap è opaco.
static bool thBitmap(HBITMAP hb, bool alpha, std::vector<uint32_t>& px, int& w, int& h) {
    BITMAP bm = {};
    if (!GetObjectW(hb, sizeof(bm), &bm) || bm.bmWidth <= 0 || !bm.bmHeight) return false;
    w = bm.bmWidth;
    h = abs(bm.bmHeight);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // dall'alto, qualunque sia il verso del bitmap
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    px.resize((size_t)w * h);
    HDC dc = GetDC(nullptr);
    int got = GetDIBits(dc, hb, 0, (UINT)h, px.data(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (got != h) return false;
    bool any = false;
    if (alpha)
        for (uint32_t p : px) if (p >> 24) { any = true; break; }
    if (any) pvUnpremultiply(px);
    else for (uint32_t& p : px) p |= 0xFF000000u;
    return true;
}

// Mai più grande del riquadro (la cache di Windows può dare la misura sopra)
static void thFit(ThumbRes& o, int box) {
    if (o.w <= box && o.h <= box) return;
    int fw, fh;
    pvFit(o.w, o.h, box, box, fw, fh);
    std::vector<uint32_t> d;
    pvScaleDown(o.px.data(), o.w, o.h, o.w, d, fw, fh);
    o.px.swap(d);
    o.w = fw; o.h = fh;
}

static int thFromCache(ThCtx& c, const ThumbReq& r, ThumbRes& o) {
    if (!c.tc && FAILED(CoCreateInstance(kClsidThumbCache, nullptr, CLSCTX_INPROC_SERVER, kIidThumbCache, (void**)&c.tc))) {
        c.tc = nullptr;
        return TR_MISS;
    }
    IShellItem* it = thItem(c, r.dir, r.name);
    if (!it) return TR_MISS;
    ISharedBitmap* sb = nullptr;
    WTS_CACHEFLAGS cf;
    WTS_THUMBNAILID id;
    HRESULT hr = c.tc->GetThumbnail(it, (UINT)r.box, WTS_INCACHEONLY, &sb, &cf, &id);
    it->Release();
    if (hr != S_OK || !sb) { if (sb) sb->Release(); return TR_MISS; }
    HBITMAP hb = nullptr;
    WTS_ALPHATYPE at = WTSAT_UNKNOWN;
    sb->GetFormat(&at);
    sb->GetSharedBitmap(&hb); // resta della cache: non si cancella
    bool ok = hb && thBitmap(hb, at == WTSAT_ARGB, o.px, o.w, o.h);
    sb->Release();
    if (!ok) return TR_MISS;
    // Più piccola del riquadro (in cache c'è una misura minore): per un'immagine WIC la fa nitida
    if (r.image && o.w < r.box && o.h < r.box) { o.px.clear(); return TR_MISS; }
    thFit(o, r.box);
    return TR_OK;
}

static int thFromWic(const ThumbReq& r, ThumbRes& o) {
    PvReq pr{ nullptr, r.gen, joinPath(r.dir, r.name.c_str()), r.attr, false, ICON_EXT, r.box, r.box, 0, thSuperseded };
    const wchar_t* ext = fileExt(pr.path);
    PvRes pres;
    if (!ext || !pvImage(pr, pres, ext)) return thSuperseded(r.gen) ? TR_DROP : TR_FAIL;
    o.px.swap(pres.px);
    o.w = pres.w;
    o.h = pres.h;
    return TR_OK;
}

// La shell calcola la miniatura (e la mette nella sua cache). out nullptr: solo per la cache
static int thFromShell(ThCtx& c, const std::wstring& dir, const std::wstring& name, int box, ThumbRes* o) {
    IShellItem* it = thItem(c, dir, name);
    if (!it) return TR_FAIL;
    IShellItemImageFactory* f = nullptr;
    HRESULT hr = it->QueryInterface(kIidImageFactory, (void**)&f);
    it->Release();
    if (FAILED(hr) || !f) return TR_FAIL;
    HBITMAP hb = nullptr;
    hr = f->GetImage(SIZE{ box, box }, SIIGBF_THUMBNAILONLY, &hb);
    f->Release();
    bool ok = SUCCEEDED(hr) && hb;
    if (ok && o) {
        ok = thBitmap(hb, true, o->px, o->w, o->h);
        if (ok) thFit(*o, box);
    }
    if (hb) DeleteObject(hb);
    return ok ? TR_OK : TR_FAIL;
}

static int thIconWork(const ThumbReq& r, ThumbRes& o) {
    int src = iconSource(r.name.c_str(), r.attr, r.drive, r.inZip); // come la chiave (thIconKeyFor)
    PvReq pr{ nullptr, 0, joinPath(r.dir, r.name.c_str()), r.attr, r.drive, src, 0, 0, r.box, nullptr };
    PvRes pres;
    pvBigIcon(pr, pres);
    if (pres.px.empty()) return TR_FAIL;
    o.px.swap(pres.px);
    o.w = pres.w;
    o.h = pres.h;
    return TR_OK;
}

static DWORD WINAPI thThread(void* p) {
    int pool = (int)(INT_PTR)p;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); // shell e WIC
    ThCtx ctx;
    HANDLE sem = TH.pool[pool].sem;
    // Sotto la priorità normale: con tutti i core di WIC occupati, interfaccia e compositore passano prima
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    // Un thread STA deve smistare i messaggi (come quello delle icone): la shell può creare
    // finestre nascoste qui, e chi manda loro un messaggio resterebbe bloccato.
    auto pump = []() {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
    };
    for (;;) {
        DWORD w = MsgWaitForMultipleObjectsEx(1, &sem, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        pump();
        if (w != WAIT_OBJECT_0) continue;
        for (;;) {
            pump();
            ThumbReq r;
            ThumbWarm wr;
            bool have = false, warm = false;
            EnterCriticalSection(&TH.lock);
            std::deque<ThumbReq>& q = TH.pool[pool].q;
            if (!q.empty()) { r = std::move(q.front()); q.pop_front(); have = true; }
            else if (pool == POOL_SHELL && !TH.warm.empty()) { wr = std::move(TH.warm.front()); TH.warm.pop_front(); warm = true; }
            LeaveCriticalSection(&TH.lock);
            if (warm) { // a riposo: la miniatura entra nella cache di Windows, la volta dopo costa 0,3 ms
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
                thFromShell(ctx, wr.dir, wr.name, wr.box, nullptr);
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
                continue;
            }
            if (!have) break;
            ThumbRes o;
            o.key = r.key; o.icon = r.icon; o.src = r.src; o.box = r.box; o.gen = r.gen; o.sizeGen = r.sizeGen; o.attr = r.attr;
            o.local = r.local; o.dir = r.dir; o.name = r.name;
            double t0 = nowMs();
            if (r.icon) o.outcome = thIconWork(r, o);
            else if (thSuperseded(r.gen)) o.outcome = TR_DROP;
            else if (r.src == SRC_CACHE) o.outcome = thFromCache(ctx, r, o);
            else if (r.src == SRC_WIC) o.outcome = thFromWic(r, o);
            else o.outcome = thFromShell(ctx, r.dir, r.name, r.box, &o);
            o.ms = nowMs() - t0;
            EnterCriticalSection(&TH.lock);
            bool wasEmpty = TH.results.empty();
            TH.results.push_back(std::move(o));
            LeaveCriticalSection(&TH.lock);
            if (wasEmpty) PostMessageW(A.hwnd, WM_APP_THUMB, 0, 0); // un messaggio per lotto
        }
    }
    return 0;
}

// Thread alla prima miniatura, mai all'avvio. WIC: metà dei core fisici (da 2 a 8): misurato,
// la prima schermata di foto da 12 MP passa da 1080 ms con un thread a 294 con 4 e 175 con 8.
static void thStart() {
    TH.started = true;
    InitializeCriticalSection(&TH.lock);
    int cores = 0;
    DWORD len = 0;
    GetLogicalProcessorInformation(nullptr, &len);
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) + 1);
    if (len && GetLogicalProcessorInformation(info.data(), &len))
        for (DWORD i = 0; i < len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); i++)
            if (info[i].Relationship == RelationProcessorCore) cores++;
    int counts[POOL_N] = { 1, std::max(2, std::min(8, cores / 2)), 1, 2 };
    A.th.threads = 0;
    for (int p = 0; p < POOL_N; p++) {
        TH.pool[p].sem = CreateSemaphoreW(nullptr, 0, 0x3FFFFFFF, nullptr);
        for (int i = 0; i < counts[p]; i++) {
            HANDLE h = CreateThread(nullptr, 0, thThread, (void*)(INT_PTR)p, 0, nullptr);
            if (h) { CloseHandle(h); TH.pool[p].threads++; A.th.threads++; }
        }
    }
}

// --- stato sul thread dell'interfaccia ------------------------------------------------------
static int thKind(const Entry& e) {
    if (A.cur->isDrives || (e.attr & FILE_ATTRIBUTE_DIRECTORY)) return TK_NONE;
    const wchar_t* dot = wcsrchr(A.cur->name(e), L'.');
    if (!dot) return TK_NONE;
    if (extIn(dot, kImgExt)) return TK_IMAGE;
    if (A.cur->zipLen) return TK_NONE; // dentro uno zip la shell non vede il file: solo le immagini (WIC dalla memoria)
    if (extIn(dot, kShellThumbExt)) return TK_SHELL;
    return TK_NONE;
}

// La voce si rifà da capo, dalla prima fonte. Dentro uno zip è WIC: la cache di Windows non ha quei
// file e chiederla passerebbe dalla cartella della shell dentro lo zip (zipfldr, lenta).
static void thRestart(ThumbEntry& t) {
    t.state = THS_NEW;
    t.src = t.inZip ? SRC_WIC : SRC_CACHE;
}

static void thForget(ThumbEntry& t) { // pixel e posto se ne vanno; la voce si rifà se serve
    if (t.slot >= 0) { TH.owner[(size_t)t.slot] = nullptr; TH.freeSlots.push_back(t.slot); t.slot = -1; }
    TH.ram -= t.px.size() * 4;
    std::vector<uint32_t>().swap(t.px);
    thRestart(t);
}

// Miniatura della voce e (creata se manca); se il file è cambiato si rifà
static ThumbEntry& thEntry(const Entry& e) {
    std::wstring key = joinPath(A.cur->path, A.cur->name(e));
    auto it = TH.map.find(key);
    if (it == TH.map.end()) {
        it = TH.map.emplace(std::move(key), ThumbEntry()).first;
        ThumbEntry& t = it->second;
        t.kind = (uint8_t)thKind(e);
        t.inZip = A.cur->zipLen != 0;
        thRestart(t);
        t.online = onlineOnly(e.attr);
        t.mtime = e.mtime;
        t.size = e.size;
    } else if (it->second.mtime != e.mtime || it->second.size != e.size) {
        ThumbEntry& t = it->second;
        thForget(t);
        t.shown = false;
        t.online = onlineOnly(e.attr);
        t.mtime = e.mtime;
        t.size = e.size;
    }
    return it->second;
}

// Icona grande della voce: la stessa scelta dell'elenco (iconSource); per file i tipi con un'icona
// propria e le unità ("C:\", con dir vuota), per tipo tutte le altre
static std::wstring thIconKeyFor(const std::wstring& dir, const wchar_t* name, DWORD attr, bool drive, bool inZip, bool& perFile) {
    int src = iconSource(name, attr, drive, inZip);
    perFile = src == ICON_REAL;
    if (perFile) return joinPath(dir, name);
    if (src == ICON_DIR) return L"<dir>";
    const wchar_t* dot = wcsrchr(name, L'.');
    std::wstring k = dot ? dot : L".";
    for (auto& c : k) c = (wchar_t)towlower(c);
    return k;
}

static std::wstring thIconKey(const Entry& e, bool& perFile) {
    return thIconKeyFor(A.cur->path, A.cur->name(e), e.attr, iconDrive(A.cur, e), A.cur->zipLen != 0, perFile);
}

static ThumbEntry& thIcon(const Entry& e, std::wstring* keyOut = nullptr, bool* perFile = nullptr) {
    bool pf;
    std::wstring key = thIconKey(e, pf);
    if (perFile) *perFile = pf;
    if (keyOut) *keyOut = key;
    return TH.icons[key];
}


// --- atlante sulla GPU ----------------------------------------------------------------------
static void thAtlasForget() { // i posti non valgono più (texture rilasciata o device nuovo)
    for (ThumbEntry* t : TH.owner)
        if (t) {
            t->slot = -1;
            if (t->px.empty()) thRestart(*t);
        }
    TH.owner.clear();
    TH.freeSlots.clear();
    TH.per = 0;
    A.th.vramMB = 0;
}

static void thAtlasDrop() {
    thAtlasForget();
    safeRelease(G.thumbSrv);
    safeRelease(G.thumbTex);
}

static bool thAtlasEnsure() {
    if (G.thumbTex) return true;
    if (!G.dev || TH.forBox <= 0) return false;
    double t0 = nowMs(); // 64 MB sul thread UI (con WARP in RAM): si misura, F12 e registro
    struct Measure {
        double t0;
        ~Measure() {
            A.th.atlasMs = nowMs() - t0;
            if (A.th.atlasMs > ui::LOG_FRAME) hitchLog("atlante miniature lento", 0, A.th.atlasMs);
        }
    } measure{ t0 };
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = kThAtlas;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(G.dev->CreateTexture2D(&td, nullptr, &G.thumbTex))) { G.thumbTex = nullptr; return false; }
    if (FAILED(G.dev->CreateShaderResourceView(G.thumbTex, nullptr, &G.thumbSrv))) { safeRelease(G.thumbTex); return false; }
    TH.per = kThAtlas / TH.forBox;
    int n = TH.per * TH.per;
    TH.owner.assign((size_t)n, nullptr);
    TH.freeSlots.clear();
    for (int s = n - 1; s >= 0; s--) TH.freeSlots.push_back(s);
    A.th.vramMB = (double)kThAtlas * kThAtlas * 4 / 1048576.0;
    return true;
}

// Posto libero, o quello della voce usata meno di recente (non disegnata in questo frame)
static int thSlotAlloc() {
    if (!TH.freeSlots.empty()) { int s = TH.freeSlots.back(); TH.freeSlots.pop_back(); return s; }
    int best = -1;
    uint64_t bu = UINT64_MAX;
    for (size_t s = 0; s < TH.owner.size(); s++) {
        ThumbEntry* t = TH.owner[s];
        if (t && t->lastUse < TH.frame && t->lastUse < bu) { bu = t->lastUse; best = (int)s; }
    }
    if (best < 0) return -1;
    ThumbEntry* t = TH.owner[(size_t)best];
    t->slot = -1;
    if (t->px.empty()) thRestart(*t);
    TH.owner[(size_t)best] = nullptr;
    return best;
}

// Sulla GPU, entro il limite del frame (altrimenti al frame dopo)
static bool thUpload(ThumbEntry& t) {
    if (TH.frameUpMs >= kThUploadMs) { A.animActive = true; return false; }
    if (t.w <= 0 || t.h <= 0 || t.w > TH.forBox || t.h > TH.forBox || !thAtlasEnsure()) return false;
    int s = thSlotAlloc();
    if (s < 0) return false;
    double t0 = nowMs();
    UINT x = (UINT)((s % TH.per) * TH.forBox), y = (UINT)((s / TH.per) * TH.forBox);
    D3D11_BOX b = { x, y, 0, x + (UINT)t.w, y + (UINT)t.h, 1 };
    G.ctx->UpdateSubresource(G.thumbTex, 0, &b, t.px.data(), (UINT)t.w * 4, 0);
    t.slot = s;
    TH.owner[(size_t)s] = &t;
    t.readyAt = t.shown ? 0 : nowMs();
    t.shown = true;
    TH.frameUpMs += nowMs() - t0;
    return true;
}

// Passaggio WARP → GPU: il device nuovo non ha l'atlante; le miniature tornano dalle copie in RAM
static void thumbsDeviceChanged() {
    thAtlasForget();
    A.dirty = true;
}

// --- code -----------------------------------------------------------------------------------
// Il thread UI si riprende le richieste non ancora partite (quelle sui thread arrivano comunque)
static void thQueuesTakeBack() {
    if (!TH.started) return;
    std::deque<ThumbReq> old[POOL_N];
    EnterCriticalSection(&TH.lock);
    for (int p = 0; p < POOL_N; p++) old[p].swap(TH.pool[p].q);
    LeaveCriticalSection(&TH.lock);
    for (int p = 0; p < POOL_N; p++)
        for (const ThumbReq& r : old[p]) {
            auto& m = r.icon ? TH.icons : TH.map;
            auto it = m.find(r.key);
            if (it != m.end()) it->second.queued = false;
        }
}

static void thPush(std::vector<ThumbReq>* add) {
    EnterCriticalSection(&TH.lock);
    for (int p = 0; p < POOL_N; p++)
        for (ThumbReq& r : add[p]) TH.pool[p].q.push_back(std::move(r));
    LeaveCriticalSection(&TH.lock);
    for (int p = 0; p < POOL_N; p++)
        if (!add[p].empty()) ReleaseSemaphore(TH.pool[p].sem, (LONG)add[p].size(), nullptr);
}

// Icone: quelle chieste al file vero (per file, o un'unità) toccano il disco, e su un'unità di rete
// o ottica vanno sui thread della rete; in Questo PC (TH.remote falso) decide il tipo dell'unità,
// quello letto con la lista (un'unità appena connessa può non essere ancora in A.drives).
static int thPoolFor(const ThumbReq& r, bool perFile) {
    if (r.icon) return perFile && (r.drive ? r.slowDrive : TH.remote) ? POOL_NET : POOL_FAST;
    return r.src == SRC_SHELL ? POOL_SHELL : TH.remote ? POOL_NET : r.src == SRC_WIC ? POOL_WIC : POOL_FAST;
}

// Richieste per le voci volute, in ordine: visibili, una schermata avanti nella direzione dello
// scorrimento, una dall'altra parte
static void thSync(int first, int last) {
    thQueuesTakeBack();
    int n = (int)A.view.size(), span = std::max(1, last - first);
    int a0 = TH.dir >= 0 ? last : first - span, a1 = TH.dir >= 0 ? last + span : first;
    int b0 = TH.dir >= 0 ? first - span : last, b1 = TH.dir >= 0 ? first : last + span;
    int ranges[3][2] = { { first, last }, { a0, a1 }, { b0, b1 } };
    std::vector<ThumbReq> add[POOL_N];
    for (auto& rg : ranges)
        for (int vi = std::max(0, rg[0]); vi < std::min(n, rg[1]); vi++) {
            const Entry& e = A.cur->e[A.view[(size_t)vi]];
            std::wstring name(A.cur->name(e), e.nameLen);
            int kind = thKind(e);
            if (kind != TK_NONE) {
                ThumbEntry& t = thEntry(e);
                if (t.state == THS_NEW && !t.queued) {
                    ThumbReq r;
                    r.key = joinPath(A.cur->path, name.c_str());
                    r.dir = A.cur->path; r.name = name; r.attr = e.attr; r.box = TH.forBox; r.src = t.src;
                    r.image = kind == TK_IMAGE; r.local = !TH.remote && !A.cur->zipLen; r.gen = TH.gen; r.sizeGen = TH.sizeGen; // zip: niente cache di Windows da riempire
                    t.queued = true;
                    int pool = thPoolFor(r, false);
                    add[pool].push_back(std::move(r));
                }
                if (t.state != THS_NONE) continue;
            }
            std::wstring key;
            bool perFile;
            ThumbEntry& ic = thIcon(e, &key, &perFile);
            if (ic.state == THS_NEW && !ic.queued) {
                ThumbReq r;
                r.key = key; r.dir = A.cur->path; r.name = name; r.attr = e.attr; r.drive = iconDrive(A.cur, e);
                r.slowDrive = r.drive && slowDriveType((UINT)e.size); r.inZip = A.cur->zipLen != 0;
                r.box = std::max(1, (int)(TH.forBox * ui::THUMB_ICON)); r.icon = true; r.sizeGen = TH.sizeGen;
                ic.queued = true;
                int pool = thPoolFor(r, perFile);
                add[pool].push_back(std::move(r));
            }
        }
    thPush(add);
}

// Risultati arrivati dai thread: pronti, o si prova la fonte successiva, o l'icona. Il passo
// successivo si accoda subito (rifare tutte le code a ogni risultato costava fino a 1 ms per frame).
static void thApply() {
    if (!TH.started) return;
    std::vector<ThumbRes> res;
    EnterCriticalSection(&TH.lock);
    res.swap(TH.results);
    LeaveCriticalSection(&TH.lock);
    if (res.empty()) return;
    std::vector<ThumbWarm> warm;
    std::vector<ThumbReq> add[POOL_N];
    // Passo successivo della stessa voce (la cartella è ancora quella: le voci sono ancora volute)
    auto next = [&](const ThumbRes& r, ThumbEntry& t, bool icon) {
        if (r.gen != TH.gen) { TH.needSync = true; return; }
        ThumbReq q;
        q.dir = r.dir; q.name = r.name; q.attr = r.attr; q.local = r.local; q.gen = r.gen; q.sizeGen = r.sizeGen;
        bool perFile = false;
        if (icon) { // la stessa chiave del disegno: i .ico, per esempio, hanno l'icona per file
            q.key = thIconKeyFor(r.dir, r.name.c_str(), r.attr, false, t.inZip, perFile);
            ThumbEntry& ic = TH.icons[q.key];
            if (ic.state != THS_NEW || ic.queued) return;
            ic.queued = true;
            q.icon = true;
            q.inZip = t.inZip;
            q.box = std::max(1, (int)(TH.forBox * ui::THUMB_ICON));
        } else {
            q.key = r.key; q.src = t.src; q.box = r.box; q.image = t.kind == TK_IMAGE;
            t.queued = true;
        }
        int pool = thPoolFor(q, perFile);
        add[pool].push_back(std::move(q));
    };
    for (ThumbRes& r : res) {
        if (r.sizeGen != TH.sizeGen) continue; // altra misura del riquadro
        auto& m = r.icon ? TH.icons : TH.map;
        auto it = m.find(r.key);
        if (it == m.end()) continue;
        ThumbEntry& t = it->second;
        t.queued = false;
        if (r.outcome == TR_DROP) { TH.needSync = true; continue; } // superata: si richiede se torna voluta
        int st = r.icon ? SRC_ICON : r.src;
        A.th.ms[st] += r.ms;
        A.th.n[st]++;
        if (t.state != THS_NEW) continue;
        if (r.outcome == TR_OK) {
            if (t.slot >= 0) { TH.owner[(size_t)t.slot] = nullptr; TH.freeSlots.push_back(t.slot); t.slot = -1; }
            TH.ram -= t.px.size() * 4;
            t.px.swap(r.px);
            TH.ram += t.px.size() * 4;
            t.w = r.w; t.h = r.h;
            t.state = THS_READY;
            if (!r.icon && r.src == SRC_WIC && r.local) warm.push_back(ThumbWarm{ r.dir, r.name, r.box });
        } else if (r.outcome == TR_MISS && !r.icon) {
            A.th.miss++;
            if (t.online) { t.state = THS_NONE; next(r, t, true); } // solo online: niente letture, l'icona
            else { t.src = (uint8_t)(t.kind == TK_IMAGE ? SRC_WIC : SRC_SHELL); next(r, t, false); }
        } else {
            t.state = THS_NONE;
            if (!r.icon) { A.th.failed++; next(r, t, true); }
        }
    }
    thPush(add);
    if (!warm.empty()) {
        EnterCriticalSection(&TH.lock);
        for (ThumbWarm& w : warm) {
            if (TH.warm.size() >= kThWarmMax) TH.warm.pop_front();
            TH.warm.push_back(std::move(w));
        }
        LeaveCriticalSection(&TH.lock);
        ReleaseSemaphore(TH.pool[POOL_SHELL].sem, 1, nullptr);
    }
    A.dirty = true;
}

static void onThumbResults() { A.dirty = true; } // si applicano nel prossimo frame (thumbsFrame)

// Le potature girano all'inizio del frame, dopo TH.frame++: le voci sullo schermo sono quelle
// disegnate nel frame precedente (lastUse == TH.frame - 1), e non si toccano. Togliere quelle
// rimetterebbe in THS_NEW celle visibili senza richiederle, che resterebbero vuote fino allo scorrimento.
static bool thOnScreen(const ThumbEntry& t) { return t.lastUse + 1 >= TH.frame; }

// Oltre il limite di RAM si tolgono le copie usate meno di recente (sulla GPU restano)
static void thTrimRam() {
    if (TH.ram <= kThRamMax) return;
    std::vector<ThumbEntry*> v;
    for (auto* m : { &TH.map, &TH.icons })
        for (auto& kv : *m)
            if (!kv.second.px.empty() && !thOnScreen(kv.second)) v.push_back(&kv.second);
    std::sort(v.begin(), v.end(), [](const ThumbEntry* a, const ThumbEntry* b) { return a->lastUse < b->lastUse; });
    for (ThumbEntry* t : v) {
        if (TH.ram <= kThRamMax / 100 * 85) break;
        TH.ram -= t->px.size() * 4;
        std::vector<uint32_t>().swap(t->px);
        if (t->slot < 0) thRestart(*t);
    }
}

// Troppe voci ricordate (cartelle enormi visitate una dopo l'altra): via quelle vuote
static void thTrimEntries() {
    if (TH.map.size() <= kThMaxEntries) return;
    for (auto it = TH.map.begin(); it != TH.map.end();) {
        const ThumbEntry& t = it->second;
        if (t.slot < 0 && t.px.empty() && !t.queued && !thOnScreen(t)) it = TH.map.erase(it);
        else ++it;
    }
}

static bool thReset() { // altra misura del riquadro: si ricomincia da capo
    thQueuesTakeBack();
    thAtlasDrop();
    TH.map.clear();
    TH.icons.clear();
    TH.ram = 0;
    TH.sizeGen++;
    TH.first = TH.last = -1;
    return true;
}

// Prima schermata: tutte le voci visibili hanno la loro immagine sulla GPU
static bool thVisibleDone() {
    int n = (int)A.view.size();
    for (int vi = std::max(0, TH.first); vi < std::min(n, TH.last); vi++) {
        const Entry& e = A.cur->e[A.view[(size_t)vi]];
        if (thKind(e) != TK_NONE) {
            const ThumbEntry& t = thEntry(e);
            if (t.state == THS_READY) { if (t.slot < 0) return false; continue; }
            if (t.state == THS_NEW) return false;
        }
        const ThumbEntry& ic = thIcon(e);
        if (ic.state == THS_NEW || (ic.state == THS_READY && ic.slot < 0)) return false;
    }
    return true;
}

static bool anyTabGrid() {
    if (A.grid) return true;
    for (int i = 0; i < (int)A.tabs.size(); i++)
        if (i != A.tab && A.tabs[(size_t)i].grid) return true;
    return false;
}

// Una volta per frame, prima di disegnare le voci [first, last) (in elenco: -1, -1)
static void thumbsFrame(int first, int last) {
    TH.frame++;
    if (TH.frameUpMs > 0) { A.th.texMs = TH.frameUpMs; A.th.texMax = std::max(A.th.texMax, TH.frameUpMs); }
    TH.frameUpMs = 0;
    if (!A.grid || !A.cur) {
        if (TH.active) { thQueuesTakeBack(); TH.active = false; TH.first = TH.last = -1; }
        // Nessuna griglia aperta: tornano liberi la VRAM e anche la RAM (fino a 96 MB di pixel). Le
        // miniature si rifanno se si torna in griglia: dalla cache di Windows costano meno di 1 ms l'una.
        if ((G.thumbTex || !TH.map.empty() || !TH.icons.empty()) && !anyTabGrid()) {
            thReset();
            if (TH.started) {
                std::vector<ThumbRes> old; // risultati arrivati intanto: hanno la misura vecchia, si buttano
                EnterCriticalSection(&TH.lock);
                old.swap(TH.results);
                LeaveCriticalSection(&TH.lock);
            }
        }
        return;
    }
    if (!TH.started) thStart();
    if (!TH.active) { TH.fsStart = nowMs(); TH.fsDone = false; TH.first = TH.last = -1; } // griglia appena aperta
    TH.active = true;
    if ((int)A.thumbPx != TH.forBox) {
        thReset();
        TH.forBox = (int)A.thumbPx;
        TH.fsStart = nowMs();
        TH.fsDone = false;
    }
    if (A.cur != TH.forList || A.cur->gen != TH.forListGen) { // cartella nuova o ricaricata
        TH.forList = A.cur;
        TH.forListGen = A.cur->gen;
        TH.gen++;
        InterlockedExchange64(&g_thLatest, (LONG64)TH.gen); // quelle in corso per la cartella di prima smettono
        TH.remote = slowPath(A.cur->path);
        TH.first = TH.last = -1;
        if (A.cur->path != TH.forPath) { TH.forPath = A.cur->path; TH.fsStart = nowMs(); TH.fsDone = false; }
    }
    thApply();
    thTrimRam();
    thTrimEntries();
    if (A.scrollY != TH.lastScroll) { TH.dir = A.scrollY > TH.lastScroll ? 1 : -1; TH.lastScroll = A.scrollY; }
    if (first != TH.first || last != TH.last || TH.needSync) {
        TH.first = first;
        TH.last = last;
        TH.needSync = false;
        thSync(first, last);
    }
    if (!TH.fsDone && thVisibleDone()) { TH.fsDone = true; A.th.firstMs = nowMs() - TH.fsStart; }
    if (A.showStats) {
        A.th.inRam = 0;
        for (auto* m : { &TH.map, &TH.icons })
            for (auto& kv : *m) if (!kv.second.px.empty()) A.th.inRam++;
        A.th.onGpu = 0;
        for (ThumbEntry* t : TH.owner) if (t) A.th.onGpu++;
        A.th.ramMB = TH.ram / 1048576.0;
    }
}

static void thQuad(float x, float y, const ThumbEntry& t, float a, uint32_t tint) {
    float k = 1.0f / kThAtlas;
    float u = (float)((t.slot % TH.per) * TH.forBox), v = (float)((t.slot / TH.per) * TH.forBox);
    // u + 3: nello shader indica l'atlante delle miniature
    quad(x, y, x + t.w, y + t.h, 3.0f + u * k, v * k, 3.0f + (u + t.w) * k, (v + t.h) * k, fadeA(tint, a));
}

static float thFade(const ThumbEntry& t, double now) {
    if (!A.animations || t.readyAt <= 0) return 1;
    float a = easeOut((float)(now - t.readyAt) / ui::DUR_ICON);
    if (a < 1) A.animActive = true;
    return a;
}

// Miniatura della voce vi nel riquadro (coordinate della finestra): centrata e appoggiata al fondo.
// Senza miniatura l'icona grande, centrata; per i tipi che ne avranno una, finché arriva, niente.
static void drawThumb(int vi, const Box& box, bool hidden, double now) {
    if (!TH.active) return;
    const Entry& e = A.cur->e[A.view[(size_t)vi]];
    uint32_t tint = hidden ? C_ICON_HIDDEN : C_ICON;
    if (thKind(e) != TK_NONE) {
        ThumbEntry& t = thEntry(e);
        t.lastUse = TH.frame;
        if (t.state == THS_NEW) return;
        if (t.state == THS_READY) {
            if (t.slot < 0) thUpload(t);
            if (t.slot >= 0) thQuad(box.x + floorf((box.w - t.w) * 0.5f), box.y + box.h - t.h, t, thFade(t, now), tint);
            return;
        }
    }
    ThumbEntry& ic = thIcon(e);
    ic.lastUse = TH.frame;
    if (ic.state != THS_READY) return;
    if (ic.slot < 0) thUpload(ic);
    if (ic.slot >= 0)
        thQuad(box.x + floorf((box.w - ic.w) * 0.5f), box.y + floorf((box.h - ic.h) * 0.5f), ic, thFade(ic, now), tint);
}

// --- vista per cartella (viste.txt) -------------------------------------------------------------
// Una riga per cartella in griglia: "percorso<TAB>griglia". Le altre sono in elenco.
static std::wstring viewKey(const std::wstring& path) {
    std::wstring k = path;
    if (!k.empty()) CharLowerBuffW(&k[0], (DWORD)k.size());
    return k;
}

static bool viewIsGrid(const std::wstring& path) {
    auto it = A.gridFolders.find(viewKey(path));
    if (it == A.gridFolders.end()) return false;
    it->second = ++A.gridUse; // usata di recente: resta tra le kViewsMax
    return true;
}

static void viewsSave() {
    if (!A.viewsReady) return; // prima della lettura si riscriverebbe il file con le sole viste nuove: ci pensa viewsLoaded
    std::vector<std::pair<uint32_t, const std::wstring*>> v;
    for (auto& kv : A.gridFolders) v.push_back({ kv.second, &kv.first });
    std::sort(v.begin(), v.end(), [](const std::pair<uint32_t, const std::wstring*>& a, const std::pair<uint32_t, const std::wstring*>& b) { return a.first > b.first; });
    if ((int)v.size() > kViewsMax) {
        for (size_t i = kViewsMax; i < v.size(); i++) A.gridFolders.erase(*v[i].second);
        v.resize(kViewsMax);
    }
    std::string d;
    for (auto& p : v) d += toUtf8(*p.second) + "\tgriglia\r\n";
    saveDataAsync(L"viste.txt", std::move(d));
}

// Geometria nuova (vista o dimensione): resta in vista la prima voce, poi il cursore
static void viewRelayout(int first) {
    A.cursorFor = nullptr; // il cursore salta, non scivola da una forma all'altra
    A.hover = -1;
    A.rowFade = HoverFade();
    tipHide();
    layout();
    scrollToItem(first);
    clampScroll();
    ensureVisible();
    A.dirty = true;
}

// viste.txt letto all'avvio. Le griglie scelte prima del suo arrivo restano, come le più recenti, e si salvano.
static void viewsLoaded(const std::vector<std::wstring>& lines) {
    std::vector<std::wstring> early;
    for (auto& kv : A.gridFolders) early.push_back(kv.first);
    uint32_t n = (uint32_t)lines.size();
    for (uint32_t i = 0; i < n; i++) { // dalla più recente: le prime hanno il numero più alto
        const std::wstring& l = lines[i];
        size_t tab = l.find(L'\t');
        if (tab == std::wstring::npos || l.compare(tab + 1, std::wstring::npos, L"griglia") != 0) continue;
        std::wstring k = viewKey(l.substr(0, tab));
        if (!A.gridFolders.count(k)) A.gridFolders[k] = n - i;
    }
    A.gridUse = std::max(A.gridUse, n);
    A.viewsReady = true;
    for (const std::wstring& k : early) A.gridFolders[k] = ++A.gridUse;
    if (!early.empty()) viewsSave();
    if (A.cur && !A.grid && viewIsGrid(A.cur->path)) { // la prima cartella è arrivata prima del file
        int first = firstVisibleItem();
        A.grid = true;
        viewRelayout(first);
    }
}

// Elenco o griglia (size: dimensione delle miniature, -1 quella di prima). Si ricorda per cartella.
static void viewSet(bool grid, int size) {
    int first = firstVisibleItem();
    bool changed = grid != A.grid;
    if (size >= 0 && size <= 2 && size != A.thumbSize) {
        A.thumbSize = size;
        settingsSave();
        changed = true;
    }
    if (!changed) return;
    if (A.cur && grid != A.grid) {
        std::wstring k = viewKey(A.cur->path);
        if (grid) A.gridFolders[k] = ++A.gridUse;
        else A.gridFolders.erase(k);
        viewsSave();
    }
    A.grid = grid;
    viewRelayout(first);
}

// Ctrl+rotellina in griglia: piccole ↔ medie ↔ grandi
static void thumbSizeStep(int delta) {
    int s = std::max(0, std::min(2, A.thumbSize + delta));
    if (s != A.thumbSize) viewSet(true, s);
}
