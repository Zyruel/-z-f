// [z]f — collegamenti (.lnk): destinazione letta su un thread, colonna Destinazione.
//
// Il formato lo legge lnk.cpp, senza la shell. I collegamenti senza percorso (app dello Store,
// Pannello di controllo) non hanno destinazione e la cella resta vuota, come "Apri percorso file"
// disattivato in Explorer.
//
// La colonna c'è solo nelle cartelle con almeno un .lnk: lo dice il thread di lettura dai nomi,
// senza aprire niente, quindi la lista non cambia forma quando arrivano le destinazioni. Queste le
// legge un thread, nell'ordine della vista, e restano in memoria per percorso e data di modifica:
// una ricarica della cartella (watcher, cambio di scheda) le ritrova già sul thread di lettura.
//
// Clic sulla destinazione: va nella cartella che la contiene, con la destinazione selezionata
// ("Apri percorso file" di Explorer); clic centrale: in una nuova scheda.

struct LinkJob { std::wstring path; uint64_t mtime; uint64_t listGen; uint32_t ei; uint64_t gen; };
struct LinkDone { uint64_t listGen; uint32_t ei; LinkTarget t; };
struct LinkCached { uint64_t mtime; LinkTarget t; };

static constexpr size_t kLinkCacheMax = 20000;     // destinazioni in memoria: oltre si ricomincia da capo

static struct Links {
    CRITICAL_SECTION lock;
    HANDLE wake = nullptr;
    HWND hwnd = nullptr;
    std::unordered_map<std::wstring, LinkCached> cache; // chiave: percorso del .lnk in minuscolo
    std::vector<LinkJob> queue;       // cartella aperta, nell'ordine della vista
    size_t next = 0;
    uint64_t gen = 0;                 // cambia a ogni cartella nuova: i risultati vecchi si scartano
    std::vector<LinkDone> done;       // per il thread UI
    LONG posted = 0;                  // WM_APP_LINKS già in coda
    double busyMs = 0, maxMs = 0;     // F12: tempo totale del thread, lettura più lenta
    uint64_t read = 0;                // F12: collegamenti letti dal disco
} LK;

// --- thread ------------------------------------------------------------------------------
static std::wstring lkKey(std::wstring p) {
    if (!p.empty()) CharLowerBuffW(&p[0], (DWORD)p.size());
    return p;
}

static void lkPost() {
    if (InterlockedExchange(&LK.posted, 1) == 0 && !PostMessageW(LK.hwnd, WM_APP_LINKS, 0, 0))
        InterlockedExchange(&LK.posted, 0);
}

static DWORD WINAPI linksThread(void*) {
    for (;;) {
        LinkJob j;
        bool have = false;
        EnterCriticalSection(&LK.lock);
        if (LK.next < LK.queue.size()) { j = LK.queue[LK.next++]; have = true; }
        LeaveCriticalSection(&LK.lock);
        if (!have) { WaitForSingleObject(LK.wake, INFINITE); continue; }
        std::wstring key = lkKey(j.path);
        LinkTarget t;
        EnterCriticalSection(&LK.lock);
        auto it = LK.cache.find(key);
        if (it != LK.cache.end() && it->second.mtime == j.mtime) t = it->second.t; // letto per un'altra scheda o ricarica
        LeaveCriticalSection(&LK.lock);
        if (!t.known) {
            double t0 = nowMs();
            linkRead(j.path, t.path, t.dir);
            t.known = true; // anche se illeggibile: non si riprova a ogni ricarica
            double ms = nowMs() - t0;
            EnterCriticalSection(&LK.lock);
            if (LK.cache.size() >= kLinkCacheMax) LK.cache.clear();
            LK.cache[key] = LinkCached{ j.mtime, t };
            LK.busyMs += ms;
            LK.maxMs = std::max(LK.maxMs, ms);
            LK.read++;
            LeaveCriticalSection(&LK.lock);
        }
        EnterCriticalSection(&LK.lock);
        bool current = LK.gen == j.gen;
        if (current) LK.done.push_back(LinkDone{ j.listGen, j.ei, std::move(t) });
        LeaveCriticalSection(&LK.lock);
        if (current) lkPost();
    }
}

static void linksStart() {
    if (LK.wake) return;
    InitializeCriticalSection(&LK.lock);
    LK.wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    LK.hwnd = A.hwnd;
    HANDLE h = CreateThread(nullptr, 0, linksThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}

// Thread di lettura della cartella: le voci .lnk (dal nome, senza aprirle) e le destinazioni già
// in memoria con la stessa data di modifica. I file solo online non si leggono: li scaricherebbe.
static void linksFind(Listing* l) {
    for (uint32_t i = 0; i < (uint32_t)l->e.size(); i++) {
        const Entry& x = l->e[i];
        if ((x.attr & FILE_ATTRIBUTE_DIRECTORY) || x.nameLen < 5 || _wcsicmp(l->name(x) + x.nameLen - 4, L".lnk")) continue;
        l->lnk.push_back(i);
    }
    l->lnkTo.resize(l->lnk.size());
    for (size_t k = 0; k < l->lnk.size(); k++)
        if (onlineOnly(l->e[l->lnk[k]].attr)) l->lnkTo[k].known = true;
    if (l->lnk.empty() || !LK.wake) return;
    EnterCriticalSection(&LK.lock);
    for (size_t k = 0; k < l->lnk.size(); k++) {
        const Entry& x = l->e[l->lnk[k]];
        auto it = LK.cache.find(lkKey(joinPath(l->path, l->name(x))));
        if (it != LK.cache.end() && it->second.mtime == x.mtime) l->lnkTo[k] = it->second.t;
    }
    LeaveCriticalSection(&LK.lock);
}

// Posto della voce ei tra i collegamenti della cartella, -1 se non è un collegamento
static int linkSlot(const Listing* l, uint32_t ei) {
    auto it = std::lower_bound(l->lnk.begin(), l->lnk.end(), ei);
    return it != l->lnk.end() && *it == ei ? (int)(it - l->lnk.begin()) : -1;
}
// Destinazione nota della voce ei, nullptr se non c'è (o non è ancora arrivata)
static const LinkTarget* linkOf(const Listing* l, uint32_t ei) {
    if (!l || l->lnk.empty()) return nullptr;
    int s = linkSlot(l, ei);
    return s >= 0 && !l->lnkTo[s].path.empty() ? &l->lnkTo[s] : nullptr;
}

// Cartella aperta: le destinazioni non ancora lette, nell'ordine della vista (prima quelle in alto)
static void linksQueueCurrent() {
    if (!A.sideStarted) return; // prima del primo frame: ci pensa il ciclo principale
    std::vector<LinkJob> jobs;
    if (A.cur && !A.cur->lnk.empty())
        for (uint32_t ei : A.view) {
            int s = linkSlot(A.cur, ei);
            if (s >= 0 && !A.cur->lnkTo[s].known)
                jobs.push_back(LinkJob{ joinPath(A.cur->path, A.cur->name(A.cur->e[ei])), A.cur->e[ei].mtime, A.cur->gen, ei, 0 });
        }
    if (jobs.empty() && !LK.wake) return;
    linksStart();
    EnterCriticalSection(&LK.lock);
    LK.gen++;
    for (LinkJob& j : jobs) j.gen = LK.gen;
    LK.queue.swap(jobs);
    LK.next = 0;
    LeaveCriticalSection(&LK.lock);
    SetEvent(LK.wake);
}

// Destinazioni arrivate: nella cartella aperta, se è ancora quella
static void onLinks() {
    InterlockedExchange(&LK.posted, 0);
    std::vector<LinkDone> d;
    EnterCriticalSection(&LK.lock);
    d.swap(LK.done);
    LeaveCriticalSection(&LK.lock);
    bool any = false;
    for (LinkDone& x : d) {
        if (!A.cur || A.cur->gen != x.listGen) continue;
        int s = linkSlot(A.cur, x.ei);
        if (s < 0) continue;
        A.cur->lnkTo[s] = std::move(x.t);
        any = true;
    }
    if (!any) return;
    if (A.sortCol == COL_LINK) resort(); // ordinata per destinazione: ora si sa dove va
    A.dirty = true;
}

// --- aprire la destinazione ------------------------------------------------------------------
// La cartella che contiene la destinazione, con la destinazione selezionata ("Apri percorso
// file"). Un'unità si mostra in Questo PC; \\server\condivisione si apre (sopra non si elenca).
static void linkReveal(const std::wstring& target, bool newTab) {
    std::wstring parent, child;
    if (uncShareRoot(target)) parent = target;
    else if (!parentOf(target, parent, child)) return;
    if (newTab) tabNew(parent, false, child);
    else navigate(parent, 1, child);
}

// Voce della vista vi: clic (newTab false) o clic centrale sulla destinazione
static void linkOpen(int vi, bool newTab) {
    if (!A.cur || vi < 0 || vi >= (int)A.view.size()) return;
    if (const LinkTarget* t = linkOf(A.cur, A.view[vi])) linkReveal(t->path, newTab);
}

// Destinazione sotto il mouse (indice di vista), -1 se nessuna. Vale solo il testo (più GAP_S per
// lato, come il fondo che si accende), non lo spazio vuoto della colonna: lì parte il rettangolo.
static int linkHit(int x, int y) {
    if (A.zoom != ZOOM_DETAILS || A.linkW <= 0 || !A.cur) return -1;
    int r = itemAt(x, y);
    if (r < 0) return -1;
    const LinkTarget* t = linkOf(A.cur, A.view[r]);
    if (!t) return -1;
    float pad = px(ui::GAP_S);
    float w = std::min((float)textWidth(t->path.c_str(), (int)t->path.size()), A.dateX - A.padX - A.linkX);
    return x >= A.linkX - pad && x < A.linkX + w + pad ? r : -1;
}
