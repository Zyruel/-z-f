// Prova del salto dei dialoghi: apre un dialogo di Windows e scrive su stdout cosa è stato scelto.
//   dialoghi-prova apri | salva | cartella | classico | classico-salva
// Compilare (Developer PowerShell): cl /nologo /EHsc /DUNICODE /D_UNICODE prove\dialoghi-prova.cpp
//   /Fe:build\dialoghi-prova.exe /Fo:build\ ole32.lib shell32.lib comdlg32.lib user32.lib
#include <windows.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <stdio.h>
#include <string>

// Il risultato va anche in build\dialoghi-prova.txt (stdout si perde se il programma parte senza console)
static void say(const wchar_t* fmt, const wchar_t* s) {
    wprintf(fmt, s);
    FILE* f = nullptr;
    if (!_wfopen_s(&f, L"dialoghi-prova.txt", L"a, ccs=UTF-8") && f) { fwprintf(f, fmt, s); fclose(f); }
}

static int itemDialog(bool save, bool folders) {
    IFileDialog* d = nullptr;
    if (FAILED(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&d)))) return 1;
    if (folders) { FILEOPENDIALOGOPTIONS o; d->GetOptions(&o); d->SetOptions(o | FOS_PICKFOLDERS); }
    if (!folders) {
        COMDLG_FILTERSPEC f[] = { { L"Testo", L"*.txt" }, { L"Tutti i file", L"*.*" } };
        d->SetFileTypes(2, f);
    }
    if (save) d->SetFileName(L"prova salvataggio.txt");
    HRESULT hr = d->Show(nullptr);
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
    int r = !wcscmp(m, L"salva") ? itemDialog(true, false)
          : !wcscmp(m, L"cartella") ? itemDialog(false, true)
          : !wcscmp(m, L"classico") ? classic(false)
          : !wcscmp(m, L"classico-salva") ? classic(true)
          : itemDialog(false, false);
    fflush(stdout);
    CoUninitialize();
    return r;
}
