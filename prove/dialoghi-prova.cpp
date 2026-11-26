// Prova del salto dei dialoghi: apre un dialogo di Windows e scrive su stdout cosa è stato scelto.
//   dialoghi-prova apri | salva | cartella | classico | classico-salva
//   dialoghi-prova ritorno <pid di z-f-dev> <cartella A> <cartella B> [ms tra i due ritorni] [ms in [z]f]
// "ritorno" prova il ritorno rapido (filedlg.cpp: un salto alla volta per dialogo). Apre Salva; dopo
// il primo salto consegna A a z-f-dev (che viene davanti, come passando a [z]f e cambiando
// cartella), torna al dialogo, poi fa lo stesso con B mentre il salto verso A è ancora in corso.
// Scrive ogni cartella in cui entra il dialogo e ogni conferma del nome (con Salva salverebbe il
// file: qui la si blocca), poi il nome rimasto nel campo, e chiude il dialogo. Atteso: ultima
// cartella B, nessuna conferma, nome "prova salvataggio.txt". Senza console: z-f-dev di prova
// soltanto (il pid dice a quale finestra consegnare).
// Compilare (Developer PowerShell): cl /nologo /EHsc /DUNICODE /D_UNICODE prove\dialoghi-prova.cpp
//   /Fe:build\dialoghi-prova.exe /Fo:build\ ole32.lib shell32.lib comdlg32.lib user32.lib
#include <windows.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <stdio.h>
#include <string>

static double t0Ms() {
    static LARGE_INTEGER f = {}, t0 = {};
    LARGE_INTEGER t;
    if (!f.QuadPart) { QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0); }
    QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - t0.QuadPart) * 1000.0 / (double)f.QuadPart;
}

// Il risultato va anche in build\dialoghi-prova.txt (stdout si perde se il programma parte senza console)
static void say(const wchar_t* fmt, const wchar_t* s) {
    wprintf(fmt, s);
    FILE* f = nullptr;
    if (!_wfopen_s(&f, L"dialoghi-prova.txt", L"a, ccs=UTF-8") && f) { fwprintf(f, fmt, s); fclose(f); }
}

// Con "ritorno": ogni cartella in cui entra il dialogo, e le conferme del nome (bloccate)
struct Events : IFileDialogEvents {
    LONG ref = 1;
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == __uuidof(IFileDialogEvents)) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return (ULONG)InterlockedIncrement(&ref); }
    IFACEMETHODIMP_(ULONG) Release() override { return (ULONG)InterlockedDecrement(&ref); } // sullo stack
    IFACEMETHODIMP OnFileOk(IFileDialog* d) override {
        PWSTR n = nullptr;
        d->GetFileName(&n);
        wchar_t b[1024];
        swprintf_s(b, L"%8.1f ms  CONFERMA DEL NOME, Salva salverebbe: %s", t0Ms(), n ? n : L"?");
        say(L"%s\n", b);
        CoTaskMemFree(n);
        return S_FALSE;
    }
    IFACEMETHODIMP OnFolderChange(IFileDialog* d) override {
        IShellItem* it = nullptr;
        if (FAILED(d->GetFolder(&it))) return S_OK;
        PWSTR p = nullptr;
        it->GetDisplayName(SIGDN_FILESYSPATH, &p);
        wchar_t b[1024];
        swprintf_s(b, L"%8.1f ms  cartella %s", t0Ms(), p ? p : L"?");
        say(L"%s\n", b);
        CoTaskMemFree(p);
        it->Release();
        return S_OK;
    }
    IFACEMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*) override { return S_OK; }
    IFACEMETHODIMP OnSelectionChange(IFileDialog*) override { return S_OK; }
    IFACEMETHODIMP OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE*) override { return S_OK; }
    IFACEMETHODIMP OnTypeChange(IFileDialog*) override { return S_OK; }
    IFACEMETHODIMP OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE*) override { return S_OK; }
};

struct Ritorno { DWORD tid, zfPid; std::wstring a, b; int gap, stay; };

static HWND findTop(DWORD pid, DWORD tid, const wchar_t* cls) {
    struct Q { DWORD pid, tid; const wchar_t* cls; HWND found; } q = { pid, tid, cls, nullptr };
    EnumWindows([](HWND h, LPARAM p) -> BOOL {
        Q* q = (Q*)p;
        DWORD pid = 0, tid = GetWindowThreadProcessId(h, &pid);
        wchar_t c[64];
        if ((q->pid ? pid == q->pid : tid == q->tid) && IsWindowVisible(h) && GetClassNameW(h, c, 64) && !wcscmp(c, q->cls)) {
            q->found = h;
            return FALSE;
        }
        return TRUE;
    }, (LPARAM)&q);
    return q.found;
}

static void note(const wchar_t* what, const wchar_t* detail) {
    wchar_t b[1024];
    swprintf_s(b, L"%8.1f ms  %s%s", t0Ms(), what, detail);
    say(L"%s\n", b);
}

// Il primo piano dall'ultima notifica, non da GetForegroundWindow, che a PC bloccato risponde nullptr
static HWND volatile g_lastFg = nullptr;

// Come un clic sulla finestra: Windows lo permette solo a chi è in primo piano, quindi si passa
// dall'input del thread che lo è
// dall'input del thread che lo è; oppure a un processo appena avviato da chi è in primo piano
// ("dialoghi-prova primo-piano <hwnd>"), come fa [z]f quando consegna una cartella
static void bringFront(HWND h) {
    DWORD fgTid = GetWindowThreadProcessId(g_lastFg, nullptr), me = GetCurrentThreadId();
    AttachThreadInput(me, fgTid, TRUE);
    SetForegroundWindow(h);
    AttachThreadInput(me, fgTid, FALSE);
    if (g_lastFg == h) return;
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 64];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    swprintf_s(cmd, L"\"%s\" primo-piano %llu", exe, (unsigned long long)(ULONG_PTR)h);
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(exe, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 2000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// Come passare a [z]f e cambiare cartella: la cartella arriva come da un altro programma
static void giveToZf(HWND zf, DWORD pid, const std::wstring& path) {
    AllowSetForegroundWindow(pid);
    std::wstring msg = L"|" + path;
    COPYDATASTRUCT cd = { 0x7A66, (DWORD)((msg.size() + 1) * sizeof(wchar_t)), (void*)msg.c_str() }; // ZF_COPYDATA
    DWORD_PTR r = 0;
    SendMessageTimeoutW(zf, WM_COPYDATA, 0, (LPARAM)&cd, SMTO_ABORTIFHUNG, 3000, &r);
    note(L"[z]f in ", path.c_str());
    if (g_lastFg != zf) bringFront(zf); // [z]f si porta davanti da solo, se Windows glielo lascia fare
}

// Windows cambia il primo piano un poco dopo la richiesta: si aspetta che sia fatto
static bool waitFg(HWND h, const wchar_t* who) {
    for (int i = 0; i < 200; i++) {
        if (g_lastFg == h) return true;
        Sleep(5);
    }
    note(who, L" NON in primo piano");
    return false;
}

// Ogni cambio del primo piano, come lo vede [z]f (la stessa notifica)
static DWORD g_zfPid = 0, g_mainTid = 0;
static void CALLBACK onForeground(HWINEVENTHOOK, DWORD, HWND h, LONG obj, LONG, DWORD, DWORD) {
    if (obj != OBJID_WINDOW) return;
    DWORD pid = 0, tid = GetWindowThreadProcessId(h, &pid);
    wchar_t c[64] = L"?";
    GetClassNameW(h, c, 64);
    wchar_t b[128];
    swprintf_s(b, L"%s %p (%s)", pid == g_zfPid ? L"[z]f" : tid == g_mainTid ? L"dialogo" : L"altro", (void*)h, c);
    note(L"primo piano: ", b);
    g_lastFg = h;
}

static DWORD WINAPI ritornoThread(void* p) {
    Ritorno* r = (Ritorno*)p;
    HWND zf = findTop(r->zfPid, 0, L"z-f"), dlg = nullptr;
    for (int i = 0; i < 100 && !(dlg = findTop(0, r->tid, L"#32770")); i++) Sleep(50);
    if (!zf || !dlg) { note(!zf ? L"z-f-dev non trovato" : L"dialogo non trovato", L""); return 0; }
    wchar_t b[64];
    swprintf_s(b, L"%p", (void*)dlg);
    note(L"dialogo: ", b);
    Sleep(1500); // il salto all'apertura
    giveToZf(zf, r->zfPid, r->a);
    waitFg(zf, L"[z]f");
    Sleep(r->stay); // [z]f apre la cartella (predefiniti 100 ms)
    bringFront(dlg);
    waitFg(dlg, L"dialogo");
    Sleep(r->gap);
    giveToZf(zf, r->zfPid, r->b);
    waitFg(zf, L"[z]f");
    Sleep(r->stay);
    bringFront(dlg);
    waitFg(dlg, L"dialogo");
    Sleep(4000); // i salti finiscono
    HWND edit = nullptr;
    EnumChildWindows(dlg, [](HWND h, LPARAM p) -> BOOL {
        wchar_t c[64], pc[64];
        if (GetDlgCtrlID(h) == 0x3E9 && GetClassNameW(h, c, 64) && !wcscmp(c, L"Edit") &&
            GetClassNameW(GetParent(h), pc, 64) && !wcscmp(pc, L"ComboBox")) { *(HWND*)p = h; return FALSE; }
        return TRUE;
    }, (LPARAM)&edit);
    wchar_t name[1024] = L"?";
    DWORD_PTR res = 0;
    if (edit) SendMessageTimeoutW(edit, WM_GETTEXT, 1024, (LPARAM)name, SMTO_ABORTIFHUNG, 2000, &res);
    note(L"nome nel campo: ", name);
    PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
    return 0;
}

static int itemDialog(bool save, bool folders, Ritorno* rit = nullptr) {
    IFileDialog* d = nullptr;
    if (FAILED(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&d)))) return 1;
    if (folders) { FILEOPENDIALOGOPTIONS o; d->GetOptions(&o); d->SetOptions(o | FOS_PICKFOLDERS); }
    if (!folders) {
        COMDLG_FILTERSPEC f[] = { { L"Testo", L"*.txt" }, { L"Tutti i file", L"*.*" } };
        d->SetFileTypes(2, f);
    }
    if (save) d->SetFileName(L"prova salvataggio.txt");
    Events ev;
    DWORD cookie = 0;
    if (rit) {
        d->Advise(&ev, &cookie);
        rit->tid = GetCurrentThreadId();
        g_zfPid = rit->zfPid;
        g_mainTid = rit->tid;
        SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, onForeground, 0, 0, WINEVENT_OUTOFCONTEXT);
        HANDLE th = CreateThread(nullptr, 0, ritornoThread, rit, 0, nullptr);
        if (th) CloseHandle(th);
    }
    HRESULT hr = d->Show(nullptr);
    if (rit) d->Unadvise(cookie);
    if (SUCCEEDED(hr)) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(d->GetResult(&it))) {
            PWSTR p = nullptr;
            it->GetDisplayName(SIGDN_FILESYSPATH, &p);
            say(L"scelto: %s\n", p ? p : L"?");
            CoTaskMemFree(p);
            it->Release();
        }
    } else {
        say(L"annullato (0x%s)\n", std::to_wstring((unsigned long)hr).c_str());
    }
    d->Release();
    return 0;
}

static int classic(bool save) {
    wchar_t file[MAX_PATH] = L"prova classico.txt";
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof(o);
    o.lpstrFile = file;
    o.nMaxFile = MAX_PATH;
    o.lpstrFilter = L"Testo\0*.txt\0Tutti i file\0*.*\0";
    o.Flags = OFN_EXPLORER | OFN_ENABLEHOOK; // con un hook Windows mostra il dialogo vecchio stile
    o.lpfnHook = [](HWND, UINT, WPARAM, LPARAM) -> UINT_PTR { return 0; };
    BOOL ok = save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
    say(ok ? L"scelto: %s\n" : L"annullato%s\n", ok ? file : L"");
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const wchar_t* m = argc > 1 ? argv[1] : L"apri";
    if (!wcscmp(m, L"primo-piano") && argc > 2) // per "ritorno"
        return SetForegroundWindow((HWND)(ULONG_PTR)_wcstoui64(argv[2], nullptr, 10)) ? 0 : 1;
    if (!wcscmp(m, L"ritorno")) {
        if (argc < 5) { wprintf(L"dialoghi-prova ritorno <pid di z-f-dev> <cartella A> <cartella B> [ms] [ms in [z]f]\n"); return 1; }
        t0Ms();
        Ritorno r = { 0, (DWORD)_wtoi(argv[2]), argv[3], argv[4], argc > 5 ? _wtoi(argv[5]) : 0, argc > 6 ? _wtoi(argv[6]) : 100 };
        itemDialog(true, false, &r);
        fflush(stdout);
        CoUninitialize();
        return 0;
    }
    int r = !wcscmp(m, L"salva") ? itemDialog(true, false)
          : !wcscmp(m, L"cartella") ? itemDialog(false, true)
          : !wcscmp(m, L"classico") ? classic(false)
          : !wcscmp(m, L"classico-salva") ? classic(true)
          : itemDialog(false, false);
    fflush(stdout);
    CoUninitialize();
    return r;
}
