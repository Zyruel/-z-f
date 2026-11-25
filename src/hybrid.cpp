// [z]f — avvio grafico ibrido: si disegna subito con WARP, poi si passa alla GPU quando il driver è pronto.

static volatile LONG g_hwDone = 0;
static PVOID volatile g_mainHwnd = nullptr;
static bool g_hwOk = false;

static DWORD WINAPI hwThread(void*) {
    g_hwOk = gfxCreateDevice(g_hw, false);
    InterlockedExchange(&g_hwDone, 1);
    HWND h = (HWND)InterlockedCompareExchangePointer(&g_mainHwnd, nullptr, nullptr);
    if (h) PostMessageW(h, WM_APP_GPU, 0, 0); // altrimenti lo fa il thread UI quando crea la finestra
    return 0;
}

static void dwmFlush() {
    typedef HRESULT(WINAPI * PFN)();
    static PFN f = (PFN)GetProcAddress(LoadLibraryW(L"dwmapi.dll"), "DwmFlush");
    if (!f || FAILED(f())) Sleep(16);
}

static LRESULT CALLBACK viewProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT; // mouse e tastiera vanno alla finestra principale
    if (msg == WM_PAINT) A.dirty = true;
    return DefWindowProcW(h, msg, wp, lp);
}

static HWND createView(HWND parent) {
    RECT rc;
    GetClientRect(parent, &rc);
    return CreateWindowExW(0, L"z-f-view", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, rc.right, rc.bottom,
                           parent, nullptr, A.inst, nullptr);
}

// La GPU è pronta: nuova finestra figlia SOTTO quella di WARP, primo frame lì,
// attesa che il compositore lo mostri, poi si rimuove WARP. Nessun frame vuoto.
// Il passaggio tiene occupato il thread UI per qualche millisecondo: se l'utente sta usando
// la finestra (o la sta trascinando) si aspetta un momento tranquillo, al massimo 2 secondi.
static void gpuRecovered();

static void onGpuReady() {
    if (A.gpuLost) { gpuRecovered(); return; }
    if (A.switched || !g_hwOk || !g_hw.dev) return;
    if (!A.gpuReadyAt) A.gpuReadyAt = nowMs();
    bool busy = A.inSizeMove || A.menu.open || DD.own || DD.over || nowMs() - A.lastInputAt < 150;
    if (busy && nowMs() - A.gpuReadyAt < 2000) { SetTimer(A.hwnd, 4, 50, nullptr); return; }
    A.switched = true;
    double t0 = nowMs();
    HWND v2 = createView(A.hwnd);
    if (!v2) { gfxRelease(g_hw); return; }
    SetWindowPos(v2, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (!gfxCreateSwapchain(g_hw, v2, A.hwnd)) { DestroyWindow(v2); gfxRelease(g_hw); return; }

    A.warpDevMs = G.deviceMs;
    A.warpSwapMs = G.swapMs;
    // Gli atlanti di glifi e icone passano così come sono dal device WARP a quello nuovo
    gfxCopyRows(G, G.atlas, g_hw, g_hw.atlas, (UINT)(F.penY + F.shelfH));
    gfxCopyRows(G, G.iconAtlas, g_hw, g_hw.iconAtlas, (UINT)iconRowsUsed());
    Gfx old = G;
    G = g_hw;
    g_hw = Gfx();
    previewUpload(); // la texture dell'anteprima si ricrea dalla copia in RAM
    thumbsDeviceChanged(); // le miniature tornano sulla GPU dalle copie in RAM
    A.dirty = true;
    A.firstGpuPresent = true; // il driver prepara la swapchain nuova: 15–30 ms, atteso (render lo registra col suo nome)
    render();
    dwmFlush(); // il primo frame della GPU è a schermo: ora si può togliere WARP
    HWND oldView = A.viewWnd;
    A.viewWnd = v2;
    gfxRelease(old);
    DestroyWindow(oldView);
    A.switchAt = processAgeMs();
    A.switchCost = nowMs() - t0;
    A.dirty = true;
}

// --- GPU persa (driver aggiornato o reimpostato, scheda rimossa) -------------------------------
// Il device vecchio non disegna più e non si può copiare niente da lì. Si smette di disegnare
// (render) e un thread crea un device nuovo: la GPU, o WARP se la GPU non risponde. Poi si
// ricostruisce come all'avvio: glifi e icone si rifanno al primo uso, anteprima e miniature
// tornano dalle copie in RAM. Il thread UI non aspetta la creazione del device.
constexpr UINT kGpuRetryMs = 1000; // né GPU né WARP: si riprova

static DWORD WINAPI gpuRecoverThread(void*) {
    bool ok = g_gpuPref != GPU_WARP && gfxCreateDevice(g_hw, false);
    if (!ok) ok = gfxCreateDevice(g_hw, true);
    g_hwOk = ok;
    HWND h = (HWND)InterlockedCompareExchangePointer(&g_mainHwnd, nullptr, nullptr);
    if (h) PostMessageW(h, WM_APP_GPU, 0, 0);
    return 0;
}

static void gpuRecoverStart() {
    HANDLE h = CreateThread(nullptr, 0, gpuRecoverThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
    else SetTimer(A.hwnd, 13, kGpuRetryMs, nullptr);
}

static void gpuLost(HRESULT why) {
    if (A.gpuLost) return;
    A.gpuLost = true;
    hitchLog("GPU persa", (unsigned)why, 0);
    if (g_gpuPref == GPU_HYBRID && !A.switched) { // prima del passaggio: la GPU dell'avvio può già servire
        if (!InterlockedCompareExchange(&g_hwDone, 0, 0)) return; // la sta creando: il suo WM_APP_GPU ripristina
        if (g_hwOk && g_hw.dev) { PostMessageW(A.hwnd, WM_APP_GPU, 0, 0); return; }
    }
    gpuRecoverStart();
}

static void gpuRecovered() {
    if (!g_hwOk || !g_hw.dev) { gfxRelease(g_hw); SetTimer(A.hwnd, 13, kGpuRetryMs, nullptr); return; }
    double t0 = nowMs();
    HWND v2 = createView(A.hwnd);
    if (!v2 || !gfxCreateSwapchain(g_hw, v2, A.hwnd)) {
        if (v2) DestroyWindow(v2);
        gfxRelease(g_hw);
        SetTimer(A.hwnd, 13, kGpuRetryMs, nullptr);
        return;
    }
    Gfx old = G;
    G = g_hw;
    g_hw = Gfx();
    A.switched = true; // in modalità ibrida il passaggio non serve più
    glyphReset();
    iconsForget();
    previewUpload();
    thumbsDeviceChanged();
    HWND oldView = A.viewWnd;
    A.viewWnd = v2;
    gfxRelease(old);
    if (oldView) DestroyWindow(oldView);
    A.gpuLost = false;
    A.firstGpuPresent = true;
    A.gpuRecoveries++;
    hitchLog("GPU ripristinata", (unsigned)A.gpuRecoveries, nowMs() - t0);
    A.dirty = true;
}
