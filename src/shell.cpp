// [z]f — integrazione con la shell di Windows: menu completo, comandi (copia, taglia, proprietà,
// apri con), rinomina nella riga, terminale. Incolla, elimina e le operazioni su disco con la
// cronologia per annullare sono in fileops.cpp.
//
// Il menu è quello vero di Explorer (IContextMenu), con tutte le estensioni installate.
// COM/OLE si inizializza solo al primo uso: l'avvio non ne paga il costo.

static int g_shellCalls = 0;          // cicli modali della shell: il registro dei rallentamenti li ignora
static bool g_clipboardOwner = false;  // abbiamo messo file negli appunti (copia/taglia)

static bool oleReady() {
    static int st = 0;
    if (!st) st = SUCCEEDED(OleInitialize(nullptr)) ? 1 : -1; // STA + appunti OLE, sul thread UI
    return st > 0;
}

// Menu scuri come il resto dell'app (API non documentate di uxtheme, stabili da Windows 10 1903).
// La modalità è del processo e si imposta una volta; ogni finestra che possiede un menu va permessa.
// La chiama il thread del menu completo della shell, per la sua finestra.
static void enableDarkMenus(HWND w) {
    typedef int(WINAPI * PFN_SetPreferredAppMode)(int);
    typedef BOOL(WINAPI * PFN_AllowDarkModeForWindow)(HWND, BOOL);
    typedef void(WINAPI * PFN_FlushMenuThemes)();
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    static PFN_AllowDarkModeForWindow allow = nullptr;
    InitOnceExecuteOnce(&once, [](PINIT_ONCE, PVOID, PVOID*) -> BOOL {
        typedef LONG(WINAPI * PFN_RtlGetVersion)(OSVERSIONINFOW*);
        PFN_RtlGetVersion rtl = (PFN_RtlGetVersion)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
        OSVERSIONINFOW vi = {};
        vi.dwOSVersionInfoSize = sizeof(vi);
        if (!rtl || rtl(&vi) != 0 || vi.dwBuildNumber < 18362) return TRUE;
        HMODULE ux = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!ux) return TRUE;
        auto setMode = (PFN_SetPreferredAppMode)GetProcAddress(ux, MAKEINTRESOURCEA(135));
        auto flush = (PFN_FlushMenuThemes)GetProcAddress(ux, MAKEINTRESOURCEA(136));
        allow = (PFN_AllowDarkModeForWindow)GetProcAddress(ux, MAKEINTRESOURCEA(133));
        if (setMode) setMode(2); // ForceDark
        if (flush) flush();
        return TRUE;
    }, nullptr, nullptr);
    if (allow) allow(w, TRUE);
}

static IShellFolder* bindFolder(const std::wstring& path) {
    std::wstring p = path.empty() ? kThisPc : path;
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (FAILED(SHParseDisplayName(p.c_str(), nullptr, &pidl, 0, nullptr))) return nullptr;
    IShellFolder* sf = nullptr;
    SHBindToObject(nullptr, pidl, nullptr, IID_IShellFolder, (void**)&sf);
    CoTaskMemFree(pidl);
    return sf;
}

// Voci su cui agire: la selezione in ordine di vista; se è vuota, la voce sotto il cursore
static std::vector<uint32_t> selectedEntries() {
    std::vector<uint32_t> v;
    for (uint32_t ei : A.view) if (A.selFlags[ei]) v.push_back(ei);
    if (v.empty() && A.sel >= 0 && A.sel < (int)A.view.size()) v.push_back(A.view[A.sel]);
    return v;
}

// Oggetto della shell (dati da trascinare) per le voci indicate della cartella corrente, o per lo
// sfondo della cartella se items è vuoto (solo ciò che la vista sa creare). I menu e i comandi della
// shell lo costruiscono sul loro thread (smMenuFor).
// Tocca il disco (una ParseDisplayName per voce): si misura qui, perché i messaggi che passano dalla
// shell sono esclusi dal registro dei messaggi lenti (g_shellCalls)
static void* shellObjectFor(const std::vector<uint32_t>& items, REFIID riid) {
    if (!A.cur || !oleReady()) return nullptr;
    double t0 = nowMs();
    struct Measure {
        double t0; size_t n;
        ~Measure() {
            A.shellObjMs = nowMs() - t0;
            if (A.shellObjMs > ui::LOG_MSG) hitchLog("oggetto della shell lento", (unsigned)n, A.shellObjMs);
        }
    } measure{ t0, items.size() };
    IShellFolder* sf = bindFolder(A.cur->path);
    if (!sf) return nullptr;
    void* obj = nullptr;
    if (items.empty()) {
        sf->CreateViewObject(A.hwnd, riid, &obj);
    } else {
        std::vector<LPITEMIDLIST> pidls;
        pidls.reserve(items.size());
        for (uint32_t ei : items) {
            LPITEMIDLIST child = nullptr;
            if (SUCCEEDED(sf->ParseDisplayName(A.hwnd, nullptr, (LPWSTR)A.cur->name(A.cur->e[ei]), nullptr, &child, nullptr)))
                pidls.push_back(child);
        }
        if (!pidls.empty())
            sf->GetUIObjectOf(A.hwnd, (UINT)pidls.size(), (LPCITEMIDLIST*)pidls.data(), riid, nullptr, &obj);
        for (LPITEMIDLIST p : pidls) CoTaskMemFree(p);
    }
    sf->Release();
    return obj;
}

// Oggetto dati della shell creato vuoto, con dentro CF_HDROP (i percorsi, già in memoria) e, se
// preferred non è 0, "Preferred DropEffect": nessuna ParseDisplayName per voce. L'oggetto della shell
// per la selezione costa circa 25 µs a voce (20.300 voci: 500 ms sul thread UI); questo pochi ms.
// CF_HDROP è il formato che leggono Incolla e il rilascio di [z]f e di Explorer.
static IDataObject* hdropObject(const std::vector<std::wstring>& paths, DWORD preferred) {
    if (paths.empty() || !oleReady()) return nullptr;
    std::wstring list; // percorsi separati da \0, con un \0 in più alla fine
    for (const std::wstring& p : paths) {
        list += p;
        list += L'\0';
    }
    list += L'\0';
    HGLOBAL hd = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + list.size() * sizeof(wchar_t));
    HGLOBAL he = preferred ? GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD)) : nullptr;
    IDataObject* d = nullptr;
    bool ok = hd && (he || !preferred) && SUCCEEDED(SHCreateDataObject(nullptr, 0, nullptr, nullptr, IID_IDataObject, (void**)&d));
    if (ok) {
        DROPFILES* df = (DROPFILES*)GlobalLock(hd);
        df->pFiles = sizeof(DROPFILES);
        df->fWide = TRUE;
        memcpy((uint8_t*)df + sizeof(DROPFILES), list.data(), list.size() * sizeof(wchar_t));
        GlobalUnlock(hd);
        FORMATETC fd = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM md = {};
        md.tymed = TYMED_HGLOBAL;
        md.hGlobal = hd;
        ok = SUCCEEDED(d->SetData(&fd, &md, TRUE)); // da qui la memoria è dell'oggetto dati
        if (ok) hd = nullptr;
        if (ok && preferred) {
            *(DWORD*)GlobalLock(he) = preferred;
            GlobalUnlock(he);
            FORMATETC fe = { (CLIPFORMAT)RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            STGMEDIUM me = {};
            me.tymed = TYMED_HGLOBAL;
            me.hGlobal = he;
            ok = SUCCEEDED(d->SetData(&fe, &me, TRUE));
            if (ok) he = nullptr;
        }
    }
    if (hd) GlobalFree(hd);
    if (he) GlobalFree(he);
    if (!ok && d) { d->Release(); d = nullptr; }
    return d;
}

// Negli appunti ci sono file da incollare: percorsi (CF_HDROP) o elementi che sa estrarre solo la
// shell (dentro uno zip, telefoni, allegati). Non apre gli appunti: non aspetta il proprietario.
static bool clipHasFiles() {
    static const UINT idl = RegisterClipboardFormatW(CFSTR_SHELLIDLIST);
    static const UINT desc = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
    return IsClipboardFormatAvailable(CF_HDROP) || IsClipboardFormatAvailable(idl) || IsClipboardFormatAvailable(desc);
}

// Copia e Taglia senza disco: negli appunti CF_HDROP e "Preferred DropEffect" (copia o sposta).
// Unità e Questo PC restano alla shell.
static bool clipSetPaths(const std::vector<std::wstring>& paths, bool cut) {
    double t0 = nowMs();
    IDataObject* d = hdropObject(paths, cut ? DROPEFFECT_MOVE : DROPEFFECT_COPY | DROPEFFECT_LINK);
    if (!d) return false; // la shell ci riprova a modo suo
    bool ok = SUCCEEDED(OleSetClipboard(d));
    d->Release();
    if (!ok) return false;
    g_clipboardOwner = true;
    A.shellObjMs = nowMs() - t0;
    if (A.shellObjMs > ui::LOG_MSG) hitchLog("appunti lenti", (unsigned)paths.size(), A.shellObjMs);
    return true;
}

static bool clipSetFiles(const std::vector<uint32_t>& items, bool cut) {
    if (!A.cur || A.cur->isDrives || A.cur->path.empty() || items.empty()) return false;
    std::vector<std::wstring> paths;
    paths.reserve(items.size());
    for (uint32_t ei : items) paths.push_back(joinPath(A.cur->path, A.cur->name(A.cur->e[ei])));
    return clipSetPaths(paths, cut);
}

// --- menu completo della shell e suoi comandi, su un thread suo --------------------------------
// QueryContextMenu carica tutte le estensioni installate: 250–550 ms a ogni apertura. Costruire e
// mostrare il menu lo fa un thread STA con una finestra sua, mentre l'interfaccia continua a
// disegnare. La finestra è nascosta e non è né figlia né posseduta da quella di [z]f: tra thread
// diversi Windows legherebbe le code dell'input, e un thread fermo fermerebbe anche l'altro.
// TrackPopupMenu vuole in primo piano la finestra che possiede il menu (altrimenti un clic fuori non
// lo chiude): la prende per il tempo del menu e poi lo restituisce a [z]f, se nel frattempo
// l'utente non è passato a un'altra finestra.
// Le voci che fa [z]f (Elimina, Incolla, Rinomina, Copia, Taglia; Incolla e Aggiorna dello sfondo)
// tornano al thread UI con PostMessage, con i percorsi del momento del clic; le altre le esegue la
// shell sul thread del menu, con la finestra di [z]f come proprietaria dei dialoghi. Il thread resta
// vivo dopo il primo uso (un'estensione può lasciargli finestre); se è ancora occupato da un comando
// lungo, il menu successivo parte su un altro.
// Sullo stesso thread vanno i comandi della shell senza menu (shellVerb: Shift+Canc, Proprietà, Apri
// con, Copia e Taglia di unità): costruire l'oggetto della shell costa quanto per il menu, e su una
// condivisione che non risponde aspetta il timeout di rete.

enum SmCmd { SM_NONE, SM_PASTE, SM_REFRESH, SM_RENAME, SM_RECYCLE, SM_PASTE_INTO, SM_COPY, SM_CUT };
enum { SM_ID_PASTE = 0x8001, SM_ID_REFRESH = 0x8002, SM_THREADS_MAX = 4 };
static const UINT kSmRequest = WM_APP + 1;      // messaggio di thread: lParam SmReq*
static const wchar_t kSmClass[] = L"z-f-menu-shell";

struct SmReq {
    HWND main = nullptr;
    int slot = -1;
    uint64_t gen = 0;
    std::string verb;                            // comando senza menu ("delete"…); vuoto: il menu
    std::wstring folder;                         // cartella aperta al clic ("" Questo PC)
    std::vector<std::wstring> names;             // voci (vuoto: sfondo)
    std::wstring path;                           // comando su un percorso intero, non su voci di folder (shellVerbPath)
    std::wstring cursorName;                     // voce del cursore: è lei che si rinomina
    bool background = false, drives = false, shift = false, pasteOk = false;
    POINT pt = {};
    std::wstring pasteLabel, refreshLabel, pasteDest;
};
struct SmRes {
    int slot, cmd = SM_NONE, items = 0;
    bool verb = false;
    double buildMs = -1;                         // menu: costruzione; comando: tutto prima di InvokeCommand
    std::wstring folder, cursorName, pasteDest;
    std::vector<std::wstring> names;
};
struct SmCtx { IContextMenu2* cm2 = nullptr; IContextMenu3* cm3 = nullptr; }; // sottomenu delle estensioni
struct SmThread { DWORD tid = 0; bool busy = false; };

static std::vector<SmThread> g_sm;               // thread UI
static uint64_t g_smGen = 0;                     // thread UI
static volatile LONG64 g_smLatest = 0;           // ultima richiesta valida: le altre non si mostrano

static bool shellMenuBusy() {
    for (const SmThread& t : g_sm) if (t.busy) return true;
    return false;
}

// Un clic o un tasto mentre il menu si costruisce: l'utente è passato ad altro, il menu non compare
static void shellMenuCancel() {
    if (shellMenuBusy()) InterlockedExchange64(&g_smLatest, 0);
}

// Sottomenu disegnati dalle estensioni ("Invia a", "Apri con"): i messaggi arrivano alla finestra
// proprietaria del menu, cioè questa, sul thread del menu
static LRESULT CALLBACK smWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)lp)->lpCreateParams);
    SmCtx* c = (SmCtx*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (c && (c->cm3 || c->cm2) && (msg == WM_INITMENUPOPUP || msg == WM_DRAWITEM || msg == WM_MEASUREITEM || msg == WM_MENUCHAR)) {
        LRESULT r = 0;
        if (c->cm3 && SUCCEEDED(c->cm3->HandleMenuMsg2(msg, wp, lp, &r))) return r;
        if (c->cm2 && SUCCEEDED(c->cm2->HandleMenuMsg(msg, wp, lp))) return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// Menu della shell per un percorso intero, chiesto alla cartella che lo contiene (come Explorer):
// vale per file, cartelle, unità (la cartella è Questo PC) e \\server\condivisione (è \\server).
// Sul thread del menu: il percorso si analizza qui, e su una rete che non risponde aspetta.
static IContextMenu* smMenuForPath(HWND w, const std::wstring& path) {
    void* obj = nullptr;
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (SUCCEEDED(SHParseDisplayName(path.c_str(), nullptr, &pidl, 0, nullptr))) {
        IShellFolder* parent = nullptr;
        PCUITEMID_CHILD child = nullptr;
        if (SUCCEEDED(SHBindToParent(pidl, IID_IShellFolder, (void**)&parent, &child))) {
            parent->GetUIObjectOf(w, 1, &child, IID_IContextMenu, nullptr, &obj);
            parent->Release();
        }
        CoTaskMemFree(pidl);
    }
    return (IContextMenu*)obj;
}

// Menu della shell per le voci di folder (sfondo se names è vuoto), sul thread del menu
static IContextMenu* smMenuFor(HWND w, const std::wstring& folder, const std::vector<std::wstring>& names) {
    // Percorso di rete in Questo PC: la voce è un \\server\condivisione intero, che Questo PC non
    // sa interpretare
    if (folder.empty() && names.size() == 1 && names[0].size() > 2 && names[0][0] == L'\\' && names[0][1] == L'\\')
        return smMenuForPath(w, names[0]);
    IShellFolder* sf = bindFolder(folder);
    if (!sf) return nullptr;
    void* obj = nullptr;
    if (names.empty()) {
        sf->CreateViewObject(w, IID_IContextMenu, &obj);
    } else {
        std::vector<LPITEMIDLIST> pidls;
        pidls.reserve(names.size());
        for (const std::wstring& n : names) {
            LPITEMIDLIST child = nullptr;
            if (SUCCEEDED(sf->ParseDisplayName(w, nullptr, (LPWSTR)n.c_str(), nullptr, &child, nullptr))) pidls.push_back(child);
        }
        if (!pidls.empty())
            sf->GetUIObjectOf(w, (UINT)pidls.size(), (LPCITEMIDLIST*)pidls.data(), IID_IContextMenu, nullptr, &obj);
        for (LPITEMIDLIST p : pidls) CoTaskMemFree(p);
    }
    sf->Release();
    return (IContextMenu*)obj;
}

// Esegue un comando della shell sul thread del menu, con la finestra di [z]f come proprietaria dei dialoghi
static void smInvoke(IContextMenu* cm, const SmReq* r, LPCSTR verbA, LPCWSTR verbW, bool shift, bool ctrl, bool atPoint) {
    CMINVOKECOMMANDINFOEX ici = {};
    ici.cbSize = sizeof(ici);
    ici.fMask = CMIC_MASK_UNICODE | (atPoint ? CMIC_MASK_PTINVOKE : 0) | (shift ? CMIC_MASK_SHIFT_DOWN : 0) |
                (ctrl ? CMIC_MASK_CONTROL_DOWN : 0);
    ici.hwnd = r->main; // i dialoghi stanno sopra [z]f, come prima
    ici.lpVerb = verbA;
    ici.lpVerbW = verbW;
    ici.lpDirectoryW = r->folder.empty() ? nullptr : r->folder.c_str();
    ici.nShow = SW_SHOWNORMAL;
    if (atPoint) ici.ptInvoke = r->pt;
    cm->InvokeCommand((CMINVOKECOMMANDINFO*)&ici);
    // Dati messi negli appunti da questo thread (un'estensione, Copia su un'unità): si consegnano
    // subito a Windows, così non dipendono da lui
    HWND owner = GetClipboardOwner();
    if (owner && GetWindowThreadProcessId(owner, nullptr) == GetCurrentThreadId()) OleFlushClipboard();
}

// Comando senza menu (shellVerb, shellVerbPath). Si esegue anche se nel frattempo l'utente ha
// cliccato altrove: lo ha chiesto lui, e le finestre che apre (conferma, Proprietà, Apri con) sono
// quelle che aspetta. Il tempo si misura anche se la shell non trova il percorso (rete che non risponde).
static void smRunVerb(HWND w, const SmReq* r, SmRes* res) {
    res->verb = true;
    res->items = r->path.empty() ? (int)r->names.size() : 1;
    double t0 = nowMs();
    IContextMenu* cm = r->path.empty() ? smMenuFor(w, r->folder, r->names) : smMenuForPath(w, r->path);
    // Alcune implementazioni vogliono QueryContextMenu prima di InvokeCommand.
    // CMF_DEFAULTONLY la tiene leggera: niente estensioni da caricare ("openas" funziona lo stesso).
    HMENU m = cm ? CreatePopupMenu() : nullptr;
    if (m) cm->QueryContextMenu(m, 0, 1, 0x7FFF, CMF_DEFAULTONLY);
    res->buildMs = nowMs() - t0;
    if (res->buildMs > ui::LOG_MENU) hitchLog("comando della shell lento", (unsigned)res->items, res->buildMs);
    if (!cm) return;
    std::wstring verbW(r->verb.begin(), r->verb.end()); // ASCII
    smInvoke(cm, r, r->verb.c_str(), verbW.c_str(), r->shift, false, false);
    DestroyMenu(m);
    cm->Release();
}

// Una richiesta: costruisce, mostra, esegue o rimanda al thread UI
static void smRun(HWND w, SmCtx& ctx, SmReq* r) {
    SmRes* res = new SmRes;
    res->slot = r->slot;
    res->folder = r->folder;
    res->cursorName = r->cursorName;
    res->pasteDest = r->pasteDest;
    res->names = r->names;
    if (!r->verb.empty()) {
        smRunVerb(w, r, res);
        if (!PostMessageW(r->main, WM_APP_SHELLMENU, 0, (LPARAM)res)) delete res;
        delete r;
        return;
    }
    auto current = [&] { return InterlockedCompareExchange64(&g_smLatest, 0, 0) == (LONG64)r->gen; };
    double t0 = nowMs();
    IContextMenu* cm = current() ? smMenuFor(w, r->folder, r->names) : nullptr;
    HMENU m = cm ? CreatePopupMenu() : nullptr;
    UINT flags = CMF_NORMAL | CMF_CANRENAME | (r->shift ? CMF_EXTENDEDVERBS : 0);
    if (m && FAILED(cm->QueryContextMenu(m, 0, 1, 0x7FFF, flags))) { DestroyMenu(m); m = nullptr; }
    if (m && r->background) { // "Incolla" e "Aggiorna" in Explorer li aggiunge la vista, non il menu della shell
        InsertMenuW(m, 0, MF_BYPOSITION | MF_STRING | (r->pasteOk ? 0 : MF_GRAYED), SM_ID_PASTE, r->pasteLabel.c_str());
        InsertMenuW(m, 1, MF_BYPOSITION | MF_STRING, SM_ID_REFRESH, r->refreshLabel.c_str());
        InsertMenuW(m, 2, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    }
    if (m) {
        res->buildMs = nowMs() - t0;
        res->items = GetMenuItemCount(m);
        if (res->buildMs > ui::LOG_MENU) hitchLog("menu della shell lento", (unsigned)res->items, res->buildMs);
    }
    UINT cmd = 0;
    // Si mostra solo se è ancora quello chiesto e [z]f è ancora davanti (altrimenti non si chiuderebbe)
    if (m && current() && GetForegroundWindow() == r->main) {
        enableDarkMenus(w);
        cm->QueryInterface(IID_IContextMenu3, (void**)&ctx.cm3);
        if (!ctx.cm3) cm->QueryInterface(IID_IContextMenu2, (void**)&ctx.cm2);
        SetForegroundWindow(w);
        cmd = (UINT)TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, r->pt.x, r->pt.y, w, nullptr);
        PostMessageW(w, WM_NULL, 0, 0);
        safeRelease(ctx.cm3);
        safeRelease(ctx.cm2);
        if (GetForegroundWindow() == w) SetForegroundWindow(r->main);
    }
    if (cmd == SM_ID_PASTE) {
        res->cmd = SM_PASTE;
    } else if (cmd == SM_ID_REFRESH) {
        res->cmd = SM_REFRESH;
    } else if (cmd) {
        wchar_t verb[64] = L"";
        if (FAILED(cm->GetCommandString(cmd - 1, GCS_VERBW, nullptr, (LPSTR)verb, 64))) verb[0] = 0;
        bool shift = GetKeyState(VK_SHIFT) < 0, files = !r->background && !r->drives && !r->folder.empty();
        // Elimina, Incolla, Copia e Taglia della shell non passerebbero dalla cronologia o dagli appunti
        // di [z]f: li fa [z]f, come i tasti. Shift+Elimina resta alla shell: è definitivo e lo chiede lei.
        if (!_wcsicmp(verb, L"rename") && files) res->cmd = SM_RENAME; // la shell non rinomina in una vista non sua
        else if (!_wcsicmp(verb, L"delete") && files && !shift) res->cmd = SM_RECYCLE;
        else if (!_wcsicmp(verb, L"paste") && (r->background || !r->pasteDest.empty())) res->cmd = SM_PASTE_INTO;
        else if (!_wcsicmp(verb, L"copy") && files) res->cmd = SM_COPY;
        else if (!_wcsicmp(verb, L"cut") && files) res->cmd = SM_CUT;
        else smInvoke(cm, r, MAKEINTRESOURCEA(cmd - 1), MAKEINTRESOURCEW(cmd - 1), shift, GetKeyState(VK_CONTROL) < 0, true);
    }
    if (m) DestroyMenu(m);
    if (cm) cm->Release();
    if (!PostMessageW(r->main, WM_APP_SHELLMENU, 0, (LPARAM)res)) delete res;
    delete r;
}

static DWORD WINAPI smThread(void* p) {
    SmReq* first = (SmReq*)p;
    HRESULT ole = OleInitialize(nullptr); // STA, e gli appunti OLE per i comandi delle estensioni
    SmCtx ctx;
    HWND w = CreateWindowExW(WS_EX_TOOLWINDOW, kSmClass, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                             GetModuleHandleW(nullptr), &ctx);
    if (!w) { // senza finestra niente menu: il thread UI lo sa dal risultato vuoto
        SmRes* res = new SmRes;
        res->slot = first->slot;
        if (!PostMessageW(first->main, WM_APP_SHELLMENU, 0, (LPARAM)res)) delete res;
        delete first;
        if (SUCCEEDED(ole)) OleUninitialize();
        return 0;
    }
    smRun(w, ctx, first);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!msg.hwnd && msg.message == kSmRequest) { smRun(w, ctx, (SmReq*)msg.lParam); continue; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

// Consegna una richiesta a un thread libero, o a uno nuovo (il primo al primo uso: niente all'avvio).
// Se sono tutti occupati la richiesta si scarta con l'avviso busyText.
static void smSend(SmReq* r, const wchar_t* busyText) {
    int slot = -1;
    for (int i = 0; i < (int)g_sm.size() && slot < 0; i++) if (!g_sm[(size_t)i].busy) slot = i;
    if (slot < 0) {
        if ((int)g_sm.size() >= SM_THREADS_MAX) { notice(busyText, false); delete r; return; }
        if (g_sm.empty()) {
            WNDCLASSW wc = {};
            wc.lpfnWndProc = smWndProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = kSmClass;
            RegisterClassW(&wc);
        }
        g_sm.push_back(SmThread());
        slot = (int)g_sm.size() - 1;
    }
    r->slot = slot;
    if (r->verb.empty()) InterlockedExchange64(&g_smLatest, (LONG64)r->gen); // il menu da mostrare
    SmThread& t = g_sm[(size_t)slot];
    bool sent;
    if (!t.tid) {
        HANDLE h = CreateThread(nullptr, 0, smThread, r, 0, &t.tid);
        sent = h != nullptr;
        if (h) CloseHandle(h);
    } else {
        sent = PostThreadMessageW(t.tid, kSmRequest, 0, (LPARAM)r) != 0;
    }
    if (!sent) { delete r; t.tid = 0; return; }
    t.busy = true;
}

static void showContextMenu(POINT screenPt, bool background) {
    if (!A.cur || zipNoShell()) return;
    if (A.dirty) render(); // la selezione appena cambiata deve vedersi sotto il menu
    std::vector<uint32_t> items;
    if (!background) {
        items = selectedEntries();
        if (items.empty()) return;
    }
    SmReq* r = new SmReq;
    r->main = A.hwnd;
    r->gen = ++g_smGen;
    r->folder = A.cur->path;
    for (uint32_t ei : items) r->names.emplace_back(A.cur->name(A.cur->e[ei]), A.cur->e[ei].nameLen);
    if (A.sel >= 0 && A.sel < (int)A.view.size()) r->cursorName = A.cur->name(A.cur->e[A.view[A.sel]]);
    r->background = background;
    r->drives = A.cur->isDrives;
    r->shift = GetKeyState(VK_SHIFT) < 0;
    r->pasteOk = clipHasFiles();
    r->pt = screenPt;
    // Le scorciatoie vengono da kShortcuts, come nei nostri menu (il "\t" le allinea a destra)
    r->pasteLabel = std::wstring(L"Incolla\t") + shortcutFor(M_PASTE);
    r->refreshLabel = std::wstring(L"Aggiorna\t") + shortcutFor(M_REFRESH);
    if (items.size() == 1 && (A.cur->e[items[0]].attr & FILE_ATTRIBUTE_DIRECTORY) && !A.cur->isDrives)
        r->pasteDest = joinPath(A.cur->path, A.cur->name(A.cur->e[items[0]])); // "Incolla" su una cartella
    smSend(r, L"Il menu di Windows è ancora occupato");
}

// Comando della shell senza menu sulla selezione: Shift+Canc ("delete"), Proprietà ("properties"),
// Apri con ("openas"), Copia e Taglia dove non bastano i percorsi (unità, Questo PC). Oggetto della
// shell e QueryContextMenu costano circa 80 µs a voce in locale e 400 µs su una condivisione (20.000 voci: 1,6 s e 8 s; misure
// con prove\shell-verbi.cpp), e "delete" conta gli elementi dentro InvokeCommand prima di chiedere
// (altri 1,4 s) e poi elimina lì dentro: va tutto sul thread del menu della shell.
static void shellVerb(const char* verb) {
    if (!A.cur) return;
    std::vector<uint32_t> items = selectedEntries();
    if (items.empty()) return;
    bool cut = !strcmp(verb, "cut");
    if (A.cur->zipLen) { // dentro uno zip: Copia estrae e mette negli appunti i file estratti (zipout.cpp)
        if (!strcmp(verb, "copy")) zipJobStart(ZJ_COPY, std::move(items));
        else if (cut || !strcmp(verb, "delete")) zipReadOnly();
        else zipNoShell();
        return;
    }
    if ((cut || !strcmp(verb, "copy")) && clipSetFiles(items, cut)) { A.dirty = true; return; }
    SmReq* r = new SmReq;
    r->main = A.hwnd;
    r->verb = verb;
    r->folder = A.cur->path;
    for (uint32_t ei : items) r->names.emplace_back(A.cur->name(A.cur->e[ei]), A.cur->e[ei].nameLen);
    r->drives = A.cur->isDrives;
    r->shift = GetKeyState(VK_SHIFT) < 0; // lo stato dei tasti è per thread: si legge qui
    smSend(r, L"Windows è ancora occupato con un altro comando");
}

// Comando della shell su un percorso intero, dal thread del menu come shellVerb: Proprietà della
// cartella aperta e delle voci della barra laterale (unità, percorsi di rete). Non
// SHObjectProperties sul thread UI: la finestra avrebbe il suo thread, ma l'analisi del percorso no,
// e una cartella su una condivisione che non risponde fermerebbe l'interfaccia per il timeout di rete.
static void shellVerbPath(const char* verb, const std::wstring& path) {
    if (path.empty()) return;
    SmReq* r = new SmReq;
    r->main = A.hwnd;
    r->verb = verb;
    r->path = path;
    smSend(r, L"Windows è ancora occupato con un altro comando");
}

// Fine di un menu o di un comando (WM_APP_SHELLMENU): le voci che fa [z]f, con i percorsi del
// momento del clic
static void onShellMenu(SmRes* r) {
    if (r->slot >= 0 && r->slot < (int)g_sm.size()) g_sm[(size_t)r->slot].busy = false;
    if (r->buildMs >= 0 && r->verb) { A.verbMs = r->buildMs; A.verbItems = r->items; }
    else if (r->buildMs >= 0) { A.menuMs = r->buildMs; A.menuItems = r->items; }
    bool here = A.cur && !_wcsicmp(A.cur->path.c_str(), r->folder.c_str());
    std::vector<std::wstring> paths;
    for (const std::wstring& n : r->names) paths.push_back(joinPath(r->folder, n.c_str()));
    switch (r->cmd) {
    case SM_PASTE:      if (!r->folder.empty()) pasteFiles(r->folder); break;
    case SM_PASTE_INTO: pasteFiles(r->pasteDest.empty() ? r->folder : r->pasteDest); break;
    case SM_REFRESH:    if (here) refreshNow(); break;
    case SM_RECYCLE:    recyclePaths(std::move(paths)); break;
    case SM_COPY:       clipSetPaths(paths, false); break;
    case SM_CUT:        clipSetPaths(paths, true); break;
    case SM_RENAME:     // nella cartella aperta, se c'è ancora
        if (here && !A.renaming)
            for (int i = 0; i < (int)A.view.size(); i++)
                if (!_wcsicmp(A.cur->name(A.cur->e[A.view[(size_t)i]]), r->cursorName.c_str())) { A.sel = i; beginRename(); break; }
        break;
    }
    delete r;
    A.dirty = true;
    closeIfIdle();
}

// Incolla intelligente: file negli appunti → incolla nella cartella; un percorso assoluto → ci si va;
// qualsiasi altro testo → diventa il filtro
static void smartPaste() {
    if (!A.cur) return;
    if (clipHasFiles()) { pasteFiles(); return; }
    clipAskText(A.hwnd, nullptr); // il testo arriva in smartPasteText
}

static void smartPasteText(const std::wstring& t) {
    if (!A.cur) return;
    std::wstring p = normalizePath(t);
    bool isPath = (p.size() >= 3 && p[1] == L':' && p[2] == L'\\') || (p.size() >= 3 && p[0] == L'\\' && p[1] == L'\\');
    if (isPath) { navigate(p, 1, L""); return; }
    if (t.empty()) return;
    A.filter = t;
    rebuildView(false);
    A.dirty = true;
}

// ---------------------------------------------------------------------------
// Rinomina nella riga
// ---------------------------------------------------------------------------

// La rinomina avviene su un thread (fsStart, fileops.cpp): il thread UI non tocca mai il disco

static void notice(const std::wstring& text, bool info) {
    A.error = text;
    A.errorIsInfo = info;
    SetTimer(A.hwnd, 1, info ? ui::NOTICE_INFO : ui::NOTICE_ERROR, nullptr);
    A.dirty = true;
}

static void beginRename() {
    if (!A.cur || A.cur->isDrives || A.sel < 0 || A.sel >= (int)A.view.size() || zipReadOnly()) return;
    uint32_t ei = A.view[A.sel];
    const Entry& e = A.cur->e[ei];
    std::wstring n(A.cur->name(e), e.nameLen);
    int stop = (int)n.size();
    if (!(e.attr & FILE_ATTRIBUTE_DIRECTORY)) { // come Explorer: selezionato il nome senza estensione
        size_t d = n.find_last_of(L'.');
        if (d != std::wstring::npos && d > 0) stop = (int)d;
    }
    A.renameField.set(n, 0, stop);
    A.renaming = true;
    A.renameEntry = (int)ei;
    selOnly(A.sel);
    ensureVisible();
    A.dirty = true;
}

static void endRename(bool commit) {
    if (!A.renaming) return;
    A.renaming = false;
    A.dirty = true;
    if (!commit || !A.cur || A.renameEntry < 0 || A.renameEntry >= (int)A.cur->e.size()) return;
    std::wstring oldName = A.cur->name(A.cur->e[A.renameEntry]);
    std::wstring nn = A.renameField.text;
    while (!nn.empty() && (nn.back() == L' ' || nn.back() == L'.')) nn.pop_back(); // Windows li toglierebbe in silenzio
    if (nn.empty() || nn == oldName) return;
    if (nn.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) {
        notice(L"Un nome non può contenere \\ / : * ? \" < > |", false);
        A.renaming = true; // si resta nella casella per correggere
        return;
    }
    fsStart(FS_RENAME, joinPath(A.cur->path, oldName.c_str()), joinPath(A.cur->path, nn.c_str()), nn);
}

static DWORD WINAPI openThread(void* p) {
    wchar_t* path = (wchar_t*)p;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    ShellExecuteW(nullptr, L"open", path, nullptr, nullptr, SW_SHOWNORMAL);
    CoUninitialize();
    free(path);
    return 0;
}

// Apri con: il verbo "openas" sul thread del menu, non SHOpenWithDialog sul thread UI, che è modale
// (con la finestra aperta [z]f non si ridisegnerebbe). La finestra è di OpenWith.exe, un processo a parte.
static void openWith() {
    if (!A.cur || selectedEntries().size() != 1 || zipNoShell()) return;
    shellVerb("openas");
}

static void copyPaths(bool folder) {
    if (!A.cur) return;
    std::wstring t;
    if (folder) {
        t = A.cur->path;
    } else {
        for (uint32_t ei : selectedEntries()) {
            if (!t.empty()) t += L"\r\n";
            t += joinPath(A.cur->path, A.cur->name(A.cur->e[ei]));
        }
    }
    if (t.empty()) return;
    bool one = t.find(L'\n') == std::wstring::npos;
    // L'avviso arriva con l'esito (onClipSet): dice "copiato" solo se il testo è negli appunti
    clipSetText(new ClipSet{ A.hwnd, t, one ? L"Percorso copiato" : L"Percorsi copiati",
                             one ? L"Impossibile copiare il percorso" : L"Impossibile copiare i percorsi" });
}

// Proprietà della selezione o della cartella aperta: sempre dal thread del menu (shellVerb)
static void showProperties(bool folder) {
    if (!A.cur || zipNoShell()) return;
    if (folder) shellVerbPath("properties", A.cur->path);
    else shellVerb("properties");
}

static void newFolder() {
    if (!A.cur || A.cur->isDrives || zipReadOnly()) return;
    std::wstring base = L"Nuova cartella", name = base;
    auto exists = [&](const std::wstring& n) {
        for (const Entry& e : A.cur->e) if (!_wcsicmp(A.cur->name(e), n.c_str())) return true;
        return false;
    };
    for (int k = 2; exists(name); k++) name = base + L" (" + std::to_wstring(k) + L")";
    fsStart(FS_MKDIR, joinPath(A.cur->path, name.c_str()), L"", name);
}

static DWORD WINAPI terminalThread(void* p) {
    std::wstring* dir = (std::wstring*)p;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    std::wstring d = *dir;
    if (!d.empty() && d.back() == L'\\') d += L'.'; // "C:\" tra virgolette diventerebbe C:" per la riga di comando
    std::wstring args = L"-d \"" + d + L"\"";
    if ((INT_PTR)ShellExecuteW(nullptr, L"open", L"wt.exe", args.c_str(), nullptr, SW_SHOWNORMAL) <= 32)
        ShellExecuteW(nullptr, L"open", L"cmd.exe", nullptr, dir->c_str(), SW_SHOWNORMAL); // senza Windows Terminal
    CoUninitialize();
    delete dir;
    return 0;
}

static void openTerminal() {
    if (!A.cur || A.cur->path.empty() || zipNoShell()) return;
    HANDLE h = CreateThread(nullptr, 0, terminalThread, new std::wstring(A.cur->path), 0, nullptr);
    if (h) CloseHandle(h);
}
