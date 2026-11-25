// [z]f — cornice della finestra disegnata da [z]f: le schede stanno nella barra del titolo.
//
//   [≡][▯] [scheda][scheda][+]   ·····trascina·····   [–][□][×]
//   [←][→][↑]  Questo PC › Disco locale (C:) › Users              [☆][◨]
//
// L'area client arriva fino al bordo superiore (WM_NCCALCSIZE); WM_NCHITTEST dice a Windows cos'è
// ogni punto: bordo superiore per ridimensionare, spazio vuoto per trascinare (doppio clic:
// ingrandisci; tasto destro: menu di sistema), i tre pulsanti come pulsanti di sistema (su
// Windows 11 il pulsante ingrandisci mostra gli snap layout). I clic sui pulsanti arrivano come
// messaggi non client e li gestisce [z]f.

enum ChromeId { CH_NONE = -1, CH_MENU = 1, CH_SIDEBAR, CH_MIN, CH_MAX, CH_CLOSE, CH_BACK, CH_FWD, CH_UP, CH_STAR,
               CH_VIEW,       // elenco ↔ griglia
               CH_PREVIEW }; // anteprima: l'ultimo a destra nella riga del percorso

typedef int(WINAPI* PFN_GetSystemMetricsForDpi)(int, UINT);
static PFN_GetSystemMetricsForDpi pGetSystemMetricsForDpi;

static int sysMetric(int m) {
    if (!pGetSystemMetricsForDpi)
        pGetSystemMetricsForDpi = (PFN_GetSystemMetricsForDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi");
    UINT dpi = (UINT)lroundf(A.scale * 96);
    return pGetSystemMetricsForDpi ? pGetSystemMetricsForDpi(m, dpi) : GetSystemMetrics(m);
}

// Spessore del bordo di ridimensionamento (come quello delle finestre di Windows)
static int frameY() { return sysMetric(SM_CYFRAME) + sysMetric(SM_CXPADDEDBORDER); }

struct Chrome {
    int hover = CH_NONE;     // pulsante sotto il mouse (anche nell'area non client)
    int pressed = CH_NONE;   // pulsante di sistema premuto
    HoverFade fade;
    bool ncTracking = false;
} CH;

// Rettangoli dei pulsanti (coordinate client)
static bool chromeRect(int id, float& x, float& y, float& w, float& h) {
    float W = (float)G.w;
    switch (id) {
    case CH_MENU:    x = 0;                y = 0; w = A.btnW; h = A.tabH; return true;
    case CH_SIDEBAR: x = A.btnW;           y = 0; w = A.btnW; h = A.tabH; return true;
    case CH_MIN:     x = W - A.capW * 3;   y = 0; w = A.capW; h = A.tabH; return true;
    case CH_MAX:     x = W - A.capW * 2;   y = 0; w = A.capW; h = A.tabH; return true;
    case CH_CLOSE:   x = W - A.capW;       y = 0; w = A.capW; h = A.tabH; return true;
    case CH_BACK: case CH_FWD: case CH_UP:
        x = px(ui::GAP_S) + (id - CH_BACK) * A.navW; y = A.pathY + floorf((A.topH - A.rowH) * 0.5f);
        w = A.navW; h = A.rowH; return true;
    case CH_STAR: case CH_VIEW: case CH_PREVIEW: // a destra: ☆ ▦ ◨
        x = W - px(ui::GAP_S) - A.navW * (CH_PREVIEW + 1 - id); y = A.pathY + floorf((A.topH - A.rowH) * 0.5f);
        w = A.navW; h = A.rowH; return true;
    }
    return false;
}

static int chromeHit(int px_, int py) {
    for (int id = CH_MENU; id <= CH_PREVIEW; id++) {
        float x, y, w, h;
        if (chromeRect(id, x, y, w, h) && px_ >= x && px_ < x + w && py >= y && py < y + h) return id;
    }
    return CH_NONE;
}

static bool chromeEnabled(int id) {
    switch (id) {
    case CH_BACK: return !A.back.empty();
    case CH_FWD:  return !A.fwd.empty();
    case CH_UP:   return A.cur && !A.cur->path.empty() && !uncShareRoot(A.cur->path);
    case CH_STAR: return A.cur && !A.cur->path.empty();
    }
    return true;
}

static int favIndex(const std::wstring& path); // sidebar.cpp
static void menuOpenApp(float cx, float cy);    // menu.cpp

// ▯, Ctrl+B, opzioni: mostra o nasconde la barra laterale (la lista si sposta: niente hover vecchio)
static void sidebarToggle() {
    A.sideVisible = !A.sideVisible;
    A.sideHover = -1;
    A.hover = -1;
    settingsSave();
    A.dirty = true;
}

static void chromeExec(int id) {
    if (!chromeEnabled(id)) return;
    switch (id) {
    case CH_MENU:    menuOpenApp(0, A.tabH); break;
    case CH_SIDEBAR: sidebarToggle(); break;
    case CH_MIN:     ShowWindow(A.hwnd, SW_MINIMIZE); break;
    case CH_MAX:     ShowWindow(A.hwnd, IsZoomed(A.hwnd) ? SW_RESTORE : SW_MAXIMIZE); break;
    case CH_CLOSE:   PostMessageW(A.hwnd, WM_CLOSE, 0, 0); break;
    case CH_BACK:    goBack(); break;
    case CH_FWD:     goForward(); break;
    case CH_UP:      goUp(); break;
    case CH_STAR:    favToggle(A.cur->path); break;
    case CH_VIEW:    viewSet(!A.grid, -1); break;
    case CH_PREVIEW: previewToggle(); break;
    }
    A.dirty = true;
}

// --- area non client ---------------------------------------------------------------------
static LRESULT chromeNcCalcSize(HWND hwnd, WPARAM wp, LPARAM lp) {
    NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lp;
    int top = p->rgrc[0].top;
    LRESULT r = DefWindowProcW(hwnd, WM_NCCALCSIZE, wp, lp); // bordi laterali e inferiore come sempre
    p->rgrc[0].top = top;                                     // niente barra del titolo di Windows
    if (IsZoomed(hwnd)) p->rgrc[0].top += frameY();           // ingrandita: la finestra sborda dallo schermo
    return r;
}

static LRESULT chromeNcHitTest(HWND hwnd, WPARAM wp, LPARAM lp) {
    LRESULT hit = DefWindowProcW(hwnd, WM_NCHITTEST, wp, lp);
    if (hit != HTCLIENT) return hit; // bordi laterali e inferiore
    POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    ScreenToClient(hwnd, &pt);
    if (!IsZoomed(hwnd) && pt.y < frameY()) { // bordo superiore: ridimensiona
        int corner = frameY() * 2;
        return pt.x < corner ? HTTOPLEFT : (pt.x >= G.w - corner ? HTTOPRIGHT : HTTOP);
    }
    if (pt.y >= A.tabH) return HTCLIENT;
    int id = chromeHit(pt.x, pt.y);
    if (id == CH_MIN) return HTMINBUTTON;
    if (id == CH_MAX) return HTMAXBUTTON; // Windows 11: snap layout al passaggio del mouse
    if (id == CH_CLOSE) return HTCLOSE;
    if (id != CH_NONE) return HTCLIENT;
    bool closeHit, plusHit;
    if (tabHit(pt.x, pt.y, &closeHit, &plusHit) >= 0 || plusHit) return HTCLIENT;
    return HTCAPTION; // spazio vuoto: trascina la finestra
}

static int ncToChrome(WPARAM ht) {
    return ht == HTMINBUTTON ? CH_MIN : ht == HTMAXBUTTON ? CH_MAX : ht == HTCLOSE ? CH_CLOSE : CH_NONE;
}

static void chromeSetHover(int id) {
    if (id != CH.hover) { CH.hover = id; A.dirty = true; }
}

// true se il messaggio è stato gestito (va restituito 0)
static bool chromeNcMouse(HWND hwnd, UINT msg, WPARAM wp) {
    int id = ncToChrome(wp);
    switch (msg) {
    case WM_NCMOUSEMOVE:
        chromeSetHover(id);
        if (!CH.ncTracking) {
            TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE | TME_NONCLIENT, hwnd, 0 };
            TrackMouseEvent(&t);
            CH.ncTracking = true;
        }
        return false; // il resto (snap layout) lo fa Windows
    case WM_NCMOUSELEAVE:
        CH.ncTracking = false;
        chromeSetHover(CH_NONE);
        CH.pressed = CH_NONE;
        return false;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (id == CH_NONE) return false;
        CH.pressed = id; // niente DefWindowProc: disegnerebbe i pulsanti classici
        A.dirty = true;
        return true;
    case WM_NCLBUTTONUP:
        if (id == CH_NONE) { CH.pressed = CH_NONE; return false; }
        if (CH.pressed == id) chromeExec(id);
        CH.pressed = CH_NONE;
        return true;
    }
    return false;
}

// --- disegno ---------------------------------------------------------------------------
static void drawChromeButton(int id, bool toolbar) {
    float x, y, w, h;
    if (!chromeRect(id, x, y, w, h)) return;
    bool en = chromeEnabled(id);
    float a = en ? hoverAlpha(CH.fade, id) : 0;
    bool closeBtn = id == CH_CLOSE;
    if (CH.pressed == id && en) a = 1;
    if (a > 0) {
        bool caption = id == CH_MIN || id == CH_MAX || id == CH_CLOSE; // pulsanti di sistema: squadrati e a filo, come in Windows
        uint32_t hc = fadeA(closeBtn ? C_CLOSE : (toolbar ? C_HOVER : C_HOVER_STRONG), a);
        if (caption) rect(x, y, w, h, hc);
        else rrect(x + px(ui::GAP_XS), y + px(ui::GAP_XS), w - px(ui::GAP_XS) * 2, h - px(ui::GAP_XS) * 2, px(ui::R_S), hc);
    }
    uint32_t col = !en ? C_FAINT : (closeBtn && a > 0.5f) ? C_ON_CLOSE : (toolbar ? C_DIM : C_TEXT);
    uint32_t cp = 0;
    switch (id) {
    case CH_MENU:    cp = IC_MENU; break;
    case CH_SIDEBAR: cp = IC_SIDEBAR; col = A.sideVisible ? col : C_DIM; break;
    case CH_MIN:     cp = IC_MIN; break;
    case CH_MAX:     cp = IsZoomed(A.hwnd) ? IC_RESTORE : IC_MAX; break;
    case CH_CLOSE:   cp = IC_WCLOSE; break;
    case CH_BACK:    cp = IC_BACK; break;
    case CH_FWD:     cp = IC_FORWARD; break;
    case CH_UP:      cp = IC_UP; break;
    case CH_STAR: {
        bool fav = A.cur && favIndex(A.cur->path) >= 0;
        cp = fav ? IC_STAR_FILL : IC_STAR;
        if (fav) col = C_ACCENT;
        break;
    }
    case CH_VIEW:    cp = IC_GRID; if (A.grid && en) col = C_TEXT; break; // in griglia: come ◨ aperta
    case CH_PREVIEW: cp = IC_PREVIEW; if (A.prevOpen && en) col = C_TEXT; break; // aperta: come ▯ con la barra laterale
    }
    drawUiIcon(x, y, w, h, cp, col);
    if (id == CH_MENU && updBadge()) { // versione nuova pronta: un pallino su ≡, dove sta "Aggiorna"
        float b = px(ui::BADGE), in = px(ui::BADGE_INSET);
        rrect(floorf(x + w - in - b * 0.5f), floorf(y + in - b * 0.5f), b, b, b * 0.5f, C_ACCENT);
    }
}

static void drawChromeTitle(double dt) {
    hoverFadeUpdate(CH.fade, CH.hover, dt);
    drawChromeButton(CH_MENU, false);
    drawChromeButton(CH_SIDEBAR, false);
    drawChromeButton(CH_MIN, false);
    drawChromeButton(CH_MAX, false);
    drawChromeButton(CH_CLOSE, false);
}

static void drawChromeToolbar() {
    drawChromeButton(CH_BACK, true);
    drawChromeButton(CH_FWD, true);
    drawChromeButton(CH_UP, true);
    drawChromeButton(CH_STAR, true);
    drawChromeButton(CH_VIEW, true);
    drawChromeButton(CH_PREVIEW, true);
}
