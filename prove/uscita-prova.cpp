// Che cosa tiene vivo un processo dopo la chiusura della finestra: ogni 100 ms legge la pila di
// ogni suo thread (DbgHelp di Windows, solo i nomi esportati delle DLL: niente simboli scaricati) e
// la scrive quando cambia, con il tempo dall'avvio della prova, finché il processo esce.
// Così si è visto che a tenerlo vivo era il thread delle voci lente della barra laterale, dentro una
// chiamata alla rete (docs/ARCHITETTURA.md, "Barra laterale").
//
// Compilare (da Bash, nella cartella del progetto; l'exe va in build\):
//   cmd //c '.\build.bat prova prove\uscita-prova.cpp dbghelp.lib'
// Uso: build\uscita-prova.exe <pid> [secondi massimi, predefinito 30]
// Di solito si avvia subito dopo aver chiesto la chiusura (taskkill /PID <pid>, senza /F).
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>

static double freq;
static double nowMs() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart * 1000.0 / freq; }

static std::string frameName(HANDLE proc, DWORD64 a) {
    char buf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* si = (SYMBOL_INFO*)buf;
    si->SizeOfStruct = sizeof(SYMBOL_INFO);
    si->MaxNameLen = 255;
    IMAGEHLP_MODULE64 m{};
    m.SizeOfStruct = sizeof(m);
    std::string mod = SymGetModuleInfo64(proc, a, &m) ? m.ModuleName : "?";
    char out[512];
    DWORD64 disp = 0;
    if (SymFromAddr(proc, a, &disp, si)) snprintf(out, sizeof(out), "%s!%s+0x%llx", mod.c_str(), si->Name, (unsigned long long)disp);
    else if (mod != "?") snprintf(out, sizeof(out), "%s+0x%llx", mod.c_str(), (unsigned long long)(a - m.BaseOfImage));
    else snprintf(out, sizeof(out), "0x%llx", (unsigned long long)a);
    return out;
}

static std::string stackOf(HANDLE proc, DWORD tid) {
    HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
    if (!th) return "(thread non apribile)";
    std::string s;
    if (SuspendThread(th) != (DWORD)-1) {
        CONTEXT c{};
        c.ContextFlags = CONTEXT_FULL;
        if (GetThreadContext(th, &c)) {
            STACKFRAME64 f{};
            f.AddrPC.Offset = c.Rip; f.AddrPC.Mode = AddrModeFlat;
            f.AddrFrame.Offset = c.Rbp; f.AddrFrame.Mode = AddrModeFlat;
            f.AddrStack.Offset = c.Rsp; f.AddrStack.Mode = AddrModeFlat;
            for (int i = 0; i < 40; i++) {
                if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, th, &f, &c, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
                    break;
                if (!f.AddrPC.Offset) break;
                s += "      " + frameName(proc, f.AddrPC.Offset) + "\n";
            }
        }
        ResumeThread(th);
    }
    CloseHandle(th);
    return s;
}

int main(int argc, char** argv) {
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); freq = (double)f.QuadPart;
    if (argc < 2) { printf("uso: uscita-prova <pid> [secondi]\n"); return 1; }
    DWORD pid = (DWORD)strtoul(argv[1], nullptr, 10);
    double maxMs = (argc > 2 ? atof(argv[2]) : 30) * 1000;
    HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, pid);
    if (!proc) { printf("processo %lu non apribile (%lu)\n", pid, GetLastError()); return 1; }
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_EXACT_SYMBOLS);
    SymInitialize(proc, "", TRUE); // solo i moduli caricati, nessun percorso di simboli
    double t0 = nowMs();
    std::map<DWORD, std::string> last;
    while (nowMs() - t0 < maxMs) {
        if (WaitForSingleObject(proc, 0) == WAIT_OBJECT_0) { printf("%7.0f ms: uscito\n", nowMs() - t0); return 0; }
        HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        std::map<DWORD, std::string> now;
        if (sn != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te{ sizeof(te) };
            for (BOOL ok = Thread32First(sn, &te); ok; ok = Thread32Next(sn, &te))
                if (te.th32OwnerProcessID == pid) now[te.th32ThreadID] = stackOf(proc, te.th32ThreadID);
            CloseHandle(sn);
        }
        if (now != last) {
            printf("%7.0f ms: %zu thread\n", nowMs() - t0, now.size());
            for (auto& kv : now) {
                auto it = last.find(kv.first);
                if (it != last.end() && it->second == kv.second) continue;
                printf("   thread %lu\n%s", kv.first, kv.second.c_str());
            }
            fflush(stdout);
            last.swap(now);
        }
        Sleep(100);
    }
    printf("%7.0f ms: ancora vivo, smetto\n", nowMs() - t0);
    return 0;
}
