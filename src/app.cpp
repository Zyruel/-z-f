// [z]f — logica della vista: layout, selezione, filtro, navigazione, ricariche, formattazione.

// Spazio per le colonne dopo il nome (Destinazione, Data, Dimensione): tutto tranne il minimo del nome
static float colRoom() { return A.listR - A.sbW - A.padX - A.listX - px(ui::COL_NAME_MIN); }

// Colonna Destinazione: in Dettagli, nelle cartelle con almeno un collegamento (links.cpp)
static bool linkColumn() { return A.zoom == ZOOM_DETAILS && A.cur && !A.cur->lnk.empty(); }

// Larghezza massima di Destinazione con Data in dateX: mai più larga di Nome. Senza questo
// limite, con la finestra stretta Nome restava al suo minimo e lo spazio in più andava tutto a
// Destinazione; così le due colonne crescono e calano insieme.
static float linkMax(float dateX) { return floorf((dateX - A.listX) * ui::LINK_MAX); }

// Larghezza dell'anteprima che la lista lascia libera con le colonne al minimo. L'anteprima tiene
// la larghezza scelta e le colonne si adattano allo spazio che resta: se l'anteprima prendesse
// solo quello che avanza dalle colonne alla larghezza scelta, ogni colonna stretta col mouse la
// allargherebbe, e allargare una colonna sarebbe impossibile (ci si scontra con l'anteprima).
static float previewRoom() {
    float cols = px(ui::COL_MIN) * (linkColumn() ? 3 : 2);
    return floorf((float)G.w - A.listX - px(ui::COL_NAME_MIN) - cols - A.padX - A.sbW);
}

// In basso: l'altezza che lascia alla lista l'intestazione, PREVIEW_LIST_ROWS righe e le barre in fondo
static float previewRoomH() {
    return floorf((float)G.h - A.listTop - A.rowH * ui::PREVIEW_LIST_ROWS - A.footH);
}

// Anteprima: la misura scelta (larghezza a destra, altezza in basso), al massimo PREVIEW_MAX della
// finestra e quello che la lista lascia (a destra con le colonne al minimo, in basso con
// PREVIEW_LIST_ROWS righe); sotto il minimo sparisce (resta aperta: torna quando c'è spazio).
static float previewSize() {
    if (!A.prevOpen) return 0;
    if (A.prevBottom) {
        float h = std::min({px(A.prevHeight), floorf(G.h * ui::PREVIEW_MAX), previewRoomH()});
        return h >= px(ui::PREVIEW_HMIN) ? h : 0;
    }
    float w = std::min({px(A.prevWidth), floorf(G.w * ui::PREVIEW_MAX), previewRoom()});
    return w >= px(ui::PREVIEW_MIN) ? w : 0;
}

// Punto dentro il pannello di anteprima (intestazione, corpo e banda in fondo)
static bool inPreview(int x, int y) {
    return A.prevW > 0 && x >= A.prevX && x < A.prevX + A.prevW && y >= A.prevY && y < A.prevY + A.prevH;
}
// Riga della barra in fondo alla lista di App: la barra di stato della lista principale (sopra
// l'anteprima, se è in basso) o, nel PaneScope dell'anteprima, la sua banda in fondo alla finestra
static float barTop() { return (g_inPane ? (float)G.h : A.footY) - A.statusH; }
static bool inBar(int y) { return y >= barTop() && y < barTop() + A.statusH; }
// Barra dei tempi e di stato della lista principale
static bool inListFoot(int x, int y) {
    return x >= A.sideW && x < A.listR && y >= A.footY - A.footH && y < A.footY;
}

static float lineLen();
static float viewLen();
static bool cursorInView(float s, bool whole);
static void ensureVisible();

// La geometria di prima, presa prima di cambiare i bordi: il cursore che si vedeva resta in vista
// anche con quella nuova (l'anteprima che si apre stringe Elenco e la colonna del cursore uscirebbe
// a destra). Se l'utente ha scorso lontano dal cursore la lista non si muove.
struct LayoutKeep { bool cursor; int perLine; float line, view; };
static LayoutKeep layoutKeep() { return { cursorInView(A.scrollY, false), A.perLine, lineLen(), viewLen() }; }

// Geometria delle voci dentro listX…listR, da listTop per listH, nella vista di A.zoom: colonne e
// celle. La lista principale (layout) e l'anteprima di una cartella (preview.cpp, nel suo
// PaneScope), che in Dettagli non ha Destinazione e, a destra, nemmeno Data (noDate).
static void layoutItems(bool noDate, const LayoutKeep& k) {
    // Colonne: Nome prende quello che avanza, Destinazione (se c'è), Data e Dimensione sono
    // allineate a destra. Se la finestra è stretta si stringono prima Destinazione, poi Data e poi
    // Dimensione, fino al minimo (senza salvare); Destinazione inoltre non supera Nome (linkMax).
    A.dateW = noDate ? 0 : px(A.colDate);
    A.sizeW = px(A.colSize);
    A.linkW = !noDate && linkColumn() ? px(A.colLink) : 0;
    float right = A.listR - A.sbW - A.padX, room = colRoom(), mn = px(ui::COL_MIN);
    if (A.linkW > 0 && A.linkW + A.dateW + A.sizeW > room) A.linkW = std::max(mn, room - A.dateW - A.sizeW);
    if (A.dateW > 0 && A.linkW + A.dateW + A.sizeW > room) A.dateW = std::max(mn, room - A.sizeW - A.linkW);
    if (A.linkW + A.dateW + A.sizeW > room) A.sizeW = std::max(mn, room - A.dateW - A.linkW);
    A.sizeX = right - A.sizeW;
    A.dateX = A.sizeX - A.dateW;
    if (A.linkW > 0) A.linkW = std::max(mn, std::min(A.linkW, linkMax(A.dateX)));
    A.linkX = A.dateX - A.linkW;
    A.nameEnd = A.linkX - A.padX; // senza Destinazione linkX è dateX
    // Voci: in Dettagli una colonna di righe. In Icone il riquadro della miniatura a GAP_M dai
    // bordi, sotto il nome su GRID_NAME_LINES righe; le voci di una riga si dividono la larghezza.
    // In Elenco colonne larghe LIST_COL_W di righe alte una riga, quante ne stanno sopra la barra di
    // scorrimento orizzontale (alta sbW, in fondo alla lista).
    float lw = itemsRight() - A.listX;
    if (gridView()) {
        float m = px(ui::GAP_S), pad = px(ui::GAP_M);
        A.thumbPx = px(ui::THUMB_PX[A.zoom - ZOOM_ICONS]);
        A.perLine = std::max(1, (int)((lw - m * 2) / (A.thumbPx + pad * 2)));
        A.cellW = floorf((lw - m * 2) / A.perLine);
        A.cellX = A.listX + m;
        A.cellH = pad * 2 + A.thumbPx + px(ui::GAP_S) + ui::GRID_NAME_LINES * (float)F.lineH +
                  (ui::GRID_NAME_LINES - 1) * px(ui::LINE_GAP);
    } else if (listView()) {
        float m = px(ui::GAP_XS); // più GAP_XS di fondo: le righe cominciano a GAP_S come in Dettagli
        A.thumbPx = 0;
        A.cellH = A.rowH;
        A.perLine = std::max(1, (int)((A.listH - A.sbW) / A.rowH));
        A.cellW = std::max(1.0f, std::min(px(ui::LIST_COL_W), lw - m * 2));
        A.cellX = A.listX + m;
    } else {
        A.thumbPx = 0;
        A.perLine = 1;
        A.cellW = lw;
        A.cellX = A.listX;
        A.cellH = A.rowH;
    }
    bool changed = A.perLine != k.perLine || lineLen() != k.line || viewLen() != k.view;
    if (k.cursor && changed && viewLen() > 0 && !cursorInView(A.scrollTarget, true)) ensureVisible();
}

static void layout() {
    if (g_inPane) { g_paneAfter.push_back(layout); return; } // App è l'anteprima (PaneScope): all'uscita
    LayoutKeep keep = layoutKeep();
    A.rowH = (float)(F.lineH + (int)lroundf(ui::ROW_EXTRA * A.scale)); // non px(): vedi ROW_EXTRA in theme.h
    A.topH = (float)(int)(A.rowH * ui::PATH_BAR);
    A.headH = A.rowH;
    A.statusH = A.rowH;
    A.sbW = px(ui::SCROLL_W);
    A.padX = px(ui::PAD_X);
    // Dall'alto: [schede] [barra del percorso] poi [barra laterale | intestazione + lista |
    // anteprima]. I due pannelli arrivano in fondo alla finestra; tra loro, sotto la lista, la barra
    // dei tempi (se scelta) e quella di stato, con il filtro: sono del corpo centrale.
    // Le schede sono sempre visibili: se comparissero alla seconda, il contenuto salterebbe in
    // basso proprio mentre si sta cliccando.
    A.tabH = floorf(A.rowH * ui::TAB_BAR);
    A.tabY = 0;
    // Barra del titolo: [⚙] schede [+] · spazio per trascinare · [–][□][×]
    A.btnW = A.tabH;
    A.capW = px(ui::CAPTION_W);
    A.navW = floorf(A.rowH * ui::NAV_W);
    A.navMenuW = floorf(A.rowH * ui::NAV_MENU_W);
    A.tabStripX = A.btnW + px(ui::GAP_S);
    A.tabStripW = std::max(0.0f, G.w - A.capW * 3 - px(ui::DRAG_MIN) - A.tabStripX - A.rowH * ui::TAB_PLUS_W);
    A.pathY = A.tabH;
    A.sideTop = A.pathY + A.topH;
    A.sideItemH = A.rowH;
    A.sideW = A.sideVisible ? std::min(px(A.sideWidth), floorf(G.w * ui::SIDE_MAX)) : 0;
    A.listX = A.sideW;
    A.listTop = A.sideTop + A.headH;
    A.timesH = A.timesBar ? A.rowH : 0;
    A.footH = A.statusH + A.timesH;
    // Anteprima a destra: dall'intestazione al fondo, la lista alla sua sinistra. In basso: sotto le
    // barre della lista, larga quanto la lista, fino al fondo
    float ps = previewSize();
    if (A.prevBottom) {
        A.prevX = A.listX;
        A.prevW = ps > 0 ? (float)G.w - A.listX : 0;
        A.prevH = ps;
        A.prevY = (float)G.h - ps;
        A.listR = (float)G.w;
    } else {
        A.prevW = ps;
        A.prevX = (float)G.w - ps;
        A.prevY = A.sideTop;
        A.prevH = ps > 0 ? (float)G.h - A.sideTop : 0;
        A.listR = A.prevX;
    }
    A.footY = A.prevBottom && A.prevW > 0 ? A.prevY : (float)G.h;
    A.listH = std::max(0.0f, A.footY - A.listTop - A.footH);
    // Filtro: all'inizio della barra di stato, allineato alla lista. Questa è la larghezza voluta:
    // quella mostrata (fltW) la decide il disegno, che con la lista stretta la cede a contatori e vista
    A.fltX = A.listX + px(ui::GAP_S);
    A.fltWant = floorf(std::min(px(ui::FILTER_W), (A.listR - A.listX) * ui::FILTER_MAX));
    A.fltW = A.fltWant;
    layoutItems(false, keep);
}

// Bordo b tra due colonne: 0 a sinistra di Data, 1 a sinistra di Dimensione, 2 a sinistra di
// Destinazione (se c'è); a metà dello spazio tra il testo di una colonna e quello della successiva.
// Ogni bordo comanda la colonna alla sua destra: colChosen è il valore scelto (impostazioni.txt),
// colShown la larghezza mostrata adesso.
static float colBorderX(int b) { return (b == 0 ? A.dateX : b == 1 ? A.sizeX : A.linkX) - floorf(A.padX * 0.5f); }
// Oltre ai bordi delle colonne si trascinano con lo stesso gesto quelli dei pannelli (barra
// laterale, anteprima): cursore ↔, Esc torna a prima, si salva al rilascio (listmouse.cpp).
enum { EDGE_SIDE = 3, EDGE_PREVIEW = 4 };
static float& colChosen(int b) {
    return b == 0 ? A.colDate : b == 1 ? A.colSize : b == 2 ? A.colLink : b == EDGE_SIDE ? A.sideWidth
         : A.prevBottom ? A.prevHeight : A.prevWidth;
}
static float colShown(int b) {
    return b == 0 ? A.dateW : b == 1 ? A.sizeW : b == 2 ? A.linkW : b == EDGE_SIDE ? A.sideW
         : A.prevBottom ? A.prevH : A.prevW;
}
// Bordo di un pannello sotto il mouse: EDGE_SIDE, EDGE_PREVIEW o -1. Barra laterale per tutta
// l'altezza: COL_GRIP per lato (la lista ha GAP_S di margine). Anteprima per tutto il suo lato:
// COL_GRIP verso il pannello, solo GAP_XS verso la lista per non prendere la barra di scorrimento
// (in basso, quella orizzontale di Elenco).
static int panelEdgeAt(int x, int y) {
    if (y < A.sideTop) return -1;
    float grip = px(ui::COL_GRIP), in = px(ui::GAP_XS);
    if (A.sideW > 0 && x >= A.sideW - grip && x < A.sideW + grip) return EDGE_SIDE;
    if (A.prevW <= 0) return -1;
    if (A.prevBottom) {
        if (x >= A.prevX && y >= A.prevY - in && y < A.prevY + grip) return EDGE_PREVIEW;
    } else if (x >= A.prevX - in && x < A.prevX + grip) {
        return EDGE_PREVIEW;
    }
    return -1;
}
// Il bordo b si trascina in verticale (quello dell'anteprima in basso) e non in orizzontale
static bool edgeVertical(int b) { return b == EDGE_PREVIEW && A.prevBottom; }
static int colBorderAt(int x, int y) {
    if (A.zoom != ZOOM_DETAILS) return -1; // in Elenco e Icone l'intestazione ha solo le etichette per ordinare
    if (y < A.sideTop || y >= A.listTop || x < A.listX || x >= A.listR - A.sbW) return -1;
    for (int b = A.linkW > 0 ? 2 : 1; b >= 0; b--)
        if (fabsf(x - colBorderX(b)) <= px(ui::COL_GRIP)) return b;
    return -1;
}
// Colonna dell'intestazione sotto x; in Elenco e Icone l'etichetta (sortX, dal disegno), -1 fuori
static int colAt(int x) {
    if (A.zoom != ZOOM_DETAILS) {
        for (int c = 0; c < 3; c++)
            if (x >= A.sortX[c] - px(ui::GAP_S) && x < A.sortX[c + 1] - px(ui::GAP_XL) + px(ui::GAP_S)) return c;
        return -1;
    }
    return x >= colBorderX(1) ? COL_SIZE : x >= colBorderX(0) ? COL_DATE : A.linkW > 0 && x >= colBorderX(2) ? COL_LINK : COL_NAME;
}

// --- animazioni ---------------------------------------------------------------------------
// Avvicinamento esponenziale: costante di tempo tau in ms (circa 3·tau per arrivare al 95%).
static float approach(float v, float target, double dt, float tau, float eps) {
    if (!A.animations || tau <= 0) return target;
    float d = target - v;
    if (fabsf(d) <= eps) return target;
    A.animActive = true;
    return v + d * (1.0f - expf(-(float)dt / tau));
}

// target < 0: nessuna voce sotto il mouse
static void hoverFadeUpdate(HoverFade& h, int target, double dt) {
    if (target < 0) target = -1;
    if (target != h.cur) {
        if (target >= 0 && target == h.prev) { std::swap(h.cur, h.prev); std::swap(h.a, h.pa); } // ritorno rapido
        else { if (h.cur >= 0) { h.prev = h.cur; h.pa = h.a; } h.cur = target; h.a = 0; }
    }
    h.a = h.cur >= 0 ? approach(h.a, 1, dt, ui::TAU_HOVER_IN, 0.02f) : 0;
    h.pa = approach(h.pa, 0, dt, ui::TAU_HOVER_OUT, 0.02f);
    if (h.pa <= 0) h.prev = -1;
}
static float hoverAlpha(const HoverFade& h, int i) { return i < 0 ? 0 : i == h.cur ? h.a : i == h.prev ? h.pa : 0; }

static float easeOut(float t) { t = t < 0 ? 0 : (t > 1 ? 1 : t); float u = 1 - t; return 1 - u * u * u; }

// --- scorrimento: le voci stanno su linee (righe; colonne in Elenco), che scorrono lungo un asse
// Lunghezza di una linea e spazio visibile lungo l'asse
static float lineLen() { return listView() ? A.cellW : A.cellH; }
static float viewLen() { return listView() ? itemsRight() - A.listX - (A.cellX - A.listX) * 2 : A.listH; }
static float contentLen() { return (float)((A.view.size() + A.perLine - 1) / A.perLine) * lineLen(); }
static float maxScroll() { return std::max(0.0f, contentLen() - viewLen()); }
static void clampScroll() {
    A.scrollTarget = std::min(std::max(A.scrollTarget, 0.0f), maxScroll());
    A.scrollY = std::min(std::max(A.scrollY, 0.0f), maxScroll());
}
// Rotellina sulla lista di App (anche l'anteprima nel suo PaneScope), con lo scorrimento morbido.
// Icone: una riga di voci a scatto; Elenco: una colonna (scorre in orizzontale)
static void listWheel(float lines) {
    A.scrollTarget -= lines * (gridView() ? A.cellH : listView() ? A.cellW : A.rowH * ui::WHEEL_LINES);
    clampScroll();
    A.dirty = true;
}

// Di quanto lo scorrimento sposta le voci (pixel interi): in y, in Elenco in x
static void scrollOffset(float& ox, float& oy) {
    float s = floorf(A.scrollY);
    ox = listView() ? s : 0;
    oy = listView() ? 0 : s;
}

// --- voci: un solo posto per "rettangolo di una voce" e "voce sotto il punto" ----------------
// Dettagli, Elenco e Icone passano da qui (in Dettagli: una colonna alta una riga).
struct Box { float x, y, w, h; };
// Rettangolo della voce vi nel contenuto, senza scorrimento: x come nella finestra, y da 0 =
// inizio della lista. Nella finestra: (x - ox, listTop + y - oy), con scrollOffset.
static Box itemRect(int vi) {
    int line = vi / A.perLine, k = vi % A.perLine;
    if (listView()) return { A.cellX + line * A.cellW, k * A.cellH, A.cellW, A.cellH };
    return { A.cellX + k * A.cellW, line * A.cellH, A.cellW, A.cellH };
}
// Fondo di hover, selezione e cursore: in Dettagli con un margine di GAP_S ai lati, in Elenco di
// GAP_XS ai lati (le colonne non si toccano, le righe sì), in Icone di GAP_XS tutto intorno
static Box itemBg(int vi) {
    Box b = itemRect(vi);
    if (A.zoom == ZOOM_DETAILS) return { A.listX + px(ui::GAP_S), b.y, b.w - px(ui::GAP_S) * 2, b.h };
    float m = px(ui::GAP_XS);
    if (listView()) return { b.x + m, b.y, b.w - m * 2, b.h };
    return { b.x + m, b.y + m, b.w - m * 2, b.h - m * 2 };
}
// Elenco: icona, inizio e fine del nome di una voce (x come itemRect). Icona e nome stanno nella
// colonna dove stanno nella riga di Dettagli (A.iconX, A.nameX, misurati dal disegno)
struct RowParts { float iconX, nameX, nameEnd; };
static RowParts listParts(int vi) {
    Box b = itemRect(vi), bg = itemBg(vi);
    float d = b.x - A.cellX;
    return { A.iconX + d, A.nameX + d, bg.x + bg.w - px(ui::GAP_S) };
}
// Griglia: riquadro della miniatura e spazio del nome di una voce (stesse coordinate di itemRect)
struct CellParts { Box thumb, name; };
static CellParts gridParts(int vi) {
    Box b = itemRect(vi);
    float pad = px(ui::GAP_M), tx = floorf(b.x + (b.w - A.thumbPx) * 0.5f), ny = b.y + pad + A.thumbPx + px(ui::GAP_S);
    return { { tx, b.y + pad, A.thumbPx, A.thumbPx }, { b.x + px(ui::GAP_S), ny, b.w - px(ui::GAP_S) * 2, b.y + b.h - pad - ny } };
}

// Voce sotto il punto (coordinate della finestra), -1 se nessuna
static int itemAt(int x, int y) {
    if (y < A.listTop || y >= A.listTop + A.listH || x < A.listX || x >= itemsRight()) return -1;
    float ox, oy;
    scrollOffset(ox, oy);
    float cx = x + ox - A.cellX, cy = y - A.listTop + oy;
    int line, k;
    if (listView()) { line = (int)floorf(cx / A.cellW); k = (int)floorf(cy / A.cellH); }
    else { line = (int)floorf(cy / A.cellH); k = gridView() ? (int)floorf(cx / A.cellW) : 0; }
    if (line < 0 || k < 0 || k >= A.perLine) return -1;
    int i = line * A.perLine + k;
    return i < (int)A.view.size() ? i : -1;
}
// Prima voce visibile (per tenerla in vista quando cambia la geometria)
static int firstVisibleItem() { return lineLen() > 0 ? (int)(A.scrollY / lineLen()) * A.perLine : 0; }
static void scrollToItem(int vi) {
    A.scrollY = A.scrollTarget = (float)(vi / A.perLine) * lineLen();
}

// Il cursore, con lo scorrimento s, si vede tutto (whole) o almeno in parte
static bool cursorInView(float s, bool whole) {
    if (A.sel < 0 || A.sel >= (int)A.view.size() || A.perLine < 1) return false;
    float len = lineLen(), view = viewLen(), top = (float)(A.sel / A.perLine) * len;
    if (len <= 0 || view <= 0) return false;
    return whole ? top >= s && top + len <= s + view : top < s + view && top + len > s;
}

static void ensureVisible() {
    if (A.view.empty()) return;
    float top = (float)(A.sel / A.perLine) * lineLen(), len = lineLen(), view = viewLen();
    float s = A.scrollTarget;
    if (top < s) s = top;
    else if (top + len > s + view) s = top + len - view;
    A.scrollTarget = A.scrollY = s; // da tastiera: niente animazione, deve sembrare immediato
    clampScroll();
}

// ---------------------------------------------------------------------------
// Selezione multipla
// ---------------------------------------------------------------------------

static inline bool isSel(uint32_t ei) { return ei < A.selFlags.size() && A.selFlags[ei]; }
static void selReset() { A.selFlags.assign(A.cur ? A.cur->e.size() : 0, 0); A.anchor = -1; }
static void selClear() { std::fill(A.selFlags.begin(), A.selFlags.end(), (uint8_t)0); }
static int viewIndexOf(int ei) {
    if (ei < 0) return -1;
    for (int i = 0; i < (int)A.view.size(); i++) if ((int)A.view[i] == ei) return i;
    return -1;
}
static void selOnly(int vi) {
    selClear();
    if (vi >= 0 && vi < (int)A.view.size()) { A.selFlags[A.view[vi]] = 1; A.anchor = (int)A.view[vi]; }
}
static void selRange(int a, int b) { // indici di vista, inclusi
    if (a > b) std::swap(a, b);
    a = std::max(a, 0);
    b = std::min(b, (int)A.view.size() - 1);
    for (int i = a; i <= b; i++) A.selFlags[A.view[i]] = 1;
}
static int selCount(uint64_t* bytes) {
    int n = 0;
    uint64_t b = 0;
    for (uint32_t ei : A.view)
        if (A.selFlags[ei]) {
            n++;
            if (!(A.cur->e[ei].attr & FILE_ATTRIBUTE_DIRECTORY)) b += A.cur->e[ei].size;
        }
    if (bytes) *bytes = b;
    return n;
}
// Toglie dalla selezione ciò che il filtro nasconde: le operazioni future agiranno solo su ciò che si vede
static void selKeepVisible() {
    std::vector<uint8_t> vis(A.selFlags.size(), 0);
    for (uint32_t ei : A.view) vis[ei] = A.selFlags[ei];
    A.selFlags.swap(vis);
}

// Applica il filtro all'ordine corrente (sottostringa, maiuscole/minuscole indifferenti)
static void applyFilter() {
    double t0 = nowMs();
    A.view.clear();
    if (A.filter.empty()) {
        A.view = A.order;
    } else {
        std::wstring needle = A.filter;
        CharLowerBuffW(&needle[0], (DWORD)needle.size());
        A.view.reserve(A.order.size());
        for (uint32_t i : A.order)
            if (wcsstr(A.cur->lowerName(A.cur->e[i]), needle.c_str())) A.view.push_back(i);
    }
    A.filterMs = nowMs() - t0;
    A.countsDirty = true; // un filtro diverso può dare lo stesso numero di voci, ma non le stesse
}

static void rebuildView(bool keepSelection) {
    int before = A.cur ? selCount(nullptr) : 0;
    uint32_t selEntry = (keepSelection && A.sel >= 0 && A.sel < (int)A.view.size()) ? A.view[A.sel] : UINT32_MAX;
    applyFilter();
    A.sel = 0;
    if (selEntry != UINT32_MAX)
        for (int i = 0; i < (int)A.view.size(); i++)
            if (A.view[i] == selEntry) { A.sel = i; break; }
    // Selezione multipla: restano selezionate le voci ancora visibili.
    // Una sola voce selezionata è di fatto il cursore: col filtro digitato passa al primo risultato.
    selKeepVisible();
    if ((!keepSelection && before <= 1) || selCount(nullptr) == 0) selOnly(A.sel);
    clampScroll();
    ensureVisible();
}

static void resort() {
    if (!A.cur) return;
    double t0 = nowMs();
    if (A.sortCol == COL_SIZE) dirSizeFill(A.cur); // le cartelle col totale già contato
    sortListing(A.cur, A.order, A.sortCol, A.sortDesc);
    A.sortMs = nowMs() - t0;
    rebuildView(true);
}

static bool parentOf(std::wstring p, std::wstring& parent, std::wstring& child);

static void navigate(const std::wstring& path, int history, const std::wstring& selectName, bool refresh = false) {
    if (g_inPane) {
        // Dall'anteprima di una cartella (PaneScope): la lista principale si muove all'uscita. Aprire
        // una sua voce (cartella, zip) non scende nell'anteprima e non sposta la lista: la apre in una
        // nuova scheda in background, come il clic centrale (scelta con Zyruel: per ora niente
        // navigazione dentro l'anteprima)
        std::wstring here = A.cur ? A.cur->path : L"", p = path, s = selectName, parent, child;
        if (s.empty() && !refresh && parentOf(p, parent, child) && !_wcsicmp(parent.c_str(), here.c_str())) {
            g_paneAfter.push_back([p] { tabNew(p, false); });
            return;
        }
        g_paneAfter.push_back([p, history, s, refresh] { navigate(p, history, s, refresh); });
        return;
    }
    ListRequest* r = new ListRequest{ A.hwnd, ++A.gen, path, A.sortCol, A.sortDesc, refresh };
    A.refreshAgain = false; // la lettura che parte ora vede già tutti i cambiamenti arrivati finora
    if (refresh) {
        A.refreshInFlight = true;
        HANDLE h = CreateThread(nullptr, 0, listThread, r, 0, nullptr);
        if (h) CloseHandle(h); else { delete r; A.refreshInFlight = false; }
        return;
    }
    A.refreshInFlight = false; // la generazione nuova scarta la ricarica in corso
    A.loading = true;
    A.loadStart = nowMs();
    A.loadPath = path;
    A.pendingHistory = history;
    A.pendingSelect = selectName;
    HANDLE h = CreateThread(nullptr, 0, listThread, r, 0, nullptr);
    if (h) CloseHandle(h); else { delete r; A.loading = false; }
    SetTimer(A.hwnd, 2, ui::LOADING_AFTER, nullptr); // per mostrare "caricamento…" se è lento
}

static const wchar_t* errText(DWORD e) {
    switch (e) {
    case ERROR_ACCESS_DENIED:    return L"Accesso negato";
    case ERROR_PATH_NOT_FOUND:
    case ERROR_FILE_NOT_FOUND:   return L"Percorso non trovato";
    case ERROR_DIRECTORY:        return L"Non è una cartella";
    case ERROR_NOT_READY:        return L"Dispositivo non pronto";
    case ERROR_BAD_NETPATH:      return L"Percorso di rete non trovato";
    case ERROR_INVALID_NAME:     return L"Nome non valido";
    case ERROR_BAD_FORMAT:       return L"Archivio zip non valido o non supportato";
    default:                     return L"Impossibile aprire la cartella";
    }
}


static void applyRefresh(Listing* l) {
    // Stato da conservare
    std::wstring selName, anchorName, renameName;
    int oldSel = A.sel;
    if (A.cur && A.sel >= 0 && A.sel < (int)A.view.size()) selName = A.cur->name(A.cur->e[A.view[A.sel]]);
    // La voce in rinomina per nome suo: il cursore può essersi spostato durante la rinomina
    if (A.cur && A.renaming && A.renameEntry >= 0 && A.renameEntry < (int)A.cur->e.size())
        renameName = A.cur->name(A.cur->e[A.renameEntry]);
    if (A.cur && A.anchor >= 0 && A.anchor < (int)A.cur->e.size()) anchorName = A.cur->name(A.cur->e[A.anchor]);
    std::unordered_map<std::wstring, int> selNames; // nomi selezionati (insensibili a maiuscole)
    if (A.cur)
        for (size_t i = 0; i < A.selFlags.size(); i++)
            if (A.selFlags[i]) {
                std::wstring n = A.cur->name(A.cur->e[i]);
                CharLowerBuffW(&n[0], (DWORD)n.size());
                selNames[n] = 1;
            }
    float sy = A.scrollY, st = A.scrollTarget;

    delete A.cur;
    A.cur = l;
    A.order.swap(l->order);
    std::vector<uint32_t>().swap(l->order);
    A.sortMs = l->sortMs;
    applyFilter();

    A.selFlags.assign(l->e.size(), 0);
    A.anchor = -1;
    if (!selNames.empty() || !anchorName.empty()) {
        std::wstring n;
        for (size_t i = 0; i < l->e.size(); i++) {
            n.assign(l->name(l->e[i]), l->e[i].nameLen);
            CharLowerBuffW(&n[0], (DWORD)n.size());
            if (selNames.count(n)) A.selFlags[i] = 1;
            if (A.anchor < 0 && !anchorName.empty() && _wcsicmp(l->name(l->e[i]), anchorName.c_str()) == 0) A.anchor = (int)i;
        }
    }
    selKeepVisible();
    A.sel = -1;
    if (!selName.empty())
        for (int i = 0; i < (int)A.view.size(); i++)
            if (_wcsicmp(l->name(l->e[A.view[i]]), selName.c_str()) == 0) { A.sel = i; break; }
    if (A.sel < 0) A.sel = std::max(0, std::min(oldSel, (int)A.view.size() - 1)); // eliminato: resta lì
    if (!A.view.empty() && selCount(nullptr) == 0) selOnly(A.sel); // eliminata: si seleziona quella che ne prende il posto
    A.scrollY = sy;
    A.scrollTarget = st;
    clampScroll();

    // Prima la rinomina già aperta, poi quella della nuova cartella: beginRename imposta già la voce
    // giusta nella lista nuova, e questo controllo la rimetterebbe su un nome della lista vecchia
    if (A.renaming) { // la voce in rinomina esiste ancora?
        A.renameEntry = -1;
        if (!renameName.empty())
            for (size_t i = 0; i < l->e.size(); i++)
                if (_wcsicmp(l->name(l->e[i]), renameName.c_str()) == 0) { A.renameEntry = (int)i; break; }
        if (A.renameEntry < 0) A.renaming = false;
    }
    if (!A.selectAfterRefresh.empty()) { // dopo una rinomina
        for (int i = 0; i < (int)A.view.size(); i++)
            if (_wcsicmp(l->name(l->e[A.view[i]]), A.selectAfterRefresh.c_str()) == 0) {
                A.sel = i;
                selOnly(i);
                ensureVisible();
                A.selectAfterRefresh.clear();
                if (A.renameAfterRefresh) { A.renameAfterRefresh = false; beginRename(); }
                break;
            }
    }
    A.refreshCount++;
    A.refreshLatency = nowMs() - A.refreshStart;
    A.dirty = true;
}

static void onListing(Listing* l) {
    // Risposta superata: chi ha cambiato la generazione ha già sistemato refreshInFlight (navigazione,
    // cambio di scheda) o ha lanciato una ricarica nuova, che è quella in corso (cartella spostata)
    if (l->gen != A.gen) { delete l; return; }
    if (l->refresh) A.refreshInFlight = false;
    if (l->refresh) {
        if (l->err) { // la cartella non esiste più
            std::wstring p = l->path;
            delete l;
            folderGone(p);
            return;
        }
        std::wstring oldPath = A.cur ? A.cur->path : L"";
        applyRefresh(l);
        dirSizeForget(l->path); // è cambiato qualcosa qui dentro: il totale di qui e di sopra non vale più
        dirSizeQueueCurrent();
        linksQueueCurrent(); // collegamenti nuovi o cambiati (gli altri sono già arrivati con la lettura)
        if (_wcsicmp(oldPath.c_str(), l->path.c_str()) != 0) { // cartella seguita dopo uno spostamento
            startWatch(l->path);
            updateTitle();
        }
        if (A.refreshAgain) { A.refreshAgain = false; requestRefresh(); }
        return;
    }
    A.loading = false;
    if (l->err) {
        wchar_t buf[512];
        _snwprintf(buf, 511, L"%s: %s (codice %lu)", errText(l->err), l->path.c_str(), (unsigned long)l->err);
        buf[511] = 0;
        A.noticeKeep = false; // questo errore sostituisce l'avviso che doveva restare
        notice(buf, false);
        if (l->err == ERROR_PATH_NOT_FOUND || l->err == ERROR_FILE_NOT_FOUND || l->err == ERROR_DIRECTORY ||
            l->err == ERROR_INVALID_NAME)
            recentForget(l->path); // sparita: "Vai a" non la propone più (un'unità non pronta invece resta)
        bool first = !A.cur;
        delete l;
        A.dirty = true;
        static bool fellBack = false;
        if (first && !fellBack) { // il percorso iniziale non esiste: ripiego (una volta) sulla cartella utente
            fellBack = true;
            wchar_t home[MAX_PATH];
            DWORD n = GetEnvironmentVariableW(L"USERPROFILE", home, MAX_PATH);
            navigate((n && n < MAX_PATH) ? home : L"C:\\", 0, L"");
        }
        return;
    }
    if (A.cur) {
        if (A.pendingHistory == 1) { A.back.push_back(A.cur->path); A.fwd.clear(); }
        else if (A.pendingHistory == 2) { A.fwd.push_back(A.cur->path); }
        else if (A.pendingHistory == 3) { A.back.push_back(A.cur->path); }
        delete A.cur;
    }
    A.cur = l;
    recentVisit(l->path);
    A.order.swap(l->order);
    std::vector<uint32_t>().swap(l->order);
    A.sortMs = l->sortMs;
    A.listFade = 0; // la cartella nuova entra in dissolvenza
    A.rowFade = A.linkFade = HoverFade();
    A.linkHover = -1;
    if (A.noticeKeep) A.noticeKeep = false; // l'avviso spiega proprio questo cambio di cartella
    else A.error.clear();
    A.filter.clear();
    A.view = A.order;
    A.sel = 0;
    A.scrollY = A.scrollTarget = 0;
    if (!A.pendingSelect.empty()) {
        for (int i = 0; i < (int)A.view.size(); i++)
            if (_wcsicmp(l->name(l->e[A.view[i]]), A.pendingSelect.c_str()) == 0) { A.sel = i; break; }
    }
    selReset();
    selOnly(A.sel);
    A.renaming = false;
    A.menu.open = false;
    A.zoom = viewZoomOf(l->path); // la vista è della cartella (viste.txt)
    layout();
    ensureVisible();
    startWatch(l->path);
    updateTitle();
    dirSizeQueueCurrent();
    linksQueueCurrent();
    A.dirty = true;
}

static std::wstring joinPath(const std::wstring& dir, const wchar_t* name) {
    if (dir.empty()) return name; // unità
    if (dir.back() == L'\\') return dir + name;
    return dir + L'\\' + name;
}

// Tipo dell'unità che contiene p secondo l'elenco drives (A.drives sul thread UI, o quello appena
// letto dal thread della barra laterale): \\server\… conta come rete, una lettera che non c'è come
// DRIVE_UNKNOWN. Solo testo, niente disco.
static UINT pathDriveType(const std::wstring& p, const std::vector<SideItem>& drives) {
    if (p.size() >= 2 && p[0] == L'\\' && p[1] == L'\\') return DRIVE_REMOTE;
    if (p.size() < 2 || p[1] != L':') return DRIVE_UNKNOWN;
    for (const SideItem& d : drives)
        if (d.drive && towupper(d.path[0]) == towupper(p[0])) return d.driveType;
    return DRIVE_UNKNOWN;
}
static bool slowDriveType(UINT t) { return t == DRIVE_REMOTE || t == DRIVE_CDROM; }

// Percorso su un'unità lenta: di rete (anche \\server\…) o ottica. Lì non si legge niente in
// background senza che l'utente lo chieda (dimensione delle cartelle), e le letture vanno dopo
// quelle locali ("Vai a", miniature, barra laterale). Decide dal tipo delle unità già noto
// (A.drives, riempito dal caricamento veloce della barra laterale): niente disco, ma solo sul thread UI.
static bool slowPath(const std::wstring& p) { return slowDriveType(pathDriveType(p, A.drives)); }

// File solo online (OneDrive e simili): leggerne il contenuto lo scaricherebbe. Tooltip, anteprima e
// miniature devono dare la stessa risposta. 0x00400000 = RECALL_ON_DATA_ACCESS, 0x00040000 =
// RECALL_ON_OPEN (mancano negli SDK vecchi).
static bool onlineOnly(DWORD attr) { return (attr & (FILE_ATTRIBUTE_OFFLINE | 0x00400000 | 0x00040000)) != 0; }

// \\server\condivisione: sopra c'è solo \\server, che non si può elencare (niente ↑)
static bool uncShareRoot(std::wstring p) {
    while (p.size() > 2 && p.back() == L'\\') p.pop_back();
    if (p.size() < 3 || p[0] != L'\\' || p[1] != L'\\') return false;
    size_t a = p.find(L'\\', 2);
    return a != std::wstring::npos && p.find(L'\\', a + 1) == std::wstring::npos;
}

// Cartella che contiene p e nome di p lì dentro: per salire di livello e per "Apri percorso file"
// (links.cpp). La radice di un'unità sta in Questo PC (parent ""). Solo testo, niente disco.
static bool parentOf(std::wstring p, std::wstring& parent, std::wstring& child) {
    while (p.size() > 3 && p.back() == L'\\') p.pop_back();
    if (p.size() <= 3 && p.size() >= 2 && p[1] == L':') { // radice di un'unità → Questo PC
        parent.clear();
        child = p.substr(0, 2) + L"\\";
        return true;
    }
    size_t k = p.find_last_of(L'\\');
    if (k == std::wstring::npos) return false;
    child = p.substr(k + 1);
    parent = p.substr(0, k);
    if (parent.size() == 2 && parent[1] == L':') parent += L'\\';
    return true;
}

static void goUp() {
    if (!A.cur || A.cur->path.empty() || uncShareRoot(A.cur->path)) return;
    std::wstring parent, child;
    if (parentOf(A.cur->path, parent, child)) navigate(parent, 1, child);
}

static void goBack() {
    if (A.back.empty()) return;
    std::wstring p = A.back.back(); A.back.pop_back();
    navigate(p, 2, L"");
}

static void goForward() {
    if (A.fwd.empty()) return;
    std::wstring p = A.fwd.back(); A.fwd.pop_back();
    navigate(p, 3, L"");
}

static void openSelected() {
    if (!A.cur || A.sel < 0 || A.sel >= (int)A.view.size()) return;
    const Entry& e = A.cur->e[A.view[A.sel]];
    std::wstring full = joinPath(A.cur->path, A.cur->name(e));
    if (e.attr & FILE_ATTRIBUTE_DIRECTORY) {
        navigate(full, 1, L"");
    } else if (g_inPane && !A.cur->zipLen && zipNameEnd(full, full.size())) { // uno zip nell'anteprima: in una nuova scheda, come una cartella
        navigate(full, 1, L""); // (dentro uno zip, uno zip è un file: si estrae, qui sotto)
    } else if (A.cur->zipLen) { // un file dentro uno zip: si estrae su un thread, poi si apre (zipout.cpp)
        zipJobStart(ZJ_OPEN, { A.view[A.sel] });
    } else if (zipNameEnd(full, full.size())) { // uno zip si apre come una cartella
        navigate(full, 1, L"");
    } else {
        HANDLE h = CreateThread(nullptr, 0, openThread, _wcsdup(full.c_str()), 0, nullptr);
        if (h) CloseHandle(h);
    }
}

// Dentro uno zip niente si modifica: le operazioni che scriverebbero lo dicono e non fanno niente
static bool zipReadOnly() {
    if (!A.cur || !A.cur->zipLen) return false;
    notice(L"L'archivio zip è in sola lettura", false);
    return true;
}
// Comandi della shell (Proprietà, Apri con, menu di Windows, terminale): dentro uno zip non ci sono
static bool zipNoShell() {
    if (!A.cur || !A.cur->zipLen) return false;
    notice(L"Non disponibile dentro un archivio zip", false);
    return true;
}

// y sulla riga del percorso (segmenti, "›", spazio vuoto): il clic e il doppio clic la riconoscono allo stesso modo
static bool onPathBar(int y) { return y >= A.pathY && y < A.pathY + A.topH; }

// Percorso dentro lo zip aperto (o un altro, a vista: un componente che finisce in ".zip"). Per le
// destinazioni di un trascinamento: in uno zip non si rilascia.
static bool pathInZip(const std::wstring& p) {
    if (A.cur && A.cur->zipLen && p.size() >= A.cur->zipLen && !_wcsnicmp(p.c_str(), A.cur->path.c_str(), A.cur->zipLen) &&
        (p.size() == A.cur->zipLen || p[A.cur->zipLen] == L'\\'))
        return true;
    return zipPathLike(p);
}

static std::wstring normalizePath(std::wstring p) {
    // spazi e virgolette (tipico di "Copia come percorso")
    while (!p.empty() && (p.back() == L' ' || p.back() == L'"')) p.pop_back();
    size_t a = p.find_first_not_of(L" \"");
    p = (a == std::wstring::npos) ? L"" : p.substr(a);
    if (p.find(L'%') != std::wstring::npos) { // %USERPROFILE%, ecc.
        wchar_t buf[2048];
        DWORD n = ExpandEnvironmentStringsW(p.c_str(), buf, 2048);
        if (n && n <= 2048) p = buf;
    }
    for (auto& c : p) if (c == L'/') c = L'\\';
    while (p.size() > 3 && p.back() == L'\\') p.pop_back();
    if (p.size() == 2 && p[1] == L':') p += L'\\';
    return p;
}

// Shift: estende dall'ancora · Ctrl: sposta solo il cursore · nessuno: seleziona solo la nuova voce
static void moveCursorTo(int vi, bool shift, bool ctrl) {
    if (A.view.empty()) return;
    vi = std::max(0, std::min((int)A.view.size() - 1, vi));
    if (shift) {
        int av = viewIndexOf(A.anchor);
        if (av < 0) { av = A.sel; A.anchor = (int)A.view[A.sel]; }
        if (!ctrl) selClear();
        selRange(av, vi);
    } else if (!ctrl) {
        selOnly(vi);
    }
    A.sel = vi;
    ensureVisible();
    A.dirty = true;
}

static void moveSel(int delta) {
    moveCursorTo(A.sel + delta, GetKeyState(VK_SHIFT) < 0, GetKeyState(VK_CONTROL) < 0);
}

// ---------------------------------------------------------------------------
// Formattazione
// ---------------------------------------------------------------------------

static int fmtDate(uint64_t ft64, wchar_t* buf) {
    FILETIME ft = { (DWORD)ft64, (DWORD)(ft64 >> 32) }, lt;
    SYSTEMTIME st;
    FileTimeToLocalFileTime(&ft, &lt);
    FileTimeToSystemTime(&lt, &st);
    return _snwprintf(buf, 31, L"%02u/%02u/%04u %02u:%02u", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
}

// Virgola decimale all'italiana: solo i punti tra due cifre (un nome come "GeForce 5070.Ti" non c'è,
// ma un "x.y" fuori dai numeri resta com'è)
static void decimalComma(wchar_t* s, int n) {
    for (int k = 1; k + 1 < n; k++)
        if (s[k] == L'.' && iswdigit(s[k - 1]) && iswdigit(s[k + 1])) s[k] = L',';
}

// Punto delle migliaia (212.388): tooltip, piede dell'anteprima, suggerimento del filtro
static std::wstring withThousands(uint64_t v) {
    std::wstring s = std::to_wstring(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, 1, L'.');
    return s;
}

static int fmtSize(uint64_t b, wchar_t* buf) {
    if (b < 1024) return _snwprintf(buf, 31, L"%llu B", (unsigned long long)b);
    const wchar_t* u[] = { L"KB", L"MB", L"GB", L"TB" };
    double v = (double)b / 1024.0;
    int i = 0;
    while (v >= 1024.0 && i < 3) { v /= 1024.0; i++; }
    int n = _snwprintf(buf, 31, v < 10 ? L"%.1f %s" : L"%.0f %s", v, u[i]);
    decimalComma(buf, n);
    return n;
}

// Tipo di unità (GetDriveTypeW) con le parole di Windows: lista di Questo PC, barra laterale e
// tooltip dicono la stessa cosa
static const wchar_t* driveKind(UINT t) {
    switch (t) {
    case DRIVE_FIXED:     return L"Disco locale";
    case DRIVE_REMOVABLE: return L"Unità rimovibile";
    case DRIVE_REMOTE:    return L"Unità di rete";
    case DRIVE_CDROM:     return L"Unità ottica";
    case DRIVE_RAMDISK:   return L"Disco RAM";
    case DRIVE_NETPLACE:  return L"Percorso di rete";
    default:              return L"Unità";
    }
}

// Barra di scorrimento: a destra della lista, in Elenco orizzontale in fondo alla lista. Il binario
// va da trackStart per trackLen lungo l'asse (x in Elenco, y altrimenti)
static float trackStart() { return listView() ? A.listX : A.listTop; }
static float trackLen() { return listView() ? itemsRight() - A.listX : A.listH; }
// Il punto (x, y) è sulla barra di scorrimento, con o senza maniglia
static bool scrollbarAt(int x, int y) {
    if (listView()) return y >= A.listTop + A.listH - A.sbW && y < A.listTop + A.listH && x >= A.listX && x < itemsRight();
    return x >= A.listR - A.sbW && x < A.listR && y >= A.listTop && y < A.listTop + A.listH;
}
// Maniglia: inizio (nella finestra) e lunghezza lungo l'asse; false se non c'è niente da scorrere
static bool thumbRect(float& t0, float& tl) {
    float cl = contentLen(), vl = viewLen(), track = trackLen();
    if (cl <= vl || vl <= 0 || track <= 0) return false;
    tl = std::max(px(ui::THUMB_MIN), track * vl / cl);
    t0 = trackStart() + (A.scrollY / maxScroll()) * (track - tl);
    return true;
}
