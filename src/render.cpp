// [z]f — disegno di un frame: lista virtualizzata, intestazioni, barre, menu, pannello F12.

static bool animating() { return fabsf(A.scrollTarget - A.scrollY) > 0.5f || A.animActive; }

// --- pannelli sovrapposti (menu, tooltip, opzioni, F12) ------------------------------------
// Un solo disegno per tutti. Compaiono con una dissolvenza e uno spostamento di ENTER_DY in
// durMs (ease-out); spariscono di colpo. k è l'opacità con cui disegnare anche il contenuto,
// dy quanto il pannello sta ancora sopra la sua posizione finale.
struct PanelEnter { float k, dy; };
static PanelEnter panelEnter(double openedAt, float durMs) {
    float k = A.animations ? easeOut((float)(nowMs() - openedAt) / durMs) : 1.0f;
    if (k < 1) A.animActive = true;
    return { k, floorf((1 - k) * px(ui::ENTER_DY)) };
}
// Ombra spostata di SHADOW_X/SHADOW_Y, fondo C_MENU, contorno 1 px C_LINE. r: R_M per i
// contenitori, R_S per i tooltip (DESIGN.md, "Forme").
static void drawPanel(float x, float y, float w, float h, float r, float k) {
    rrect(x + px(ui::SHADOW_X), y + px(ui::SHADOW_Y), w, h, r, fadeA(C_SHADOW, k));
    rrect(x, y, w, h, r, fadeA(C_MENU, k));
    rrect(x, y, w, h, r, fadeA(C_LINE, k), 1);
}

// Voce di un elenco in un pannello sovrapposto (menu, "Vai a"): fondo della voce attiva e icona.
// icon: 0 nessuna, 1 cartella, 2 unità (iconPath), 3 segno di spunta; dim: elemento nascosto.
// iconW: spazio riservato alle icone (0 se l'elenco non ne ha). Ritorna la x da cui comincia il testo.
static float drawPanelItem(float x, float w, float y, float ih, float pad, bool hot, int icon,
                           const std::wstring* iconPath, bool dim, float iconW, float k) {
    if (hot) rrect(x + px(ui::GAP_S), y, w - px(ui::GAP_M), ih, px(ui::R_S), fadeA(C_SEL, k));
    float tx = x + pad;
    if (icon == 3) drawUiIcon(tx, y, (float)I.size, ih, IC_CHECK, fadeA(C_TEXT, k));
    else if (icon) { // cartella o unità: stessa icona della lista
        const IconEntry* ie = icon == 2 && iconPath ? iconForKey(*iconPath, *iconPath, 0, false, false, slowPath(*iconPath))
                                                    : iconForKey(L"<dir>", L"cartella", FILE_ATTRIBUTE_DIRECTORY, true, false);
        if (ie) drawIcon(tx, y + floorf((ih - I.size) * 0.5f), *ie, fadeA(dim ? C_ICON_HIDDEN : C_ICON, k));
    }
    return tx + iconW;
}

// Barra di scorrimento di un elenco che scorre voce per voce (menu, "Vai a"), al bordo destro right
static void drawPanelScroll(float right, float trackY, float trackH, int first, int visible, int n, float k) {
    if (visible >= n) return;
    float th = std::max(px(ui::THUMB_MIN), trackH * visible / (float)n);
    float ty = trackY + (trackH - th) * first / (float)std::max(1, n - visible);
    rrect(right - px(ui::GAP_S) - px(ui::EDGE), floorf(ty), px(ui::EDGE), floorf(th), px(ui::EDGE), fadeA(C_SCROLL, k));
}

// Anello di punti di un lavoro in corso, centrato in (cx, cy), diametro d: SPIN_DOTS punti fermi
// su un cerchio, ne gira solo la luminosità (l'unica animazione continua, DESIGN.md §5). Punti e
// non un glifo che ruota: i glifi sono rasterizzati diritti nell'atlante. Senza animazioni è fermo.
static void drawSpinner(float cx, float cy, float d, uint32_t col) {
    float dot = std::max(2.0f, px(ui::SPIN_DOT)), r = (d - dot) * 0.5f;
    float head = A.animations ? (float)fmod(nowMs() / ui::SPIN_PERIOD, 1.0) : 0.0f;
    for (int i = 0; i < ui::SPIN_DOTS; i++) {
        float f = (float)i / ui::SPIN_DOTS, ang = (f - 0.25f) * 6.2831853f; // il primo punto in alto
        float behind = head - f;
        if (behind < 0) behind += 1; // 0 la testa, poi la scia che si spegne
        float a = ui::SPIN_MIN_ALPHA + (1 - ui::SPIN_MIN_ALPHA) * (1 - behind);
        rrect(floorf(cx + r * cosf(ang) - dot * 0.5f + 0.5f), floorf(cy + r * sinf(ang) - dot * 0.5f + 0.5f),
              dot, dot, dot * 0.5f, fadeA(col, a));
    }
    if (A.animations) A.animActive = true;
}

// Titolo di una colonna nell'intestazione; arrow: con la freccia dell'ordinamento
static std::wstring colTitle(int col, bool arrow) {
    std::wstring t = col == COL_NAME ? L"Nome" : col == COL_SIZE ? L"Dimensione" : col == COL_LINK ? L"Destinazione"
                   : A.cur && A.cur->isDrives ? L"Tipo" : L"Data modifica";
    if (arrow) t += A.sortDesc ? L" \x2193" : L" \x2191";
    return t;
}

// Testo allineato a destra in [left, right]; se non ci sta, da left con "…"
static void drawTextRight(float left, float right, float y, const wchar_t* s, int n, uint32_t c) {
    float w = (float)textWidth(s, n);
    drawText(w <= right - left ? right - w : left, y, s, n, c, right);
}

// Griglia: nome su GRID_NAME_LINES righe centrate in [x, x + w]. Va a capo dopo l'ultimo spazio,
// trattino o trattino basso che ci sta, o prima dell'ultimo punto (l'estensione va sotto);
// altrimenti dove finisce lo spazio. L'ultima riga finisce in "…".
static void drawGridName(const wchar_t* s, int n, float x, float y, float w, uint32_t col) {
    float lh = (float)F.lineH + px(ui::LINE_GAP);
    int start = 0;
    for (int line = 0; line < ui::GRID_NAME_LINES && start < n; line++, y += lh) {
        int end = n;
        if (line + 1 < ui::GRID_NAME_LINES) {
            int i = start, fit = start, brk = -1;
            float acc = 0;
            while (i < n) {
                int j = i;
                float a = (float)glyphGet(decodeUtf16(s, n, j)).adv;
                if (acc + a > w && i > start) break;
                wchar_t c = s[i];
                if (c == L'.' && i > start) brk = i;
                acc += a;
                i = fit = j;
                if (c == L' ' || c == L'-' || c == L'_') brk = i;
            }
            if (fit < n) end = brk > start ? brk : fit;
        }
        int len = end - start;
        float tw = (float)textWidth(s + start, len);
        drawText(tw <= w ? floorf(x + (w - tw) * 0.5f) : x, y, s + start, len, col, x + w);
        start = end;
        while (start < n && s[start] == L' ') start++;
    }
}

static void drawMenu() {
    const PopupMenu& m = A.menu;
    PanelEnter e = panelEnter(m.openedAt, ui::DUR_MENU);
    float k = e.k;
    float textOff = floorf((m.ih - F.lineH) * 0.5f);
    float my = m.y - e.dy;
    drawPanel(m.x, my, m.w, m.h, px(ui::R_M), k);
    float y = my + m.vpad;
    int end = std::min((int)m.items.size(), m.first + m.visible);
    for (int i = m.first; i < end; i++) {
        const MenuItem& it = m.items[i];
        if (!it.id) {
            rect(m.x + m.pad * 0.5f, y + floorf(m.sepH * 0.5f), m.w - m.pad, 1, fadeA(C_LINE, k));
            y += m.sepH;
            continue;
        }
        float tx = drawPanelItem(m.x, m.w, y, m.ih, m.pad, i == m.hot, it.icon,
                                 i < (int)m.paths.size() ? &m.paths[i] : nullptr, it.dim, m.iconW, k);
        int kn = (int)wcslen(it.keys);
        float keysX = m.x + m.w - m.pad - (kn ? textWidth(it.keys, kn) : 0);
        uint32_t tc = !it.enabled ? C_FAINT : it.dim ? C_FAINT : C_TEXT;
        float textEnd = kn ? keysX - px(ui::GAP_M) : m.x + m.w - m.pad;
        drawText(tx, y + textOff, it.label, (int)wcslen(it.label), fadeA(tc, k), textEnd);
        if (kn) drawText(keysX, y + textOff, it.keys, kn, fadeA(it.enabled ? C_DIM : C_FAINT, k));
        y += m.ih;
    }
    drawPanelScroll(m.x + m.w, my + m.vpad, m.visible * m.ih, m.first, m.visible, (int)m.items.size(), k);
}

// Il permesso di disegnare un frame (lo swapchain ha posto): il ciclo principale lo ottiene
// aspettando il vsync insieme ai messaggi, così l'input non resta mai in coda dietro a un frame.
// Le chiamate dirette a render() (ridimensionamento) aspettano qui, al massimo 100 ms.
static bool g_vsyncToken = false;

static void noteVsyncWait(double d) {
    if (d > A.waitMax) A.waitMax = d;
    // Una finestra coperta riceve i vsync in ritardo: è normale e non blocca niente
    if (d > ui::LOG_VSYNC && GetForegroundWindow() == A.hwnd) hitchLog("attesa vsync lunga", 0, d);
}

// Nome e icona della vista di un livello di zoom (barra di stato, tooltip)
static const wchar_t* viewName(int zoom) {
    return zoom == ZOOM_DETAILS ? L"Dettagli" : zoom == ZOOM_LIST ? L"Elenco" : L"Icone";
}
static uint32_t viewIcon(int zoom) { return zoom == ZOOM_DETAILS ? IC_DETAILS : zoom == ZOOM_LIST ? IC_LIST : IC_GRID; }
// Controllo della vista nella barra di stato (il disegno lo misura)
static bool viewCtlAt(int x, int y) { return A.viewW > 0 && y >= G.h - A.statusH && x >= A.viewX && x < A.viewX + A.viewW; }

// --- barre in fondo: della lista (barra di stato, barra dei tempi) e dell'anteprima -----------
// Le stesse parti nei due pannelli, così l'anteprima si legge come la lista. Si disegnano da
// destra: ogni funzione riceve il bordo destro libero e restituisce quello per la parte prima.

// Fondo C_BAR con la linea C_LINE in alto
static void drawBarBand(float x0, float x1, float y, float h) {
    rect(x0, y, x1 - x0, h, C_BAR);
    rect(x0, y, x1 - x0, 1, C_LINE);
}

// Larghezza del blocco della vista: quanto il nome più largo tra quelli possibili, perché cambiando
// vista quello che sta prima non si sposti
static float barViewW(const wchar_t* const* names, int n) {
    float tw = 0;
    for (int i = 0; i < n; i++) tw = std::max(tw, (float)textWidth(names[i], (int)wcslen(names[i])));
    float pad = px(ui::GAP_S);
    return pad + (float)F.lineH + px(ui::GAP_S) + tw + pad;
}

// La vista: icona in C_FAINT e nome in C_DIM, largo w; hover > 0 ne disegna il fondo (solo dove si
// clicca). Prima, a GAP_M, la barretta verticale che la separa dai dati (sep: solo se ci sono)
static float drawBarView(float right, float y0, float w, uint32_t icon, const wchar_t* name, float hover, bool sep) {
    float fy = y0 + px(ui::GAP_XS), fh = A.statusH - px(ui::GAP_XS) * 2;
    float iw = (float)F.lineH, pad = px(ui::GAP_S), x = right - w;
    if (hover > 0) rrect(x, fy, w, fh, px(ui::R_S), fadeA(C_HOVER, hover));
    drawUiIcon(x + pad, y0, iw, A.statusH, icon, C_FAINT);
    drawText(x + pad + iw + px(ui::GAP_S), y0 + floorf((A.statusH - F.lineH) * 0.5f), name, (int)wcslen(name), C_DIM);
    float bar = x - px(ui::GAP_M);
    if (sep) rect(bar, fy + px(ui::GAP_XS), 1, fh - px(ui::GAP_XS) * 2, C_LINE);
    return bar - px(ui::GAP_L);
}

// Contatori di cartelle e file con le loro icone; more: la lettura si è fermata prima del fondo
// (anteprima di una cartella enorme), i numeri sono un minimo e hanno un "+"
static float drawBarCounts(float right, float y0, int dirs, int files, bool more) {
    wchar_t nd[16], nf[16];
    int dn = _snwprintf(nd, 15, more ? L"%d+" : L"%d", dirs), fn = _snwprintf(nf, 15, more ? L"%d+" : L"%d", files);
    float iw = (float)F.lineH, ig = px(ui::GAP_S), y = y0 + floorf((A.statusH - F.lineH) * 0.5f);
    float x = right - (iw + ig + textWidth(nd, dn) + px(ui::GAP_L) + iw + ig + textWidth(nf, fn));
    float left = x;
    drawUiIcon(x, y0, iw, A.statusH, IC_FOLDER, C_FAINT);
    x = drawText(x + iw + ig, y, nd, dn, C_DIM) + px(ui::GAP_L);
    drawUiIcon(x, y0, iw, A.statusH, IC_PAGE, C_FAINT);
    drawText(x + iw + ig, y, nf, fn, C_DIM);
    return left;
}

// Fondo di una voce (finestra): selezione e hover arrotondati R_S, come le liste di Windows 11
static void drawItemBg(int vi, float x, float y, float w, float h) {
    float ha = hoverAlpha(A.rowFade, vi), r = px(ui::R_S);
    if (isSel(A.view[vi])) {
        rrect(x, y, w, h, r, C_SEL);
        if (ha > 0) rrect(x, y, w, h, r, fadeA(C_SEL_HOVER, ha));
    } else if (ha > 0) {
        rrect(x, y, w, h, r, fadeA(C_HOVER, ha));
    }
}

// Icona di una voce in Dettagli ed Elenco, in (x, y): finché non arriva il segnaposto (cartella o
// foglio), poi l'icona vera che lo sostituisce con una dissolvenza incrociata
static void drawRowIcon(const Entry& e, float x, float y, double now) {
    float iconS = (float)I.size;
    bool dir = (e.attr & FILE_ATTRIBUTE_DIRECTORY) != 0, hidden = (e.attr & FILE_ATTRIBUTE_HIDDEN) != 0;
    const IconEntry* ie = iconFor(A.cur, e);
    float ia = ie ? (A.animations ? easeOut((float)(now - ie->readyAt) / ui::DUR_ICON) : 1.0f) : 0.0f;
    if (ie && ia < 1) A.animActive = true;
    if (ia < 1) {
        float pa = 1 - ia;
        if (dir) {
            rect(x, y + floorf(iconS * ui::PH_TAB_Y), floorf(iconS * ui::PH_TAB_W), floorf(iconS * ui::PH_TAB_H), fadeA(C_FOLDER, pa));
            rect(x, y + floorf(iconS * ui::PH_BODY_Y), iconS, iconS - floorf(iconS * ui::PH_BODY_CUT), fadeA(C_FOLDER, pa));
        } else {
            rect(x + floorf(iconS * ui::PH_FILE_X), y, floorf(iconS * ui::PH_FILE_W), iconS, fadeA(C_FILE, pa));
        }
    }
    if (ie) drawIcon(x, y, *ie, fadeA(hidden ? C_ICON_HIDDEN : C_ICON, ia));
}

static void render() {
    if (A.gpuLost) { g_vsyncToken = false; return; } // si disegna di nuovo quando il device nuovo è pronto
    if (G.w <= 0 || G.h <= 0 || !G.rtv) { A.dirty = false; return; }
    if (G.waitable && !g_vsyncToken) {
        double tw = nowMs();
        WaitForSingleObjectEx(G.waitable, 100, TRUE);
        noteVsyncWait(nowMs() - tw);
    }
    g_vsyncToken = false;

    double t0 = nowMs();
    // Dopo una pausa il primo frame di un'animazione avanza di un vsync, non del tempo trascorso
    double dt = (A.prevAnimating && A.lastFrame > 0) ? std::min(50.0, t0 - A.lastFrame) : 16.7;
    A.lastFrame = t0;
    A.animActive = false;

    // scorrimento morbido (solo rotellina)
    A.scrollY = approach(A.scrollY, A.scrollTarget, dt, ui::TAU_SCROLL, 0.5f); // senza animazioni: subito

    layout();
    clampScroll();
    previewSync(); // la voce sotto il cursore è cambiata: si chiede l'anteprima (su un thread)

    float W = (float)G.w, H = (float)G.h;
    float s = A.scale;
    float textOff = floorf((A.rowH - F.lineH) * 0.5f);

    G.ctx->OMSetRenderTargets(1, &G.rtv, nullptr);
    D3D11_VIEWPORT vp = { 0, 0, W, H, 0, 1 };
    G.ctx->RSSetViewports(1, &vp);
    float clear[4] = { (C_BG & 0xff) / 255.f, ((C_BG >> 8) & 0xff) / 255.f, ((C_BG >> 16) & 0xff) / 255.f, 1 };
    G.ctx->ClearRenderTargetView(G.rtv, clear);
    float cbData[4] = { 1.0f / W, 1.0f / H, 0, 0 };
    G.ctx->UpdateSubresource(G.cb, 0, nullptr, cbData, 0, 0);
    UINT stride = sizeof(Vtx), off = 0;
    G.ctx->IASetInputLayout(G.il);
    G.ctx->IASetVertexBuffers(0, 1, &G.vb, &stride, &off);
    G.ctx->IASetIndexBuffer(G.ib, DXGI_FORMAT_R16_UINT, 0);
    G.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    G.ctx->VSSetShader(G.vs, nullptr, 0);
    G.ctx->VSSetConstantBuffers(0, 1, &G.cb);
    G.ctx->PSSetShader(G.ps, nullptr, 0);
    ID3D11ShaderResourceView* srvs[4] = { G.atlasSrv, G.iconSrv, G.prevSrv, G.thumbSrv };
    G.ctx->PSSetShaderResources(0, 4, srvs);
    G.ctx->PSSetSamplers(0, 1, &G.samp);
    float bf[4] = { 0, 0, 0, 0 };
    G.ctx->OMSetBlendState(G.blend, bf, 0xffffffff);
    G.ctx->RSSetState(G.rs);
    bool timing = A.showStats && G.tsDisjoint && G.tsBegin && G.tsEnd;
    if (timing) { G.ctx->Begin(G.tsDisjoint); G.ctx->End(G.tsBegin); }
    mapVB();

    float LX = A.listX;            // la lista inizia dopo la barra laterale
    float listRight = A.listR - A.sbW; // a destra della barra di scorrimento può esserci l'anteprima
    float sizeRight = listRight - A.padX;
    float sizeX = A.sizeX, dateX = A.dateX, dateEnd = sizeX - A.padX; // colonne (layout)
    float iconS = (float)I.size;
    float iconX = LX + A.padX;
    float bpX = LX + px(ui::GAP_S), bpW = listRight - LX - px(ui::GAP_S) * 2, bpR = px(ui::R_S); // sfondo delle righe
    float nameX = iconX + iconS + px(ui::GAP_M);
    A.iconX = iconX; A.nameX = nameX; // per il mouse: il trascinamento dei file parte da icona e nome

    hoverFadeUpdate(A.rowFade, A.hover, dt);
    hoverFadeUpdate(A.linkFade, A.linkHover, dt);
    A.listFade = approach(A.listFade, 1, dt, ui::TAU_LIST_IN, 0.01f);
    float enterDy = floorf((1 - A.listFade) * px(ui::LIST_ENTER_DY)); // la lista nuova sale di qualche pixel mentre entra
    double now = t0;

    // --- voci (virtualizzate: solo quelle visibili) ---
    if (A.cur) {
        float ox, oy; // scorrimento: le voci si spostano in y, in Elenco in x
        scrollOffset(ox, oy);
        oy -= enterDy;
        float s0 = listView() ? ox : oy, len = lineLen(); // linee visibili lungo l'asse
        int first = std::max(0, (int)(s0 / len)) * A.perLine;
        int last = std::min((int)A.view.size(), ((int)((s0 + viewLen()) / len) + 1) * A.perLine);
        double ts = nowMs();
        thumbsFrame(gridView() ? first : -1, gridView() ? last : -1); // miniature: risultati arrivati, richieste
        wchar_t buf[64];
        if (gridView()) {
            A.th.syncMs = nowMs() - ts;
            A.th.syncMax = std::max(A.th.syncMax, A.th.syncMs);
            ts = nowMs();
            for (int i = first; i < last; i++) {
                const Entry& e = A.cur->e[A.view[i]];
                bool hidden = (e.attr & FILE_ATTRIBUTE_HIDDEN) != 0;
                Box bg = itemBg(i);
                drawItemBg(i, bg.x, A.listTop + bg.y - oy, bg.w, bg.h);
                CellParts p = gridParts(i);
                float dy = A.listTop - oy;
                drawThumb(i, Box{ p.thumb.x, p.thumb.y + dy, p.thumb.w, p.thumb.h }, hidden, now);
                if (A.renaming && (int)A.view[i] == A.renameEntry)
                    drawField(A.renameField, p.name.x, p.name.y + dy, p.name.w, A.rowH, s);
                else
                    drawGridName(A.cur->name(e), (int)e.nameLen, p.name.x, p.name.y + dy, p.name.w, hidden ? C_FAINT : C_TEXT);
            }
            A.th.cellsMs = nowMs() - ts;
            A.th.cellsMax = std::max(A.th.cellsMax, A.th.cellsMs);
        } else if (listView()) { // Elenco: icona e nome, in colonne che scorrono in orizzontale
            for (int i = first; i < last; i++) {
                const Entry& e = A.cur->e[A.view[i]];
                bool hidden = (e.attr & FILE_ATTRIBUTE_HIDDEN) != 0;
                Box bg = itemBg(i);
                float y = A.listTop + bg.y - oy;
                drawItemBg(i, bg.x - ox, y, bg.w, bg.h);
                RowParts p = listParts(i);
                float nx = p.nameX - ox, ne = p.nameEnd - ox;
                drawRowIcon(e, p.iconX - ox, y + floorf((A.rowH - iconS) * 0.5f), now);
                if (A.renaming && (int)A.view[i] == A.renameEntry)
                    drawField(A.renameField, nx - px(ui::FIELD_PAD), y + 1, ne + px(ui::GAP_S) - nx, A.rowH - 2, s);
                else
                    drawText(nx, y + textOff, A.cur->name(e), (int)e.nameLen, hidden ? C_FAINT : C_TEXT, ne);
            }
        } else {
            for (int i = first; i < last; i++) {
                float y = A.listTop + itemRect(i).y - oy;
                const Entry& e = A.cur->e[A.view[i]];
                bool dir = (e.attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
                // Sfondo della riga: rettangolo arrotondato con un margine, come le liste di Windows 11
                drawItemBg(i, bpX, y, bpW, A.rowH);
                drawRowIcon(e, iconX, y + floorf((A.rowH - iconS) * 0.5f), now);
                bool hidden = (e.attr & FILE_ATTRIBUTE_HIDDEN) != 0;
                if (A.renaming && (int)A.view[i] == A.renameEntry)
                    drawField(A.renameField, nameX - px(ui::FIELD_PAD), y + 1, A.nameEnd + A.padX - nameX, A.rowH - 2, s);
                else
                    drawText(nameX, y + textOff, A.cur->name(e), (int)e.nameLen, hidden ? C_FAINT : C_TEXT, A.nameEnd);
                // Destinazione di un collegamento (links.cpp): si clicca, quindi sotto il mouse ha il
                // fondo dei controlli piccoli dentro un elemento già evidenziato
                if (A.linkW > 0)
                    if (const LinkTarget* t = linkOf(A.cur, A.view[i])) {
                        int n = (int)t->path.size();
                        float la = hoverAlpha(A.linkFade, i), lp = px(ui::GAP_S), m = px(ui::GAP_XS);
                        if (la > 0) {
                            float w = std::min((float)textWidth(t->path.c_str(), n), dateX - A.padX - A.linkX);
                            rrect(A.linkX - lp, y + m, w + lp * 2, A.rowH - m * 2, bpR, fadeA(C_HOVER_STRONG, la));
                        }
                        drawText(A.linkX, y + textOff, t->path.c_str(), n, C_DIM, dateX - A.padX);
                    }
                if (A.cur->isDrives) {
                    { const wchar_t* k = driveKind((UINT)e.size); drawText(dateX, y + textOff, k, (int)wcslen(k), C_DIM, dateEnd); }
                } else {
                    int n = fmtDate(e.mtime, buf);
                    drawText(dateX, y + textOff, buf, n, C_DIM, dateEnd);
                    if (!dir || (A.cur->zipLen && A.dirSizes)) { // zip: le cartelle hanno già il totale (zip.cpp)
                        n = fmtSize(e.size, buf);
                        drawTextRight(sizeX, sizeRight, y + textOff, buf, n, C_DIM);
                    } else if (A.dirSizes) { // totale contato in background (dirsize.cpp); mentre conta, attenuato
                        DirSize d;
                        bool computing = false;
                        if (dirSizeGet(joinPath(A.cur->path, A.cur->name(e)), d, computing) && (d.files || !d.denied)) { // illeggibile: niente
                            n = fmtSize(d.bytes, buf);
                            drawTextRight(sizeX, sizeRight, y + textOff, buf, n, computing ? C_FAINT : C_DIM);
                        }
                    }
                }
            }
        }

        // Cursore: barra a sinistra (e contorno se la voce non è selezionata). Scivola tra le righe;
        // con una cartella nuova o un salto lungo si sposta e basta.
        if (A.sel >= 0 && A.sel < (int)A.view.size()) {
            Box c = itemBg(A.sel);
            if (A.cursorFor != (const void*)A.cur || fabsf(c.y - A.cursorY) > A.listH * 1.5f ||
                fabsf(c.x - A.cursorX) > (A.listR - A.listX) * 1.5f) {
                A.cursorX = c.x;
                A.cursorY = c.y;
                A.cursorFor = A.cur;
            } else {
                A.cursorX = approach(A.cursorX, c.x, dt, ui::TAU_CURSOR, 0.5f);
                A.cursorY = approach(A.cursorY, c.y, dt, ui::TAU_CURSOR, 0.5f);
            }
            float x = floorf(A.cursorX - ox), y = floorf(A.listTop + A.cursorY - oy);
            float t = std::max(1.0f, floorf(s));
            // pillola verticale a sinistra (come l'indicatore di selezione di Windows 11)
            float pw = px(ui::PILL_W), ph = floorf(c.h * 0.5f);
            rrect(x, y + floorf((c.h - ph) * 0.5f), pw, ph, pw * 0.5f, C_SEL_EDGE);
            if (!isSel(A.view[A.sel])) rrect(x, y, c.w, c.h, bpR, C_SEL_EDGE, t); // contorno se non selezionata
        }
        drawMarquee();
        if (A.listFade < 1) rect(LX, A.listTop, listRight - LX, A.listH, fadeA(C_BG, (1 - A.listFade) * ui::LIST_ENTER_COVER));

        // Elenco: le colonne tagliate a destra non entrano nello spazio della barra verticale né
        // nell'anteprima, che non ridipinge il suo fondo (a sinistra le copre la barra laterale,
        // disegnata dopo)
        if (listView()) rect(listRight, A.listTop, W - listRight, A.listH, C_BG);
        // Barra di scorrimento: verticale a destra, in Elenco orizzontale in fondo alla lista
        float tStart, tLen;
        if (thumbRect(tStart, tLen)) {
            float pad = px(ui::GAP_XS), th = A.sbW - pad * 2;
            if (listView()) rrect(tStart, A.listTop + A.listH - A.sbW + pad, tLen, th, th * 0.5f, C_SCROLL);
            else rrect(listRight + pad, tStart, th, tLen, th * 0.5f, C_SCROLL);
        }
        if (A.view.empty()) {
            const wchar_t* msg = A.filter.empty() ? L"Cartella vuota" : L"Nessun risultato per il filtro";
            drawText(iconX, A.listTop + A.rowH * 0.5f, msg, (int)wcslen(msg), C_FAINT);
        }
    }

    // --- intestazione colonne (disegnata dopo le righe, le copre) ---
    float headY = A.sideTop;
    rect(LX, headY, A.listR - LX, A.headH, C_HEADER);
    rect(LX, headY + A.headH - 1, A.listR - LX, 1, C_LINE);
    if (A.zoom != ZOOM_DETAILS) { // solo le etichette per ordinare, a sinistra (le colonne non ci sono)
        float x = LX + A.padX, y = headY + textOff;
        for (int c = 0; c < 3; c++) {
            std::wstring t = colTitle(c, A.sortCol == c || (c == COL_NAME && A.sortCol == COL_LINK)); // Destinazione qui non c'è
            A.sortX[c] = x;
            x = drawText(x, y, t, C_DIM, listRight - A.padX) + px(ui::GAP_XL);
        }
        A.sortX[3] = x;
    } else {
        std::wstring n = colTitle(COL_NAME, A.sortCol == COL_NAME), d = colTitle(COL_DATE, A.sortCol == COL_DATE),
                     z = colTitle(COL_SIZE, A.sortCol == COL_SIZE);
        float y = headY + textOff;
        if (A.linkW > 0) drawText(A.linkX, y, colTitle(COL_LINK, A.sortCol == COL_LINK), C_DIM, dateX - A.padX);
        else if (A.sortCol == COL_LINK) n = colTitle(COL_NAME, true); // senza la colonna, l'ordine è quello del nome
        drawText(nameX, y, n, C_DIM, A.nameEnd);
        drawText(dateX, y, d, C_DIM, dateEnd);
        drawTextRight(sizeX, sizeRight, y, z.c_str(), (int)z.size(), C_DIM);
        // Bordo tra due colonne: si vede solo sotto il mouse (C_LINE) o mentre lo si trascina (C_SEL_EDGE)
        int b = A.colDrag >= 0 ? A.colDrag : A.colHover;
        if (b >= 0 && b < EDGE_SIDE) rect(colBorderX(b), headY + px(ui::GAP_S), 1, A.headH - 1 - px(ui::GAP_S) * 2,
                         A.colDrag >= 0 ? C_SEL_EDGE : C_LINE);
    }

    drawPreview();
    drawSidebar(dt);

    drawTabs(dt); // prima della riga del percorso: la scheda attiva ci sconfina e la riga ne copre il fondo

    // --- riga del percorso: [▯][←][→][↑] percorso a segmenti [☆][◨] ---
    rect(0, A.pathY, W, A.topH, C_BAR);
    rect(0, A.pathY + A.topH - 1, W, 1, C_LINE);
    {
        float bx = px(ui::GAP_S) + A.navW * 4 + px(ui::GAP_M); // a sinistra ▯ ← → ↑
        float bw = W - px(ui::GAP_S) - A.navW * 2 - px(ui::GAP_M) - bx; // a destra ☆ e anteprima
        float by = A.pathY + floorf((A.topH - A.rowH) * 0.5f), bh = A.rowH;
        float ty = by + textOff;
        drawCrumbs(bx, bx + bw, by, bh, ty, dt);
        drawChromeToolbar();
    }

    // --- in basso: barra dei tempi e barra di stato (con il filtro) ---
    // Barra laterale e anteprima arrivano in fondo alla finestra: queste stanno tra le due, sotto la
    // lista, perché parlano della lista (filtro, contatori, vista).
    float sy0 = H - A.statusH, bx0 = A.sideW, bx1 = A.listR;
    drawBarBand(bx0, bx1, sy0, A.statusH);
    if (A.timesH > 0) { // tempi misurati, solo con l'opzione (barra_tempi)
        float ty0 = sy0 - A.timesH;
        drawBarBand(bx0, bx1, ty0, A.timesH);
        if (A.cur) {
            wchar_t t[256];
            int n = _snwprintf(t, 255,
                L"lettura %.2f ms · chiavi %.2f · ordinamento %.2f · filtro %.2f · frame %.2f · avvio %.0f ms · F12",
                A.cur->enumMs, A.cur->keyMs, A.sortMs, A.filterMs, A.frameMs, A.startupMs);
            decimalComma(t, n);
            drawText(bx0 + A.padX, ty0 + textOff, t, n, C_FAINT, bx1 - A.padX);
        }
    }
    {
        wchar_t buf[256];
        float y = sy0 + textOff;
        float fy = sy0 + px(ui::GAP_XS), fh = A.statusH - px(ui::GAP_XS) * 2, fx = A.fltX;
        // A destra la vista con lo zoom; prima, dopo una barretta, contatori di cartelle e file (della
        // vista, quindi rispettano il filtro) e la selezione; dopo il filtro avvisi e
        // "caricamento…", che si fermano prima della parte destra. Si calcola prima la parte destra:
        // con la lista stretta il filtro si accorcia per lasciarle posto.
        float rightX = bx1 - A.padX + px(ui::GAP_S); // il fondo del controllo della vista sporge di GAP_S
        {
            // Vista: si clicca (menu delle viste) e ci si gira la rotellina (zoom). Sotto il mouse, e col
            // suo menu aperto, ha il fondo dei pulsanti della barra del percorso. Largo sempre quanto il
            // nome più largo: cambiando vista i contatori accanto non si spostano
            static const wchar_t* const kNames[] = { viewName(ZOOM_DETAILS), viewName(ZOOM_LIST), viewName(ZOOM_ICONS) };
            float w = barViewW(kNames, 3);
            A.viewX = rightX - w; A.viewW = w;
            hoverFadeUpdate(A.viewFade, A.viewHover ? 0 : -1, dt);
            float ha = A.menu.open && A.menu.kind == MENU_VIEW ? 1.0f : hoverAlpha(A.viewFade, 0);
            rightX = drawBarView(rightX, sy0, w, viewIcon(A.zoom), viewName(A.zoom), ha, true);
        }
        if (A.cur) {
            static const void* cFor = nullptr;
            static size_t cN = (size_t)-1;
            static int cDirs = 0;
            if (cFor != (const void*)A.cur || cN != A.view.size() || A.countsDirty) {
                cDirs = 0;
                for (uint32_t ei : A.view) if (A.cur->e[ei].attr & FILE_ATTRIBUTE_DIRECTORY) cDirs++;
                cFor = A.cur; cN = A.view.size(); A.countsDirty = false;
            }
            rightX = drawBarCounts(rightX, sy0, cDirs, (int)A.view.size() - cDirs, false);
        }
        // Campo del filtro, sempre visibile (si scrive ovunque nella lista, qui si vede), largo quanto
        // vuole il layout ma mai sopra i contatori; troppo stretto per servire, non si disegna
        A.fltW = std::min(A.fltWant, floorf(rightX - px(ui::GAP_L) - fx));
        if (A.fltW < fh * ui::FILTER_MIN) A.fltW = 0;
        bool filtering = !A.filter.empty();
        A.fltClearW = filtering && A.fltW > 0 ? fh : 0;
        hoverFadeUpdate(A.fltFade, A.fltClearW > 0 && A.fltClearHover ? 0 : -1, dt); // × in dissolvenza come ogni hover
        if (A.fltW > 0) {
            rrect(fx, fy, A.fltW, fh, px(ui::R_S), C_INPUT);
            if (filtering) rect(fx + px(ui::R_S) * 0.5f, fy + fh - px(ui::EDGE), A.fltW - px(ui::R_S), px(ui::EDGE), C_ACCENT);
            // Imbuto nella colonna delle icone della lista e testo in quella dei nomi: il campo, che
            // comincia come lo sfondo delle righe, si legge allineato alla lista
            drawUiIcon(A.iconX, fy, (float)I.size, fh, IC_FILTER, filtering ? C_ACCENT : C_FAINT);
            float tx = A.nameX;
            if (filtering) {
                float ex = drawText(tx, y, A.filter, C_TEXT, fx + A.fltW - fh);
                rect(ex + 1, y, std::max(1.0f, floorf(s)), (float)F.lineH, C_ACCENT);
                float ha = hoverAlpha(A.fltFade, 0);
                if (ha > 0) rrect(fx + A.fltW - fh, fy, fh, fh, px(ui::R_S), fadeA(C_HOVER_STRONG, ha));
                drawUiIcon(fx + A.fltW - fh, fy, fh, fh, IC_CLOSE, A.fltClearHover ? C_TEXT : C_DIM);
            } else if (A.cur) {
                int n = _snwprintf(buf, 63, L"Filtra %zu elementi\x2026", A.cur->e.size());
                drawText(tx, y, buf, n, C_FAINT, fx + A.fltW - px(ui::GAP_S));
            }
        }
        float leftX = fx + A.fltW + px(ui::GAP_L);
        if (A.cur) {
            uint64_t bytes = 0;
            int k = selCount(&bytes);
            if (k > 1) {
                wchar_t sz[32];
                fmtSize(bytes, sz);
                int n = _snwprintf(buf, 255, L"%d selezionati%s%s   \x00B7   ", k, bytes ? L"  " : L"", bytes ? sz : L"");
                float sx = std::max(leftX, rightX - textWidth(buf, n));
                drawText(sx, y, buf, n, C_ACCENT, rightX);
                rightX = sx;
            }
        }
        float leftMax = rightX - px(ui::GAP_L);
        if (!A.error.empty()) drawText(leftX, y, A.error, A.errorIsInfo ? C_ACCENT : C_ERR, leftMax);
        else if (A.loading && nowMs() - A.loadStart > ui::LOADING_AFTER) drawText(leftX, y, L"caricamento\x2026", 12, C_DIM, leftMax);
    }

    // --- pannello statistiche (F12) ---
    if (A.showStats && A.cur) {
        struct Row { const wchar_t* label; wchar_t val[96]; bool head; };
        Row rows[128]; // con tutte le sezioni aperte se ne usano una settantina: margine, non c'è un controllo per riga
        int nr = 0;
        auto head = [&](const wchar_t* t) { rows[nr].label = t; rows[nr].val[0] = 0; rows[nr].head = true; nr++; };
        auto row = [&](const wchar_t* t, const wchar_t* fmt, double v, const wchar_t* extra) {
            rows[nr].label = t; rows[nr].head = false;
            _snwprintf(rows[nr].val, 95, fmt, v, extra); rows[nr].val[95] = 0; nr++;
        };
        double total = A.startupMs;
        double stFirst = total - A.stWait;
        head(L"Cartella corrente");
        row(L"elementi", L"%.0f%s", (double)A.cur->e.size(), L"");
        row(L"lettura", L"%.2f ms%s", A.cur->enumMs, L"");
        row(L"chiavi di ordinamento", L"%.2f ms%s", A.cur->keyMs, L"");
        row(L"ordinamento", L"%.2f ms%s", A.sortMs, L"");
        row(L"filtro", L"%.2f ms%s", A.filterMs, L"");
        auto row2 = [&](const wchar_t* t, double v, double mx) {
            rows[nr].label = t; rows[nr].head = false;
            _snwprintf(rows[nr].val, 95, L"%.2f ms · peggiore %.2f", v, mx); rows[nr].val[95] = 0; nr++;
        };
        row2(L"frame: preparazione (CPU)", A.frameMs, A.prepMax);
        row2(L"frame: lavoro GPU (timestamp)", A.gpuMs, A.gpuMax);
        row2(L"frame: fine disegno (con attese)", A.execMs, A.execMax);
        row2(L"Present", A.presentMs, A.presentMax);
        {
            rows[nr].label = L"messaggio più lento"; rows[nr].head = false;
            _snwprintf(rows[nr].val, 95, L"0x%04X · %.2f ms · %d sopra 8 ms", A.slowMsg, A.slowMsgMs, A.slowMsgCount);
            rows[nr].val[95] = 0; nr++;
            row(L"attesa vsync più lunga", L"%.2f ms%s", A.waitMax, L"");
        }
        {
            rows[nr].label = L"aggiornamento automatico"; rows[nr].head = false;
            if (A.cur && A.cur->path.empty()) wcscpy(rows[nr].val, L"—");
            else _snwprintf(rows[nr].val, 95, L"%s · %d notifiche · %d ricariche", A.watching ? L"attivo" : L"non attivo",
                            A.notifyCount, A.refreshCount);
            rows[nr].val[95] = 0; nr++;
            if (A.refreshCount) row(L"ultima ricarica (da notifica a schermo)", L"%.1f ms%s", A.refreshLatency, L"");
            if (A.ourMenuMs > 0) row(L"menu contestuale: apertura", L"%.3f ms%s", A.ourMenuMs, L"");
            if (A.dropRegisterMs > 0) row(L"trascinamento: registrazione (OLE)", L"%.1f ms%s", A.dropRegisterMs, L"");
            if (A.dropMs > 0) row(L"trascinamento: rilascio e operazione (thread)", L"%.1f ms%s", A.dropMs, L"");
            if (A.dropUiMax > 0) row(L"trascinamento: chiamata più lenta (UI)", L"%.2f ms%s", A.dropUiMax, L"");
            if (A.fileOpMs > 0) row(L"copia, spostamento, Cestino (thread)", L"%.1f ms%s", A.fileOpMs, L"");
            if (g_zipLastMs > 0) {
                wchar_t ex[48];
                _snwprintf(ex, 47, L" (%ld voci)", (long)g_zipLastEntries); ex[47] = 0;
                row(L"zip: directory centrale (thread)", L"%.1f ms%s", (double)g_zipLastMs, ex);
            }
            if (A.zipMs > 0) {
                wchar_t ex[48];
                _snwprintf(ex, 47, L" (%.1f MB)", A.zipBytes / 1048576.0); ex[47] = 0;
                row(L"zip: ultima estrazione (thread)", L"%.1f ms%s", A.zipMs, ex);
            }
            if (DS.wake) {
                EnterCriticalSection(&DS.lock);
                double ms = DS.busyMs;
                wchar_t ex[64];
                _snwprintf(ex, 63, L" · %llu cartelle · %zu in memoria", (unsigned long long)DS.counted, DS.cache.size()); ex[63] = 0;
                LeaveCriticalSection(&DS.lock);
                row(L"dimensione cartelle (thread)", L"%.0f ms%s", ms, ex);
            }
            if (LK.wake) {
                EnterCriticalSection(&LK.lock);
                double ms = LK.busyMs;
                wchar_t ex[64];
                _snwprintf(ex, 63, L" · %llu letti · peggiore %.1f ms", (unsigned long long)LK.read, LK.maxMs); ex[63] = 0;
                LeaveCriticalSection(&LK.lock);
                row(L"collegamenti (thread)", L"%.1f ms%s", ms, ex);
            }
            if (A.sideMs > 0) row(L"barra laterale: voci locali (thread)", L"%.1f ms%s", A.sideMs, L"");
            if (A.sideSlowN) {
                wchar_t ex[48];
                _snwprintf(ex, 47, L" · %d voci", A.sideSlowN); ex[47] = 0;
                row(L"barra laterale: rete e ottiche (thread)", L"%.1f ms%s", A.sideSlowMs, ex);
            }
            if (A.fdHookMs > 0) row(L"dialoghi: notifica più lenta (UI)", L"%.3f ms%s", A.fdHookMs, L"");
            if (A.fdJumpUs > 0) row(L"dialoghi: ultimo salto (thread)", L"%.1f ms%s", (double)A.fdJumpUs / 1000, L"");
            int nUndo = 0, nRedo = 0;
            undoCounts(nUndo, nRedo);
            if (A.undoRecMs > 0 || nUndo || nRedo) {
                rows[nr].label = L"annulla: cronologia"; rows[nr].head = false;
                _snwprintf(rows[nr].val, 95, L"%d da annullare · %d da ripetere", nUndo, nRedo);
                rows[nr].val[95] = 0; nr++;
                row(L"annulla: registrazione (UI)", L"%.3f ms%s", A.undoRecMs, L"");
            }
            if (A.undoMs > 0) row(L"annulla o ripeti: esecuzione (thread)", L"%.1f ms%s", A.undoMs, L"");
            if (A.shellObjMs > 0) row(L"copia, taglia, trascina, menu (UI)", L"%.2f ms%s", A.shellObjMs, L"");
            if (A.gotoCands) {
                wchar_t ex[48];
                _snwprintf(ex, 47, L" (%d candidati)", A.gotoCands); ex[47] = 0;
                row(L"Vai a: apertura", L"%.2f ms%s", A.gotoOpenMs, ex);
                row2(L"Vai a: ricerca a ogni tasto", A.gotoSearchMs, A.gotoSearchMax);
                if (A.gotoReadMs > 0) {
                    _snwprintf(ex, 47, L" (%d cartelle)", A.gotoReadDirs); ex[47] = 0;
                    row(L"Vai a: sottocartelle (sul thread)", L"%.1f ms%s", A.gotoReadMs, ex);
                }
            }
            if (A.mqMax > 0) row2(L"rettangolo: aggiornamento", A.mqMs, A.mqMax);
            row2(L"percorso: segmenti (UI, per frame)", A.crumbMs, A.crumbMax);
            if (A.colFitMs > 0) row(L"colonne: adatta al contenuto", L"%.2f ms%s", A.colFitMs, L"");
            if (A.prevKind) {
                wchar_t ex[48];
                _snwprintf(ex, 47, L" (%s)", A.prevKind); ex[47] = 0;
                row(L"anteprima: lettura (thread)", L"%.1f ms%s", A.prevReadMs, ex);
                row(L"anteprima: decodifica (thread)", L"%.1f ms%s", A.prevDecodeMs, L"");
                row(L"anteprima: texture (thread UI)", L"%.2f ms%s", A.prevTexMs, L"");
            }
            if (g_wicFirstMs > 0) row(L"primo caricamento di WIC", L"%.1f ms%s", (double)g_wicFirstMs, L" (thread: anteprima o miniature)");
            if (A.th.threads) { // griglia: fonti delle miniature, texture, memoria, prima schermata
                static const wchar_t* src[4] = { L"miniature: cache di Windows (thread)", L"miniature: WIC (thread)",
                                                 L"miniature: shell (thread)", L"miniature: icone grandi (thread)" };
                for (int k = 0; k < 4; k++)
                    if (A.th.n[k]) {
                        wchar_t ex[48];
                        _snwprintf(ex, 47, L" (%d)", A.th.n[k]); ex[47] = 0;
                        row(src[k], L"media %.2f ms%s", A.th.ms[k] / A.th.n[k], ex);
                    }
                row2(L"miniature: texture (thread UI, per frame)", A.th.texMs, A.th.texMax);
                if (A.th.atlasMs > 0) row(L"miniature: creazione dell'atlante (UI)", L"%.2f ms%s", A.th.atlasMs, L"");
                row2(L"griglia: risultati e richieste (UI)", A.th.syncMs, A.th.syncMax);
                row2(L"griglia: disegno delle voci (UI)", A.th.cellsMs, A.th.cellsMax);
                rows[nr].label = L"miniature: in memoria"; rows[nr].head = false;
                _snwprintf(rows[nr].val, 95, L"%d in RAM (%.0f MB) · %d sulla GPU (%.0f MB)", A.th.inRam, A.th.ramMB,
                           A.th.onGpu, A.th.vramMB);
                rows[nr].val[95] = 0; nr++;
                if (A.th.firstMs > 0) row(L"miniature: prima schermata completa", L"%.0f ms%s", A.th.firstMs, L"");
            }
            if (A.menuItems) {
                wchar_t ex[32];
                _snwprintf(ex, 31, L" (%d voci)", A.menuItems); ex[31] = 0;
                row(L"menu della shell: costruzione (thread)", L"%.1f ms%s", A.menuMs, ex);
            }
            if (A.verbItems) {
                wchar_t ex[32];
                _snwprintf(ex, 31, L" (%d voci)", A.verbItems); ex[31] = 0;
                row(L"comando della shell: preparazione (thread)", L"%.1f ms%s", A.verbMs, ex);
            }
            rows[nr].label = L"icone"; rows[nr].head = false;
            _snwprintf(rows[nr].val, 95, L"%d px · %d caricate · %d fallite · media %.2f ms", I.size, I.loaded, I.failed,
                       (I.loaded + I.failed) ? I.totalMs / (I.loaded + I.failed) : 0.0);
            rows[nr].val[95] = 0; nr++;
        }
        {
            wchar_t ex[32];
            _snwprintf(ex, 31, L"%d × %d", G.w, G.h); ex[31] = 0;
            rows[nr].label = L"risoluzione"; rows[nr].head = false;
            wcsncpy(rows[nr].val, ex, 95); nr++;
        }
        head(L"Avvio (dalla creazione del processo)");
        row(L"caricamento → main", L"%.1f ms%s", A.stMain, L"");
        {
            rows[nr].label = L"GPU"; rows[nr].head = false;
            _snwprintf(rows[nr].val, 95, L"%s (%s)", G.adapterName, kGpuPrefName[g_gpuPref]);
            rows[nr].val[95] = 0; nr++;
        }
        row(L"finestra visibile", L"a %.1f ms%s", A.stWindow, L"");
        bool hybrid = g_gpuPref == GPU_HYBRID;
        if (hybrid || G.isWarp) {
            double d = A.switched ? A.warpDevMs : G.deviceMs, w = A.switched ? A.warpSwapMs : G.swapMs;
            row(L"WARP: device + swapchain", L"%.1f ms%s", d + w, L"");
        }
        if (!G.isWarp) {
            row(L"GPU: device (driver)", L"%.1f ms%s", G.deviceMs, hybrid ? L" (in parallelo)" : L"");
            row(L"GPU: altre risorse", L"%.1f ms%s", G.resMs, hybrid ? L" (in parallelo)" : L"");
            row(L"GPU: swapchain", L"%.1f ms%s", G.swapMs, L"");
        }
        row(L"shader", L"%.1f ms%s", g_shaderMs, g_shadersPrecompiled ? L" (precompilati)" : L" (a runtime)");
        row(L"grafica sul thread UI", L"%.1f ms%s", A.stGfx - A.stWindow, L"");
        row(L"font", L"%.1f ms%s", A.stFont - A.stGfx, L"");
        row(L"attesa prima lettura", L"%.1f ms%s", A.stWait - A.stFont, L"");
        row(L"primo frame", L"%.1f ms%s", stFirst, L"");
        {
            wchar_t ex[48];
            _snwprintf(ex, 47, L" (%d glifi)", F.rasterCount); ex[47] = 0;
            row(L"  di cui rasterizzare glifi", L"%.1f ms%s", F.rasterMs, ex);
        }
        row(L"contenuto visibile", L"a %.1f ms%s", total, L"");
        if (A.stActivate >= 0) row(L"attivazione (dopo il primo frame)", L"%.1f ms%s", A.stActivate, L"");
        if (A.iconMs >= 0) row(L"icona della finestra (dopo il primo frame)", L"%.1f ms%s", A.iconMs, L"");
        if (I.firstAt > 0) row(L"prima icona", L"a %.1f ms%s", I.firstAt, L"");
        if (hybrid) {
            if (A.switched) {
                wchar_t ex[48];
                _snwprintf(ex, 47, L" (costo %.1f ms)", A.switchCost); ex[47] = 0;
                row(L"passaggio WARP → GPU", L"a %.1f ms%s", A.switchAt, ex);
            } else {
                rows[nr].label = L"passaggio WARP → GPU"; rows[nr].head = false;
                wcscpy(rows[nr].val, L"in attesa del driver"); nr++;
            }
        }

        float lh = (float)F.lineH + px(ui::GAP_S);
        float pw = px(ui::STATS_W), ph = nr * lh + px(ui::GAP_XL) + lh;
        PanelEnter e = panelEnter(A.statsOpenedAt, ui::DUR_MENU);
        float k = e.k;
        float sx = W - pw - A.padX - A.sbW, sy = H - A.footH - ph - px(ui::GAP_M) - e.dy;
        drawPanel(sx, sy, pw, ph, px(ui::R_M), k);
        float yy = sy + px(ui::GAP_M);
        float lx = sx + px(ui::GAP_L), vx = sx + pw - px(ui::GAP_L);
        for (int i = 0; i < nr; i++, yy += lh) {
            if (rows[i].head) { drawText(lx, yy, rows[i].label, (int)wcslen(rows[i].label), fadeA(C_ACCENT, k)); continue; }
            int vn = (int)wcslen(rows[i].val);
            if (wcscmp(rows[i].label, L"GPU") != 0) decimalComma(rows[i].val, vn); // il nome della scheda video resta com'è
            float vw = (float)textWidth(rows[i].val, vn);
            // Il valore vince: un'etichetta che non ci sta finisce in "…" invece di passarci sotto
            drawText(lx + px(ui::GAP_M), yy, rows[i].label, (int)wcslen(rows[i].label), fadeA(C_DIM, k), vx - vw - px(ui::GAP_M));
            drawText(vx - vw, yy, rows[i].val, vn, fadeA(C_TEXT, k));
        }
        const wchar_t* foot = L"F12 chiude";
        drawText(lx, yy, foot, (int)wcslen(foot), fadeA(C_FAINT, k));
    }

    drawDrop(); // destinazione del trascinamento, sopra liste e barre
    drawOptions(dt);
    drawGoto();
    if (A.menu.open) drawMenu(); // sopra a tutto
    else drawTooltip();
    flushQuads(false);
    A.frameMs = nowMs() - t0; // tempo CPU per costruire e inviare il frame

    // Con F12 aperto: aspetta che la GPU (o WARP sui thread della CPU) abbia finito davvero.
    // Costa latenza, per questo non è sempre attivo.
    if (timing) { G.ctx->End(G.tsEnd); G.ctx->End(G.tsDisjoint); }
    if (A.frameMs > A.prepMax) A.prepMax = A.frameMs;
    if (A.frameMs > ui::LOG_FRAME) hitchLog("frame lento (CPU)", 0, A.frameMs);
    if (A.showStats && G.doneQuery) {
        G.ctx->End(G.doneQuery);
        while (G.ctx->GetData(G.doneQuery, nullptr, 0, 0) == S_FALSE) YieldProcessor();
        A.execMs = nowMs() - t0;
        if (A.execMs > A.execMax) A.execMax = A.execMs;
        if (timing) {
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj;
            UINT64 b = 0, e = 0;
            while (G.ctx->GetData(G.tsDisjoint, &dj, sizeof(dj), 0) == S_FALSE) YieldProcessor();
            if (G.ctx->GetData(G.tsBegin, &b, sizeof(b), 0) == S_OK && G.ctx->GetData(G.tsEnd, &e, sizeof(e), 0) == S_OK &&
                !dj.Disjoint && dj.Frequency) {
                A.gpuMs = (double)(e - b) * 1000.0 / (double)dj.Frequency;
                if (A.gpuMs > A.gpuMax) A.gpuMax = A.gpuMs;
            }
        }
    }
    double tp = nowMs();
    HRESULT pr = G.swap->Present(1, 0);
    A.presentMs = nowMs() - tp;
    if (pr == DXGI_ERROR_DEVICE_REMOVED || pr == DXGI_ERROR_DEVICE_RESET) {
        gpuLost(pr == DXGI_ERROR_DEVICE_REMOVED ? G.dev->GetDeviceRemovedReason() : pr);
        return; // A.dirty resta: il primo frame del device nuovo ridisegna tutto
    }
    // Il primo Present su una swapchain nuova della GPU (passaggio da WARP, ripristino) è lento per il
    // driver, ogni volta: lo si registra col suo nome, così nel log non sembra un rallentamento nuovo
    if (A.presentMs > ui::LOG_FRAME) hitchLog(A.firstGpuPresent ? "primo Present GPU" : "Present lento", 0, A.presentMs);
    A.firstGpuPresent = false;
    if (A.showStats && A.presentMs > A.presentMax) A.presentMax = A.presentMs;
    A.dirty = false;
    A.prevAnimating = animating();
    if (!A.firstFrameDone) {
        A.firstFrameDone = true;
        A.startupMs = processAgeMs();
        A.dirty = true; // un frame in più per mostrare il tempo di avvio
    }
}

