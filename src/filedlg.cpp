// [z]f — salto dei dialoghi Apri e Salva verso la cartella aperta in [z]f.
//
// I dialoghi sono degli altri programmi e non si possono sostituire senza iniettare codice. Si
// guarda quale finestra passa in primo piano (SetWinEventHook fuori contesto: nessuna DLL negli
// altri processi, la notifica arriva al nostro thread UI) e, se è un dialogo Apri o Salva, si fa
// quello che farebbe l'utente: il percorso nel campo del nome e "Apri"/"Salva", che con una
// cartella ci entra. Poi il campo torna com'era (il nome proposto da Salva resta).
// Quando:
//  - all'apertura del dialogo;
//  - al ritorno, se nel frattempo si è passati da [z]f e lì la cartella è cambiata.
// La scelta di una cartella (FOS_PICKFOLDERS) non si tocca: lì confermare la sceglierebbe.
// Con più finestre (anche processi) di [z]f agisce solo l'ultima usata: il suo HWND sta in memoria
// condivisa. Il thread UI fa solo controlli che non mandano messaggi all'altro processo (classe,
// figli); i messaggi, che devono aspettare il dialogo, li manda un thread, uno per dialogo (FdWork).

// Un salto alla volta per dialogo, su un thread suo. Due thread sullo stesso dialogo si
// intreccerebbero: il secondo leggerebbe come nome "vecchio" il percorso messo dal primo, e il suo
// Apri/Salva potrebbe confermare il nome appena rimesso dal primo (Salva salverebbe il file). Se
// durante un salto si torna con un'altra cartella, vale l'ultima: il salto in corso si lascia se
// non ha ancora toccato il campo del nome, altrimenti finisce e poi si fa l'ultima.
// Che la cartella esista si controlla prima, fuori dal turno, con un thread per richiesta
// (fdCheckThread): su una condivisione che non risponde il controllo aspetta il timeout di rete, e
// dentro il turno farebbe aspettare in fila le richieste più nuove.
struct FdWork {
    HWND dlg = nullptr;
    SRWLOCK lock = SRWLOCK_INIT; // solo per copiare want e i contatori: mai tenuto durante un messaggio
    std::wstring want;           // cartella dell'ultima richiesta controllata
    double wantAt = 0;           // quando è arrivata
    unsigned gen = 0;            // richieste arrivate (thread UI)
    unsigned ready = 0;          // l'ultima richiesta controllata, da fare (il suo gen)
    unsigned taken = 0;          // l'ultima presa dal thread del salto
    bool running = false;        // c'è un thread del salto su questo dialogo
};

struct FdDialog { HWND h; std::wstring lastPath; double lastFgAt; std::shared_ptr<FdWork> work; };

static struct FileDialogs {
    HWINEVENTHOOK hook = nullptr;
    volatile LONG64* lastZf = nullptr; // HWND dell'ultima finestra di [z]f usata (tutti i processi)
    double zfActiveAt = 0;             // quando [z]f è tornato in primo piano
    std::vector<FdDialog> seen;        // dialoghi già visti: per il salto al ritorno
} FD;

static const int kFdNameOpen = 0x47C;  // Apri e dialoghi classici: ComboBoxEx32 › ComboBox › Edit
static const int kFdNameSave = 0x3E9;  // Salva (dialoghi nuovi): Edit dentro un ComboBox

static bool fdClass(HWND h, const wchar_t* cls) {
    wchar_t b[64];
    return GetClassNameW(h, b, 64) && !wcscmp(b, cls);
}

// Campo del nome di un dialogo Apri o Salva, nullptr altrimenti (anche per la scelta di una
// cartella, che ha solo un Edit 0x480). Nessun messaggio al dialogo.
static HWND fdNameEdit(HWND dlg) {
    for (HWND c = nullptr; (c = FindWindowExW(dlg, c, L"ComboBoxEx32", nullptr)) != nullptr;)
        if (GetDlgCtrlID(c) == kFdNameOpen) {
            HWND cb = FindWindowExW(c, nullptr, L"ComboBox", nullptr);
            if (HWND e = cb ? FindWindowExW(cb, nullptr, L"Edit", nullptr) : nullptr) return e;
        }
    HWND found = nullptr;
    EnumChildWindows(dlg, [](HWND h, LPARAM p) -> BOOL {
        if (GetDlgCtrlID(h) == kFdNameSave && fdClass(h, L"Edit") && fdClass(GetParent(h), L"ComboBox")) {
            *(HWND*)p = h;
            return FALSE;
        }
        return TRUE;
    }, (LPARAM)&found);
    return found;
}

// Pronto: la vista dei file esiste e ha il suo elenco. Prima di così "Apri" fa cadere il programma
// del dialogo (provato: accesso non valido in COMDLG32 se il comando arriva mentre si costruisce).
static bool fdReady(HWND dlg) {
    HWND view = nullptr;
    EnumChildWindows(dlg, [](HWND h, LPARAM p) -> BOOL {
        if (fdClass(h, L"SHELLDLL_DefView")) { *(HWND*)p = h; return FALSE; }
        return TRUE;
    }, (LPARAM)&view);
    return view && (FindWindowExW(view, nullptr, L"DirectUIHWND", nullptr) || FindWindowExW(view, nullptr, L"SysListView32", nullptr));
}

// Una richiesta più nuova è arrivata mentre questa aspettava
static bool fdStale(FdWork* w, unsigned gen) {
    AcquireSRWLockShared(&w->lock);
    bool stale = w->gen != gen;
    ReleaseSRWLockShared(&w->lock);
    return stale;
}

// Un salto: aspetta il dialogo pronto, scrive la cartella nel campo del nome, preme Apri/Salva e
// rimette il nome. Fino a che il campo non è cambiato, una richiesta più nuova fa lasciare questa;
// dopo si va fino in fondo, perché solo così il nome proposto torna al suo posto.
// FD_WAIT_MAX vale dalla richiesta (askedAt): oltre, l'utente lavora già nel dialogo, e un salto
// che arriva dopo lo porterebbe via da dove si trova.
static void fdJump(FdWork* w, const std::wstring& path, unsigned gen, double askedAt) {
    double t0 = nowMs();
    HWND edit = nullptr;
    while (IsWindow(w->dlg) && !fdStale(w, gen)) {
        if (nowMs() - askedAt >= ui::FD_WAIT_MAX) return; // dialogo che non si costruisce, o salto in ritardo
        if (fdReady(w->dlg) && (edit = fdNameEdit(w->dlg)) != nullptr) break;
        Sleep(ui::FD_POLL);
    }
    if (!edit) return;
    for (double t1 = nowMs(); nowMs() - t1 < ui::FD_SETTLE; Sleep(ui::FD_POLL)) // il margine, lasciato appena serve
        if (fdStale(w, gen)) return;
    if (!IsWindow(w->dlg) || fdStale(w, gen)) return;
    wchar_t old[2048] = {};
    DWORD_PTR r = 0;
    const UINT f = SMTO_ABORTIFHUNG | SMTO_BLOCK;
    if (!SendMessageTimeoutW(edit, WM_GETTEXT, 2048, (LPARAM)old, f, ui::FD_MSG_WAIT, &r) || fdStale(w, gen)) return;
    if (SendMessageTimeoutW(edit, WM_SETTEXT, 0, (LPARAM)path.c_str(), f, ui::FD_MSG_WAIT, &r)) {
        SendMessageTimeoutW(w->dlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(w->dlg, IDOK), f, ui::FD_OK_WAIT, &r);
        SendMessageTimeoutW(edit, WM_SETTEXT, 0, (LPARAM)old, f, ui::FD_MSG_WAIT, &r); // il nome proposto torna
    }
    // F12. Più dialoghi insieme hanno più thread: un intero a 64 bit scritto con Interlocked non si spezza
    InterlockedExchange64(&A.fdJumpUs, (LONG64)((nowMs() - t0) * 1000));
}

// Il thread del salto di un dialogo: fa l'ultima richiesta controllata finché ne arrivano, poi finisce
static DWORD WINAPI fdThread(void* p) {
    std::shared_ptr<FdWork> w = std::move(*(std::shared_ptr<FdWork>*)p);
    delete (std::shared_ptr<FdWork>*)p;
    for (;;) {
        AcquireSRWLockExclusive(&w->lock);
        if (w->taken == w->ready) {
            w->running = false;
            ReleaseSRWLockExclusive(&w->lock);
            return 0;
        }
        std::wstring path = w->want;
        double askedAt = w->wantAt;
        unsigned gen = w->taken = w->ready;
        ReleaseSRWLockExclusive(&w->lock);
        fdJump(w.get(), path, gen, askedAt);
    }
}

// Il thread del salto, se non c'è già (con il lock non preso)
static void fdRunJumps(const std::shared_ptr<FdWork>& w) {
    AcquireSRWLockExclusive(&w->lock);
    bool start = !w->running && w->taken != w->ready;
    if (start) w->running = true;
    ReleaseSRWLockExclusive(&w->lock);
    if (!start) return; // la richiesta la prende il thread che c'è già, appena finisce
    auto* p = new std::shared_ptr<FdWork>(w);
    HANDLE th = CreateThread(nullptr, 0, fdThread, p, 0, nullptr);
    if (th) { CloseHandle(th); return; }
    delete p;
    AcquireSRWLockExclusive(&w->lock);
    w->running = false;
    w->taken = w->ready; // persa; la prossima cartella fa partire un thread
    ReleaseSRWLockExclusive(&w->lock);
}

// Una richiesta: la cartella c'è ancora? Fuori dal turno del dialogo (vedi FdWork). Passa al salto
// solo se è ancora l'ultima richiesta e non è passato FD_WAIT_MAX.
struct FdCheck { std::shared_ptr<FdWork> w; std::wstring path; unsigned gen; double askedAt; };
static DWORD WINAPI fdCheckThread(void* p) {
    FdCheck* c = (FdCheck*)p;
    DWORD at = GetFileAttributesW(longPath(c->path).c_str());
    if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY) && nowMs() - c->askedAt < ui::FD_WAIT_MAX) {
        FdWork* w = c->w.get();
        AcquireSRWLockExclusive(&w->lock);
        bool last = w->gen == c->gen;
        if (last) {
            w->want = c->path;
            w->wantAt = c->askedAt;
            w->ready = c->gen;
        }
        ReleaseSRWLockExclusive(&w->lock);
        if (last) fdRunJumps(c->w);
    }
    delete c;
    return 0;
}

// La finestra di [z]f visibile più in alto, tranne skip (FindWindowEx va in ordine di profondità;
// nessun messaggio agli altri processi)
static HWND fdTopWindow(HWND skip) {
    for (HWND w = nullptr; (w = FindWindowExW(nullptr, w, ZF_ID, nullptr)) != nullptr;)
        if (w != skip && IsWindowVisible(w)) return w;
    return nullptr;
}

// Notifica di Windows (sul thread UI): una finestra di un altro processo è in primo piano
static void CALLBACK fdOnForeground(HWINEVENTHOOK, DWORD, HWND h, LONG obj, LONG, DWORD, DWORD) {
    if (obj != OBJID_WINDOW || !h || !A.dialogJump || A.closePending) return; // nascosta: sta per chiudersi
    double t0 = nowMs();
    for (size_t i = 0; i < FD.seen.size();) // dialoghi chiusi
        if (!IsWindow(FD.seen[i].h)) FD.seen.erase(FD.seen.begin() + (ptrdiff_t)i); else i++;
    if (!fdClass(h, L"#32770")) return;
    if (!FindWindowExW(h, nullptr, L"DUIViewWndClassName", nullptr) && !FindWindowExW(h, nullptr, L"ComboBoxEx32", nullptr))
        return; // un altro tipo di dialogo
    if (FD.lastZf) { // agisce solo l'ultima finestra di [z]f usata
        HWND last = (HWND)(intptr_t)*FD.lastZf;
        // Nessuna, o un processo caduto che non l'ha svuotata: la più in alto, la stessa per tutti
        if (!last || !IsWindow(last)) last = fdTopWindow(nullptr);
        if (last != A.hwnd) return;
    }
    if (!A.cur || A.cur->path.empty() || A.cur->zipLen) return; // Questo PC, dentro uno zip: niente da proporre
    FdDialog* d = nullptr;
    for (FdDialog& s : FD.seen) if (s.h == h) d = &s;
    bool go;
    if (!d) {
        FD.seen.push_back(FdDialog{ h, L"", 0 });
        d = &FD.seen.back();
        go = true; // appena aperto
    } else {
        go = FD.zfActiveAt > d->lastFgAt && !samePath(A.cur->path, d->lastPath); // si torna da [z]f con un'altra cartella
    }
    d->lastFgAt = nowMs();
    if (go) {
        d->lastPath = A.cur->path;
        if (!d->work) {
            d->work = std::make_shared<FdWork>();
            d->work->dlg = h;
        }
        FdWork* w = d->work.get();
        // I thread tengono il lock solo per copiare: qui non si aspetta. Una richiesta nuova fa
        // lasciare il salto in corso, se non ha ancora toccato il campo del nome (fdStale).
        AcquireSRWLockExclusive(&w->lock);
        unsigned gen = ++w->gen;
        ReleaseSRWLockExclusive(&w->lock);
        FdCheck* c = new FdCheck{ d->work, A.cur->path, gen, nowMs() };
        HANDLE th = CreateThread(nullptr, 0, fdCheckThread, c, 0, nullptr);
        if (th) CloseHandle(th); else delete c; // persa; la prossima cartella ci riprova
    }
    A.fdHookMs = std::max(A.fdHookMs, nowMs() - t0);
}

// [z]f in primo piano: diventa la finestra a cui i dialoghi guardano
static void fdActivated() {
    FD.zfActiveAt = nowMs();
    if (FD.lastZf) InterlockedExchange64(FD.lastZf, (LONG64)(intptr_t)A.hwnd);
}

// La finestra sparisce (chiusa, o nascosta in attesa di un'operazione sui file): se era l'ultima
// usata, la memoria condivisa si svuota, così le altre finestre di [z]f tornano a far saltare i dialoghi
static void fdRelease() {
    if (FD.lastZf) InterlockedCompareExchange64(FD.lastZf, (LONG64)(intptr_t)fdTopWindow(A.hwnd), (LONG64)(intptr_t)A.hwnd);
}

// Dopo il primo frame (main.cpp)
static void fdStart() {
    if (FD.hook) return;
    HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(LONG64), L"Local\\" ZF_ID L"-ultima-finestra");
    if (m) FD.lastZf = (volatile LONG64*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LONG64)); // resta aperta fino all'uscita
    if (GetForegroundWindow() == A.hwnd) fdActivated();
    FD.hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, fdOnForeground, 0, 0,
                              WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}
