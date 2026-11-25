// Prova del lettore dei collegamenti di [z]f (src/lnk.cpp), senza interfaccia: per ogni .lnk nelle
// cartelle date (anche nelle sottocartelle) confronta la destinazione letta da noi con quella di
// Windows (IShellLink::GetPath, quella del campo "Destinazione" nelle Proprietà), con i tempi.
//
// Compilazione (dalla cartella del progetto, con l'ambiente di Visual Studio):
//   cl /nologo /std:c++17 /utf-8 /EHsc /W3 /O2 /MT /DUNICODE /D_UNICODE /Fe:build\collegamenti-prova.exe prove\collegamenti-prova.cpp ole32.lib shell32.lib
// Uso:
//   build\collegamenti-prova.exe cartella [cartella…]
// Esempio, tutti i collegamenti del PC che contano:
//   build\collegamenti-prova.exe "%USERPROFILE%\Desktop" "%PUBLIC%\Desktop" "%APPDATA%\Microsoft\Windows\Start Menu" "%ProgramData%\Microsoft\Windows\Start Menu"

#include "../src/base.h"
#include "../src/theme.h"
#include "../src/listing.cpp"
static void dirSizeFill(Listing*) {}
static void linksFind(Listing*) {}
#include "../src/zip.cpp"  // listing.cpp legge anche gli zip
#include "../src/lnk.cpp"
#include <shobjidl.h>

static void collect(const std::wstring& dir, std::vector<std::wstring>& out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        std::wstring p = dir + L"\\" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) collect(p, out);
        } else if (n.size() > 4 && !_wcsicmp(n.c_str() + n.size() - 4, L".lnk")) {
            out.push_back(p);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { fwprintf(stderr, L"uso: collegamenti-prova cartella [cartella…]\n"); return 2; }
    SetConsoleOutputCP(CP_UTF8);
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcFreq = (double)fq.QuadPart;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::vector<std::wstring> files;
    for (int i = 1; i < argc; i++) collect(argv[i], files);

    int same = 0, both = 0, oursOnly = 0, shellOnly = 0, diff = 0;
    double oursMs = 0, oursMax = 0, shellMs = 0;
    for (const std::wstring& f : files) {
        std::wstring ours;
        bool dir = false;
        double t0 = nowMs();
        bool ok = linkRead(f, ours, dir);
        double ms = nowMs() - t0;
        oursMs += ms;
        oursMax = std::max(oursMax, ms);

        std::wstring shell;
        t0 = nowMs();
        IShellLinkW* sl = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&sl)))) {
            IPersistFile* pf = nullptr;
            if (SUCCEEDED(sl->QueryInterface(IID_PPV_ARGS(&pf)))) {
                if (SUCCEEDED(pf->Load(f.c_str(), STGM_READ))) {
                    wchar_t buf[2048] = {};
                    if (sl->GetPath(buf, 2048, nullptr, 0) == S_OK) shell = buf;
                }
                pf->Release();
            }
            sl->Release();
        }
        shellMs += nowMs() - t0;

        if (ours.empty() && shell.empty()) { same++; continue; }
        if (!_wcsicmp(ours.c_str(), shell.c_str())) { same++; both++; continue; }
        if (shell.empty()) oursOnly++; else if (ours.empty()) shellOnly++; else diff++;
        wprintf(L"%s\n  noi:     %s%s\n  Windows: %s\n", f.c_str(), ok ? L"" : L"(non letto) ", ours.c_str(), shell.c_str());
    }
    size_t n = files.size();
    wprintf(L"\n%zu collegamenti: %d uguali (%d con un percorso), %d diversi, %d solo noi, %d solo Windows\n",
            n, same, both, diff, oursOnly, shellOnly);
    if (n)
        wprintf(L"tempo a collegamento: noi %.3f ms (il più lento %.2f ms), IShellLink %.3f ms\n",
                oursMs / n, oursMax, shellMs / n);
    CoUninitialize();
    return 0;
}
