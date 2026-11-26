// [z]f — lista col mouse: selezione col rettangolo e colonne ridimensionabili.
//
// Rettangolo: il trascinamento dei file parte da icona e nome di una voce già
// selezionata (drag.cpp); da tutto il resto della lista (nome di una voce non selezionata, parte
// vuota della colonna Nome, Destinazione, Data, Dimensione, margine, spazio sotto le righe) parte il
// rettangolo, oltre la soglia di trascinamento di Windows. Il clic vale subito, come sempre;
// se poi parte il rettangolo si riparte dalla selezione che c'era prima del clic.
// In Dettagli prende le righe che tocca in altezza, in Elenco e Icone le voci che tocca; senza
// tasti sostituisce la selezione, Ctrl alterna, Shift aggiunge. Il punto di partenza è in
// coordinate del contenuto: resta sulla sua voce anche quando la lista scorre (in Elenco in
// orizzontale). Esc rimette la selezione di prima.
//
// Colonne: i bordi si afferrano nell'intestazione (colBorderAt, in app.cpp); ogni bordo comanda la
// colonna alla sua destra. Doppio clic: adatta quella colonna al testo più largo. Le larghezze si
// salvano in impostazioni.txt al rilascio, mai durante il trascinamento. Lo stesso codice muove i
// bordi della barra laterale e dell'anteprima (panelEdgeAt); per loro il doppio clic torna alla
// larghezza predefinita.

enum { MQ_REPLACE, MQ_TOGGLE, MQ_ADD };

struct Marquee {
    bool armed = false;         // tasto premuto nella lista, fuori da icona e nome
    bool active = false;        // oltre la soglia: il rettangolo c'è
    POINT down = {};            // dove è stato premuto (client)
    float x0 = 0, y0 = 0;       // inizio in coordinate del contenuto (itemRect: con lo scorrimento)
    int x = 0, y = 0;           // ultima posizione del mouse (client, anche fuori dalla finestra)
    int mode = MQ_REPLACE;
    const Listing* cur = nullptr; // cartella su cui è partito: se cambia (ricarica, scheda) si smette
    std::vector<uint8_t> base;  // selezione prima del clic (per indice di voce)
    int sel = 0, anchor = -1;   // cursore e ancora prima del clic
} MQ;

static bool boxHit(const Box& b, float x, float y) { return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h; }

// Punto della finestra in coordinate del contenuto (quelle di itemRect)
static void toContent(float x, float y, float& cx, float& cy) {
    float ox, oy;
    scrollOffset(ox, oy);
    cx = x + ox;
    cy = y - A.listTop + oy;
}

// Il tasto è stato premuto su icona e nome di una voce (Dettagli, Elenco), o su miniatura e nome
// (Icone): se è selezionata, lì parte il trascinamento dei file
static bool itemGrabHit(int r, int x, int y) {
    float cx, cy;
    toContent((float)x, (float)y, cx, cy);
    if (gridView()) {
        CellParts p = gridParts(r);
        return boxHit(p.thumb, cx, cy) || boxHit(p.name, cx, cy);
    }
    const Entry& e = A.cur->e[A.view[r]];
    float tw = (float)textWidth(A.cur->name(e), (int)e.nameLen);
    if (listView()) {
        RowParts p = listParts(r);
        return cx >= p.iconX && cx < std::min(p.nameX + tw, p.nameEnd) + px(ui::GAP_S);
    }
    float nameEnd = std::min(A.nameX + tw, A.nameEnd);
    return x >= A.iconX && x < nameEnd + px(ui::GAP_S);
}

// Elenco e Icone: voce nella cella che contiene il punto (coordinate del contenuto), riportata
// tra le voci che ci sono
static int cellAt(float cx, float cy) {
    int n = (int)A.view.size(), pl = A.perLine;
    int across = (int)floorf((cx - A.cellX) / A.cellW), along = (int)floorf(cy / A.cellH);
    int line = listView() ? across : along, k = listView() ? along : across;
    line = std::max(0, line);
    k = std::max(0, std::min(pl - 1, k));
    return std::max(0, std::min(n - 1, line * pl + k));
}

static void mqEnd() {
    bool was = MQ.armed || MQ.active;
    MQ.armed = MQ.active = false;
    if (!was) return;
    KillTimer(A.hwnd, 10);
    std::vector<uint8_t>().swap(MQ.base);
    if (GetCapture() == A.hwnd) ReleaseCapture();
    A.dirty = true;
}

// Tasto sinistro premuto nella lista fuori da icona e nome (r: riga, -1 = spazio sotto le righe)
static void mqPress(int x, int y, int r, bool shift, bool ctrl) {
    MQ.base = A.selFlags;
    MQ.sel = A.sel;
    MQ.anchor = A.anchor;
    MQ.cur = A.cur;
    MQ.armed = true;
    MQ.down = { x, y };
    MQ.x = x; MQ.y = y;
    toContent((float)x, (float)y, MQ.x0, MQ.y0);
    MQ.mode = shift ? MQ_ADD : ctrl ? MQ_TOGGLE : MQ_REPLACE;
    if (r >= 0) {
        if (ctrl && !shift) { // Ctrl+clic: alterna
            uint32_t ei = A.view[r];
            A.selFlags[ei] = !A.selFlags[ei];
            A.anchor = (int)ei;
            A.sel = r;
        } else {
            moveCursorTo(r, shift, ctrl);
        }
    } else if (!shift && !ctrl) {
        selClear(); // clic sullo spazio vuoto: toglie la selezione, il cursore resta (come Explorer)
    }
    SetCapture(A.hwnd);
    A.dirty = true;
}

// Ricalcola la selezione dal rettangolo: base (prima del clic) combinata con le righe toccate.
// O(voci) con memset/memcpy più O(righe toccate): misurato in F12.
static void mqUpdate() {
    if (!MQ.active) return;
    if (A.cur != MQ.cur || A.selFlags.size() != MQ.base.size()) { mqEnd(); return; } // la cartella è cambiata
    double t0 = nowMs();
    float xc, yc; // il mouse, dentro la lista, in coordinate del contenuto
    toContent(std::min(std::max((float)MQ.x, A.listX), A.listR - A.sbW),
              std::min(std::max((float)MQ.y, A.listTop), A.listTop + A.listH), xc, yc);
    float top = std::min(MQ.y0, yc), bot = std::max(MQ.y0, yc);
    float left = std::min(MQ.x0, xc), right = std::max(MQ.x0, xc);
    int n = (int)A.view.size();
    if (MQ.mode == MQ_REPLACE) selClear();
    else memcpy(A.selFlags.data(), MQ.base.data(), MQ.base.size());
    auto take = [&](int i) {
        uint32_t ei = A.view[i];
        A.selFlags[ei] = MQ.mode == MQ_TOGGLE ? !MQ.base[ei] : 1;
    };
    bool any = false;
    if (gridView()) { // le voci di cui tocca miniatura o nome
        int ra = std::max(0, (int)(top / A.cellH)), rb = std::min((n - 1) / A.perLine, (int)(bot / A.cellH));
        for (int r = ra; r <= rb; r++)
            for (int c = 0; c < A.perLine; c++) {
                int i = r * A.perLine + c;
                if (i >= n) break;
                CellParts p = gridParts(i);
                float x0 = std::min(p.thumb.x, p.name.x), x1 = std::max(p.thumb.x + p.thumb.w, p.name.x + p.name.w);
                if (x0 < right && x1 > left && p.thumb.y < bot && p.name.y + p.name.h > top) { take(i); any = true; }
            }
    } else if (listView()) { // le voci di cui tocca il fondo, colonna per colonna
        int ca = std::max(0, (int)floorf((left - A.cellX) / A.cellW)), cb = std::min((n - 1) / A.perLine, (int)floorf((right - A.cellX) / A.cellW));
        int ka = std::max(0, (int)(top / A.cellH)), kb = std::min(A.perLine - 1, (int)(bot / A.cellH));
        for (int c = ca; c <= cb; c++)
            for (int k = ka; k <= kb; k++) {
                int i = c * A.perLine + k;
                if (i >= n) break;
                Box b = itemBg(i);
                if (b.x < right && b.x + b.w > left) { take(i); any = true; }
            }
    }
    if (gridView() || listView()) {
        if (any) { // ancora dove è partito, cursore dove è il mouse
            A.anchor = (int)A.view[cellAt(MQ.x0, MQ.y0)];
            A.sel = cellAt(xc, yc);
        }
    } else { // le righe che tocca in altezza
        int i0 = std::max(0, (int)(top / A.cellH)), i1 = std::min(n - 1, (int)(bot / A.cellH));
        for (int i = i0; i <= i1; i++) take(i);
        if (i0 <= i1) { // ancora dove è partito, cursore dove è il mouse: Shift+frecce continua da qui
            A.anchor = (int)A.view[std::min(n - 1, (int)(MQ.y0 / A.cellH))];
            A.sel = std::min(std::max(i0, (int)(yc / A.cellH)), i1);
        }
    }
    A.dirty = true;
    A.mqMs = nowMs() - t0;
    if (A.mqMs > A.mqMax) A.mqMax = A.mqMs;
}

static void mqStart() {
    MQ.active = true;
    MQ.armed = false;
    if (A.cur != MQ.cur || A.selFlags.size() != MQ.base.size()) { mqEnd(); return; }
    A.selFlags = MQ.base; // si riparte dalla selezione di prima del clic
    A.sel = MQ.sel;
    A.anchor = MQ.anchor;
    A.hover = A.linkHover = -1;
    tipHide();
    SetTimer(A.hwnd, 10, ui::DRAG_TICK, nullptr); // scorrimento vicino ai bordi
}

// Movimento col tasto premuto. true se il rettangolo prende il mouse
static bool mqMouseMove(int x, int y, WPARAM wp) {
    if (!MQ.armed && !MQ.active) return false;
    if (!(wp & MK_LBUTTON)) { mqEnd(); return false; }
    MQ.x = x; MQ.y = y;
    if (!MQ.active) {
        if (abs(x - MQ.down.x) <= GetSystemMetrics(SM_CXDRAG) && abs(y - MQ.down.y) <= GetSystemMetrics(SM_CYDRAG)) return true;
        mqStart();
    }
    mqUpdate();
    return true;
}

// Timer: vicino ai bordi (una riga di testo, anche in Icone: con le miniature grandi una fascia
// alta una cella farebbe partire lo scorrimento da mezzo schermo) o oltre, la lista scorre; più il
// mouse è lontano, più veloce. In Elenco i bordi sono quelli a sinistra e a destra
static void mqTick() {
    if (!MQ.active) { KillTimer(A.hwnd, 10); return; }
    bool lst = listView();
    float m = (float)(lst ? MQ.x : MQ.y);
    float edgeLo = (lst ? A.listX : A.listTop) + A.rowH, edgeHi = (lst ? A.listR - A.sbW : A.listTop + A.listH) - A.rowH;
    float d = m < edgeLo ? m - edgeLo : m > edgeHi ? m - edgeHi : 0;
    if (d != 0 && maxScroll() > 0) {
        float k = std::min(ui::MARQUEE_SCROLL_MAX, 1 + fabsf(d) / A.rowH);
        A.scrollTarget += (d < 0 ? -1 : 1) * A.rowH * ui::DRAG_SCROLL * k;
        clampScroll();
        A.scrollY = A.scrollTarget;
    }
    mqUpdate(); // anche senza scorrere: la rotellina può aver spostato la lista sotto il rettangolo
}

// Esc: rimette la selezione di prima del clic
static bool mqCancel() {
    if (!MQ.active) return false;
    if (A.cur == MQ.cur && A.selFlags.size() == MQ.base.size()) {
        A.selFlags = MQ.base;
        A.sel = MQ.sel;
        A.anchor = MQ.anchor;
    }
    mqEnd();
    return true;
}

// Contorno 1 px C_SEL_EDGE, fondo C_MARQUEE, angoli squadrati come in Windows; dentro la lista
static void drawMarquee() {
    if (!MQ.active) return;
    float top = A.listTop, bot = A.listTop + A.listH, l0 = A.listX, r0 = A.listR - A.sbW;
    float ox, oy;
    scrollOffset(ox, oy);
    float ya = std::min(std::max(A.listTop + MQ.y0 - oy, top), bot);
    float yb = std::min(std::max((float)MQ.y, top), bot);
    float xa = std::min(std::max(MQ.x0 - ox, l0), r0), xb = std::min(std::max((float)MQ.x, l0), r0);
    float x = floorf(std::min(xa, xb)), y = floorf(std::min(ya, yb));
    float w = std::max(1.0f, floorf(std::max(xa, xb)) - x), h = std::max(1.0f, floorf(std::max(ya, yb)) - y);
    rect(x, y, w, h, C_MARQUEE);
    rect(x, y, w, 1, C_SEL_EDGE);
    rect(x, y + h - 1, w, 1, C_SEL_EDGE);
    rect(x, y, 1, h, C_SEL_EDGE);
    rect(x + w - 1, y, 1, h, C_SEL_EDGE);
}

// --- colonne --------------------------------------------------------------------------------
// Larghezza w (pixel) per la colonna comandata dal bordo b: tra il minimo e lo spazio che lascia
// al nome il suo minimo (Destinazione anche non più larga di Nome, linkMax). Pannelli: tra il
// minimo e la frazione massima della finestra; l'anteprima anche entro quello che la lista lascia
// con le colonne al minimo
static void colSetWidth(int b, float w) {
    float mn, mx;
    if (b == EDGE_SIDE) {
        mn = px(ui::SIDE_MIN);
        mx = floorf(G.w * ui::SIDE_MAX);
    } else if (b == EDGE_PREVIEW) {
        mn = px(ui::PREVIEW_MIN);
        mx = std::min(floorf(G.w * ui::PREVIEW_MAX), previewRoom());
    } else {
        mn = px(ui::COL_MIN);
        if (b == 2) // Destinazione si ferma dove sarebbe più larga di Nome
            mx = std::min(colRoom() - A.dateW - A.sizeW, linkMax(A.dateX));
        else // Data e Dimensione prendono spazio a Destinazione, che cede fino al minimo (layout)
            mx = colRoom() - (b == 0 ? A.sizeW : A.dateW) - (A.linkW > 0 ? mn : 0);
    }
    w = std::max(mn, std::min(floorf(w), std::max(mn, mx)));
    colChosen(b) = (w + 0.5f) / A.scale; // px() lo riporta esattamente a w
    A.dirty = true;
}

static void colDragStart(int b, int x) {
    tipHide();
    A.colDrag = b;
    A.colDragX0 = x;
    A.colDragW0 = colShown(b);
    A.colDragSaved = colChosen(b);
    SetCapture(A.hwnd);
    SetCursor(LoadCursorW(nullptr, IDC_SIZEWE)); // con il mouse catturato WM_SETCURSOR non arriva
    A.dirty = true;
}

static void colDragMove(int x) {
    if (x == A.colDragX0) { // tornato al punto di partenza: il valore di prima, non quello ristretto
        colChosen(A.colDrag) = A.colDragSaved;
        A.dirty = true;
        return;
    }
    // Il bordo segue il mouse: quello della barra laterale le sta a destra, gli altri a sinistra di ciò che comandano
    float d = (float)(A.colDragX0 - x);
    colSetWidth(A.colDrag, A.colDragW0 + (A.colDrag == EDGE_SIDE ? -d : d));
}

static void panelEdgeReset(int b) { // doppio clic sul bordo di un pannello
    colChosen(b) = b == EDGE_SIDE ? ui::SIDE_W : ui::PREVIEW_W;
    settingsSave();
    A.dirty = true;
}

// keep false (Esc): rimette la larghezza di prima
static bool colDragEnd(bool keep) {
    if (A.colDrag < 0) return false;
    float& v = colChosen(A.colDrag);
    A.colDrag = -1;
    if (!keep) v = A.colDragSaved;
    else if (v != A.colDragSaved) settingsSave(); // una volta sola, al rilascio
    if (GetCapture() == A.hwnd) ReleaseCapture();
    A.dirty = true;
    return true;
}

// Larghezza del testo con le cifre più larghe al posto di quelle vere: con le cifre tabulari
// (Segoe UI) è esatta, con un carattere a cifre proporzionali è per eccesso, quindi non tronca.
static float digitsWidth(const wchar_t* s, int n) {
    int w0 = textWidth(L"0", 1), wmax = w0, nd = 0;
    for (wchar_t c = L'1'; c <= L'9'; c++) wmax = std::max(wmax, textWidth(&c, 1));
    wchar_t shape[64];
    n = std::min(n, 63);
    for (int i = 0; i < n; i++) {
        bool d = s[i] >= L'0' && s[i] <= L'9';
        shape[i] = d ? L'0' : s[i];
        nd += d;
    }
    return (float)(textWidth(shape, n) + nd * (wmax - w0));
}

// Doppio clic sul bordo b: la colonna alla sua destra si adatta al testo più largo tra
// l'intestazione (con la freccia dell'ordinamento) e tutte le voci della vista. Le date hanno
// tutte la stessa forma; per le dimensioni basta il valore più grande di ogni unità (con e senza
// decimale): niente formattazione di ogni voce, quindi pochi decimi di ms anche con 30k voci.
// Le destinazioni dei collegamenti si misurano una per una (sono poche, e solo i .lnk).
static void colFit(int b) {
    double t0 = nowMs();
    wchar_t buf[64];
    std::wstring head = colTitle(b == 0 ? COL_DATE : b == 1 ? COL_SIZE : COL_LINK, true);
    float w = (float)textWidth(head.c_str(), (int)head.size());
    if (A.cur && b == 2) {
        for (uint32_t ei : A.view)
            if (const LinkTarget* t = linkOf(A.cur, ei)) w = std::max(w, (float)textWidth(t->path.c_str(), (int)t->path.size()));
    } else if (A.cur && b == 0) {
        if (A.cur->isDrives) {
            for (uint32_t ei : A.view) {
                const wchar_t* t = driveKind((UINT)A.cur->e[ei].size);
                w = std::max(w, (float)textWidth(t, (int)wcslen(t)));
            }
        } else if (!A.view.empty()) {
            int n = fmtDate(A.cur->e[A.view[0]].mtime, buf);
            w = std::max(w, digitsWidth(buf, n));
        }
    } else if (A.cur && !A.cur->isDrives) {
        uint64_t rep[5][2] = {}; // per unità (B, KB, MB, GB, TB): il più grande sotto 10 e da 10 in su
        bool has[5][2] = {};
        for (uint32_t ei : A.view) {
            const Entry& e = A.cur->e[ei];
            uint64_t size = e.size;
            if ((e.attr & FILE_ATTRIBUTE_DIRECTORY) && A.cur->zipLen) {
                if (!A.dirSizes) continue; // zip: il totale è già nella voce
            } else if (e.attr & FILE_ATTRIBUTE_DIRECTORY) {
                // Con le dimensioni delle cartelle la colonna mostra anche i loro totali (render.cpp)
                DirSize d;
                bool computing = false;
                if (!A.dirSizes || !dirSizeGet(joinPath(A.cur->path, A.cur->name(e)), d, computing) || (!d.files && d.denied))
                    continue;
                size = d.bytes;
            }
            int u = 0, c = 1; // stessi passaggi di fmtSize
            if (size >= 1024) {
                double v = (double)size / 1024.0;
                u = 1;
                while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
                c = v < 10 ? 0 : 1;
            }
            if (!has[u][c] || size > rep[u][c]) { rep[u][c] = size; has[u][c] = true; }
        }
        for (int u = 0; u < 5; u++)
            for (int c = 0; c < 2; c++)
                if (has[u][c]) w = std::max(w, digitsWidth(buf, fmtSize(rep[u][c], buf)));
    }
    if (b != 1) w += A.padX; // lo spazio prima della colonna successiva sta in Data e in Destinazione
    colSetWidth(b, ceilf(w));
    settingsSave();
    A.colFitMs = nowMs() - t0;
}
