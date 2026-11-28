// [z]f — logica della vista: layout, selezione, filtro, navigazione, ricariche, formattazione.

// Spazio per le colonne dopo il nome: tutto tranne il minimo del nome
static float colRoom() { return A.listR - A.sbW - A.padX - A.listX - px(ui::COL_NAME_MIN); }

// Larghezza massima di Destinazione con la colonna dopo in nextX: mai più larga di Nome. Senza
// questo limite, con la finestra stretta Nome restava al suo minimo e lo spazio in più andava tutto
// a Destinazione; così le due colonne crescono e calano insieme.
static float linkMax(float nextX) { return floorf((nextX - A.listX) * ui::LINK_MAX); }

// Larghezza dell'anteprima che la lista lascia libera con le colonne al minimo. L'anteprima tiene
// la larghezza scelta e le colonne si adattano allo spazio che resta: se l'anteprima prendesse
// solo quello che avanza dalle colonne alla larghezza scelta, ogni colonna stretta col mouse la
// allargherebbe, e allargare una colonna sarebbe impossibile (ci si scontra con l'anteprima).
// Si contano le colonne di Dettagli anche nelle altre viste: cambiando vista l'anteprima non salta.
static float previewRoom() {
    int n = 0;
    for (int c : kColOrder) n += colWanted(c, false);
    return floorf((float)G.w - A.listX - px(ui::COL_NAME_MIN) - px(ui::COL_MIN) * n - A.padX - A.sbW);
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
// Campo del filtro nella barra in fondo della lista di App (anche l'anteprima, nel suo PaneScope) e
// la sua ×, che c'è solo mentre si filtra: le misure le decide il disegno (drawBarFilter)
static bool fltFieldAt(int x, int y) { return A.fltW > 0 && inBar(y) && x >= A.fltX && x < A.fltX + A.fltW; }
static bool fltClearAt(int x, int y) { return A.fltClearW > 0 && fltFieldAt(x, y) && x >= A.fltX + A.fltW - A.fltClearW; }
// Barra dei tempi e di stato della lista principale
static bool inListFoot(int x, int y) {
    return x >= A.sideW && x < A.listR && y >= A.footY - A.footH && y < A.footY;
}

static float lineLen();
static float viewLen();
static bool cursorInView(float s, bool whole);
static void ensureVisible();
static void groupsPlace();

// La geometria di prima, presa prima di cambiare i bordi: il cursore che si vedeva resta in vista
// anche con quella nuova (l'anteprima che si apre stringe Elenco e la colonna del cursore uscirebbe
// a destra). Se l'utente ha scorso lontano dal cursore la lista non si muove.
struct LayoutKeep { bool cursor; int perLine; float line, view; };
static LayoutKeep layoutKeep() { return { cursorInView(A.scrollY, false), A.perLine, lineLen(), viewLen() }; }

// Geometria delle voci dentro listX…listR, da listTop per listH, nella vista di A.zoom: colonne e
// celle. La lista principale (layout) e l'anteprima di una cartella (preview.cpp, nel suo
// PaneScope), che in Dettagli non ha Destinazione e, a destra, solo Dimensione (noDate).
static void layoutItems(bool noDate, const LayoutKeep& k) {
    // Colonne, solo in Dettagli: Nome prende quello che avanza, le altre stanno a destra nell'ordine
    // di kColOrder. Se la finestra è stretta cedono nell'ordine di kColYield, ognuna fino al minimo
    // (senza salvare); Destinazione inoltre non supera Nome (linkMax).
    float right = A.listR - A.sbW - A.padX, room = colRoom(), mn = px(ui::COL_MIN), total = 0;
    for (int c = 0; c < COL_COUNT; c++) {
        A.colWd[c] = A.zoom == ZOOM_DETAILS && c != COL_NAME && colWanted(c, noDate) ? px(A.colW[c]) : 0;
        total += A.colWd[c];
    }
    for (int c : kColYield)
        if (A.colWd[c] > 0 && total > room) {
            float w = std::max(mn, A.colWd[c] - (total - room));
            total -= A.colWd[c] - w;
            A.colWd[c] = w;
        }
    // Da destra: ogni colonna comincia dove finisce quella prima (una nascosta, larga 0, comincia
    // dove comincia la successiva)
    float x = right;
    for (int i = COL_COUNT - 2; i >= 0; i--) {
        x -= A.colWd[kColOrder[i]];
        A.colX[kColOrder[i]] = x;
    }
    if (A.colWd[COL_LINK] > 0) { // Destinazione è la prima da sinistra: si stringe verso destra
        float next = A.colX[COL_LINK] + A.colWd[COL_LINK];
        A.colWd[COL_LINK] = std::max(mn, std::min(A.colWd[COL_LINK], linkMax(next)));
        A.colX[COL_LINK] = next - A.colWd[COL_LINK];
    }
    A.nameEnd = A.colX[kColOrder[0]] - A.padX;
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
        A.perLine = std::max(1, (int)((A.listH - A.sbW) / A.rowH) - (grouped() ? 1 : 0)); // con i gruppi la prima riga è delle intestazioni
        A.cellW = std::max(1.0f, std::min(px(ui::LIST_COL_W), lw - m * 2));
        A.cellX = A.listX + m;
    } else {
        A.thumbPx = 0;
        A.perLine = 1;
        A.cellW = lw;
        A.cellX = A.listX;
        A.cellH = A.rowH;
    }
    groupsPlace();
    bool changed = A.perLine != k.perLine || lineLen() != k.line || viewLen() != k.view;
    if (k.cursor && changed && viewLen() > 0 && !cursorInView(A.scrollTarget, true)) ensureVisible();
}

// Filtro all'inizio della barra in fondo, allineato alla lista di App (anche l'anteprima, nel suo
// PaneScope). Questa è la larghezza voluta: quella mostrata (fltW) la decide il disegno
// (drawBarFilter), che con la lista stretta la cede a contatori e vista
static void fltLayout() {
    A.fltX = A.listX + px(ui::GAP_S);
    A.fltWant = floorf(std::min(px(ui::FILTER_W), (A.listR - A.listX) * ui::FILTER_MAX));
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
    fltLayout();
    A.fltW = A.fltWant;
    layoutItems(false, keep);
}

// Bordo b tra due colonne: quello a sinistra della colonna b (COL_*), a metà dello spazio tra il
// testo della colonna prima e il suo. Ogni bordo comanda la colonna alla sua destra: colChosen è
// il valore scelto (impostazioni.txt), colShown la larghezza mostrata adesso.
static float colBorderX(int b) { return A.colX[b] + (colAfterRight(b) ? 1 : -1) * floorf(A.padX * 0.5f); }
// Oltre ai bordi delle colonne si trascinano con lo stesso gesto quelli dei pannelli (barra
// laterale, anteprima): cursore ↔, Esc torna a prima, si salva al rilascio (listmouse.cpp).
enum { EDGE_SIDE = COL_COUNT, EDGE_PREVIEW };
static float& colChosen(int b) {
    return b < COL_COUNT ? A.colW[b] : b == EDGE_SIDE ? A.sideWidth : A.prevBottom ? A.prevHeight : A.prevWidth;
}
static float colShown(int b) {
    return b < COL_COUNT ? A.colWd[b] : b == EDGE_SIDE ? A.sideW : A.prevBottom ? A.prevH : A.prevW;
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
    for (int i = COL_COUNT - 2; i >= 0; i--) {
        int c = kColOrder[i];
        if (A.colWd[c] > 0 && fabsf(x - colBorderX(c)) <= px(ui::COL_GRIP)) return c;
    }
    return -1;
}
// Colonna dell'intestazione sotto x; in Elenco e Icone l'etichetta (sortX, dal disegno), -1 fuori
static int colAt(int x) {
    if (A.zoom != ZOOM_DETAILS) {
        for (int i = 0; i < A.sortN; i++)
            if (x >= A.sortX[i] - px(ui::GAP_S) && x < A.sortX[i + 1] - px(ui::GAP_XL) + px(ui::GAP_S)) return A.sortC[i];
        return -1;
    }
    for (int i = COL_COUNT - 2; i >= 0; i--) {
        int c = kColOrder[i];
        if (A.colWd[c] > 0 && x >= colBorderX(c)) return c;
    }
    return COL_NAME;
}

struct Box { float x, y, w, h; };
// Imbuto del filtro della colonna c nell'intestazione di Dettagli (colHeadParts, columns.cpp): il
// rettangolo cliccabile, con GAP_XS intorno, alto l'intestazione meno GAP_XS sopra e sotto
static Box colGlyphBox(int c) {
    HeadParts h = colHeadParts(c, colTitle(c, colArrow() == c), true);
    float m = px(ui::GAP_XS);
    return { h.glyphX - m, A.sideTop + m, (float)F.lineH + m * 2, A.headH - 1 - m * 2 };
}
// Colonna dell'imbuto sotto il punto, -1 se nessuna (solo dove si vede)
static int colGlyphAt(int x, int y) {
    if (A.zoom != ZOOM_DETAILS || y < A.sideTop || y >= A.listTop) return -1;
    for (int c : kColOrder) {
        if (!colGlyphShown(c)) continue;
        Box b = colGlyphBox(c);
        if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) return c;
    }
    return -1;
}
// Colonna con filtro la cui intestazione è sotto il punto (l'imbuto compare lì), -1 se nessuna
static int colFilterHeadAt(int x, int y) {
    if (A.zoom != ZOOM_DETAILS || A.colDrag >= 0 || y < A.sideTop || y >= A.listTop || x < A.listX || x >= A.listR - A.sbW) return -1;
    int c = colAt(x);
    return colFilterable(c) && A.colWd[c] > 0 ? c : -1;
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

// --- gruppi (columns.cpp, groupsBuild): ogni gruppo è un'intestazione e le sue linee di voci ---
// In Dettagli e Icone l'intestazione è una riga lungo l'asse, sopra le voci; in Elenco sta nella
// prima riga in alto (perLine ne tiene conto) e lungo l'asse non prende spazio. Tra i gruppi GROUP_GAP.
static float groupHeadLen() { return listView() ? 0 : A.rowH; }
static int groupLines(const Group& g) { return (g.n + A.perLine - 1) / A.perLine; }
// Lunghezza di un gruppo lungo l'asse: in Elenco almeno una colonna, che porta l'intestazione
static float groupLen(const Group& g) {
    int lines = groupLines(g);
    if (listView()) lines = std::max(lines, 1);
    return groupHeadLen() + lines * lineLen();
}
// Dove comincia ogni gruppo (Group::at): dopo ogni cambio di geometria (layoutItems) o di gruppi
// (applyFilter)
static void groupsPlace() {
    float at = 0;
    for (size_t i = 0; i < A.groups.size(); i++) {
        if (i) at += px(ui::GROUP_GAP);
        A.groups[i].at = at;
        at += groupLen(A.groups[i]);
    }
}
// Il gruppo della voce vi (che è nella vista, quindi in un gruppo aperto): l'ultimo che comincia
// prima; i gruppi chiusi hanno lo stesso first del gruppo dopo e stanno prima di lui
static int groupOf(int vi) {
    auto it = std::upper_bound(A.groups.begin(), A.groups.end(), vi, [](int v, const Group& g) { return v < g.first; });
    return std::max(0, (int)(it - A.groups.begin()) - 1);
}
// L'ultimo gruppo che comincia prima della posizione a lungo l'asse (contenuto), o a a: nello spazio
// tra due gruppi quello prima; 0 se a è prima del primo. Solo con A.groups non vuota
static int groupAtLen(float a) {
    auto it = std::upper_bound(A.groups.begin(), A.groups.end(), a, [](float v, const Group& g) { return v < g.at; });
    return std::max(0, (int)(it - A.groups.begin()) - 1);
}
static float contentLen() {
    if (!A.groups.empty()) return A.groups.back().at + groupLen(A.groups.back());
    return (float)((A.view.size() + A.perLine - 1) / A.perLine) * lineLen();
}
// Inizio lungo l'asse della linea della voce vi (contenuto)
static float itemAlong(int vi) {
    if (A.groups.empty()) return (float)(vi / A.perLine) * lineLen();
    const Group& g = A.groups[groupOf(vi)];
    return g.at + groupHeadLen() + (float)((vi - g.first) / A.perLine) * lineLen();
}
// La voce vi è nella prima linea del suo gruppo: scorrendo fin lì si mostra anche l'intestazione
static float itemAlongWithHead(int vi) {
    if (A.groups.empty()) return itemAlong(vi);
    const Group& g = A.groups[groupOf(vi)];
    return vi - g.first < A.perLine ? g.at : itemAlong(vi);
}
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
// Rettangolo della voce vi nel contenuto, senza scorrimento: x come nella finestra, y da 0 =
// inizio della lista. Nella finestra: (x - ox, listTop + y - oy), con scrollOffset.
static Box itemRect(int vi) {
    int k = vi % A.perLine;
    float along = itemAlong(vi), cross = 0;
    if (!A.groups.empty()) { // k dall'inizio del gruppo; in Elenco sotto la riga delle intestazioni
        k = (vi - A.groups[groupOf(vi)].first) % A.perLine;
        if (listView()) cross = A.rowH;
    }
    if (listView()) return { A.cellX + along, cross + k * A.cellH, A.cellW, A.cellH };
    return { A.cellX + k * A.cellW, along, A.cellW, A.cellH };
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
    float along = listView() ? cx : cy, cross = listView() ? cy : cx;
    int first = 0, n = (int)A.view.size();
    if (!A.groups.empty()) { // il gruppo lungo l'asse; fuori dalle sue voci (intestazione, spazio tra i gruppi) nessuna
        const Group& g = A.groups[groupAtLen(along)];
        along -= g.at + groupHeadLen();
        if (listView()) cross -= A.rowH;
        if (along < 0 || cross < 0) return -1;
        first = g.first;
        n = g.n;
    }
    int line = (int)floorf(along / lineLen()), k = listView() ? (int)floorf(cross / A.cellH) : gridView() ? (int)floorf(cross / A.cellW) : 0;
    if (line < 0 || k < 0 || k >= A.perLine) return -1;
    int i = line * A.perLine + k;
    return i < n ? first + i : -1;
}
// Intestazione di gruppo sotto il punto (coordinate della finestra): l'indice in A.groups, -1 se nessuna
static int groupHeadAt(int x, int y) {
    if (A.groups.empty() || y < A.listTop || y >= A.listTop + A.listH || x < A.listX || x >= itemsRight()) return -1;
    float ox, oy;
    scrollOffset(ox, oy);
    float cx = x + ox - A.cellX, cy = y - A.listTop + oy;
    if (listView()) { // la riga in alto, larga quanto le colonne del gruppo
        // cy, come itemAt per le voci sotto: intestazioni e voci si prendono con le stesse coordinate
        // (lo spostamento dell'entrata, LIST_ENTER_DY per un istante, non lo conta nessuna delle due)
        if (cy < 0 || cy >= A.rowH) return -1;
        int g = groupAtLen(cx);
        return cx >= A.groups[g].at && cx < A.groups[g].at + groupLen(A.groups[g]) ? g : -1;
    }
    int g = groupAtLen(cy);
    return cy >= A.groups[g].at && cy < A.groups[g].at + groupHeadLen() ? g : -1;
}
// Le voci che stanno, anche in parte, tra a0 e a1 lungo l'asse (contenuto): da first a last escluso
static void itemsInSpan(float a0, float a1, int& first, int& last) {
    float len = lineLen();
    int n = (int)A.view.size();
    if (A.groups.empty() || len <= 0) {
        first = len > 0 ? std::min(n, std::max(0, (int)(a0 / len)) * A.perLine) : 0;
        last = len > 0 ? std::min(n, ((int)(a1 / len) + 1) * A.perLine) : 0;
        return;
    }
    // Le linee del gruppo g tra a0 e a1, contate dall'inizio delle sue voci
    auto line = [&](const Group& g, float a) { return (int)floorf((a - g.at - groupHeadLen()) / len); };
    const Group& g0 = A.groups[groupAtLen(a0)];
    const Group& g1 = A.groups[groupAtLen(a1)];
    first = g0.first + std::min(g0.n, std::max(0, line(g0, a0)) * A.perLine);
    last = g1.first + std::min(g1.n, std::max(0, line(g1, a1) + 1) * A.perLine);
    last = std::max(first, last);
}
// Prima voce visibile (per tenerla in vista quando cambia la geometria)
static int firstVisibleItem() {
    int first, last;
    itemsInSpan(A.scrollY, A.scrollY, first, last);
    return std::min(first, std::max(0, (int)A.view.size() - 1));
}
static void scrollToItem(int vi) {
    A.scrollY = A.scrollTarget = vi >= 0 && vi < (int)A.view.size() ? itemAlongWithHead(vi) : 0;
}

// Il cursore, con lo scorrimento s, si vede tutto (whole) o almeno in parte
static bool cursorInView(float s, bool whole) {
    if (A.sel < 0 || A.sel >= (int)A.view.size() || A.perLine < 1) return false;
    float len = lineLen(), view = viewLen(), top = itemAlong(A.sel);
    if (len <= 0 || view <= 0) return false;
    return whole ? top >= s && top + len <= s + view : top < s + view && top + len > s;
}

static void ensureVisible() {
    if (A.view.empty()) return;
    float top = itemAlong(A.sel), len = lineLen(), view = viewLen();
    float s = A.scrollTarget;
    // La prima voce della vista porta fino all'inizio e l'ultima fino alla fine del contenuto: con
    // Home e Fine si vedono anche le intestazioni dei gruppi chiusi in cima e in fondo, che
    // nessuna voce porterebbe in vista. Senza gruppi sono le stesse posizioni della voce
    bool first = A.sel == 0, last = A.sel == (int)A.view.size() - 1;
    float head = first ? 0 : itemAlongWithHead(A.sel), end = last ? contentLen() : top + len;
    if (top < s || (first && s > 0)) s = std::min(top, std::max(head, top + len - view)); // salendo, anche l'intestazione del gruppo se ci sta
    else if (end > s + view) s = std::max(top + len - view, std::min(top, end - view)); // la voce resta in vista
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

// Le voci dell'ordine corrente che passano il filtro per nome (sottostringa, maiuscole/minuscole
// indifferenti) e quelli delle colonne (columns.cpp), tranne quello della colonna skip (-1: tutti):
// la vista, e le voci da contare nel menu del filtro di una colonna. ctx: già preparato per tutte le
// colonne filtrate (applyFilter), o nullptr
static void filterInto(std::vector<uint32_t>& out, int skip, const ColCtx* ctx = nullptr) {
    out.clear();
    ColMatch m;
    bool cols = A.cur && colMatchInit(m, A.colFilter, A.cur, skip, ctx);
    if (A.filter.empty() && !cols) { out = A.order; return; }
    std::wstring needle = A.filter;
    if (!needle.empty()) CharLowerBuffW(&needle[0], (DWORD)needle.size());
    out.reserve(A.order.size());
    for (uint32_t i : A.order) {
        const Entry& e = A.cur->e[i];
        if (!needle.empty() && !wcsstr(A.cur->lowerName(e), needle.c_str())) continue;
        if (cols && !colMatches(m, e)) continue;
        out.push_back(i);
    }
}

// Filtro o gruppi per una data: alla prossima mezzanotte locale la vista si rifà (timer 19,
// onDayChange), perché colCtxInit calcola "Oggi", "Ieri"… solo quando si filtra. Senza date nella
// vista il timer è spento: a riposo nessun lavoro. Della lista di App, non dell'anteprima di una
// cartella (PaneScope: lo stato scambiato non è quello della lista)
static uint64_t g_dayEnd = 0; // la mezzanotte (FILETIME UTC) a cui scatta il timer 19; 0: spento
static void dayTimerUpdate() {
    if (g_inPane || !A.hwnd) return;
    if (!viewUsesDates()) {
        if (g_dayEnd) { KillTimer(A.hwnd, 19); g_dayEnd = 0; }
        return;
    }
    if (g_dayEnd) return; // già armato per la stessa mezzanotte
    g_dayEnd = nextLocalMidnight();
    FILETIME f;
    GetSystemTimeAsFileTime(&f);
    uint64_t now = fileTime64(f);
    SetTimer(A.hwnd, 19, g_dayEnd > now ? (UINT)((g_dayEnd - now) / 10000 + 1) : 0, nullptr); // al più 25 ore
}

// Applica i filtri all'ordine corrente
static void applyFilter() {
    double t0 = nowMs();
    ColCtx ctx; // uno per filtro e gruppi: i totali delle cartelle si leggono una volta sola
    if (A.cur) colCtxInit(ctx, A.cur, viewCtxCols());
    filterInto(A.view, -1, &ctx);
    double t1 = nowMs();
    A.filterMs = t1 - t0;
    groupsBuild(ctx); // i gruppi dopo i filtri: un gruppo c'è solo se ha voci che passano
    groupsPlace();
    if (!A.groups.empty()) {
        A.groupMs = nowMs() - t1;
        if (A.groupMs > ui::LOG_MSG) hitchLog("gruppi lenti", (unsigned)A.view.size(), A.groupMs);
    }
    A.counts.dirty = true; // un filtro diverso può dare lo stesso numero di voci, ma non le stesse
    dayTimerUpdate();
    dropRetarget(); // la vista è cambiata: un trascinamento fermo sopra non punta più all'indice di prima
}
static bool filtering() { return !A.filter.empty() || A.colFilter.any(); }

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

// I filtri o i gruppi danno un risultato diverso senza che la lista debba seguire il cursore (un totale
// di cartella arrivato, un gruppo chiuso): come rebuildView, ma la lista non scorre verso il cursore.
// Il cursore uscito dalla vista resta allo stesso indice, o con closed (indice in A.groups, che
// chiudere un gruppo non cambia) va alla prima voce dei gruppi dopo quel gruppo: all'ultima voce
// prima, se dopo non ce ne sono di aperti. Se la selezione esce tutta dalla vista, si seleziona la
// voce al cursore, come rebuildView e listReload: non si resta senza nessuna voce selezionata
static void refilterQuiet(int closed = -1) {
    if (!A.cur) return;
    bool hadSel = selCount(nullptr) > 0;
    uint32_t selEntry = A.sel >= 0 && A.sel < (int)A.view.size() ? A.view[A.sel] : UINT32_MAX;
    applyFilter();
    int sel = -1;
    for (int i = 0; selEntry != UINT32_MAX && i < (int)A.view.size(); i++)
        if (A.view[i] == selEntry) { sel = i; break; }
    if (sel < 0) sel = closed >= 0 && closed < (int)A.groups.size() ? A.groups[closed].first : A.sel;
    A.sel = std::max(0, std::min(sel, (int)A.view.size() - 1));
    selKeepVisible();
    if (hadSel && selCount(nullptr) == 0) selOnly(A.sel);
    clampScroll();
    A.dirty = true;
}

// Timer 19, ora di sistema cambiata (WM_TIMECHANGE: timeChanged) o ritorno dalla sospensione: se è
// passata la mezzanotte, o l'ora è cambiata, la vista si rifà (e applyFilter riarma per la prossima);
// se il timer è scattato prima (o la sospensione l'ha spostato), si riarma per il resto
static void onDayChange(bool timeChanged) {
    if (!g_dayEnd || g_inPane) return; // nessuna data nella vista
    KillTimer(A.hwnd, 19);
    uint64_t end = g_dayEnd;
    g_dayEnd = 0;
    FILETIME f;
    GetSystemTimeAsFileTime(&f);
    if (!timeChanged && fileTime64(f) < end) { dayTimerUpdate(); return; }
    refilterQuiet();
}

// Clic sull'intestazione del gruppo g: lo chiude o lo apre (A.groupClosed, della scheda). La lista
// non scorre verso il cursore: l'intestazione resta sotto il mouse (i gruppi prima non cambiano);
// le voci selezionate di un gruppo chiuso escono dalla selezione, come col filtro
static void groupToggle(int g) {
    if (!A.cur || g < 0 || g >= (int)A.groups.size()) return;
    const ColValue v = A.groups[g].v;
    auto it = std::find_if(A.groupClosed.begin(), A.groupClosed.end(), [&](const ColValue& c) { return colValueSame(c, v); });
    if (it != A.groupClosed.end()) A.groupClosed.erase(it);
    else A.groupClosed.push_back(v);
    refilterQuiet(g);
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


// La stessa cartella riletta (l) nella lista di App: la principale (applyRefresh) o, nel suo
// PaneScope, l'anteprima (pvPaneSet). Restano filtro e scorrimento; selezione, cursore, ancora e voce
// in rinomina si ritrovano per nome (minuscolo: buildLower). selectAfter: dopo una rinomina o una
// nuova cartella, la voce su cui mettere il cursore (renameAfter: da rinominare); se la lettura non
// la contiene ancora, resta per la prossima
static void listReload(Listing* l, std::wstring& selectAfter, bool& renameAfter) {
    std::unordered_map<std::wstring, bool> selNames;
    std::wstring curName, anchorName, renameName;
    int oldSel = A.sel;
    bool hadSel = false;
    if (const Listing* o = A.cur) {
        if (A.sel >= 0 && A.sel < (int)A.view.size()) curName = o->lowerName(o->e[A.view[A.sel]]);
        // La voce in rinomina per nome suo: il cursore può essersi spostato durante la rinomina
        if (A.renaming && A.renameEntry >= 0 && A.renameEntry < (int)o->e.size()) renameName = o->lowerName(o->e[A.renameEntry]);
        if (A.anchor >= 0 && A.anchor < (int)o->e.size()) anchorName = o->lowerName(o->e[A.anchor]);
        for (size_t i = 0; i < A.selFlags.size() && i < o->e.size(); i++)
            if (A.selFlags[i]) { selNames[o->lowerName(o->e[i])] = true; hadSel = true; }
    }
    delete A.cur;
    A.cur = l;
    A.order.swap(l->order);
    std::vector<uint32_t>().swap(l->order);
    applyFilter();

    A.selFlags.assign(l->e.size(), 0);
    A.anchor = -1;
    int renameEntry = -1;
    if (hadSel || !anchorName.empty() || !renameName.empty())
        for (size_t i = 0; i < l->e.size(); i++) {
            const wchar_t* n = l->lowerName(l->e[i]);
            if (hadSel && selNames.count(n)) A.selFlags[i] = 1;
            if (A.anchor < 0 && anchorName == n) A.anchor = (int)i;
            if (renameEntry < 0 && renameName == n) renameEntry = (int)i;
        }
    selKeepVisible();
    A.sel = -1;
    if (!curName.empty())
        for (int i = 0; i < (int)A.view.size(); i++)
            if (curName == l->lowerName(l->e[A.view[i]])) { A.sel = i; break; }
    if (A.sel < 0) A.sel = std::max(0, std::min(oldSel, (int)A.view.size() - 1)); // eliminata: resta lì
    // Sparita la selezione (eliminata da fuori, Canc): si seleziona la voce che ne prende il posto.
    // Senza selezione prima (Ctrl+clic l'ha tolta) resta senza: la ricarica non sceglie al posto dell'utente
    if (hadSel && !A.view.empty() && selCount(nullptr) == 0) selOnly(A.sel);
    clampScroll();

    // Prima la rinomina già aperta, poi quella della nuova cartella: beginRename imposta già la voce
    // giusta nella lista nuova, e questo controllo la rimetterebbe su un nome della lista vecchia
    if (A.renaming) { // la voce in rinomina esiste ancora? Il nome che si sta scrivendo resta
        A.renameEntry = renameEntry;
        if (A.renameEntry < 0) A.renaming = false;
    }
    if (!selectAfter.empty()) {
        std::wstring want = selectAfter;
        CharLowerBuffW(&want[0], (DWORD)want.size());
        bool found = false;
        for (int i = 0; i < (int)A.view.size(); i++)
            if (want == l->lowerName(l->e[A.view[i]])) {
                A.sel = i;
                selOnly(i);
                ensureVisible();
                found = true;
                if (renameAfter) { renameAfter = false; beginRename(); }
                break;
            }
        // C'è ma il filtro la nasconde: non la si aspetta più. Altrimenti, tolto il filtro, una
        // rilettura molto più tardi la selezionerebbe (e aprirebbe la rinomina) senza motivo
        if (!found)
            for (size_t i = 0; i < l->e.size() && !found; i++) found = want == l->lowerName(l->e[i]);
        if (found) { selectAfter.clear(); renameAfter = false; }
    }
}

static void applyRefresh(Listing* l) {
    A.sortMs = l->sortMs;
    listReload(l, A.selectAfterRefresh, A.renameAfterRefresh);
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
    A.colFilter.clear(); // i filtri sono della cartella: cambiandola si tolgono tutti
    A.zoom = viewZoomOf(l->path, &A.groupCol); // vista e gruppi sono della cartella (viste.txt)
    A.groupClosed.clear(); // i gruppi chiusi sono della scheda finché resta nella cartella
    applyFilter();
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

// La voce nella linea accanto a quella di vi (dir -1 prima, 1 dopo), nella stessa posizione o
// nell'ultima della linea se è più corta. Con i gruppi si passa al gruppo aperto accanto, saltando
// le intestazioni (DESIGN §7). -1 se vi è nella prima o nell'ultima linea
static int lineStep(int vi, int dir) {
    int n = (int)A.view.size(), pl = A.perLine;
    if (vi < 0 || vi >= n) return -1;
    int gi = -1, first = 0, cnt = n;
    if (!A.groups.empty()) {
        gi = groupOf(vi);
        first = A.groups[gi].first;
        cnt = A.groups[gi].n;
    }
    int k = (vi - first) % pl, line = (vi - first) / pl + dir;
    if (line < 0 || line > (cnt - 1) / pl) { // fuori dal gruppo: il primo aperto in quella direzione
        if (gi < 0) return -1;
        do gi += dir; while (gi >= 0 && gi < (int)A.groups.size() && A.groups[gi].n == 0);
        if (gi < 0 || gi >= (int)A.groups.size()) return -1;
        first = A.groups[gi].first;
        cnt = A.groups[gi].n;
        line = dir > 0 ? 0 : (cnt - 1) / pl;
    }
    return first + std::min(line * pl + k, cnt - 1);
}

// PagSu, PagGiù (dir -1, 1): le linee che stanno in dist lungo l'asse, contando intestazioni e spazi
// tra i gruppi (almeno una); dalla prima o dall'ultima linea, la prima o l'ultima voce
static int pageStep(int vi, int dir, float dist) {
    if (A.view.empty()) return 0;
    float a0 = itemAlong(vi);
    int t = vi;
    for (;;) {
        int u = lineStep(t, dir);
        if (u < 0) return dir > 0 ? (int)A.view.size() - 1 : 0;
        if (t != vi && fabsf(itemAlong(u) - a0) > dist + 0.5f) return t; // mezzo pixel: gli arrotondamenti dei prodotti
        t = u;
    }
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

// Punto delle migliaia (212.388): ogni numero di voci, righe o byte che si mostra (contatori delle
// barre, selezionati, tooltip, gruppi, valori del filtro, piede dell'anteprima, suggerimento del filtro)
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
