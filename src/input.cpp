// [z]f — tastiera, mouse e messaggi della finestra.

// La casella a cui torna un risultato degli appunti, se è ancora aperta
static TextField* clipField(void* target) {
    if (target == &A.renameField && A.renaming) return &A.renameField;
    if (target == &A.pathField && A.editingPath) return &A.pathField;
    if (target == &GT.field && gotoIsOpen()) return &GT.field;
    if (target == &OP.search && OP.open) return &OP.search;
    return nullptr;
}

// Il testo della casella è cambiato senza un tasto: "Vai a" e le opzioni rifanno la ricerca
static void clipFieldChanged(TextField* f) {
    if (f == &GT.field) gotoSearch(false);
    else if (f == &OP.search) OP.scroll = 0;
}

// Testo degli appunti arrivato dal thread (clipAskText): alla casella che l'ha chiesto, se è ancora
// aperta; nullptr: Ctrl+V nella lista
static void onClipText(void* target, const std::wstring& t) {
    if (!target) { smartPasteText(t); return; }
    if (t.empty()) return;
    if (TextField* f = clipField(target)) {
        f->insert(t);
        clipFieldChanged(f);
    }
}

// Esito della scrittura negli appunti (clipSetText). Se non è riuscita, un Taglia rimette il testo
// nella casella, selezionato, purché la casella sia ancora aperta e com'era dopo il taglio
static void onClipSet(ClipSet* c) {
    if (c->skipped) return; // una copia più recente l'ha sostituita
    if (c->ok) {
        if (c->done) notice(c->done, true);
        return;
    }
    TextField* f = clipField(c->target);
    if (f && f->text == c->after && c->at <= (int)f->text.size()) {
        f->text.insert((size_t)c->at, c->text);
        f->anchor = c->at;
        f->caret = c->at + (int)c->text.size();
        clipFieldChanged(f);
    }
    std::wstring m = c->fail;
    if (!c->holder.empty()) m += L": gli appunti sono aperti da " + c->holder;
    else if (c->err == ERROR_ACCESS_DENIED) m += L": gli appunti sono aperti da un altro programma";
    if (c->err) m += L" (codice " + std::to_wstring(c->err) + L")";
    notice(m, false);
}

static void onKey(WPARAM vk, bool alt) {
    bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
    if (vk == VK_ESCAPE && (mqCancel() || colDragEnd(false) || favMouseUp(false, false) || favDisarm())) return; // rettangolo, bordo, preferito trascinato: si torna a prima; cestino: niente conferma
    if (A.menu.open) { menuKey(vk); return; }
    if (gotoIsOpen() && alt) return; // Alt+frecce non navigano sotto il pannello
    if (gotoKey(vk, ctrl, shift)) return;
    if (optionsKey(vk, ctrl, shift)) return;
    if (A.renaming) {
        if (vk == VK_RETURN) endRename(true);
        else if (vk == VK_ESCAPE) endRename(false);
        else A.renameField.key(A.hwnd, vk, ctrl, shift);
        A.dirty = true;
        return;
    }
    if (A.editingPath) {
        if (vk == VK_RETURN) {
            A.editingPath = false;
            std::wstring p = normalizePath(A.pathField.text);
            if (!p.empty()) navigate(p, 1, L"");
        } else if (vk == VK_ESCAPE) {
            A.editingPath = false;
        } else {
            A.pathField.key(A.hwnd, vk, ctrl, shift);
        }
        A.dirty = true;
        return;
    }
    // I tasti che eseguono solo un comando vengono dalla tabella delle scorciatoie (kShortcuts), con
    // esattamente quei modificatori, ed eseguono lo stesso codice della voce di menu; qui sotto
    // restano quelli che dipendono dal contesto
    int mods = (ctrl ? KC : 0) | (shift ? KS : 0) | (alt ? KA : 0);
    if (int cmd = shortcutCmd((UINT)vk, mods)) {
        CmdCtx c;
        c.target = A.tab; // Ctrl+W: la scheda attiva
        menuRun(cmd, c);
        return;
    }
    if (alt) return;
    // In griglia: ← → una voce, ↑ ↓ una riga (dalla prima o dall'ultima riga non si esce), pagine di righe intere
    int page = A.grid ? std::max(1, (int)(A.listH / A.cellH)) * A.cols : std::max(1, (int)(A.listH / A.cellH) - 1);
    int lastRow = A.view.empty() ? 0 : ((int)A.view.size() - 1) / A.cols;
    switch (vk) {
    case VK_UP:     if (A.sel >= A.cols) moveSel(-A.cols); break;
    case VK_DOWN:   if (A.sel / A.cols < lastRow) moveSel(A.cols); break;
    case VK_LEFT:   if (A.grid) moveSel(-1); break;
    case VK_RIGHT:  if (A.grid) moveSel(1); break;
    case VK_PRIOR:  moveSel(-page); break;
    case VK_NEXT:   moveSel(page); break;
    case VK_HOME:   moveSel(-(int)A.view.size()); break;
    case VK_END:    moveSel((int)A.view.size()); break;
    case VK_BACK:
        if (!A.filter.empty()) { A.filter.pop_back(); rebuildView(true); A.dirty = true; }
        else goUp();
        break;
    case VK_ESCAPE: // prima toglie il filtro, poi la selezione (resta solo il cursore)
        if (!A.filter.empty()) { A.filter.clear(); rebuildView(true); }
        else { selOnly(A.sel); }
        A.dirty = true;
        break;
    case VK_SPACE:
        if (ctrl && A.sel < (int)A.view.size()) { // Ctrl+Spazio: alterna la voce sotto il cursore
            uint32_t ei = A.view[A.sel];
            A.selFlags[ei] = !A.selFlags[ei];
            A.anchor = (int)ei;
            A.dirty = true;
        }
        break;
    case VK_APPS:
        onContextMenu(POINT{ -1, -1 });
        break;
    case VK_TAB: // Ctrl+Tab, Ctrl+Shift+Tab: scheda successiva / precedente
        if (ctrl && A.tabs.size() > 1) {
            int n = (int)A.tabs.size();
            tabActivate((A.tab + (shift ? n - 1 : 1)) % n);
        }
        break;
    case VK_F12:
        A.showStats = !A.showStats;
        A.statsOpenedAt = nowMs();
        A.execMax = A.gpuMax = A.prepMax = A.presentMax = 0;
        A.slowMsgMs = A.waitMax = A.dropUiMax = A.mqMax = A.crumbMax = A.th.texMax = A.th.syncMax = A.th.cellsMax = 0; A.slowMsg = 0; A.slowMsgCount = 0;
        A.dirty = true;
        break;
    default:
        if (ctrl && vk >= '1' && vk <= '9') tabActivate(vk == '9' ? (int)A.tabs.size() - 1 : (int)(vk - '1'));
        else if (ctrl && (vk == VK_OEM_PLUS || vk == VK_ADD)) fontSizeStep(1);
        else if (ctrl && (vk == VK_OEM_MINUS || vk == VK_SUBTRACT)) fontSizeStep(-1);
        else if (ctrl && (vk == '0' || vk == VK_NUMPAD0)) fontSizeStep(0);
        else if (ctrl && vk == 'A') { for (uint32_t ei : A.view) A.selFlags[ei] = 1; A.dirty = true; }
        else if (ctrl && vk == 'V') smartPaste();
        break;
    }
}

// Modificatori di un tasto premuto con Alt (WM_SYSKEYDOWN, WM_SYSCHAR), come li vuole shortcutCmd
static int altMods() {
    return KA | (GetKeyState(VK_CONTROL) < 0 ? KC : 0) | (GetKeyState(VK_SHIFT) < 0 ? KS : 0);
}

static void onChar(wchar_t c) {
    if (c < 32 || c == 127 || A.menu.open) return;
    if (GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) >= 0) return; // scorciatoie Ctrl
    if (gotoChar(c)) return;
    if (optionsChar(c)) return;
    if (A.renaming) A.renameField.insert(std::wstring(1, c));
    else if (A.editingPath) A.pathField.insert(std::wstring(1, c));
    else if (A.cur) { A.filter += c; rebuildView(false); }
    A.dirty = true;
}

static void onDpi(UINT dpi) {
    A.scale = dpi / 96.0f;
    fontCreate(A.scale);
    iconSetSize((int)lroundf(ui::LIST_ICON * A.scale));
    layout(); // in griglia il riquadro delle miniature cambia: si rifanno alla misura nuova
    if (A.sideStarted) appIconSet(); // prima del primo frame la carica main.cpp
    A.dirty = true;
}

static LRESULT CALLBACK wndProcImpl(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

// × del campo del filtro nella barra di stato (c'è solo mentre si filtra: render la misura)
static bool fltClearAt(int x, int y) {
    return A.fltClearW > 0 && y >= G.h - A.statusH && x >= A.fltX + A.fltW - A.fltClearW && x < A.fltX + A.fltW;
}

// Rotellina per gli elenchi che scorrono a righe intere (menu, "Vai a"). I touchpad di
// precisione e le rotelle libere mandano frazioni di scatto: sommate, arrivano a una riga invece
// di perdersi una per una nell'arrotondamento.
static int wheelSteps(float lines) {
    static float acc = 0;
    acc += lines;
    int n = (int)acc; // verso lo zero: il resto resta per il messaggio successivo
    acc -= (float)n;
    return n;
}

// Misura quanto tempo passa in ogni messaggio gestito dalla finestra principale.
// Esclusi quelli che entrano nei cicli modali di Windows (spostamento, ridimensionamento, menu):
// lì dentro il tempo è dell'utente, non nostro.
static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool modal = msg == WM_NCLBUTTONDOWN || msg == WM_SYSCOMMAND || msg == WM_NCLBUTTONDBLCLK ||
                 msg == WM_NCRBUTTONDOWN || msg == WM_ENTERSIZEMOVE || msg == WM_ENTERMENULOOP;
    double t0 = modal ? 0 : nowMs();
    int shellBefore = g_shellCalls;
    LRESULT r = wndProcImpl(hwnd, msg, wp, lp);
    if (g_shellCalls != shellBefore) modal = true; // menu o dialogo della shell: tempo dell'utente
    if (!modal) {
        double d = nowMs() - t0;
        if (d > A.slowMsgMs) { A.slowMsgMs = d; A.slowMsg = msg; }
        if (d > ui::LOG_MSG_F12) A.slowMsgCount++;
        if (d > ui::LOG_MSG) hitchLog("messaggio lento", msg, d);
    }
    return r;
}

static LRESULT CALLBACK wndProcImpl(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) ||
        msg == WM_NCLBUTTONDOWN)
        A.lastInputAt = nowMs();
    // Un clic o un tasto (non un modificatore tenuto premuto) mentre il menu della shell si costruisce
    if (msg == WM_LBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_NCLBUTTONDOWN ||
        (msg == WM_KEYDOWN && !(lp & (1 << 30)) && wp != VK_SHIFT && wp != VK_CONTROL && wp != VK_MENU))
        shellMenuCancel();
    if (msg == g_msgRestart && msg) { restartBegin(false); return 0; } // un'altra finestra ha cliccato "Aggiorna"
    switch (msg) {
    case WM_COPYDATA: // cartella aperta da una seconda istanza di [z]f
        return onCopyData((const COPYDATASTRUCT*)lp);
    case WM_NCCALCSIZE:
        if (wp) return chromeNcCalcSize(hwnd, wp, lp);
        break;
    case WM_NCHITTEST:
        return chromeNcHitTest(hwnd, wp, lp);
    case WM_NCMOUSEMOVE: case WM_NCMOUSELEAVE: case WM_NCLBUTTONDOWN: case WM_NCLBUTTONUP: case WM_NCLBUTTONDBLCLK:
        if (chromeNcMouse(hwnd, msg, wp)) return 0;
        break;
    case WM_SETCURSOR: // sul bordo tra due colonne: cursore ↔ (la finestra figlia è trasparente al mouse)
        if (LOWORD(lp) == HTCLIENT && !A.menu.open && !OP.open && !gotoIsOpen()) {
            POINT c;
            GetCursorPos(&c);
            ScreenToClient(hwnd, &c);
            if (A.colDrag >= 0 || colBorderAt(c.x, c.y) >= 0 || panelEdgeAt(c.x, c.y) >= 0) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return TRUE;
            }
            // Destinazione di un collegamento: la manina dice che è un collegamento da seguire
            // (non durante il rettangolo o un trascinamento, quando conta il gesto in corso)
            if (!MQ.active && !DD.armed && linkHit(c.x, c.y) >= 0) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        break;
    case WM_CAPTURECHANGED: // un'altra finestra ha preso il mouse: rettangolo e bordo si fermano dove sono
        if ((HWND)lp != hwnd) { mqEnd(); colDragEnd(true); favMouseUp(false, false); zipDragCancel(); }
        break;
    case WM_CONTEXTMENU:
        if (OP.open || gotoIsOpen()) return 0;
        onContextMenu(POINT{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) });
        return 0;
    case WM_APP_FSDONE:
        onFsDone((DWORD)wp, (FsJob*)lp);
        return 0;
    case WM_APP_LISTING:
        onListing((Listing*)lp);
        return 0;
    case WM_APP_GPU:
        onGpuReady();
        return 0;
    case WM_APP_CRUMB:
        onCrumbResult((CrumbRes*)lp);
        return 0;
    case WM_APP_INFO:
        onTipInfo((TipInfo*)lp);
        return 0;
    case WM_APP_SIDEBAR:
        onSideLoaded((SideLoad*)lp);
        return 0;
    case WM_APP_SIDESLOW:
        onSideSlow((SlowItem*)lp);
        return 0;
    case WM_APP_START:
        onStartTabs((StartTabs*)lp);
        return 0;
    case WM_APP_UPDATE:
        onUpdate((UpdJob*)lp);
        return 0;
    case WM_APP_UPDCLEAN:
        onUpdClean();
        return 0;
    case WM_APP_NET:
        onNetDone((NetJob*)lp);
        return 0;
    case WM_APP_OPENTAB: {
        std::wstring* p = (std::wstring*)lp;
        tabNew(*p, true);
        delete p;
        return 0;
    }
    case WM_APP_DIRSIZE:
        onDirSize();
        return 0;
    case WM_APP_CLIPTEXT: {
        ClipText* c = (ClipText*)lp;
        if (c->gen == g_clipGen) onClipText(c->target, c->text); // una richiesta più recente la sostituisce
        delete c;
        A.dirty = true;
        return 0;
    }
    case WM_APP_CLIPSET: {
        ClipSet* c = (ClipSet*)lp;
        onClipSet(c);
        delete c;
        A.dirty = true;
        return 0;
    }
    case WM_APP_SHELLMENU:
        onShellMenu((SmRes*)lp);
        return 0;
    case WM_APP_ZIP:
        onZipJob((ZipJob*)lp);
        return 0;
    case WM_APP_LINKS:
        onLinks();
        return 0;
    case WM_APP_DEFAULT:
        onDefaultDone(wp != 0, lp != 0);
        if (OP.open) OP.defaultState = defaultState();
        A.dirty = true;
        return 0;
    case WM_APP_DROPDONE:
        onShellDrop((ShellDrop*)lp);
        return 0;
    case WM_APP_FILEOP:
        onFileJob((FileJob*)lp);
        return 0;
    case WM_APP_FAVDROP:
        onFavDrop((FavDrop*)lp);
        return 0;
    case WM_APP_GOTO:
        onGotoDir((GoDirRes*)lp);
        return 0;
    case WM_APP_PREVIEW:
        onPreviewResult((PvRes*)lp);
        return 0;
    case WM_APP_THUMB:
        onThumbResults();
        return 0;
    case WM_DEVICECHANGE: // unità collegata o scollegata, disco inserito o tolto
        if (wp == 0x8000 /*DBT_DEVICEARRIVAL*/ || wp == 0x8004 /*DBT_DEVICEREMOVECOMPLETE*/) {
            sideLoad(true);
        }
        return TRUE;
    case WM_ACTIVATEAPP: // tornando su [z]f lo spazio libero può essere cambiato (al massimo ogni 5 s)
        if (wp && nowMs() - A.sideLoadedAt > ui::SIDE_RELOAD) sideLoad();
        if (wp) fdActivated(); // i dialoghi Apri e Salva guardano questa finestra
        break;
    case WM_ENTERSIZEMOVE:
        A.inSizeMove = true;
        break;
    case WM_EXITSIZEMOVE:
        A.inSizeMove = false;
        A.dirty = true; // l'anteprima di un'immagine si rifà alla misura finale
        break;
    case WM_APP_DIRCHANGED:
        onDirChanged((uint64_t)wp, (int)lp);
        return 0;
    case WM_APP_ICON:
        onIconResults();
        return 0;
    case WM_APP_DIRMOVED:
        onDirMoved((uint64_t)wp, (wchar_t*)lp);
        return 0;
    case WM_SIZE:
        menuClose();
        if (A.viewWnd) MoveWindow(A.viewWnd, 0, 0, LOWORD(lp), HIWORD(lp), FALSE);
        gfxResize(LOWORD(lp), HIWORD(lp));
        if (G.swap && LOWORD(lp) > 0) { A.dirty = true; render(); } // ridimensionamento fluido
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        A.dirty = true;
        return 0;
    }
    case WM_ERASEBKGND:
        if (!G.swap) break; // prima di D3D: sfondo del tema disegnato da GDI
        return 1;
    case WM_DPICHANGED: {
        RECT* r = (RECT*)lp;
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        onDpi(HIWORD(wp));
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = (LONG)(ui::WINDOW_MIN_W * A.scale);
        mm->ptMinTrackSize.y = (LONG)(ui::WINDOW_MIN_H * A.scale);
        return 0;
    }
    case WM_TIMER:
        if (wp == 5) { tipOnTimer(); return 0; }
        if (wp == 7) { crumbOnTimer(); return 0; }
        if (wp == 8) { dragTick(); return 0; }
        if (wp == 10) { mqTick(); return 0; }
        if (wp == 11 || wp == 12) { previewOnTimer(wp); return 0; }
        if (wp == 6) { if (T.waiting) tipShow(); KillTimer(hwnd, 6); return 0; }
        if (wp == 1) { A.error.clear(); KillTimer(hwnd, 1); }
        if (wp == 2) KillTimer(hwnd, 2);
        if (wp == 9) KillTimer(hwnd, 9); // "Vai a": cartella lenta da leggere, si mostra "caricamento…"
        if (wp == 3) { KillTimer(hwnd, 3); A.changePending = false; requestRefresh(); }
        if (wp == 4) { KillTimer(hwnd, 4); onGpuReady(); }
        if (wp == 13) { KillTimer(hwnd, 13); gpuRecoverStart(); return 0; } // GPU persa: nuovo tentativo
        if (wp == 14) { zipOnTimer(); return 0; } // estrazione da uno zip che dura
        if (wp == 15) { updOnTimer(); return 0; } // aggiornamento: un controllo al giorno
        A.dirty = true;
        return 0;
    case WM_KEYDOWN:
        tipHide();
        onKey(wp, false);
        return 0;
    case WM_SYSKEYDOWN:
        tipHide();
        // Alt+frecce, Alt+Invio, Alt+P (il riquadro di anteprima di Explorer)…: quelli della tabella.
        // Gli altri (Alt+F4, Alt+Spazio) restano a Windows.
        if (shortcutCmd((UINT)wp, altMods())) { onKey(wp, true); return 0; }
        break;
    case WM_SYSCHAR: // un tasto Alt della tabella: niente segnale acustico di Windows
        if (wp < 128 && shortcutCmd((UINT)towupper((wint_t)wp), altMods())) return 0;
        break;
    case WM_CHAR:
        onChar((wchar_t)wp);
        return 0;
    case WM_MOUSEWHEEL: {
        tipHide();
        float lines = (float)GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
        if (A.menu.open && A.menu.visible < (int)A.menu.items.size()) { // menu che scorre
            POINT c = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &c);
            if (menuInside(c.x, c.y)) {
                menuScroll(-wheelSteps(lines * ui::WHEEL_LINES));
                A.menu.hot = menuHit(c.x, c.y);
                return 0;
            }
        }
        menuClose();
        if (gotoIsOpen()) {
            POINT c = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &c);
            gotoWheel(c.x, c.y, wheelSteps(lines));
            return 0;
        }
        if (OP.open && !(GET_KEYSTATE_WPARAM(wp) & MK_CONTROL)) { optionsWheel(lines); return 0; }
        if (GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) { // Ctrl+rotellina: dimensione del testo, in griglia delle miniature
            A.wheelAcc += GET_WHEEL_DELTA_WPARAM(wp);
            while (A.wheelAcc >= WHEEL_DELTA) { A.wheelAcc -= WHEEL_DELTA; if (A.grid) thumbSizeStep(1); else fontSizeStep(1); }
            while (A.wheelAcc <= -WHEEL_DELTA) { A.wheelAcc += WHEEL_DELTA; if (A.grid) thumbSizeStep(-1); else fontSizeStep(-1); }
            return 0;
        }
        POINT c = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }; // coordinate schermo
        ScreenToClient(hwnd, &c);
        if (previewWheel(c.x, c.y, lines)) return 0; // sopra l'anteprima: scorre quella
        if (c.x < A.sideW && c.y >= A.sideTop) { // sopra la barra laterale: scorre quella
            A.sideScroll = std::min(sideMaxScroll(), std::max(0.0f, A.sideScroll - lines * A.sideItemH * ui::WHEEL_LINES));
            A.sideHover = sideHit(c.x, c.y);
            A.dirty = true;
            return 0;
        }
        A.scrollTarget -= lines * (A.grid ? A.cellH : A.rowH * ui::WHEEL_LINES); // griglia: una riga di voci a scatto
        clampScroll();
        A.dirty = true;
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (A.menu.open) {
            int h = menuHit(x, y);
            if (h != A.menu.hot) { A.menu.hot = h; A.dirty = true; }
            return 0;
        }
        if (!A.trackingMouse) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            A.trackingMouse = true;
        }
        if (A.colDrag >= 0) { colDragMove(x); return 0; }
        if (favMouseMove(x, y)) return 0;
        if (gotoMouseMove(x, y)) { tipMouseMove(x, y); return 0; }
        if (optionsMouseMove(x, y)) return 0;
        if (dragCheck(x, y, wp)) return 0;
        if (mqMouseMove(x, y, wp)) return 0;
        int cb = colBorderAt(x, y);
        if (cb != A.colHover) { A.colHover = cb; A.dirty = true; }
        if (A.draggingThumb) {
            float ty, th;
            if (thumbRect(ty, th)) {
                float t = (y - A.listTop - A.dragOffset) / (A.listH - th);
                A.scrollY = A.scrollTarget = t * maxScroll();
                clampScroll();
                A.dirty = true;
            }
            return 0;
        }
        int h = itemAt(x, y);
        if (h != A.hover) { A.hover = h; A.dirty = true; }
        int lh = linkHit(x, y);
        if (lh != A.linkHover) { A.linkHover = lh; A.dirty = true; }
        bool closeHit, plusHit;
        int t = tabHit(x, y, &closeHit, &plusHit);
        if (t != A.tabHover || closeHit != A.tabCloseHover || plusHit != A.tabPlusHover) {
            A.tabHover = t; A.tabCloseHover = closeHit; A.tabPlusHover = plusHit; A.dirty = true;
        }
        int sh = sideHit(x, y);
        if (sh != A.sideHover) { A.sideHover = sh; A.dirty = true; }
        int ft = favTrashAt(x, y);
        if (ft != A.favTrashHover) { A.favTrashHover = ft; A.dirty = true; }
        if (A.favTrashArmed >= 0 && sh != A.favTrashArmed) favDisarm(); // uscendo dalla voce la conferma si annulla
        int ch = crumbHit(x, y);
        if (ch != CB.hover) { CB.hover = ch; A.dirty = true; }
        chromeSetHover(chromeHit(x, y));
        bool fc = fltClearAt(x, y);
        if (fc != A.fltClearHover) { A.fltClearHover = fc; A.dirty = true; }
        tipMouseMove(x, y);
        return 0;
    }
    case WM_MOUSELEAVE:
        A.trackingMouse = false;
        tipHide();
        chromeSetHover(CH_NONE);
        if (A.colHover >= 0) { A.colHover = -1; A.dirty = true; }
        if (A.favTrashHover >= 0) { A.favTrashHover = -1; A.dirty = true; }
        favDisarm();
        if (A.hover != -1 || A.linkHover != -1 || A.tabHover != -1 || A.tabPlusHover || A.sideHover != -1 || CB.hover != -1 || A.fltClearHover) {
            A.hover = A.linkHover = A.tabHover = A.sideHover = CB.hover = -1;
            A.fltClearHover = false;
            A.tabPlusHover = A.tabCloseHover = false;
            A.dirty = true;
        }
        return 0;
    case WM_LBUTTONDOWN: {
        tipHide();
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (A.menu.open) { // clic fuori: chiude e basta, come i menu di Windows
            if (menuInside(x, y)) { A.menu.pressed = true; return 0; }
            // eccezione: da un menu del percorso si passa direttamente a quello di un altro "›"
            int id = A.menu.kind == MENU_CRUMB ? crumbHit(x, y) : -1;
            bool other = (id == CRUMB_ELL || (id >= 0 && (id & 1))) && !(id & 1 && id / 2 == CB.menuFor);
            menuClose();
            if (other) crumbClick(x, y);
            return 0;
        }
        SetFocus(hwnd);
        if (gotoMouseDown(x, y, false, (wp & MK_CONTROL) != 0)) return 0;
        if (optionsMouseDown(x, y)) return 0;
        if (A.renaming) endRename(true);
        bool onPath = y >= A.pathY && y < A.pathY + A.topH;
        int chId = chromeHit(x, y);
        if (A.editingPath && (!onPath || chId != CH_NONE)) { A.editingPath = false; A.dirty = true; }
        if (chId != CH_NONE) { chromeExec(chId); return 0; } // ≡, barra laterale, ← → ↑, ☆
        if (fltClearAt(x, y)) {
            A.filter.clear(); // × del filtro
            rebuildView(true);
            A.dirty = true;
            return 0;
        }
        if (y < A.pathY) { // barra delle schede
            bool closeHit, plusHit;
            int t = tabHit(x, y, &closeHit, &plusHit);
            if (plusHit) tabNew(A.cur ? A.cur->path : L"", true);
            else if (t >= 0 && closeHit) tabClose(t);
            else if (t >= 0) tabActivate(t);
            return 0;
        }
        if (onPath) { // segmento → ci va; "›" → sottocartelle; spazio vuoto → modifica del percorso
            if (!A.editingPath && !crumbClick(x, y)) beginPathEdit();
            return 0;
        }
        if (int pe = panelEdgeAt(x, y); pe >= 0) { colDragStart(pe, x); return 0; } // bordo di un pannello: larghezza
        if (x < A.sideW && favMouseDown(x, y)) return 0; // preferito: cestino, o clic che vale al rilascio (si può trascinare)
        if (x < A.sideW) { // barra laterale
            int si = sideHit(x, y);
            if (si == -2 || si == -3) sideToggleSection(si);
            else if (const SideItem* it = sideItem(si)) {
                if (wp & MK_CONTROL) tabNew(it->path, false);
                else navigate(it->path, 1, L"");
            }
            return 0;
        }
        if (x >= A.listR) return 0; // dentro l'anteprima non c'è niente da cliccare
        if (y >= G.h - A.footH) return 0; // barra dei tempi e di stato: niente da cliccare oltre la × del filtro
        if (y < A.listTop) { // intestazione: bordo → larghezza della colonna; altrove → ordinamento
            int b = colBorderAt(x, y);
            if (b >= 0) { colDragStart(b, x); return 0; }
            int col = colAt(x);
            if (col < 0) return 0; // griglia: fuori dalle etichette
            if (col == A.sortCol) A.sortDesc = !A.sortDesc;
            else { A.sortCol = col; A.sortDesc = col == COL_DATE || col == COL_SIZE; } // nomi e percorsi dalla A
            resort();
            A.dirty = true;
            return 0;
        }
        if (x >= A.listR - A.sbW) { // scrollbar
            float ty, th;
            if (thumbRect(ty, th)) {
                if (y < ty || y > ty + th) { // clic sulla traccia: salta lì
                    float t = (y - A.listTop - th * 0.5f) / (A.listH - th);
                    A.scrollY = A.scrollTarget = t * maxScroll();
                    clampScroll();
                    thumbRect(ty, th);
                }
                A.draggingThumb = true;
                A.dragOffset = y - ty;
                SetCapture(hwnd);
                A.dirty = true;
            }
            return 0;
        }
        if (!A.cur || y >= A.listTop + A.listH) return 0;
        int r = itemAt(x, y);
        bool shift = (wp & MK_SHIFT) != 0, ctrl = (wp & MK_CONTROL) != 0;
        // Da icona o nome di una voce già selezionata si trascinano i file (il clic
        // vale al rilascio, così si può trascinare tutta la selezione); da ogni altro punto, nome
        // di una voce non selezionata compreso, il clic vale subito e trascinando parte il rettangolo.
        if (r >= 0 && !shift && isSel(A.view[r]) && itemGrabHit(r, x, y)) {
            A.sel = r;
            DD.deferRow = r;
            DD.deferToggle = ctrl;
            A.dirty = true;
            dragArm(x, y, MK_LBUTTON);
            return 0;
        }
        // Destinazione di un collegamento: il clic seleziona la riga come sempre e, se al rilascio non
        // è partito il rettangolo, apre la cartella della destinazione (WM_LBUTTONUP)
        A.linkPress = shift || ctrl ? -1 : linkHit(x, y);
        A.linkPressFor = A.cur;
        mqPress(x, y, r, shift, ctrl);
        return 0;
    }
    case WM_MBUTTONDOWN: { // clic centrale: cartella in nuova scheda, scheda chiusa
        tipHide();
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        menuClose();
        if (gotoMouseDown(x, y, true, false)) return 0;
        if (OP.open) return 0;
        bool closeHit, plusHit;
        int t = tabHit(x, y, &closeHit, &plusHit);
        if (t >= 0) { tabClose(t); return 0; }
        if (crumbMiddle(x, y)) return 0;
        if (const SideItem* it = sideItem(sideHit(x, y))) { tabNew(it->path, false); return 0; }
        if (int lh = linkHit(x, y); lh >= 0) { linkOpen(lh, true); return 0; } // destinazione di un collegamento
        int r = itemAt(x, y);
        if (r >= 0 && A.cur && (A.cur->e[A.view[r]].attr & FILE_ATTRIBUTE_DIRECTORY))
            tabNew(joinPath(A.cur->path, A.cur->name(A.cur->e[A.view[r]])), false);
        return 0;
    }
    case WM_RBUTTONDOWN:
        tipHide();
        if (A.menu.open && !menuInside(GET_X_LPARAM(lp), GET_Y_LPARAM(lp))) menuClose(); // poi WM_CONTEXTMENU lo riapre qui
        else dragPressRight(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_RBUTTONUP: // senza trascinamento: DefWindowProc manda WM_CONTEXTMENU
        if (DD.armed) { DD.armed = false; ReleaseCapture(); }
        zipDragCancel();
        break;
    case WM_KILLFOCUS:
        tipHide();
        menuClose();
        break;
    case WM_LBUTTONUP:
        if (A.menu.open) {
            if (A.menu.pressed) menuExec(menuHit(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
            A.menu.pressed = false;
            return 0;
        }
        if (A.draggingThumb) { A.draggingThumb = false; ReleaseCapture(); }
        if (colDragEnd(true) || favMouseUp(true, (wp & MK_CONTROL) != 0)) return 0;
        {
            // Clic sulla destinazione di un collegamento: premuto e rilasciato lì, senza rettangolo
            int pressed = A.linkPress;
            A.linkPress = -1;
            bool linkClick = pressed >= 0 && MQ.armed && A.cur == A.linkPressFor &&
                             linkHit(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)) == pressed;
            mqEnd(); // il clic è già stato applicato alla pressione
            zipDragCancel();
            dragRelease();
            if (linkClick) { A.linkClickAt = nowMs(); linkOpen(pressed, false); }
        }
        return 0;
    case WM_LBUTTONDBLCLK: {
        int y = GET_Y_LPARAM(lp), x = GET_X_LPARAM(lp);
        if (gotoDblClk(x, y, (wp & MK_CONTROL) != 0)) return 0;
        if (optionsMouseDown(x, y)) return 0; // nel pannello il secondo clic conta come un clic
        int chId = chromeHit(x, y);
        if (chId >= CH_BACK) { chromeExec(chId); return 0; } // doppio clic rapido su ← →: due passi
        if (y < A.pathY) return 0;
        if (int pe = panelEdgeAt(x, y); pe >= 0) { panelEdgeReset(pe); return 0; } // bordo di un pannello: larghezza predefinita
        int b = colBorderAt(x, y);
        if (b >= 0) { colFit(b); return 0; } // bordo tra due colonne: adatta al contenuto
        // Il primo clic ha aperto la destinazione di un collegamento: il secondo non apre la voce
        // che intanto è finita sotto il mouse (o il collegamento stesso, se la cartella è lenta)
        if (nowMs() - A.linkClickAt < GetDoubleClickTime()) { A.linkClickAt = -1e9; return 0; }
        if (itemAt(x, y) >= 0 && !(wp & (MK_SHIFT | MK_CONTROL))) { A.sel = itemAt(x, y); openSelected(); }
        return 0;
    }
    case WM_XBUTTONDOWN:
        if (OP.open || gotoIsOpen()) return TRUE;
        if (GET_XBUTTON_WPARAM(wp) == XBUTTON1) goBack(); else goForward();
        return TRUE;
    case WM_CLOSE:
        if (closeDeferred()) return 0; // operazioni sui file in corso: si chiude quando finiscono
        break;
    case WM_ENDSESSION:
        // Windows si spegne o l'utente esce: dopo questo messaggio il processo può finire senza
        // WM_DESTROY. Le schede e i salvataggi in coda si scrivono adesso: nessuno sta aspettando
        if (wp) { tabsSaveOnExit(); saveFlush(); logFlush(); }
        return 0;
    case WM_DESTROY:
        tabsSaveOnExit(); // schede.txt, riavvio-<pid>.txt: li scrive saveFlush, a fine processo
        // I dati messi negli appunti vivono in questo processo: prima di uscire vanno consegnati
        // a Windows, altrimenti un "Incolla" successivo riceve solo l'elenco dei file e perde
        // l'informazione copia/taglia.
        if (g_clipboardOwner) OleFlushClipboard();
        fdRelease();
        dropUnregister();
        RemovePropW(hwnd, kInstanceProp);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
