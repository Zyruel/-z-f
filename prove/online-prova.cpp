// Prova dei file solo online (OneDrive e simili) senza OneDrive: una radice di sincronizzazione
// finta con l'API Cloud Files di Windows (cfapi, la stessa di OneDrive) e dei segnaposto di file
// veri. Ogni volta che qualcuno ne legge il contenuto Windows chiede i dati al "fornitore", cioè a
// questo programma: la richiesta si conta, con il processo che l'ha fatta, e si serve con il file
// vero, come farebbe OneDrive scaricandolo.
//
// Due modi:
//   passi    per ogni file, le chiamate che [z]f fa sul suo thread, ognuna su un segnaposto suo
//            (un file già scaricato non direbbe più niente): attributi (readInfo), icona della
//            lista (icons.cpp, ICON_REAL), icona grande (pvBigIcon), lettura dell'intestazione
//            (imageSize; questo deve scaricare: prova che il contatore funziona);
//   attendi  crea i segnaposto e resta in ascolto per N secondi: si apre la cartella con
//            prove\dev.bat e si passa col mouse, si apre l'anteprima, la griglia. Alla fine
//            l'elenco delle richieste per processo.
// Alla fine la radice si toglie (CfUnregisterSyncRoot); la cartella resta, con i file scaricati.
// Niente registro: la radice non si registra presso la shell (StorageProviderSyncRootManager).
//
// Compilazione (dalla cartella del progetto, con l'ambiente di Visual Studio):
//   cl /nologo /std:c++17 /utf-8 /EHsc /W3 /O2 /MT /DUNICODE /D_UNICODE /Fe:build\online-prova.exe prove\online-prova.cpp cldapi.lib shell32.lib ole32.lib user32.lib
// Uso:
//   build\online-prova.exe passi <cartella nuova> [file…]
//   build\online-prova.exe attendi <cartella nuova> <secondi> [file…]
// Senza file: %windir%\regedit.exe, src\z-f.ico, il primo .lnk del menu Start comune, un .png di
// System32 e un .jpg di %windir%\Web\Screen.
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <cfapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commoncontrols.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <map>

// Dimensione dei parametri di un'operazione: l'SDK non la definisce (la scrivono gli esempi Microsoft)
#define CF_SIZE_OF_OP_PARAM(field) \
    (FIELD_OFFSET(CF_OPERATION_PARAMETERS, field) + RTL_FIELD_SIZE(CF_OPERATION_PARAMETERS, field))

static double freq;
static double nowMs() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart * 1000.0 / freq; }

// --- fornitore ------------------------------------------------------------------------------
static CRITICAL_SECTION g_lock;
static volatile LONG g_fetches = 0;
static std::map<std::wstring, int> g_byProc;   // processo → richieste
static std::vector<std::wstring> g_log;
static double g_t0;

static std::wstring baseName(const wchar_t* p) {
    if (!p) return L"?";
    const wchar_t* s = wcsrchr(p, L'\\');
    return s ? s + 1 : p;
}

// Windows chiede un intervallo del file: si manda tutto il file dall'inizio (è piccolo), così il
// segnaposto diventa un file intero come dopo un download di OneDrive
static void CALLBACK onFetch(const CF_CALLBACK_INFO* ci, const CF_CALLBACK_PARAMETERS* cp) {
    InterlockedIncrement(&g_fetches);
    std::wstring src((const wchar_t*)ci->FileIdentity, ci->FileIdentityLength / sizeof(wchar_t));
    std::wstring proc = ci->ProcessInfo ? baseName(ci->ProcessInfo->ImagePath) : L"?";
    wchar_t line[1024];
    _snwprintf(line, 1023, L"%8.0f ms  %-24s %s (da %lld, %lld byte)", nowMs() - g_t0, proc.c_str(),
               baseName(ci->NormalizedPath).c_str(), cp->FetchData.RequiredFileOffset.QuadPart, cp->FetchData.RequiredLength.QuadPart);
    EnterCriticalSection(&g_lock);
    g_byProc[proc]++;
    g_log.push_back(line);
    LeaveCriticalSection(&g_lock);
    wprintf(L"  scaricamento: %s\n", line);

    std::vector<char> data;
    HANDLE f = CreateFileW(src.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz;
        GetFileSizeEx(f, &sz);
        data.resize((size_t)sz.QuadPart);
        DWORD n = 0;
        if (!data.empty()) ReadFile(f, data.data(), (DWORD)data.size(), &n, nullptr);
        data.resize(n);
        CloseHandle(f);
    }
    CF_OPERATION_INFO op = {};
    op.StructSize = sizeof(op);
    op.Type = CF_OPERATION_TYPE_TRANSFER_DATA;
    op.ConnectionKey = ci->ConnectionKey;
    op.TransferKey = ci->TransferKey;
    op.RequestKey = ci->RequestKey;
    CF_OPERATION_PARAMETERS pa = {};
    pa.ParamSize = CF_SIZE_OF_OP_PARAM(TransferData);
    pa.TransferData.CompletionStatus = data.empty() ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS;
    pa.TransferData.Buffer = data.data();
    pa.TransferData.Offset.QuadPart = 0;
    pa.TransferData.Length.QuadPart = (LONGLONG)data.size();
    HRESULT hr = CfExecute(&op, &pa);
    if (FAILED(hr)) wprintf(L"  CfExecute: 0x%08lX\n", (unsigned long)hr);
}

static CF_CONNECTION_KEY g_key;
static std::wstring g_root;

static bool rootStart(const std::wstring& root) {
    if (!CreateDirectoryW(root.c_str(), nullptr)) {
        wprintf(L"La cartella deve essere nuova: %s (errore %lu)\n", root.c_str(), GetLastError());
        return false;
    }
    CF_SYNC_REGISTRATION reg = {};
    reg.StructSize = sizeof(reg);
    reg.ProviderName = L"z-f online-prova";
    reg.ProviderVersion = L"1";
    CF_SYNC_POLICIES pol = {};
    pol.StructSize = sizeof(pol);
    pol.Hydration.Primary = CF_HYDRATION_POLICY_FULL;
    pol.Population.Primary = CF_POPULATION_POLICY_ALWAYS_FULL;
    pol.InSync = CF_INSYNC_POLICY_NONE;
    pol.HardLink = CF_HARDLINK_POLICY_NONE;
    HRESULT hr = CfRegisterSyncRoot(root.c_str(), &reg, &pol, CF_REGISTER_FLAG_NONE);
    if (FAILED(hr)) { wprintf(L"CfRegisterSyncRoot: 0x%08lX\n", (unsigned long)hr); return false; }
    g_root = root;
    CF_CALLBACK_REGISTRATION cb[] = { { CF_CALLBACK_TYPE_FETCH_DATA, onFetch }, CF_CALLBACK_REGISTRATION_END };
    hr = CfConnectSyncRoot(root.c_str(), cb, nullptr,
                           CF_CONNECT_FLAG_REQUIRE_PROCESS_INFO | CF_CONNECT_FLAG_REQUIRE_FULL_FILE_PATH, &g_key);
    if (FAILED(hr)) { wprintf(L"CfConnectSyncRoot: 0x%08lX\n", (unsigned long)hr); return false; }
    return true;
}

static void rootStop() {
    if (g_key.Internal) CfDisconnectSyncRoot(g_key);
    if (!g_root.empty()) CfUnregisterSyncRoot(g_root.c_str());
}

// Un segnaposto "solo online" del file src, con il nome name, nella radice
static bool placeholder(const std::wstring& src, const std::wstring& name) {
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(src.c_str(), GetFileExInfoStandard, &fa)) return false;
    CF_PLACEHOLDER_CREATE_INFO pi = {};
    pi.RelativeFileName = name.c_str();
    pi.FsMetadata.FileSize.QuadPart = (LONGLONG)fa.nFileSizeHigh << 32 | fa.nFileSizeLow;
    auto t = [](FILETIME f) { LARGE_INTEGER l; l.LowPart = f.dwLowDateTime; l.HighPart = (LONG)f.dwHighDateTime; return l; };
    pi.FsMetadata.BasicInfo.CreationTime = t(fa.ftCreationTime);
    pi.FsMetadata.BasicInfo.LastAccessTime = t(fa.ftLastAccessTime);
    pi.FsMetadata.BasicInfo.LastWriteTime = t(fa.ftLastWriteTime);
    pi.FsMetadata.BasicInfo.ChangeTime = t(fa.ftLastWriteTime);
    pi.FsMetadata.BasicInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    pi.FileIdentity = src.c_str();
    pi.FileIdentityLength = (DWORD)((src.size() + 1) * sizeof(wchar_t));
    pi.Flags = CF_PLACEHOLDER_CREATE_FLAG_MARK_IN_SYNC;
    DWORD done = 0;
    HRESULT hr = CfCreatePlaceholders(g_root.c_str(), &pi, 1, CF_CREATE_FLAG_NONE, &done);
    if (FAILED(hr) || FAILED(pi.Result)) {
        wprintf(L"CfCreatePlaceholders %s: 0x%08lX\n", name.c_str(), (unsigned long)(FAILED(hr) ? hr : pi.Result));
        return false;
    }
    return true;
}

// Lo stesso controllo di [z]f (onlineOnly, app.cpp)
static bool onlineOnly(DWORD a) { return (a & (FILE_ATTRIBUTE_OFFLINE | 0x00400000 | 0x00040000)) != 0; }

// --- le chiamate di [z]f ---------------------------------------------------------------------
static void stepAttrs(const std::wstring& p) {
    WIN32_FILE_ATTRIBUTE_DATA fa;
    GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa);
}
static void stepListIcon(const std::wstring& p) { // icons.cpp, ICON_REAL (16 e 32 px)
    const wchar_t* dot = wcsrchr(p.c_str(), L'.');
    bool lnk = dot && (!_wcsicmp(dot, L".lnk") || !_wcsicmp(dot, L".url"));
    for (UINT sz : { (UINT)SHGFI_SMALLICON, (UINT)SHGFI_LARGEICON }) {
        SHFILEINFOW fi = {};
        if (SHGetFileInfoW(p.c_str(), 0, &fi, sizeof(fi), SHGFI_ICON | sz | (lnk ? SHGFI_LINKOVERLAY : 0)) && fi.hIcon) DestroyIcon(fi.hIcon);
    }
}
static void stepBigIcon(const std::wstring& p) { // preview.cpp, pvBigIcon con ICON_REAL
    SHFILEINFOW fi = {};
    if (!SHGetFileInfoW(p.c_str(), 0, &fi, sizeof(fi), SHGFI_SYSICONINDEX)) return;
    IImageList* il = nullptr;
    if (FAILED(SHGetImageList(SHIL_JUMBO, IID_IImageList, (void**)&il)) || !il) return;
    HICON hi = nullptr;
    il->GetIcon(fi.iIcon, ILD_TRANSPARENT, &hi);
    il->Release();
    if (hi) DestroyIcon(hi);
}
static void stepHead(const std::wstring& p) { // tooltip.cpp, imageSize prima della correzione
    HANDLE f = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    std::vector<char> b(256 * 1024);
    DWORD n = 0;
    ReadFile(f, b.data(), (DWORD)b.size(), &n, nullptr);
    CloseHandle(f);
}

struct Step { const wchar_t* tag; const wchar_t* what; void (*fn)(const std::wstring&); };
static const Step kSteps[] = {
    { L"attributi", L"GetFileAttributesEx (readInfo)", stepAttrs },
    { L"icona", L"SHGetFileInfo SHGFI_ICON (icone della lista)", stepListIcon },
    { L"grande", L"SHGetFileInfo + SHIL_JUMBO (pvBigIcon)", stepBigIcon },
    { L"inizio", L"CreateFile + ReadFile 256 KB (imageSize)", stepHead },
};

static std::wstring envPath(const wchar_t* s) { wchar_t b[MAX_PATH * 2]; ExpandEnvironmentStringsW(s, b, MAX_PATH * 2); return b; }
static std::wstring firstMatch(const std::wstring& pattern) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return L"";
    FindClose(h);
    return pattern.substr(0, pattern.rfind(L'\\') + 1) + fd.cFileName;
}
static std::wstring fullPath(const std::wstring& p) { wchar_t b[MAX_PATH * 2]; GetFullPathNameW(p.c_str(), MAX_PATH * 2, b, nullptr); return b; }

int wmain(int argc, wchar_t** argv) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    freq = (double)f.QuadPart;
    InitializeCriticalSection(&g_lock);
    if (argc < 3 || (wcscmp(argv[1], L"passi") && wcscmp(argv[1], L"attendi")) || (!wcscmp(argv[1], L"attendi") && argc < 4)) {
        wprintf(L"Uso: online-prova passi <cartella nuova> [file…]\n     online-prova attendi <cartella nuova> <secondi> [file…]\n");
        return 2;
    }
    bool wait = !wcscmp(argv[1], L"attendi");
    int secs = wait ? _wtoi(argv[3]) : 0;
    std::vector<std::wstring> src;
    for (int i = wait ? 4 : 3; i < argc; i++) src.push_back(fullPath(argv[i]));
    if (src.empty()) {
        src.push_back(envPath(L"%windir%\\regedit.exe"));
        src.push_back(fullPath(L"src\\z-f.ico"));
        src.push_back(firstMatch(envPath(L"%ProgramData%\\Microsoft\\Windows\\Start Menu\\Programs\\*.lnk")));
        src.push_back(firstMatch(envPath(L"%windir%\\System32\\*.png")));
        src.push_back(firstMatch(envPath(L"%windir%\\Web\\Screen\\*.jpg")));
    }
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); // come i thread di [z]f
    std::wstring root = fullPath(argv[2]);
    if (!rootStart(root)) { rootStop(); return 1; }
    g_t0 = nowMs();
    int bad = 0;

    if (!wait) {
        wprintf(L"%-10s %-6s %-12s %s\n", L"passo", L"scar.", L"prima/dopo", L"file");
        for (auto& s : src) {
            if (s.empty()) continue;
            std::wstring ext = s.substr(s.rfind(L'.'));
            for (const Step& st : kSteps) {
                std::wstring name = std::wstring(st.tag) + ext;
                if (!placeholder(s, name)) { bad++; continue; }
                std::wstring p = root + L"\\" + name;
                DWORD a0 = GetFileAttributesW(p.c_str());
                LONG f0 = g_fetches;
                double t0 = nowMs();
                st.fn(p);
                double ms = nowMs() - t0;
                LONG n = g_fetches - f0;
                DWORD a1 = GetFileAttributesW(p.c_str());
                wprintf(L"%-10s %-6ld %-5s/%-6s %-6s %7.1f ms  %s\n", st.tag, n, onlineOnly(a0) ? L"online" : L"locale",
                        onlineOnly(a1) ? L"online" : L"locale", ext.c_str(), ms, st.what);
                if (!onlineOnly(a0)) bad++;
                if ((n > 0) != (st.fn == stepHead)) bad++; // atteso: scarica solo la lettura del contenuto
            }
            // Che icona dà la shell al segnaposto: la stessa del file scaricato ("inizio") o un'altra
            // (quella generica del tipo)?
            SHFILEINFOW ph = {}, full = {};
            std::wstring pp = root + L"\\grande" + ext, pf = root + L"\\inizio" + ext;
            SHGetFileInfoW(pp.c_str(), 0, &ph, sizeof(ph), SHGFI_SYSICONINDEX);
            SHGetFileInfoW(pf.c_str(), 0, &full, sizeof(full), SHGFI_SYSICONINDEX);
            wprintf(L"           attributi del segnaposto 0x%08lX; icona %d, del file scaricato %d: %s\n\n",
                    GetFileAttributesW(pp.c_str()), ph.iIcon, full.iIcon, ph.iIcon == full.iIcon ? L"la stessa" : L"diversa");
        }
        wprintf(L"\n%s\n", bad ? L"RISULTATO INATTESO (vedi sopra)" : L"Come atteso: scarica solo la lettura del contenuto.");
    } else {
        for (auto& s : src) if (!s.empty() && !placeholder(s, baseName(s.c_str()))) bad++;
        wprintf(L"Segnaposto pronti in %s; in ascolto per %d s.\n", root.c_str(), secs);
        fflush(stdout);
        Sleep((DWORD)secs * 1000);
        wprintf(L"\nRichieste di scaricamento: %ld\n", g_fetches);
        EnterCriticalSection(&g_lock);
        for (auto& kv : g_byProc) wprintf(L"  %-28s %d\n", kv.first.c_str(), kv.second);
        LeaveCriticalSection(&g_lock);
    }
    rootStop();
    if (SUCCEEDED(co)) CoUninitialize();
    return bad ? 1 : 0;
}
