// [z]f — tooltip: informazioni su ciò che sta sotto il mouse.
//
// Compaiono dopo 500 ms fermi su un elemento; passando a un elemento vicino subito dopo, in 60 ms
// (come in Windows). Spariscono a qualsiasi clic, tasto o rotellina. Le informazioni che toccano il
// disco (data di creazione, tipo di file, contenuto di una cartella, dimensioni di un'immagine) le
// legge un thread al momento di mostrare il tooltip; il tooltip le aspetta al massimo 150 ms, così
// compare già completo invece di allargarsi sotto gli occhi.

enum TipKind { TIP_NONE, TIP_ROW, TIP_SIDE, TIP_TAB, TIP_TABPLUS, TIP_TABCLOSE, TIP_HEADER, TIP_STATS, TIP_PATH, TIP_DRIVES,
               TIP_CHROME, TIP_FILTER, TIP_GOTO, TIP_FAVDEL, // cestino di un preferito (sidebar.cpp)
               TIP_LINK }; // destinazione di un collegamento nella colonna Destinazione (links.cpp)

struct TipInfo {           // risultato del thread
    uint64_t gen = 0;
    bool ok = false;
    uint64_t created = 0;
    std::wstring type;
    int dirs = -1, files = -1; bool more = false; // contenuto di una cartella
    int imgW = 0, imgH = 0;
    bool zip = false;      // voce dentro uno zip: una cartella ha già il totale come dimensione
    std::wstring link;     // collegamento (.lnk): destinazione (links.cpp)
};

struct Tooltip {
    int kind = TIP_NONE, index = -1;
    const void* owner = nullptr; // la cartella a cui si riferisce l'indice di riga
    bool visible = false, waiting = false;
    double shownAt = 0, hiddenAt = -1e9;
    int mx = 0, my = 0;
    uint64_t gen = 0;
    TipInfo info;
    std::wstring title;
    std::vector<std::pair<std::wstring, std::wstring>> rows;
} T;

struct TipReq { HWND hwnd; uint64_t gen; std::wstring path; bool dir; bool drive; };

static void tipHide() {
    if (T.kind == TIP_ROW) dirSizeAsk(DS_TIP, L""); // il conto della cartella non serve più
    KillTimer(A.hwnd, 5);
    KillTimer(A.hwnd, 6);
    if (T.visible) { T.hiddenAt = nowMs(); A.dirty = true; }
    T.visible = T.waiting = false;
    T.kind = TIP_NONE;
    T.index = -1;
}

// --- lettura delle informazioni (thread) --------------------------------------------------
static uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

// Larghezza × altezza dalle prime centinaia di KB di un'immagine (PNG, GIF, BMP, JPEG)
static constexpr size_t kImageHead = 256 * 1024;
static void imageSizeOf(const uint8_t* p, size_t n, int& w, int& h);
static void imageSize(const std::wstring& path, int& w, int& h) {
    HANDLE f = CreateFileW(longPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    std::vector<uint8_t> b(kImageHead);
    DWORD n = 0;
    if (!ReadFile(f, b.data(), (DWORD)b.size(), &n, nullptr)) n = 0;
    CloseHandle(f);
    imageSizeOf(b.data(), n, w, h);
}
static void imageSizeOf(const uint8_t* p, size_t n, int& w, int& h) {
    if (n >= 24 && !memcmp(p, "\x89PNG", 4)) { w = (int)be32(p + 16); h = (int)be32(p + 20); return; }
    if (n >= 10 && !memcmp(p, "GIF8", 4)) { w = p[6] | p[7] << 8; h = p[8] | p[9] << 8; return; }
    if (n >= 26 && p[0] == 'B' && p[1] == 'M') { int32_t ww, hh; memcpy(&ww, p + 18, 4); memcpy(&hh, p + 22, 4); w = ww; h = hh < 0 ? -hh : hh; return; }
    if (n >= 4 && p[0] == 0xFF && p[1] == 0xD8) { // JPEG: si cerca il segmento SOF
        size_t i = 2;
        while (i + 9 < n) {
            if (p[i] != 0xFF) { i++; continue; }
            uint8_t m = p[i + 1];
            if (m == 0xFF) { i++; continue; }
            size_t len = (size_t)p[i + 2] << 8 | p[i + 3];
            if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
                h = p[i + 5] << 8 | p[i + 6];
                w = p[i + 7] << 8 | p[i + 8];
                return;
            }
            if (m == 0xD9 || m == 0xDA) return; // fine o inizio dei dati: niente SOF prima
            i += 2 + len;
        }
    }
}

static bool isImageExt(const wchar_t* ext) {
    static const wchar_t* k[] = { L".png", L".jpg", L".jpeg", L".jfif", L".gif", L".bmp" };
    for (auto e : k) if (!_wcsicmp(ext, e)) return true;
    return false;
}

// Estensione del nome (col punto), o nullptr
static const wchar_t* fileExt(const std::wstring& path) {
    const wchar_t* dot = wcsrchr(path.c_str(), L'.');
    const wchar_t* slash = wcsrchr(path.c_str(), L'\\');
    return dot && (!slash || dot > slash) ? dot : nullptr;
}

// Informazioni che toccano il disco o la shell (data di creazione, tipo, contenuto di una cartella,
// dimensioni di un'immagine). Su un thread: tooltip e anteprima.
static void readInfo(const std::wstring& path, bool dir, bool drive, TipInfo* inf) {
    size_t alen = 0;
    if (!drive && zipInside(path, alen)) { // dentro uno zip: dall'archivio in memoria (zip.cpp)
        inf->zip = true;
        if (!dir) {
            if (const wchar_t* dot = fileExt(path)) {
                SHFILEINFOW sfi = {};
                if (SHGetFileInfoW(dot, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES))
                    inf->type = sfi.szTypeName;
                std::string head;
                if (isImageExt(dot) && zipReadMem(path, alen, kImageHead, head, nullptr, nullptr, nullptr) == ZE_OK)
                    imageSizeOf((const uint8_t*)head.data(), head.size(), inf->imgW, inf->imgH);
            }
        } else {
            ZipRef r = zipResolve(path, alen);
            if (r.z && !r.err) { inf->dirs = (int)r.z->nodes[r.node].dirs; inf->files = (int)r.z->nodes[r.node].files; }
        }
        inf->ok = true;
        return;
    }
    WIN32_FILE_ATTRIBUTE_DATA fa = {};
    std::wstring lp = longPath(path);
    bool attrs = !drive && GetFileAttributesExW(lp.c_str(), GetFileExInfoStandard, &fa);
    if (attrs) inf->created = (uint64_t)fa.ftCreationTime.dwHighDateTime << 32 | fa.ftCreationTime.dwLowDateTime;
    if (!dir) {
        if (const wchar_t* dot = fileExt(path)) {
            SHFILEINFOW sfi = {};
            if (SHGetFileInfoW(dot, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES))
                inf->type = sfi.szTypeName;
            if (isImageExt(dot)) imageSize(path, inf->imgW, inf->imgH);
            if (!_wcsicmp(dot, L".lnk") && attrs && !onlineOnly(fa.dwFileAttributes)) { bool d; linkRead(path, inf->link, d); }
        }
    } else if (!drive) { // contenuto: al massimo 200 ms di lettura
        double t0 = nowMs();
        WIN32_FIND_DATAW fd;
        std::wstring pat = lp + (lp.back() == L'\\' ? L"*" : L"\\*");
        HANDLE h = FindFirstFileExW(pat.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h != INVALID_HANDLE_VALUE) {
            int d = 0, f = 0;
            do {
                const wchar_t* nm = fd.cFileName;
                if (nm[0] == L'.' && (!nm[1] || (nm[1] == L'.' && !nm[2]))) continue;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) d++; else f++;
                if (((d + f) & 1023) == 0 && nowMs() - t0 > 200) { inf->more = true; break; }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
            inf->dirs = d; inf->files = f;
        }
    }
    inf->ok = true;
}

static DWORD WINAPI tipThread(void* p) {
    TipReq* r = (TipReq*)p;
    TipInfo* inf = new TipInfo;
    inf->gen = r->gen;
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); // richiesto da SHGetFileInfo
    readInfo(r->path, r->dir, r->drive, inf);
    if (SUCCEEDED(co)) CoUninitialize();
    if (!PostMessageW(r->hwnd, WM_APP_INFO, 0, (LPARAM)inf)) delete inf;
    delete r;
    return 0;
}

// --- contenuto ---------------------------------------------------------------------------
static std::wstring withThousands(uint64_t v) {
    std::wstring s = std::to_wstring(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, 1, L'.');
    return s;
}

static std::wstring relTime(uint64_t ft) {
    FILETIME nf;
    GetSystemTimeAsFileTime(&nf);
    uint64_t now = (uint64_t)nf.dwHighDateTime << 32 | nf.dwLowDateTime;
    if (ft > now) return L"";
    uint64_t sec = (now - ft) / 10000000ull;
    wchar_t b[48];
    if (sec < 60) return L"adesso";
    if (sec < 3600) { _snwprintf(b, 47, L"%llu min fa", sec / 60); return b; }
    if (sec < 86400) { _snwprintf(b, 47, sec / 3600 == 1 ? L"un'ora fa" : L"%llu ore fa", sec / 3600); return b; }
    uint64_t d = sec / 86400;
    if (d == 1) return L"ieri";
    if (d < 31) { _snwprintf(b, 47, L"%llu giorni fa", d); return b; }
    if (d < 365) { _snwprintf(b, 47, d / 30 == 1 ? L"un mese fa" : L"%llu mesi fa", d / 30); return b; }
    _snwprintf(b, 47, d / 365 == 1 ? L"un anno fa" : L"%llu anni fa", d / 365);
    return b;
}

static std::wstring dateLine(uint64_t ft) {
    wchar_t b[32];
    fmtDate(ft, b);
    std::wstring r = relTime(ft);
    return r.empty() ? std::wstring(b) : std::wstring(b) + L" · " + r;
}

static std::wstring sizeLine(uint64_t bytes) {
    wchar_t b[32];
    fmtSize(bytes, b);
    return bytes < 1024 ? std::wstring(b) : std::wstring(b) + L"  (" + withThousands(bytes) + L" byte)";
}

// Dimensione di una cartella (dirsize.cpp): "12,3 GB  (13.245.123.456 byte) · 1.234 file";
// mentre conta "almeno 1,2 GB…"
static std::wstring dirSizeLine(const DirSize& d, bool computing) {
    if (!computing && d.denied && !d.files) return L"accesso negato";
    wchar_t b[32];
    fmtSize(d.bytes, b);
    std::wstring files = L" \x00B7 " + withThousands(d.files) + L" file";
    if (computing) return L"almeno " + std::wstring(b) + L"\x2026" + files;
    std::wstring t = sizeLine(d.bytes) + files;
    if (d.denied) t += L" \x00B7 alcune cartelle non leggibili";
    return t;
}

static std::wstring attrLine(DWORD a) {
    std::wstring r;
    auto add = [&](const wchar_t* t) { if (!r.empty()) r += L", "; r += t; };
    if (a & FILE_ATTRIBUTE_HIDDEN) add(L"nascosto");
    if (a & FILE_ATTRIBUTE_SYSTEM) add(L"di sistema");
    if (a & FILE_ATTRIBUTE_READONLY) add(L"sola lettura");
    if (a & FILE_ATTRIBUTE_COMPRESSED) add(L"compresso");
    if (a & FILE_ATTRIBUTE_ENCRYPTED) add(L"crittografato");
    if (a & FILE_ATTRIBUTE_REPARSE_POINT) add(L"collegamento (reparse point)");
    if (onlineOnly(a)) add(L"solo online");
    return r;
}

// Coppie etichetta / valore (etichetta vuota: nota). Le stesse righe nel tooltip e nell'anteprima.
typedef std::vector<std::pair<std::wstring, std::wstring>> InfoRows;
static void infoRow(InfoRows& rows, const wchar_t* label, const std::wstring& value) {
    if (!value.empty()) rows.push_back({ label, value });
}
static void tipRow(const wchar_t* label, const std::wstring& value) { infoRow(T.rows, label, value); }

// Unità ("C:\") o percorso di rete (dove porta) di una voce di Questo PC
static const SideItem* driveInfo(const std::wstring& path) {
    for (const SideItem& d : A.drives)
        if (d.place ? samePath(d.path, path) : path.size() >= 2 && path[1] == L':' && !_wcsnicmp(d.path.c_str(), path.c_str(), 2)) return &d;
    return nullptr;
}

static void driveRows(InfoRows& rows, const SideItem& d) {
    infoRow(rows, L"Tipo", d.place ? driveKind(DRIVE_NETPLACE) : d.label.find(L'(') != std::wstring::npos ? driveKind(d.driveType) : L"");
    infoRow(rows, L"Percorso di rete", d.unc);
    infoRow(rows, L"File system", d.fs);
    if (d.total) {
        wchar_t a[32], b[32], c[64];
        fmtSize(d.total, a);
        infoRow(rows, L"Capacità", a);
        fmtSize(d.total - d.free, b);
        _snwprintf(c, 63, L"%s  (%.0f%%)", b, 100.0 * (double)(d.total - d.free) / (double)d.total);
        c[63] = 0;
        infoRow(rows, L"Usato", c);
        fmtSize(d.free, a);
        infoRow(rows, L"Libero", a);
    }
}

// Contenuto di una cartella: "3 cartelle, 1.204 file"; "più di …" se la lettura si è fermata prima
// del fondo; "vuota". Tooltip e piede dell'anteprima.
static std::wstring contentLine(int dirs, int files, bool more) {
    if (!dirs && !files && !more) return L"vuota";
    return std::wstring(more ? L"più di " : L"") + withThousands((uint64_t)dirs) + (dirs == 1 ? L" cartella, " : L" cartelle, ") +
           withThousands((uint64_t)files) + L" file";
}

// Righe di un file o di una cartella; quelle lette dal disco (info) solo se sono arrivate
// path: per la dimensione totale di una cartella (dirsize.cpp), chiesta da chi mostra le righe
static void fileRows(InfoRows& rows, const std::wstring& path, uint32_t attr, uint64_t size, uint64_t mtime, const TipInfo& info) {
    bool dir = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    infoRow(rows, L"Tipo", dir ? L"Cartella" : (info.ok ? info.type : L""));
    if (info.ok) infoRow(rows, L"Destinazione", info.link);
    if (!dir || info.zip) infoRow(rows, L"Dimensione", sizeLine(size));
    else if (!(attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
        DirSize d;
        bool computing = false;
        infoRow(rows, L"Dimensione", dirSizeGet(path, d, computing) ? dirSizeLine(d, computing) : L"conto\x2026");
    }
    if (dir && info.ok && info.dirs >= 0) infoRow(rows, L"Contenuto", contentLine(info.dirs, info.files, info.more));
    if (info.imgW > 0 && info.imgH > 0) infoRow(rows, L"Immagine", std::to_wstring(info.imgW) + L" × " + std::to_wstring(info.imgH) + L" pixel");
    infoRow(rows, L"Modificato", dateLine(mtime));
    if (info.ok && info.created) infoRow(rows, L"Creato", dateLine(info.created));
    infoRow(rows, L"Attributi", attrLine(attr));
}

// Ritorna false se l'elemento non esiste più (la vista è cambiata)
static bool tipBuild() {
    T.title.clear();
    T.rows.clear();
    switch (T.kind) {
    case TIP_ROW: {
        if (T.owner != (const void*)A.cur || !A.cur || T.index < 0 || T.index >= (int)A.cur->e.size()) return false;
        const Entry& e = A.cur->e[T.index];
        T.title.assign(A.cur->name(e), e.nameLen);
        if (A.cur->isDrives) {
            std::wstring p(A.cur->name(e), e.nameLen);
            if (const SideItem* d = driveInfo(p)) { T.title = d->label; driveRows(T.rows, *d); }
            break;
        }
        fileRows(T.rows, joinPath(A.cur->path, A.cur->name(e)), e.attr, e.size, e.mtime, T.info);
        if (isSel((uint32_t)T.index)) {
            uint64_t bytes = 0;
            int k = selCount(&bytes);
            if (k > 1) {
                wchar_t b[32];
                fmtSize(bytes, b);
                tipRow(L"Selezione", std::to_wstring(k) + L" elementi" + (bytes ? L" · " + std::wstring(b) : L""));
            }
        }
        break;
    }
    case TIP_LINK: {
        if (T.owner != (const void*)A.cur || !A.cur || T.index < 0) return false;
        const LinkTarget* t = linkOf(A.cur, (uint32_t)T.index);
        if (!t) return false;
        T.title = t->path;
        tipRow(L"", L"Clic: apri percorso file \x00B7 clic centrale: nuova scheda");
        break;
    }
    case TIP_SIDE: {
        const SideItem* it = sideItem(T.index);
        if (!it) return false;
        T.title = it->label;
        if (it->drive || it->place) driveRows(T.rows, *it);
        else { tipRow(L"Percorso", it->path); tipRow(L"", L"Clic centrale: nuova scheda"); }
        break;
    }
    case TIP_TAB: {
        if (T.index < 0 || T.index >= (int)A.tabs.size()) return false;
        std::wstring p;
        if (T.index == A.tab) p = A.cur ? A.cur->path : A.loadPath;
        else p = A.tabs[T.index].pending ? A.tabs[T.index].pendingPath : (A.tabs[T.index].cur ? A.tabs[T.index].cur->path : L"");
        T.title = p.empty() ? L"Questo PC" : p;
        if (T.index < 8) tipRow(L"Scorciatoia", L"Ctrl+" + std::to_wstring(T.index + 1));
        tipRow(L"", L"Clic centrale: chiude");
        break;
    }
    case TIP_TABPLUS:  T.title = L"Nuova scheda"; tipRow(L"Scorciatoia", shortcutsOf(M_TAB_NEW)); break;
    case TIP_TABCLOSE: T.title = L"Chiudi scheda"; tipRow(L"Scorciatoia", shortcutsOf(M_TAB_CLOSE)); break;
    case TIP_FAVDEL: T.title = L"Togli dai preferiti"; tipRow(L"", L"Clic, poi un altro clic per confermare"); break;
    case TIP_HEADER: {
        static const wchar_t* names[] = { L"Ordina per nome", L"Ordina per data di modifica", L"Ordina per dimensione",
                                          L"Ordina per destinazione" };
        if (T.index < 0 || T.index > COL_LINK) return false;
        T.title = names[T.index];
        if (A.sortCol == T.index) tipRow(L"", A.sortDesc ? L"Ora: decrescente. Clic: crescente" : L"Ora: crescente. Clic: decrescente");
        break;
    }
    case TIP_PATH: {
        if (A.editingPath) return false;
        int id = T.index;
        if (id == CRUMB_ELL) { T.title = L"Livelli superiori"; break; }
        if (id >= 0 && id / 2 < (int)CB.segs.size()) {
            const Crumb& c = CB.segs[id / 2];
            if (id & 1) { T.title = c.path.empty() ? L"Unità" : L"Sottocartelle di " + c.label; break; }
            T.title = c.path.empty() ? L"Questo PC" : c.path;
            if (id / 2 + 1 < (int)CB.segs.size()) tipRow(L"", L"Clic: apri · clic centrale: nuova scheda");
            break;
        }
        T.title = A.cur && !A.cur->path.empty() ? A.cur->path : L"Questo PC";
        tipRow(L"Modifica", shortcutsOf(M_EDIT_PATH));
        if (!A.filter.empty()) tipRow(L"Togli il filtro", shortcutsOf(M_FILTER_CLEAR));
        break;
    }
    case TIP_GOTO:
        if (!gotoTipBuild(T.index)) return false;
        break;
    case TIP_DRIVES:
        T.title = (T.index == -3 ? A.favOpen : A.drvOpen) ? L"Comprimi" : L"Espandi";
        tipRow(L"", T.index == -3 ? L"Preferiti" : L"Unità di Questo PC");
        break;
    case TIP_CHROME:
        switch (T.index) {
        case CH_MENU:    T.title = L"Menu di [z]f"; tipRow(L"", L"Opzioni, nuova finestra, integrazione con Windows"); break;
        case CH_SIDEBAR: T.title = A.sideVisible ? L"Nascondi la barra laterale" : L"Mostra la barra laterale"; tipRow(L"Scorciatoia", shortcutsOf(M_SIDEBAR)); break;
        case CH_BACK:
            T.title = L"Indietro";
            if (!A.back.empty()) tipRow(L"Va a", A.back.back().empty() ? L"Questo PC" : A.back.back());
            tipRow(L"Scorciatoia", shortcutsOf(M_BACK));
            break;
        case CH_FWD:
            T.title = L"Avanti";
            if (!A.fwd.empty()) tipRow(L"Va a", A.fwd.back().empty() ? L"Questo PC" : A.fwd.back());
            tipRow(L"Scorciatoia", shortcutsOf(M_FWD));
            break;
        case CH_UP:      T.title = L"Cartella superiore"; tipRow(L"Scorciatoia", shortcutsOf(M_UP)); break;
        case CH_STAR:
            T.title = A.cur && favIndex(A.cur->path) >= 0 ? L"Togli dai preferiti" : L"Aggiungi ai preferiti";
            tipRow(L"Scorciatoia", shortcutFor(M_FAV_TOGGLE));
            break;
        case CH_VIEW:
            T.title = A.grid ? L"Mostra come elenco" : L"Mostra come griglia";
            tipRow(L"Scorciatoia", shortcutFor(A.grid ? M_VIEW_LIST : M_GRID_S + A.thumbSize));
            if (A.grid) tipRow(L"", L"Ctrl+rotellina cambia la dimensione delle miniature");
            break;
        case CH_PREVIEW:
            T.title = A.prevOpen ? L"Nascondi l'anteprima" : L"Mostra l'anteprima";
            tipRow(L"Scorciatoia", shortcutFor(M_PREVIEW));
            if (A.prevOpen && !A.prevW) tipRow(L"", L"La finestra è troppo stretta per mostrarla");
            break;
        default: return false;
        }
        break;
    case TIP_FILTER:
        T.title = A.filter.empty() ? L"Filtro" : L"Filtro: " + A.filter;
        tipRow(L"", L"Scrivi in qualsiasi punto della lista per filtrare i nomi");
        tipRow(L"Togli il filtro", shortcutsOf(M_FILTER_CLEAR) + L" \x00B7 \x00D7");
        break;
    case TIP_STATS:
        T.title = L"Tempi misurati";
        tipRow(L"lettura", L"elenco dei file dal disco");
        tipRow(L"chiavi", L"preparazione dell'ordinamento naturale");
        tipRow(L"ordinamento", L"ordinamento della lista");
        tipRow(L"filtro", L"ultimo filtro applicato");
        tipRow(L"frame", L"preparazione dell'ultimo frame (CPU)");
        tipRow(L"avvio", L"dalla creazione del processo al contenuto visibile");
        tipRow(L"", L"F12: tutti i dettagli");
        break;
    default: return false;
    }
    return true;
}

static void tipShow() {
    KillTimer(A.hwnd, 6);
    T.waiting = false;
    if (A.menu.open || A.renaming || A.draggingThumb || !tipBuild()) { tipHide(); return; }
    if (!T.visible) T.shownAt = nowMs();
    T.visible = true;
    A.dirty = true;
}

// Timer 5: il mouse è fermo da abbastanza. Per file e cartelle si chiedono prima le informazioni.
static void tipOnTimer() {
    KillTimer(A.hwnd, 5);
    if (T.kind == TIP_NONE) return;
    if (T.kind == TIP_ROW && A.cur && !A.cur->isDrives && T.owner == (const void*)A.cur &&
        T.index >= 0 && T.index < (int)A.cur->e.size()) {
        const Entry& e = A.cur->e[T.index];
        T.info = TipInfo();
        TipReq* r = new TipReq{ A.hwnd, ++T.gen, joinPath(A.cur->path, A.cur->name(e)),
                                (e.attr & FILE_ATTRIBUTE_DIRECTORY) != 0, false };
        if (r->dir && !(e.attr & FILE_ATTRIBUTE_REPARSE_POINT) && !A.cur->zipLen) dirSizeAsk(DS_TIP, r->path); // il totale arriva mentre cresce
        HANDLE h = CreateThread(nullptr, 0, tipThread, r, 0, nullptr);
        if (h) {
            CloseHandle(h);
            T.waiting = true;
            SetTimer(A.hwnd, 6, ui::TIP_INFO_WAIT, nullptr); // se le informazioni tardano, si mostra quello che c'è
            return;
        }
        delete r;
    }
    tipShow();
}

// Dimensioni delle cartelle aggiornate (dirsize.cpp): tooltip, anteprima e colonna
static void onDirSize() {
    InterlockedExchange(&DS.posted, 0);
    if (T.visible && T.kind == TIP_ROW) tipBuild();
    A.dirty = true;
}

static void onTipInfo(TipInfo* inf) {
    if (inf->gen == T.gen && T.kind == TIP_ROW) {
        T.info = *inf;
        if (T.waiting) tipShow();
        else if (T.visible) { tipBuild(); A.dirty = true; } // arrivate dopo: si aggiorna
    }
    delete inf;
}

// Elemento sotto il mouse: (tipo, indice)
static void tipTarget(int x, int y, int& kind, int& index) {
    kind = TIP_NONE; index = -1;
    if (gotoIsOpen()) { if (gotoTipTarget(x, y, index)) kind = TIP_GOTO; return; } // sopra il resto
    bool closeHit, plusHit;
    int t = tabHit(x, y, &closeHit, &plusHit);
    if (plusHit) { kind = TIP_TABPLUS; return; }
    int chId = chromeHit(x, y);
    if (chId != CH_NONE && chId != CH_MIN && chId != CH_MAX && chId != CH_CLOSE) { kind = TIP_CHROME; index = chId; return; }
    if (y >= G.h - A.statusH && x >= A.fltX && x < A.fltX + A.fltW) { kind = TIP_FILTER; return; }
    if (A.timesH > 0 && x >= A.sideW && y >= G.h - A.footH && y < G.h - A.statusH) { kind = TIP_STATS; return; }
    if (t >= 0) { kind = closeHit ? TIP_TABCLOSE : TIP_TAB; index = t; return; }
    if (int ft = favTrashAt(x, y); ft >= 0) { kind = TIP_FAVDEL; index = ft; return; }
    int si = sideHit(x, y);
    if (si >= 0) { kind = TIP_SIDE; index = si; return; }
    if (si == -2 || si == -3) { kind = TIP_DRIVES; index = si; return; }
    if (y >= A.pathY && y < A.pathY + A.topH) { kind = TIP_PATH; index = crumbHit(x, y); return; }
    if (x < A.listX || x >= A.listR - A.sbW) return;
    if (y >= A.sideTop && y < A.listTop) {
        if (colBorderAt(x, y) >= 0 || A.colDrag >= 0) return; // sul bordo il clic non ordina: niente "Ordina per…"
        kind = TIP_HEADER; index = colAt(x);
        return;
    }
    if (int lh = linkHit(x, y); lh >= 0) { kind = TIP_LINK; index = (int)A.view[lh]; return; }
    int r = itemAt(x, y);
    if (r >= 0) { kind = TIP_ROW; index = (int)A.view[r]; }
}

static void tipMouseMove(int x, int y) {
    int kind, index;
    tipTarget(x, y, kind, index);
    if (A.menu.open) kind = TIP_NONE;
    const void* owner = kind == TIP_ROW || kind == TIP_LINK ? (const void*)A.cur : nullptr;
    if (kind == T.kind && index == T.index && owner == T.owner) return;
    bool wasVisible = T.visible;
    tipHide();
    if (kind == TIP_NONE) return;
    T.kind = kind; T.index = index; T.owner = owner;
    T.mx = x; T.my = y;
    // Appena chiuso un tooltip, il successivo compare quasi subito (scorrendo tra elementi vicini)
    bool quick = wasVisible || nowMs() - T.hiddenAt < ui::TIP_NEAR_WINDOW;
    SetTimer(A.hwnd, 5, quick ? ui::TIP_DELAY_NEAR : ui::TIP_DELAY, nullptr);
}

// --- disegno ---------------------------------------------------------------------------
static void drawTooltip() {
    if (!T.visible) return;
    float pad = px(ui::TIP_PAD), lh = (float)F.lineH + px(ui::LINE_GAP), gap = px(ui::TIP_COL_GAP);
    float maxW = px(ui::TIP_MAX_W);
    PanelEnter e = panelEnter(T.shownAt, ui::DUR_TIP);
    float k = e.k;
    // Titolo su più righe se lungo (nomi di file lunghi: il punto del tooltip è leggerli interi)
    std::vector<std::pair<int, int>> tl;
    {
        int start = 0, n = (int)T.title.size();
        while (start < n) {
            int w = 0, i = start, lastFit = start;
            while (i < n) {
                int j = i;
                int cw = glyphGet(decodeUtf16(T.title.c_str(), n, j)).adv;
                if (w + cw > maxW - pad * 2 && i > start) break;
                w += cw; i = j; lastFit = i;
            }
            tl.push_back({ start, lastFit - start });
            start = lastFit;
        }
    }
    float labW = 0, valW = 0, titleW = 0;
    for (auto& l : tl) titleW = std::max(titleW, (float)textWidth(T.title.c_str() + l.first, l.second));
    for (auto& r : T.rows) {
        labW = std::max(labW, (float)textWidth(r.first.c_str(), (int)r.first.size()));
        valW = std::max(valW, (float)textWidth(r.second.c_str(), (int)r.second.size()));
    }
    float rowsW = labW > 0 ? labW + gap + valW : valW;
    float w = std::min(maxW, std::max(titleW, rowsW) + pad * 2);
    float h = pad * 2 + tl.size() * lh + (T.rows.empty() ? 0 : px(ui::TIP_ROWS_GAP) + T.rows.size() * lh) - px(ui::LINE_GAP);
    float x = T.mx + px(ui::TIP_OFFSET_X), y = T.my + px(ui::TIP_OFFSET_Y);
    float edge = px(ui::GAP_S); // mai a filo del bordo della finestra
    if (x + w > G.w - edge) x = std::max(edge, G.w - edge - w);
    if (y + h > G.h - edge) y = std::max(edge, T.my - h - px(ui::TIP_OFFSET_ABOVE));
    x = floorf(x); y = floorf(y) - e.dy;
    drawPanel(x, y, w, h, px(ui::R_S), k); // tooltip: 4 px, come in Windows 11 (sono piccoli)
    float ty = y + pad;
    for (auto& l : tl) { drawText(x + pad, ty, T.title.c_str() + l.first, l.second, fadeA(C_TEXT, k), x + w - pad); ty += lh; }
    if (!T.rows.empty()) ty += px(ui::TIP_ROWS_GAP);
    for (auto& r : T.rows) {
        float vx = x + pad + (labW > 0 && !r.first.empty() ? labW + gap : 0);
        if (!r.first.empty()) drawText(x + pad, ty, r.first, fadeA(C_DIM, k));
        drawText(r.first.empty() ? x + pad : vx, ty, r.second, fadeA(r.first.empty() ? C_FAINT : C_TEXT, k), x + w - pad);
        ty += lh;
    }
}
