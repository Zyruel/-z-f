// [z]f — base: include di sistema, misura del tempo, registro dei rallentamenti, colori RGBA.
// Incluso per primo da z-f.cpp; nessun altro modulo include file di sistema.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
// Le funzioni C "classiche" (_snwprintf, wcscpy...) sono usate sempre con dimensioni esplicite:
// gli avvisi di deprecazione di MSVC sarebbero solo rumore che nasconde quelli veri.
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winnetwk.h>       // solo tipi: mpr.dll si carica al primo uso (unità di rete, sidebar.cpp)
#include <ole2.h>
#include <objbase.h>
#include <commctrl.h>       // solo tipi (IImageList): nessuna funzione di comctl32 è usata
#include <commoncontrols.h> // icone grandi della shell (anteprima)
#include <wincodec.h>       // WIC, caricato alla prima immagine dell'anteprima
#include <thumbcache.h>     // cache delle miniature di Windows (griglia)
#include <winhttp.h>        // aggiornamento: winhttp.dll si carica sul thread del controllo (update.cpp)
#include <bcrypt.h>         // aggiornamento: impronta SHA-256 e firma ECDSA dell'exe scaricato
#include <d3d11.h>
#include <dxgi1_3.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>        // shared_ptr: archivi zip letti, condivisi tra i thread (zip.cpp)
#include <functional>    // lavori della lista principale rimandati alla fine di un PaneScope (app.h)
#include <algorithm>
#include <iterator>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

// Shader precompilati da build.bat (fxc). Se mancano, si compila a runtime.
#if defined(__has_include)
#if __has_include("shader_vs.h") && __has_include("shader_ps.h")
#include "shader_vs.h"
#include "shader_ps.h"
#define ZF_PRECOMPILED_SHADERS 1
#endif
#endif

// ============================================================================
// Utilità
// ============================================================================

static double g_qpcFreq = 1.0;
static double nowMs() {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / g_qpcFreq;
}

// Registro dei rallentamenti: z-f-log.txt accanto all'exe. Scrive solo quando qualcosa supera una soglia.
// Chi registra (quasi sempre il thread UI, proprio quando è già in ritardo) formatta la riga e la
// mette in coda; il file lo apre e lo scrive un thread breve, come per saveDataAsync. All'uscita la
// coda si scrive direttamente (logFlush). In un crash no: il lock potrebbe essere preso, e le righe
// in coda sono di pochi microsecondi prima.
static SRWLOCK g_logLock = SRWLOCK_INIT;  // la coda
static SRWLOCK g_logWrite = SRWLOCK_INIT; // il file: una scrittura alla volta, in ordine
static std::string g_logQueue;
static bool g_logWriter = false;          // un thread di scrittura è in giro

static void logFlush() {
    AcquireSRWLockExclusive(&g_logWrite);
    for (;;) {
        std::string s;
        AcquireSRWLockExclusive(&g_logLock);
        s.swap(g_logQueue);
        if (s.empty()) g_logWriter = false;
        ReleaseSRWLockExclusive(&g_logLock);
        if (s.empty()) break;
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
        while (n && path[n - 1] != L'\\') n--;
        wcscpy(path + n, L"z-f-log.txt");
        HANDLE f = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE) continue;
        DWORD w;
        WriteFile(f, s.data(), (DWORD)s.size(), &w, nullptr);
        CloseHandle(f);
    }
    ReleaseSRWLockExclusive(&g_logWrite);
}

static DWORD WINAPI logThread(void*) { logFlush(); return 0; }

static void hitchLog(const char* what, unsigned code, double ms) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[256];
    int len = _snprintf(buf, 255, "%02u:%02u:%02u.%03u  %-22s 0x%04X  %7.1f ms\r\n", st.wHour, st.wMinute, st.wSecond,
                        st.wMilliseconds, what, code, ms);
    AcquireSRWLockExclusive(&g_logLock);
    g_logQueue.append(buf, (size_t)std::max(0, std::min(len, 255)));
    bool start = !g_logWriter;
    g_logWriter = true;
    ReleaseSRWLockExclusive(&g_logLock);
    if (!start) return; // quello in giro la scriverà
    HANDLE h = CreateThread(nullptr, 0, logThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
    else logFlush(); // senza thread (non dovrebbe succedere): meglio scrivere che perdere la riga
}

static inline uint32_t rgba(int r, int g, int b, int a = 255) {
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

// Stesso colore con l'alfa moltiplicato per a (0..1): dissolvenze
static inline uint32_t fadeA(uint32_t c, float a) {
    float k = a <= 0 ? 0 : (a >= 1 ? 1 : a);
    return (c & 0x00FFFFFF) | ((uint32_t)((c >> 24) * k + 0.5f) << 24);
}


// Messaggi interni (thread di lavoro → finestra)
#define ZF_VERSION L"1.0.0-beta.8" // versione (SemVer) mostrata in fondo alle opzioni
#include "risorse.h" // ID delle risorse (ZF_ICON), in comune con src\z-f.rc

#define WM_APP_LISTING (WM_APP + 1)
#define WM_APP_GPU     (WM_APP + 2)
#define WM_APP_DIRCHANGED (WM_APP + 3)
#define WM_APP_DIRMOVED   (WM_APP + 4)
#define WM_APP_ICON       (WM_APP + 5)
#define WM_APP_FSDONE     (WM_APP + 6)
#define WM_APP_SIDEBAR    (WM_APP + 7)
#define WM_APP_INFO       (WM_APP + 8)
#define WM_APP_CRUMB      (WM_APP + 9)
#define WM_APP_FAVDROP    (WM_APP + 10)
#define WM_APP_DROPDONE   (WM_APP + 11)
#define WM_APP_GOTO       (WM_APP + 12)
#define WM_APP_PREVIEW    (WM_APP + 13)
#define WM_APP_FILEOP     (WM_APP + 14)
#define WM_APP_THUMB      (WM_APP + 15)
#define WM_APP_NET        (WM_APP + 16)
#define WM_APP_DIRSIZE    (WM_APP + 17)
#define WM_APP_OPENTAB    (WM_APP + 18) // lParam: new std::wstring, cartella da aprire in una scheda
#define WM_APP_DEFAULT    (WM_APP + 19) // "[z]f al posto di Explorer" scritto: wParam acceso, lParam riuscito
#define WM_APP_CLIPTEXT   (WM_APP + 20) // testo degli appunti letto da un thread: lParam ClipText*
#define WM_APP_SHELLMENU  (WM_APP + 21) // menu completo della shell chiuso (shell.cpp): lParam SmRes*
#define WM_APP_ZIP        (WM_APP + 22) // estrazione da uno zip finita (zip.cpp): lParam ZipJob*
#define WM_APP_LINKS      (WM_APP + 23) // destinazioni dei collegamenti lette (links.cpp)
#define WM_APP_SIDESLOW   (WM_APP + 24) // voce lenta della barra laterale letta, o fine del giro (sidebar.cpp): lParam SlowItem*
#define WM_APP_START      (WM_APP + 25) // schede da riaprire all'avvio lette (tabs.cpp): lParam StartTabs*
#define WM_APP_UPDATE     (WM_APP + 26) // controllo o download dell'aggiornamento finito (update.cpp): lParam UpdJob*
#define WM_APP_UPDCLEAN   (WM_APP + 27) // exe della versione precedente tolto dopo un aggiornamento (update.cpp)
#define WM_APP_CLIPSET    (WM_APP + 28) // testo scritto negli appunti da un thread, o no (text.cpp): lParam ClipSet*
#define WM_APP_PVCHANGED  (WM_APP + 29) // cartella dell'anteprima cambiata (watch.cpp): wParam generazione, lParam WATCH_*

template <class T> static void safeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

static double processAgeMs() {
    typedef void(WINAPI * PFN)(LPFILETIME);
    static PFN precise = (PFN)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemTimePreciseAsFileTime");
    FILETIME c, e, k, u, now;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    if (precise) precise(&now); else GetSystemTimeAsFileTime(&now);
    uint64_t a = ((uint64_t)c.dwHighDateTime << 32) | c.dwLowDateTime;
    uint64_t b = ((uint64_t)now.dwHighDateTime << 32) | now.dwLowDateTime;
    return (double)(b - a) / 10000.0;
}
