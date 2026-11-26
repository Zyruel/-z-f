// Misura a parte di Shift+Canc e Proprietà di più voci (shellVerb in shell.cpp), con gli stessi
// passi che [z]f fa sul thread dell'interfaccia: bindFolder, una ParseDisplayName per voce,
// GetUIObjectOf, QueryContextMenu con CMF_DEFAULTONLY, InvokeCommand.
// Un secondo thread aspetta la finestra di Windows (conferma dell'eliminazione, Proprietà), segna
// quando compare e la chiude: per l'eliminazione preme "No", quindi non si elimina niente.
// Usare solo su una cartella di prova con file finti.
//
// Compilare (da Bash, nella cartella del progetto; l'exe va in build\):
//   cmd //c '.\build.bat prova prove\shell-verbi.cpp user32.lib shell32.lib ole32.lib'
// Uso:  shell-verbi.exe <cartella> <voci> prepara|elimina|proprieta
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>

static double freq;
static double nowMs() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart * 1000.0 / freq; }

static HWND g_main;
static volatile LONG g_found = 0;
static double g_foundAt = 0;
static wchar_t g_title[256];

static BOOL CALLBACK findNo(HWND c, LPARAM lp) {
    wchar_t t[64], cls[32];
    GetClassNameW(c, cls, 32);
    GetWindowTextW(c, t, 64);
    std::wstring s;
    for (wchar_t* p = t; *p; p++) if (*p != L'&') s += *p;
    if (!_wcsicmp(cls, L"Button") && (s == L"No" || s == L"Annulla")) { *(HWND*)lp = c; return FALSE; }
    return TRUE;
}

static BOOL CALLBACK findOurs(HWND w, LPARAM lp) {
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == GetCurrentProcessId() && w != g_main && IsWindowVisible(w)) { *(HWND*)lp = w; return FALSE; }
    return TRUE;
}

// Eliminazione: si aspetta la finestra con "No" (con molte voci prima compare quella che conta gli
// elementi, con "Annulla": va lasciata stare, chiuderla non annulla l'eliminazione). Proprietà: la
// prima finestra, chiusa con WM_CLOSE.
static bool g_delete;
static double g_firstAt = 0;

static DWORD WINAPI watcher(void*) {
    double t0 = nowMs();
    while (nowMs() - t0 < 120000) {
        HWND w = nullptr;
        EnumWindows(findOurs, (LPARAM)&w);
        if (w && !g_firstAt) g_firstAt = nowMs();
        if (w && g_delete) {
            HWND no = nullptr;
            EnumChildWindows(w, findNo, (LPARAM)&no);
            wchar_t t[16] = L"";
            if (no) GetWindowTextW(no, t, 16);
            if (!no || _wcsicmp(t, L"&No") && _wcsicmp(t, L"No")) w = nullptr; // non è ancora la conferma
        }
        if (w) {
            g_foundAt = nowMs();
            GetWindowTextW(w, g_title, 256);
            Sleep(300); // il dialogo finisce di prepararsi
            HWND no = nullptr;
            EnumChildWindows(w, findNo, (LPARAM)&no);
            if (g_delete) SendMessageW(no, BM_CLICK, 0, 0);
            else PostMessageW(w, WM_CLOSE, 0, 0);
            InterlockedExchange(&g_found, 1);
            return 0;
        }
        Sleep(1);
    }
    InterlockedExchange(&g_found, 2);
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 4) { wprintf(L"uso: shell-verbi <cartella> <voci> prepara|elimina|proprieta\n"); return 1; }
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); freq = (double)f.QuadPart;
    std::wstring dir = argv[1], mode = argv[3];
    int n = _wtoi(argv[2]);
    OleInitialize(nullptr);
    g_main = CreateWindowExW(0, L"STATIC", L"shell-verbi", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);

    // I nomi come li ha [z]f: già in memoria dalla lettura della cartella
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) names.push_back(fd.cFileName);
        } while (FindNextFileW(h, &fd) && (int)names.size() < n);
        FindClose(h);
    }
    std::sort(names.begin(), names.end());
    wprintf(L"%s: %d voci, %s\n", dir.c_str(), (int)names.size(), mode.c_str());

    int rounds = mode == L"prepara" ? 3 : 1;
    for (int round = 0; round < rounds; round++) {
        double t0 = nowMs();
        PIDLIST_ABSOLUTE pidl = nullptr;
        SHParseDisplayName(dir.c_str(), nullptr, &pidl, 0, nullptr);
        IShellFolder* sf = nullptr;
        SHBindToObject(nullptr, pidl, nullptr, IID_IShellFolder, (void**)&sf);
        CoTaskMemFree(pidl);
        if (!sf) { wprintf(L"cartella non trovata\n"); return 1; }
        double t1 = nowMs();
        std::vector<LPITEMIDLIST> pidls;
        for (const std::wstring& nm : names) {
            LPITEMIDLIST c = nullptr;
            if (SUCCEEDED(sf->ParseDisplayName(g_main, nullptr, (LPWSTR)nm.c_str(), nullptr, &c, nullptr))) pidls.push_back(c);
        }
        double t2 = nowMs();
        IContextMenu* cm = nullptr;
        sf->GetUIObjectOf(g_main, (UINT)pidls.size(), (LPCITEMIDLIST*)pidls.data(), IID_IContextMenu, nullptr, (void**)&cm);
        double t3 = nowMs();
        HMENU m = CreatePopupMenu();
        if (cm) cm->QueryContextMenu(m, 0, 1, 0x7FFF, CMF_DEFAULTONLY);
        double t4 = nowMs();
        wprintf(L"  cartella %.1f ms, ParseDisplayName %.1f ms (%.1f us a voce), GetUIObjectOf %.1f ms, QueryContextMenu %.1f ms: prima del comando %.1f ms\n",
                t1 - t0, t2 - t1, (t2 - t1) * 1000.0 / std::max<size_t>(1, pidls.size()), t3 - t2, t4 - t3, t4 - t0);
        if (cm && mode != L"prepara") {
            g_delete = mode == L"elimina";
            const char* verb = g_delete ? "delete" : "properties";
            HANDLE wt = CreateThread(nullptr, 0, watcher, nullptr, 0, nullptr);
            CMINVOKECOMMANDINFOEX ici = {};
            ici.cbSize = sizeof(ici);
            ici.fMask = CMIC_MASK_UNICODE | (mode == L"elimina" ? CMIC_MASK_SHIFT_DOWN : 0);
            ici.hwnd = g_main;
            ici.lpVerb = verb;
            std::wstring vw(verb, verb + strlen(verb));
            ici.lpVerbW = vw.c_str();
            ici.lpDirectoryW = dir.c_str();
            ici.nShow = SW_SHOWNORMAL;
            double ti = nowMs();
            HRESULT hr = cm->InvokeCommand((CMINVOKECOMMANDINFO*)&ici);
            double te = nowMs();
            // La finestra può comparire dopo il ritorno (thread della shell): si pompano i messaggi
            MSG msg;
            while (!g_found) {
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
                MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
            }
            wprintf(L"  InvokeCommand 0x%08lx: tornato dopo %.1f ms; finestra \"%s\" dopo %.1f ms dall'inizio del comando, %.1f ms dal tasto\n",
                    (unsigned long)hr, te - ti, g_found == 1 ? g_title : L"(nessuna)", g_found == 1 ? g_foundAt - ti : -1.0,
                    g_found == 1 ? g_foundAt - t0 : -1.0);
            if (g_firstAt && g_firstAt < g_foundAt) wprintf(L"  prima finestra (avanzamento) dopo %.1f ms dall'inizio del comando\n", g_firstAt - ti);
            WaitForSingleObject(wt, INFINITE);
            CloseHandle(wt);
            double tw = nowMs();
            while (nowMs() - tw < 1500) { // la shell chiude le sue finestre
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
                Sleep(5);
            }
        }
        DestroyMenu(m);
        if (cm) cm->Release();
        for (LPITEMIDLIST p : pidls) CoTaskMemFree(p);
        sf->Release();
    }
    int left = 0;
    h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h != INVALID_HANDLE_VALUE) {
        do left++; while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    wprintf(L"  voci nella cartella alla fine: %d\n", left - 2);
    OleUninitialize();
    return 0;
}
