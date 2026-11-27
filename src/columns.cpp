// [z]f — colonne della lista in Dettagli: quali ci sono, in che ordine, come si chiamano, cosa
// mostrano e quali si vedono. Layout (layoutItems, app.cpp), disegno (render.cpp), bordi
// (listmouse.cpp), impostazioni (settings.cpp) e menu delle colonne (menu.cpp) passano da qui.

struct ColDef {
    const wchar_t* title;  // nell'intestazione
    const wchar_t* sortBy; // tooltip dell'intestazione
    const char* key;       // impostazioni.txt: nella riga "colonne" e in "colonna_<key>" (larghezza)
    bool right;            // testo allineato a destra (Dimensione)
    bool desc;             // il primo clic ordina dal più recente o dal più grande
};
static const ColDef kCols[COL_COUNT] = { // per COL_*
    { L"Nome",           L"Ordina per nome",              nullptr,        false, false },
    { L"Data modifica",  L"Ordina per data di modifica",  "data",         false, true },
    { L"Dimensione",     L"Ordina per dimensione",        "dimensione",   true,  true },
    { L"Destinazione",   L"Ordina per destinazione",      "destinazione", false, false },
    { L"Tipo",           L"Ordina per tipo",              "tipo",         false, false },
    { L"Data creazione", L"Ordina per data di creazione", "creazione",    false, true },
    { L"Attributi",      L"Ordina per attributi",         "attributi",    false, false },
};
// Da sinistra, dopo Nome (che c'è sempre)
static const int kColOrder[COL_COUNT - 1] = { COL_LINK, COL_TYPE, COL_DATE, COL_CTIME, COL_SIZE, COL_ATTR };
// Con la finestra stretta cedono in quest'ordine, ognuna fino al minimo, prima che Nome scenda
// sotto COL_NAME_MIN: prima le meno usate, per ultime data e dimensione
static const int kColYield[COL_COUNT - 1] = { COL_LINK, COL_ATTR, COL_CTIME, COL_TYPE, COL_DATE, COL_SIZE };

// La colonna c ha senso nella lista di App ed è tra quelle scelte (menu delle colonne). Questo PC
// ha solo il tipo di unità; Destinazione c'è solo con almeno un collegamento (links.cpp);
// l'anteprima a destra (noDate) ha solo Dimensione
static bool colWanted(int c, bool noDate) {
    if (c == COL_NAME) return true;
    if (A.cur && A.cur->isDrives) return c == COL_TYPE;
    if (!(A.colOn >> c & 1)) return false;
    if (noDate) return c == COL_SIZE;
    if (c == COL_LINK) return A.cur && !A.cur->lnk.empty();
    return true;
}

// Colonna più a destra tra quelle mostrate (layoutItems), -1 se c'è solo Nome
static int colLast() {
    for (int i = COL_COUNT - 2; i >= 0; i--)
        if (A.colWd[kColOrder[i]] > 0) return kColOrder[i];
    return -1;
}
// La colonna mostrata prima di c è allineata a destra (Dimensione): Nome e le altre no
static bool colAfterRight(int c) {
    int prev = -1;
    for (int i = 0; i < COL_COUNT - 1 && kColOrder[i] != c; i++)
        if (A.colWd[kColOrder[i]] > 0) prev = kColOrder[i];
    return prev >= 0 && kCols[prev].right;
}
// Testo della colonna c, da colStart a colEnd. Lo spazio PAD_X tra due testi sta nella colonna di
// sinistra (alla sua fine); dopo una allineata a destra sta in quella di destra (al suo inizio),
// così il testo allineato a destra arriva al bordo della sua colonna come quando è l'ultima, che
// finisce al margine della lista (lì il margine c'è già)
static float colStart(int c) { return A.colX[c] + (colAfterRight(c) ? A.padX : 0); }
static float colEnd(int c) { return A.colX[c] + A.colWd[c] - (c == colLast() || kCols[c].right ? 0 : A.padX); }

// La colonna che ha la freccia dell'ordinamento: quella dell'ordinamento se si vede, altrimenti Nome
static int colArrow() {
    int c = A.sortCol;
    if (c == COL_NAME || c < 0 || c >= COL_COUNT) return COL_NAME;
    if (A.zoom == ZOOM_DETAILS) return A.colWd[c] > 0 ? c : COL_NAME;
    for (int i = 0; i < A.sortN; i++) if (A.sortC[i] == c) return c;
    return COL_NAME;
}

// Titolo di una colonna nell'intestazione; arrow: con la freccia dell'ordinamento
static std::wstring colTitle(int col, bool arrow) {
    std::wstring t = kCols[col].title;
    if (arrow) t += A.sortDesc ? L" \x2193" : L" \x2191";
    return t;
}

// Colonna con questa chiave di impostazioni.txt, -1 se nessuna
static int colByKey(const std::wstring& k) {
    for (int c = 1; c < COL_COUNT; c++) {
        const char* s = kCols[c].key;
        size_t n = strlen(s);
        if (k.size() != n) continue;
        size_t i = 0;
        while (i < n && k[i] == (wchar_t)s[i]) i++;
        if (i == n) return c;
    }
    return -1;
}

// Testo della cella di Tipo, delle date o di Attributi per una voce (buf: almeno 64 caratteri);
// 0 se la cella è vuota. Destinazione e Dimensione hanno il loro disegno (render.cpp)
static int colText(const Listing* l, int c, const Entry& e, wchar_t* buf) {
    switch (c) {
    case COL_TYPE: {
        const wchar_t* t = l->isDrives ? driveKind((UINT)e.size) : (e.attr & FILE_ATTRIBUTE_DIRECTORY) ? L"Cartella" : nullptr;
        if (t) { int n = (int)std::min<size_t>(wcslen(t), 63); wmemcpy(buf, t, n); buf[n] = 0; return n; }
        if (!e.extOff) return 0;
        int n = (int)std::min<uint32_t>(e.nameLen - e.extOff, 63);
        wmemcpy(buf, l->name(e) + e.extOff, n);
        buf[n] = 0;
        CharUpperBuffW(buf, (DWORD)n);
        return n;
    }
    case COL_DATE:  return fmtDate(e.mtime, buf);
    case COL_CTIME: return e.ctime ? fmtDate(e.ctime, buf) : 0;
    case COL_ATTR:  return attrText(e.attr, buf);
    }
    return 0;
}

// Lunghezza del nome come si mostra nella lista: senza ".estensione" se l'opzione la nasconde (le
// cartelle non ne hanno). Tooltip, anteprima, rinomina e percorsi usano sempre il nome intero
static inline int shownLen(const Entry& e) { return !A.showExt && e.extOff ? (int)e.extOff - 1 : (int)e.nameLen; }
