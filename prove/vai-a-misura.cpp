// Misura a parte della ricerca di "Vai a" (goto.cpp) nel caso peggiore, con le stesse funzioni
// (foldText, fuzzyScore, fuzzyRest, gotoChildPath, gotoTop e il ciclo di gotoSearchFuzzy, copiati).
// Domanda: ogni cartella che arriva dal thread rifà tutta la ricerca (onGotoDir → gotoSearch):
// conviene raggrupparle?
//  - sintetico: 30 cartelle attive con 3000 sottocartelle ciascuna (DIR_MAX), 90.000 nomi;
//  - reale: le cartelle che "Vai a" leggerebbe ora (preferiti e 20 recenti di %APPDATA%\z-f).
// Per ogni ricerca: il tempo con tutte le cartelle, e la somma delle ricerche rifatte mentre le
// cartelle arrivano una alla volta (come onGotoDir).
//
// Compilare (da Bash, nella cartella del progetto; l'exe va in build\):
//   cmd //c '.\build.bat prova prove\vai-a-misura.cpp user32.lib'
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>
#include <random>

static double freq;
static double nowMs() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart * 1000.0 / freq; }

// --- da goto.cpp ------------------------------------------------------------------------------
static void foldText(const wchar_t* s, size_t n, std::wstring& out) {
    out.resize(n);
    for (size_t i = 0; i < n; i++) {
        wchar_t c = s[i];
        if (c < 0x80) {
            if (c >= L'A' && c <= L'Z') c += 32;
            else if (c == L'/') c = L'\\';
        } else {
            wchar_t b[4];
            if (FoldStringW(MAP_COMPOSITE, &c, 1, b, 4) > 0) c = b[0];
            CharLowerBuffW(&c, 1);
        }
        out[i] = c;
    }
}
static void foldText(const std::wstring& s, std::wstring& out) { foldText(s.c_str(), s.size(), out); }

static inline bool gotoWordSep(wchar_t c) {
    return c == L' ' || c == L'\\' || c == L'-' || c == L'_' || c == L'.' || c == L'(' || c == L'[';
}
static int fuzzyScore(const wchar_t* t, int n, const wchar_t* o, const wchar_t* q, int m, uint8_t* marks) {
    if (m == 0) return 0;
    if (m > n) return -1;
    int j = 0, end = -1;
    for (int i = 0; i < n; i++) if (t[i] == q[j] && ++j == m) { end = i; break; }
    if (end < 0) return -1;
    int start = end;
    j = m - 1;
    for (int i = end; i >= 0; i--) if (t[i] == q[j] && --j < 0) { start = i; break; }
    int score = 0, prev = -2;
    j = 0;
    for (int i = start; i <= end && j < m; i++) {
        if (t[i] != q[j]) continue;
        int s = 16;
        if (i == 0) s += 10;
        else if (gotoWordSep(t[i - 1])) s += 8;
        else if (o && o[i] >= L'A' && o[i] <= L'Z' && o[i - 1] >= L'a' && o[i - 1] <= L'z') s += 7;
        if (prev == i - 1) s += 8;
        else if (j > 0) s -= std::min(i - prev - 1, 10) + 2;
        if (marks) marks[i] = 1;
        score += s;
        prev = i;
        j++;
    }
    score -= std::min(start, 15) / 3;
    if (start == 0 && end == m - 1) score += n == m ? 40 : 20;
    score -= std::min(n - m, 40) / 4;
    return score;
}
static bool fuzzyRest(const std::wstring& t, const std::wstring& q, int j) {
    for (size_t i = 0; i < t.size() && j < (int)q.size(); i++) if (t[i] == q[j]) j++;
    return j == (int)q.size();
}

struct GoDir { std::wstring path, fold; std::vector<std::wstring> names, folds; std::vector<uint8_t> hidden; };
struct GoRes { int dir, idx, score; uint8_t where; };
static std::vector<GoDir> dirs;
static std::vector<GoRes> res;

static void gotoChildPath(const GoDir& d, int c, std::wstring& buf) {
    buf = d.fold;
    if (!buf.empty() && buf.back() != L'\\') buf += L'\\';
    buf += d.folds[c];
}
static std::wstring joinPath(const std::wstring& dir, const wchar_t* name) {
    if (dir.empty()) return name;
    if (dir.back() == L'\\') return dir + name;
    return dir + L'\\' + name;
}
static bool samePath(const std::wstring& a, const std::wstring& b) { return !_wcsicmp(a.c_str(), b.c_str()); }
static void gotoTop(size_t keep) {
    std::vector<GoRes>& R = res;
    auto better = [](const GoRes& a, const GoRes& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.dir != b.dir ? a.dir < b.dir : a.idx < b.idx;
    };
    size_t k = std::min(R.size(), keep * 2);
    std::partial_sort(R.begin(), R.begin() + k, R.end(), better);
    R.resize(k);
    std::vector<GoRes> out;
    std::vector<std::wstring> seen;
    for (const GoRes& r : R) {
        std::wstring p = joinPath(dirs[r.dir].path, dirs[r.dir].names[r.idx].c_str());
        bool dup = false;
        for (const std::wstring& s : seen) if (samePath(s, p)) { dup = true; break; }
        if (dup) continue;
        seen.push_back(std::move(p));
        out.push_back(r);
        if (out.size() >= keep) break;
    }
    R.swap(out);
}
// Il ciclo sulle sottocartelle di gotoSearchFuzzy (le voci in memoria sono al massimo ~300: a parte)
static void searchDirs(const std::wstring& text, int active) {
    std::wstring Q;
    foldText(text, Q);
    bool pathQuery = Q.find(L'\\') != std::wstring::npos;
    const wchar_t* q = Q.c_str();
    int m = (int)Q.size();
    res.clear();
    std::wstring buf;
    for (int a = 0; a < active; a++) {
        const GoDir& d = dirs[a];
        int jd = 0;
        for (size_t i = 0; i < d.fold.size() && jd < m; i++) if (d.fold[i] == q[jd]) jd++;
        if (jd < m && q[jd] == L'\\' && !d.fold.empty() && d.fold.back() != L'\\') jd++;
        for (int c = 0; c < (int)d.names.size(); c++) {
            const std::wstring& f = d.folds[c];
            int s = pathQuery ? -1 : fuzzyScore(f.c_str(), (int)f.size(), d.names[c].c_str(), q, m, nullptr);
            uint8_t where = 0;
            if (s < 0) {
                if (!fuzzyRest(f, Q, jd)) continue;
                gotoChildPath(d, c, buf);
                s = fuzzyScore(buf.c_str(), (int)buf.size(), nullptr, q, m, nullptr);
                if (s < 0) continue;
                where = 1;
                if (!pathQuery) s += -30;
            }
            res.push_back({ a, c, s + (d.hidden[c] ? -6 : 0), where });
        }
    }
    gotoTop(50);
}
// --- fine delle copie -------------------------------------------------------------------------

static void addDir(const std::wstring& path, std::vector<std::wstring> names) {
    GoDir d;
    d.path = path;
    foldText(path, d.fold);
    if (names.size() > 3000) names.resize(3000);
    for (std::wstring& n : names) {
        d.folds.emplace_back();
        foldText(n, d.folds.back());
        d.hidden.push_back(0);
        d.names.push_back(std::move(n));
    }
    dirs.push_back(std::move(d));
}

static std::vector<std::wstring> subfolders(const std::wstring& path) {
    std::vector<std::wstring> v;
    WIN32_FIND_DATAW fd;
    std::wstring pat = path + (path.back() == L'\\' ? L"*" : L"\\*");
    HANDLE h = FindFirstFileExW(pat.c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return v;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        v.push_back(fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return v;
}

static std::vector<std::wstring> readLines(const wchar_t* file, size_t max) {
    std::vector<std::wstring> out;
    wchar_t p[MAX_PATH];
    GetEnvironmentVariableW(L"APPDATA", p, MAX_PATH);
    std::wstring f = std::wstring(p) + L"\\z-f\\" + file;
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, f.c_str(), L"rb") || !fp) return out;
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) s.append(buf, n);
    fclose(fp);
    size_t a = 0;
    while (a < s.size() && out.size() < max) {
        size_t b = s.find('\n', a);
        if (b == std::string::npos) b = s.size();
        std::string l = s.substr(a, b - a);
        a = b + 1;
        size_t t = l.find('\t');
        if (t != std::string::npos) l.resize(t);
        while (!l.empty() && (l.back() == '\r')) l.pop_back();
        if (l.empty()) continue;
        int wn = MultiByteToWideChar(CP_UTF8, 0, l.c_str(), (int)l.size(), nullptr, 0);
        std::wstring w(wn, 0);
        MultiByteToWideChar(CP_UTF8, 0, l.c_str(), (int)l.size(), &w[0], wn);
        out.push_back(w);
    }
    return out;
}

static void run(const wchar_t* title, const std::vector<const wchar_t*>& queries) {
    size_t total = 0;
    for (const GoDir& d : dirs) total += d.names.size();
    wprintf(L"%s: %d cartelle, %zu nomi\n", title, (int)dirs.size(), total);
    for (const wchar_t* qq : queries) {
        double best = 1e9, worst = 0;
        for (int rep = 0; rep < 5; rep++) {
            double t0 = nowMs();
            searchDirs(qq, (int)dirs.size());
            double d = nowMs() - t0;
            best = std::min(best, d);
            worst = std::max(worst, d);
        }
        double sum = 0, maxStep = 0;
        for (int k = 1; k <= (int)dirs.size(); k++) { // le cartelle arrivano una alla volta
            double t0 = nowMs();
            searchDirs(qq, k);
            double d = nowMs() - t0;
            sum += d;
            maxStep = std::max(maxStep, d);
        }
        wprintf(L"  \"%s\": tutte %.2f ms (peggiore %.2f), %zu risultati; arrivo una alla volta: somma %.1f ms, passo peggiore %.2f ms\n",
                qq, best, worst, res.size(), sum, maxStep);
    }
}

int wmain() {
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); freq = (double)f.QuadPart;
    std::vector<const wchar_t*> queries = { L"a", L"e", L"foto", L"users", L"utente", L"users\\utente", L"progetti 2024", L"zzqx" };

    // Sintetico: nomi plausibili da parole comuni, percorsi sotto C:\Users\utente (le lettere di
    // "users" e "utente" sono tutte nel percorso: ogni sottocartella passa al confronto col percorso)
    const wchar_t* words[] = { L"Foto", L"Documenti", L"Progetti", L"lavoro", L"vacanze", L"2024", L"2025", L"backup",
                               L"Musica", L"video", L"archivio", L"clienti", L"Fatture", L"src", L"build", L"test",
                               L"Nuova cartella", L"old", L"copia", L"bozze", L"scansioni", L"Mare", L"Roma", L"progetto" };
    std::mt19937 rng(7);
    for (int d = 0; d < 30; d++) {
        std::vector<std::wstring> names;
        for (int i = 0; i < 3000; i++) {
            std::wstring n = words[rng() % 24];
            n += L' ';
            n += words[rng() % 24];
            n += L' ';
            n += std::to_wstring(rng() % 10000);
            names.push_back(n);
        }
        addDir(L"C:\\Users\\utente\\Documents\\Archivio\\cartella " + std::to_wstring(d), std::move(names));
    }
    run(L"sintetico (caso peggiore)", queries);

    dirs.clear();
    std::vector<std::wstring> paths = readLines(L"preferiti.txt", 1000);
    for (const std::wstring& p : readLines(L"recenti.txt", 20)) paths.push_back(p); // già in ordine di rango
    for (const std::wstring& p : paths) addDir(p, subfolders(p));
    run(L"reale (preferiti e 20 recenti di questo PC)", queries);
    return 0;
}
