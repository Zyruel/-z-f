// [z]f — "Vai a" (Ctrl+P · Ctrl+G): ricerca fuzzy istantanea sulle cartelle.
//
// Da dove vengono le voci:
//  - cartelle recenti: ogni cartella aperta (non le ricariche), da qualsiasi scheda, con il numero
//    di visite e l'ultima visita. Stanno in %APPDATA%\z-f\recenti.txt, una riga per cartella:
//    percorso, tab, visite, tab, ultima visita (FILETIME). Il file si legge all'avvio sul thread
//    veloce della barra laterale e si scrive con saveDataAsync;
//  - preferiti, unità, cartelle aperte nelle schede: già in memoria;
//  - sottocartelle delle cartelle aperte nelle schede: già lette (Listing), nessun disco;
//  - sottocartelle dei preferiti e delle cartelle recenti più usate: all'apertura del pannello le
//    legge un thread, che le consegna una cartella alla volta (prima le unità locali, poi la rete).
//    Restano in memoria: all'apertura successiva ci sono già e si rileggono se vecchie.
// Il thread dell'interfaccia cerca solo in memoria, a ogni tasto.
//
// Ricerca: le lettere scritte devono comparire in ordine nel nome, anche non vicine (o nel
// percorso, se nel nome non ci sono). Punti per l'inizio di una parola, le lettere consecutive, il
// prefisso e il nome uguale; poi pesano la fonte (preferito, scheda, recente) e quanto la cartella
// è usata. Un percorso scritto (C:\…, \\server\…, %VARIABILE%) mostra le sottocartelle di quel
// livello, filtrate da quello che segue l'ultima "\": Tab completa la voce e scende di un livello.

namespace gotolim {
constexpr int RECENT_MAX = 300;       // cartelle recenti ricordate
constexpr int READ_RECENTS = 20;      // sottocartelle lette per le recenti più usate
constexpr int DIR_MAX = 3000;         // sottocartelle per cartella, al massimo
constexpr int RESULTS = 50;           // risultati di una ricerca
constexpr int CACHE_MAX = 300;        // cartelle con le sottocartelle in memoria
constexpr double STALE_MS = 10000;    // sottocartelle in memoria più vecchie di così: si rileggono
constexpr double ASK_AGAIN_MS = 30000;// lettura chiesta e mai arrivata (rete lenta): si richiede dopo
}                                     // la soglia del registro è ui::LOG_GOTO (theme.h)

// --- cartelle recenti -------------------------------------------------------------------------
struct Recent { std::wstring path; uint32_t visits; uint64_t last; }; // last: FILETIME
struct Recents { std::vector<Recent> v; bool loaded = false; } RC;

static uint64_t fileTimeNow() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
}

// Quanto una cartella è "di casa": visite (logaritmo, così poche visite recenti contano) più un
// bonus per quanto è recente l'ultima
static float recentRank(const Recent& r, uint64_t now) {
    double h = (now > r.last ? (double)(now - r.last) : 0.0) / 36e9; // FILETIME: 100 ns
    float b = h < 1 ? 4.0f : h < 24 ? 3.0f : h < 24 * 7 ? 2.0f : h < 24 * 30 ? 1.0f : 0.0f;
    return logf(1.0f + (float)r.visits) + b;
}

static int recentFind(const std::wstring& path, size_t limit = (size_t)-1) {
    for (size_t i = 0; i < RC.v.size() && i < limit; i++) if (samePath(RC.v[i].path, path)) return (int)i;
    return -1;
}

static void recentSave() {
    if (!RC.loaded) return; // prima che arrivi il file si salverebbe un elenco parziale
    uint64_t now = fileTimeNow();
    std::vector<const Recent*> o;
    for (const Recent& r : RC.v) o.push_back(&r);
    std::sort(o.begin(), o.end(), [&](const Recent* a, const Recent* b) { return recentRank(*a, now) > recentRank(*b, now); });
    std::string d;
    for (const Recent* r : o)
        d += toUtf8(r->path) + "\t" + std::to_string(r->visits) + "\t" + std::to_string(r->last) + "\r\n";
    saveDataAsync(L"recenti.txt", std::move(d));
}

static void recentTrim() {
    uint64_t now = fileTimeNow();
    while ((int)RC.v.size() > gotolim::RECENT_MAX) {
        size_t worst = 0;
        for (size_t i = 1; i < RC.v.size(); i++)
            if (recentRank(RC.v[i], now) < recentRank(RC.v[worst], now)) worst = i;
        RC.v.erase(RC.v.begin() + worst);
    }
}

static void recentVisit(const std::wstring& path) {
    if (path.empty()) return; // Questo PC
    int i = recentFind(path);
    if (i >= 0) { RC.v[i].visits++; RC.v[i].last = fileTimeNow(); RC.v[i].path = path; }
    else RC.v.push_back(Recent{ path, 1, fileTimeNow() });
    recentTrim();
    recentSave();
}

static void recentForget(const std::wstring& path) {
    int i = recentFind(path);
    if (i < 0) return;
    RC.v.erase(RC.v.begin() + i);
    recentSave();
}

// recenti.txt letto all'avvio. Le cartelle aperte prima del suo arrivo (la cartella iniziale) si
// sommano a quelle del file.
static void recentLoaded(const std::vector<std::wstring>& lines) {
    size_t before = RC.v.size();
    for (const std::wstring& l : lines) {
        size_t a = l.find(L'\t'), b = a == std::wstring::npos ? a : l.find(L'\t', a + 1);
        Recent r{ l.substr(0, a), 1, 0 };
        if (r.path.empty()) continue;
        if (a != std::wstring::npos) r.visits = (uint32_t)std::max(1, _wtoi(l.c_str() + a + 1));
        if (b != std::wstring::npos) r.last = _wcstoui64(l.c_str() + b + 1, nullptr, 10);
        int i = recentFind(r.path, before); // solo tra le poche già in memoria
        if (i >= 0) { RC.v[i].visits += r.visits; RC.v[i].last = std::max(RC.v[i].last, r.last); }
        else RC.v.push_back(std::move(r));
    }
    RC.loaded = true;
    recentTrim();
    if (before) recentSave();
}

// --- stato del pannello -----------------------------------------------------------------------
struct GoDir {                    // cartella di cui si conoscono le sottocartelle
    std::wstring path, fold;      // fold: piegato (minuscolo, senza accenti)
    std::vector<std::wstring> names, folds;
    std::vector<uint8_t> hidden;
    double at = -1e18;            // quando sono state lette (nowMs)
    double askedAt = 0;           // lettura chiesta e non ancora arrivata (0: nessuna)
    bool ok = false;
    const Listing* from = nullptr;// riempita da una cartella aperta in una scheda (niente disco)
    uint64_t fromGen = 0;
};
struct GoItem {                   // voce in memoria: recente, preferito, unità, scheda
    std::wstring path, label, name; // name: ultimo elemento del percorso
    std::wstring labelFold, pathFold;
    int recent = -1;              // indice in RC.v
    bool drive = false;
    int bonus = 0;
};
struct GoRes { int dir, idx, score; uint8_t where; }; // dir: -1 voce, -2 percorso scritto; where 1: trovata nel percorso
struct GoActive { int dir, bonus; };

struct GotoPanel {
    bool open = false;
    double openedAt = 0, closedAt = -1e18;
    TextField field;
    std::vector<GoItem> items;
    std::vector<GoDir> dirs;
    std::unordered_map<std::wstring, int> dirIdx; // percorso in minuscolo → dirs
    std::vector<GoActive> active; // cartelle le cui sottocartelle entrano nella ricerca
    std::vector<GoRes> res;
    std::wstring q;               // ricerca piegata (per evidenziare le lettere)
    bool pathQuery = false;       // la ricerca contiene "\": si confronta il percorso
    std::wstring typed;           // percorso scritto
    int absDir = -1;              // percorso scritto: la cartella di cui si mostrano le sottocartelle
    int hot = 0, first = 0, visible = 0;
    float x = 0, y = 0, w = 0, h = 0, fieldY = 0, listY = 0, ih = 0; // geometria
} GT;

static bool gotoIsOpen() { return GT.open; }

// Minuscolo e senza accenti, carattere per carattere: stessa lunghezza dell'originale, così le
// posizioni trovate valgono anche per il testo da disegnare. "/" diventa "\".
static void foldText(const wchar_t* s, size_t n, std::wstring& out) {
    out.resize(n);
    for (size_t i = 0; i < n; i++) {
        wchar_t c = s[i];
        if (c < 0x80) {
            if (c >= L'A' && c <= L'Z') c += 32;
            else if (c == L'/') c = L'\\';
        } else {
            wchar_t b[4];
            if (FoldStringW(MAP_COMPOSITE, &c, 1, b, 4) > 0) c = b[0]; // è → e + accento: resta la e
            CharLowerBuffW(&c, 1);
        }
        out[i] = c;
    }
}
static void foldText(const std::wstring& s, std::wstring& out) { foldText(s.c_str(), s.size(), out); }

static std::wstring dirKey(std::wstring p) {
    while (p.size() > 3 && p.back() == L'\\') p.pop_back();
    if (!p.empty()) CharLowerBuffW(&p[0], (DWORD)p.size());
    return p;
}

static int gotoDir(const std::wstring& path) {
    std::wstring k = dirKey(path);
    auto it = GT.dirIdx.find(k);
    if (it != GT.dirIdx.end()) return it->second;
    GoDir d;
    d.path = path;
    foldText(path, d.fold);
    GT.dirs.push_back(std::move(d));
    GT.dirIdx[k] = (int)GT.dirs.size() - 1;
    return (int)GT.dirs.size() - 1;
}

// Sottocartelle di una cartella aperta in una scheda: già in memoria
static int gotoDirFromListing(const Listing* l) {
    int di = gotoDir(l->path);
    GoDir& d = GT.dirs[di];
    if (d.from == l && d.fromGen == l->gen && d.ok) return di;
    d.names.clear(); d.folds.clear(); d.hidden.clear();
    for (const Entry& e : l->e) {
        if (!(e.attr & FILE_ATTRIBUTE_DIRECTORY)) continue;
        d.names.emplace_back(l->name(e), e.nameLen);
        d.folds.emplace_back();
        foldText(d.names.back(), d.folds.back());
        d.hidden.push_back((e.attr & FILE_ATTRIBUTE_HIDDEN) != 0);
        if ((int)d.names.size() >= gotolim::DIR_MAX) break;
    }
    d.ok = true;
    d.at = nowMs();
    d.from = l;
    d.fromGen = l->gen;
    return di;
}

// --- lettura delle sottocartelle (thread) -------------------------------------------------------
struct GoReq { HWND hwnd; std::vector<std::wstring> dirs; };
struct GoDirRes {
    std::wstring path;
    std::vector<std::wstring> names, folds;
    std::vector<uint8_t> hidden;
    bool ok = false;
    double ms = 0;                // dall'inizio della lettura
    int n = 0;                    // cartelle lette finora
};

static DWORD WINAPI gotoThread(void* p) {
    GoReq* r = (GoReq*)p;
    SetThreadErrorMode(SEM_FAILCRITICALERRORS, nullptr); // niente "inserire un disco"
    double t0 = nowMs();
    int n = 0;
    for (const std::wstring& d : r->dirs) {
        GoDirRes* res = new GoDirRes;
        res->path = d;
        res->ok = readSubfolders(d, res->names, res->hidden);
        if ((int)res->names.size() > gotolim::DIR_MAX) {
            res->names.resize(gotolim::DIR_MAX);
            res->hidden.resize(gotolim::DIR_MAX);
        }
        res->folds.resize(res->names.size());
        for (size_t i = 0; i < res->names.size(); i++) foldText(res->names[i], res->folds[i]);
        res->ms = nowMs() - t0;
        res->n = ++n;
        if (!PostMessageW(r->hwnd, WM_APP_GOTO, 0, (LPARAM)res)) delete res;
    }
    delete r;
    return 0;
}

static void gotoRead(const std::vector<int>& which) {
    if (which.empty()) return;
    GoReq* r = new GoReq{ A.hwnd, {} };
    double now = nowMs();
    for (int di : which) { r->dirs.push_back(GT.dirs[di].path); GT.dirs[di].askedAt = now; }
    // Prima le unità locali: una cartella di rete lenta non deve far aspettare le altre
    std::stable_partition(r->dirs.begin(), r->dirs.end(), [](const std::wstring& d) { return !slowPath(d); });
    HANDLE h = CreateThread(nullptr, 0, gotoThread, r, 0, nullptr);
    if (h) CloseHandle(h);
    else { for (int di : which) GT.dirs[di].askedAt = 0; delete r; }
}

static bool gotoNeedsRead(const GoDir& d) {
    double now = nowMs();
    if (d.from) return false; // la tiene aggiornata la scheda
    if (d.askedAt > 0 && now - d.askedAt < gotolim::ASK_AGAIN_MS) return false;
    return now - d.at > gotolim::STALE_MS;
}

static void gotoSearch(bool keepHot);

static void onGotoDir(GoDirRes* r) {
    A.gotoReadMs = r->ms;
    A.gotoReadDirs = r->n;
    int di = gotoDir(r->path);
    GoDir& d = GT.dirs[di];
    if (!d.from) { // una scheda può averla riempita nel frattempo, con dati più freschi
        d.names.swap(r->names);
        d.folds.swap(r->folds);
        d.hidden.swap(r->hidden);
        d.ok = r->ok;
        d.at = nowMs();
    }
    d.askedAt = 0;
    delete r;
    if (GT.open) { gotoSearch(true); A.dirty = true; }
}

// --- voci in memoria ---------------------------------------------------------------------------
static void gotoAddItem(std::unordered_map<std::wstring, int>& seen, const std::wstring& path,
                        const std::wstring& label, int bonus, int recent, bool drive) {
    std::wstring k = dirKey(path);
    auto it = seen.find(k);
    if (it != seen.end()) { // la stessa cartella da più fonti: i bonus si sommano
        GoItem& g = GT.items[it->second];
        g.bonus += bonus;
        if (recent >= 0) g.recent = recent;
        return;
    }
    GoItem g;
    g.path = path;
    g.label = label;
    g.name = folderTitle(path);
    g.recent = recent;
    g.drive = drive;
    g.bonus = bonus;
    foldText(g.label, g.labelFold);
    foldText(g.path, g.pathFold);
    seen[k] = (int)GT.items.size();
    GT.items.push_back(std::move(g));
}

// Bonus delle fonti, sommati al punteggio della corrispondenza (una lettera trovata vale 16)
enum { GB_FAV = 12, GB_TAB = 8, GB_DRIVE = 4, GB_RECENT = 4, GB_RECENT_RANK = 2, GB_CUR_CHILD = 3, GB_TAB_CHILD = 1,
       GB_HIDDEN = -6, GB_IN_PATH = -30 };

static std::wstring gotoTitleFor(const std::wstring& path) {
    int f = favIndex(path);
    if (f >= 0) return A.favs[f].label;
    for (const SideItem& d : A.drives) if (samePath(d.path, path)) return d.label;
    return folderTitle(path);
}

static void gotoBuild() {
    GT.items.clear();
    GT.active.clear();
    if ((int)GT.dirs.size() > gotolim::CACHE_MAX) { GT.dirs.clear(); GT.dirIdx.clear(); }
    std::unordered_map<std::wstring, int> seen;
    uint64_t now = fileTimeNow();
    for (const SideItem& f : A.favs) gotoAddItem(seen, f.path, f.label, GB_FAV, -1, false);
    for (const SideItem& d : A.drives) gotoAddItem(seen, d.path, d.label, GB_DRIVE, -1, d.drive); // percorsi di rete: icona di cartella
    // Schede: la cartella aperta (o in arrivo) di ognuna
    for (int i = 0; i < (int)A.tabs.size(); i++) {
        const Listing* l = i == A.tab ? A.cur : A.tabs[i].cur;
        std::wstring p = i == A.tab ? (A.loading ? A.loadPath : (l ? l->path : L""))
                                    : (A.tabs[i].pending ? A.tabs[i].pendingPath : (l ? l->path : L""));
        if (!p.empty()) gotoAddItem(seen, p, gotoTitleFor(p), GB_TAB, -1, false);
    }
    std::vector<std::pair<float, int>> ranked;
    for (int i = 0; i < (int)RC.v.size(); i++) {
        float r = recentRank(RC.v[i], now);
        ranked.push_back({ r, i });
        gotoAddItem(seen, RC.v[i].path, gotoTitleFor(RC.v[i].path), GB_RECENT + (int)(GB_RECENT_RANK * r), i, false);
    }
    gotoAddItem(seen, L"", L"Questo PC", 0, -1, false);
    A.gotoCands = (int)GT.items.size();

    // Una cartella riempita da una scheda che ora mostra altro (o è stata chiusa) torna a leggersi dal
    // disco: altrimenti le sue sottocartelle resterebbero quelle di allora. Il percorso conta oltre al
    // puntatore, perché una Listing nuova può nascere allo stesso indirizzo di una liberata.
    for (GoDir& d : GT.dirs) {
        if (!d.from) continue;
        bool live = false;
        for (int i = 0; i < (int)A.tabs.size() && !live; i++) {
            const Listing* l = i == A.tab ? A.cur : A.tabs[i].cur;
            live = l == d.from && samePath(l->path, d.path);
        }
        if (!live) d.from = nullptr;
    }

    // Sottocartelle: delle schede (in memoria), dei preferiti e delle recenti più usate (thread)
    std::vector<int> toRead;
    auto activate = [&](int di, int bonus) {
        for (const GoActive& a : GT.active) if (a.dir == di) return;
        GT.active.push_back({ di, bonus });
        if (gotoNeedsRead(GT.dirs[di])) toRead.push_back(di);
    };
    for (int i = 0; i < (int)A.tabs.size(); i++) {
        const Listing* l = i == A.tab ? A.cur : A.tabs[i].cur;
        if (l && !l->path.empty() && !l->isDrives) activate(gotoDirFromListing(l), i == A.tab ? GB_CUR_CHILD : GB_TAB_CHILD);
    }
    for (const SideItem& f : A.favs) activate(gotoDir(f.path), 0);
    std::sort(ranked.begin(), ranked.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
    for (int i = 0; i < (int)ranked.size() && i < gotolim::READ_RECENTS; i++) activate(gotoDir(RC.v[ranked[i].second].path), 0);
    for (const GoActive& a : GT.active) A.gotoCands += (int)GT.dirs[a.dir].names.size();
    gotoRead(toRead);
}

// --- ricerca -----------------------------------------------------------------------------------
static inline bool gotoWordSep(wchar_t c) {
    return c == L' ' || c == L'\\' || c == L'-' || c == L'_' || c == L'.' || c == L'(' || c == L'[';
}

// Punteggio di q in t (entrambi piegati, lunghi m e n); o: testo originale per le maiuscole
// interne (può mancare). -1 se le lettere non ci sono in ordine. marks: posizioni trovate.
// Come fzf (v1): la prima corrispondenza completa da sinistra, poi la finestra più corta che
// finisce lì, cercando all'indietro; dentro la finestra le lettere si prendono da sinistra.
static int fuzzyScore(const wchar_t* t, int n, const wchar_t* o, const wchar_t* q, int m, uint8_t* marks) {
    if (m == 0) return 0;
    if (m > n) return -1;
    int j = 0, end = -1;
    for (int i = 0; i < n; i++) if (t[i] == q[j] && ++j == m) { end = i; break; }
    if (end < 0) return -1;
    int start = end;
    j = m - 1;
    for (int i = end; i >= 0; i--) if (t[i] == q[j] && --j < 0) { start = i; break; }
    int score = 0, prev = -2;
    j = 0;
    for (int i = start; i <= end && j < m; i++) {
        if (t[i] != q[j]) continue;
        int s = 16;
        if (i == 0) s += 10;                                  // inizio del testo
        else if (gotoWordSep(t[i - 1])) s += 8;               // inizio di una parola
        else if (o && o[i] >= L'A' && o[i] <= L'Z' && o[i - 1] >= L'a' && o[i - 1] <= L'z') s += 7; // maiuscola interna
        if (prev == i - 1) s += 8;                            // consecutiva alla precedente
        else if (j > 0) s -= std::min(i - prev - 1, 10) + 2;  // buco
        if (marks) marks[i] = 1;
        score += s;
        prev = i;
        j++;
    }
    score -= std::min(start, 15) / 3;                        // comincia lontano dall'inizio
    if (start == 0 && end == m - 1) score += n == m ? 40 : 20; // uguale, o prefisso
    score -= std::min(n - m, 40) / 4;                         // a parità, il nome più corto
    return score;
}

// Le lettere di q a partire da j ci sono ancora, in ordine, in t?
static bool fuzzyRest(const std::wstring& t, const std::wstring& q, int j) {
    for (size_t i = 0; i < t.size() && j < (int)q.size(); i++) if (t[i] == q[j]) j++;
    return j == (int)q.size();
}

static std::wstring gotoResPath(const GoRes& r) {
    if (r.dir == -2) return GT.typed;
    if (r.dir == -1) return GT.items[r.idx].path;
    return joinPath(GT.dirs[r.dir].path, GT.dirs[r.dir].names[r.idx].c_str());
}

// Percorso completo piegato di una sottocartella, in buf
static void gotoChildPath(const GoDir& d, int c, std::wstring& buf) {
    buf = d.fold;
    if (!buf.empty() && buf.back() != L'\\') buf += L'\\';
    buf += d.folds[c];
}

// I migliori `keep` risultati, senza la stessa cartella due volte
static void gotoTop(size_t keep) {
    std::vector<GoRes>& R = GT.res;
    auto better = [](const GoRes& a, const GoRes& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.dir != b.dir ? a.dir < b.dir : a.idx < b.idx; // ordine stabile tra un tasto e l'altro
    };
    size_t k = std::min(R.size(), keep * 2);
    std::partial_sort(R.begin(), R.begin() + k, R.end(), better);
    R.resize(k);
    std::vector<GoRes> out;
    std::vector<std::wstring> seen;
    for (const GoRes& r : R) {
        std::wstring p = gotoResPath(r);
        bool dup = false;
        for (const std::wstring& s : seen) if (samePath(s, p)) { dup = true; break; }
        if (dup) continue;
        seen.push_back(std::move(p));
        out.push_back(r);
        if (out.size() >= keep) break;
    }
    R.swap(out);
}

// Campo vuoto: le cartelle recenti, dalla più "di casa", esclusa quella aperta
static void gotoSearchEmpty() {
    uint64_t now = fileTimeNow();
    std::wstring cur = A.cur ? A.cur->path : L"\x1";
    for (int i = 0; i < (int)GT.items.size(); i++) {
        const GoItem& g = GT.items[i];
        if (g.recent < 0 || samePath(g.path, cur)) continue;
        GT.res.push_back({ -1, i, (int)(recentRank(RC.v[g.recent], now) * 1000), 0 });
    }
    if (GT.res.empty()) // prima volta: preferiti e unità, nel loro ordine
        for (int i = 0; i < (int)GT.items.size(); i++)
            if (!GT.items[i].path.empty() && !samePath(GT.items[i].path, cur)) GT.res.push_back({ -1, i, -i, 0 });
    gotoTop(gotolim::RESULTS);
}

static void gotoSearchFuzzy(const std::wstring& text) {
    foldText(text, GT.q);
    GT.pathQuery = GT.q.find(L'\\') != std::wstring::npos;
    const wchar_t* q = GT.q.c_str();
    int m = (int)GT.q.size();
    for (int i = 0; i < (int)GT.items.size(); i++) {
        const GoItem& g = GT.items[i];
        int s = GT.pathQuery ? -1 : fuzzyScore(g.labelFold.c_str(), (int)g.labelFold.size(), g.label.c_str(), q, m, nullptr);
        uint8_t where = 0;
        if (s < 0) {
            s = fuzzyScore(g.pathFold.c_str(), (int)g.pathFold.size(), g.path.c_str(), q, m, nullptr);
            where = 1;
            if (s >= 0 && !GT.pathQuery) s += GB_IN_PATH;
        }
        if (s >= 0) GT.res.push_back({ -1, i, s + g.bonus, where });
    }
    std::wstring buf;
    for (const GoActive& a : GT.active) {
        const GoDir& d = GT.dirs[a.dir];
        // Per il ripiego sul percorso: quante lettere trova già il percorso della cartella
        int jd = 0;
        for (size_t i = 0; i < d.fold.size() && jd < m; i++) if (d.fold[i] == q[jd]) jd++;
        if (jd < m && q[jd] == L'\\' && !d.fold.empty() && d.fold.back() != L'\\') jd++;
        for (int c = 0; c < (int)d.names.size(); c++) {
            const std::wstring& f = d.folds[c];
            int s = GT.pathQuery ? -1 : fuzzyScore(f.c_str(), (int)f.size(), d.names[c].c_str(), q, m, nullptr);
            uint8_t where = 0;
            if (s < 0) {
                if (!fuzzyRest(f, GT.q, jd)) continue;
                gotoChildPath(d, c, buf);
                s = fuzzyScore(buf.c_str(), (int)buf.size(), nullptr, q, m, nullptr);
                if (s < 0) continue;
                where = 1;
                if (!GT.pathQuery) s += GB_IN_PATH;
            }
            GT.res.push_back({ a.dir, c, s + a.bonus + (d.hidden[c] ? GB_HIDDEN : 0), where });
        }
    }
    gotoTop(gotolim::RESULTS);
}

// Percorso scritto: le sottocartelle del livello scritto, filtrate da quello che segue l'ultima "\"
static void gotoSearchPath(const std::wstring& text) {
    bool endsSep = text.back() == L'\\' || text.back() == L'/';
    std::wstring p = normalizePath(text), dir, rest;
    if (endsSep || (p.size() == 3 && p[1] == L':')) dir = p;
    else {
        size_t k = p.find_last_of(L'\\');
        if (k != std::wstring::npos) {
            dir = p.substr(0, k);
            if (dir.size() == 2 && dir[1] == L':') dir += L'\\';
            rest = p.substr(k + 1);
        }
    }
    GT.typed = rest.empty() && !dir.empty() ? dir : p;
    // "\\server" da solo non si può elencare
    bool canList = dir.size() >= 3 && !(dir[0] == L'\\' && dir.find(L'\\', 2) == std::wstring::npos);
    if (!canList) { GT.res.push_back({ -2, 0, 0, 0 }); return; }
    if (A.cur && samePath(A.cur->path, dir) && !A.cur->isDrives) GT.absDir = gotoDirFromListing(A.cur);
    else GT.absDir = gotoDir(dir);
    GoDir& d = GT.dirs[GT.absDir];
    if (gotoNeedsRead(d)) {
        gotoRead({ GT.absDir });
        SetTimer(A.hwnd, 9, ui::LOADING_AFTER, nullptr); // per mostrare "caricamento…" se è lenta
    }
    foldText(rest, GT.q);
    int m = (int)GT.q.size();
    if (!m) { // la cartella stessa, poi tutte le sottocartelle nell'ordine della lista
        GT.res.push_back({ -2, 0, 0, 0 });
        for (int c = 0; c < (int)d.names.size(); c++) GT.res.push_back({ GT.absDir, c, -c, 0 });
        return;
    }
    for (int c = 0; c < (int)d.names.size(); c++) {
        int s = fuzzyScore(d.folds[c].c_str(), (int)d.folds[c].size(), d.names[c].c_str(), GT.q.c_str(), m, nullptr);
        if (s >= 0) GT.res.push_back({ GT.absDir, c, s + (d.hidden[c] ? GB_HIDDEN : 0), 0 });
    }
    gotoTop(gotolim::RESULTS);
    if (GT.res.empty()) GT.res.push_back({ -2, 0, 0, 0 }); // niente: si può comunque provare ad aprirlo
}

static bool gotoIsPathText(const std::wstring& t) {
    if (t.size() >= 2 && t[1] == L':' && ((t[0] | 32) >= L'a' && (t[0] | 32) <= L'z')) return true;
    return (t.size() >= 2 && t[0] == L'\\' && t[1] == L'\\') || (!t.empty() && t[0] == L'%');
}

static const wchar_t* gotoNote(); // riga senza voci: "Nessuna cartella", "caricamento…"

static void gotoLayout() {
    float W = (float)G.w, H = (float)G.h, gap = px(ui::GAP_M);
    GT.ih = A.rowH;
    GT.w = floorf(std::min(px(ui::GOTO_W), W - px(ui::GAP_XL) * 2));
    GT.x = floorf((W - GT.w) * 0.5f);
    GT.y = A.pathY + A.topH + px(ui::GAP_S);
    GT.fieldY = GT.y + gap;
    GT.listY = GT.fieldY + A.rowH + gap;
    int n = (int)GT.res.size();
    int note = gotoNote() ? 1 : 0;
    int fit = (int)((H - px(ui::GAP_S) - px(ui::MENU_PAD_Y) - GT.listY) / GT.ih);
    int rows = std::max(1, std::min(ui::GOTO_ROWS, fit));
    GT.visible = std::max(0, std::min(n, rows - note));
    GT.h = GT.listY - GT.y + (GT.visible + note) * GT.ih + px(ui::MENU_PAD_Y);
    GT.first = std::max(0, std::min(GT.first, n - GT.visible));
}

static void gotoEnsureVisible() {
    gotoLayout();
    if (GT.hot < GT.first) GT.first = GT.hot;
    if (GT.hot >= GT.first + GT.visible) GT.first = GT.hot - GT.visible + 1;
    GT.first = std::max(0, GT.first);
}

// keepHot: stessa ricerca con voci nuove (sottocartelle arrivate dal thread): resta attiva la
// stessa voce, se c'è ancora. Altrimenti si riparte dalla prima.
static void gotoSearch(bool keepHot) {
    double t0 = nowMs();
    std::wstring hotPath;
    bool hadHot = keepHot && GT.hot >= 0 && GT.hot < (int)GT.res.size();
    if (hadHot) hotPath = gotoResPath(GT.res[GT.hot]);
    GT.res.clear();
    GT.q.clear();
    GT.pathQuery = false;
    GT.typed.clear();
    GT.absDir = -1;
    std::wstring t = GT.field.text;
    while (!t.empty() && t.back() == L' ') t.pop_back();
    size_t a = t.find_first_not_of(L' ');
    t = a == std::wstring::npos ? L"" : t.substr(a);
    if (t.empty()) gotoSearchEmpty();
    else if (gotoIsPathText(t)) gotoSearchPath(t);
    else gotoSearchFuzzy(t);
    int first = GT.first;
    GT.hot = 0;
    GT.first = 0;
    if (hadHot) {
        for (int i = 0; i < (int)GT.res.size(); i++)
            if (samePath(gotoResPath(GT.res[i]), hotPath)) { GT.hot = i; GT.first = first; break; }
    }
    gotoEnsureVisible();
    A.gotoSearchMs = nowMs() - t0;
    if (A.gotoSearchMs > A.gotoSearchMax) A.gotoSearchMax = A.gotoSearchMs;
    if (A.gotoSearchMs > ui::LOG_GOTO) hitchLog("vai a: ricerca lenta", (unsigned)GT.res.size(), A.gotoSearchMs);
    A.dirty = true;
}

static const wchar_t* gotoNote() {
    if (GT.absDir >= 0) {
        const GoDir& d = GT.dirs[GT.absDir];
        if (!d.ok && d.askedAt > 0) return nowMs() - d.askedAt > ui::LOADING_AFTER ? L"caricamento\x2026" : nullptr;
        if (!d.ok && d.at > 0 && GT.res.size() <= 1) return L"Impossibile leggere la cartella";
    }
    return GT.res.empty() ? L"Nessuna cartella" : nullptr;
}

// --- apertura, chiusura, comandi -----------------------------------------------------------------
static void gotoClose() {
    if (!GT.open) return;
    GT.open = false;
    GT.closedAt = nowMs();
    tipHide();
    A.dirty = true;
}

// Apre il pannello con questo testo, tutto selezionato: si scrive sopra o si incolla. Se è già
// aperto cambia solo il testo.
static void gotoStart(const std::wstring& text) {
    double t0 = nowMs();
    if (!GT.open) {
        menuClose();
        optionsClose();
        tipHide();
        if (A.renaming) endRename(true); // come un clic fuori dalla casella (dal menu ≡)
        GT.open = true;
        GT.openedAt = nowMs();
        gotoBuild();
    }
    GT.field.set(text, 0, (int)text.size());
    GT.first = 0;
    gotoSearch(false);
    A.gotoOpenMs = nowMs() - t0;
    A.gotoSearchMax = A.gotoSearchMs;
    A.hover = -1;
    A.dirty = true;
}

static void gotoOpen() {
    if (GT.open) { gotoClose(); return; } // la stessa scorciatoia lo richiude
    gotoStart(L"");
}

// Dalla cartella aperta (clic sul percorso, Ctrl+L, F4): il suo percorso con "\" in fondo, così si
// vedono subito le sue sottocartelle, prese dalla lista senza toccare il disco. Questo PC: campo vuoto.
static void gotoOpenHere() {
    std::wstring p = A.cur && !A.cur->isDrives ? A.cur->path : L"";
    if (!p.empty() && p.back() != L'\\') p += L'\\';
    gotoStart(p);
}

static void gotoExec(int i, bool newTab) {
    if (i < 0 || i >= (int)GT.res.size()) return;
    std::wstring p = gotoResPath(GT.res[i]);
    gotoClose();
    if (newTab) tabNew(p, false);
    else navigate(p, 1, L"");
}

// Tab: la voce diventa il testo, con "\" in fondo: si vedono le sue sottocartelle
static void gotoComplete() {
    if (GT.hot < 0 || GT.hot >= (int)GT.res.size()) return;
    std::wstring p = gotoResPath(GT.res[GT.hot]);
    if (p.empty()) return; // Questo PC
    if (p.back() != L'\\') p += L'\\';
    if (p == GT.field.text) return;
    GT.field.set(p, (int)p.size(), (int)p.size());
    gotoSearch(false);
}

// --- input: true se l'evento è del pannello ----------------------------------------------------
static bool gotoKey(WPARAM vk, bool ctrl, bool shift) {
    if (!GT.open) return false;
    int n = (int)GT.res.size();
    switch (vk) {
    case VK_F12: return false; // il pannello dei tempi resta raggiungibile
    case VK_ESCAPE: // prima svuota la ricerca, poi chiude
        if (!GT.field.text.empty()) { GT.field.set(L"", 0, 0); gotoSearch(false); }
        else gotoClose();
        break;
    case VK_RETURN: gotoExec(GT.hot, ctrl); break;
    case VK_TAB: gotoComplete(); break;
    case VK_UP: case VK_DOWN:
        if (n) { GT.hot = (GT.hot + (vk == VK_DOWN ? 1 : -1) + n) % n; gotoEnsureVisible(); }
        break;
    case VK_PRIOR: case VK_NEXT:
        if (n) {
            int step = std::max(1, GT.visible - 1);
            GT.hot = std::max(0, std::min(n - 1, GT.hot + (vk == VK_NEXT ? step : -step)));
            gotoEnsureVisible();
        }
        break;
    default:
        // Le scorciatoie di "Vai a" (dalla tabella): la sua lo richiude, quella del percorso riparte
        // dalla cartella aperta
        if (int cmd = shortcutCmd((UINT)vk, (ctrl ? KC : 0) | (shift ? KS : 0)); cmd == M_GOTO || cmd == M_EDIT_PATH) {
            if (cmd == M_GOTO) gotoClose();
            else gotoOpenHere();
            break;
        }
        {
            std::wstring before = GT.field.text;
            GT.field.key(A.hwnd, vk, ctrl, shift);
            if (GT.field.text != before) gotoSearch(false);
        }
        break;
    }
    A.dirty = true;
    return true;
}

static bool gotoChar(wchar_t c) {
    if (!GT.open) return false;
    GT.field.insert(std::wstring(1, c));
    gotoSearch(false);
    return true;
}

static bool gotoInside(int x, int y) { return x >= GT.x && x < GT.x + GT.w && y >= GT.y && y < GT.y + GT.h; }

static int gotoRowAt(int x, int y) {
    if (!GT.open || x < GT.x || x >= GT.x + GT.w || y < GT.listY) return -1;
    int i = (int)((y - GT.listY) / GT.ih);
    if (i >= GT.visible) return -1;
    i += GT.first;
    return i < (int)GT.res.size() ? i : -1;
}

static bool gotoMouseMove(int x, int y) {
    if (!GT.open) return false;
    int r = gotoRowAt(x, y);
    if (r >= 0 && r != GT.hot) { GT.hot = r; A.dirty = true; }
    return true;
}

// Clic (middle: clic centrale). Fuori dal pannello chiude e basta, come i menu.
static bool gotoMouseDown(int x, int y, bool middle, bool ctrl) {
    if (!GT.open) return false;
    if (!gotoInside(x, y)) { gotoClose(); return true; }
    int r = gotoRowAt(x, y);
    if (r >= 0) gotoExec(r, middle || ctrl);
    return true;
}

// Il secondo clic di un doppio clic arriva quando il pannello si è già chiuso: non deve aprire
// la voce che nella lista, sotto, si trova in quel punto. Un doppio clic sul percorso vuoto l'ha
// appena aperto con il primo clic: il secondo non lo richiude.
static bool gotoDblClk(int x, int y, bool ctrl) {
    if (GT.open && y >= A.pathY && y < A.pathY + A.topH && nowMs() - GT.openedAt < GetDoubleClickTime()) return true;
    if (GT.open) return gotoMouseDown(x, y, false, ctrl);
    return nowMs() - GT.closedAt < GetDoubleClickTime();
}

static bool gotoWheel(int x, int y, int lines) {
    if (!GT.open) return false;
    if (gotoInside(x, y)) {
        int n = (int)GT.res.size();
        GT.first = std::max(0, std::min(n - GT.visible, GT.first - lines * ui::WHEEL_LINES));
        int r = gotoRowAt(x, y);
        if (r >= 0) GT.hot = r;
        A.dirty = true;
    }
    return true;
}

// --- disegno -----------------------------------------------------------------------------------
struct GoRow {
    std::wstring label, sub;      // nome e percorso (in C_DIM)
    std::vector<uint8_t> markL, markS;
    int icon = 1;                 // 1 cartella, 2 unità
    const std::wstring* iconPath = nullptr;
    bool dim = false;
};

// Testo di una voce e lettere trovate. Le posizioni trovate nel percorso completo (where 1) si
// riportano sul nome e sul percorso mostrati.
static void gotoRow(const GoRes& r, GoRow& o) {
    int m = (int)GT.q.size();
    const wchar_t* q = GT.q.c_str();
    std::wstring full, fullFold;
    size_t nameStart = 0, subLen = 0;
    bool subIsFull = false, labelIsName = true;
    const std::wstring* labelFold = nullptr;
    if (r.dir == -2) {
        o.label = GT.typed;
        o.markL.assign(o.label.size() + 1, 0);
        return;
    }
    if (r.dir == -1) {
        const GoItem& g = GT.items[r.idx];
        o.label = g.label;
        o.icon = g.drive ? 2 : 1;
        o.iconPath = &g.path;
        labelFold = &g.labelFold;
        labelIsName = g.label == g.name;
        full = g.path;
        fullFold = g.pathFold;
        nameStart = full.size() >= g.name.size() ? full.size() - g.name.size() : 0;
        if (labelIsName && !full.empty()) {
            subLen = nameStart; // la cartella che la contiene, senza "\" finale (tranne "C:\")
            if (subLen > 3 && full[subLen - 1] == L'\\') subLen--;
            o.sub = full.substr(0, subLen);
        } else {
            o.sub = full; // il nome mostrato non è quello della cartella: percorso intero
            subIsFull = true;
        }
    } else {
        const GoDir& d = GT.dirs[r.dir];
        o.label = d.names[r.idx];
        o.dim = d.hidden[r.idx] != 0;
        labelFold = &d.folds[r.idx];
        o.sub = d.path;
        subLen = d.path.size();
        if (r.where == 1) {
            gotoChildPath(d, r.idx, fullFold);
            nameStart = fullFold.size() - o.label.size();
        }
    }
    o.markL.assign(o.label.size() + 1, 0);
    o.markS.assign(o.sub.size() + 1, 0);
    if (!m) return;
    if (r.where == 0) {
        fuzzyScore(labelFold->c_str(), (int)labelFold->size(), o.label.c_str(), q, m, o.markL.data());
        return;
    }
    std::vector<uint8_t> mk(fullFold.size() + 1, 0);
    fuzzyScore(fullFold.c_str(), (int)fullFold.size(), nullptr, q, m, mk.data());
    for (size_t p = 0; p < fullFold.size(); p++) {
        if (!mk[p]) continue;
        if (subIsFull) { if (p < o.sub.size()) o.markS[p] = 1; }
        else if (p < subLen) o.markS[p] = 1;
        else if (p >= nameStart && labelIsName && p - nameStart < o.label.size()) o.markL[p - nameStart] = 1;
    }
}

static void drawGoto() {
    if (!GT.open) return;
    gotoLayout();
    PanelEnter e = panelEnter(GT.openedAt, ui::DUR_MENU);
    float k = e.k, dy = e.dy, gap = px(ui::GAP_M), pad = px(ui::MENU_PAD_X);
    drawPanel(GT.x, GT.y - dy, GT.w, GT.h, px(ui::R_M), k);
    // Casella di testo
    float fx = GT.x + gap, fw = GT.w - gap * 2, fh = A.rowH, fy = GT.fieldY - dy;
    drawField(GT.field, fx, fy, fw, fh, A.scale, L"Vai a una cartella\x2026", k);
    // Voci
    int n = (int)GT.res.size();
    float ih = GT.ih, textOff = floorf((ih - F.lineH) * 0.5f);
    float iconW = (float)I.size + px(ui::GAP_M);
    bool scroll = GT.visible < n;
    float right = GT.x + GT.w - pad - (scroll ? px(ui::GAP_S) : 0);
    float y = GT.listY - dy;
    GoRow row;
    for (int i = GT.first; i < GT.first + GT.visible && i < n; i++, y += ih) {
        row = GoRow();
        gotoRow(GT.res[i], row);
        float tx = drawPanelItem(GT.x, GT.w, y, ih, pad, i == GT.hot, row.icon, row.iconPath, row.dim, iconW, k);
        // Il nome prende quello che gli serve, fino a GOTO_NAME_MAX dello spazio se c'è il percorso
        float nameMax = row.sub.empty() ? right : tx + floorf((right - tx) * ui::GOTO_NAME_MAX);
        uint32_t lc = row.dim ? C_FAINT : C_TEXT, sc = row.dim ? C_FAINT : C_DIM;
        float ex = drawTextMarked(tx, y + textOff, row.label.c_str(), (int)row.label.size(), fadeA(lc, k), nameMax,
                                  row.markL.data(), fadeA(C_ACCENT, k));
        if (!row.sub.empty() && ex + gap < right)
            drawTextMarked(ex + gap, y + textOff, row.sub.c_str(), (int)row.sub.size(), fadeA(sc, k), right,
                           row.markS.data(), fadeA(C_ACCENT, k));
    }
    if (const wchar_t* note = gotoNote())
        drawText(GT.x + pad, y + textOff, note, (int)wcslen(note), fadeA(C_FAINT, k), right);
    drawPanelScroll(GT.x + GT.w, GT.listY - dy, GT.visible * ih, GT.first, GT.visible, n, k);
}

// --- tooltip -------------------------------------------------------------------------------------
static bool gotoTipTarget(int x, int y, int& index) {
    index = gotoRowAt(x, y);
    return index >= 0;
}

static bool gotoTipBuild(int index) {
    if (!GT.open || index < 0 || index >= (int)GT.res.size()) return false;
    const GoRes& r = GT.res[index];
    std::wstring p = gotoResPath(r);
    GoRow row;
    gotoRow(r, row);
    T.title = r.dir == -2 ? p : row.label;
    tipRow(L"Percorso", p.empty() ? L"Questo PC" : p);
    if (r.dir == -1 && GT.items[r.idx].recent >= 0) {
        const Recent& rc = RC.v[GT.items[r.idx].recent];
        tipRow(L"Aperta", (rc.visits == 1 ? std::wstring(L"una volta") : std::to_wstring(rc.visits) + L" volte") +
                          L", l'ultima " + dateLine(rc.last));
    }
    if (favIndex(p) >= 0) tipRow(L"", L"Tra i preferiti");
    tipRow(L"Apri", shortcutFor(M_OPEN)); // i tasti da kShortcuts, come nei menu
    tipRow(L"Nuova scheda", shortcutsOf(M_GOTO_NEWTAB));
    tipRow(L"Sottocartelle", shortcutsOf(M_GOTO_ENTER));
    return true;
}
