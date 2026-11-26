// Prova del lettore zip di [z]f (src/zip.cpp), senza interfaccia: legge la directory centrale e
// estrae tutto in una cartella, con i tempi. I file estratti si confrontano poi con gli originali
// (sha1, o Expand-Archive di Windows).
//
// Compilare (da Bash, nella cartella del progetto; l'exe va in build\):
//   cmd //c '.\build.bat prova prove\zip-prova.cpp'
// Uso:
//   build\zip-prova.exe archivio.zip cartella-di-uscita     (elenco ed estrazione)
//   build\zip-prova.exe archivio.zip                        (solo elenco, cinque volte: la prima legge il disco)

#include "../src/base.h"
#include "../src/theme.h"
#include "../src/listing.cpp"
static void dirSizeFill(Listing*) {}
static void linksFind(Listing*) {}
#include "../src/zip.cpp"
#include "../src/lnk.cpp" // netPlacesRead, chiamata da listing.cpp

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { fwprintf(stderr, L"uso: zip-prova archivio.zip [cartella]\n"); return 2; }
    SetConsoleOutputCP(CP_UTF8);
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcFreq = (double)fq.QuadPart;
    initPunctRank();
    if (!wcscmp(argv[1], L"--dentro") && argc >= 3) { // un percorso dentro uno zip, come lo usano lista, anteprima e menu
        std::wstring p = argv[2];
        size_t alen = 0;
        bool inside = zipInside(p, alen), split = zipSplit(p, alen);
        wprintf(L"zipSplit %d (archivio: %s), zipInside %d\n", split, split ? p.substr(0, alen).c_str() : L"-", inside);
        if (!split) return 1;
        Listing* l = new Listing;
        l->path = p;
        zipList(l, alen);
        wprintf(L"elenco: errore %lu, %zu voci\n", (unsigned long)l->err, l->e.size());
        for (size_t i = 0; i < l->e.size() && i < 8; i++)
            wprintf(L"  %s%s  %llu byte\n", l->name(l->e[i]), (l->e[i].attr & FILE_ATTRIBUTE_DIRECTORY) ? L"\\" : L"", (unsigned long long)l->e[i].size);
        std::vector<std::wstring> names;
        std::vector<uint8_t> hidden;
        bool ok = zipSubfolders(p, alen, names, hidden);
        wprintf(L"sottocartelle %d:", ok);
        for (auto& n : names) wprintf(L" [%s]", n.c_str());
        wprintf(L"\n");
        for (const Entry& e : l->e)
            if (!(e.attr & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string s;
                uint64_t size = 0;
                int r = zipReadMem(p + L"\\" + l->name(e), alen, 120, s, &size, nullptr, nullptr);
                wprintf(L"lettura in memoria di %s: esito %d, %zu byte su %llu\n", l->name(e), r, s.size(), (unsigned long long)size);
                break;
            }
        return 0;
    }
    std::wstring path = argv[1];
    wchar_t full[MAX_PATH];
    GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr);
    path = full;
    for (int k = 0; k < (argc >= 3 ? 1 : 5); k++) {
        double t0 = nowMs();
        DWORD err = 0;
        if (k) { AcquireSRWLockExclusive(&g_zipLock); g_zipCache.clear(); ReleaseSRWLockExclusive(&g_zipLock); }
        std::shared_ptr<const ZipArchive> z = zipGet(path, err);
        double t1 = nowMs();
        if (!z) { printf("errore %lu\n", (unsigned long)err); return 1; }
        // Come la lista: la radice in un Listing, con chiavi e ordinamento
        Listing* l = new Listing;
        l->path = path;
        zipList(l, path.size());
        buildKeys(l);
        buildLower(l);
        sortListing(l, l->order, COL_NAME, false);
        double t2 = nowMs();
        printf("voci %u, nodi %zu, radice %zu voci (%u cartelle, %u file), totale %llu byte\n", z->entries, z->nodes.size(), l->e.size(),
               z->nodes[0].dirs, z->nodes[0].files, (unsigned long long)z->nodes[0].size);
        printf("directory centrale %.2f ms, elenco della radice %.2f ms\n", t1 - t0, t2 - t1);
        delete l;
    }
    if (argc < 3) return 0;
    std::wstring out = argv[2];
    GetFullPathNameW(out.c_str(), MAX_PATH, full, nullptr);
    out = full;
    CreateDirectoryW(out.c_str(), nullptr);
    DWORD err = 0;
    std::shared_ptr<const ZipArchive> z = zipGet(path, err);
    HANDLE f = CreateFileW(longPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    ZipOutState s;
    double t0 = nowMs();
    for (uint32_t c = z->nodes[0].child; c != UINT32_MAX; c = z->nodes[c].next) zipExtractNode(f, *z, c, out, s);
    double ms = nowMs() - t0;
    CloseHandle(f);
    printf("estratti %llu byte in %.1f ms (%.0f MB/s), non riusciti %d", (unsigned long long)s.bytes, ms,
           s.bytes / 1048576.0 / (ms / 1000.0), s.failed);
    if (s.failed) wprintf(L" (primo: %s, motivo %d)", s.firstName.c_str(), s.firstErr);
    printf("\n");
    return s.failed ? 1 : 0;
}
