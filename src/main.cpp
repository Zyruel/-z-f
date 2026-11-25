// [z]f — avvio: DPI, finestre, inizializzazione, ciclo dei messaggi.

typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
static PFN_GetDpiForWindow pGetDpiForWindow;

static void enableDpiAwareness() {
    typedef BOOL(WINAPI * PFN_Ctx)(HANDLE);
    HMODULE u = GetModuleHandleW(L"user32.dll");
    PFN_Ctx setCtx = (PFN_Ctx)GetProcAddress(u, "SetProcessDpiAwarenessContext");
    if (setCtx && setCtx((HANDLE)(LONG_PTR)-4)) return; // PER_MONITOR_AWARE_V2
    SetProcessDPIAware();
}

static void darkTitleBar(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll"); // resta caricata: serve anche a DwmFlush
    if (!dwm) return;
    typedef HRESULT(WINAPI * PFN)(HWND, DWORD, LPCVOID, DWORD);
    PFN set = (PFN)GetProcAddress(dwm, "DwmSetWindowAttribute");
    BOOL on = TRUE;
    if (set) set(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &on, sizeof(on));
}

// In caso di crash: codice e indirizzo (relativo al modulo) in z-f-crash.txt accanto all'exe
static LONG WINAPI crashFilter(EXCEPTION_POINTERS* ep) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    while (n && path[n - 1] != L'\\') n--;
    wcscpy(path + n, L"z-f-crash.txt");
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        char buf[512];
        HMODULE mod = nullptr;
        void* addr = ep->ExceptionRecord->ExceptionAddress;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)addr, &mod);
        char modName[MAX_PATH] = "?";
        if (mod) GetModuleFileNameA(mod, modName, MAX_PATH);
        int len = _snprintf(buf, 511, "eccezione 0x%08lX a %p\r\nmodulo %s + 0x%llX\r\n",
                            (unsigned long)ep->ExceptionRecord->ExceptionCode, addr, modName,
                            (unsigned long long)((uint8_t*)addr - (uint8_t*)mod));
        DWORD w;
        WriteFile(f, buf, (DWORD)len, &w, nullptr);
        CloseHandle(f);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// --- istanza unica ---------------------------------------------------------------
// Una sola finestra di [z]f: aprire [z]f su una cartella quando è già in esecuzione consegna il
// percorso alla finestra esistente (WM_COPYDATA), che apre una scheda, ed esce subito, prima di
// creare finestre o D3D. Con un'opzione "--" (--new-window, --gpu=...) si apre comunque una
// finestra nuova, che non riceve le cartelle aperte da fuori.

#define ZF_COPYDATA 0x7A66 // "zf"

static HWND findMainInstance() {
    for (HWND w = nullptr; (w = FindWindowExW(nullptr, w, L"z-f", nullptr)) != nullptr;)
        if (GetPropW(w, kInstanceProp)) return w;
    return nullptr;
}

// Percorso dalla riga di comando: assoluto, senza virgolette; un file diventa cartella + voce
static void resolveStartArg(std::wstring arg, std::wstring& path, std::wstring& select) {
    bool sel = false;
    if (_wcsnicmp(arg.c_str(), L"/select,", 8) == 0) { arg = arg.substr(8); sel = true; } // come explorer.exe
    arg = normalizePath(arg);
    if (arg.empty()) return;
    wchar_t full[1024];
    DWORD n = GetFullPathNameW(arg.c_str(), 1024, full, nullptr);
    if (n && n < 1024) arg = full;
    // Prima della finestra: qui il thread non serve ancora l'interfaccia
    DWORD at = GetFileAttributesW(longPath(arg).c_str());
    if (sel || (at != INVALID_FILE_ATTRIBUTES && !(at & FILE_ATTRIBUTE_DIRECTORY))) {
        while (arg.size() > 3 && arg.back() == L'\\') arg.pop_back();
        size_t k = arg.find_last_of(L'\\');
        if (k != std::wstring::npos && arg.size() > 3) {
            select = arg.substr(k + 1);
            arg = arg.substr(0, k);
            if (arg.size() == 2 && arg[1] == L':') arg += L'\\';
        }
    }
    path = arg;
}

// true se la cartella è stata consegnata a una finestra di [z]f già aperta
static bool forwardToRunning(const std::wstring& path, const std::wstring& select) {
    // Il mutex resta aperto per tutta la vita del processo: dice "[z]f principale in esecuzione".
    // Serve per i due avvii quasi simultanei: il secondo aspetta che la finestra del primo esista.
    HANDLE m = CreateMutexW(nullptr, FALSE, L"Local\\z-f-istanza");
    if (!m || GetLastError() != ERROR_ALREADY_EXISTS) return false;
    HWND w = nullptr;
    for (int i = 0; i < 300 && !(w = findMainInstance()); i++) Sleep(10);
    if (!w) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    AllowSetForegroundWindow(pid); // la finestra esistente può venire in primo piano
    std::wstring msg = select + L'|' + path; // un nome di file non contiene mai '|'
    COPYDATASTRUCT cd = { ZF_COPYDATA, (DWORD)((msg.size() + 1) * sizeof(wchar_t)), (void*)msg.c_str() };
    DWORD_PTR r = 0;
    return SendMessageTimeoutW(w, WM_COPYDATA, 0, (LPARAM)&cd, SMTO_ABORTIFHUNG, 3000, &r) && r == 1;
}

// Ricezione nella finestra principale
static LRESULT onCopyData(const COPYDATASTRUCT* cd) {
    if (!cd || cd->dwData != ZF_COPYDATA || cd->cbData < sizeof(wchar_t) || !cd->lpData) return 0;
    // Chiusa ma ancora in attesa di un'operazione sui file: riaprire [z]f la fa tornare
    if (A.closePending) { A.closePending = false; ShowWindow(A.hwnd, SW_SHOW); }
    std::wstring msg((const wchar_t*)cd->lpData, cd->cbData / sizeof(wchar_t));
    while (!msg.empty() && msg.back() == 0) msg.pop_back();
    size_t bar = msg.find(L'|');
    if (bar == std::wstring::npos) return 0;
    openFromOutside(msg.substr(bar + 1), msg.substr(0, bar));
    return 1;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    SetUnhandledExceptionFilter(crashFilter);

    // Argomenti già preparati dal runtime C: niente CommandLineToArgvW, che caricherebbe
    // shell32.dll all'avvio (con MSVC si carica solo quando serve, sul thread delle icone)
    std::wstring start, startSelect;
    bool newWindow = false, startupProbe = false;
    wchar_t** argv = __wargv;
    int argc = __argc;
    for (int i = 1; argv && i < argc; i++) {
        // --predefinito=si|no: [z]f al posto di Explorer (o ripristino), poi esce
        if (!wcscmp(argv[i], L"--predefinito=si") || !wcscmp(argv[i], L"--predefinito=no"))
            return setDefault(argv[i][14] == L's') ? 0 : 1;
        if (!wcscmp(argv[i], L"--gpu=hw")) g_gpuPref = GPU_HW;
        else if (!wcscmp(argv[i], L"--gpu=warp")) g_gpuPref = GPU_WARP;
        else if (!wcscmp(argv[i], L"--prova-avvio")) startupProbe = true; // prove\avvio.bat
        if (argv[i][0] == L'-' && argv[i][1] == L'-') newWindow = true; // --new-window, --gpu=..., prove
        else resolveStartArg(argv[i], start, startSelect);
    }
    if (!newWindow && forwardToRunning(start, startSelect)) return 0;

    hitchLog("--- avvio", GetCurrentProcessId(), 0);
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_qpcFreq = (double)f.QuadPart;
    A.stMain = processAgeMs();
    initPunctRank();
    enableDpiAwareness();
    pGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");

    // Solo GPU: il driver si carica subito, in parallelo con la creazione della finestra.
    // In modalità ibrida invece si parte DOPO WARP: caricare le DLL del driver tiene occupato
    // il loader lock di Windows e bloccherebbe il caricamento di WARP (misurato: 25 → 98 ms).
    HANDLE hwTh = nullptr;
    if (g_gpuPref != GPU_WARP && g_gpuPref != GPU_HYBRID) hwTh = CreateThread(nullptr, 0, hwThread, nullptr, 0, nullptr);

    if (start.empty()) {
        wchar_t buf[MAX_PATH];
        DWORD n = GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH);
        start = (n && n < MAX_PATH) ? buf : L"C:\\";
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = CreateSolidBrush((COLORREF)(C_BG & 0xFFFFFF)); // finché D3D non è pronto
    wc.lpszClassName = L"z-f";
    RegisterClassExW(&wc);
    WNDCLASSEXW wv = {};
    wv.cbSize = sizeof(wv);
    wv.lpfnWndProc = viewProc;
    wv.hInstance = inst;
    wv.hbrBackground = wc.hbrBackground;
    wv.lpszClassName = L"z-f-view";
    RegisterClassExW(&wv);
    A.inst = inst;

    HDC sdc = GetDC(nullptr);
    float sysScale = GetDeviceCaps(sdc, LOGPIXELSX) / 96.0f;
    ReleaseDC(nullptr, sdc);

    A.hwnd = CreateWindowExW(0, L"z-f", L"[z]f", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                             (int)(ui::WINDOW_W * sysScale), (int)(ui::WINDOW_H * sysScale), nullptr, nullptr, inst, nullptr);
    if (!A.hwnd) return 1;
    SetWindowPos(A.hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE); // cornice di [z]f
    if (!newWindow) SetPropW(A.hwnd, kInstanceProp, (HANDLE)1); // riceve le cartelle aperte da fuori
    darkTitleBar(A.hwnd);
    A.viewWnd = createView(A.hwnd);
    InterlockedExchangePointer(&g_mainHwnd, A.hwnd);
    if (InterlockedCompareExchange(&g_hwDone, 0, 0)) PostMessageW(A.hwnd, WM_APP_GPU, 0, 0);
    A.scale = pGetDpiForWindow ? pGetDpiForWindow(A.hwnd) / 96.0f : sysScale;

    // Prima lettura (cartella e barra laterale) in parallelo con l'inizializzazione della GPU
    InitializeCriticalSection(&g_saveLock);
    InitializeCriticalSection(&g_writeLock);
    navigate(start, 0, startSelect);
    sideStartFast();

    // La finestra compare subito (sfondo del tema); il contenuto arriva quando D3D è pronto.
    // L'animazione di apertura di Windows copre buona parte dell'attesa del driver.
    // Compare senza attivarsi e si attiva dopo il primo frame: la prima attivazione di una finestra
    // costa a Windows 15 ms, a volte fino a 80, per avviare il Text Input Framework del thread
    // (prove\ime-avvio.cpp), e qui ritarderebbe il contenuto. Ridotta a icona o ingrandita (come
    // chiede chi avvia [z]f) si mostra come prima.
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    GetStartupInfoW(&si);
    int show = (si.dwFlags & STARTF_USESHOWWINDOW) ? si.wShowWindow : SW_SHOWNORMAL;
    bool activateLater = show == SW_SHOWNORMAL || show == SW_SHOW || show == SW_SHOWDEFAULT;
    ShowWindow(A.hwnd, activateLater ? SW_SHOWNOACTIVATE : SW_SHOWDEFAULT);
    UpdateWindow(A.hwnd);
    A.stWindow = processAgeMs();

    bool ok = false;
    if (g_gpuPref == GPU_HYBRID || g_gpuPref == GPU_WARP) {
        // WARP: nessun driver da caricare, primo frame in poche decine di ms
        ok = gfxCreateDevice(G, true) && gfxCreateSwapchain(G, A.viewWnd, A.hwnd);
        if (ok && g_gpuPref == GPU_HYBRID) hwTh = CreateThread(nullptr, 0, hwThread, nullptr, 0, nullptr);
    } else {
        // Solo GPU: si aspetta il thread del driver
        WaitForSingleObject(hwTh, INFINITE);
        A.switched = true;
        if (g_hwOk) { G = g_hw; g_hw = Gfx(); ok = gfxCreateSwapchain(G, A.viewWnd, A.hwnd); }
        if (!ok) { gfxRelease(G); G = Gfx(); ok = gfxCreateDevice(G, true) && gfxCreateSwapchain(G, A.viewWnd, A.hwnd); }
    }
    if (hwTh) CloseHandle(hwTh);
    if (!ok) {
        MessageBoxW(A.hwnd, L"Impossibile inizializzare Direct3D 11.", L"[z]f", MB_ICONERROR);
        return 1;
    }
    A.stGfx = processAgeMs();
    fontCreate(A.scale);
    iconInit(); // prima di iconSetSize: inizializza il lock
    iconSetSize((int)lroundf(ui::LIST_ICON * A.scale));
    A.stFont = processAgeMs();
    RECT rc;
    GetClientRect(A.hwnd, &rc);
    gfxResize(rc.right, rc.bottom);

    // Breve attesa delle prime letture, così il primo frame mostra già file e barra laterale
    {
        double t0 = nowMs();
        MSG m;
        while ((!A.cur || !A.sideFastDone) && nowMs() - t0 < 100) {
            if (PeekMessageW(&m, nullptr, WM_APP_LISTING, WM_APP_LISTING, PM_REMOVE)) DispatchMessageW(&m);
            else if (PeekMessageW(&m, nullptr, WM_APP_SIDEBAR, WM_APP_SIDEBAR, PM_REMOVE)) DispatchMessageW(&m);
            else Sleep(0);
        }
    }
    A.stWait = processAgeMs();
    A.dirty = true;

    MSG msg;
    for (;;) {
        if (!A.dirty && !animating()) {
            if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else if (G.waitable && !g_vsyncToken) {
            // C'è un frame da disegnare: si aspetta il vsync, ma qualsiasi messaggio sveglia subito.
            // Senza vsync per 100 ms (finestra coperta) si disegna comunque, come prima.
            double tw = nowMs();
            DWORD r = MsgWaitForMultipleObjectsEx(1, &G.waitable, 100, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (r == WAIT_OBJECT_0) { g_vsyncToken = true; noteVsyncWait(nowMs() - tw); }
            else if (r == WAIT_TIMEOUT) g_vsyncToken = true;
        }
        bool quit = false;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { quit = true; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (quit) break;
        if ((A.dirty || animating()) && (g_vsyncToken || !G.waitable)) {
            render();
        }
        if (!A.sideStarted && A.firstFrameDone) { // dopo il primo frame: nomi delle unità, spazio libero
            if (activateLater) { // primo piano, se Windows lo permette ancora (nessun clic altrove nel frattempo)
                double ta = nowMs();
                SetForegroundWindow(A.hwnd);
                A.stActivate = nowMs() - ta;
            }
            if (startupProbe) { // --prova-avvio: i tempi nel registro, poi si chiude
                hitchLog("prova avvio: contenuto", activateLater, A.startupMs);
                hitchLog("prova avvio: primo fuoco", GetForegroundWindow() == A.hwnd, A.stActivate);
                PostMessageW(A.hwnd, WM_CLOSE, 0, 0);
            }
            A.sideStarted = true;
            sideLoad();
            dropRegister(); // OLE per ricevere i file trascinati: dopo il primo frame, non all'avvio
            dirSizeQueueCurrent(); // dimensioni delle cartelle: il disco serve prima alla lista
            linksQueueCurrent();   // destinazioni dei collegamenti, per lo stesso motivo
            fdStart();             // dialoghi Apri e Salva
        }
    }
    saveFlush();
    logFlush();
    return 0;
}
