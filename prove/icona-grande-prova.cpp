// L'icona grande della shell (pvBigIcon, SHIL_JUMBO) chiesta all'avvio insieme alla prima
// SHGetFileInfo del thread delle icone (IQ_EXT, icons.cpp). Un processo nuovo ogni volta (la
// preparazione delle icone di sistema avviene una volta per processo): due thread partono insieme,
// uno fa la SHGetFileInfo di iconThread, l'altro quello che fa pvBigIcon per "file.png", e si
// guarda se l'icona grande arriva vuota.
//
// Compilare (da Bash, nella cartella del progetto; l'exe va in build\):
//   cmd //c '.\build.bat prova prove\icona-grande-prova.cpp user32.lib gdi32.lib shell32.lib ole32.lib'
// Uso: icona-grande-prova.exe [volte] [aspetta]   (con "aspetta" il secondo thread attende la fine
// della prima SHGetFileInfo, come farebbe pvBigIcon aspettando I.shellReady)
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <stdio.h>
#include <string>

static HANDLE gGo, gReady;
static bool gWait;

static DWORD WINAPI extThread(void*) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    WaitForSingleObject(gGo, INFINITE);
    SHFILEINFOW fi = {};
    DWORD_PTR ok = SHGetFileInfoW(L"file", FILE_ATTRIBUTE_NORMAL, &fi, sizeof(fi),
                                  SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    SetEvent(gReady);
    return ok ? 0 : 1;
}

// 0 = icona con qualcosa disegnato; i bit dicono dove si è fermato
static DWORD WINAPI bigThread(void*) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    WaitForSingleObject(gGo, INFINITE);
    if (gWait) WaitForSingleObject(gReady, INFINITE);
    SHFILEINFOW sfi = {};
    if (!SHGetFileInfoW(L"file.png", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES))
        return 2;
    IImageList* il = nullptr;
    if (FAILED(SHGetImageList(SHIL_JUMBO, IID_IImageList, (void**)&il)) || !il) return 4;
    HICON hi = nullptr;
    il->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hi);
    il->Release();
    if (!hi) return 8;
    const int S = 256;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = S;
    bi.bmiHeader.biHeight = -S;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    SelectObject(dc, bmp);
    for (int i = 0; i < S * S; i++) bits[i] = 0x00FFFFFFu;
    GdiFlush();
    DrawIconEx(dc, 0, 0, hi, S, S, 0, nullptr, DI_NORMAL);
    GdiFlush();
    int drawn = 0;
    for (int i = 0; i < S * S; i++) if ((bits[i] & 0xFFFFFF) != 0xFFFFFF) drawn++;
    DeleteDC(dc);
    DeleteObject(bmp);
    DestroyIcon(hi);
    return drawn ? 0 : 16;
}

static int child() {
    gGo = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    gReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE t[2] = { CreateThread(nullptr, 0, extThread, nullptr, 0, nullptr),
                    CreateThread(nullptr, 0, bigThread, nullptr, 0, nullptr) };
    Sleep(20); // entrambi pronti sull'evento: partono insieme
    SetEvent(gGo);
    WaitForMultipleObjects(2, t, TRUE, INFINITE);
    DWORD a = 0, b = 0;
    GetExitCodeThread(t[0], &a);
    GetExitCodeThread(t[1], &b);
    return (int)(a | b);
}

int wmain(int argc, wchar_t** argv) {
    if (argc > 1 && !wcscmp(argv[1], L"figlio")) {
        gWait = argc > 2 && !wcscmp(argv[2], L"aspetta");
        return child();
    }
    int n = argc > 1 ? _wtoi(argv[1]) : 30;
    bool wait = argc > 2 && !wcscmp(argv[2], L"aspetta");
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    int counts[32] = {};
    for (int i = 0; i < n; i++) {
        std::wstring cmd = L"\"" + std::wstring(exe) + L"\" figlio" + (wait ? L" aspetta" : L"");
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return 1;
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        counts[code & 31]++;
    }
    printf("%d processi%s\n", n, wait ? ", il secondo thread aspetta" : ", i due thread insieme");
    const char* what[] = { "icona grande disegnata", "prima SHGetFileInfo fallita", "SHGetFileInfo di file.png fallita",
                           "", "SHGetImageList fallita", "", "", "", "GetIcon senza icona", "", "", "", "", "", "", "",
                           "icona grande vuota" };
    for (int c = 0; c < 32; c++)
        if (counts[c]) printf("  %3d  %s (codice %d)\n", counts[c], c <= 16 && *what[c] ? what[c] : "combinazione", c);
    return 0;
}
