// [z]f — integrazione con Windows: [z]f al posto di Explorer per aprire cartelle e unità.
//
// Tutto in HKCU (solo l'utente corrente, nessun permesso di amministratore):
//   Classes\Directory\shell, Classes\Drive\shell  → verbo "z-f" reso predefinito
//       (doppio clic sulle cartelle del desktop, programmi che aprono una cartella con ShellExecute)
//   Classes\CLSID\{52205fd8-…}\shell\opennewwindow\command → Win+E e avvio di explorer.exe
// Il verbo "open" di Windows non viene toccato, né la classe Folder (Cestino, Pannello di
// controllo e simili restano a Explorer). Prima di scrivere si salvano i valori che c'erano in
// HKCU\Software\z-f\Ripristino; disattivando si torna esattamente allo stato precedente.
// Non sostituisce: i dialoghi Apri/Salva, e "Mostra nella cartella" dei programmi che parlano
// direttamente con Explorer (SHOpenFolderAndSelectItems).

static const wchar_t* kVerb = L"z-f";
static const wchar_t* kWinEKey = L"Software\\Classes\\CLSID\\{52205fd8-5dfb-447d-801a-d0b52f2e83e1}";
static const wchar_t* kWinECmd = L"Software\\Classes\\CLSID\\{52205fd8-5dfb-447d-801a-d0b52f2e83e1}\\shell\\opennewwindow\\command";
static const wchar_t* kBackup = L"Software\\z-f\\Ripristino";
static const wchar_t* kShellKeys[2] = { L"Software\\Classes\\Directory\\shell", L"Software\\Classes\\Drive\\shell" };
static const wchar_t* kBackupNames[2] = { L"Directory", L"Drive" };

// Percorso di questo eseguibile, anche oltre MAX_PATH; "" se non si riesce a leggerlo
static std::wstring exePath() {
    std::wstring b(MAX_PATH, L'\0');
    for (int i = 0; i < 6; i++) { // fino a 32.768 caratteri, il massimo di un percorso
        DWORD n = GetModuleFileNameW(nullptr, &b[0], (DWORD)b.size());
        if (!n) return L"";
        if (n < b.size()) { b.resize(n); return b; }
        b.resize(b.size() * 2);
    }
    return L"";
}

static bool regGet(const wchar_t* sub, const wchar_t* name, std::wstring& out) {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, sub, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS) return false;
    std::wstring b(size / sizeof(wchar_t) + 1, L'\0');
    size = (DWORD)(b.size() * sizeof(wchar_t));
    if (RegGetValueW(HKEY_CURRENT_USER, sub, name, RRF_RT_REG_SZ, nullptr, &b[0], &size) != ERROR_SUCCESS) return false;
    out.assign(b.c_str());
    return true;
}

static bool regSet(const wchar_t* sub, const wchar_t* name, const std::wstring& v) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, sub, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    LONG r = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

static std::wstring zfCommand(bool withArg) {
    std::wstring c = L"\"" + exePath() + L"\"";
    return withArg ? c + L" \"%1\"" : c;
}

// Transizione dal vecchio nome (zx, fino alla 0.15): l'integrazione scritta da zx si toglie,
// rimettendo i valori che c'erano prima, prima di scrivere o togliere quella di [z]f.
static void cleanupLegacyZx() {
    const wchar_t* oldBackup = L"Software\\zx\\Ripristino";
    std::wstring v;
    bool wasOn = regGet(kShellKeys[0], nullptr, v) && v == L"zx";
    for (int i = 0; i < 2; i++) {
        std::wstring verb = std::wstring(kShellKeys[i]) + L"\\zx";
        RegDeleteTreeW(HKEY_CURRENT_USER, verb.c_str());
        RegDeleteKeyW(HKEY_CURRENT_USER, verb.c_str());
        std::wstring cur, prev;
        if (regGet(kShellKeys[i], nullptr, cur) && cur == L"zx") {
            if (regGet(oldBackup, kBackupNames[i], prev)) regSet(kShellKeys[i], nullptr, prev);
            else RegDeleteKeyValueW(HKEY_CURRENT_USER, kShellKeys[i], nullptr);
        }
    }
    if (wasOn) {
        std::wstring had, cmd, de;
        if (regGet(oldBackup, L"WinE", had)) {
            RegDeleteKeyValueW(HKEY_CURRENT_USER, kWinECmd, nullptr);
            RegDeleteKeyValueW(HKEY_CURRENT_USER, kWinECmd, L"DelegateExecute");
            if (regGet(oldBackup, L"WinE.command", cmd)) regSet(kWinECmd, nullptr, cmd);
            if (regGet(oldBackup, L"WinE.DelegateExecute", de)) regSet(kWinECmd, L"DelegateExecute", de);
        } else {
            RegDeleteTreeW(HKEY_CURRENT_USER, kWinEKey);
            RegDeleteKeyW(HKEY_CURRENT_USER, kWinEKey);
        }
    }
    RegDeleteTreeW(HKEY_CURRENT_USER, oldBackup);
    RegDeleteKeyW(HKEY_CURRENT_USER, oldBackup);
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\zx"); // solo se vuota
}

// 0 = Explorer, 1 = [z]f (questo eseguibile), 2 = [z]f ma un altro eseguibile (es. z-f-dev, o il vecchio zx)
static int defaultState() {
    std::wstring v, cmd;
    if (regGet(kShellKeys[0], nullptr, v) && v == L"zx") return 2; // integrazione del vecchio zx
    if (!regGet(kShellKeys[0], nullptr, v) || v != kVerb) return 0;
    std::wstring sub = std::wstring(kShellKeys[0]) + L"\\z-f\\command";
    if (!regGet(sub.c_str(), nullptr, cmd)) return 0;
    return _wcsicmp(cmd.c_str(), zfCommand(true).c_str()) == 0 ? 1 : 2;
}

static void notifyAssocChanged() {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr); // Explorer rilegge le associazioni
}

static bool setDefault(bool on) {
    // Senza il percorso dell'exe il comando sarebbe "" "%1": tutte le cartelle smetterebbero di aprirsi
    if (on && exePath().empty()) return false;
    bool ok = true;
    cleanupLegacyZx();
    if (on) {
        bool already = defaultState() != 0; // già attivo (anche da un altro exe): il backup c'è già
        for (int i = 0; i < 2; i++) {
            std::wstring prev;
            if (!already && regGet(kShellKeys[i], nullptr, prev) && prev != kVerb) regSet(kBackup, kBackupNames[i], prev);
            std::wstring verb = std::wstring(kShellKeys[i]) + L"\\z-f";
            ok &= regSet(verb.c_str(), nullptr, L"Apri in [z]f");
            ok &= regSet((verb + L"\\command").c_str(), nullptr, zfCommand(true));
            ok &= regSet(kShellKeys[i], nullptr, kVerb);
        }
        if (!already) {
            HKEY k;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, kWinEKey, 0, KEY_READ, &k) == ERROR_SUCCESS) { // personalizzato da prima
                RegCloseKey(k);
                std::wstring cmd, de;
                regSet(kBackup, L"WinE", L"1");
                if (regGet(kWinECmd, nullptr, cmd)) regSet(kBackup, L"WinE.command", cmd);
                if (regGet(kWinECmd, L"DelegateExecute", de)) regSet(kBackup, L"WinE.DelegateExecute", de);
            }
        }
        ok &= regSet(kWinECmd, nullptr, zfCommand(false));
        ok &= regSet(kWinECmd, L"DelegateExecute", L""); // vuoto: Windows esegue il comando invece di Explorer
    } else {
        bool wasOn = defaultState() != 0;
        for (int i = 0; i < 2; i++) {
            std::wstring verb = std::wstring(kShellKeys[i]) + L"\\z-f";
            RegDeleteTreeW(HKEY_CURRENT_USER, verb.c_str());
            RegDeleteKeyW(HKEY_CURRENT_USER, verb.c_str());
        }
        // Se [z]f non era attivo, il resto appartiene a qualcun altro (un altro file manager): non si tocca
        if (!wasOn) { notifyAssocChanged(); return true; }
        for (int i = 0; i < 2; i++) {
            std::wstring prev;
            if (regGet(kBackup, kBackupNames[i], prev)) regSet(kShellKeys[i], nullptr, prev);
            else RegDeleteKeyValueW(HKEY_CURRENT_USER, kShellKeys[i], nullptr); // il valore predefinito non c'era
        }
        std::wstring had, cmd, de;
        if (regGet(kBackup, L"WinE", had)) {
            RegDeleteKeyValueW(HKEY_CURRENT_USER, kWinECmd, nullptr);
            RegDeleteKeyValueW(HKEY_CURRENT_USER, kWinECmd, L"DelegateExecute");
            if (regGet(kBackup, L"WinE.command", cmd)) regSet(kWinECmd, nullptr, cmd);
            if (regGet(kBackup, L"WinE.DelegateExecute", de)) regSet(kWinECmd, L"DelegateExecute", de);
        } else {
            RegDeleteTreeW(HKEY_CURRENT_USER, kWinEKey); // non esisteva: si toglie tutto
            RegDeleteKeyW(HKEY_CURRENT_USER, kWinEKey);
        }
        RegDeleteTreeW(HKEY_CURRENT_USER, kBackup);
        RegDeleteKeyW(HKEY_CURRENT_USER, kBackup);
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\z-f"); // solo se vuota
        ok = defaultState() == 0;
    }
    notifyAssocChanged();
    return ok;
}

// Scrittura del registro e SHChangeNotify (che avvisa Explorer e può aspettarlo) su un thread
static bool g_defaultBusy = false;

static DWORD WINAPI defaultThread(void* p) {
    bool on = p != nullptr;
    double t0 = nowMs();
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); // SHChangeNotify
    bool ok = setDefault(on);
    if (SUCCEEDED(co)) CoUninitialize();
    hitchLog("al posto di Explorer", on, nowMs() - t0); // raro: il tempo si registra sempre
    PostMessageW(A.hwnd, WM_APP_DEFAULT, on, ok);
    return 0;
}

static void toggleDefault() {
    if (g_defaultBusy) return;
    bool on = defaultState() != 1; // lettura del registro: microsecondi
    HANDLE h = CreateThread(nullptr, 0, defaultThread, on ? (void*)1 : nullptr, 0, nullptr);
    if (!h) { notice(L"Impossibile modificare il registro", false); return; }
    CloseHandle(h);
    g_defaultBusy = true;
}

// Esito di toggleDefault (WM_APP_DEFAULT)
static void onDefaultDone(bool on, bool ok) {
    g_defaultBusy = false;
    if (!ok) notice(L"Impossibile modificare il registro", false);
    else notice(on ? L"[z]f apre cartelle e unità al posto di Explorer (anche Win+E)" : L"Ripristinato Explorer", true);
}
