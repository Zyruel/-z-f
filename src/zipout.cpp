// [z]f — dalla lista di uno zip verso fuori: aprire, copiare e trascinare estraggono su un thread
// (zipJobThread, zip.cpp) in una cartella temporanea di [z]f, poi usano i file estratti come quelli
// veri: si aprono con ShellExecute, vanno negli appunti come CF_HDROP (Incolla in [z]f e in Esplora
// file), si trascinano come percorsi.
//
// Il trascinamento parte quando i file ci sono: oltre la soglia di trascinamento si estrae, col
// tasto ancora premuto, e appena l'estrazione finisce parte il trascinamento di Windows. Rilasciando
// prima, non parte niente (e l'estrazione si ferma). Di solito sono pochi millisecondi.

static int g_zipRunning = 0;          // estrazioni in corso: dopo LOADING_AFTER l'avviso "Estrazione…"
static uint64_t g_zipGen = 0;
static const wchar_t kZipBusy[] = L"Estrazione dall'archivio\x2026";

// Estrae le voci indicate della cartella aperta (dentro uno zip). Ritorna la generazione (0: non partita)
static uint64_t zipJobStart(int kind, std::vector<uint32_t> items) {
    if (!A.cur || !A.cur->zipLen || items.empty()) return 0;
    ZipJob* j = new ZipJob;
    j->hwnd = A.hwnd;
    j->kind = kind;
    j->gen = ++g_zipGen;
    j->archive = A.cur->path.substr(0, A.cur->zipLen);
    for (uint32_t ei : items) j->items.push_back(joinPath(A.cur->path, A.cur->name(A.cur->e[ei])));
    j->firstAttr = A.cur->e[items[0]].attr;
    j->tab = A.tabs[(size_t)A.tab].id;
    j->from = A.cur->path;
    InterlockedExchange64(&g_zipLatest[kind], (LONG64)j->gen);
    HANDLE h = CreateThread(nullptr, 0, zipJobThread, j, 0, nullptr);
    if (!h) { delete j; return 0; }
    CloseHandle(h);
    if (g_zipRunning++ == 0) SetTimer(A.hwnd, 14, ui::LOADING_AFTER, nullptr);
    return j->gen;
}

// Timer 14: l'estrazione dura; l'avviso resta finché non finisce
static void zipOnTimer() {
    KillTimer(A.hwnd, 14);
    if (g_zipRunning <= 0) return;
    notice(kZipBusy, true);
    SetTimer(A.hwnd, 14, ui::NOTICE_RENEW, nullptr);
}

static const wchar_t* zipWhy(int err) {
    switch (err) {
    case ZE_CRYPT:  return L"è cifrato";
    case ZE_METHOD: return L"usa una compressione che [z]f non conosce";
    case ZE_DATA:   return L"è danneggiato";
    default:        return L"errore di lettura o di scrittura";
    }
}

static std::wstring leafName(const std::wstring& p) { return p.substr(p.find_last_of(L'\\') + 1); }

// Voci non estratte: il primo motivo, e quante altre
static void zipFailNotice(const ZipJob* j, const std::wstring& before) {
    std::wstring t = before + L"Impossibile estrarre " + j->errName + L": " + zipWhy(j->err);
    if (j->failed > 1) t += L" (e altri " + std::to_wstring(j->failed - 1) + L")";
    notice(t, false);
}

// Trascinamento: il tasto è ancora premuto davvero (lo stato della coda e quello del mouse fisico,
// che con i tasti scambiati è l'altro)
static bool zipDragButtonDown() {
    int vk = DD.armButton == MK_RBUTTON ? VK_RBUTTON : VK_LBUTTON;
    int phys = (vk == VK_LBUTTON) != (GetSystemMetrics(SM_SWAPBUTTON) != 0) ? VK_LBUTTON : VK_RBUTTON;
    return GetKeyState(vk) < 0 && (GetAsyncKeyState(phys) & 0x8000) != 0;
}

// Il tasto è stato rilasciato (o il mouse preso da altri) prima della fine dell'estrazione
static void zipDragCancel() {
    if (!DD.zipWait) return;
    DD.zipWait = 0;
    InterlockedExchange64(&g_zipLatest[ZJ_DRAG], 0); // il thread smette di estrarre
    if (GetCapture() == A.hwnd) ReleaseCapture();
}

// Fine di un'estrazione (WM_APP_ZIP)
static void onZipJob(ZipJob* j) {
    if (--g_zipRunning <= 0) {
        g_zipRunning = 0;
        KillTimer(A.hwnd, 14);
        if (A.error == kZipBusy) A.error.clear();
    }
    A.zipMs = j->ms;
    A.zipBytes = j->bytes;
    A.dirty = true;
    bool latest = (uint64_t)InterlockedCompareExchange64(&g_zipLatest[j->kind], 0, 0) == j->gen;
    std::wstring first = j->items.empty() ? L"" : leafName(j->items[0]);
    if (j->openErr) {
        if (j->kind != ZJ_DRAG || j->gen == DD.zipWait) {
            wchar_t b[64];
            _snwprintf(b, 63, L" (codice %lu)", (unsigned long)j->openErr);
            b[63] = 0;
            notice(L"Impossibile leggere l'archivio " + leafName(j->archive) + b, false);
        }
        if (j->kind == ZJ_DRAG && j->gen == DD.zipWait) zipDragCancel();
        delete j;
        return;
    }
    switch (j->kind) {
    case ZJ_OPEN:
        if (!j->out.empty()) {
            // Uno zip nello zip si apre qui, ma solo se è l'ultima richiesta e l'utente è ancora dove
            // l'ha chiesto: dopo un'estrazione lunga non lo si porta via da dove è andato nel
            // frattempo (la copia estratta resta per ZIP_TEMP_KEEP; il prossimo doppio clic la rifà).
            // Un file invece si apre sempre, anche se superato: ogni doppio clic è una richiesta,
            // come fuori dagli zip, e la cartella in cui si è non conta per un programma esterno.
            if (zipNameEnd(j->out[0], j->out[0].size())) {
                if (latest && A.tabs[(size_t)A.tab].id == j->tab && A.cur && !A.loading && A.cur->path == j->from) navigate(j->out[0], 1, L"");
            } else if (HANDLE h = CreateThread(nullptr, 0, openThread, _wcsdup(j->out[0].c_str()), 0, nullptr)) CloseHandle(h);
        } else if (j->err == ZE_CRYPT || j->err == ZE_METHOD) { // lo lasciamo a Windows: Esplora file, sulla voce
            notice(first + L" " + zipWhy(j->err) + L": si apre in Esplora file", true);
            wchar_t win[MAX_PATH];
            UINT n = GetWindowsDirectoryW(win, MAX_PATH);
            if (n && n < MAX_PATH) {
                std::wstring exe = std::wstring(win) + L"\\explorer.exe";
                std::wstring args = L"\"" + exe + L"\" /select,\"" + j->items[0] + L"\"";
                // Su un thread, come "Nuova finestra": creare un processo può costare decine di ms
                if (HANDLE h = CreateThread(nullptr, 0, newWindowThread, new std::wstring[2]{ exe, args }, 0, nullptr)) CloseHandle(h);
            }
        } else if (!j->cancelled) {
            zipFailNotice(j, L"");
        }
        break;
    case ZJ_COPY:
        if (!latest) break; // una copia più recente la sostituisce
        if (!j->out.empty() && clipSetPaths(j->out, false)) {
            std::wstring done = j->out.size() == 1 ? L"Estratto e copiato: " + leafName(j->out[0])
                                                   : L"Estratti e copiati " + std::to_wstring(j->out.size()) + L" elementi";
            if (j->failed) zipFailNotice(j, done + L". ");
            else notice(done, true);
        } else if (j->failed) {
            zipFailNotice(j, L"");
        }
        break;
    case ZJ_DRAG: {
        if (j->gen != DD.zipWait) break; // rilasciato prima: niente trascinamento
        bool down = zipDragButtonDown();
        DD.zipWait = 0;
        if (GetCapture() == A.hwnd) ReleaseCapture();
        if (j->failed) zipFailNotice(j, L"");
        if (!down || j->out.empty()) break;
        IDataObject* obj = dragObjectFor(j->out, j->firstAttr);
        if (!obj) break;
        dragRun(obj, DROPEFFECT_COPY); // copie estratte: si copiano, non si spostano
        break;
    }
    }
    delete j;
}
