// [z]f — aggiornamento in tempo reale della cartella aperta (ReadDirectoryChangesW).
//
// Un thread osserva la cartella e avvisa la finestra. La finestra raggruppa le notifiche
// (al massimo una ricarica ogni 50 ms) e rilegge la cartella in background,
// conservando selezione, scroll e filtro.

// Osserva anche la cartella superiore: ReadDirectoryChangesW non segnala lo spostamento
// della cartella osservata, ma il suo nome che sparisce dalla superiore sì.
//
// Tutto il lavoro col disco è sul thread: aprire le due cartelle (su una condivisione di rete può
// costare secondi) e chiuderle. Il thread UI crea l'evento di stop, ne dà una copia al thread e per
// fermarlo lo segnala e basta: non aspetta. Ognuno chiude la propria copia dell'evento, così nessuno
// chiude un handle che l'altro sta ancora usando.
enum { WATCH_CHANGED = 0, WATCH_LOST = 1, WATCH_UNAVAILABLE = 2 }; // lParam di WM_APP_DIRCHANGED
struct WatchCtx { std::wstring path; HANDLE dir = nullptr, parent = nullptr; std::wstring name; HANDLE stop; HWND hwnd; uint64_t gen; };
static HANDLE g_watchStop = nullptr, g_watchThread = nullptr;

static HANDLE watchOpen(const std::wstring& path) {
    HANDLE h = CreateFileW(longPath(path).c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    return h == INVALID_HANDLE_VALUE ? nullptr : h;
}

static DWORD WINAPI watchThread(void* p) {
    WatchCtx* c = (WatchCtx*)p;
    c->dir = watchOpen(c->path);
    if (!c->dir) { // non osservabile: come prima, niente ricarica
        PostMessageW(c->hwnd, WM_APP_DIRCHANGED, (WPARAM)c->gen, WATCH_UNAVAILABLE);
        CloseHandle(c->stop);
        delete c;
        return 0;
    }
    // Cartella superiore (non per la radice di un'unità)
    std::wstring pth = c->path;
    while (pth.size() > 3 && pth.back() == L'\\') pth.pop_back();
    size_t k = pth.find_last_of(L'\\');
    if (k != std::wstring::npos && k + 1 < pth.size() && pth.size() > 3) {
        c->name = pth.substr(k + 1);
        std::wstring par = pth.substr(0, k);
        if (par.size() == 2 && par[1] == L':') par += L'\\';
        if (par.size() > 2) c->parent = watchOpen(par);
    }
    const DWORD BUF = 64 * 1024;
    void* buf = malloc(BUF);
    void* pbuf = c->parent ? malloc(BUF) : nullptr;
    OVERLAPPED ov = {}, pov = {};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    pov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    const DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE |
                         FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_ATTRIBUTES;
    bool ok = ReadDirectoryChangesW(c->dir, buf, BUF, FALSE, filter, nullptr, &ov, nullptr) != 0;
    bool pok = c->parent &&
               ReadDirectoryChangesW(c->parent, pbuf, BUF, FALSE, FILE_NOTIFY_CHANGE_DIR_NAME, nullptr, &pov, nullptr);
    if (!ok) PostMessageW(c->hwnd, WM_APP_DIRCHANGED, (WPARAM)c->gen, WATCH_LOST);
    while (ok) {
        HANDLE hs[3];
        DWORD n = 0;
        hs[n++] = c->stop;
        hs[n++] = ov.hEvent;
        if (pok) hs[n++] = pov.hEvent;
        DWORD w = WaitForMultipleObjects(n, hs, FALSE, INFINITE);
        if (w == WAIT_OBJECT_0) break; // richiesta di stop
        DWORD bytes = 0;
        if (w == WAIT_OBJECT_0 + 1) {
            if (!GetOverlappedResult(c->dir, &ov, &bytes, FALSE)) { // cartella eliminata o non più accessibile
                PostMessageW(c->hwnd, WM_APP_DIRCHANGED, (WPARAM)c->gen, WATCH_LOST);
                ok = false;
                break;
            }
            // bytes == 0: troppi eventi per il buffer. Non importa: ricarichiamo comunque tutto.
            PostMessageW(c->hwnd, WM_APP_DIRCHANGED, (WPARAM)c->gen, WATCH_CHANGED);
            ResetEvent(ov.hEvent);
            if (!ReadDirectoryChangesW(c->dir, buf, BUF, FALSE, filter, nullptr, &ov, nullptr)) {
                PostMessageW(c->hwnd, WM_APP_DIRCHANGED, (WPARAM)c->gen, WATCH_LOST);
                ok = false;
            }
        } else {
            if (!GetOverlappedResult(c->parent, &pov, &bytes, FALSE)) { pok = false; continue; }
            bool hit = false;
            if (bytes) {
                const uint8_t* q = (const uint8_t*)pbuf;
                for (;;) {
                    const FILE_NOTIFY_INFORMATION* fi = (const FILE_NOTIFY_INFORMATION*)q;
                    if ((fi->Action == FILE_ACTION_REMOVED || fi->Action == FILE_ACTION_RENAMED_OLD_NAME) &&
                        fi->FileNameLength / sizeof(wchar_t) == c->name.size() &&
                        _wcsnicmp(fi->FileName, c->name.c_str(), c->name.size()) == 0)
                        hit = true;
                    if (!fi->NextEntryOffset) break;
                    q += fi->NextEntryOffset;
                }
            }
            if (hit) {
                // L'handle segue la cartella: chiediamo a Windows dove si trova adesso
                wchar_t np[2048];
                DWORD r = GetFinalPathNameByHandleW(c->dir, np, 2048, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
                wchar_t* out = (r && r < 2048) ? _wcsdup(np) : nullptr;
                if (!PostMessageW(c->hwnd, WM_APP_DIRMOVED, (WPARAM)c->gen, (LPARAM)out)) free(out);
            }
            ResetEvent(pov.hEvent);
            pok = ReadDirectoryChangesW(c->parent, pbuf, BUF, FALSE, FILE_NOTIFY_CHANGE_DIR_NAME, nullptr, &pov, nullptr);
        }
    }
    DWORD b;
    if (ok) { CancelIoEx(c->dir, &ov); GetOverlappedResult(c->dir, &ov, &b, TRUE); }
    if (pok) { CancelIoEx(c->parent, &pov); GetOverlappedResult(c->parent, &pov, &b, TRUE); }
    CloseHandle(ov.hEvent);
    CloseHandle(pov.hEvent);
    free(buf);
    free(pbuf);
    CloseHandle(c->dir);
    if (c->parent) CloseHandle(c->parent);
    CloseHandle(c->stop);
    delete c;
    return 0;
}

// Non aspetta il thread: chiudere gli handle (e annullare le letture in corso su una cartella di
// rete) è lavoro suo. Le sue notifiche ancora in coda hanno una generazione vecchia e si scartano.
// Con thread non nullo restituisce l'handle del thread che si sta fermando, per chi deve aspettare
// che la cartella sia davvero chiusa (la disconnessione di un'unità di rete, sul suo thread).
static void stopWatch(HANDLE* thread = nullptr) {
    if (g_watchStop) {
        SetEvent(g_watchStop);
        CloseHandle(g_watchStop);
        g_watchStop = nullptr;
    }
    if (g_watchThread) {
        if (thread) *thread = g_watchThread;
        else CloseHandle(g_watchThread);
        g_watchThread = nullptr;
    }
    A.watching = false;
    A.watchGen++; // le notifiche ancora in coda del vecchio watcher vengono ignorate
}

static void startWatch(const std::wstring& path) {
    stopWatch();
    if (path.empty()) return; // "Questo PC": niente da osservare
    if (A.cur && A.cur->zipLen && !_wcsicmp(path.c_str(), A.cur->path.c_str())) return; // dentro uno zip: F5 rilegge l'archivio se è cambiato
    HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, nullptr), mine = nullptr;
    if (!stop) return;
    if (!DuplicateHandle(GetCurrentProcess(), stop, GetCurrentProcess(), &mine, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        CloseHandle(stop);
        return;
    }
    WatchCtx* c = new WatchCtx;
    c->path = path; c->stop = stop; c->hwnd = A.hwnd; c->gen = A.watchGen;
    HANDLE th = CreateThread(nullptr, 0, watchThread, c, 0, nullptr);
    if (!th) { CloseHandle(stop); CloseHandle(mine); delete c; return; }
    g_watchThread = th;
    g_watchStop = mine;
    A.watching = true; // se la cartella non si può osservare, il thread lo dice (WATCH_UNAVAILABLE)
}

static void requestRefresh() {
    if (!A.cur || A.loading) return;        // una navigazione in corso porterà comunque dati freschi
    if (A.refreshInFlight) { A.refreshAgain = true; return; }
    A.refreshStart = A.changeAt > 0 ? A.changeAt : nowMs();
    A.changeAt = 0;
    navigate(A.cur->path, 0, L"", true);
}

// F5, "Aggiorna": ricarica subito; se la cartella non era osservata (watcher fallito) si riprova
static void refreshNow() {
    if (!A.cur) return;
    if (!A.watching) startWatch(A.cur->path);
    A.changeAt = nowMs();
    requestRefresh();
}

static void onDirChanged(uint64_t gen, int what) {
    if (gen != A.watchGen) return;
    if (what == WATCH_UNAVAILABLE) { A.watching = false; return; } // F5 riprova
    A.notifyCount++;
    if (what == WATCH_LOST) A.watching = false;
    if (!A.changePending) {
        A.changePending = true;
        A.changeAt = nowMs();
        SetTimer(A.hwnd, 3, ui::WATCH_BATCH, nullptr); // raggruppa le notifiche: una ricarica ogni WATCH_BATCH al massimo
    }
}

// La cartella corrente non esiste più: si risale alla superiore
static void folderGone(std::wstring p) {
    stopWatch();
    recentForget(p); // non va più proposta da "Vai a"
    notice(L"La cartella è stata eliminata o spostata", false);
    A.noticeKeep = true; // altrimenti la cartella superiore, arrivando, lo cancellerebbe subito
    if (A.cur && A.cur->zipLen && !_wcsnicmp(p.c_str(), A.cur->path.c_str(), A.cur->zipLen))
        p.resize(A.cur->zipLen); // dentro uno zip sparito o illeggibile: si va dove stava l'archivio
    while (p.size() > 3 && p.back() == L'\\') p.pop_back();
    size_t k = p.find_last_of(L'\\');
    if (uncShareRoot(p)) {
        navigate(L"", 0, L""); // condivisione sparita: sopra c'è solo \\server, che non si elenca
    } else if (k != std::wstring::npos && k >= 2) {
        std::wstring parent = p.substr(0, k);
        if (parent.size() == 2 && parent[1] == L':') parent += L'\\';
        navigate(parent, 0, p.substr(k + 1));
    }
    A.dirty = true;
}

static void onDirMoved(uint64_t gen, wchar_t* np) {
    std::wstring n = np ? np : L"";
    free(np);
    if (gen != A.watchGen || !A.cur) return;
    if (n.compare(0, 8, L"\\\\?\\UNC\\") == 0) n = L"\\\\" + n.substr(8);
    else if (n.compare(0, 4, L"\\\\?\\") == 0) n = n.substr(4);
    std::wstring low = n;
    CharLowerBuffW(&low[0], (DWORD)low.size());
    if (n.empty() || low.find(L"\\$recycle.bin\\") != std::wstring::npos) { folderGone(A.cur->path); return; }
    if (_wcsicmp(n.c_str(), A.cur->path.c_str()) == 0) return;
    // Spostata o rinominata: la seguiamo, conservando selezione, scroll e filtro
    notice(L"Cartella spostata in " + n, true);
    A.refreshStart = nowMs();
    if (A.refreshInFlight) A.gen++; // scarta la ricarica in corso del vecchio percorso
    navigate(n, 0, L"", true);
}
