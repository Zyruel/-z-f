// [z]f — dimensione totale delle cartelle: tooltip, anteprima, colonna Dimensione.
//
// Un thread solo, con priorità di I/O bassa per il lavoro di sfondo, somma i file sotto una
// cartella. Non segue collegamenti e junction (come Explorer): niente giri infiniti né doppi conti.
// Due tipi di richieste:
//  - urgenti (tooltip, anteprima): una per chi chiede, sostituita dalla successiva; il totale si
//    vede mentre cresce, aggiornato ogni kDsProgressMs;
//  - di sfondo (colonna Dimensione): le sottocartelle della cartella aperta, nell'ordine della
//    vista; la coda ricomincia a ogni cartella nuova. Una richiesta urgente le interrompe: la
//    cartella interrotta si riconta dopo.
// I risultati vanno in una cache: la cartella contata e le sue sottocartelle dirette, così entrando
// le dimensioni ci sono già. Valgono kDsFreshMs, poi si ricontano; un'operazione sui file e una
// ricarica della cartella aperta le dimenticano prima. Le unità di rete non si contano da sole:
// solo a richiesta.

struct DirSize {
    uint64_t bytes = 0, files = 0;
    bool denied = false;    // qualche cartella non si è potuta leggere: il totale è un minimo
    double at = 0;          // quando è stato contato (nowMs)
};

enum { DS_TIP = 0, DS_PREVIEW = 1, DS_SLOTS = 2 };

static constexpr double kDsFreshMs = 60000;   // oltre, una richiesta riconta
static constexpr double kDsProgressMs = 100;  // aggiornamento del totale parziale (richieste urgenti)

static struct DirSizer {
    CRITICAL_SECTION lock;
    HANDLE wake = nullptr;
    HWND hwnd = nullptr;
    std::unordered_map<std::wstring, DirSize> cache; // chiave: dsKey
    std::wstring ask[DS_SLOTS];       // richiesta urgente in attesa o in corso, per chi chiede
    bool askPending[DS_SLOTS] = {};   // in attesa (non ancora presa dal thread)
    uint64_t askGen[DS_SLOTS] = {};
    std::wstring liveKey[DS_SLOTS];   // conto urgente in corso e il suo totale parziale
    DirSize live[DS_SLOTS];
    std::vector<std::wstring> queue;  // sfondo, nell'ordine della vista
    size_t next = 0;
    uint64_t queueGen = 0;
    std::wstring runKey;              // cartella che il thread sta contando
    bool runStale = false;            // durante il conto è arrivato un dirSizeForget che la riguarda
    LONG posted = 0;                  // WM_APP_DIRSIZE già in coda
    double busyMs = 0;                // F12: tempo totale del thread
    uint64_t counted = 0;             // F12: cartelle contate
} DS;

static std::wstring dsKey(const std::wstring& path) {
    std::wstring k = path;
    while (k.size() > 3 && k.back() == L'\\') k.pop_back();
    if (!k.empty()) CharLowerBuffW(&k[0], (DWORD)k.size());
    return k;
}

static void dsPost() {
    if (InterlockedExchange(&DS.posted, 1) == 0 && !PostMessageW(DS.hwnd, WM_APP_DIRSIZE, 0, 0))
        InterlockedExchange(&DS.posted, 0);
}

struct DsJob { std::wstring path; int slot; uint64_t gen; }; // slot -1: sfondo

static bool dsCanceled(const DsJob& j) {
    EnterCriticalSection(&DS.lock);
    bool c;
    if (j.slot >= 0) c = DS.askGen[j.slot] != j.gen;
    else {
        c = DS.queueGen != j.gen;
        for (int s = 0; s < DS_SLOTS && !c; s++) c = DS.askPending[s]; // un'urgente passa davanti
    }
    LeaveCriticalSection(&DS.lock);
    return c;
}

struct DsWalk { DsJob job; DirSize live; double lastPost = 0; uint32_t tick = 0; bool stop = false; };

// Le sottocartelle dirette della cartella contata vanno anche nella cache: entrando ci sono già
static void dsCacheSub(const std::wstring& sub, DirSize s) {
    s.at = nowMs();
    EnterCriticalSection(&DS.lock);
    if (!DS.runStale) DS.cache[dsKey(sub)] = s;
    LeaveCriticalSection(&DS.lock);
}

// Somma root in out. false se interrotto. Iterativa, con una pila delle cartelle aperte: la
// ricorsione, con un WIN32_FIND_DATAW per livello, esaurirebbe lo stack del thread in un albero
// profondissimo (con i percorsi lunghi si superano i mille livelli).
static bool dsWalk(const std::wstring& root, DirSize& out, DsWalk& w) {
    struct Frame { HANDLE h; std::wstring dir; DirSize sum; };
    std::vector<Frame> st;
    WIN32_FIND_DATAW fd;
    // Apre dir in cima alla pila, col primo elemento in fd; altrimenti dice se è illeggibile
    auto open = [&](const std::wstring& dir, bool& denied) -> bool {
        std::wstring pat = longPath(dir);
        pat += pat.back() == L'\\' ? L"*" : L"\\*";
        HANDLE h = FindFirstFileExW(pat.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE) {
            denied = GetLastError() != ERROR_FILE_NOT_FOUND;
            if (denied) w.live.denied = true;
            return false;
        }
        st.push_back(Frame{ h, dir, DirSize() });
        return true;
    };
    bool denied = false;
    if (!open(root, denied)) { out.denied = denied; return true; }
    bool have = true; // fd contiene un elemento della cartella in cima, non ancora contato
    for (;;) {
        Frame& f = st.back();
        if (!have && !FindNextFileW(f.h, &fd)) { // cartella finita: il suo totale va a quella sopra
            FindClose(f.h);
            DirSize s = f.sum;
            std::wstring dir = std::move(f.dir);
            st.pop_back();
            if (st.empty()) { out = s; return true; }
            Frame& up = st.back();
            up.sum.bytes += s.bytes;
            up.sum.files += s.files;
            up.sum.denied |= s.denied;
            if (st.size() == 1) dsCacheSub(dir, s);
            continue;
        }
        have = false;
        if ((++w.tick & 255) == 0) {
            if (dsCanceled(w.job)) {
                for (Frame& x : st) FindClose(x.h);
                w.stop = true;
                return false;
            }
            double now = nowMs();
            if (w.job.slot >= 0 && now - w.lastPost >= kDsProgressMs) {
                w.lastPost = now;
                EnterCriticalSection(&DS.lock);
                DS.live[w.job.slot] = w.live;
                LeaveCriticalSection(&DS.lock);
                dsPost();
            }
        }
        const wchar_t* nm = fd.cFileName;
        if (nm[0] == L'.' && (!nm[1] || (nm[1] == L'.' && !nm[2]))) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue; // junction, collegamento
            std::wstring sub = joinPath(f.dir, nm);
            bool top = st.size() == 1;
            if (open(sub, denied)) { have = true; continue; } // f non vale più: la pila è cresciuta
            st.back().sum.denied |= denied;
            if (top) { DirSize s; s.denied = denied; dsCacheSub(sub, s); }
        } else {
            uint64_t b = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            f.sum.bytes += b;
            f.sum.files++;
            w.live.bytes += b;
            w.live.files++;
        }
    }
}

static bool dsFresh(const std::wstring& key) {
    auto it = DS.cache.find(key);
    return it != DS.cache.end() && nowMs() - it->second.at < kDsFreshMs;
}

static DWORD WINAPI dsThread(void*) {
    bool background = false;
    for (;;) {
        DsJob j;
        bool have = false;
        EnterCriticalSection(&DS.lock);
        for (int s = 0; s < DS_SLOTS && !have; s++)
            if (DS.askPending[s]) {
                DS.askPending[s] = false;
                j = DsJob{ DS.ask[s], s, DS.askGen[s] };
                DS.liveKey[s] = dsKey(j.path);
                DS.live[s] = DirSize();
                have = true;
            }
        while (!have && DS.next < DS.queue.size()) {
            const std::wstring& p = DS.queue[DS.next++];
            if (!dsFresh(dsKey(p))) { j = DsJob{ p, -1, DS.queueGen }; have = true; }
        }
        if (have) { DS.runKey = dsKey(j.path); DS.runStale = false; }
        LeaveCriticalSection(&DS.lock);
        if (!have) { WaitForSingleObject(DS.wake, INFINITE); continue; }

        // Sfondo: I/O a priorità bassa, per non rallentare il resto del PC; urgente: normale
        bool bg = j.slot < 0;
        if (bg != background) {
            SetThreadPriority(GetCurrentThread(), bg ? THREAD_MODE_BACKGROUND_BEGIN : THREAD_MODE_BACKGROUND_END);
            background = bg;
        }
        double t0 = nowMs();
        DsWalk w;
        w.job = j;
        DirSize d;
        bool ok = dsWalk(j.path, d, w);
        EnterCriticalSection(&DS.lock);
        DS.busyMs += nowMs() - t0;
        bool stale = DS.runStale;
        DS.runKey.clear();
        if (ok && !stale) {
            d.at = nowMs();
            DS.cache[dsKey(j.path)] = d;
            DS.counted++;
        } else if (bg && DS.queueGen == j.gen && DS.next > 0) {
            DS.next--; // interrotta da un'urgente, o contata prima di un cambiamento: si riprende da questa
        } else if (!bg && ok && DS.askGen[j.slot] == j.gen) {
            DS.askPending[j.slot] = true; // cambiata durante il conto: il totale si rifà
        }
        if (j.slot >= 0 && DS.askGen[j.slot] == j.gen && !DS.askPending[j.slot]) DS.liveKey[j.slot].clear();
        LeaveCriticalSection(&DS.lock);
        if (ok) dsPost();
    }
}

static void dsStart() {
    if (DS.wake) return;
    InitializeCriticalSection(&DS.lock);
    DS.wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    DS.hwnd = A.hwnd;
    HANDLE h = CreateThread(nullptr, 0, dsThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}

// Dimensione nota di una cartella. computing: il conto è in corso (out è il parziale, o il
// valore vecchio se c'era). false: niente da mostrare.
static bool dirSizeGet(const std::wstring& path, DirSize& out, bool& computing) {
    if (!DS.wake) return false;
    std::wstring k = dsKey(path);
    EnterCriticalSection(&DS.lock);
    computing = false;
    for (int s = 0; s < DS_SLOTS; s++)
        if ((DS.askPending[s] && dsKey(DS.ask[s]) == k) || DS.liveKey[s] == k) { computing = true; out = DS.live[s]; }
    auto it = DS.cache.find(k);
    bool known = it != DS.cache.end();
    if (known && (!computing || it->second.bytes >= out.bytes)) out = it->second; // il vecchio totale finché il nuovo non lo supera
    LeaveCriticalSection(&DS.lock);
    return known || computing;
}

// Richiesta urgente (tooltip, anteprima); path vuoto: non serve più (si ferma il conto)
static void dirSizeAsk(int slot, const std::wstring& path) {
    dsStart();
    EnterCriticalSection(&DS.lock);
    std::wstring k = dsKey(path);
    bool same = !path.empty() && dsKey(DS.ask[slot]) == k && (DS.askPending[slot] || DS.liveKey[slot] == k);
    if (!same) {
        DS.askGen[slot]++;
        DS.liveKey[slot].clear();
        DS.ask[slot] = path;
        DS.askPending[slot] = !path.empty() && !dsFresh(k);
    }
    LeaveCriticalSection(&DS.lock);
    if (!same) SetEvent(DS.wake);
}


// Colonna Dimensione: si contano le sottocartelle della cartella aperta, nell'ordine della vista
static void dirSizeQueueCurrent() {
    if (!A.sideStarted) return; // prima del primo frame: ci pensa il ciclo principale
    dsStart();
    EnterCriticalSection(&DS.lock);
    DS.queueGen++;
    DS.queue.clear();
    DS.next = 0;
    if (A.dirSizes && A.cur && !A.cur->isDrives && !A.cur->zipLen && !slowPath(A.cur->path)) // zip: i totali li dà l'archivio
        for (uint32_t ei : A.view) {
            const Entry& e = A.cur->e[ei];
            if ((e.attr & FILE_ATTRIBUTE_DIRECTORY) && !(e.attr & FILE_ATTRIBUTE_REPARSE_POINT))
                DS.queue.push_back(joinPath(A.cur->path, A.cur->name(e)));
        }
    LeaveCriticalSection(&DS.lock);
    SetEvent(DS.wake);
}

// Qualcosa è cambiato in path: il suo totale e quelli delle cartelle che la contengono non valgono più
static void dirSizeForget(const std::wstring& path) {
    if (!DS.wake || path.empty()) return;
    std::wstring k = dsKey(path);
    EnterCriticalSection(&DS.lock);
    // Il conto in corso la comprende (è lei o una che la contiene): il suo totale sarebbe quello di
    // prima e resterebbe "fresco" per kDsFreshMs. Il thread non lo scrive e riconta.
    const std::wstring& rk = DS.runKey;
    if (!rk.empty() && k.compare(0, rk.size(), rk) == 0 &&
        (k.size() == rk.size() || rk.back() == L'\\' || k[rk.size()] == L'\\'))
        DS.runStale = true;
    for (;;) {
        DS.cache.erase(k);
        size_t s = k.find_last_of(L'\\');
        if (s == std::wstring::npos || k.size() <= 3) break;
        k.resize(s < 3 && k[1] == L':' ? 3 : s); // "c:\users" → "c:\"
    }
    LeaveCriticalSection(&DS.lock);
}

// Un'operazione su migliaia di voci: si dimentica tutto in un colpo invece che percorso per percorso
static void dirSizeForgetAll() {
    if (!DS.wake) return;
    EnterCriticalSection(&DS.lock);
    DS.cache.clear();
    if (!DS.runKey.empty()) DS.runStale = true; // il conto in corso si rifà
    LeaveCriticalSection(&DS.lock);
}

// Ordinamento per dimensione (thread di lettura o thread UI): le cartelle prendono il totale noto
static void dirSizeFill(Listing* l) {
    if (!DS.wake || l->isDrives || l->zipLen) return; // zip: le cartelle hanno già il totale
    std::wstring base = dsKey(l->path);
    if (base.back() != L'\\') base += L'\\';
    std::wstring k;
    EnterCriticalSection(&DS.lock);
    for (Entry& e : l->e) {
        if (!(e.attr & FILE_ATTRIBUTE_DIRECTORY)) continue;
        k.assign(base);
        k.append(l->name(e), e.nameLen);
        CharLowerBuffW(&k[base.size()], e.nameLen);
        auto it = DS.cache.find(k);
        e.size = it != DS.cache.end() ? it->second.bytes : 0;
    }
    LeaveCriticalSection(&DS.lock);
}
