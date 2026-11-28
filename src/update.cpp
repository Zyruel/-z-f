// [z]f — aggiornamento automatico dalle Release di GitHub.
//
// Un thread a priorità bassa chiede a GitHub le Release (API pubblica, senza login), sceglie la
// versione più alta del canale (stabili, o anche beta), scarica l'exe e la sua firma, controlla
// dimensione, impronta SHA-256 (quella che dà GitHub) e firma ECDSA P-256 (chiave privata solo sul
// PC di chi pubblica: strumenti\firma.cpp), poi lo mette al posto di quello in uso con due
// rinomine (Windows permette di rinominare un exe in esecuzione): z-f.exe → z-f.old.exe, il nuovo
// → z-f.exe. La versione nuova parte al prossimo avvio, o subito dalle opzioni ("Aggiorna"), che chiude e
// riapre tutte le finestre di [z]f con le loro schede (riavvio-<pid>.txt, tabs.cpp).
// Il thread dell'interfaccia non tocca mai la rete né il disco: riceve solo il risultato.
// winhttp.dll e bcrypt.dll si caricano sul thread, al primo controllo (MSVC: /DELAYLOAD).

static const wchar_t kUpdApi[] = L"https://api.github.com/repos/Zyruel/-z-f/releases?per_page=10";
// La Release di questa versione (novità e download): la apre un clic sulla versione nelle opzioni
static const wchar_t kUpdThisRelease[] = L"https://github.com/Zyruel/-z-f/releases/tag/v" ZF_VERSION;

// Chiave pubblica della firma dei rilasci (BCRYPT_ECCPUBLIC_BLOB: "ECS1", 32, X, Y)
static const uint8_t kUpdKey[72] = {
    0x45, 0x43, 0x53, 0x31, 0x20, 0x00, 0x00, 0x00, 0x4e, 0xc3, 0xf6, 0xa9,
    0xe0, 0x50, 0x15, 0x83, 0x39, 0xf4, 0xde, 0xc4, 0xa9, 0x8b, 0x2b, 0xcc,
    0xd0, 0xd3, 0xb7, 0x87, 0x46, 0x91, 0x34, 0x31, 0x3d, 0x32, 0xa2, 0xd2,
    0x9f, 0x2a, 0xa4, 0xc9, 0xcd, 0xd9, 0x50, 0x90, 0x92, 0x64, 0xfe, 0x92,
    0xb5, 0xa3, 0x21, 0x72, 0x55, 0xcf, 0xdf, 0xb9, 0x33, 0x73, 0x7e, 0x95,
    0x7c, 0xf9, 0x5f, 0x5f, 0x0c, 0xf5, 0xc4, 0x96, 0xb7, 0xd8, 0x3d, 0x77,
};

enum UpdState { UP_IDLE, UP_CHECKING, UP_CURRENT, UP_READY, UP_MANUAL, UP_ERROR };

static struct Upd {
    int st = UP_IDLE;
    bool running = false;
    uint64_t gen = 0;
    std::wstring version;  // versione nuova (UP_READY, UP_MANUAL)
    std::wstring page;     // pagina della sua Release (UP_MANUAL: si scarica a mano)
    std::wstring base;     // --aggiorna-da=<url>: un server di prova al posto di GitHub (releases.json)
    double checkAt = 0;    // inizio dell'ultimo controllo: l'anello resta almeno un giro (drawUpdateIcon)
} U;

static UINT g_msgRestart = 0; // "riavviati con l'exe nuovo", tra le finestre di [z]f (RegisterWindowMessage)

static bool updBeta() { return A.updBeta >= 0 ? A.updBeta == 1 : wcschr(ZF_VERSION, L'-') != nullptr; }

static long long unixNow() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return (long long)((((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000000ULL) - 11644473600LL;
}

// --- JSON: quanto basta per la risposta delle Release --------------------------------------------
struct JVal {
    enum Type : uint8_t { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } t = J_NULL;
    bool b = false;
    std::string s;              // stringa (UTF-8), o il testo di un numero
    std::vector<JVal> a;        // elementi dell'array, o valori dell'oggetto
    std::vector<std::string> k; // chiavi dell'oggetto, stesso indice di a
    const JVal* get(const char* key) const {
        for (size_t i = 0; i < k.size(); i++) if (k[i] == key) return &a[i];
        return nullptr;
    }
    std::string str(const char* key) const { const JVal* v = get(key); return v && v->t == J_STR ? v->s : std::string(); }
    bool flag(const char* key) const { const JVal* v = get(key); return v && v->t == J_BOOL && v->b; }
    long long num(const char* key) const { const JVal* v = get(key); return v && v->t == J_NUM ? _atoi64(v->s.c_str()) : -1; }
};

struct JParser {
    const char *p, *e;
    int depth = 0;
    void ws() { while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
    bool lit(const char* w) {
        size_t n = strlen(w);
        if ((size_t)(e - p) < n || memcmp(p, w, n)) return false;
        p += n;
        return true;
    }
    bool hex4(uint32_t& v) {
        if (e - p < 4) return false;
        v = 0;
        for (int i = 0; i < 4; i++) {
            char c = *p++;
            v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 99);
            if (v > 0xFFFF) return false;
        }
        return true;
    }
    static void utf8(std::string& o, uint32_t c) {
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    }
    bool str(std::string& o) {
        if (p >= e || *p != '"') return false;
        p++;
        while (p < e && *p != '"') {
            char c = *p++;
            if (c != '\\') { o += c; continue; }
            if (p >= e) return false;
            char x = *p++;
            if (x == 'n') o += '\n';
            else if (x == 't') o += '\t';
            else if (x == 'r') o += '\r';
            else if (x == 'b') o += '\b';
            else if (x == 'f') o += '\f';
            else if (x == 'u') {
                uint32_t c1;
                if (!hex4(c1)) return false;
                if (c1 >= 0xD800 && c1 <= 0xDBFF && e - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                    p += 2;
                    uint32_t c2;
                    if (!hex4(c2)) return false;
                    c1 = 0x10000 + ((c1 - 0xD800) << 10) + (c2 - 0xDC00);
                }
                utf8(o, c1);
            } else o += x; // \" \\ \/
        }
        if (p >= e) return false;
        p++;
        return true;
    }
    bool val(JVal& v) {
        if (++depth > 64) return false;
        ws();
        bool ok = p < e;
        if (!ok) {
        } else if (*p == '{' || *p == '[') {
            bool obj = *p++ == '{';
            v.t = obj ? JVal::J_OBJ : JVal::J_ARR;
            ws();
            if (p < e && *p == (obj ? '}' : ']')) p++;
            else for (;;) {
                if (obj) {
                    ws();
                    std::string key;
                    if (!str(key)) { ok = false; break; }
                    ws();
                    if (p >= e || *p != ':') { ok = false; break; }
                    p++;
                    v.k.push_back(std::move(key));
                }
                v.a.emplace_back();
                if (!val(v.a.back())) { ok = false; break; }
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == (obj ? '}' : ']')) { p++; break; }
                ok = false;
                break;
            }
        } else if (*p == '"') {
            v.t = JVal::J_STR;
            ok = str(v.s);
        } else if (lit("true")) { v.t = JVal::J_BOOL; v.b = true; }
        else if (lit("false")) v.t = JVal::J_BOOL;
        else if (lit("null")) v.t = JVal::J_NULL;
        else {
            const char* b = p;
            while (p < e && ((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E')) p++;
            v.t = JVal::J_NUM;
            v.s.assign(b, p);
            ok = p > b;
        }
        depth--;
        return ok;
    }
};

// --- versioni (Semantic Versioning: 1.0.0-beta.2 < 1.0.0-beta.10 < 1.0.0-rc.1 < 1.0.0) ----------
struct SemVer { long long n[3] = {}; std::vector<std::string> pre; bool ok = false; };

static bool allDigits(const std::string& s) { return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos; }

static SemVer semverParse(std::string s) {
    SemVer v;
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s.erase(0, 1);
    size_t plus = s.find('+'); // i metadati di build non contano
    if (plus != std::string::npos) s.resize(plus);
    std::string core = s, pre;
    size_t dash = s.find('-');
    if (dash != std::string::npos) { core = s.substr(0, dash); pre = s.substr(dash + 1); }
    int i = 0;
    for (size_t st = 0;;) {
        size_t d = core.find('.', st);
        std::string part = core.substr(st, d == std::string::npos ? std::string::npos : d - st);
        if (i > 2 || !allDigits(part)) return v;
        v.n[i++] = _atoi64(part.c_str());
        if (d == std::string::npos) break;
        st = d + 1;
    }
    if (i != 3) return v;
    for (size_t st = 0; dash != std::string::npos;) {
        size_t d = pre.find('.', st);
        std::string id = pre.substr(st, d == std::string::npos ? std::string::npos : d - st);
        if (id.empty()) return v;
        v.pre.push_back(id);
        if (d == std::string::npos) break;
        st = d + 1;
    }
    v.ok = true;
    return v;
}

static int semverCmp(const SemVer& a, const SemVer& b) {
    for (int i = 0; i < 3; i++)
        if (a.n[i] != b.n[i]) return a.n[i] < b.n[i] ? -1 : 1;
    if (a.pre.empty() || b.pre.empty()) return a.pre.empty() == b.pre.empty() ? 0 : a.pre.empty() ? 1 : -1;
    for (size_t i = 0; i < a.pre.size() && i < b.pre.size(); i++) {
        const std::string &x = a.pre[i], &y = b.pre[i];
        bool nx = allDigits(x), ny = allDigits(y);
        if (nx && ny) {
            long long p = _atoi64(x.c_str()), q = _atoi64(y.c_str());
            if (p != q) return p < q ? -1 : 1;
        } else if (nx != ny) return nx ? -1 : 1; // i numeri vengono prima delle parole
        else if (x != y) return x < y ? -1 : 1;
    }
    return a.pre.size() == b.pre.size() ? 0 : a.pre.size() < b.pre.size() ? -1 : 1;
}

// --- rete (WinHTTP) -------------------------------------------------------------------------
#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

static std::wstring netError(DWORD e) {
    if (e == ERROR_WINHTTP_NAME_NOT_RESOLVED || e == ERROR_WINHTTP_CANNOT_CONNECT) return L"nessuna connessione";
    if (e == ERROR_WINHTTP_TIMEOUT) return L"la rete non risponde";
    return L"errore di rete " + std::to_wstring(e);
}

// GET di url in out (al massimo maxBytes). api: risposta JSON dell'API di GitHub
static bool httpGet(const std::wstring& url, size_t maxBytes, bool api, std::string& out, std::wstring& err) {
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = uc.dwHostNameLength = uc.dwUrlPathLength = uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) { err = L"indirizzo non valido"; return false; }
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path = std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength) + std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    // Proxy come il sistema; prima di Windows 8.1 c'era solo quello predefinito
    HINTERNET s = WinHttpOpen(L"z-f/" ZF_VERSION, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(L"z-f/" ZF_VERSION, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) { err = netError(GetLastError()); return false; }
    WinHttpSetTimeouts(s, ui::UPD_TIMEOUT, ui::UPD_TIMEOUT, ui::UPD_TIMEOUT, ui::UPD_TIMEOUT_READ);
    HINTERNET c = WinHttpConnect(s, host.c_str(), uc.nPort, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    const wchar_t* hdr = api ? L"Accept: application/vnd.github+json\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS;
    bool ok = false;
    DWORD status = 0, sz = sizeof(status);
    if (r && WinHttpSendRequest(r, hdr, api ? (DWORD)-1L : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr) &&
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX)) {
        if (status != 200) err = L"GitHub risponde " + std::to_wstring(status);
        else for (ok = true;;) {
            DWORD avail = 0, got = 0;
            if (!WinHttpQueryDataAvailable(r, &avail)) { ok = false; err = netError(GetLastError()); break; }
            if (!avail) break;
            if (out.size() + avail > maxBytes) { ok = false; err = L"risposta troppo grande"; break; }
            size_t at = out.size();
            out.resize(at + avail);
            if (!WinHttpReadData(r, &out[at], avail, &got)) { ok = false; err = netError(GetLastError()); break; }
            out.resize(at + got);
        }
    } else if (err.empty()) err = netError(GetLastError());
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok;
}

// --- impronta e firma (CNG) ---------------------------------------------------------------------
static bool sha256(const void* data, size_t n, uint8_t out[32]) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return false;
    if (BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0) {
        ok = BCryptHashData(h, (PUCHAR)data, (ULONG)n, 0) == 0 && BCryptFinishHash(h, out, 32, 0) == 0;
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

static bool signatureOk(const uint8_t hash[32], const std::string& sig) {
    if (sig.size() != 64) return false;
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) return false;
    if (BCryptImportKeyPair(alg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, (PUCHAR)kUpdKey, sizeof(kUpdKey), 0) == 0) {
        ok = BCryptVerifySignature(key, nullptr, (PUCHAR)hash, 32, (PUCHAR)sig.data(), 64, 0) == 0;
        BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

// "sha256:<64 cifre esadecimali>" dell'API di GitHub
static bool parseDigest(const std::string& d, uint8_t out[32]) {
    if (d.size() != 7 + 64 || d.compare(0, 7, "sha256:")) return false;
    auto nib = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
    for (int i = 0; i < 32; i++) {
        int hi = nib(d[7 + i * 2]), lo = nib(d[8 + i * 2]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)(hi * 16 + lo);
    }
    return true;
}

static bool readWhole(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart < ui::UPD_EXE_MAX;
    if (ok) {
        out.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        ok = out.empty() || (ReadFile(h, &out[0], (DWORD)out.size(), &got, nullptr) && got == out.size());
    }
    CloseHandle(h);
    return ok;
}

static bool writeWhole(const std::wstring& path, const std::string& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    bool ok = WriteFile(h, data.data(), (DWORD)data.size(), &w, nullptr) && w == data.size();
    CloseHandle(h);
    if (!ok) DeleteFileW(path.c_str());
    return ok;
}

// La copia della versione precedente, accanto all'exe: "z-f.exe" → "z-f.old.exe"
static std::wstring oldExeName(const std::wstring& exe, const wchar_t* suffix = L"") {
    size_t dot = exe.find_last_of(L'.'), slash = exe.find_last_of(L'\\');
    std::wstring stem = dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash) ? exe.substr(0, dot) : exe;
    return stem + L".old" + suffix + L".exe";
}

// --- il lavoro sul thread -------------------------------------------------------------------
struct UpdJob {
    HWND hwnd;
    uint64_t gen;
    bool beta, manual;
    std::wstring exe, api;
    bool checkOnly = false; // build di prova: si controlla, ma non si scarica né si sostituisce l'exe
    int st = UP_ERROR;
    bool rejected = false; // scaricato ma rifiutato (impronta o firma): si dice sempre, anche nel controllo automatico
    std::wstring version, page, err;
    double checkMs = 0, downloadMs = 0;
};

static void updFinish(UpdJob* j) {
    if (!PostMessageW(j->hwnd, WM_APP_UPDATE, 0, (LPARAM)j)) delete j;
}

static DWORD WINAPI updThread(void* p) {
    UpdJob* j = (UpdJob*)p;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    double t0 = nowMs();
    std::string json;
    if (!httpGet(j->api, ui::UPD_JSON_MAX, true, json, j->err)) { j->checkMs = nowMs() - t0; updFinish(j); return 0; }
    JVal root;
    JParser jp{ json.data(), json.data() + json.size() };
    if (!jp.val(root) || root.t != JVal::J_ARR) { j->err = L"risposta di GitHub non valida"; j->checkMs = nowMs() - t0; updFinish(j); return 0; }
    // La versione più alta del canale, più nuova di questa
    SemVer cur = semverParse(toUtf8(ZF_VERSION)), best;
    const JVal* rel = nullptr;
    for (const JVal& r : root.a) {
        if (r.t != JVal::J_OBJ || r.flag("draft") || (r.flag("prerelease") && !j->beta)) continue;
        SemVer v = semverParse(r.str("tag_name"));
        if (!v.ok || semverCmp(v, cur) <= 0 || (rel && semverCmp(v, best) <= 0)) continue;
        best = v;
        rel = &r;
    }
    j->checkMs = nowMs() - t0;
    if (!rel) { j->st = UP_CURRENT; updFinish(j); return 0; }
    std::string ver = rel->str("tag_name");
    if (!ver.empty() && (ver[0] == 'v' || ver[0] == 'V')) ver.erase(0, 1);
    j->version = fromUtf8(ver);
    j->page = fromUtf8(rel->str("html_url"));
    if (j->checkOnly) { j->st = UP_MANUAL; j->err = L"la build di prova non si sostituisce"; updFinish(j); return 0; }
    // L'exe e la sua firma tra i file della Release
    std::string name = "z-f-" + ver + ".exe", exeUrl, sigUrl;
    long long size = -1;
    uint8_t digest[32];
    bool haveDigest = false;
    if (const JVal* assets = rel->get("assets"))
        for (const JVal& a : assets->a) {
            std::string n = a.str("name");
            if (n == name) { exeUrl = a.str("browser_download_url"); size = a.num("size"); haveDigest = parseDigest(a.str("digest"), digest); }
            else if (n == name + ".sig") sigUrl = a.str("browser_download_url");
        }
    j->st = UP_MANUAL; // da qui in poi un problema lascia almeno il collegamento alla Release
    if (exeUrl.empty() || sigUrl.empty() || !haveDigest || size <= 0 || size > ui::UPD_EXE_MAX) {
        j->err = L"la Release non ha l'exe firmato";
        updFinish(j);
        return 0;
    }
    // Un'altra finestra di [z]f ha già messo al suo posto la versione nuova
    std::wstring old = oldExeName(j->exe);
    std::string disk;
    uint8_t h[32];
    if (GetFileAttributesW(old.c_str()) != INVALID_FILE_ATTRIBUTES && readWhole(j->exe, disk) &&
        sha256(disk.data(), disk.size(), h) && !memcmp(h, digest, 32)) {
        j->st = UP_READY;
        updFinish(j);
        return 0;
    }
    double t1 = nowMs();
    std::string sig, bin;
    bool got = httpGet(fromUtf8(sigUrl), 4096, false, sig, j->err) && httpGet(fromUtf8(exeUrl), ui::UPD_EXE_MAX, false, bin, j->err);
    j->downloadMs = nowMs() - t1;
    if (!got) { j->st = UP_ERROR; updFinish(j); return 0; }
    if ((long long)bin.size() != size || !sha256(bin.data(), bin.size(), h) || memcmp(h, digest, 32)) {
        j->st = UP_ERROR;
        j->rejected = true;
        j->err = L"il file scaricato non corrisponde a quello pubblicato";
        updFinish(j);
        return 0;
    }
    if (!signatureOk(h, sig)) {
        j->st = UP_ERROR;
        j->rejected = true;
        j->err = L"la firma dell'exe non è valida";
        updFinish(j);
        return 0;
    }
    // Accanto all'exe (stesso disco), poi lo scambio: due rinomine. Se la copia precedente è ancora
    // in uso (una finestra della versione di prima) non si può sostituire: se ne fa un'altra.
    std::wstring tmp = j->exe + L".nuovo";
    if (!writeWhole(tmp, bin)) { j->err = L"la cartella di [z]f non è scrivibile"; updFinish(j); return 0; }
    if (!MoveFileExW(j->exe.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        old = oldExeName(j->exe, (L"-" + std::to_wstring(GetTickCount64())).c_str());
        if (!MoveFileExW(j->exe.c_str(), old.c_str(), 0)) {
            DeleteFileW(tmp.c_str());
            j->err = L"l'exe di [z]f non si può sostituire";
            updFinish(j);
            return 0;
        }
    }
    if (!MoveFileExW(tmp.c_str(), j->exe.c_str(), 0)) {
        MoveFileExW(old.c_str(), j->exe.c_str(), 0); // si rimette com'era
        DeleteFileW(tmp.c_str());
        j->err = L"l'exe di [z]f non si può sostituire";
        updFinish(j);
        return 0;
    }
    j->st = UP_READY;
    updFinish(j);
    return 0;
}

// --- thread dell'interfaccia ----------------------------------------------------------------

// manual: chiesto dall'utente (clic sull'icona accanto alla versione): risponde sempre con un avviso
static void updCheck(bool manual) {
    if (U.running || U.st == UP_READY) return;
    bool checkOnly = false;
#ifdef ZF_DEV
    // La build di prova si aggiorna ricompilando: controlla solo a richiesta, per provare l'icona
    if (U.base.empty()) { if (!manual) return; checkOnly = true; }
#endif
    // Il controllo automatico lo fa solo la finestra principale (con il server di prova, ogni finestra)
    if (!manual && (!A.updAuto || (!A.mainInstance && U.base.empty()))) return;
    std::wstring exe = exePath();
    if (exe.empty() || slowPath(exe)) { // su una condivisione: lo scambio lo farebbe la rete, a metà
        if (manual) notice(L"[z]f è su un'unità di rete: gli aggiornamenti si scaricano a mano", false);
        return;
    }
    UpdJob* j = new UpdJob{ A.hwnd, ++U.gen, updBeta(), manual, exe, U.base.empty() ? std::wstring(kUpdApi) : U.base + L"/releases.json" };
    j->checkOnly = checkOnly;
    HANDLE h = CreateThread(nullptr, 0, updThread, j, 0, nullptr);
    if (!h) { delete j; return; }
    CloseHandle(h);
    U.running = true;
    U.st = UP_CHECKING;
    U.checkAt = nowMs();
    A.dirty = true;
}

static void onUpdate(UpdJob* j) {
    std::unique_ptr<UpdJob> own(j);
    U.running = false;
    if (j->gen != U.gen) return;
    U.st = j->st;
    U.version = j->version;
    U.page = j->page;
    hitchLog("aggiornamento: controllo", (unsigned)j->st, j->checkMs);
    if (j->downloadMs > 0) hitchLog("aggiornamento: download", (unsigned)j->st, j->downloadMs);
    if (!j->err.empty()) hitchLog(("aggiornamento: " + toUtf8(j->err)).c_str(), (unsigned)j->st, 0); // il perché, per chi legge il registro
    if (j->st != UP_ERROR) { A.updLast = unixNow(); settingsSave(); }
    std::wstring v = L"[z]f " + j->version;
    if (j->st == UP_READY) notice(v + L" è pronto: parte al prossimo avvio, o subito dalle opzioni", true);
    else if (j->st == UP_MANUAL) notice(L"C'è " + v + L", ma " + j->err + L": si scarica dalle opzioni", false);
    else if (j->manual && j->st == UP_CURRENT) notice(L"[z]f " ZF_VERSION L" è l'ultima versione", true);
    else if (j->rejected) notice(v + L" scaricato e scartato: " + j->err, false);
    else if (j->manual) notice(L"Aggiornamenti non controllati: " + j->err, false);
    A.dirty = true;
}

// Riavvio con l'exe nuovo: tutte le finestre di [z]f (anche quelle aperte con "Nuova finestra")
// salvano le schede ed escono, e ognuna si riapre al suo posto (main.cpp, tabsSaveOnExit)
static void restartBegin(bool clicked) {
    if (A.restartOnExit || A.closePending) return;
    A.restartOnExit = true;
    A.restartActive = clicked || GetForegroundWindow() == A.hwnd;
    A.restartPlace.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(A.hwnd, &A.restartPlace);
    PostMessageW(A.hwnd, WM_CLOSE, 0, 0); // con operazioni sui file in corso aspetta che finiscano
}

static void updRestartAll() {
    for (HWND w = nullptr; (w = FindWindowExW(nullptr, w, ZF_ID, nullptr)) != nullptr;)
        if (w != A.hwnd) PostMessageW(w, g_msgRestart, 0, 0);
    restartBegin(true);
}

// Una pagina web nel browser predefinito: ShellExecute può metterci, la chiama un thread
static void updOpenPage(const wchar_t* url) {
    HANDLE h = CreateThread(nullptr, 0, openThread, _wcsdup(url), 0, nullptr);
    if (h) CloseHandle(h);
}

// Clic sull'icona accanto alla versione nelle opzioni: aggiorna, apre la Release, o controlla di nuovo
static void updAction() {
    if (U.st == UP_READY) updRestartAll();
    else if (U.st == UP_MANUAL && !U.page.empty()) updOpenPage(U.page.c_str());
    else updCheck(true);
}

// Fine processo, dopo saveFlush: la finestra si riapre con l'exe nuovo, dov'era
static void restartLaunch() {
    std::wstring exe = exePath();
    const RECT& r = A.restartPlace.rcNormalPosition;
    int state = A.restartPlace.showCmd == SW_SHOWMAXIMIZED ? 1 : A.restartPlace.showCmd == SW_SHOWMINIMIZED ? 2 : 0;
    wchar_t b[160];
    _snwprintf(b, 159, L" --riavvio=%lu --finestra=%ld,%ld,%ld,%ld,%d,%d%s", GetCurrentProcessId(), r.left, r.top,
               r.right, r.bottom, state, A.restartActive ? 1 : 0, A.mainInstance ? L" --principale" : L"");
    b[159] = 0;
    std::wstring cmd = L"\"" + exe + L"\"" + b;
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    if (CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

// Dopo il primo frame: via la copia della versione precedente (se nessuno la usa più) e il primo
// controllo più tardi, per non contendere disco e rete all'avvio
struct UpdClean { HWND hwnd; std::wstring exe; DWORD restartPid; };

static DWORD WINAPI updCleanThread(void* p) {
    std::unique_ptr<UpdClean> c((UpdClean*)p);
    const std::wstring* exe = &c->exe;
    // Dopo "Aggiorna" la versione di prima lancia questa mentre sta ancora uscendo: finché il suo
    // processo c'è, il suo exe (z-f.old.exe) non si può cancellare. Si aspetta, al massimo un po'
    if (c->restartPid) {
        HANDLE old = OpenProcess(SYNCHRONIZE, FALSE, c->restartPid);
        if (old) { WaitForSingleObject(old, ui::UPD_OLD_WAIT); CloseHandle(old); }
    }
    std::wstring pattern = oldExeName(*exe, L"*"), dir = exe->substr(0, exe->find_last_of(L'\\') + 1);
    bool removed = false;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do if (DeleteFileW((dir + fd.cFileName).c_str())) removed = true;
        while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    DeleteFileW((*exe + L".nuovo").c_str()); // rimasto da un aggiornamento interrotto
    // "Aggiornato a …": dopo un riavvio dall'aggiornamento, o quando si toglie la copia di prima
    if (removed || c->restartPid) PostMessageW(c->hwnd, WM_APP_UPDCLEAN, 0, 0);
    return 0;
}

static void updStartup() {
    std::wstring exe = exePath();
    if (!exe.empty() && !slowPath(exe)) {
        UpdClean* c = new UpdClean{ A.hwnd, exe, A.restartPid };
        HANDLE h = CreateThread(nullptr, 0, updCleanThread, c, 0, nullptr);
        if (h) CloseHandle(h); else delete c;
    }
    // Con --aggiorna-da (prove) il controllo parte quasi subito e non aspetta un giorno dall'ultimo
    if (!U.base.empty()) A.updLast = 0;
    SetTimer(A.hwnd, 15, U.base.empty() ? ui::UPD_DELAY : ui::UPD_DELAY_TEST, nullptr);
}

static void updOnTimer() { // ogni tanto: un giorno dopo l'ultimo controllo se ne fa un altro
    SetTimer(A.hwnd, 15, ui::UPD_TICK, nullptr);
    if (unixNow() - A.updLast >= ui::UPD_EVERY) updCheck(false);
}

static void onUpdClean() {
    notice(L"Aggiornato a [z]f " ZF_VERSION, true);
}

// --- disegno --------------------------------------------------------------------------------
// Icona dello stato, centrata in (x, y, w, h): anello di punti mentre controlla, spunta se è
// aggiornato, freccia di download se c'è una versione nuova, frecce circolari se non si sa ancora
static float updIconW() { return (float)FI.lineH; }

static void drawUpdateIcon(float x, float y, float w, float h, float k) {
    // Un controllo dura 300–800 ms, meno di un giro: l'anello resta fino al giro intero, se no si
    // vedrebbe solo un lampo. Senza animazioni l'anello è fermo, e il risultato si mostra subito
    bool spin = U.st == UP_CHECKING || (A.animations && U.checkAt > 0 && nowMs() - U.checkAt < ui::SPIN_PERIOD);
    if (spin) { drawSpinner(x + w * 0.5f, y + h * 0.5f, floorf(FI.lineH * ui::SPIN_SIZE), fadeA(C_DIM, k)); return; }
    uint32_t cp = IC_SYNC, col = C_FAINT;
    if (U.st == UP_CURRENT) { cp = IC_DONE; col = C_DIM; }
    else if (U.st == UP_READY || U.st == UP_MANUAL) { cp = IC_DOWNLOAD; col = C_ACCENT; }
    drawUiIcon(x, y, w, h, cp, fadeA(col, k));
}

static bool updBadge() { return U.st == UP_READY || U.st == UP_MANUAL; }

// Testo della versione nel piede delle opzioni e nel tooltip di ⚙
static std::wstring updMenuLabel() {
    if (U.st == UP_READY) return L"Aggiorna a [z]f " + U.version;
    if (U.st == UP_MANUAL) return L"Scarica [z]f " + U.version;
    return L"[z]f " ZF_VERSION;
}

// "oggi alle 10:42", "ieri alle 18:05", "il 25/11 alle 9:30"
static std::wstring updLastText() {
    if (A.updLast <= 0) return L"";
    uint64_t t = (uint64_t)(A.updLast + 11644473600LL) * 10000000ULL;
    FILETIME ft = { (DWORD)t, (DWORD)(t >> 32) }, lt, nowFt, nowLt;
    SYSTEMTIME s, n;
    if (!FileTimeToLocalFileTime(&ft, &lt) || !FileTimeToSystemTime(&lt, &s)) return L"";
    GetSystemTimeAsFileTime(&nowFt);
    FileTimeToLocalFileTime(&nowFt, &nowLt);
    FileTimeToSystemTime(&nowLt, &n);
    auto day = [](const FILETIME& f) { return ((((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime) / 10000000ULL) / 86400; };
    uint64_t d0 = day(lt), d1 = day(nowLt);
    wchar_t b[64];
    if (d0 == d1) _snwprintf(b, 63, L"oggi alle %u:%02u", s.wHour, s.wMinute);
    else if (d0 + 1 == d1) _snwprintf(b, 63, L"ieri alle %u:%02u", s.wHour, s.wMinute);
    else _snwprintf(b, 63, L"il %u/%u alle %u:%02u", s.wDay, s.wMonth, s.wHour, s.wMinute);
    b[63] = 0;
    return b;
}
