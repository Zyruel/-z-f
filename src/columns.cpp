// [z]f — colonne della lista in Dettagli: quali ci sono, in che ordine, come si chiamano, cosa
// mostrano e quali si vedono. Layout (layoutItems, app.cpp), disegno (render.cpp), bordi
// (listmouse.cpp), impostazioni (settings.cpp) e menu delle colonne (menu.cpp) passano da qui.

struct ColDef {
    const wchar_t* title;    // nell'intestazione
    const wchar_t* sortBy;   // tooltip dell'intestazione
    const wchar_t* filterBy; // tooltip dell'imbuto del filtro; nullptr: la colonna non ha filtro
    const char* key;         // impostazioni.txt: nella riga "colonne" e in "colonna_<key>" (larghezza)
    bool right;              // testo allineato a destra (Dimensione)
    bool desc;               // il primo clic ordina dal più recente o dal più grande
};
static const ColDef kCols[COL_COUNT] = { // per COL_*
    { L"Nome",           L"Ordina per nome",              nullptr,                         nullptr,        false, false },
    { L"Data modifica",  L"Ordina per data di modifica",  L"Filtra per data di modifica",  "data",         false, true },
    { L"Dimensione",     L"Ordina per dimensione",        L"Filtra per dimensione",        "dimensione",   true,  true },
    { L"Destinazione",   L"Ordina per destinazione",      nullptr,                         "destinazione", false, false },
    { L"Tipo",           L"Ordina per tipo",              L"Filtra per tipo",              "tipo",         false, false },
    { L"Data creazione", L"Ordina per data di creazione", L"Filtra per data di creazione", "creazione",    false, true },
    { L"Attributi",      L"Ordina per attributi",         L"Filtra per attributi",         "attributi",    false, false },
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

// --- valori di una colonna: filtro per colonna (e i gruppi, che li riusano) -------------------------
// colBucket dice in quale valore della colonna sta una voce. Le chiavi non dipendono dalla lettura
// (un'estensione è il suo testo), così il filtro resta uguale quando la cartella si rilegge.

static bool colFilterable(int c) { return c >= 0 && c < COL_COUNT && kCols[c].filterBy; }

// Tipo: cartelle, senza estensione, un'estensione (ColBucket::ext); in Questo PC TK_DRIVE + il tipo
// di unità (Entry::size)
enum : uint32_t { TK_DIR = 0, TK_NOEXT = 1, TK_EXT = 2, TK_DRIVE = 16 };
// Date: i periodi dal più vicino, nell'ordine in cui si provano; prima di quest'anno la chiave è
// l'anno stesso (>= 1601, oltre DK_NONE)
enum : uint32_t { DK_FUTURE = 0, DK_TODAY, DK_YESTERDAY, DK_WEEK, DK_LASTWEEK, DK_MONTH, DK_LASTMONTH, DK_YEAR, DK_NONE };
// Dimensione: le cartelle senza un totale, poi le fasce (kSizeTop: il massimo di ognuna)
enum : uint32_t { SK_DIR = 0, SK_EMPTY, SK_16K, SK_1M, SK_128M, SK_1G, SK_4G, SK_MORE };
static const uint64_t kSizeTop[] = { 0, 16ull << 10, 1ull << 20, 128ull << 20, 1ull << 30, 4ull << 30 }; // da SK_EMPTY
// Attributi: un bit per lettera (kAttrBits, nel loro ordine), anche più d'uno; AK_NONE senza nessuno
static constexpr uint32_t AK_NONE = 1u << 6;

static const wchar_t* const kDateLabels[] = { L"Data futura", L"Oggi", L"Ieri", L"Questa settimana", L"Settimana scorsa",
                                              L"Questo mese", L"Mese scorso", L"Quest'anno", L"Senza data" };
static const wchar_t* const kSizeLabels[] = { L"Cartelle", L"Vuoti", L"Fino a 16 KB", L"Fino a 1 MB", L"Fino a 128 MB",
                                              L"Fino a 1 GB", L"Fino a 4 GB", L"Oltre 4 GB" };
static const wchar_t* const kAttrLabels[] = { L"Sola lettura (R)", L"Nascosto (H)", L"Sistema (S)", L"Archivio (A)",
                                              L"Compresso (C)", L"Crittografato (E)", L"Nessun attributo" };
static constexpr int kYearTable = 1980; // gli inizi d'anno precalcolati arrivano fin qui; prima si calcolano voce per voce

// Ciò che colBucket deve sapere oltre alla voce, calcolato una volta per applicazione (colCtxInit):
// gli inizi dei periodi in ora locale, come FILETIME UTC, e i totali delle cartelle già contati
struct ColCtx {
    const Listing* l = nullptr;
    uint64_t from[DK_NONE] = {};    // inizio di ogni periodo; from[DK_FUTURE] è domani
    int year = 0;                   // l'anno in corso: yearFrom[i] è l'inizio di year - 1 - i
    std::vector<uint64_t> yearFrom;
    std::vector<uint64_t> dirTotal; // Dimensione: per indice di voce, UINT64_MAX se non noto (o non cartella)
};

static constexpr uint64_t kDay100ns = 864000000000ull;
// Mezzanotte locale del giorno di st (data locale) come FILETIME UTC: l'ora legale la decide Windows
static uint64_t localMidnight(SYSTEMTIME st) {
    st.wHour = st.wMinute = st.wSecond = st.wMilliseconds = 0;
    SYSTEMTIME u;
    FILETIME f;
    if (!TzSpecificLocalTimeToSystemTime(nullptr, &st, &u)) u = st;
    return SystemTimeToFileTime(&u, &f) ? fileTime64(f) : 0;
}
// La data locale di st spostata di days giorni (solo calendario, senza fusi)
static SYSTEMTIME dayAdd(SYSTEMTIME st, int days) {
    st.wHour = st.wMinute = st.wSecond = st.wMilliseconds = 0;
    FILETIME f;
    if (!SystemTimeToFileTime(&st, &f)) return st;
    uint64_t v = fileTime64(f) + (uint64_t)((int64_t)days * (int64_t)kDay100ns);
    f.dwLowDateTime = (DWORD)v;
    f.dwHighDateTime = (DWORD)(v >> 32);
    FileTimeToSystemTime(&f, &st);
    return st;
}

static bool colNeedsTotals(const Listing* l) { return A.dirSizes && !l->isDrives && !l->zipLen; }

// cols: un bit per COL_* che si chiederà a colBucket (date e Dimensione hanno qualcosa da preparare)
static void colCtxInit(ColCtx& c, const Listing* l, uint32_t cols) {
    c.l = l;
    if (cols & (1u << COL_DATE | 1u << COL_CTIME)) {
        SYSTEMTIME now;
        GetLocalTime(&now);
        c.from[DK_FUTURE] = localMidnight(dayAdd(now, 1));
        c.from[DK_TODAY] = localMidnight(now);
        c.from[DK_YESTERDAY] = localMidnight(dayAdd(now, -1));
        SYSTEMTIME mon = dayAdd(now, -((now.wDayOfWeek + 6) % 7)); // la settimana comincia di lunedì
        c.from[DK_WEEK] = localMidnight(mon);
        c.from[DK_LASTWEEK] = localMidnight(dayAdd(mon, -7));
        SYSTEMTIME m = now;
        m.wDay = 1;
        c.from[DK_MONTH] = localMidnight(m);
        if (m.wMonth == 1) { m.wMonth = 12; m.wYear--; } else m.wMonth--;
        c.from[DK_LASTMONTH] = localMidnight(m);
        SYSTEMTIME y = now;
        y.wMonth = y.wDay = 1;
        c.from[DK_YEAR] = localMidnight(y);
        c.year = now.wYear;
        c.yearFrom.clear();
        for (int yy = now.wYear - 1; yy >= kYearTable; yy--) { y.wYear = (WORD)yy; c.yearFrom.push_back(localMidnight(y)); }
    }
    if ((cols & 1u << COL_SIZE) && colNeedsTotals(l)) dirSizeTotals(l, c.dirTotal);
    else c.dirTotal.clear();
}

static uint32_t colDateKey(uint64_t t, const ColCtx& c) {
    if (!t) return DK_NONE;
    if (t >= c.from[DK_FUTURE]) return DK_FUTURE;
    // Il primo periodo che comincia prima: gli inizi non sono in ordine (la settimana può cominciare
    // nel mese scorso), ma ogni periodo è provato dopo quelli più vicini
    for (uint32_t k = DK_TODAY; k <= DK_YEAR; k++) if (t >= c.from[k]) return k;
    for (size_t i = 0; i < c.yearFrom.size(); i++) if (t >= c.yearFrom[i]) return (uint32_t)(c.year - 1 - (int)i);
    FILETIME f = { (DWORD)t, (DWORD)(t >> 32) };
    SYSTEMTIME u, loc;
    if (!FileTimeToSystemTime(&f, &u)) return DK_NONE;
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &u, &loc)) loc = u;
    return std::max<uint32_t>(loc.wYear, DK_NONE + 1);
}

// Valore della voce e (una voce di c.l) nella colonna col. ext: per un'estensione di Tipo, il suo
// testo in minuscolo (dentro Listing::lower, finisce col nome)
struct ColBucket { uint32_t key; const wchar_t* ext; };
static ColBucket colBucket(int col, const Entry& e, const ColCtx& c) {
    const Listing* l = c.l;
    bool dir = (e.attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    switch (col) {
    case COL_TYPE:
        if (l->isDrives) return { TK_DRIVE + (uint32_t)e.size, nullptr };
        if (dir) return { TK_DIR, nullptr };
        if (!e.extOff) return { TK_NOEXT, nullptr };
        return { TK_EXT, l->lowerName(e) + e.extOff };
    case COL_DATE:  return { colDateKey(e.mtime, c), nullptr };
    case COL_CTIME: return { colDateKey(e.ctime, c), nullptr };
    case COL_SIZE: {
        uint64_t s = e.size;
        if (dir && !l->zipLen) { // il totale contato (dirsize.cpp), se c'è: come nella colonna
            size_t i = (size_t)(&e - l->e.data());
            if (i >= c.dirTotal.size() || c.dirTotal[i] == UINT64_MAX) return { SK_DIR, nullptr };
            s = c.dirTotal[i];
        } else if (dir && !A.dirSizes) { // zip: le cartelle hanno il totale, che si mostra con l'opzione
            return { SK_DIR, nullptr };
        }
        uint32_t k = SK_EMPTY;
        while (k < SK_MORE && s > kSizeTop[k - SK_EMPTY]) k++;
        return { k, nullptr };
    }
    case COL_ATTR: {
        uint32_t m = 0;
        for (int i = 0; i < 6; i++) if (e.attr & kAttrBits[i]) m |= 1u << i;
        return { m ? m : AK_NONE, nullptr };
    }
    }
    return { 0, nullptr };
}

// Testo di un valore (menu del filtro, tooltip). Attributi: v.key è un bit solo
static std::wstring colValueLabel(int col, const ColValue& v) {
    switch (col) {
    case COL_TYPE: {
        if (v.key >= TK_DRIVE) return driveKind(v.key - TK_DRIVE);
        if (v.key == TK_DIR) return L"Cartelle";
        if (v.key == TK_NOEXT) return L"Senza estensione";
        std::wstring s = v.ext; // come nella colonna: in maiuscolo
        if (!s.empty()) CharUpperBuffW(&s[0], (DWORD)s.size());
        return s;
    }
    case COL_DATE: case COL_CTIME:
        return v.key <= DK_NONE ? std::wstring(kDateLabels[v.key]) : std::to_wstring(v.key);
    case COL_SIZE:
        return v.key <= SK_MORE ? kSizeLabels[v.key] : L"";
    case COL_ATTR:
        for (int i = 0; i < 7; i++) if (v.key == 1u << i) return kAttrLabels[i];
        return L"";
    }
    return L"";
}

// Estensioni scelte: in ordine, per la ricerca binaria
static std::vector<std::wstring>::const_iterator colExtFind(const std::vector<std::wstring>& exts, const wchar_t* x) {
    return std::lower_bound(exts.begin(), exts.end(), x, [](const std::wstring& a, const wchar_t* b) { return wcscmp(a.c_str(), b) < 0; });
}
static bool colFilterHas(const ColFilter& f, int c, const ColValue& v) {
    if (c == COL_TYPE && v.key == TK_EXT) {
        auto it = colExtFind(f.exts, v.ext.c_str());
        return it != f.exts.end() && *it == v.ext;
    }
    return std::find(f.keys[c].begin(), f.keys[c].end(), v.key) != f.keys[c].end();
}
// Aggiunge il valore al filtro della colonna, o lo toglie se c'era
static void colFilterFlip(ColFilter& f, int c, const ColValue& v) {
    if (c == COL_TYPE && v.key == TK_EXT) {
        auto it = colExtFind(f.exts, v.ext.c_str());
        if (it != f.exts.end() && *it == v.ext) f.exts.erase(it);
        else f.exts.insert(it, v.ext);
        return;
    }
    auto& k = f.keys[c];
    auto it = std::find(k.begin(), k.end(), v.key);
    if (it != k.end()) k.erase(it); else k.push_back(v.key);
}
// I valori scelti di una colonna, " · " tra l'uno e l'altro (tooltip)
static std::wstring colFilterText(const ColFilter& f, int c) {
    std::wstring s;
    auto add = [&](const ColValue& v) { if (!s.empty()) s += L" \x00B7 "; s += colValueLabel(c, v); };
    for (uint32_t k : f.keys[c]) add(ColValue{ k, L"" });
    if (c == COL_TYPE) for (const std::wstring& x : f.exts) add(ColValue{ TK_EXT, x });
    return s;
}

// Il filtro delle colonne pronto per le voci (colMatchInit, una volta per applicazione): le chiavi
// sotto 64 come bit (gli attributi come la loro maschera), le altre (anni, percorsi di rete) cercate
// nell'elenco, le estensioni con la ricerca binaria
struct ColMatch {
    const ColFilter* f = nullptr;
    ColCtx ctx;
    int cols[COL_COUNT] = {}, n = 0;
    uint64_t bits[COL_COUNT] = {};
};
// skip: la colonna da non applicare (i conteggi del suo menu), -1 nessuna. false: nessun filtro
static bool colMatchInit(ColMatch& m, const ColFilter& f, const Listing* l, int skip) {
    uint32_t used = 0;
    for (int c = 0; c < COL_COUNT; c++) {
        if (c == skip || !f.on(c)) continue;
        m.cols[m.n++] = c;
        used |= 1u << c;
        for (uint32_t k : f.keys[c])
            if (c == COL_ATTR) m.bits[c] |= k;
            else if (k < 64) m.bits[c] |= 1ull << k;
    }
    if (!m.n) return false;
    m.f = &f;
    colCtxInit(m.ctx, l, used);
    return true;
}
static bool colMatches(const ColMatch& m, const Entry& e) {
    for (int i = 0; i < m.n; i++) {
        int c = m.cols[i];
        ColBucket b = colBucket(c, e, m.ctx);
        bool ok;
        if (c == COL_ATTR) ok = (b.key & m.bits[c]) != 0;
        else if (b.ext) { auto it = colExtFind(m.f->exts, b.ext); ok = it != m.f->exts.end() && *it == b.ext; }
        else if (b.key < 64) ok = (m.bits[c] >> b.key) & 1;
        else ok = std::find(m.f->keys[c].begin(), m.f->keys[c].end(), b.key) != m.f->keys[c].end();
        if (!ok) return false;
    }
    return true;
}

// Ordine dei valori di una colonna, nel menu del filtro e nei gruppi. Tipo: i tipi di unità (Questo
// PC), "Cartelle", "Senza estensione", poi le estensioni dalla A; date: i periodi dal più vicino, gli
// anni dal più recente, "Senza data"; Dimensione e Attributi: per chiave (le fasce crescono, gli
// attributi nell'ordine delle lettere)
static int colValueClass(int c, const ColValue& v) {
    if (c == COL_TYPE) return v.key >= TK_DRIVE ? 0 : v.key == TK_DIR ? 1 : v.key == TK_NOEXT ? 2 : 3;
    if (c == COL_DATE || c == COL_CTIME) return v.key < DK_NONE ? 0 : v.key > DK_NONE ? 1 : 2;
    return 0;
}
static bool colValueBefore(int c, const ColValue& a, const ColValue& b) {
    int ka = colValueClass(c, a), kb = colValueClass(c, b);
    if (ka != kb) return ka < kb;
    if (c == COL_TYPE && a.key == TK_EXT) return a.ext < b.ext;
    if ((c == COL_DATE || c == COL_CTIME) && ka == 1) return a.key > b.key; // gli anni, dal più recente
    return a.key < b.key;
}
static bool colValueSame(const ColValue& a, const ColValue& b) { return a.key == b.key && a.ext == b.ext; }

// I valori distinti di una colonna, ognuno col suo numero di voci (menu del filtro, gruppi): at dà
// l'indice del valore in vals, aggiungendolo la prima volta. Le chiavi sotto 64 da una tabella, le
// altre (anni, percorsi di rete: poche) da un elenco, le estensioni da una mappa sul loro testo, che
// deve restare dov'è finché si conta (Listing::lower, ColFilter::exts)
struct ColCount { ColValue v; int n = 0; };
struct ColTally {
    std::vector<ColCount> vals;
    int slot[64];                                       // "small" è una macro degli header RPC
    std::vector<std::pair<uint32_t, int>> big;
    std::unordered_map<std::wstring_view, int> exts;
    ColTally() { std::fill(slot, slot + 64, -1); }
    int at(uint32_t key, const wchar_t* ext) {
        int i = (int)vals.size();
        if (ext) {
            auto it = exts.find(std::wstring_view(ext));
            if (it != exts.end()) return it->second;
            exts.emplace(std::wstring_view(ext), i);
        } else if (key < 64) {
            if (slot[key] >= 0) return slot[key];
            slot[key] = i;
        } else {
            for (auto& b : big) if (b.first == key) return b.second;
            big.emplace_back(key, i);
        }
        vals.push_back(ColCount{ ColValue{ key, ext ? ext : L"" }, 0 });
        return i;
    }
};

// I valori della colonna c tra le voci vis (quelle che passano gli altri filtri), ognuno col numero
// di voci, nell'ordine del menu; con i valori scelti in f che non ci sono più (0), per poterli togliere
static void colValues(int c, const Listing* l, const std::vector<uint32_t>& vis, const ColFilter& f, std::vector<ColCount>& out) {
    ColCtx ctx;
    colCtxInit(ctx, l, 1u << c);
    ColTally t;
    for (uint32_t k : f.keys[c]) {                      // i valori scelti ci sono anche senza voci
        if (c == COL_ATTR) { for (int i = 0; i < 7; i++) if (k & 1u << i) t.at(1u << i, nullptr); }
        else t.at(k, nullptr);
    }
    if (c == COL_TYPE) for (const std::wstring& x : f.exts) t.at(TK_EXT, x.c_str());
    for (uint32_t ei : vis) {
        ColBucket b = colBucket(c, l->e[ei], ctx);
        if (c == COL_ATTR) { for (int i = 0; i < 7; i++) if (b.key & 1u << i) t.vals[t.at(1u << i, nullptr)].n++; } // una voce in ogni suo attributo
        else t.vals[t.at(b.key, b.ext)].n++;
    }
    out.swap(t.vals);
    std::sort(out.begin(), out.end(), [c](const ColCount& a, const ColCount& b) { return colValueBefore(c, a.v, b.v); });
}

// --- gruppi: le voci della vista divise per il valore di una colonna ----------------------------
// Tipo, date e Dimensione; Attributi no: una voce ha più attributi, e in un gruppo sta una volta
// sola (scelta di Zyruel). In Questo PC solo Tipo, i tipi di unità
static bool colGroupable(int c, const Listing* l) {
    if (!l) return false;
    if (l->isDrives) return c == COL_TYPE;
    return c == COL_TYPE || c == COL_DATE || c == COL_CTIME || c == COL_SIZE;
}
// La lista di App è raggruppata (anche con la vista vuota: la geometria di Elenco cambia lo stesso)
static bool grouped() { return colGroupable(A.groupCol, A.cur); }
// La vista dipende dai totali delle cartelle (filtro o gruppi per Dimensione): un totale appena contato
// può far entrare o uscire una cartella, o spostarla da "Cartelle" alla sua fascia (onDirSize)
static bool viewUsesDirTotals() { return A.colFilter.on(COL_SIZE) || (grouped() && A.groupCol == COL_SIZE); }

// A.view divisa in gruppi (A.groups) nell'ordine di colValueBefore, rovesciato se la lista è
// ordinata per la stessa colonna nel verso opposto (il menu delle date va dal più recente, gli altri
// crescono): ordinando per Dimensione dal più grande il primo gruppo è "Oltre 4 GB". Dentro un gruppo
// resta l'ordine della vista (ordinamento stabile per rango, in O(n)). Le voci dei gruppi chiusi
// escono dalla vista, come col filtro; il gruppo resta, col suo numero
static void groupsBuild() {
    A.groups.clear();
    if (!grouped() || A.view.empty()) return;
    int c = A.groupCol;
    ColCtx ctx;
    colCtxInit(ctx, A.cur, 1u << c);
    ColTally t;
    std::vector<int> gid(A.view.size());
    for (size_t i = 0; i < A.view.size(); i++) {
        ColBucket b = colBucket(c, A.cur->e[A.view[i]], ctx);
        gid[i] = t.at(b.key, b.ext);
        t.vals[gid[i]].n++;
    }
    std::vector<int> rank(t.vals.size());
    for (size_t i = 0; i < rank.size(); i++) rank[i] = (int)i;
    bool rev = A.sortCol == c && A.sortDesc != (c == COL_DATE || c == COL_CTIME);
    std::sort(rank.begin(), rank.end(), [&](int a, int b) {
        return rev ? colValueBefore(c, t.vals[b].v, t.vals[a].v) : colValueBefore(c, t.vals[a].v, t.vals[b].v);
    });
    std::vector<int> start(t.vals.size(), -1); // dove va la prossima voce di ogni valore; -1: gruppo chiuso
    int pos = 0;
    for (int g : rank) {
        Group gr;
        gr.v = std::move(t.vals[g].v);
        gr.total = t.vals[g].n;
        for (const ColValue& v : A.groupClosed) if (colValueSame(v, gr.v)) { gr.closed = true; break; }
        gr.first = pos;
        gr.n = gr.closed ? 0 : gr.total;
        if (!gr.closed) start[g] = pos;
        pos += gr.n;
        A.groups.push_back(std::move(gr));
    }
    std::vector<uint32_t> out((size_t)pos);
    for (size_t i = 0; i < A.view.size(); i++)
        if (start[gid[i]] >= 0) out[(size_t)start[gid[i]]++] = A.view[i];
    A.view.swap(out);
}

// --- intestazione di Dettagli: il titolo e l'imbuto del filtro ------------------------------------
// L'imbuto sta a GAP_XS dal titolo, dalla parte dell'interno della colonna: a destra, a sinistra in
// Dimensione (allineata a destra). Così il titolo non si sposta quando compare; se non c'è posto il
// titolo finisce in "…". glyph: l'imbuto si vede (sotto il mouse, menu aperto, colonna filtrata)
struct HeadParts { float textX, textEnd, glyphX; };
static HeadParts colHeadParts(int c, const std::wstring& t, bool glyph) {
    float x0 = colStart(c), x1 = colEnd(c), tw = (float)textWidth(t.c_str(), (int)t.size());
    float gw = glyph ? (float)F.lineH + px(ui::GAP_XS) : 0;
    HeadParts h;
    if (kCols[c].right) {
        h.textEnd = x1;
        h.textX = std::max(x0 + gw, x1 - tw);
        h.glyphX = h.textX - gw;
    } else {
        h.textX = x0;
        h.textEnd = std::min(x0 + tw, x1 - gw);
        h.glyphX = h.textEnd + px(ui::GAP_XS);
    }
    return h;
}
// L'imbuto del filtro: si vede nell'intestazione di Dettagli per la colonna sotto il mouse,
// per quella del menu aperto e per quelle che filtrano
static bool colGlyphShown(int c) {
    if (!colFilterable(c) || A.zoom != ZOOM_DETAILS || A.colWd[c] <= 0) return false;
    return c == A.colFltHead || A.colFilter.on(c) || (A.menu.open && A.menu.kind == MENU_FILTER && A.menu.target == c);
}
