// Prova della scrittura negli appunti di [z]f (clipSetText in text.cpp, su un thread): gli appunti
// tenuti aperti da un altro programma, o posseduti da un programma fermo.
//
// Compilare da un prompt di Visual Studio:
//   cl /std:c++17 /O2 /EHsc /MT /utf-8 /DUNICODE /D_UNICODE appunti-prova.cpp user32.lib
// Uso:
//   appunti-prova.exe occupa <secondi> [senza-finestra]
//       apre gli appunti e li tiene aperti: Copia percorso in [z]f deve dire, dopo un secondo,
//       che non ha copiato e perché (con la finestra, il nome di questo programma); se si chiude
//       prima di un secondo, deve riuscire
//   appunti-prova.exe fermo <secondi>
//       diventa proprietario degli appunti con un testo e poi non risponde più ai messaggi: lo
//       svuotamento (EmptyClipboard → WM_DESTROYCLIPBOARD) aspetta lui. L'interfaccia di [z]f deve
//       continuare a rispondere; nel registro "appunti lenti (testo)" con il tempo
//   appunti-prova.exe chi
//       chi possiede gli appunti (processo e classe della finestra) e il testo che contengono
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

static double freq;
static double nowMs() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart * 1000.0 / freq; }

static std::wstring procName(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"?";
    wchar_t buf[MAX_PATH];
    DWORD n = MAX_PATH;
    std::wstring r = QueryFullProcessImageNameW(h, 0, buf, &n) ? buf : L"?";
    CloseHandle(h);
    size_t s = r.rfind(L'\\');
    return s == std::wstring::npos ? r : r.substr(s + 1);
}

static WNDPROC g_prevProc;
static double g_destroyAt = 0;
static LRESULT CALLBACK spyProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROYCLIPBOARD) g_destroyAt = nowMs();
    return CallWindowProcW(g_prevProc, h, msg, wp, lp);
}

static HWND window() {
    return CreateWindowExW(0, L"STATIC", L"appunti-prova", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
}

int wmain(int argc, wchar_t** argv) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    freq = (double)f.QuadPart;
    if (argc < 2) { wprintf(L"uso: appunti-prova occupa|fermo <secondi> · appunti-prova chi\n"); return 1; }
    std::wstring mode = argv[1];
    double secs = argc > 2 ? _wtof(argv[2]) : 5; // anche decimali: 0.5

    if (mode == L"occupa") {
        bool noWin = argc > 3 && !wcscmp(argv[3], L"senza-finestra");
        HWND w = noWin ? nullptr : window();
        if (!OpenClipboard(w)) { wprintf(L"OpenClipboard fallita (%lu)\n", GetLastError()); return 1; }
        wprintf(L"appunti aperti %s per %.1f s\n", noWin ? L"senza finestra" : L"con una finestra", secs);
        fflush(stdout);
        Sleep((DWORD)(secs * 1000));
        CloseClipboard();
        wprintf(L"appunti chiusi\n");
        return 0;
    }
    if (mode == L"fermo") {
        HWND w = window();
        if (!OpenClipboard(w)) { wprintf(L"OpenClipboard fallita (%lu)\n", GetLastError()); return 1; }
        EmptyClipboard();
        const wchar_t t[] = L"testo di appunti-prova";
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizeof(t));
        memcpy(GlobalLock(g), t, sizeof(t));
        GlobalUnlock(g);
        SetClipboardData(CF_UNICODETEXT, g);
        CloseClipboard();
        wprintf(L"proprietario degli appunti; fermo per %.1f s (nessun messaggio)\n", secs);
        fflush(stdout);
        g_prevProc = (WNDPROC)SetWindowLongPtrW(w, GWLP_WNDPROC, (LONG_PTR)spyProc);
        double t0 = nowMs();
        Sleep((DWORD)(secs * 1000));
        MSG m; // i messaggi mandati intanto: chi ha svuotato gli appunti ha aspettato o ha rinunciato
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&m);
        if (g_destroyAt) wprintf(L"WM_DESTROYCLIPBOARD ricevuto al risveglio (%.0f ms)\n", g_destroyAt - t0);
        else wprintf(L"WM_DESTROYCLIPBOARD non arrivato: chi ha scritto non l'ha aspettato\n");
        return 0;
    }
    if (mode == L"chi") {
        HWND o = GetClipboardOwner();
        DWORD pid = 0;
        wchar_t cls[128] = L"";
        if (o) { GetWindowThreadProcessId(o, &pid); GetClassNameW(o, cls, 128); }
        wprintf(L"proprietario: %s (pid %lu, classe %s)\n", o ? procName(pid).c_str() : L"nessuno", pid, cls);
        if (OpenClipboard(nullptr)) {
            HANDLE d = GetClipboardData(CF_UNICODETEXT);
            const wchar_t* p = d ? (const wchar_t*)GlobalLock(d) : nullptr;
            wprintf(L"testo: [%s]\n", p ? p : L"(nessuno)");
            if (p) GlobalUnlock(d);
            CloseClipboard();
        }
        return 0;
    }
    wprintf(L"modo sconosciuto\n");
    return 1;
}
