// [z]f — impostazioni e salvataggio dei file di [z]f in %APPDATA%\z-f.
//
// impostazioni.txt: righe "chiave=valore" (UTF-8), modificabili a mano.
//   dimensione=13          dimensione del testo in pixel a scala 100% (Ctrl+rotellina, Ctrl +/−, Ctrl+0)
//   carattere=Segoe UI     famiglia del testo (se non è installata si torna a Segoe UI)
//   barra_laterale=1       Ctrl+B
//   barra_laterale_larghezza=210  in pixel a scala 100% (trascinando il bordo; doppio clic: predefinita)
//   barra_tempi=0          1: sopra la barra di stato, i tempi misurati (lettura, ordinamento, frame…)
//   animazioni=1           0: niente animazioni (tutto cambia di colpo)
//   colonna_data=150       larghezza delle colonne Data e Dimensione in pixel a scala 100%
//   colonna_dimensione=100 (trascinando il bordo nell'intestazione; doppio clic: adatta al contenuto)
//   colonna_destinazione=300  colonna Destinazione, nelle cartelle con collegamenti (.lnk)
//   anteprima=0            pannello di anteprima (Alt+P)
//   anteprima_larghezza=360  in pixel a scala 100% (trascinando il bordo; doppio clic: predefinita)
//   carattere_anteprima=Cascadia Mono  testo nell'anteprima (se non è installato: Cascadia Mono, Consolas)
//   annulla_dopo_riavvio=0 1: la cronologia di Ctrl+Z si salva in annulla.txt e si ritrova al riavvio
//   menu_completo=0        1: il tasto destro apre il menu completo della shell (Shift quello di [z]f)
//   dimensione_cartelle=1  0: la colonna Dimensione non conta le cartelle (tooltip e anteprima sì)
//   dialoghi=1             0: i dialoghi Apri e Salva degli altri programmi non saltano alla cartella di [z]f
//   griglia_dimensione=1  miniature della griglia: 0 piccole, 1 medie, 2 grandi (la vista è per cartella, in viste.txt)
//   riapri_schede=0        1: la finestra principale riapre all'avvio le schede che aveva alla chiusura (schede.txt)
//   aggiornamenti=1        0: niente controllo automatico degli aggiornamenti (≡ › versione controlla lo stesso)
//   aggiornamenti_beta=1   anche le beta; senza la riga: sì se la versione in uso è una beta
//   aggiornamenti_ultimo=0 ultimo controllo degli aggiornamenti (secondi dal 1970): uno al giorno
// Si leggono all'avvio sul thread veloce della barra laterale (il thread UI non tocca il disco)
// e si salvano su un thread a parte, scrivendo un file temporaneo e rinominandolo.

static std::wstring zfDataPath(const wchar_t* name) {
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    if (!n || n >= MAX_PATH) return L"";
    return std::wstring(buf) + L"\\z-f\\" + name;
}

static std::string toUtf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    if (n) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

// Transizione dal vecchio nome: %APPDATA%\zx (fino alla 0.15) diventa %APPDATA%\z-f.
// Si chiama dal thread d'avvio, prima di leggere preferiti e impostazioni.
static void migrateLegacyData() {
    std::wstring nw = zfDataPath(L"");
    if (nw.size() < 4) return;
    nw.pop_back();                                      // ...\z-f
    std::wstring old = nw.substr(0, nw.size() - 3) + L"zx"; // ...\zx
    if (GetFileAttributesW(nw.c_str()) == INVALID_FILE_ATTRIBUTES && GetFileAttributesW(old.c_str()) != INVALID_FILE_ATTRIBUTES)
        MoveFileW(old.c_str(), nw.c_str());
}

// Righe di un file UTF-8 piccolo di %APPDATA%\z-f (senza fine riga e spazi finali). false se non esiste.
static bool readDataLines(const wchar_t* name, std::vector<std::wstring>& out) {
    std::wstring fp = zfDataPath(name);
    if (fp.empty()) return false;
    HANDLE h = CreateFileW(fp.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::string data;
    LARGE_INTEGER sz;
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart < (1 << 20)) {
        data.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        if (!ReadFile(h, &data[0], (DWORD)data.size(), &got, nullptr)) got = 0;
        data.resize(got);
    }
    CloseHandle(h);
    size_t i = (data.size() >= 3 && (unsigned char)data[0] == 0xEF) ? 3 : 0; // BOM
    while (i < data.size()) {
        size_t e = data.find('\n', i);
        if (e == std::string::npos) e = data.size();
        size_t len = e - i;
        while (len && (data[i + len - 1] == '\r' || data[i + len - 1] == ' ')) len--;
        if (len) {
            int n = MultiByteToWideChar(CP_UTF8, 0, data.data() + i, (int)len, nullptr, 0);
            std::wstring w(n, 0);
            MultiByteToWideChar(CP_UTF8, 0, data.data() + i, (int)len, &w[0], n);
            out.push_back(std::move(w));
        }
        i = e + 1;
    }
    return true;
}

// --- salvataggio asincrono: per ogni file vale l'ultima versione richiesta ----------------------
// Due lock: g_saveLock protegge solo la coda (il thread UI lo tiene per uno scambio di puntatori),
// g_writeLock tiene in ordine le scritture (mai due insieme). Con un lock solo, tenuto durante
// CreateFile/WriteFile/MoveFile, un disco lento o l'antivirus bloccavano il thread UI al salvataggio successivo.
static CRITICAL_SECTION g_saveLock, g_writeLock;
struct PendingWrite { std::wstring name; std::string data; };
static std::vector<PendingWrite> g_pendingWrites;

static DWORD WINAPI saveThread(void*) {
    EnterCriticalSection(&g_writeLock);
    std::vector<PendingWrite> todo;
    EnterCriticalSection(&g_saveLock);
    todo.swap(g_pendingWrites);
    LeaveCriticalSection(&g_saveLock);
    for (const PendingWrite& w : todo) {
        std::wstring fp = zfDataPath(w.name.c_str());
        if (fp.empty()) continue;
        CreateDirectoryW(fp.substr(0, fp.find_last_of(L'\\')).c_str(), nullptr);
        std::wstring tmp = fp + L".tmp";
        HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;
        DWORD n = 0;
        bool ok = WriteFile(h, w.data.data(), (DWORD)w.data.size(), &n, nullptr) && n == w.data.size();
        CloseHandle(h);
        if (ok) MoveFileExW(tmp.c_str(), fp.c_str(), MOVEFILE_REPLACE_EXISTING); // mai un file a metà
        else DeleteFileW(tmp.c_str());
    }
    LeaveCriticalSection(&g_writeLock);
    return 0;
}

// All'uscita, dopo il ciclo dei messaggi (la finestra non c'è più, nessuno aspetta): il ritorno da
// wWinMain chiuderebbe il processo con i thread di salvataggio appena partiti, e l'ultima modifica
// (un'operazione sui file finita mentre la finestra si chiudeva) andrebbe persa.
static void saveFlush() {
    saveThread(nullptr); // g_writeLock aspetta la scrittura in corso; poi scrive quello che resta
}

static void saveDataAsync(const wchar_t* name, std::string data) {
    EnterCriticalSection(&g_saveLock);
    bool found = false;
    for (PendingWrite& w : g_pendingWrites)
        if (w.name == name) { w.data.swap(data); found = true; break; }
    if (!found) g_pendingWrites.push_back(PendingWrite{ name, std::move(data) });
    LeaveCriticalSection(&g_saveLock);
    HANDLE h = CreateThread(nullptr, 0, saveThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}

// --- impostazioni -------------------------------------------------------------------------

static void settingsSave() {
    std::string d;
    d += "dimensione=" + std::to_string(F.px) + "\r\n";
    d += "carattere=" + toUtf8(F.face) + "\r\n";
    d += std::string("barra_laterale=") + (A.sideVisible ? "1" : "0") + "\r\n";
    d += "barra_laterale_larghezza=" + std::to_string(lroundf(A.sideWidth)) + "\r\n";
    d += std::string("barra_tempi=") + (A.timesBar ? "1" : "0") + "\r\n";
    d += std::string("animazioni=") + (A.animations ? "1" : "0") + "\r\n";
    d += std::string("preferiti_aperti=") + (A.favOpen ? "1" : "0") + "\r\n";
    d += std::string("unita_aperte=") + (A.drvOpen ? "1" : "0") + "\r\n";
    d += "colonna_data=" + std::to_string(lroundf(A.colDate)) + "\r\n";
    d += "colonna_dimensione=" + std::to_string(lroundf(A.colSize)) + "\r\n";
    d += "colonna_destinazione=" + std::to_string(lroundf(A.colLink)) + "\r\n";
    d += std::string("anteprima=") + (A.prevOpen ? "1" : "0") + "\r\n";
    d += "anteprima_larghezza=" + std::to_string(lroundf(A.prevWidth)) + "\r\n";
    d += "carattere_anteprima=" + toUtf8(FM.face) + "\r\n";
    d += std::string("annulla_dopo_riavvio=") + (A.undoPersist ? "1" : "0") + "\r\n";
    d += std::string("menu_completo=") + (A.shellMenu ? "1" : "0") + "\r\n";
    d += std::string("dimensione_cartelle=") + (A.dirSizes ? "1" : "0") + "\r\n";
    d += std::string("dialoghi=") + (A.dialogJump ? "1" : "0") + "\r\n";
    d += "griglia_dimensione=" + std::to_string(A.thumbSize) + "\r\n";
    d += std::string("riapri_schede=") + (A.reopenTabs ? "1" : "0") + "\r\n";
    d += std::string("aggiornamenti=") + (A.updAuto ? "1" : "0") + "\r\n";
    if (A.updBeta >= 0) d += std::string("aggiornamenti_beta=") + (A.updBeta ? "1" : "0") + "\r\n";
    d += "aggiornamenti_ultimo=" + std::to_string(A.updLast) + "\r\n";
    saveDataAsync(L"impostazioni.txt", std::move(d));
}

// Applica le righe lette all'avvio. true se il carattere è cambiato (va ricreato)
static bool settingsApply(const std::vector<std::wstring>& lines) {
    int px = F.px;
    std::wstring face = F.face;
    for (const std::wstring& l : lines) {
        size_t eq = l.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = l.substr(0, eq), v = l.substr(eq + 1);
        if (k == L"dimensione") { int n = _wtoi(v.c_str()); if (n >= ui::FONT_MIN && n <= ui::FONT_MAX) px = n; }
        else if (k == L"carattere" && !v.empty() && v.size() < LF_FACESIZE) face = v;
        else if (k == L"barra_laterale") A.sideVisible = v != L"0";
        else if (k == L"barra_laterale_larghezza") {
            int n = _wtoi(v.c_str()); // il massimo lo decide la finestra (layout)
            if (n >= (int)ui::SIDE_MIN && n <= 8000) A.sideWidth = (float)n;
        }
        else if (k == L"barra_tempi") A.timesBar = v == L"1";
        else if (k == L"animazioni") A.animations = v != L"0";
        else if (k == L"preferiti_aperti") A.favOpen = v != L"0";
        else if (k == L"unita_aperte") A.drvOpen = v != L"0";
        else if (k == L"colonna_data" || k == L"colonna_dimensione" || k == L"colonna_destinazione") {
            int n = _wtoi(v.c_str()); // il massimo lo decide la finestra (layout), qui solo i valori assurdi
            if (n >= (int)ui::COL_MIN && n <= 4000)
                (k == L"colonna_data" ? A.colDate : k == L"colonna_dimensione" ? A.colSize : A.colLink) = (float)n;
        }
        else if (k == L"anteprima") A.prevOpen = v != L"0";
        else if (k == L"anteprima_larghezza") {
            int n = _wtoi(v.c_str()); // il massimo lo decide la finestra (layout)
            if (n >= (int)ui::PREVIEW_MIN && n <= 8000) A.prevWidth = (float)n;
        }
        else if (k == L"annulla_dopo_riavvio") A.undoPersist = v == L"1";
        else if (k == L"menu_completo") A.shellMenu = v == L"1";
        else if (k == L"dimensione_cartelle") A.dirSizes = v != L"0";
        else if (k == L"dialoghi") A.dialogJump = v != L"0";
        else if (k == L"griglia_dimensione") { int n = _wtoi(v.c_str()); if (n >= 0 && n <= 2) A.thumbSize = n; }
        else if (k == L"riapri_schede") A.reopenTabs = v == L"1";
        else if (k == L"aggiornamenti") A.updAuto = v != L"0";
        else if (k == L"aggiornamenti_beta") A.updBeta = v == L"1" ? 1 : v == L"0" ? 0 : -1;
        else if (k == L"aggiornamenti_ultimo") A.updLast = _wtoi64(v.c_str());
        else if (k == L"carattere_anteprima" && !v.empty() && v.size() < LF_FACESIZE && v != FM.face) { FM.face = v; monoReset(); }
    }
    bool changed = px != F.px || face != F.face;
    F.px = px;
    F.face = face;
    return changed;
}

// Ctrl+rotellina, Ctrl +/−: dimensione del testo; delta 0 = predefinita
static void fontSizeStep(int delta) {
    int px = delta ? std::min(ui::FONT_MAX, std::max(ui::FONT_MIN, F.px + delta)) : ui::FONT_PX;
    if (px == F.px) return;
    int first = firstVisibleItem(); // resta in vista la stessa voce
    F.px = px;
    fontCreate(A.scale);
    layout();
    scrollToItem(first);
    clampScroll();
    wchar_t b[48];
    _snwprintf(b, 47, L"Testo %d px%s", px, px == ui::FONT_PX ? L" (predefinito)" : L"");
    b[47] = 0;
    notice(b, true);
    settingsSave();
    A.dirty = true;
}
