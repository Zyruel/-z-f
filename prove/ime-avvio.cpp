// Misura a parte del "messaggio lento 0x0006" all'avvio (WM_ACTIVATE, con dentro WM_IME_SETCONTEXT
// 0x0281). [z]f non gestisce questi messaggi: il tempo è in DefWindowProc, cioè in Windows, la prima
// volta che una finestra del thread prende il fuoco (IME e Text Services Framework del thread).
// Qui una finestra nascosta prende il fuoco solo nel suo thread (SetFocus): stessi messaggi, nessuna
// finestra sullo schermo, il primo piano resta all'app che c'è. Si misura ogni messaggio (anche
// annidato) e si elencano le DLL caricate durante il primo fuoco.
//
// Compilare da un prompt di Visual Studio:
//   cl /std:c++17 /O2 /EHsc /MT /DUNICODE /D_UNICODE ime-avvio.cpp user32.lib psapi.lib
// Uso: ime-avvio.exe [volte] [precarica]   (ogni volta un processo nuovo: il costo è quello del
// primo fuoco; con "precarica" le DLL del primo fuoco si caricano prima su un altro thread)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

static double freq;
static double nowMs() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart * 1000.0 / freq; }

struct Rec { UINT msg; int depth; double ms; };
static std::vector<Rec> recs;
static int depth = 0;

static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    double t0 = nowMs();
    depth++;
    LRESULT r = DefWindowProcW(h, m, wp, lp);
    depth--;
    recs.push_back({ m, depth, nowMs() - t0 });
    return r;
}

static std::set<std::wstring> modules() {
    std::set<std::wstring> s;
    HMODULE mods[1024];
    DWORD need = 0;
    if (EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &need)) {
        for (DWORD i = 0; i < need / sizeof(HMODULE) && i < 1024; i++) {
            wchar_t n[MAX_PATH];
            if (GetModuleBaseNameW(GetCurrentProcess(), mods[i], n, MAX_PATH)) s.insert(n);
        }
    }
    return s;
}

// Le DLL che Windows carica al primo fuoco, caricate prima da un altro thread
static const wchar_t* kTsfDlls[] = { L"textinputframework.dll", L"CoreMessaging.dll", L"CoreUIComponents.dll",
                                     L"wintypes.dll", L"Oleacc.dll", L"OLEAUT32.dll" };
static DWORD WINAPI preload(void*) {
    for (const wchar_t* d : kTsfDlls) LoadLibraryExW(d, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return 0;
}

static int child(bool pre) {
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); freq = (double)f.QuadPart;
    if (pre) {
        double tp = nowMs();
        HANDLE h = CreateThread(nullptr, 0, preload, nullptr, 0, nullptr);
        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);
        wprintf(L"  precaricate in %.1f ms (thread)\n", nowMs() - tp);
    }
    WNDCLASSW wc = {};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ime-avvio";
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    std::set<std::wstring> before = modules();
    recs.clear();
    double t0 = nowMs();
    SetFocus(w); // nascosta: niente schermo, niente primo piano
    double total = nowMs() - t0;
    std::set<std::wstring> after = modules();
    wprintf(L"primo fuoco %.1f ms:", total);
    for (const Rec& r : recs) if (r.ms >= 1) wprintf(L" 0x%04X%s %.1f", r.msg, r.depth ? L"(annidato)" : L"", r.ms);
    wprintf(L"\n  DLL nuove:");
    for (const std::wstring& m : after) if (!before.count(m)) wprintf(L" %s", m.c_str());
    wprintf(L"\n");
    // Secondo fuoco, per confronto
    SetFocus(nullptr);
    t0 = nowMs();
    SetFocus(w);
    wprintf(L"  secondo fuoco %.1f ms\n", nowMs() - t0);
    DestroyWindow(w);
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    if (argc > 1 && !wcscmp(argv[1], L"--figlio")) return child(argc > 2);
    int n = argc > 1 ? _wtoi(argv[1]) : 10;
    bool pre = argc > 2 && !wcscmp(argv[2], L"precarica");
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    for (int i = 0; i < n; i++) {
        std::wstring cmd = std::wstring(L"\"") + exe + L"\" --figlio" + (pre ? L" precarica" : L"");
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return 1;
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return 0;
}
