// [z]f — lettura delle cartelle su thread di lavoro, chiavi di ordinamento naturale, ordinamento.

struct Entry {
    uint32_t nameOff;
    uint32_t nameLen;
    uint32_t keyOff;  // chiave di ordinamento in Listing::keys
    uint32_t keyLen;
    uint32_t attr;
    uint64_t size;  // per le unità: tipo di unità (GetDriveTypeW)
    uint64_t mtime; // FILETIME come uint64
};

// Destinazione di un collegamento (.lnk), letta su un thread (links.cpp). path vuoto: nessun
// percorso su disco (app dello Store, Pannello di controllo) o file illeggibile
struct LinkTarget { std::wstring path; bool dir = false; bool known = false; };

struct Listing {
    std::wstring          path;   // "" = Questo PC (elenco unità)
    std::vector<Entry>    e;
    std::vector<wchar_t>  names;  // tutti i nomi, terminati da 0
    std::vector<uint16_t> keys;   // chiavi di ordinamento (token big-endian, confrontabili con memcmp)
    std::vector<wchar_t>  lower;  // i nomi in minuscolo, stessi offset di names: per il filtro
    std::vector<uint32_t> order;  // indici ordinati
    DWORD                 err = 0;
    double                enumMs = 0, keyMs = 0, sortMs = 0;
    uint64_t              gen = 0;
    bool                  isDrives = false;
    uint32_t              zipLen = 0;   // dentro uno zip: lunghezza del percorso dell'archivio in path (zip.cpp)
    bool                  refresh = false; // ricarica automatica: conserva selezione, scroll e filtro
    // Collegamenti (links.cpp): le voci .lnk in ordine di indice e la loro destinazione. Se ce n'è
    // almeno uno, l'elenco ha la colonna Destinazione.
    std::vector<uint32_t>   lnk;
    std::vector<LinkTarget> lnkTo;
    const wchar_t* name(const Entry& x) const { return names.data() + x.nameOff; }
    const wchar_t* lowerName(const Entry& x) const { return lower.data() + x.nameOff; }
};

enum { COL_NAME = 0, COL_DATE = 1, COL_SIZE = 2, COL_LINK = 3 };

static void dirSizeFill(Listing* l); // dirsize.cpp: per ordinare per dimensione, le cartelle col totale noto
static void linksFind(Listing* l);   // links.cpp: voci .lnk, con le destinazioni già lette

// Percorso di rete di Windows (lnk.cpp): nome mostrato e cartella a cui porta
struct NetPlace { std::wstring name, path; };
static std::vector<NetPlace> netPlacesRead(); // lnk.cpp, solo su un thread
// In Questo PC il tipo di unità di una voce (Entry::size) è quello di GetDriveTypeW; un percorso
// di rete ne ha uno nostro, fuori dai valori di Windows
constexpr UINT DRIVE_NETPLACE = 100;

// ---------------------------------------------------------------------------
// Ordinamento naturale con chiavi precalcolate
//
// StrCmpLogicalW rianalizza le due stringhe a ogni confronto (≈ N·log N volte).
// Qui ogni nome viene trasformato UNA volta in una sequenza di token da 16 bit:
//   punteggiatura/spazi   0x01..0x21   (rango ASCII compattato: prima di tutto il resto)
//   numero                0x40 + numero di cifre significative (max 32), poi 0x30+cifra per ogni cifra
//                         → "2" < "10" perché ha meno cifre; gli zeri iniziali sono ignorati
//   lettere               carattere minuscolo, accenti rimossi (è → e)
// I token sono salvati big-endian, quindi il confronto è un semplice memcmp (vettorizzato dalla CRT).
// A parità di chiave decide il confronto ordinale del nome originale (maiuscole, zeri iniziali).
// ---------------------------------------------------------------------------

static uint16_t g_punctRank[128];
static void initPunctRank() {
    uint16_t r = 1;
    for (int c = 0; c < 128; c++) {
        bool alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        g_punctRank[c] = alnum ? 0 : (c < 0x20 ? 1 : r++);
    }
}

static inline uint16_t be16(uint32_t v) { return (uint16_t)(((v & 0xFF) << 8) | ((v >> 8) & 0xFF)); }

// Scrive i token in out e ne ritorna il numero. Il chiamante garantisce kKeyMax token di spazio:
// dopo la scomposizione (FoldStringW, al massimo 1024 caratteri) il nome può essere più lungo
// dell'originale (Hangul: una sillaba diventa 2–3 lettere), e una cifra isolata vale 2 token.
static constexpr int kKeyFold = 1024, kKeyMax = 2 * kKeyFold;
static uint32_t buildKey(const wchar_t* s, int n, uint16_t* out) {
    wchar_t buf[kKeyFold];
    const wchar_t* src = s;
    int m = n;
    for (int i = 0; i < n; i++) {
        if (s[i] >= 0x80) {
            // Scompone i caratteri accentati (è → e + accento) e porta tutto in minuscolo
            int r = FoldStringW(MAP_COMPOSITE, s, n, buf, kKeyFold);
            if (r <= 0) { r = std::min(n, kKeyFold); memcpy(buf, s, r * sizeof(wchar_t)); }
            CharLowerBuffW(buf, (DWORD)r);
            src = buf;
            m = r;
            break;
        }
    }
    uint16_t* o = out;
    for (int i = 0; i < m;) {
        uint32_t c = src[i];
        if (c >= L'0' && c <= L'9') {
            int j = i;
            while (j < m && src[j] == L'0') j++;
            int k = j;
            while (k < m && src[k] >= L'0' && src[k] <= L'9') k++;
            *o++ = be16(0x40u + (uint32_t)std::min(k - j, 32));
            for (int p = j; p < k; p++) *o++ = be16((uint32_t)src[p]); // '0'..'9' = 0x30..0x39
            i = k;
            continue;
        }
        i++;
        if (c < 0x80) {
            if (c >= 'A' && c <= 'Z') c += 32;
            *o++ = be16((c >= 'a' && c <= 'z') ? c : g_punctRank[c]);
        } else if (c < 0x300 || c > 0x36F) { // gli accenti combinanti vengono ignorati
            *o++ = be16(c);
        }
    }
    return (uint32_t)(o - out);
}

static void buildKeys(Listing* l) {
    l->keys.resize(l->names.size() * 2 + 64);
    uint32_t pos = 0;
    for (Entry& x : l->e) {
        size_t need = pos + kKeyMax;
        if (l->keys.size() < need) l->keys.resize(std::max(need, l->keys.size() * 2));
        x.keyOff = pos;
        x.keyLen = buildKey(l->name(x), (int)x.nameLen, l->keys.data() + pos);
        pos += x.keyLen;
    }
    l->keys.resize(pos);
}

static inline int keyCmp(const Listing* l, const Entry& x, const Entry& y) {
    const uint16_t* K = l->keys.data();
    uint32_t n = std::min(x.keyLen, y.keyLen);
    int c = memcmp(K + x.keyOff, K + y.keyOff, n * sizeof(uint16_t));
    if (c) return c;
    if (x.keyLen != y.keyLen) return x.keyLen < y.keyLen ? -1 : 1;
    return wcscmp(l->name(x), l->name(y));
}

// Nomi in minuscolo: ciclo semplice per l'ASCII (quasi tutti i nomi), CharLowerBuffW solo
// per i nomi che contengono altri caratteri. Su 30k nomi: circa 1 ms invece di 4,5.
static void buildLower(Listing* l) {
    l->lower.resize(l->names.size());
    const wchar_t* src = l->names.data();
    wchar_t* dst = l->lower.data();
    for (const Entry& x : l->e) {
        bool ascii = true;
        for (uint32_t i = 0; i < x.nameLen; i++) {
            wchar_t c = src[x.nameOff + i];
            if (c >= 0x80) ascii = false;
            dst[x.nameOff + i] = (c >= L'A' && c <= L'Z') ? (wchar_t)(c + 32) : c;
        }
        dst[x.nameOff + x.nameLen] = 0;
        if (!ascii) CharLowerBuffW(dst + x.nameOff, x.nameLen);
    }
}

static void sortListing(const Listing* l, std::vector<uint32_t>& o, int col, bool desc) {
    o.resize(l->e.size());
    for (uint32_t i = 0; i < (uint32_t)o.size(); i++) o[i] = i;
    const Entry* E = l->e.data();
    // Per destinazione: la stringa di ogni voce (nullptr se non è un collegamento con destinazione)
    std::vector<const std::wstring*> tg;
    if (col == COL_LINK) {
        tg.assign(l->e.size(), nullptr);
        for (size_t k = 0; k < l->lnk.size(); k++)
            if (!l->lnkTo[k].path.empty()) tg[l->lnk[k]] = &l->lnkTo[k].path;
    }
    std::sort(o.begin(), o.end(), [&](uint32_t a, uint32_t b) {
        const Entry& x = E[a];
        const Entry& y = E[b];
        bool dx = (x.attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
        bool dy = (y.attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (dx != dy) return dx; // cartelle sempre prima
        int c = 0;
        if (col == COL_DATE) c = x.mtime < y.mtime ? -1 : (x.mtime > y.mtime ? 1 : 0);
        else if (col == COL_SIZE) c = x.size < y.size ? -1 : (x.size > y.size ? 1 : 0); // cartelle: totale noto o 0
        else if (col == COL_LINK) {
            const std::wstring *p = tg[a], *q = tg[b];
            if (!p != !q) return p != nullptr; // senza destinazione in fondo, in tutti e due i versi
            if (p) c = _wcsicmp(p->c_str(), q->c_str());
        }
        if (c == 0) c = keyCmp(l, x, y);
        if (c == 0) c = a < b ? -1 : (a > b ? 1 : 0);
        return desc ? c > 0 : c < 0;
    });
}

// Prefisso \\?\ per superare MAX_PATH
static std::wstring longPath(const std::wstring& p) {
    if (p.size() >= 2 && p[0] == L'\\' && p[1] == L'\\') {
        if (p.size() >= 4 && (p[2] == L'?' || p[2] == L'.')) return p;
        return L"\\\\?\\UNC\\" + p.substr(2);
    }
    return L"\\\\?\\" + p;
}

static bool zipSplit(const std::wstring& path, size_t& alen); // zip.cpp
static void zipList(Listing* l, size_t alen);                 // zip.cpp

struct ListRequest { HWND hwnd; uint64_t gen; std::wstring path; int sortCol; bool sortDesc; bool refresh; };

static void addEntry(Listing* l, const wchar_t* name, uint32_t len, uint32_t attr, uint64_t size, uint64_t mtime) {
    Entry x;
    x.nameLen = len;
    x.nameOff = (uint32_t)l->names.size();
    x.keyOff = x.keyLen = 0;
    x.attr = attr; x.size = size; x.mtime = mtime;
    l->names.insert(l->names.end(), name, name + len);
    l->names.push_back(0);
    l->e.push_back(x);
}

static inline bool isDots(const wchar_t* n, uint32_t len) {
    return (len == 1 && n[0] == L'.') || (len == 2 && n[0] == L'.' && n[1] == L'.');
}

static void enumFindFirst(Listing* l, const std::wstring& lp) {
    std::wstring q = lp;
    if (q.back() != L'\\') q += L'\\';
    q += L'*';
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(q.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr,
                                FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) {
        l->err = GetLastError();
        if (l->err == ERROR_FILE_NOT_FOUND) l->err = 0; // radice vuota
        return;
    }
    do {
        uint32_t len = (uint32_t)wcslen(fd.cFileName);
        if (isDots(fd.cFileName, len)) continue;
        uint64_t size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        uint64_t mt = ((uint64_t)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime;
        addEntry(l, fd.cFileName, len, fd.dwFileAttributes, size, mt);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static DWORD WINAPI listThread(void* p) {
    ListRequest* r = (ListRequest*)p;
    Listing* l = new Listing;
    l->path = r->path;
    l->gen = r->gen;
    l->refresh = r->refresh;
    double t0 = nowMs();

    if (r->path.empty()) {
        l->isDrives = true;
        wchar_t buf[512];
        DWORD n = GetLogicalDriveStringsW(511, buf);
        for (wchar_t* d = buf; d < buf + n && *d; d += wcslen(d) + 1)
            addEntry(l, d, (uint32_t)wcslen(d), FILE_ATTRIBUTE_DIRECTORY, GetDriveTypeW(d), 0);
        for (const NetPlace& np : netPlacesRead()) // come in Explorer, dopo le unità
            addEntry(l, np.path.c_str(), (uint32_t)np.path.size(), FILE_ATTRIBUTE_DIRECTORY, DRIVE_NETPLACE, 0);
    } else if (size_t alen = 0; zipSplit(r->path, alen)) { // uno zip, o una cartella dentro uno zip
        zipList(l, alen);
    } else {
        // Misurato: GetFileInformationByHandleEx con buffer da 1 MB non è più veloce
        // (WinSxS, 30k voci: 29,3 ms contro 27,9 ms). Il costo è in NTFS, non nelle chiamate.
        l->names.reserve(1 << 15);
        l->e.reserve(1 << 10);
        enumFindFirst(l, longPath(r->path));
        linksFind(l); // con le destinazioni già lette: in una ricarica la colonna non lampeggia
    }
    l->enumMs = nowMs() - t0;

    double t1 = nowMs();
    buildKeys(l);
    // Minuscole preparate una volta qui: il filtro diventa una ricerca di sottostringa semplice
    // (StrStrIW su 30k nomi lunghi costava circa 100 ms a ogni tasto)
    buildLower(l);
    l->keyMs = nowMs() - t1;

    t1 = nowMs();
    if (r->sortCol == COL_SIZE) dirSizeFill(l);
    sortListing(l, l->order, r->sortCol, r->sortDesc);
    l->sortMs = nowMs() - t1;

    if (!PostMessageW(r->hwnd, WM_APP_LISTING, 0, (LPARAM)l)) delete l;
    delete r;
    return 0;
}
