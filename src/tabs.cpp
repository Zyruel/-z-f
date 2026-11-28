// [z]f — schede: ognuna conserva cartella, selezione, scroll, filtro e cronologia.
//
// Lo stato della scheda attiva vive nei campi di App, così il resto del codice non sa nulla
// delle schede. Cambiare scheda scambia quei campi con quelli salvati: O(1), nessuna copia.
// Solo la scheda attiva è osservata dal watcher; una scheda che torna attiva viene ricaricata.
// Le schede aperte in background dalla stessa scheda si accodano in ordine, come nei browser.

static void tabSwap(TabState& t) {
    std::swap(A.cur, t.cur);
    A.order.swap(t.order);
    A.view.swap(t.view);
    A.selFlags.swap(t.selFlags);
    std::swap(A.sel, t.sel);
    std::swap(A.anchor, t.anchor);
    std::swap(A.scrollY, t.scrollY);
    std::swap(A.scrollTarget, t.scrollTarget);
    A.filter.swap(t.filter);
    std::swap(A.colFilter, t.colFilter);
    std::swap(A.groupCol, t.groupCol);
    A.groups.swap(t.groups);
    A.groupClosed.swap(t.groupClosed);
    A.back.swap(t.back);
    A.fwd.swap(t.fwd);
    std::swap(A.sortCol, t.sortCol);
    std::swap(A.sortDesc, t.sortDesc);
    std::swap(A.zoom, t.zoom);
}

// Nome breve di una cartella: "Documenti", "C:\", "Questo PC"
static std::wstring folderTitle(const std::wstring& path) {
    if (path.empty()) return L"Questo PC";
    std::wstring p = path;
    while (p.size() > 3 && p.back() == L'\\') p.pop_back();
    if (p.size() <= 3) return p;
    size_t k = p.find_last_of(L'\\');
    return k == std::wstring::npos ? p : p.substr(k + 1);
}

static int favIndex(const std::wstring& path); // sidebar.cpp

// Titolo: il nome del preferito se la cartella lo è ("Download", non "Downloads")
static std::wstring tabTitleFor(const std::wstring& path) {
    int f = favIndex(path);
    return f >= 0 ? A.favs[f].label : folderTitle(path);
}
static std::wstring tabTitle(int i) {
    if (i == A.tab) return A.cur ? tabTitleFor(A.cur->path) : L"…";
    const TabState& t = A.tabs[i];
    if (t.pending) return tabTitleFor(t.pendingPath);
    return t.cur ? tabTitleFor(t.cur->path) : L"…";
}

static void updateTitle() {
    std::wstring t = L"[z]f";
    if (A.cur) t += L" — " + (A.cur->path.empty() ? std::wstring(L"Questo PC") : A.cur->path);
    SetWindowTextW(A.hwnd, t.c_str());
}

static void tabActivate(int i) {
    if (g_inPane) { g_paneAfter.push_back([i] { tabActivate(i); }); return; } // App è l'anteprima (PaneScope): all'uscita
    if (i < 0 || i >= (int)A.tabs.size() || i == A.tab) return;
    A.renaming = false;
    A.menu.open = false;
    A.hover = A.groupHover = -1;
    TabState& old = A.tabs[A.tab];
    if (A.loading) { // lasciata a metà caricamento: riprende quando torna attiva
        old.pending = true;
        old.pendingPath = A.loadPath;
        old.pendingSelect = A.pendingSelect; // la sottocartella da cui si è saliti, /select da fuori…
        old.pendingHistory = A.pendingHistory;
    }
    tabSwap(old);           // la scheda corrente torna nel suo guscio
    A.tab = i;
    A.tabOpenAfter = -1;
    tabSwap(A.tabs[i]);     // la nuova diventa viva; il suo guscio resta vuoto
    A.listFade = ui::LIST_FADE_TAB; // cambio di scheda: dissolvenza breve
    A.rowFade = HoverFade();
    A.gen++;                // le letture ancora in corso della scheda precedente vanno scartate
    A.loading = false;
    A.refreshInFlight = false;
    A.refreshAgain = false; // riguardava la cartella della scheda precedente
    A.pendingHistory = 0;
    TabState& shell = A.tabs[i];
    bool redo = shell.resort; // il flag non passa da tabSwap: resta nel guscio, si legge qui
    shell.resort = false;
    if (shell.pending) {
        shell.pending = false;
        std::wstring p, sel;
        p.swap(shell.pendingPath);
        sel.swap(shell.pendingSelect);
        stopWatch(); // la cartella della scheda precedente non riguarda questa
        navigate(p, shell.pendingHistory, sel);
        shell.pendingHistory = 0;
    } else if (A.cur) {
        if (redo) resort(); // subito: la ricarica qui sotto arriva dopo il primo frame
        startWatch(A.cur->path);
        A.changeAt = nowMs();
        requestRefresh(); // mentre non era attiva la cartella può essere cambiata
    }
    layout();
    clampScroll();
    updateTitle();
    A.dirty = true;
}

// Nuova scheda accanto a quella corrente
static void tabNew(const std::wstring& path, bool activate, const std::wstring& select) {
    TabState t;
    t.pending = true;
    t.pendingPath = path;
    t.pendingSelect = select;
    int at = (A.tabOpenAfter > A.tab && A.tabOpenAfter < (int)A.tabs.size() ? A.tabOpenAfter : A.tab) + 1;
    A.tabs.insert(A.tabs.begin() + at, std::move(t));
    A.tabOpenAfter = at;
    if (activate) tabActivate(at);
    A.dirty = true;
}

static void tabClose(int i) {
    if (i < 0 || i >= (int)A.tabs.size()) return;
    if (A.tabs.size() == 1) { PostMessageW(A.hwnd, WM_CLOSE, 0, 0); return; }
    if (i == A.tab) tabActivate(i + 1 < (int)A.tabs.size() ? i + 1 : i - 1);
    delete A.tabs[i].cur; // non è più la scheda attiva: il suo stato è nel guscio
    A.tabs.erase(A.tabs.begin() + i);
    if (A.tab > i) A.tab--;
    A.tabHover = -1;
    A.tabOpenAfter = -1;
    A.dirty = true;
}

static bool samePath(const std::wstring& a, const std::wstring& b); // sidebar.cpp

// Seleziona per nome nella scheda attiva (già caricata)
static void selectByName(const std::wstring& name) {
    if (!A.cur || name.empty()) return;
    for (int i = 0; i < (int)A.view.size(); i++)
        if (_wcsicmp(A.cur->name(A.cur->e[A.view[i]]), name.c_str()) == 0) {
            selOnly(i);
            A.sel = i;
            ensureVisible();
            A.dirty = true;
            return;
        }
}

// Una cartella aperta da fuori (seconda istanza di [z]f): diventa una scheda di questa finestra.
// Se la cartella è già aperta in una scheda si passa a quella invece di duplicarla.
static void openFromOutside(const std::wstring& path, const std::wstring& select) {
    if (IsIconic(A.hwnd)) ShowWindow(A.hwnd, SW_RESTORE);
    SetForegroundWindow(A.hwnd);
    if (path.empty() && select.empty()) return; // solo "portami [z]f davanti"
    for (int i = 0; i < (int)A.tabs.size(); i++) {
        TabState& t = A.tabs[i];
        if (i == A.tab && A.loading) { // in caricamento: la selezione si applica all'arrivo
            if (!samePath(A.loadPath, path)) continue;
            if (!select.empty()) A.pendingSelect = select;
            return;
        }
        if (i != A.tab && t.pending) { // in attesa di diventare attiva
            if (!samePath(t.pendingPath, path)) continue;
            if (!select.empty()) t.pendingSelect = select;
            tabActivate(i);
            return;
        }
        const Listing* l = i == A.tab ? A.cur : t.cur;
        if (l && samePath(l->path, path)) {
            tabActivate(i);
            // La voce da mostrare può essere nascosta dai filtri o in un gruppo chiuso: si tolgono i
            // filtri e si riaprono i gruppi, se no selectByName non la trova
            if ((!A.filter.empty() || A.colFilter.any() || !A.groupClosed.empty()) && !select.empty()) {
                A.filter.clear();
                A.colFilter.clear();
                A.groupClosed.clear();
                rebuildView(true);
            }
            selectByName(select);
            return;
        }
    }
    A.tabOpenAfter = (int)A.tabs.size() - 1; // in fondo, come una scheda nuova del browser
    tabNew(path, true, select);
}

// --- riapertura delle schede ----------------------------------------------------
// schede.txt (opzione riapri_schede, solo la finestra principale) e riavvio-<pid>.txt (riavvio
// dopo un aggiornamento, ogni finestra il suo): "attiva=<n>" e una riga per scheda,
// "scheda=<cartella>|<voce selezionata>". Cartella vuota: Questo PC. Un nome di file non contiene
// mai '|'. Si scrivono uscendo (WM_DESTROY, WM_ENDSESSION) e si leggono sul thread veloce dell'avvio.

struct SavedTab { std::wstring path, select; };
struct StartTabs { bool restart = false; std::vector<std::wstring> lines; }; // WM_APP_START

// Cartella e voce selezionata della scheda i, come le ritroverebbe chi la riapre
static bool tabPlace(int i, std::wstring& path, std::wstring& select) {
    auto selName = [](const Listing* l, const std::vector<uint32_t>& view, int sel) {
        if (!l || sel < 0 || sel >= (int)view.size() || view[sel] >= l->e.size()) return std::wstring();
        const Entry& e = l->e[view[sel]];
        return std::wstring(l->name(e), e.nameLen);
    };
    if (i == A.tab) {
        if (A.loading) { path = A.loadPath; select = A.pendingSelect; return true; }
        if (!A.cur) return false;
        path = A.cur->path;
        select = selName(A.cur, A.view, A.sel);
        return true;
    }
    const TabState& t = A.tabs[i];
    if (t.pending) { path = t.pendingPath; select = t.pendingSelect; return true; }
    if (!t.cur) return false;
    path = t.cur->path;
    select = selName(t.cur, t.view, t.sel);
    return true;
}

static std::string tabsSerialize() {
    std::string d, tabs;
    int active = 0, n = 0;
    for (int i = 0; i < (int)A.tabs.size(); i++) {
        std::wstring path, select;
        if (!tabPlace(i, path, select)) continue;
        if (i == A.tab) active = n;
        tabs += "scheda=" + toUtf8(path) + "|" + toUtf8(select) + "\r\n";
        n++;
    }
    d = "attiva=" + std::to_string(active) + "\r\n" + tabs;
    return d;
}

// Uscendo: le schede vanno nella coda dei salvataggi, che saveFlush scrive prima della fine del processo
static void tabsSaveOnExit() {
    if (A.mainInstance && A.reopenTabs) saveDataAsync(L"schede.txt", tabsSerialize());
    if (A.restartOnExit)
        saveDataAsync((L"riavvio-" + std::to_wstring(GetCurrentProcessId()) + L".txt").c_str(), tabsSerialize());
}

static bool tabsParse(const std::vector<std::wstring>& lines, std::vector<SavedTab>& tabs, int& active) {
    active = 0;
    for (const std::wstring& l : lines) {
        if (!l.compare(0, 7, L"attiva=")) active = _wtoi(l.c_str() + 7);
        else if (!l.compare(0, 7, L"scheda=")) {
            size_t bar = l.find(L'|', 7);
            SavedTab t;
            t.path = l.substr(7, bar == std::wstring::npos ? std::wstring::npos : bar - 7);
            if (bar != std::wstring::npos) t.select = l.substr(bar + 1);
            tabs.push_back(std::move(t));
        }
    }
    if (active < 0 || active >= (int)tabs.size()) active = 0;
    return !tabs.empty();
}

// Le schede lette diventano schede di questa finestra, in attesa come quelle aperte in background:
// si caricano quando diventano attive. replace: la finestra non ha ancora una cartella (avvio
// senza cartella), quindi le schede salvate sono tutte e si apre quella che era attiva. Altrimenti
// (avvio su una cartella) vanno prima della scheda aperta, che resta attiva; una scheda sulla
// stessa cartella non si ripete.
static void tabsRestore(const std::vector<SavedTab>& saved, int active, bool replace) {
    if (replace) {
        A.tabs = std::vector<TabState>(saved.size()); // costruite una per una: ognuna col suo id
        for (int i = 0; i < (int)saved.size(); i++) {
            if (i == active) continue;
            A.tabs[i].pending = true;
            A.tabs[i].pendingPath = saved[i].path;
            A.tabs[i].pendingSelect = saved[i].select;
        }
        A.tab = active;
        A.tabOpenAfter = -1;
        navigate(saved[active].path, 0, saved[active].select);
    } else {
        std::wstring cur = A.loading ? A.loadPath : A.cur ? A.cur->path : L"";
        std::vector<TabState> add;
        for (const SavedTab& s : saved) {
            if (samePath(s.path, cur)) continue;
            TabState t;
            t.pending = true;
            t.pendingPath = s.path;
            t.pendingSelect = s.select;
            add.push_back(std::move(t));
        }
        A.tabs.insert(A.tabs.begin(), std::make_move_iterator(add.begin()), std::make_move_iterator(add.end()));
        A.tab += (int)add.size();
        A.tabOpenAfter = -1;
    }
    A.dirty = true;
}

// Dal thread veloce dell'avvio: le schede da riaprire (anche nessuna)
static void onStartTabs(StartTabs* s) {
    std::vector<SavedTab> saved;
    int active = 0;
    bool ok = tabsParse(s->lines, saved, active);
    delete s;
    if (A.startDeferred) {
        A.startDeferred = false;
        if (ok) tabsRestore(saved, active, true);
        else navigate(A.startDefault, 0, L"");
    } else if (ok) {
        tabsRestore(saved, active, false);
    }
}

// --- barra delle schede -------------------------------------------------------

static float tabWidth() {
    return floorf(std::max(px(ui::TAB_MIN), std::min(px(ui::TAB_MAX), A.tabStripW / (float)A.tabs.size())));
}

// Indice della scheda sotto il punto, -1 se nessuna. closeHit: sopra la ×. plusHit: sopra il +.
static int tabHit(int x, int y, bool* closeHit, bool* plusHit) {
    *closeHit = *plusHit = false;
    if (A.tabH <= 0 || y >= A.tabH) return -1;
    float tw = tabWidth(), x0 = A.tabStripX;
    int n = (int)A.tabs.size();
    if (x >= x0 + n * tw && x < x0 + n * tw + A.rowH * ui::TAB_PLUS_W) { *plusHit = true; return -1; }
    int i = (int)((x - x0) / tw);
    if (x < x0 || i < 0 || i >= n) return -1;
    float cx = x0 + (i + 1) * tw - A.rowH * ui::TAB_CLOSE_W;
    *closeHit = x >= cx;
    return i;
}

static void drawTabs(double dt) {
    if (A.tabH <= 0) return;
    float W = (float)G.w;
    float tw = tabWidth(), x0 = A.tabStripX, top = px(ui::TAB_TOP);
    float textY = top + floorf((A.tabH - top - F.lineH) * 0.5f);
    rect(0, 0, W, A.tabH, C_TABBAR);
    hoverFadeUpdate(A.tabFade, A.tabHover, dt);
    // + e × in dissolvenza come ogni hover: 0 è il +, i + 1 la × della scheda i
    hoverFadeUpdate(A.tabBtnFade, A.tabPlusHover ? 0 : A.tabCloseHover && A.tabHover >= 0 ? A.tabHover + 1 : -1, dt);
    // Indicatore della scheda attiva: scorre fino alla nuova posizione
    float lx = x0 + A.tab * tw;
    if (A.tabLineX < 0 || fabsf(A.tabLineW - (tw - 1)) > 0.5f) { A.tabLineX = lx; A.tabLineW = tw - 1; }
    else A.tabLineX = approach(A.tabLineX, lx, dt, ui::TAU_TAB_LINE, 0.5f);
    for (int i = 0; i < (int)A.tabs.size(); i++) {
        float x = x0 + i * tw;
        bool act = i == A.tab, hov = i == A.tabHover;
        // Scheda attiva: angoli superiori da 8 px, si fonde con la riga del percorso (che la copre sotto)
        if (act) rrect(x, top, tw - 1, A.tabH - top + px(ui::R_M), px(ui::R_M), C_BAR);
        float ha = act ? 0 : hoverAlpha(A.tabFade, i);
        if (ha > 0) rrect(x + px(ui::GAP_XS), top + px(ui::GAP_XS), tw - 1 - px(ui::GAP_XS) * 2, A.tabH - top - px(ui::GAP_S), px(ui::R_S), fadeA(C_HOVER, ha));
        if (!act && i + 1 != A.tab) rect(x + tw - 1, top + px(ui::TAB_SEP_INSET), 1, A.tabH - top - px(ui::TAB_SEP_INSET) * 2, C_LINE);
        std::wstring t = tabTitle(i);
        float closeW = A.rowH * ui::TAB_CLOSE_W;
        drawText(x + px(ui::TAB_TEXT_X), textY, t, act ? C_TEXT : C_DIM, x + tw - closeW);
        if (act || hov) { // × per chiudere
            float cx = x + tw - closeW;
            float ca = hoverAlpha(A.tabBtnFade, i + 1);
            if (ca > 0) rrect(cx + px(ui::GAP_XS), top + px(ui::TAB_CLOSE_INSET_Y), closeW - px(ui::GAP_XS) - px(ui::GAP_S),
                              A.tabH - top - px(ui::TAB_CLOSE_INSET_Y) * 2, px(ui::R_S), fadeA(C_HOVER_STRONG, ca));
            drawUiIcon(cx, top, closeW, A.tabH - top, IC_CLOSE, C_DIM);
        }
    }
    { // indicatore della scheda attiva: pillola in alto, lontana dagli angoli arrotondati
        float in = px(ui::R_M), eh = std::max(1.0f, px(ui::EDGE));
        rrect(floorf(A.tabLineX) + in, top, A.tabLineW - in * 2, eh, eh * 0.5f, C_ACCENT);
    }
    float plusX = x0 + A.tabs.size() * tw;
    float pa = hoverAlpha(A.tabBtnFade, 0);
    if (pa > 0) rrect(plusX + px(ui::GAP_XS), top + px(ui::GAP_XS), A.rowH * ui::TAB_PLUS_W - px(ui::GAP_XS) * 2,
                      A.tabH - top - px(ui::GAP_XS) * 2, px(ui::R_S), fadeA(C_HOVER, pa));
    drawUiIcon(plusX, top, A.rowH * ui::TAB_PLUS_W, A.tabH - top, IC_ADD, C_DIM);
    // linea sotto la barra del titolo, interrotta sotto la scheda attiva
    float ax = x0 + A.tab * tw;
    rect(0, A.tabH - 1, ax, 1, C_LINE);
    rect(ax + tw - 1, A.tabH - 1, W - (ax + tw - 1), 1, C_LINE);
    drawChromeTitle(dt);
}
