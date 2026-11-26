// [z]f — menu contestuale nostro: voci essenziali, si apre in un frame.
//
// "Altre opzioni…" (o Shift+clic destro, Shift+F10) apre il menu completo della shell,
// che costa 200–400 ms per via delle estensioni installate: per questo non è il predefinito
// (lo diventa con l'opzione "Menu completo di Windows", A.shellMenu).

// I comandi (MenuId) stanno in app.h, perché anche i tooltip ne mostrano le scorciatoie.

static int g_dropChoice = 0; // voce scelta nel menu del trascinamento col tasto destro (0: nessuna)

static void menuClose() {
    if (!A.menu.open) return;
    A.menu.open = false;
    A.menu.pressed = false;
    A.dirty = true;
}

// La voce sotto il cursore da tastiera resta nella parte visibile di un menu che scorre
static void menuEnsureVisible() {
    PopupMenu& m = A.menu;
    if (m.hot < 0) return;
    if (m.hot < m.first) m.first = m.hot;
    if (m.hot >= m.first + m.visible) m.first = m.hot - m.visible + 1;
}

// Rotellina sopra un menu che scorre
static void menuScroll(int lines) {
    PopupMenu& m = A.menu;
    int maxFirst = std::max(0, (int)m.items.size() - m.visible);
    m.first = std::max(0, std::min(maxFirst, m.first + lines));
    A.dirty = true;
}

// La scorciatoia accanto alla voce viene da kShortcuts. note: testo al posto della scorciatoia,
// per uno stato ("attivo")
static void menuAdd(const wchar_t* label, int id, bool enabled = true, const wchar_t* note = nullptr) {
    A.menu.items.push_back(MenuItem{ label, note ? note : shortcutFor(id), id, enabled });
}
static void menuSep() {
    if (!A.menu.items.empty() && A.menu.items.back().id != 0) A.menu.items.push_back(MenuItem{ L"", L"", 0, false });
}

// Cartella selezionata (singola) come percorso completo, "" altrimenti
static std::wstring selectedFolder() {
    if (!A.cur) return L"";
    std::vector<uint32_t> sel = selectedEntries();
    if (sel.size() != 1 || !(A.cur->e[sel[0]].attr & FILE_ATTRIBUTE_DIRECTORY)) return L"";
    return joinPath(A.cur->path, A.cur->name(A.cur->e[sel[0]]));
}

static void menuPlace(float cx, float cy, bool fromKeyboard, double t0);

// Menu di una voce della barra laterale (i = indice di sideHit) o di una scheda
static void menuOpenSide(float cx, float cy, int i) {
    double t0 = nowMs();
    PopupMenu& m = A.menu;
    m.items.clear();
    m.kind = MENU_SIDE;
    m.target = i;
    const SideItem* it = sideItem(i);
    if (!it) return;
    menuAdd(L"Apri", M_SIDE_OPEN);
    menuAdd(L"Apri in nuova scheda", M_NEWTAB);
    menuSep();
    if (it->path.empty()) { // Questo PC
        menuAdd(L"Connetti unità di rete\x2026", M_NET_CONNECT);
        menuAdd(L"Aggiungi percorso di rete\x2026", M_NET_PLACE); // le parole di Windows, come la voce sopra
        menuAdd(L"Proprietà", M_SIDE_PROPS);
    } else if (it->place) {
        menuAdd(L"Proprietà", M_SIDE_PROPS);
    } else if (!it->drive) {
        menuAdd(L"Sposta su", M_SIDE_UP, i > 0);
        menuAdd(L"Sposta giù", M_SIDE_DOWN, i + 1 < (int)A.favs.size());
        menuAdd(L"Togli dai preferiti", M_SIDE_REMOVE);
    } else {
        if (it->driveType == DRIVE_REMOTE) menuAdd(L"Disconnetti", M_NET_DISCONNECT);
        menuAdd(L"Proprietà", M_SIDE_PROPS);
    }
    menuPlace(cx, cy, false, t0);
}
static void menuOpenTab(float cx, float cy, int i) {
    double t0 = nowMs();
    PopupMenu& m = A.menu;
    m.items.clear();
    m.kind = MENU_TAB;
    m.target = i;
    menuAdd(L"Nuova scheda", M_TAB_NEW);
    menuAdd(L"Nuova finestra", M_NEW_WINDOW);
    menuSep();
    menuAdd(L"Chiudi scheda", M_TAB_CLOSE);
    menuAdd(L"Chiudi le altre schede", M_TAB_OTHERS, A.tabs.size() > 1);
    menuPlace(cx, cy, false, t0);
}

// Rilascio col tasto destro: cosa fare dei file. allowed: effetti che la sorgente permette;
// preferred: quello che farebbe un trascinamento normale, già evidenziato come la voce
// predefinita di Explorer.
static void menuOpenDrop(float cx, float cy, DWORD allowed, DWORD preferred) {
    double t0 = nowMs();
    PopupMenu& m = A.menu;
    m.items.clear();
    m.kind = MENU_DROP;
    m.target = -1;
    menuAdd(L"Copia qui", M_DROP_COPY, (allowed & DROPEFFECT_COPY) != 0);
    menuAdd(L"Sposta qui", M_DROP_MOVE, (allowed & DROPEFFECT_MOVE) != 0);
    menuAdd(L"Crea collegamento qui", M_DROP_LINK, (allowed & DROPEFFECT_LINK) != 0);
    menuSep();
    menuAdd(L"Annulla", M_DROP_CANCEL);
    m.hot = preferred == DROPEFFECT_MOVE ? 1 : preferred == DROPEFFECT_LINK ? 2 : 0;
    if (!m.items[m.hot].enabled) m.hot = -1; // menuPlace sceglie la prima voce attiva
    menuPlace(cx, cy, true, t0);
}

// Spazio vuoto della barra delle schede: comandi dell'applicazione
// Dettagli ↔ Icone: la voce mostra la scorciatoia della vista in cui porta. Tutte le viste sono
// nel menu della vista, nella barra di stato (menuOpenView)
static void menuAddView() {
    if (A.zoom == ZOOM_DETAILS) menuAdd(L"Mostra come icone", M_ICONS_M);
    else menuAdd(L"Mostra come dettagli", M_VIEW_DETAILS);
}

// Menu della vista, dal suo controllo nella barra di stato: la vista attuale ha il segno di spunta
// (le icone solo se lo zoom è uno dei tre delle scorciatoie). Si apre sopra il controllo, allineato
// al suo bordo destro (cx)
static void menuOpenView(float cx, float cy) {
    double t0 = nowMs();
    PopupMenu& m = A.menu;
    m.items.clear();
    m.kind = MENU_VIEW;
    m.target = -1;
    static const struct { const wchar_t* label; int id, zoom; } kViews[] = {
        { L"Dettagli", M_VIEW_DETAILS, ZOOM_DETAILS }, { L"Elenco", M_VIEW_LIST, ZOOM_LIST },
        { L"Icone piccole", M_ICONS_S, ZOOM_SMALL }, { L"Icone medie", M_ICONS_M, ZOOM_MEDIUM }, { L"Icone grandi", M_ICONS_L, ZOOM_LARGE },
    };
    for (const auto& v : kViews) {
        menuAdd(v.label, v.id);
        if (v.zoom == A.zoom) m.items.back().icon = 3;
    }
    menuPlace(cx, cy, false, t0);
    if (m.x + m.w > cx) { m.x = std::max(px(ui::GAP_XS), floorf(cx - m.w)); } // allineato a destra anche se ci starebbe
}

// cx, cy: coordinate client; screenPt: per il menu della shell; fromKeyboard: evidenzia la prima voce
static void menuOpen(float cx, float cy, POINT screenPt, bool background, bool fromKeyboard) {
    double t0 = nowMs();
    PopupMenu& m = A.menu;
    m.items.clear();
    m.kind = MENU_LIST;
    m.target = -1;
    m.background = background;
    m.screenPt = screenPt;
    bool drives = A.cur->isDrives;
    bool zip = A.cur->zipLen != 0; // dentro uno zip: si legge e si copia, niente si modifica, niente shell
    if (!background) {
        std::vector<uint32_t> sel = selectedEntries();
        bool single = sel.size() == 1;
        bool file = single && !(A.cur->e[sel[0]].attr & FILE_ATTRIBUTE_DIRECTORY);
        menuAdd(L"Apri", M_OPEN);
        if (file && !drives && !zip) menuAdd(L"Apri con…", M_OPENWITH);
        if (file && linkOf(A.cur, sel[0])) menuAdd(L"Apri percorso file", M_LINK_OPEN); // come Explorer
        std::wstring folder = selectedFolder();
        if (!folder.empty()) {
            menuAdd(L"Apri in nuova scheda", M_NEWTAB);
            menuAdd(favIndex(folder) >= 0 ? L"Togli dai preferiti" : L"Aggiungi ai preferiti", M_FAV_SEL);
        }
        menuSep();
        if (!drives) {
            menuAdd(L"Taglia", M_CUT, !zip);
            menuAdd(L"Copia", M_COPY);
        }
        menuAdd(L"Copia percorso", M_COPYPATH);
        menuSep();
        if (!drives) {
            menuAdd(L"Rinomina", M_RENAME, single && !zip);
            menuAdd(L"Elimina", M_DELETE, !zip);
            menuSep();
        } else if (single && A.cur->e[sel[0]].size == DRIVE_REMOTE) { // per le unità size è il tipo
            menuAdd(L"Disconnetti", M_NET_DISCONNECT);
            menuSep();
        }
    } else if (!drives) {
        menuAdd(L"Incolla", M_PASTE, !zip && clipHasFiles());
        menuAdd(undoLabel(false), M_UNDO, undoCan(false)); // dopo Incolla, come in Explorer
        if (undoCan(true)) menuAdd(undoLabel(true), M_REDO); // Ripeti solo quando c'è, come in Explorer
        menuAdd(L"Nuova cartella", M_NEWFOLDER, !zip);
        menuAdd(L"Aggiorna", M_REFRESH);
        menuAddView();
        menuSep();
        menuAdd(favIndex(A.cur->path) >= 0 ? L"Togli dai preferiti" : L"Aggiungi ai preferiti", M_FAV_TOGGLE);
        menuAdd(L"Copia percorso", M_COPYPATH);
        if (!zip) menuAdd(L"Apri terminale qui", M_TERMINAL);
        menuSep();
    } else {
        menuAdd(L"Aggiorna", M_REFRESH);
        menuAddView();
        menuSep();
        menuAdd(L"Connetti unità di rete\x2026", M_NET_CONNECT);
        menuAdd(L"Aggiungi percorso di rete\x2026", M_NET_PLACE);
        menuSep();
    }
    if (zip) { // la shell non vede dentro lo zip: niente Proprietà né menu di Windows
        if (!m.items.empty() && !m.items.back().id) m.items.pop_back(); // l'ultimo separatore
        menuPlace(cx, cy, fromKeyboard, t0);
        return;
    }
    if (!background || !drives) menuAdd(L"Proprietà", M_PROPS);
    menuAdd(L"Altre opzioni…", M_MORE, true, A.shellMenu ? L"" : nullptr); // con l'opzione Shift+F10 apre questo menu
    menuPlace(cx, cy, fromKeyboard, t0);
}

static void menuPlace(float cx, float cy, bool fromKeyboard, double t0) {
    PopupMenu& m = A.menu;
    m.ih = A.rowH;
    m.sepH = px(ui::MENU_SEP);
    m.pad = px(ui::MENU_PAD_X);
    m.vpad = px(ui::MENU_PAD_Y);
    float lw = 0, kw = 0, h = m.vpad * 2;
    bool icons = false, seps = false;
    for (const MenuItem& it : m.items) {
        if (!it.id) { h += m.sepH; seps = true; continue; }
        lw = std::max(lw, (float)textWidth(it.label, (int)wcslen(it.label)));
        kw = std::max(kw, (float)textWidth(it.keys, (int)wcslen(it.keys)));
        if (it.icon) icons = true;
        h += m.ih;
    }
    m.iconW = icons ? (float)I.size + px(ui::GAP_M) : 0;
    m.w = floorf(std::min((float)G.w - px(ui::GAP_S), lw + kw + m.iconW + m.pad * 2 + (kw > 0 ? px(ui::MENU_KEYS_GAP) : 0)));
    // Troppe voci per la finestra: si mostrano quelle che stanno e si scorre (voce per voce)
    m.first = 0;
    m.visible = (int)m.items.size();
    float below = G.h - px(ui::GAP_S) - cy, above = cy - px(ui::GAP_S);
    if (!seps && h > std::max(below, above)) {
        float avail = std::max(below, above);
        m.visible = std::max(1, (int)((avail - m.vpad * 2) / m.ih));
        h = m.visible * m.ih + m.vpad * 2;
    }
    m.h = h;
    // Dentro la finestra: se non c'è spazio sotto si apre verso l'alto, a destra verso sinistra
    float edge = px(ui::GAP_XS); // mai a filo del bordo della finestra
    m.x = cx + m.w > G.w - edge ? std::max(edge, cx - m.w) : cx;
    m.y = cy + m.h > G.h - edge ? std::max(edge, cy - m.h) : cy;
    m.x = floorf(m.x);
    m.y = floorf(m.y);
    if (!fromKeyboard) m.hot = -1;
    if (fromKeyboard && m.hot < 0)
        for (int i = 0; i < (int)m.items.size(); i++) if (m.items[i].id && m.items[i].enabled) { m.hot = i; break; }
    menuEnsureVisible();
    m.open = true;
    m.openedAt = nowMs();
    m.pressed = false;
    A.hover = -1;
    A.dirty = true;
    A.ourMenuMs = nowMs() - t0;
}

static bool menuInside(int x, int y) {
    const PopupMenu& m = A.menu;
    return x >= m.x && x < m.x + m.w && y >= m.y && y < m.y + m.h;
}

// Indice della voce sotto il punto (solo voci attive), -1 altrimenti
static int menuHit(int x, int y) {
    const PopupMenu& m = A.menu;
    if (!menuInside(x, y)) return -1;
    float yy = m.y + m.vpad;
    for (int i = m.first; i < m.first + m.visible && i < (int)m.items.size(); i++) {
        float hh = m.items[i].id ? m.ih : m.sepH;
        if (y >= yy && y < yy + hh) return (m.items[i].id && m.items[i].enabled) ? i : -1;
        yy += hh;
    }
    return -1;
}

// "Nuova finestra": p è new std::wstring[2]{ eseguibile, riga di comando }
static DWORD WINAPI newWindowThread(void* p) {
    std::wstring* a = (std::wstring*)p;
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    if (CreateProcessW(a[0].c_str(), &a[1][0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    delete[] a;
    return 0;
}

// Un comando, da una voce di menu o da un tasto di kShortcuts (onKey). Il contesto dice su cosa
// agisce: kind e target la voce della barra laterale o la scheda del menu, bg lo sfondo della
// lista, pt il punto del clic. Da tastiera: la selezione e la scheda attiva.
struct CmdCtx { int kind = MENU_LIST, target = -1; bool bg = false; POINT pt = { -1, -1 }; };

static void menuRun(int id, const CmdCtx& c) {
    int kind = c.kind, target = c.target;
    bool bg = c.bg;
    POINT pt = c.pt;
    const SideItem* si = kind == MENU_SIDE ? sideItem(target) : nullptr;
    std::wstring sidePath = si ? si->path : L"";
    switch (id) {
    case M_NEWTAB: {
        std::wstring f = kind == MENU_SIDE ? sidePath : selectedFolder();
        if (!f.empty() || (kind == MENU_SIDE && si)) tabNew(f, false); // "" dalla barra laterale: Questo PC
        break;
    }
    case M_NET_CONNECT: netStart(NET_CONNECT, L""); break;
    case M_NET_PLACE: netStart(NET_PLACE, L""); break;
    case M_NET_DISCONNECT: {
        std::wstring d = sidePath;
        if (kind != MENU_SIDE) {
            std::vector<uint32_t> sel = selectedEntries();
            d = sel.size() == 1 ? std::wstring(A.cur->name(A.cur->e[sel[0]])) : L"";
        }
        netStart(NET_DISCONNECT, d);
        break;
    }
    case M_FAV_TOGGLE: if (A.cur) favToggle(A.cur->path); break;
    case M_FAV_SEL:    favToggle(selectedFolder()); break;
    case M_SIDE_OPEN:   if (si) navigate(sidePath, 1, L""); break;
    case M_SIDE_UP:     favMove(target, -1); break;
    case M_SIDE_DOWN:   favMove(target, 1); break;
    case M_SIDE_REMOVE: if (si && !si->drive) favToggle(sidePath); break;
    case M_SIDE_PROPS:
        if (si && sidePath.empty()) thisPcProperties();
        else if (si) shellVerbPath("properties", sidePath, si->label); // unità o percorso di rete
        break;
    case M_TAB_NEW:     tabNew(A.cur ? A.cur->path : L"", true); break;
    case M_TAB_CLOSE:   tabClose(target); break;
    case M_OPTIONS:     optionsOpen(); break;
    case M_GOTO:        gotoOpen(); break;
    case M_PREVIEW:     previewToggle(); break;
    case M_ICONS_S:       viewZoom(ZOOM_SMALL); break;
    case M_ICONS_M:       viewZoom(ZOOM_MEDIUM); break;
    case M_ICONS_L:       viewZoom(ZOOM_LARGE); break;
    case M_VIEW_LIST:    viewZoom(ZOOM_LIST); break;
    case M_VIEW_DETAILS: viewZoom(ZOOM_DETAILS); break;
    case M_NEW_WINDOW: { // un'altra finestra di [z]f sulla stessa cartella
        std::wstring exe = exePath(), args = L"\"" + exe + L"\" --new-window";
        if (A.cur && !A.cur->path.empty()) {
            std::wstring p = A.cur->path;
            if (p.back() == L'\\') p += L'\\'; // "C:\" tra virgolette diventerebbe C:" per la riga di comando
            args += L" \"" + p + L"\"";
        }
        // Su un thread: creare un processo (con l'antivirus che lo esamina) può costare decine di ms
        HANDLE h = CreateThread(nullptr, 0, newWindowThread, new std::wstring[2]{ exe, args }, 0, nullptr);
        if (h) CloseHandle(h);
        break;
    }
    case M_TAB_OTHERS:
        tabActivate(target);
        for (int k = (int)A.tabs.size() - 1; k >= 0; k--) if (k != A.tab) tabClose(k);
        break;
    case M_OPEN:      openSelected(); break;
    case M_OPENWITH:  openWith(); break;
    case M_LINK_OPEN: {
        std::vector<uint32_t> sel = selectedEntries();
        if (const LinkTarget* t = sel.size() == 1 ? linkOf(A.cur, sel[0]) : nullptr) linkReveal(t->path, false);
        break;
    }
    case M_CUT:       shellVerb("cut"); break;
    case M_COPY:      shellVerb("copy"); break;
    case M_PASTE:     pasteFiles(); break;
    case M_UNDO:      undoRun(false); break;
    case M_REDO:      undoRun(true); break;
    case M_COPYPATH:  copyPaths(bg); break;
    case M_RENAME:    beginRename(); break;
    case M_DELETE:    deleteSelected(); break;
    case M_PROPS:     showProperties(bg); break;
    case M_MORE:      showContextMenu(pt, bg); break;
    case M_NEWFOLDER: newFolder(); break;
    case M_REFRESH:   refreshNow(); break;
    case M_TERMINAL:  openTerminal(); break;
    // Comandi senza voce di menu: solo da tastiera (e dai pulsanti, che chiamano le stesse funzioni)
    case M_BACK:      goBack(); break;
    case M_FWD:       goForward(); break;
    case M_UP:        goUp(); break;
    case M_EDIT_PATH: gotoOpenHere(); break;
    case M_SIDEBAR:   sidebarToggle(); break;
    }
    A.dirty = true;
}

static void menuExec(int i) {
    PopupMenu& m = A.menu;
    if (i < 0 || i >= (int)m.items.size() || !m.items[i].id || !m.items[i].enabled) return;
    int id = m.items[i].id;
    CmdCtx c;
    c.bg = m.background;
    c.pt = m.screenPt;
    c.target = m.target;
    c.kind = m.kind;
    std::wstring itemPath = (c.kind == MENU_CRUMB && i < (int)m.paths.size()) ? m.paths[i] : L"";
    menuClose();
    if (c.kind == MENU_CRUMB) { if (!itemPath.empty()) navigate(itemPath, 1, L""); return; }
    if (c.kind == MENU_DROP) { g_dropChoice = id; return; } // lo esegue chi ha aperto il menu (drag.cpp)
    menuRun(id, c);
}

static void menuKey(WPARAM vk) {
    PopupMenu& m = A.menu;
    int n = (int)m.items.size();
    if (vk == VK_UP || vk == VK_DOWN) {
        int d = vk == VK_DOWN ? 1 : -1, i = m.hot;
        for (int k = 0; k < n; k++) {
            i = (i + d + n) % n;
            if (m.items[i].id && m.items[i].enabled) { m.hot = i; break; }
        }
        menuEnsureVisible();
        A.dirty = true;
    } else if (vk == VK_PRIOR || vk == VK_NEXT || vk == VK_HOME || vk == VK_END) {
        int step = vk == VK_PRIOR ? -m.visible : vk == VK_NEXT ? m.visible : vk == VK_HOME ? -n : n;
        int i = std::max(0, std::min(n - 1, (m.hot < 0 ? 0 : m.hot) + step));
        if (m.items[i].id && m.items[i].enabled) m.hot = i;
        menuEnsureVisible();
        A.dirty = true;
    } else if (vk == VK_RETURN || vk == VK_SPACE) {
        menuExec(m.hot);
    } else {
        menuClose(); // Esc o qualsiasi altro tasto
    }
}

// Tasto destro, tasto Menu o Shift+F10. pt in coordinate schermo; (-1,-1) = da tastiera.
// Con Shift si va dritti al menu completo della shell, come in Windows 11. Con l'opzione "Menu
// completo di Windows" è il contrario: il tasto destro apre quello della shell, Shift il nostro.
static void onContextMenu(POINT pt) {
    if (A.renaming) return;
    if (pt.x != -1 || pt.y != -1) { // schede e barra laterale hanno i loro menu
        POINT c = pt;
        ScreenToClient(A.hwnd, &c);
        if (A.menu.open && menuInside(c.x, c.y)) return;
        bool closeHit, plusHit;
        int t = tabHit(c.x, c.y, &closeHit, &plusHit);
        if (t >= 0) { menuOpenTab((float)c.x, (float)c.y, t); return; }
        if (c.y < A.pathY) return; // ⚙ e "+": niente menu (lo spazio vuoto ha quello di sistema)
        int si = sideHit(c.x, c.y);
        if (si == -2) si = (int)A.favs.size(); // la freccia è sulla riga di Questo PC: il suo menu
        if (si >= 0) { menuOpenSide((float)c.x, (float)c.y, si); return; }
    }
    if (!A.cur) return;
    bool shift = GetKeyState(VK_SHIFT) < 0;
    bool keyboard = pt.x == -1 && pt.y == -1;
    POINT c;
    bool background;
    if (keyboard) {
        background = A.view.empty();
        if (!background) ensureVisible();
        Box b = background ? Box{ A.listX, 0, 0, A.rowH } : itemRect(A.sel);
        float ox, oy;
        scrollOffset(ox, oy);
        if (background) ox = oy = 0;
        // Dettagli ed Elenco: poco dentro la riga (in Elenco, la sua colonna); Icone: a metà della voce
        c.x = background || A.zoom == ZOOM_DETAILS ? (LONG)(A.listX + px(ui::MENU_KEYBOARD_X))
            : listView() ? (LONG)(b.x - ox + px(ui::MENU_KEYBOARD_X)) : (LONG)(b.x + b.w * 0.5f);
        c.y = (LONG)(A.listTop + b.y + b.h - oy);
        pt = c;
        ClientToScreen(A.hwnd, &pt);
    } else {
        c = pt;
        ScreenToClient(A.hwnd, &c);
        if (A.menu.open && menuInside(c.x, c.y)) return;
        if (c.y < A.listTop || c.y >= A.listTop + A.listH || c.x < A.listX || c.x >= itemsRight()) return;
        int r = itemAt(c.x, c.y);
        background = r < 0;
        if (!background) {
            if (!isSel(A.view[r])) moveCursorTo(r, false, false); // fuori dalla selezione: seleziona quella voce
            else A.sel = r;
        }
    }
    if (shift != A.shellMenu && !A.cur->zipLen) { menuClose(); showContextMenu(pt, background); } // l'opzione inverte il ruolo di Shift; dentro uno zip c'è solo il nostro
    else menuOpen((float)c.x, (float)c.y, pt, background, keyboard);
}
