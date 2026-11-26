// Misura a parte dei costi per frame segnalati dal controllo (principio 2), con lo stesso codice
// di [z]f: selCount su tutta la vista, dirSizeGet e iconFor per ogni riga visibile.
// Compilare (da Bash, nella cartella del progetto; l'exe va in build\):
//   cmd //c '.\build.bat prova prove\costi-frame.cpp user32.lib'
// Novembre 2026 (Ryzen 9 7900): selCount 30.734 voci 0,020 ms; dirSizeGet 60 righe 0,003 ms; iconFor 0,001 ms.
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <random>

static double freq;
static double nowMs() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart * 1000.0 / freq; }

struct Entry { uint32_t attr; uint64_t size; };
struct DirSize { uint64_t bytes = 0, files = 0; bool denied = false; };

static std::wstring joinPath(const std::wstring& dir, const wchar_t* name) {
    if (dir.empty()) return name;
    if (dir.back() == L'\\') return dir + name;
    return dir + L'\\' + name;
}
static std::wstring dsKey(const std::wstring& path) {
    std::wstring k = path;
    while (k.size() > 3 && k.back() == L'\\') k.pop_back();
    if (!k.empty()) CharLowerBuffW(&k[0], (DWORD)k.size());
    return k;
}

int main() {
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); freq = (double)f.QuadPart;
    const int N = 30734; // WinSxS
    std::vector<Entry> e(N);
    std::vector<uint32_t> view(N);
    std::vector<uint8_t> sel(N);
    std::mt19937 rng(1);
    for (int i = 0; i < N; i++) { e[i] = { (i % 3) ? 0u : (uint32_t)FILE_ATTRIBUTE_DIRECTORY, (uint64_t)rng() }; view[i] = i; sel[i] = (i % 2) != 0; }
    std::shuffle(view.begin(), view.end(), rng); // ordine della vista diverso da quello di lettura
    std::vector<std::wstring> names(N);
    for (int i = 0; i < N; i++) names[i] = L"amd64_microsoft-windows-componente-" + std::to_wstring(i) + L"_31bf3856ad364e35";
    std::wstring dir = L"C:\\Windows\\WinSxS";

    // DS: cache dei totali (come dirsize.cpp) con le cartelle viste
    CRITICAL_SECTION lock; InitializeCriticalSection(&lock);
    std::unordered_map<std::wstring, DirSize> cache;
    for (int i = 0; i < N; i += 3) cache[dsKey(joinPath(dir, names[i].c_str()))] = DirSize{ 1234, 5 };
    std::wstring ask[2] = { L"c:\\windows\\winsxs\\x", L"" }, liveKey[2] = { L"", L"" };
    bool askPending[2] = { true, false };

    const int ROWS = 60, REPS = 2000;
    volatile uint64_t sink = 0;
    double best[3] = { 1e9, 1e9, 1e9 };
    for (int r = 0; r < REPS; r++) {
        int first = (r * 37) % (N - ROWS);
        // 1. selCount
        double t0 = nowMs();
        int n = 0; uint64_t b = 0;
        for (uint32_t ei : view) if (sel[ei]) { n++; if (!(e[ei].attr & FILE_ATTRIBUTE_DIRECTORY)) b += e[ei].size; }
        sink += n + b;
        best[0] = std::min(best[0], nowMs() - t0);
        // 2. dirSizeGet per le righe visibili (cartelle)
        t0 = nowMs();
        for (int i = first; i < first + ROWS; i++) {
            uint32_t ei = view[i];
            if (!(e[ei].attr & FILE_ATTRIBUTE_DIRECTORY)) continue;
            std::wstring k = dsKey(joinPath(dir, names[ei].c_str()));
            EnterCriticalSection(&lock);
            bool computing = false; DirSize out;
            for (int s = 0; s < 2; s++)
                if ((askPending[s] && dsKey(ask[s]) == k) || liveKey[s] == k) { computing = true; }
            auto it = cache.find(k);
            if (it != cache.end()) out = it->second;
            LeaveCriticalSection(&lock);
            sink += out.bytes + computing;
        }
        best[1] = std::min(best[1], nowMs() - t0);
        // 3. iconFor (file per estensione) per le righe visibili
        t0 = nowMs();
        for (int i = first; i < first + ROWS; i++) {
            const wchar_t* dot = wcsrchr(names[view[i]].c_str(), L'.');
            std::wstring key = dot ? dot : L"", path;
            for (auto& c : key) c = (wchar_t)towlower(c);
            if (key.empty()) key = L".";
            path = L"file" + key;
            sink += path.size();
        }
        best[2] = std::min(best[2], nowMs() - t0);
    }
    printf("selCount su %d voci:            %.4f ms\n", N, best[0]);
    printf("dirSizeGet per %d righe:          %.4f ms\n", ROWS, best[1]);
    printf("iconFor per %d righe:             %.4f ms\n", ROWS, best[2]);
    return (int)(sink & 1);
}
