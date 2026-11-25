// [z]f — percorso a segmenti cliccabili (barra del percorso).
//
//   Questo PC › Disco locale (C:) › Users › utente
//
// Clic su un segmento: ci va (selezionando la cartella da cui si arriva). Clic su "›": menu delle
// sottocartelle, letto su un thread; se la lettura supera 150 ms il menu si apre subito con
// "caricamento…" e si riempie all'arrivo. Clic centrale su un segmento: nuova scheda. Clic sullo
// spazio vuoto: modifica del percorso come testo. Se il percorso non sta nella barra, i primi
// livelli si raccolgono in "…", che li mostra in un menu.
// I nomi sono quelli che si vedono altrove: unità con la loro etichetta, preferiti con il loro nome.

struct Crumb {
    std::wstring label, path, name; // name: nome vero sul disco (per selezionarlo tornando su)
    float x = 0, w = 0, chevX = 0, chevW = 0;
    bool shown = true;
};

struct CrumbBar {
    std::vector<Crumb> segs;
    bool ell = false;            // primi livelli raccolti in "…"
    float ellX = 0, ellW = 0;
    float y = 0, h = 0, endX = 0;
    HoverFade fade;              // id: 2·i segmento, 2·i+1 "›", 1000 "…"
    int hover = -1;
    int menuFor = -1;            // "›" il cui menu è aperto (resta evidenziato)
    uint64_t gen = 0;            // richiesta di sottocartelle in corso
    int pendingSeg = -1;
    std::wstring pendingPath;
    bool loadingShown = false;   // il menu "caricamento…" è già stato aperto
} CB;

enum { CRUMB_ELL = 1000 };
enum { M_CRUMB_ITEM = 5000 };   // voci dei menu del percorso: M_CRUMB_ITEM + indice

static void crumbsBuild() {
    CB.segs.clear();
    Crumb pc;
    pc.label = L"Questo PC";
    CB.segs.push_back(pc);
    if (!A.cur || A.cur->path.empty()) return;
    const std::wstring& p = A.cur->path;
    size_t start;
    if (p.size() >= 2 && p[1] == L':') { // unità
        Crumb d;
        d.path = p.substr(0, 2) + L"\\";
        d.name = d.path;
        const SideItem* di = nullptr;
        for (const SideItem& s : A.drives) if (!_wcsnicmp(s.path.c_str(), d.path.c_str(), 2)) di = &s;
        d.label = di ? di->label : p.substr(0, 2);
        CB.segs.push_back(d);
        start = 3;
    } else if (p.size() > 2 && p[0] == L'\\' && p[1] == L'\\') { // \\server\condivisione
        size_t a = p.find(L'\\', 2);
        size_t b = a == std::wstring::npos ? std::wstring::npos : p.find(L'\\', a + 1);
        Crumb u;
        u.path = b == std::wstring::npos ? p : p.substr(0, b);
        u.label = u.name = u.path;
        CB.segs.push_back(u);
        start = b == std::wstring::npos ? p.size() : b + 1;
    } else {
        return;
    }
    while (start < p.size()) {
        size_t e = p.find(L'\\', start);
        if (e == std::wstring::npos) e = p.size();
        if (e > start) {
            Crumb c;
            c.name = p.substr(start, e - start);
            c.path = p.substr(0, e);
            int f = favIndex(c.path);
            c.label = f >= 0 ? A.favs[f].label : c.name;
            CB.segs.push_back(c);
        }
        start = e + 1;
    }
}

// Posizioni, dentro [x0, x1). Se non c'è spazio i primi livelli finiscono in "…".
static void crumbsLayout(float x0, float x1) {
    float segPad = px(ui::GAP_S + ui::GAP_XS), chevPad = px(ui::GAP_XS);
    int chevAdv = textWidth(L"\x203A", 1);
    float total = 0;
    for (Crumb& c : CB.segs) {
        c.w = textWidth(c.label.c_str(), (int)c.label.size()) + segPad * 2;
        c.chevW = chevAdv + chevPad * 2;
        total += c.w + c.chevW;
        c.shown = true;
    }
    CB.ell = false;
    CB.ellW = textWidth(L"\x2026", 1) + segPad * 2;
    int n = (int)CB.segs.size();
    for (int i = 0; i < n - 1 && total > x1 - x0; i++) { // si nasconde dal primo, l'ultimo resta sempre
        if (!CB.ell) { CB.ell = true; total += CB.ellW + CB.segs[0].chevW; }
        CB.segs[i].shown = false;
        total -= CB.segs[i].w + CB.segs[i].chevW;
    }
    float x = x0;
    if (CB.ell) { CB.ellX = x; x += CB.ellW; x += CB.segs[0].chevW; } // "…" seguito da "›" (solo grafico)
    for (Crumb& c : CB.segs) {
        if (!c.shown) continue;
        c.x = x; x += c.w;
        c.chevX = x; x += c.chevW;
        if (c.x + c.w > x1) c.w = std::max(0.0f, x1 - c.x); // un solo segmento più largo della barra
    }
    CB.endX = x;
}

// id sotto il punto: 2·i segmento, 2·i+1 "›", CRUMB_ELL, -1 nulla (spazio vuoto)
static int crumbHit(int x, int y) {
    if (A.editingPath || y < CB.y || y >= CB.y + CB.h) return -1;
    if (CB.ell && x >= CB.ellX && x < CB.ellX + CB.ellW) return CRUMB_ELL;
    for (int i = 0; i < (int)CB.segs.size(); i++) {
        const Crumb& c = CB.segs[i];
        if (!c.shown) continue;
        if (x >= c.x && x < c.x + c.w) return 2 * i;
        if (x >= c.chevX && x < c.chevX + c.chevW) return 2 * i + 1;
    }
    return -1;
}

static void drawCrumbs(float x0, float x1, float y, float h, float textY, double dt) {
    double t0 = nowMs();
    crumbsBuild();
    crumbsLayout(x0, x1);
    A.crumbMs = nowMs() - t0;
    A.crumbMax = std::max(A.crumbMax, A.crumbMs);
    CB.y = y; CB.h = h;
    hoverFadeUpdate(CB.fade, CB.hover, dt);
    auto bg = [&](float x, float w, int id) {
        float a = (id == 2 * CB.menuFor + 1 && A.menu.open && A.menu.kind == MENU_CRUMB) ? 1.0f : hoverAlpha(CB.fade, id);
        if (a > 0) rrect(x, y, w, h, px(ui::R_S), fadeA(C_HOVER, a));
    };
    float segPad = px(ui::GAP_S + ui::GAP_XS), chevPad = px(ui::GAP_XS);
    int n = (int)CB.segs.size();
    if (CB.ell) {
        bg(CB.ellX, CB.ellW, CRUMB_ELL);
        drawText(CB.ellX + segPad, textY, L"\x2026", 1, C_DIM);
        drawText(CB.ellX + CB.ellW + chevPad, textY, L"\x203A", 1, C_FAINT);
    }
    for (int i = 0; i < n; i++) {
        const Crumb& c = CB.segs[i];
        if (!c.shown) continue;
        bg(c.x, c.w, 2 * i);
        drawText(c.x + segPad, textY, c.label, i == n - 1 ? C_TEXT : C_DIM, c.x + c.w - segPad);
        bg(c.chevX, c.chevW, 2 * i + 1);
        drawText(c.chevX + chevPad, textY, L"\x203A", 1, C_FAINT);
    }
}

// --- menu delle sottocartelle ---------------------------------------------------------
struct CrumbReq { HWND hwnd; uint64_t gen; std::wstring path; };
struct CrumbRes { uint64_t gen; std::vector<std::wstring> names; std::vector<uint8_t> hidden; bool ok; };

// Sottocartelle di path con l'ordinamento naturale della lista; hidden: nascoste. Tocca il disco:
// solo su un thread (menu "›", "Vai a"). false se la cartella non si può leggere.
static bool readSubfolders(const std::wstring& path, std::vector<std::wstring>& names, std::vector<uint8_t>& hidden) {
    size_t alen = 0;
    if (zipSplit(path, alen)) return zipSubfolders(path, alen, names, hidden); // uno zip o una cartella dentro
    std::wstring pat = longPath(path);
    pat += pat.back() == L'\\' ? L"*" : L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(pat.c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return false;
    struct Item { std::vector<uint16_t> key; std::wstring name; bool hid; };
    std::vector<Item> items;
    std::vector<uint16_t> key(kKeyMax);
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue; // il filtro è solo un suggerimento
        const wchar_t* nm = fd.cFileName;
        if (nm[0] == L'.' && (!nm[1] || (nm[1] == L'.' && !nm[2]))) continue;
        Item it;
        it.name = nm;
        it.hid = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
        uint32_t kn = buildKey(nm, (int)it.name.size(), key.data()); // la chiave può superare il doppio del nome
        it.key.assign(key.begin(), key.begin() + kn);
        items.push_back(std::move(it));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        size_t n = std::min(a.key.size(), b.key.size());
        int c = memcmp(a.key.data(), b.key.data(), n * sizeof(uint16_t));
        if (c) return c < 0;
        return a.key.size() < b.key.size();
    });
    for (Item& it : items) { names.push_back(std::move(it.name)); hidden.push_back(it.hid); }
    return true;
}

static DWORD WINAPI crumbThread(void* p) {
    CrumbReq* r = (CrumbReq*)p;
    CrumbRes* res = new CrumbRes{ r->gen, {}, {}, false };
    res->ok = readSubfolders(r->path, res->names, res->hidden);
    if (!PostMessageW(r->hwnd, WM_APP_CRUMB, 0, (LPARAM)res)) delete res;
    delete r;
    return 0;
}

static void menuPlace(float cx, float cy, bool fromKeyboard, double t0);

// Apre il menu del percorso con le voci date (label, percorso, icona, attenuata)
static void crumbMenuOpen(int seg, const std::vector<std::wstring>& labels, const std::vector<std::wstring>& paths,
                          const std::vector<uint8_t>& dim, int icon, const std::wstring& hotName, const wchar_t* emptyText) {
    double t0 = nowMs();
    PopupMenu& m = A.menu;
    m.items.clear();
    m.labels.clear();
    m.paths.clear();
    m.kind = MENU_CRUMB;
    m.target = seg;
    m.hot = -1;
    for (size_t i = 0; i < labels.size(); i++) {
        m.labels.push_back(labels[i]);
        m.paths.push_back(paths[i]);
        MenuItem it{ m.labels.back().c_str(), L"", M_CRUMB_ITEM + (int)i, true };
        it.icon = icon;
        it.dim = i < dim.size() && dim[i];
        m.items.push_back(it);
        if (!hotName.empty() && !_wcsicmp(paths[i].c_str(), hotName.c_str())) m.hot = (int)i;
    }
    if (m.items.empty()) { m.labels.push_back(emptyText); m.items.push_back(MenuItem{ m.labels.back().c_str(), L"", M_CRUMB_ITEM, false }); }
    float x = seg == CRUMB_ELL ? CB.ellX : CB.segs[seg].chevX;
    CB.menuFor = seg == CRUMB_ELL ? -1 : seg;
    menuPlace(x, CB.y + CB.h + px(ui::GAP_XS), m.hot >= 0, t0);
}

// "›" del segmento seg: le sue sottocartelle (o le unità, per "Questo PC")
static void crumbChevron(int seg) {
    if (seg < 0 || seg >= (int)CB.segs.size()) return;
    const Crumb& c = CB.segs[seg];
    std::wstring next = seg + 1 < (int)CB.segs.size() ? CB.segs[seg + 1].path : L"";
    if (c.path.empty()) { // Questo PC: unità, già in memoria
        std::vector<std::wstring> l, p;
        for (const SideItem& d : A.drives) { l.push_back(d.label); p.push_back(d.path); }
        crumbMenuOpen(seg, l, p, {}, 2, next, L"Nessuna unità");
        return;
    }
    CB.pendingSeg = seg;
    CB.pendingPath = c.path;
    CB.loadingShown = false;
    CrumbReq* r = new CrumbReq{ A.hwnd, ++CB.gen, c.path };
    HANDLE h = CreateThread(nullptr, 0, crumbThread, r, 0, nullptr);
    if (h) { CloseHandle(h); SetTimer(A.hwnd, 7, ui::LOADING_AFTER, nullptr); }
    else { delete r; CB.pendingSeg = -1; }
}

// La lettura è lenta (unità di rete, disco che si sveglia): il menu si apre subito, vuoto
static void crumbOnTimer() {
    KillTimer(A.hwnd, 7);
    if (CB.pendingSeg < 0) return;
    CB.loadingShown = true;
    crumbMenuOpen(CB.pendingSeg, {}, {}, {}, 0, L"", L"caricamento\x2026");
}

static void onCrumbResult(CrumbRes* r) {
    bool stillWanted = r->gen == CB.gen && CB.pendingSeg >= 0 && CB.pendingSeg < (int)CB.segs.size() &&
                       CB.segs[CB.pendingSeg].path == CB.pendingPath &&                       // stessa cartella
                       (!CB.loadingShown || (A.menu.open && A.menu.kind == MENU_CRUMB)); // menu non chiuso nel frattempo
    if (r->gen == CB.gen) { KillTimer(A.hwnd, 7); }
    if (stillWanted) {
        int seg = CB.pendingSeg;
        CB.pendingSeg = -1;
        const Crumb& c = CB.segs[seg];
        std::vector<std::wstring> paths;
        for (const std::wstring& n : r->names) paths.push_back(joinPath(c.path, n.c_str()));
        std::wstring next = seg + 1 < (int)CB.segs.size() ? CB.segs[seg + 1].path : L"";
        crumbMenuOpen(seg, r->names, paths, r->hidden, 1, next,
                      r->ok ? L"Nessuna sottocartella" : L"Impossibile leggere la cartella");
    } else if (r->gen == CB.gen) {
        CB.pendingSeg = -1;
    }
    delete r;
}

// "…": i livelli nascosti, dal più vicino
static void crumbEllipsis() {
    std::vector<std::wstring> l, p;
    std::vector<uint8_t> dim;
    for (int i = (int)CB.segs.size() - 1; i >= 0; i--)
        if (!CB.segs[i].shown) { l.push_back(CB.segs[i].label); p.push_back(CB.segs[i].path); }
    crumbMenuOpen(CRUMB_ELL, l, p, dim, 1, L"", L"");
}

// Clic sinistro nella barra del percorso. false: spazio vuoto (si passa alla modifica del testo)
static bool crumbClick(int x, int y) {
    int id = crumbHit(x, y);
    if (id < 0) return false;
    if (id == CRUMB_ELL) { crumbEllipsis(); return true; }
    int seg = id / 2;
    if (id & 1) { // "›": se il suo menu è già aperto, il clic lo chiude
        if (A.menu.open && A.menu.kind == MENU_CRUMB && CB.menuFor == seg) { A.menu.open = false; A.dirty = true; return true; }
        crumbChevron(seg);
        return true;
    }
    if (seg == (int)CB.segs.size() - 1) return true; // la cartella corrente
    std::wstring sel = seg + 1 < (int)CB.segs.size() ? CB.segs[seg + 1].name : L"";
    navigate(CB.segs[seg].path, 1, sel);
    return true;
}

// Clic centrale: segmento in una nuova scheda
static bool crumbMiddle(int x, int y) {
    int id = crumbHit(x, y);
    if (id < 0 || id == CRUMB_ELL || (id & 1)) return false;
    tabNew(CB.segs[id / 2].path, false);
    return true;
}
