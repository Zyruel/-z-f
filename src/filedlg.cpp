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
// figli); i messaggi, che devono aspettare il dialogo, li manda un thread.

struct FdDialog { HWND h; std::wstring lastPath; double lastFgAt; };

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

struct FdJob { HWND dlg; std::wstring path; };

static constexpr int kFdPollMs = 25, kFdWaitMax = 120; // al massimo 3 s per un dialogo che si costruisce
static constexpr int kFdSettleMs = 150;                // margine dopo che la vista è comparsa

static DWORD WINAPI fdThread(void* p) {
    FdJob* j = (FdJob*)p;
    double t0 = nowMs();
    DWORD at = GetFileAttributesW(longPath(j->path).c_str());
    HWND edit = nullptr;
    for (int i = 0; i < kFdWaitMax && IsWindow(j->dlg); i++) {
        if (fdReady(j->dlg) && (edit = fdNameEdit(j->dlg)) != nullptr) break;
        Sleep(kFdPollMs);
    }
    if (edit) Sleep(kFdSettleMs);
    if (edit && IsWindow(j->dlg) && at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY)) {
        wchar_t old[2048] = {};
        DWORD_PTR r = 0;
        const UINT f = SMTO_ABORTIFHUNG | SMTO_BLOCK;
        if (SendMessageTimeoutW(edit, WM_GETTEXT, 2048, (LPARAM)old, f, 500, &r) &&
            SendMessageTimeoutW(edit, WM_SETTEXT, 0, (LPARAM)j->path.c_str(), f, 500, &r)) {
            SendMessageTimeoutW(j->dlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(j->dlg, IDOK), f, 2000, &r);
            SendMessageTimeoutW(edit, WM_SETTEXT, 0, (LPARAM)old, f, 500, &r); // il nome proposto torna
        }
        A.fdJumpMs = nowMs() - t0; // F12 (un double scritto da un thread solo)
    }
    delete j;
    return 0;
}

// La finestra di [z]f visibile più in alto, tranne skip (FindWindowEx va in ordine di profondità;
// nessun messaggio agli altri processi)
static HWND fdTopWindow(HWND skip) {
    for (HWND w = nullptr; (w = FindWindowExW(nullptr, w, L"z-f", nullptr)) != nullptr;)
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
        FdJob* j = new FdJob{ h, A.cur->path };
        HANDLE th = CreateThread(nullptr, 0, fdThread, j, 0, nullptr);
        if (th) CloseHandle(th); else delete j;
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
    HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(LONG64), L"Local\\z-f-ultima-finestra");
    if (m) FD.lastZf = (volatile LONG64*)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LONG64)); // resta aperta fino all'uscita
    if (GetForegroundWindow() == A.hwnd) fdActivated();
    FD.hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, fdOnForeground, 0, 0,
                              WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}
