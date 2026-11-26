// [z]f — barra laterale: Preferiti, Questo PC e le sue unità.
//
// Tutto ciò che può essere lento (nomi localizzati delle cartelle, etichette e spazio libero delle
// unità: un'unità di rete o un lettore ottico possono bloccare per secondi) si legge su un thread
// a parte, avviato dopo il primo frame (caricare shell32/ole32 prima rallenterebbe l'avvio).
// All'avvio, insieme alla prima lettura della cartella, un thread leggero (solo kernel32) legge
// il file dei preferiti e le lettere delle unità: decimi di millisecondo, così il primo frame
// mostra già la barra completa. Il thread UI non tocca il disco nemmeno qui.
//
// I preferiti si salvano in %APPDATA%\z-f\preferiti.txt, una riga per cartella (UTF-8):
// il percorso, poi facoltativi un tab e il nome mostrato (così all'avvio non serve la shell).
// Si può modificare a mano.

struct SideLoad {
    HWND hwnd;
    uint64_t gen;
    bool readFile;                 // false: usa favs così come sono (dopo una modifica)
    bool fast = false;             // lettura veloce dell'avvio
    bool fileFound = false;
    std::vector<std::wstring> settings; // righe di impostazioni.txt (solo lettura veloce)
    std::vector<SideItem> favs, drives;
    std::vector<std::wstring> recents;  // righe di recenti.txt (solo lettura veloce)
    std::vector<std::wstring> undo;     // righe di annulla.txt (solo lettura veloce; vuoto se l'opzione è spenta)
    std::vector<std::wstring> views;    // righe di viste.txt (solo lettura veloce)
    bool startTabs = false;             // lettura veloce: cercare le schede da riaprire (WM_APP_START)
    DWORD restartPid = 0;               // …in riavvio-<pid>.txt (riavvio dopo un aggiornamento)
};

static std::wstring knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p)) && p) r = p;
    CoTaskMemFree(p);
    return r;
}

static bool readFavFile(std::vector<SideItem>& out) {
    std::vector<std::wstring> lines;
    if (!readDataLines(L"preferiti.txt", lines)) return false;
    for (const std::wstring& w : lines) {
        SideItem it;
        size_t tab = w.find(L'\t');
        it.path = w.substr(0, tab);
        if (tab != std::wstring::npos) it.label = w.substr(tab + 1);
        if (it.label.empty()) it.label = folderTitle(it.path);
        if (!it.path.empty()) out.push_back(std::move(it));
    }
    return true;
}

// mpr.dll (unità di rete) si carica solo quando serve: non è tra le DLL dell'avvio
static FARPROC mprProc(const char* name) {
    static HMODULE m = LoadLibraryExW(L"mpr.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return m ? GetProcAddress(m, name) : nullptr;
}

// \\server\condivisione di un'unità di rete (anche se ora non è raggiungibile), "" altrimenti
static std::wstring netRemote(const std::wstring& root) {
    typedef DWORD(WINAPI * PFN_GetConnection)(LPCWSTR, LPWSTR, LPDWORD);
    auto get = (PFN_GetConnection)mprProc("WNetGetConnectionW");
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD n = MAX_PATH * 2;
    if (!get) return L"";
    DWORD r = get(root.substr(0, 2).c_str(), buf, &n);
    return r == NO_ERROR || r == ERROR_CONNECTION_UNAVAIL ? buf : L"";
}

// Nome di un'unità di rete: quello di Explorer ("Condivisione (\\server) (G:)", o quello dato
// dall'utente) senza il server, che sta nel tooltip. L'etichetta del volume non va: è quella del
// disco sul server, spesso uguale per tutte le condivisioni.
static std::wstring netLabel(const SideItem& it) {
    std::wstring tail = L" (" + it.path.substr(0, 2) + L")", n;
    SHFILEINFOW sfi = {};
    if (SHGetFileInfoW(it.path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME)) n = sfi.szDisplayName;
    if (n.size() > tail.size() && n.compare(n.size() - tail.size(), tail.size(), tail) == 0) n.resize(n.size() - tail.size());
    else n.clear();
    size_t k = n.find(L" (\\\\");
    if (k != std::wstring::npos) n.resize(k);
    if (n.empty() && !it.unc.empty()) n = it.unc.substr(it.unc.find_last_of(L'\\') + 1); // nome della condivisione
    return n.empty() ? L"" : n + tail;
}

// Percorsi di rete di Windows in fondo alle unità (lettura veloce: senza la rete)
static void netPlacesAdd(std::vector<SideItem>& out) {
    for (NetPlace& np : netPlacesRead()) {
        SideItem it;
        it.place = true;
        it.label = std::move(np.name);
        it.path = it.unc = std::move(np.path);
        out.push_back(std::move(it));
    }
}

// Etichetta, file system e spazio del volume che contiene root (con la barra finale). Può
// aspettare: un'unità ottica o di rete, una condivisione che non risponde. Solo sul thread.
static std::wstring volumeRead(SideItem& it, const std::wstring& root) {
    wchar_t label[MAX_PATH + 1] = {}, fs[32] = {};
    // niente "inserire un disco"; per questo thread solo (SetErrorMode vale per tutto il processo)
    DWORD oldMode = 0;
    SetThreadErrorMode(SEM_FAILCRITICALERRORS, &oldMode);
    bool ready = GetVolumeInformationW(root.c_str(), label, MAX_PATH + 1, nullptr, nullptr, nullptr, fs, 32) != 0;
    it.fs = fs;
    ULARGE_INTEGER fr, tot;
    if (ready && GetDiskFreeSpaceExW(root.c_str(), &fr, &tot, nullptr)) { it.free = fr.QuadPart; it.total = tot.QuadPart; }
    SetThreadErrorMode(oldMode, nullptr);
    return label;
}

// Avvio: solo operazioni economiche e senza shell (file piccolo, lettere delle unità, percorsi di rete)
static DWORD WINAPI sideFastThread(void* p) {
    SideLoad* L = (SideLoad*)p;
    migrateLegacyData();
    readDataLines(L"impostazioni.txt", L->settings);
    // Le schede da riaprire, subito: la prima cartella le aspetta (main.cpp), il resto no
    if (L->startTabs) {
        StartTabs* s = new StartTabs;
        if (L->restartPid) {
            std::wstring name = L"riavvio-" + std::to_wstring(L->restartPid) + L".txt";
            s->restart = true;
            readDataLines(name.c_str(), s->lines);
            DeleteFileW(zfDataPath(name.c_str()).c_str());
        } else {
            for (const std::wstring& l : L->settings)
                if (l == L"riapri_schede=1") { readDataLines(L"schede.txt", s->lines); break; }
        }
        if (!PostMessageW(L->hwnd, WM_APP_START, 0, (LPARAM)s)) delete s;
    }
    L->fileFound = readFavFile(L->favs);
    readDataLines(L"recenti.txt", L->recents);
    readDataLines(L"annulla.txt", L->undo);
    readDataLines(L"viste.txt", L->views);
    DWORD mask = GetLogicalDrives();
    for (int d = 0; d < 26; d++) {
        if (!(mask & (1u << d))) continue;
        SideItem it;
        it.drive = true;
        it.path = std::wstring(1, (wchar_t)(L'A' + d)) + L":\\";
        it.driveType = GetDriveTypeW(it.path.c_str());
        it.label = std::wstring(driveKind(it.driveType)) + L" (" + it.path.substr(0, 2) + L")";
        L->drives.push_back(std::move(it));
    }
    netPlacesAdd(L->drives);
    if (!PostMessageW(L->hwnd, WM_APP_SIDEBAR, 0, (LPARAM)L)) delete L;
    return 0;
}
static void sideStartFast() {
    settingsMarkStart();
    SideLoad* L = new SideLoad{ A.hwnd, 0, true, true, false, {}, {}, {}, {}, {}, {} };
    // La finestra principale cerca le schede da riaprire; dopo un aggiornamento ogni finestra le sue
    L->startTabs = A.mainInstance || A.restartPid;
    L->restartPid = A.restartPid;
    HANDLE h = CreateThread(nullptr, 0, sideFastThread, L, 0, nullptr);
    if (h) CloseHandle(h);
    else {
        delete L;
        A.sideFastDone = true;
        recentLoaded({});
        viewsLoaded({});
        if (A.startDeferred) { A.startDeferred = false; navigate(A.startDefault, 0, L""); }
    }
}

static DWORD WINAPI sideThread(void* p) {
    SideLoad* L = (SideLoad*)p;
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (L->readFile && !readFavFile(L->favs)) { // primo avvio: cartelle di sistema
        const KNOWNFOLDERID* ids[] = { &FOLDERID_Desktop, &FOLDERID_Downloads, &FOLDERID_Documents,
                                       &FOLDERID_Pictures, &FOLDERID_Profile };
        for (auto id : ids) {
            SideItem it;
            it.path = knownFolder(*id);
            if (!it.path.empty()) L->favs.push_back(std::move(it));
        }
    }
    for (SideItem& it : L->favs) {
        SHFILEINFOW sfi = {};
        // Nome mostrato da Windows ("Documenti" invece di "Documents")
        // (non per uno zip: la shell lo aprirebbe con zipfldr, lenta sugli archivi grandi)
        if (!zipPathLike(it.path) && SHGetFileInfoW(it.path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME) && sfi.szDisplayName[0])
            it.label = sfi.szDisplayName;
        else if (it.label.empty())
            it.label = folderTitle(it.path);
    }
    DWORD mask = GetLogicalDrives();
    for (int d = 0; d < 26; d++) {
        if (!(mask & (1u << d))) continue;
        SideItem it;
        it.drive = true;
        it.path = std::wstring(1, (wchar_t)(L'A' + d)) + L":\\";
        it.driveType = GetDriveTypeW(it.path.c_str());
        std::wstring label = volumeRead(it, it.path);
        it.label = (label.empty() ? std::wstring(driveKind(it.driveType)) : label) + L" (" + it.path.substr(0, 2) + L")";
        if (it.driveType == DRIVE_REMOTE) {
            it.unc = netRemote(it.path);
            std::wstring n = netLabel(it);
            if (!n.empty()) it.label = n;
        }
        L->drives.push_back(std::move(it));
    }
    netPlacesAdd(L->drives); // lo spazio a parte (placeSpaceStart): il server può non rispondere per un minuto
    if (SUCCEEDED(co)) CoUninitialize();
    if (!PostMessageW(L->hwnd, WM_APP_SIDEBAR, 0, (LPARAM)L)) delete L;
    return 0;
}

// --- spazio dei percorsi di rete ------------------------------------------------------------
// Una condivisione lenta (provato: una condivisione che per elencare la radice ci mette un
// minuto) non deve trattenere le unità: il loro spazio arriva col thread della barra, quello dei
// percorsi di rete con questo, dopo. Uno alla volta; le richieste arrivate intanto ne fanno uno alla fine.

struct PlaceSpace { HWND hwnd; std::vector<SideItem> places; };
static bool g_placeRunning = false, g_placeAgain = false;

static DWORD WINAPI placeSpaceThread(void* p) {
    PlaceSpace* S = (PlaceSpace*)p;
    for (SideItem& it : S->places)
        volumeRead(it, it.path.back() == L'\\' ? it.path : it.path + L'\\'); // GetDiskFreeSpaceExW vuole la barra finale
    if (!PostMessageW(S->hwnd, WM_APP_PLACES, 0, (LPARAM)S)) delete S;
    return 0;
}

static void placeSpaceStart() {
    if (g_placeRunning) { g_placeAgain = true; return; }
    PlaceSpace* S = new PlaceSpace{ A.hwnd, {} };
    for (const SideItem& d : A.drives) if (d.place) S->places.push_back(d);
    if (S->places.empty()) { delete S; return; }
    HANDLE h = CreateThread(nullptr, 0, placeSpaceThread, S, 0, nullptr);
    if (h) { CloseHandle(h); g_placeRunning = true; }
    else delete S;
}

static bool samePath(const std::wstring& a, const std::wstring& b);

// Spazio, file system: nelle voci attuali con lo stesso percorso (la barra può essere cambiata intanto)
static void onPlaceSpace(PlaceSpace* S) {
    g_placeRunning = false;
    for (const SideItem& r : S->places)
        for (SideItem& d : A.drives)
            if (d.place && samePath(d.path, r.path)) { d.fs = r.fs; d.free = r.free; d.total = r.total; }
    delete S;
    A.dirty = true;
    if (g_placeAgain) { g_placeAgain = false; placeSpaceStart(); }
}

// Un thread alla volta. Ogni WM_DEVICECHANGE (più d'uno per chiavetta), il ritorno su [z]f e i
// preferiti ne chiedevano uno nuovo: con un'unità di rete appesa (GetVolumeInformationW che non
// torna) i thread si accumulavano. Le richieste arrivate intanto diventano una sola rilettura alla fine.
static bool g_sideRunning = false, g_sideAgain = false;

static void sideLoad() {
    if (!A.sideStarted) return; // prima del primo frame: ci pensa il ciclo principale
    A.sideLoadedAt = nowMs();
    if (g_sideRunning) { g_sideAgain = true; return; }
    bool readFile = !A.favsReady;
    SideLoad* L = new SideLoad{ A.hwnd, ++A.sideGen, readFile, false, false, {}, {}, {}, {}, {}, {} };
    if (!readFile) L->favs = A.favs;
    HANDLE h = CreateThread(nullptr, 0, sideThread, L, 0, nullptr);
    if (h) { CloseHandle(h); g_sideRunning = true; }
    else delete L;
}

static void favSave();
static bool samePath(const std::wstring& a, const std::wstring& b);

// Preferiti letti (dal file o, al primo avvio, quelli predefiniti). Le cartelle aggiunte prima del
// loro arrivo (Ctrl+D o un trascinamento nei primi istanti) vanno in fondo: favSave le ha tenute
// solo in memoria, perché salvare allora avrebbe riscritto il file con loro sole. true se ce n'erano.
static bool favAdopt(std::vector<SideItem>& loaded) {
    bool early = false;
    for (SideItem& f : A.favs) {
        bool dup = false;
        for (const SideItem& l : loaded) if (samePath(l.path, f.path)) { dup = true; break; }
        if (!dup) { loaded.push_back(std::move(f)); early = true; }
    }
    A.favs.swap(loaded);
    A.favsReady = true;
    return early;
}

static void onSideLoaded(SideLoad* L) {
    if (L->fast) {
        A.sideFastDone = true;
        if (settingsLoaded(L->settings)) fontCreate(A.scale);
        if (!A.favsReady && L->fileFound && favAdopt(L->favs)) favSave();
        if (A.drives.empty()) A.drives.swap(L->drives);
        recentLoaded(L->recents);
        undoLoaded(L->undo); // dopo le impostazioni: vale solo con "Annulla dopo il riavvio"
        viewsLoaded(L->views);
        A.dirty = true;
        delete L;
        return;
    }
    bool changed = false;
    if (!A.favsReady) { // primo avvio: il file nasce con le cartelle predefinite
        favAdopt(L->favs);
        changed = true;
    } else {
        // Nel frattempo i preferiti possono essere stati riordinati o modificati: dal risultato
        // si prendono solo i nomi, abbinati per percorso
        for (SideItem& f : A.favs)
            for (const SideItem& r : L->favs)
                if (samePath(f.path, r.path)) {
                    if (f.label != r.label) { f.label = r.label; changed = true; }
                    break;
                }
    }
    if (changed) favSave();
    if (L->gen == A.sideGen) {
        bool same = A.drives.size() == L->drives.size();
        for (size_t i = 0; same && i < A.drives.size(); i++) same = A.drives[i].path == L->drives[i].path;
        // I percorsi di rete arrivano senza spazio: resta quello già letto finché arriva il nuovo
        for (SideItem& n : L->drives)
            if (n.place)
                for (const SideItem& o : A.drives)
                    if (o.place && samePath(o.path, n.path)) { n.fs = o.fs; n.free = o.free; n.total = o.total; break; }
        A.drives.swap(L->drives);
        placeSpaceStart();
        // Unità collegate o scollegate (anche di rete): "Questo PC" non ha un watcher, si rilegge
        if (!same && A.cur && A.cur->isDrives) requestRefresh();
    }
    A.dirty = true;
    delete L;
    g_sideRunning = false;
    if (g_sideAgain) { g_sideAgain = false; sideLoad(); } // chiesta mentre questo thread lavorava
}

static void favSave() {
    if (!A.favsReady) return; // il file non è ancora letto: ci pensa favAdopt
    std::string data;
    for (const SideItem& f : A.favs) {
        data += toUtf8(f.path);
        if (f.label != folderTitle(f.path)) data += "\t" + toUtf8(f.label); // il nome serve solo se diverso
        data += "\r\n";
    }
    saveDataAsync(L"preferiti.txt", std::move(data));
}

static bool samePath(const std::wstring& a, const std::wstring& b) {
    size_t na = a.size(), nb = b.size();
    while (na > 3 && a[na - 1] == L'\\') na--;
    while (nb > 3 && b[nb - 1] == L'\\') nb--;
    return na == nb && _wcsnicmp(a.c_str(), b.c_str(), na) == 0;
}

static int favIndex(const std::wstring& path) {
    for (int i = 0; i < (int)A.favs.size(); i++) if (samePath(A.favs[i].path, path)) return i;
    return -1;
}

// Mette la cartella tra i preferiti alla posizione at; se c'è già, la sposta lì.
// Chi chiama salva (favSave) e avvisa: così più cartelle insieme fanno un salvataggio solo.
static void favPut(const std::wstring& path, int at) {
    if (path.empty()) return;
    int i = favIndex(path);
    at = std::max(0, std::min(at, (int)A.favs.size()));
    if (i >= 0) {
        SideItem it = std::move(A.favs[i]);
        A.favs.erase(A.favs.begin() + i);
        if (i < at) at--; // la posizione era contata con la voce ancora al suo posto
        A.favs.insert(A.favs.begin() + at, std::move(it));
    } else {
        SideItem it;
        it.path = path;
        it.label = folderTitle(path); // il nome localizzato arriva col prossimo caricamento
        A.favs.insert(A.favs.begin() + at, std::move(it));
        sideLoad();
    }
    A.dirty = true;
}

// Ctrl+D: aggiunge la cartella corrente ai preferiti, o la toglie se c'è già
static void favToggle(const std::wstring& path) {
    if (path.empty()) return;
    int i = favIndex(path);
    if (i >= 0) {
        A.favs.erase(A.favs.begin() + i);
        notice(L"Tolta dai preferiti", true);
    } else {
        favPut(path, (int)A.favs.size());
        notice(L"Aggiunta ai preferiti", true);
    }
    favSave();
    A.dirty = true;
}

static void favMove(int i, int d) {
    int j = i + d;
    if (i < 0 || j < 0 || i >= (int)A.favs.size() || j >= (int)A.favs.size()) return;
    std::swap(A.favs[i], A.favs[j]);
    favSave();
    A.dirty = true;
}

// --- disegno e hit test -------------------------------------------------------
// Struttura verticale: [titolo Preferiti] voci… [spazio] Questo PC [˅], unità rientrate…
// "Questo PC" è un nodo dell'albero, come nella navigazione di Explorer: il nome apre Questo PC,
// la freccia a destra apre e chiude il ramo delle unità.

// "Questo PC": indice nf in sideHit (le unità seguono)
static const SideItem& sidePc() {
    static SideItem pc = [] { SideItem it; it.label = L"Questo PC"; return it; }();
    return pc;
}

static float sideSectionH() { return floorf(A.rowH * ui::SIDE_SECTION_H); }
// Preferiti aperti: le voci, o una riga per il suggerimento se non ce ne sono
static float sideFavH() {
    if (!A.favOpen) return 0;
    return A.favs.empty() ? A.sideItemH : A.favs.size() * A.sideItemH;
}
static float sideContentH() {
    size_t n = 1 + (A.drvOpen ? A.drives.size() : 0); // Questo PC e le unità
    return sideSectionH() + sideFavH() + px(ui::GAP_L) + n * A.sideItemH + px(ui::GAP_M);
}
static float sideMaxScroll() {
    float vis = (float)G.h - A.sideTop;
    return std::max(0.0f, sideContentH() - vis);
}

// Posizioni verticali (schermo): prima voce dei preferiti e "Questo PC" (le unità seguono)
static float sideFavY() { return A.sideTop - floorf(A.sideScroll) + sideSectionH(); }
static float sidePcY() { return sideFavY() + sideFavH() + px(ui::GAP_L); }
// Bordo superiore della voce i (favs, poi Questo PC e drives), come la indicizza sideHit
static float sideItemY(int i) {
    int nf = (int)A.favs.size();
    return i < nf ? sideFavY() + i * A.sideItemH : sidePcY() + (i - nf) * A.sideItemH;
}
// Freccia del ramo di Questo PC: stessa colonna di quella dei titoli
static float sideChevronX() { return A.sideW - 1 - px(ui::SIDE_PAD) - F.lineH; }

// Indice della voce (favs, poi Questo PC e drives); -3 titolo "Preferiti", -2 freccia di Questo PC; -1 niente
static int sideHit(int x, int y) {
    if (A.sideW <= 0 || x >= A.sideW || y < A.sideTop || y >= G.h) return -1;
    float fy = sideFavY(), py = sidePcY();
    int nf = (int)A.favs.size(), nd = (int)A.drives.size();
    if (y >= fy - sideSectionH() && y < fy) return -3;
    if (A.favOpen && y >= fy && y < fy + nf * A.sideItemH) return (int)((y - fy) / A.sideItemH);
    if (y >= py && y < py + A.sideItemH) return x >= sideChevronX() - px(ui::GAP_S) ? -2 : nf;
    if (A.drvOpen && y >= py + A.sideItemH && y < py + (nd + 1) * A.sideItemH) return nf + (int)((y - py) / A.sideItemH);
    return -1;
}

// Clic sul titolo dei preferiti o sulla freccia di Questo PC: apre o chiude
static void sideToggleSection(int hit) {
    if (hit == -3) A.favOpen = !A.favOpen;
    else if (hit == -2) A.drvOpen = !A.drvOpen;
    else return;
    A.sideScroll = std::min(A.sideScroll, sideMaxScroll());
    settingsSave();
    A.dirty = true;
}

static const SideItem* sideItem(int i) {
    if (i < 0) return nullptr;
    if (i < (int)A.favs.size()) return &A.favs[i];
    i -= (int)A.favs.size() + 1;
    if (i < 0) return &sidePc();
    return i < (int)A.drives.size() ? &A.drives[i] : nullptr;
}

// --- preferiti col mouse: riordinare e togliere ----------------------------------------
// Un preferito si trascina tra gli altri (solo nella barra: non sposta la cartella vera); per
// questo il clic vale al rilascio, come nella navigazione di Explorer. Sotto il mouse ha a destra
// un cestino, nella colonna delle frecce dei titoli: il primo clic chiede conferma sul posto
// ("Togli?" in C_ERR), il secondo toglie. Uscendo dalla voce o con Esc la conferma si annulla:
// niente dialoghi.

// Cestino del preferito sotto il mouse: indice, o -1
static int favTrashAt(int x, int y) {
    int si = sideHit(x, y);
    return si >= 0 && si < (int)A.favs.size() && x >= sideChevronX() - px(ui::GAP_S) ? si : -1;
}

// Posizione d'inserimento (0…n) più vicina al punto y, tra le voci dei preferiti
static int favGapAt(int y) {
    int at = (int)floorf((y - sideFavY()) / A.sideItemH + 0.5f);
    return std::max(0, std::min((int)A.favs.size(), at));
}

// Linea d'inserimento tra i preferiti: trascinando un preferito o dei file da fuori (drag.cpp)
static void drawFavGap(int at) {
    float ly = sideFavY() + (A.favOpen ? at * A.sideItemH : 0), eh = std::max(1.0f, px(ui::EDGE));
    rrect(px(ui::GAP_S), floorf(ly - eh * 0.5f), A.sideW - 1 - px(ui::GAP_M), eh, eh * 0.5f, C_SEL_EDGE);
}

static bool favDisarm() { // true se c'era una conferma da annullare (Esc)
    if (A.favTrashArmed < 0) return false;
    A.favTrashArmed = -1;
    A.dirty = true;
    return true;
}

// Pressione nella barra laterale: true se riguarda un preferito (cestino o voce)
static bool favMouseDown(int x, int y) {
    int t = favTrashAt(x, y);
    if (t >= 0) {
        if (A.favTrashArmed == t) { // secondo clic: si toglie
            A.favTrashArmed = A.favTrashHover = -1;
            favToggle(A.favs[t].path); // salva e avvisa "Tolta dai preferiti"
        } else {
            A.favTrashArmed = t;
        }
        A.dirty = true;
        return true;
    }
    int si = sideHit(x, y);
    if (si < 0 || si >= (int)A.favs.size()) return false;
    favDisarm();
    A.favPress = si;
    A.favPressX = x;
    A.favPressY = y;
    A.favDropAt = -1;
    SetCapture(A.hwnd);
    return true;
}

// Oltre la soglia di trascinamento di Windows parte il riordino; true finché il tasto è premuto
static bool favMouseMove(int x, int y) {
    if (A.favPress < 0) return false;
    if (A.favDropAt < 0) {
        if (abs(x - A.favPressX) < GetSystemMetrics(SM_CXDRAG) && abs(y - A.favPressY) < GetSystemMetrics(SM_CYDRAG)) return true;
        tipHide();
    }
    int at = favGapAt(y);
    if (at != A.favDropAt) { A.favDropAt = at; A.dirty = true; }
    return true;
}

// Rilascio: trascinato → nuova posizione; altrimenti clic → ci va (Ctrl: nuova scheda).
// keep false (Esc, mouse preso da un'altra finestra): non succede niente.
static bool favMouseUp(bool keep, bool ctrl) {
    if (A.favPress < 0) return false;
    int i = A.favPress, at = A.favDropAt;
    A.favPress = A.favDropAt = -1;
    if (GetCapture() == A.hwnd) ReleaseCapture();
    A.dirty = true;
    if (!keep || i >= (int)A.favs.size()) return true;
    std::wstring path = A.favs[i].path;
    if (at >= 0) {
        favPut(path, at);
        if (favIndex(path) != i) favSave();
    } else if (ctrl) tabNew(path, false);
    else navigate(path, 1, L"");
    return true;
}

static bool insidePath(const std::wstring& child, const std::wstring& dir); // fileops.cpp

// La cartella aperta p sta sull'unità d o nel percorso di rete d (anche d stesso)
static bool sideHolds(const SideItem& d, const std::wstring& p) {
    if (d.drive) return p.size() >= 2 && p[1] == L':' && !_wcsnicmp(p.c_str(), d.path.c_str(), 2);
    return d.place && (samePath(p, d.path) || insidePath(p, d.path));
}

static void drawSidebar(double dt) {
    if (A.sideW <= 0) return;
    float x0 = 0, w = A.sideW - 1;
    float top = A.sideTop, bottom = (float)G.h;
    float textOff = floorf((A.sideItemH - F.lineH) * 0.5f);
    float secOff = floorf((sideSectionH() - F.lineH) * 0.5f) + px(ui::GAP_XS);
    // Fino in fondo alla finestra: il filtro e le barre in basso stanno a destra (render.cpp)
    rect(x0, top, A.sideW, bottom - top, C_TABBAR);
    rect(A.sideW - 1, top, 1, bottom - top, A.colDrag == EDGE_SIDE ? C_SEL_EDGE : C_LINE);
    std::wstring curPath = A.cur ? A.cur->path : L"\x1";
    float pad = px(ui::SIDE_PAD), iconS = (float)I.size;
    float y = top - floorf(A.sideScroll);
    hoverFadeUpdate(A.sideFade, A.sideHover, dt);

    auto section = [&](const wchar_t* t, bool hot, bool open) {
        if (y + sideSectionH() > top && y < bottom) {
            drawText(x0 + pad, y + secOff, t, (int)wcslen(t), hot ? C_TEXT : C_FAINT, w - pad - A.rowH);
            drawUiIcon(x0 + w - pad - F.lineH, y + px(ui::GAP_XS), (float)F.lineH, sideSectionH(),
                       open ? IC_CHEVRON_DOWN : IC_CHEVRON_RIGHT, hot ? C_DIM : C_FAINT);
        }
        y += sideSectionH();
    };
    // indent: rientro delle unità sotto Questo PC; right: dove finisce lo spazio del testo
    auto item = [&](const SideItem& it, int idx, float indent = 0, float right = 0) {
        if (y + A.sideItemH <= top || y >= bottom) { y += A.sideItemH; return; }
        bool cur = samePath(it.path, curPath);
        float ha = hoverAlpha(A.sideFade, idx);
        float bx = x0 + px(ui::GAP_S), bw2 = w - px(ui::GAP_M), br = px(ui::R_S);
        if (cur) rrect(bx, y, bw2, A.sideItemH, br, C_SEL);
        else if (ha > 0) rrect(bx, y, bw2, A.sideItemH, br, fadeA(C_HOVER, ha));
        // Unità o percorso di rete che contiene la cartella aperta: la sua voce, o Questo PC se il
        // ramo è chiuso
        bool parent = false;
        if (it.drive || it.place) parent = sideHolds(it, curPath);
        else if (it.path.empty() && !A.drvOpen)
            for (const SideItem& d : A.drives) if (sideHolds(d, curPath)) { parent = true; break; }
        if (cur || parent) {
            // pillola: cartella corrente, oppure unità su cui si trova (come la navigazione di Windows 11)
            float pw = px(ui::PILL_W), ph = floorf(A.sideItemH * 0.5f);
            rrect(bx, y + floorf((A.sideItemH - ph) * 0.5f), pw, ph, pw * 0.5f, cur ? C_SEL_EDGE : C_SEL_EDGE_PARENT);
        }
        float iy = y + floorf((A.sideItemH - iconS) * 0.5f);
        const IconEntry* ie = it.drive ? iconForKey(it.path, it.path, 0, false, false)
                            : it.path.empty() ? iconForKey(L"<questo-pc>", kThisPc, 0, false, false)
                                              : iconForKey(L"<dir>", L"cartella", FILE_ATTRIBUTE_DIRECTORY, true, false);
        if (ie) drawIcon(x0 + pad + indent, iy, *ie, C_ICON);
        float tx = x0 + pad + indent + iconS + px(ui::GAP_M);
        if (right <= 0) right = x0 + w - pad;
        if (it.total) { // unità e percorsi di rete: spazio libero a destra, barra d'uso sotto
            wchar_t b[32];
            int n = fmtSize(it.free, b);
            float fw = (float)textWidth(b, n);
            drawText(right - fw, y + textOff - px(ui::GAP_XS), b, n, C_FAINT);
            right -= fw + px(ui::GAP_M);
            float bw = x0 + w - pad - tx, bh = std::max(1.0f, px(ui::EDGE)), by = y + A.sideItemH - bh - px(ui::GAP_XS);
            double used = 1.0 - (double)it.free / (double)it.total;
            rrect(tx, by, bw, bh, bh * 0.5f, C_LINE); // barra: estremità arrotondate
            rrect(tx, by, std::max(bh, floorf(bw * (float)used)), bh, bh * 0.5f, used > 0.9 ? C_ERR : C_SCROLL);
        }
        // Preferito sotto il mouse: cestino nella colonna delle frecce, in dissolvenza con l'hover
        // (non mentre si riordina); cliccato una volta resta, in C_ERR, con "Togli?" accanto
        if (idx < (int)A.favs.size() && A.favDropAt < 0) {
            bool armed = A.favTrashArmed == idx, hot = armed || A.favTrashHover == idx;
            float ta = armed ? 1.0f : ha;
            if (ta > 0) {
                float chx = sideChevronX(), g = px(ui::GAP_XS);
                if (hot) rrect(chx - g, y + g, (float)F.lineH + g * 2, A.sideItemH - g * 2, px(ui::R_S), fadeA(C_HOVER_STRONG, ta));
                drawUiIcon(chx, y, (float)F.lineH, A.sideItemH, IC_DELETE, armed ? C_ERR : hot ? C_TEXT : fadeA(C_DIM, ta));
                right = chx - px(ui::GAP_M);
                if (armed) {
                    static const wchar_t q[] = L"Togli?";
                    float qw = (float)textWidth(q, 6);
                    drawText(right - qw, y + textOff, q, 6, C_ERR);
                    right -= qw + px(ui::GAP_M);
                }
            }
        }
        float lift = it.total ? px(ui::GAP_XS) : 0; // spazio per la barra d'uso
        drawText(tx, y + textOff - lift, it.label, cur ? C_TEXT : C_DIM, right);
        y += A.sideItemH;
    };

    section(L"Preferiti", A.sideHover == -3, A.favOpen);
    if (A.favOpen) {
        // Rientrati sotto il titolo come le unità sotto Questo PC: un livello dell'albero
        for (int i = 0; i < (int)A.favs.size(); i++) item(A.favs[i], i, px(ui::SIDE_INDENT));
        if (A.favs.empty()) { // la scorciatoia da kShortcuts, come nei menu
            if (y < bottom) drawText(x0 + pad + px(ui::SIDE_INDENT), y + textOff, std::wstring(shortcutFor(M_FAV_TOGGLE)) + L" aggiunge", C_FAINT, w - pad);
            y += A.sideItemH;
        }
    }
    y += px(ui::GAP_L);
    float chx = sideChevronX(), pcY = y;
    item(sidePc(), (int)A.favs.size(), 0, chx - px(ui::GAP_M));
    if (pcY + A.sideItemH > top && pcY < bottom)
        drawUiIcon(chx, pcY, (float)F.lineH, A.sideItemH, A.drvOpen ? IC_CHEVRON_DOWN : IC_CHEVRON_RIGHT,
                   A.sideHover == -2 ? C_DIM : C_FAINT);
    if (A.drvOpen)
        for (int i = 0; i < (int)A.drives.size(); i++) item(A.drives[i], (int)A.favs.size() + 1 + i, px(ui::SIDE_INDENT));
    // Preferito trascinato: dove andrà (niente linea nei due punti che lo lasciano dov'è)
    if (A.favDropAt >= 0 && A.favDropAt != A.favPress && A.favDropAt != A.favPress + 1) drawFavGap(A.favDropAt);
}

// --- unità di rete: connetti e disconnetti ----------------------------------------------
// mpr.dll può aspettare la rete: tutto gira su un thread. Il dialogo di Windows "Connetti unità
// di rete" ha la nostra finestra come proprietaria, così resta davanti e la blocca come in Explorer.

struct NetJob { HWND hwnd; bool connect; std::wstring drive; DWORD err; HANDLE watcher = nullptr; };
static bool g_netBusy = false;

static DWORD WINAPI netThread(void* p) {
    NetJob* j = (NetJob*)p;
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); // il dialogo usa la shell
    if (j->connect) {
        typedef DWORD(WINAPI * PFN_Dialog)(LPCONNECTDLGSTRUCTW);
        auto dlg = (PFN_Dialog)mprProc("WNetConnectionDialog1W");
        NETRESOURCEW nr = {};
        nr.dwType = RESOURCETYPE_DISK;
        CONNECTDLGSTRUCTW c = {};
        c.cbStructure = sizeof(c);
        c.hwndOwner = j->hwnd;
        c.lpConnRes = &nr;
        c.dwFlags = CONNDLG_USE_MRU;
        j->err = dlg ? dlg(&c) : ERROR_PROC_NOT_FOUND;
        // Chiudendosi, il dialogo (di un altro thread) passa il fuoco alla finestra successiva, non
        // a [z]f. L'ultimo clic è stato nostro, quindi Windows ci lascia tornare davanti.
        SetForegroundWindow(j->hwnd);
        if (j->err == NO_ERROR && c.dwDevNum >= 1 && c.dwDevNum <= 26)
            j->drive = std::wstring(1, (wchar_t)(L'A' + c.dwDevNum - 1)) + L":\\";
    } else {
        // Il watcher della cartella che era aperta sull'unità chiude i suoi handle da sé: si aspetta
        // qui, sul thread, che l'abbia fatto (in pratica pochi millisecondi), poi si disconnette
        if (j->watcher) { WaitForSingleObject(j->watcher, 5000); CloseHandle(j->watcher); j->watcher = nullptr; }
        typedef DWORD(WINAPI * PFN_Cancel)(LPCWSTR, DWORD, BOOL);
        auto cancel = (PFN_Cancel)mprProc("WNetCancelConnection2W");
        // senza forzare: con file aperti sull'unità Windows rifiuta, e lo si dice
        j->err = cancel ? cancel(j->drive.substr(0, 2).c_str(), CONNECT_UPDATE_PROFILE, FALSE) : ERROR_PROC_NOT_FOUND;
    }
    if (SUCCEEDED(co)) CoUninitialize();
    if (!PostMessageW(j->hwnd, WM_APP_NET, 0, (LPARAM)j)) delete j;
    return 0;
}

static void netStart(bool connect, const std::wstring& drive) {
    if (g_netBusy) return;
    HANDLE watcher = nullptr;
    if (!connect) {
        if (drive.size() < 2) return;
        // La cartella aperta è sull'unità: il watcher la tiene aperta e Windows rifiuterebbe.
        // Si va subito su "Questo PC", con l'unità selezionata.
        if (A.cur && A.cur->path.size() >= 2 && !_wcsnicmp(A.cur->path.c_str(), drive.c_str(), 2)) {
            stopWatch(&watcher);
            navigate(L"", 1, drive.substr(0, 2) + L"\\");
        }
    }
    NetJob* j = new NetJob{ A.hwnd, connect, drive, 0, watcher };
    HANDLE h = CreateThread(nullptr, 0, netThread, j, 0, nullptr);
    if (h) { CloseHandle(h); g_netBusy = true; }
    else { if (watcher) CloseHandle(watcher); delete j; }
}

static void onNetDone(NetJob* j) {
    g_netBusy = false;
    std::wstring d = j->drive.substr(0, std::min<size_t>(2, j->drive.size()));
    if (j->connect) {
        if (j->err == NO_ERROR) {
            sideLoad();
            if (!j->drive.empty()) navigate(j->drive, 1, L""); // come Explorer: si apre l'unità appena connessa
        } else if (j->err != (DWORD)-1 && j->err != ERROR_CANCELLED) { // -1: dialogo chiuso senza connettere
            notice(L"Impossibile connettere l'unità di rete (codice " + std::to_wstring(j->err) + L")", false);
        }
    } else if (j->err == NO_ERROR || j->err == ERROR_NOT_CONNECTED) {
        notice(L"Unità " + d + L" disconnessa", true);
        sideLoad();
    } else {
        const wchar_t* why = j->err == ERROR_OPEN_FILES || j->err == ERROR_DEVICE_IN_USE ? L"ci sono file aperti"
                           : j->err == ERROR_ACCESS_DENIED ? L"accesso negato" : L"errore";
        notice(L"Impossibile disconnettere " + d + L": " + why + L" (codice " + std::to_wstring(j->err) + L")", false);
    }
    delete j;
    A.dirty = true;
}

// Proprietà di "Questo PC": Impostazioni › Sistema › Informazioni, come in Explorer. Il verbo
// "properties" sul CLSID non apre niente (provato). Su un thread: sul thread UI costava 222 ms.
static void thisPcProperties() {
    wchar_t* uri = _wcsdup(L"ms-settings:about");
    HANDLE h = uri ? CreateThread(nullptr, 0, openThread, uri, 0, nullptr) : nullptr;
    if (h) CloseHandle(h); else free(uri);
}
