// Prova del lettore dei collegamenti di [z]f (src/lnk.cpp), senza interfaccia: per ogni .lnk nelle
// cartelle date (anche nelle sottocartelle) confronta la destinazione letta da noi con quella di
// Windows (IShellLink::GetPath, quella del campo "Destinazione" nelle Proprietà), con i tempi.
//
// Compilare (da Bash, nella cartella del progetto; l'exe va in build\):
//   cmd //c '.\build.bat prova prove\collegamenti-prova.cpp ole32.lib shell32.lib'
// Per ognuno prova anche la sola IDList, come se mancasse la LinkInfo. Con --crea, prima scrive
// nella cartella data dei collegamenti a destinazioni irraggiungibili (senza LinkInfo) e due
// collegamenti di rete con la sola IDList (a condivisioni del PC, scritti a mano), e li prova.
// Uso:
//   build\collegamenti-prova.exe [--crea cartella] cartella [cartella…]
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

// Collegamenti a destinazioni che non esistono: Windows li salva senza LinkInfo, con la sola IDList
// (il caso di un'unità di rete non collegata), e la destinazione si legge solo da lì
static void createUnreachable(const std::wstring& dir) {
    static const wchar_t* kTargets[] = {
        L"Q:\\Cartella inesistente\\Documento con un nome lungo.txt",
        L"Q:\\Àccènti e spazi\\Più giù\\file.dat",
        L"Q:\\CORTO\\A.TXT",
        L"\\\\server-inesistente\\condivisione\\cartella\\file.txt",
    };
    CreateDirectoryW(dir.c_str(), nullptr);
    int i = 0;
    for (const wchar_t* t : kTargets) {
        IShellLinkW* sl = nullptr;
        if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&sl)))) continue;
        HRESULT hr = sl->SetPath(t);
        IPersistFile* pf = nullptr;
        if (SUCCEEDED(sl->QueryInterface(IID_PPV_ARGS(&pf)))) {
            std::wstring out = dir + L"\\irraggiungibile-" + std::to_wstring(++i) + L".lnk";
            HRESULT hs = pf->Save(out.c_str(), TRUE);
            wprintf(L"creato %s -> %s (SetPath 0x%08lX, Save 0x%08lX)\n", out.c_str(), t, (unsigned long)hr, (unsigned long)hs);
            pf->Release();
        }
        sl->Release();
    }
    // Rete: Windows 11 non salva mai una IDList di rete in un .lnk (SetPath con un percorso UNC, e
    // anche SetIDList, danno la sola LinkInfo), che però c'è nei collegamenti scritti da versioni
    // vecchie. Il collegamento si scrive a mano: la radice Rete, poi gli elementi dalla condivisione
    // in giù presi da SHParseDisplayName (il suo elemento di rete è quello vero di Windows), senza
    // LinkInfo. Serve una condivisione che esiste: quella amministrativa del PC
    static const wchar_t* kNet[] = { L"\\\\localhost\\C$\\Windows\\win.ini", L"\\\\127.0.0.1\\ADMIN$\\System32" };
    static const uint8_t kNetRoot[20] = { 0x14, 0x00, 0x1F, 0x58, 0x60, 0x2C, 0x8D, 0x20, 0xEA, 0x3A,
                                          0x69, 0x10, 0xA2, 0xD7, 0x08, 0x00, 0x2B, 0x30, 0x30, 0x9D };
    for (const wchar_t* t : kNet) {
        PIDLIST_ABSOLUTE pidl = nullptr;
        HRESULT hp = SHParseDisplayName(t, nullptr, &pidl, 0, nullptr);
        if (FAILED(hp)) { wprintf(L"non creato: %s (SHParseDisplayName 0x%08lX)\n", t, (unsigned long)hp); continue; }
        const uint8_t* p = (const uint8_t*)pidl;
        while (lkU16(p) && (p[2] & 0x70) != 0x40) p += lkU16(p); // fino all'elemento di rete
        std::vector<uint8_t> ids(kNetRoot, kNetRoot + 20);
        for (; lkU16(p); p += lkU16(p)) ids.insert(ids.end(), p, p + lkU16(p));
        CoTaskMemFree(pidl);
        if (ids.size() == 20) { wprintf(L"non creato: %s (nessun elemento di rete)\n", t); continue; }
        ids.push_back(0);
        ids.push_back(0);
        std::vector<uint8_t> f(0x4C, 0);
        f[0] = 0x4C;
        static const uint8_t kClsid[16] = { 0x01, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 };
        memcpy(&f[4], kClsid, 16);
        f[0x14] = 0x81; // HasLinkTargetIDList, IsUnicode
        uint16_t len = (uint16_t)ids.size();
        f.push_back((uint8_t)len);
        f.push_back((uint8_t)(len >> 8));
        f.insert(f.end(), ids.begin(), ids.end());
        f.insert(f.end(), 4, 0); // blocchi extra: nessuno
        std::wstring out = dir + L"\\rete-" + std::to_wstring(++i) + L".lnk";
        HANDLE h = CreateFileW(out.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        DWORD w = 0;
        bool ok = h != INVALID_HANDLE_VALUE && WriteFile(h, f.data(), (DWORD)f.size(), &w, nullptr) && w == f.size();
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        wprintf(L"%s %s -> %s (solo IDList, scritto a mano)\n", ok ? L"creato" : L"non creato", out.c_str(), t);
    }
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { fwprintf(stderr, L"uso: collegamenti-prova cartella [cartella…]\n"); return 2; }
    SetConsoleOutputCP(CP_UTF8);
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcFreq = (double)fq.QuadPart;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int first = 1;
    if (!wcscmp(argv[1], L"--crea")) {
        if (argc < 3) { fwprintf(stderr, L"uso: collegamenti-prova --crea cartella\n"); return 2; }
        createUnreachable(argv[2]);
        first = 2;
    }
    std::vector<std::wstring> files;
    for (int i = first; i < argc; i++) collect(argv[i], files);

    int same = 0, both = 0, oursOnly = 0, shellOnly = 0, diff = 0, idsSame = 0, idsNone = 0, idsDiff = 0, idsMissed = 0;
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

        // La IDList da sola, come se la LinkInfo mancasse: deve dare lo stesso percorso (o nessuno,
        // per le cartelle speciali e le app)
        std::wstring ids;
        {
            HANDLE h = CreateFileW(f.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            std::vector<uint8_t> b(kLinkMaxBytes);
            DWORD k = 0;
            if (h != INVALID_HANDLE_VALUE) { if (!ReadFile(h, b.data(), (DWORD)b.size(), &k, nullptr)) k = 0; CloseHandle(h); }
            if (k >= 0x4E && (lkU32(b.data() + 0x14) & 1))
                ids = lkIdList(b.data() + 0x4E, b.data() + std::min<size_t>(k, 0x4E + (size_t)lkU16(b.data() + 0x4C)));
        }
        // Senza percorso solo per noi: radici che non leggiamo (la cartella dell'utente, Download), con
        // la LinkInfo sempre presente perché Windows scrive la sola IDList solo se la destinazione
        // è irraggiungibile, e allora la radice è Computer
        if (ids.empty() && !shell.empty()) idsMissed++;
        else if (ids.empty()) idsNone++;
        else if (!_wcsicmp(ids.c_str(), shell.c_str())) idsSame++;
        else {
            idsDiff++;
            wprintf(L"%s\n  IDList:  %s\n  Windows: %s\n", f.c_str(), ids.c_str(), shell.c_str());
        }

        if (ours.empty() && shell.empty()) { same++; continue; }
        if (!_wcsicmp(ours.c_str(), shell.c_str())) { same++; both++; continue; }
        if (shell.empty()) oursOnly++; else if (ours.empty()) shellOnly++; else diff++;
        wprintf(L"%s\n  noi:     %s%s\n  Windows: %s\n", f.c_str(), ok ? L"" : L"(non letto) ", ours.c_str(), shell.c_str());
    }
    size_t n = files.size();
    wprintf(L"\n%zu collegamenti: %d uguali (%d con un percorso), %d diversi, %d solo noi, %d solo Windows\n",
            n, same, both, diff, oursOnly, shellOnly);
    wprintf(L"solo IDList: %d come Windows, %d diversi, %d senza percorso anche per Windows, %d senza percorso solo per noi (radice non letta o senza IDList)\n",
            idsSame, idsDiff, idsNone, idsMissed);
    if (n)
        wprintf(L"tempo a collegamento: noi %.3f ms (il più lento %.2f ms), IShellLink %.3f ms\n",
                oursMs / n, oursMax, shellMs / n);
    CoUninitialize();
    return 0;
}
