// [z]f — archivi zip in sola lettura, senza la shell: elenco dalla directory centrale (anche
// zip64), inflate nostro, estrazione su un thread.
//
// Uno zip si apre come una cartella: il percorso continua oltre il nome dell'archivio
// ("C:\x\foto.zip\2024\mare"). Chi lavora su un thread riconosce questi percorsi con zipSplit
// (un GetFileAttributes sul prefisso che finisce in ".zip", solo se c'è); il thread UI li
// riconosce dalla cartella aperta (Listing::zipLen), senza disco.
//
// Perché non la shell (zipfldr): legge l'archivio a ogni passo e su uno zip grande impiega secondi.
// Qui la directory centrale si legge una volta (decine di migliaia di voci: pochi ms) e resta in
// memoria per gli ultimi archivi aperti; un file si decomprime solo quando serve.
// Metodi: "stored" e deflate, che coprono quasi tutti gli zip. Gli altri (deflate64, bzip2, LZMA,
// zstd) e gli zip cifrati si lasciano a Windows.

struct ZipNode {
    uint32_t nameOff = 0, nameLen = 0;  // nome (solo l'ultimo componente) in ZipArchive::names
    uint32_t parent = 0;                // cartella che lo contiene (la radice è il nodo 0)
    uint32_t child = UINT32_MAX;        // cartella: primo figlio; i fratelli in next
    uint32_t next = UINT32_MAX;
    uint32_t attr = 0;                  // FILE_ATTRIBUTE_*
    uint32_t dirs = 0, files = 0;       // cartella: figli diretti
    uint64_t size = 0;                  // file: dimensione; cartella: totale del contenuto
    uint64_t csize = 0, off = 0;        // dati compressi e intestazione locale
    uint64_t mtime = 0;                 // FILETIME (UTC)
    uint32_t crc = 0;
    uint16_t method = 0, flags = 0;
    bool explicitTime = false;          // cartella con una sua voce nell'archivio (altrimenti: la più recente dentro)
};

struct ZipArchive {
    std::wstring path;                  // l'archivio sul disco
    uint64_t fileSize = 0, fileTime = 0;// per sapere se è cambiato
    std::vector<ZipNode> nodes;
    std::vector<wchar_t> names;
    uint32_t entries = 0;               // voci della directory centrale
    double readMs = 0;
    const wchar_t* name(const ZipNode& n) const { return names.data() + n.nameOff; }
    bool isDir(const ZipNode& n) const { return (n.attr & FILE_ATTRIBUTE_DIRECTORY) != 0; }
};

enum ZipErr { ZE_OK, ZE_STOP, ZE_IO, ZE_DATA, ZE_METHOD, ZE_CRYPT, ZE_CANCEL };

static inline uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint64_t rd64(const uint8_t* p) { return (uint64_t)rd32(p) | (uint64_t)rd32(p + 4) << 32; }

// Componente che finisce in ".zip" (senza distinzione tra maiuscole e minuscole), da start a end
static bool zipNameEnd(const std::wstring& p, size_t end) {
    return end >= 4 && p[end - 4] == L'.' && towlower(p[end - 3]) == L'z' && towlower(p[end - 2]) == L'i' &&
           towlower(p[end - 1]) == L'p';
}

// Senza disco: c'è un componente che finisce in ".zip" (anche l'ultimo). Per le decisioni che non
// devono essere esatte (icona per estensione invece che per file): una cartella vera che si chiama
// "x.zip" darebbe solo icone generiche.
static bool zipPathLike(const std::wstring& p) {
    for (size_t i = 0; i <= p.size(); i++)
        if ((i == p.size() || p[i] == L'\\') && zipNameEnd(p, i)) return true;
    return false;
}

// Su un thread (tocca il disco): path è un archivio zip o un percorso dentro di esso. alen: lunghezza
// del percorso dell'archivio. Si guarda solo un prefisso che finisce in ".zip": nessun costo per
// tutti gli altri percorsi.
static bool zipSplit(const std::wstring& path, size_t& alen) {
    for (size_t i = 3; i <= path.size(); i++) {
        if (i < path.size() && path[i] != L'\\') continue;
        if (!zipNameEnd(path, i)) continue;
        DWORD a = GetFileAttributesW(longPath(path.substr(0, i)).c_str());
        if (a == INVALID_FILE_ATTRIBUTES) return false;
        if (!(a & FILE_ATTRIBUTE_DIRECTORY)) { alen = i; return true; }
    }
    return false;
}
// Dentro un archivio (non l'archivio stesso)
static bool zipInside(const std::wstring& path, size_t& alen) { return zipSplit(path, alen) && alen < path.size(); }

// --- directory centrale ----------------------------------------------------------------
static bool zipReadAt(HANDLE f, uint64_t off, void* buf, size_t n) {
    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)off;
    if (!SetFilePointerEx(f, li, nullptr, FILE_BEGIN)) return false;
    uint8_t* p = (uint8_t*)buf;
    while (n) {
        DWORD got = 0, want = (DWORD)std::min<size_t>(n, 1 << 24);
        if (!ReadFile(f, p, want, &got, nullptr) || !got) return false;
        p += got;
        n -= got;
    }
    return true;
}

static uint64_t dosTime(uint16_t t, uint16_t d) {
    SYSTEMTIME st = {};
    st.wYear = (WORD)(1980 + (d >> 9));
    st.wMonth = (WORD)std::max(1, (d >> 5) & 15);
    st.wDay = (WORD)std::max(1, d & 31);
    st.wHour = (WORD)(t >> 11);
    st.wMinute = (WORD)((t >> 5) & 63);
    st.wSecond = (WORD)((t & 31) * 2);
    FILETIME lt, ft;
    if (!SystemTimeToFileTime(&st, &lt) || !LocalFileTimeToFileTime(&lt, &ft)) return 0; // ora locale di chi l'ha creato
    return (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
}

// Caratteri che Windows non accetta in un nome: diventano "_". "." e ".." non sono nomi (e un ".."
// porterebbe l'estrazione fuori dalla cartella di destinazione).
static void zipCleanName(std::wstring& s) {
    for (wchar_t& c : s)
        if (c < 32 || c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'|' || c == L'?' || c == L'*') c = L'_';
    while (!s.empty() && (s.back() == L' ' || s.back() == L'.') && s != L"." && s != L"..") s.pop_back();
}

static uint32_t crc32Update(uint32_t crc, const uint8_t* p, size_t n);

// Legge la directory centrale di un archivio e costruisce l'albero delle cartelle
static DWORD zipParse(HANDLE f, uint64_t fsize, ZipArchive& z) {
    // Fine della directory centrale: negli ultimi 22 byte + commento (al massimo 64 KB)
    size_t tail = (size_t)std::min<uint64_t>(fsize, 22 + 65535 + 20);
    if (tail < 22) return ERROR_BAD_FORMAT;
    std::vector<uint8_t> t(tail);
    if (!zipReadAt(f, fsize - tail, t.data(), tail)) return GetLastError() ? GetLastError() : ERROR_READ_FAULT;
    size_t e = SIZE_MAX;
    for (size_t i = tail - 22 + 1; i-- > 0;)
        if (rd32(&t[i]) == 0x06054b50 && i + 22 + rd16(&t[i + 20]) <= tail) { e = i; break; }
    if (e == SIZE_MAX) return ERROR_BAD_FORMAT;
    uint64_t count = rd16(&t[e + 10]), cdSize = rd32(&t[e + 12]), cdOff = rd32(&t[e + 16]);
    if (rd16(&t[e + 4]) != rd16(&t[e + 6])) return ERROR_NOT_SUPPORTED; // su più dischi
    if (count == 0xFFFF || cdSize == 0xFFFFFFFF || cdOff == 0xFFFFFFFF) { // zip64: il localizzatore sta subito prima
        if (fsize - tail + e < 20) return ERROR_BAD_FORMAT;
        uint8_t loc[20], rec[56];
        if (e >= 20) memcpy(loc, &t[e - 20], 20);
        else if (!zipReadAt(f, fsize - tail + e - 20, loc, 20)) return ERROR_BAD_FORMAT;
        if (rd32(loc) != 0x07064b50) return ERROR_BAD_FORMAT;
        if (!zipReadAt(f, rd64(loc + 8), rec, 56) || rd32(rec) != 0x06064b50) return ERROR_BAD_FORMAT;
        count = rd64(rec + 32);
        cdSize = rd64(rec + 40);
        cdOff = rd64(rec + 48);
    }
    if (cdOff > fsize || cdSize > fsize - cdOff || cdSize > ((uint64_t)1 << 30) || count > cdSize / 46) return ERROR_BAD_FORMAT;
    std::vector<uint8_t> cd((size_t)cdSize);
    if (cdSize && !zipReadAt(f, cdOff, cd.data(), cd.size())) return ERROR_READ_FAULT;

    z.nodes.clear();
    z.names.clear();
    z.nodes.reserve((size_t)count + 64);
    z.names.reserve((size_t)cdSize);
    z.nodes.emplace_back(); // radice
    z.nodes[0].attr = FILE_ATTRIBUTE_DIRECTORY;
    z.names.push_back(0);
    // Nodo per (cartella, nome in minuscolo): per trovare le cartelle già create e le voci doppie.
    // Tabella a indirizzamento aperto sugli indici dei nodi, senza un'allocazione per voce (con una
    // mappa di percorsi 70.000 voci costavano 36 ms, quasi tutti in allocazioni).
    std::vector<wchar_t> lower;               // nomi in minuscolo, stessi offset di z.names
    lower.reserve((size_t)cdSize);
    lower.push_back(0);
    std::vector<uint32_t> slots;
    auto hashOf = [](uint32_t parent, const wchar_t* s, size_t n) {
        uint64_t h = 1469598103934665603ull ^ parent;
        for (size_t i = 0; i < n; i++) { h ^= s[i]; h *= 1099511628211ull; }
        return (size_t)(h ^ (h >> 29));
    };
    auto slotsGrow = [&](size_t want) {
        size_t cap = 64;
        while (cap < want * 2) cap <<= 1;
        slots.assign(cap, UINT32_MAX);
        for (uint32_t i = 1; i < (uint32_t)z.nodes.size(); i++) {
            const ZipNode& n = z.nodes[i];
            size_t j = hashOf(n.parent, &lower[n.nameOff], n.nameLen) & (cap - 1);
            while (slots[j] != UINT32_MAX) j = (j + 1) & (cap - 1);
            slots[j] = i;
        }
    };
    slotsGrow((size_t)count + 16);
    std::wstring wname, comp, lc;
    // Il nodo di comp (lc: in minuscolo) dentro parent, creato se manca
    auto child = [&](uint32_t parent, bool wantDir) -> uint32_t {
        size_t m = slots.size() - 1, j = hashOf(parent, lc.data(), lc.size()) & m;
        for (; slots[j] != UINT32_MAX; j = (j + 1) & m) {
            ZipNode& n = z.nodes[slots[j]];
            if (n.parent == parent && n.nameLen == lc.size() && !wmemcmp(&lower[n.nameOff], lc.data(), lc.size())) {
                if (wantDir) n.attr |= FILE_ATTRIBUTE_DIRECTORY; // un file e una cartella con lo stesso nome: vince la cartella
                return slots[j];
            }
        }
        uint32_t idx = (uint32_t)z.nodes.size();
        ZipNode n;
        n.nameOff = (uint32_t)z.names.size();
        n.nameLen = (uint32_t)comp.size();
        n.parent = parent;
        n.attr = wantDir ? FILE_ATTRIBUTE_DIRECTORY : 0;
        z.names.insert(z.names.end(), comp.begin(), comp.end());
        z.names.push_back(0);
        lower.insert(lower.end(), lc.begin(), lc.end());
        lower.push_back(0);
        ZipNode& par = z.nodes[parent];
        n.next = par.child;
        par.child = idx;
        z.nodes.push_back(n);
        slots[j] = idx;
        if (z.nodes.size() * 2 > slots.size()) slotsGrow(z.nodes.size()); // cartelle create in più delle voci
        return idx;
    };
    // Le voci di una cartella stanno quasi sempre una dopo l'altra: la cartella dell'ultima voce non si
    // ricerca di nuovo; lo stesso per l'ora, spesso uguale
    std::wstring prevDir;
    uint32_t prevParent = 0;
    bool havePrev = false;
    uint16_t prevTm = 0, prevDt = 0;
    uint64_t prevTime = 0;
    size_t p = 0;
    uint32_t got = 0;
    for (uint64_t k = 0; k < count; k++) {
        if (p + 46 > cd.size() || rd32(&cd[p]) != 0x02014b50) return ERROR_BAD_FORMAT;
        const uint8_t* h = &cd[p];
        uint16_t made = rd16(h + 4), flags = rd16(h + 8), method = rd16(h + 10);
        uint16_t tm = rd16(h + 12), dt = rd16(h + 14);
        uint32_t crc = rd32(h + 16);
        uint64_t csize = rd32(h + 20), usize = rd32(h + 24);
        uint16_t nlen = rd16(h + 28), xlen = rd16(h + 30), clen = rd16(h + 32);
        uint32_t ext = rd32(h + 38);
        uint64_t off = rd32(h + 42);
        if (p + 46 + nlen + xlen + clen > cd.size()) return ERROR_BAD_FORMAT;
        const uint8_t* nm = h + 46;
        const uint8_t* x = nm + nlen;
        if (!k || tm != prevTm || dt != prevDt) { prevTm = tm; prevDt = dt; prevTime = dosTime(tm, dt); }
        uint64_t mtime = prevTime;
        const uint8_t* utf8 = nullptr;
        size_t utf8Len = 0;
        // Campi extra: zip64 (misure e posizione oltre 4 GB), orari NTFS o Unix, nome Unicode di Info-ZIP
        for (size_t q = 0; q + 4 <= xlen;) {
            uint16_t id = rd16(x + q), sz = rd16(x + q + 2);
            const uint8_t* d = x + q + 4;
            if (q + 4 + sz > xlen) break;
            if (id == 0x0001) {
                size_t r = 0;
                if (usize == 0xFFFFFFFF && r + 8 <= sz) { usize = rd64(d + r); r += 8; }
                if (csize == 0xFFFFFFFF && r + 8 <= sz) { csize = rd64(d + r); r += 8; }
                if (off == 0xFFFFFFFF && r + 8 <= sz) { off = rd64(d + r); r += 8; }
            } else if (id == 0x000a && sz >= 32) {
                for (size_t r = 4; r + 4 <= sz;) {
                    uint16_t tag = rd16(d + r), tsz = rd16(d + r + 2);
                    if (tag == 1 && tsz >= 8 && r + 4 + 8 <= sz) { mtime = rd64(d + r + 4); break; }
                    r += 4 + (size_t)tsz;
                }
            } else if (id == 0x5455 && sz >= 5 && (d[0] & 1)) {
                uint64_t unix = rd32(d + 1);
                mtime = unix * 10000000ULL + 116444736000000000ULL;
            } else if (id == 0x7075 && sz >= 5 && d[0] == 1 && rd32(d + 1) == crc32Update(0, nm, nlen)) {
                utf8 = d + 5;
                utf8Len = sz - 5u;
            }
            q += 4 + (size_t)sz;
        }
        p += 46 + (size_t)nlen + xlen + clen;
        // Nome: UTF-8 se lo dice il bit 11 (o il campo Unicode), altrimenti la codepage OEM del sistema,
        // come fanno Esplora file e 7-Zip quando creano uno zip (437 negli Stati Uniti, 850 in Italia: per
        // le lettere accentate sono uguali)
        UINT cp = (flags & 0x800) || utf8 ? CP_UTF8 : CP_OEMCP;
        const char* src = utf8 ? (const char*)utf8 : (const char*)nm;
        int slen = utf8 ? (int)utf8Len : (int)nlen;
        bool ascii = true;
        for (int i = 0; i < slen && ascii; i++) ascii = (uint8_t)src[i] < 0x80;
        if (ascii) { // quasi tutti i nomi: senza conversione
            wname.resize((size_t)slen);
            for (int i = 0; i < slen; i++) wname[(size_t)i] = (wchar_t)(uint8_t)src[i];
        } else {
            int wl = slen ? MultiByteToWideChar(cp, 0, src, slen, nullptr, 0) : 0;
            wname.assign((size_t)std::max(0, wl), 0);
            if (wl > 0) MultiByteToWideChar(cp, 0, src, slen, &wname[0], wl);
        }
        for (wchar_t& c : wname) if (c == L'/') c = L'\\';
        bool dirEntry = !wname.empty() && wname.back() == L'\\';
        // Attributi: DOS e NTFS li scrivono nel byte basso, Unix nei 16 bit alti (il tipo di file)
        uint32_t attr = 0, host = made >> 8;
        if (host == 0 || host == 10 || host == 14) attr = ext & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                                                                  FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_DIRECTORY);
        else if (host == 3 && ((ext >> 16) & 0170000) == 0040000) attr = FILE_ATTRIBUTE_DIRECTORY;
        if (dirEntry) attr |= FILE_ATTRIBUTE_DIRECTORY;
        bool isDir = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
        // Componenti del percorso: le cartelle che mancano si creano
        uint32_t parent = 0;
        size_t s = 0;
        bool any = false;
        size_t body = wname.find_last_not_of(L'\\');
        size_t dirLen = body == std::wstring::npos ? std::wstring::npos : wname.find_last_of(L'\\', body);
        if (havePrev && dirLen != std::wstring::npos && dirLen == prevDir.size() && !wname.compare(0, dirLen, prevDir)) {
            parent = prevParent; // stessa cartella della voce prima
            s = dirLen + 1;
        }
        uint32_t lastParent = parent;
        while (s <= wname.size()) {
            size_t q = wname.find(L'\\', s);
            if (q == std::wstring::npos) q = wname.size();
            comp.assign(wname, s, q - s);
            s = q + 1;
            zipCleanName(comp);
            if (comp.empty() || comp == L"." || comp == L"..") continue;
            bool last = s > wname.size() || wname.find_first_not_of(L'\\', s) == std::wstring::npos;
            if (last) lastParent = parent;
            lc = comp;
            bool ascii = true;
            for (wchar_t& c : lc) {
                if (c >= 0x80) ascii = false;
                else if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32);
            }
            if (!ascii) CharLowerBuffW(&lc[0], (DWORD)lc.size());
            uint32_t idx = child(parent, !last || isDir);
            parent = idx;
            any = true;
            if (last) {
                ZipNode& n = z.nodes[idx];
                if (isDir) {
                    n.attr = attr | FILE_ATTRIBUTE_DIRECTORY;
                    n.mtime = mtime;
                    n.explicitTime = true;
                } else if (!(n.attr & FILE_ATTRIBUTE_DIRECTORY)) { // voce doppia: vale l'ultima, come per chi aggiorna lo zip
                    n.attr = attr;
                    n.size = usize;
                    n.csize = csize;
                    n.off = off;
                    n.mtime = mtime;
                    n.crc = crc;
                    n.method = method;
                    n.flags = flags;
                }
                break;
            }
        }
        if (any) got++;
        if (any && dirLen != std::wstring::npos) {
            prevDir.assign(wname, 0, dirLen);
            prevParent = lastParent;
            havePrev = true;
        } else {
            havePrev = false;
        }
    }
    z.entries = got;
    // Totali delle cartelle e orari di quelle senza voce: un figlio viene sempre dopo la sua cartella
    for (size_t i = z.nodes.size(); i-- > 1;) {
        ZipNode& n = z.nodes[i];
        ZipNode& par = z.nodes[n.parent];
        par.size += n.size;
        if (z.isDir(n)) par.dirs++; else par.files++;
        if (!par.explicitTime && n.mtime > par.mtime) par.mtime = n.mtime;
    }
    return 0;
}

// --- archivi in memoria ----------------------------------------------------------------
// Gli ultimi archivi aperti, con la loro directory centrale: navigare dentro uno zip non rilegge niente.
// Se l'archivio cambia (dimensione o data) si rilegge.
static constexpr size_t kZipKeep = 4;
static SRWLOCK g_zipLock = SRWLOCK_INIT;
static std::vector<std::shared_ptr<const ZipArchive>> g_zipCache; // il più recente in fondo
static volatile double g_zipLastMs = 0;   // F12: ultima lettura di una directory centrale
static volatile LONG g_zipLastEntries = 0;

// Su un thread: l'archivio letto (dalla memoria se non è cambiato), o nullptr con err
static std::shared_ptr<const ZipArchive> zipGet(const std::wstring& path, DWORD& err) {
    err = 0;
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(longPath(path).c_str(), GetFileExInfoStandard, &fa)) { err = GetLastError(); return nullptr; }
    uint64_t size = (uint64_t)fa.nFileSizeHigh << 32 | fa.nFileSizeLow;
    uint64_t time = (uint64_t)fa.ftLastWriteTime.dwHighDateTime << 32 | fa.ftLastWriteTime.dwLowDateTime;
    AcquireSRWLockExclusive(&g_zipLock);
    for (size_t i = 0; i < g_zipCache.size(); i++) {
        std::shared_ptr<const ZipArchive> z = g_zipCache[i];
        if (z->fileSize == size && z->fileTime == time && !_wcsicmp(z->path.c_str(), path.c_str())) {
            g_zipCache.erase(g_zipCache.begin() + (ptrdiff_t)i);
            g_zipCache.push_back(z);
            ReleaseSRWLockExclusive(&g_zipLock);
            return z;
        }
    }
    ReleaseSRWLockExclusive(&g_zipLock);
    double t0 = nowMs();
    HANDLE f = CreateFileW(longPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (f == INVALID_HANDLE_VALUE) { err = GetLastError(); return nullptr; }
    std::shared_ptr<ZipArchive> z = std::make_shared<ZipArchive>();
    z->path = path;
    z->fileSize = size;
    z->fileTime = time;
    err = zipParse(f, size, *z);
    CloseHandle(f);
    if (err) return nullptr;
    z->readMs = nowMs() - t0;
    g_zipLastMs = z->readMs;
    InterlockedExchange(&g_zipLastEntries, (LONG)z->entries);
    if (z->readMs > ui::LOG_ZIP) hitchLog("zip: elenco lento", z->entries, z->readMs);
    AcquireSRWLockExclusive(&g_zipLock);
    for (size_t i = 0; i < g_zipCache.size(); i++) // un'altra versione dello stesso archivio non serve più
        if (!_wcsicmp(g_zipCache[i]->path.c_str(), path.c_str())) { g_zipCache.erase(g_zipCache.begin() + (ptrdiff_t)i); break; }
    g_zipCache.push_back(z);
    if (g_zipCache.size() > kZipKeep) g_zipCache.erase(g_zipCache.begin());
    ReleaseSRWLockExclusive(&g_zipLock);
    return z;
}

// Nodo del percorso interno inner ("" = radice, "a\b"), UINT32_MAX se non c'è
static uint32_t zipFind(const ZipArchive& z, const wchar_t* inner) {
    uint32_t cur = 0;
    while (*inner == L'\\') inner++;
    while (*inner) {
        const wchar_t* e = wcschr(inner, L'\\');
        size_t n = e ? (size_t)(e - inner) : wcslen(inner);
        uint32_t found = UINT32_MAX;
        if (z.isDir(z.nodes[cur]))
            for (uint32_t c = z.nodes[cur].child; c != UINT32_MAX; c = z.nodes[c].next)
                if (z.nodes[c].nameLen == n && !_wcsnicmp(z.name(z.nodes[c]), inner, n)) { found = c; break; }
        if (found == UINT32_MAX) return UINT32_MAX;
        cur = found;
        inner += n;
        while (*inner == L'\\') inner++;
    }
    return cur;
}

// Archivio e nodo di un percorso (archivio o dentro di esso), su un thread
struct ZipRef { std::shared_ptr<const ZipArchive> z; uint32_t node = UINT32_MAX; DWORD err = 0; };
static ZipRef zipResolve(const std::wstring& path, size_t alen) {
    ZipRef r;
    r.z = zipGet(path.substr(0, alen), r.err);
    if (!r.z) return r;
    r.node = zipFind(*r.z, path.c_str() + alen);
    if (r.node == UINT32_MAX) r.err = ERROR_PATH_NOT_FOUND;
    return r;
}

// Voci di una cartella dello zip in un elenco (lista, anteprima). max: oltre si smette (more)
static void zipFill(Listing* l, const ZipArchive& z, uint32_t dir, size_t max, bool* more) {
    for (uint32_t c = z.nodes[dir].child; c != UINT32_MAX; c = z.nodes[c].next) {
        if (l->e.size() >= max) { if (more) *more = true; break; }
        const ZipNode& n = z.nodes[c];
        addEntry(l, z.name(n), n.nameLen, n.attr, n.size, n.mtime);
    }
}

// Lettura di una cartella dello zip per la lista (listThread)
static void zipList(Listing* l, size_t alen) {
    l->zipLen = (uint32_t)alen;
    ZipRef r = zipResolve(l->path, alen);
    if (!r.z) { l->err = r.err == ERROR_BAD_FORMAT || r.err == ERROR_NOT_SUPPORTED ? ERROR_BAD_FORMAT : r.err; return; }
    if (r.err) { l->err = r.err; return; }
    if (!r.z->isDir(r.z->nodes[r.node])) { l->err = ERROR_DIRECTORY; return; }
    zipFill(l, *r.z, r.node, SIZE_MAX, nullptr);
}

// Sottocartelle di una cartella dello zip, già nell'ordine della lista (menu "›", "Vai a")
static bool zipSubfolders(const std::wstring& path, size_t alen, std::vector<std::wstring>& names, std::vector<uint8_t>& hidden) {
    ZipRef r = zipResolve(path, alen);
    if (!r.z || r.err || !r.z->isDir(r.z->nodes[r.node])) return false;
    Listing l;
    for (uint32_t c = r.z->nodes[r.node].child; c != UINT32_MAX; c = r.z->nodes[c].next) {
        const ZipNode& n = r.z->nodes[c];
        if (r.z->isDir(n)) addEntry(&l, r.z->name(n), n.nameLen, n.attr, 0, 0);
    }
    buildKeys(&l);
    sortListing(&l, l.order, COL_NAME, false);
    for (uint32_t i : l.order) {
        names.emplace_back(l.name(l.e[i]), l.e[i].nameLen);
        hidden.push_back((l.e[i].attr & FILE_ATTRIBUTE_HIDDEN) != 0);
    }
    return true;
}

// --- CRC-32 e inflate ------------------------------------------------------------------
static uint32_t crc32Update(uint32_t crc, const uint8_t* p, size_t n) {
    static uint32_t tab[8][256];
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&once, [](PINIT_ONCE, PVOID, PVOID*) -> BOOL {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
            tab[0][i] = c;
        }
        for (uint32_t i = 0; i < 256; i++)
            for (int t = 1; t < 8; t++) tab[t][i] = (tab[t - 1][i] >> 8) ^ tab[0][tab[t - 1][i] & 255];
        return TRUE;
    }, nullptr, nullptr);
    crc = ~crc;
    while (n >= 8) { // otto byte per passo (slicing-by-8): il CRC non deve rallentare l'estrazione
        uint32_t a = rd32(p) ^ crc, b = rd32(p + 4);
        crc = tab[7][a & 255] ^ tab[6][(a >> 8) & 255] ^ tab[5][(a >> 16) & 255] ^ tab[4][a >> 24] ^
              tab[3][b & 255] ^ tab[2][(b >> 8) & 255] ^ tab[1][(b >> 16) & 255] ^ tab[0][b >> 24];
        p += 8;
        n -= 8;
    }
    while (n--) crc = (crc >> 8) ^ tab[0][(crc ^ *p++) & 255];
    return ~crc;
}

// Dove vanno i byte estratti: put ritorna false per fermarsi (anteprima che ne ha abbastanza);
// cancel dice se chi ha chiesto non li vuole più
struct ZipSink {
    bool (*put)(void* ctx, const uint8_t* p, size_t n) = nullptr;
    bool (*cancel)(void* ctx) = nullptr;
    void* ctx = nullptr;
};

static constexpr size_t kZipInBuf = 256 * 1024;   // letture dall'archivio
static constexpr size_t kZipWindow = 32768;        // distanza massima di deflate
static constexpr size_t kZipOutBuf = 1 << 20;      // uscita, con la finestra in testa

// Ingresso a bit (deflate li impacchetta dal meno significativo), letto a blocchi dal file
struct ZBits {
    HANDLE f = nullptr;
    uint64_t left = 0;                 // byte compressi ancora da leggere
    std::vector<uint8_t> buf;
    size_t pos = 0, end = 0;
    uint64_t bits = 0;
    int nb = 0;
    int over = 0;                      // byte a zero aggiunti oltre la fine dei dati
    bool ioErr = false;
    bool fill() {
        if (!left) return false;
        DWORD want = (DWORD)std::min<uint64_t>(left, buf.size()), got = 0;
        if (!ReadFile(f, buf.data(), want, &got, nullptr) || !got) { ioErr = true; left = 0; return false; }
        left -= got;
        pos = 0;
        end = got;
        return true;
    }
    void refill() {
        while (nb <= 56) {
            if (pos == end && !fill()) { over++; nb += 8; continue; } // zeri: un flusso troncato finisce in errore
            bits |= (uint64_t)buf[pos++] << nb;
            nb += 8;
        }
    }
    void drop(int n) { bits >>= n; nb -= n; }
    uint32_t take(int n) { uint32_t v = (uint32_t)(bits & ((1ull << n) - 1)); drop(n); return v; }
};

// Codici di Huffman canonici: tabella diretta per i codici fino a kZFast bit, il resto bit per bit
static constexpr int kZFast = 10;
struct ZHuff {
    uint16_t fast[1 << kZFast];        // simbolo << 4 | lunghezza (0: codice più lungo)
    uint16_t count[16];
    uint16_t sym[320];
};

static bool zBuild(ZHuff& h, const uint8_t* len, int n) {
    memset(h.count, 0, sizeof(h.count));
    for (int i = 0; i < n; i++) h.count[len[i]]++;
    h.count[0] = 0;
    int left = 1;
    for (int l = 1; l < 16; l++) {
        left <<= 1;
        left -= h.count[l];
        if (left < 0) return false; // codice impossibile
    }
    uint16_t offs[16] = {}, next[16] = {};
    for (int l = 1; l < 15; l++) offs[l + 1] = (uint16_t)(offs[l] + h.count[l]);
    for (int i = 0; i < n; i++) if (len[i]) h.sym[offs[len[i]]++] = (uint16_t)i;
    int code = 0;
    for (int l = 1; l < 16; l++) { code = (code + h.count[l - 1]) << 1; next[l] = (uint16_t)code; }
    memset(h.fast, 0, sizeof(h.fast));
    for (int i = 0; i < n; i++) {
        int l = len[i];
        if (!l) continue;
        int c = next[l]++;
        if (l > kZFast) continue;
        int rev = 0;
        for (int k = 0; k < l; k++) rev |= ((c >> k) & 1) << (l - 1 - k);
        for (int j = rev; j < (1 << kZFast); j += 1 << l) h.fast[j] = (uint16_t)(i << 4 | l);
    }
    return true;
}

// Prossimo simbolo (servono almeno 15 bit nel buffer); -1 se il codice non esiste
static inline int zDecode(ZBits& in, const ZHuff& h) {
    uint16_t e = h.fast[in.bits & ((1u << kZFast) - 1)];
    if (e & 15) { in.drop(e & 15); return e >> 4; }
    int code = 0, first = 0, index = 0;
    for (int l = 1; l < 16; l++) {
        code |= (int)((in.bits >> (l - 1)) & 1);
        int c = h.count[l];
        if (code - c < first) { in.drop(l); return h.sym[index + (code - first)]; }
        index += c;
        first += c;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

// Uscita: la finestra di 32 KB resta in testa al buffer per i riferimenti all'indietro
struct ZOut {
    std::vector<uint8_t> w;
    size_t pos = 0, flushed = 0;
    uint64_t total = 0;
    uint32_t crc = 0;
    ZipSink sink;
    bool stop = false;
    bool flush() {
        if (pos > flushed) {
            crc = crc32Update(crc, &w[flushed], pos - flushed);
            total += pos - flushed;
            if (!sink.put(sink.ctx, &w[flushed], pos - flushed)) stop = true;
            flushed = pos;
        }
        if (pos > kZipWindow) {
            memmove(w.data(), &w[pos - kZipWindow], kZipWindow);
            pos = flushed = kZipWindow;
        }
        return !stop;
    }
};

static int zInflate(ZBits& in, ZOut& out, const ZipSink& sink) {
    static const uint16_t lenBase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
    static const uint8_t lenExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
    static const uint16_t distBase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
                                           2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
    static const uint8_t distExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
    static const uint8_t clOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    std::unique_ptr<ZHuff[]> tabs(new ZHuff[3]); // letterali e lunghezze, distanze, lunghezze dei codici
    ZHuff& lit = tabs[0];
    ZHuff& dist = tabs[1];
    ZHuff& clh = tabs[2];
    const size_t cap = out.w.size();
    uint32_t blocks = 0;
    for (;;) {
        if ((++blocks & 15) == 0 && sink.cancel && sink.cancel(sink.ctx)) return ZE_CANCEL;
        in.refill();
        int final = (int)in.take(1), type = (int)in.take(2);
        if (type == 0) { // non compresso
            in.drop(in.nb & 7);
            in.refill();
            uint32_t len = in.take(16), nlen = in.take(16);
            if ((len ^ 0xFFFF) != nlen) return ZE_DATA;
            while (len) {
                if (out.pos == cap && !out.flush()) return ZE_STOP;
                size_t room = std::min<size_t>(len, cap - out.pos);
                size_t k = 0;
                while (k < room && in.nb >= 8) { out.w[out.pos + k++] = (uint8_t)in.take(8); } // prima i byte già nel buffer a bit
                while (k < room) {
                    if (in.pos == in.end && !in.fill()) return in.ioErr ? ZE_IO : ZE_DATA;
                    size_t c = std::min(room - k, in.end - in.pos);
                    memcpy(&out.w[out.pos + k], &in.buf[in.pos], c);
                    in.pos += c;
                    k += c;
                }
                out.pos += k;
                len -= (uint32_t)k;
            }
        } else if (type == 1 || type == 2) {
            if (type == 1) { // codici fissi
                uint8_t l[288 + 30];
                for (int i = 0; i < 288; i++) l[i] = (uint8_t)(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
                for (int i = 0; i < 30; i++) l[288 + i] = 5;
                zBuild(lit, l, 288);
                zBuild(dist, l + 288, 30);
            } else { // codici nel blocco
                in.refill();
                int nlit = (int)in.take(5) + 257, ndist = (int)in.take(5) + 1, ncl = (int)in.take(4) + 4;
                if (nlit > 286 || ndist > 30) return ZE_DATA;
                uint8_t cl[19] = {};
                for (int i = 0; i < ncl; i++) { in.refill(); cl[clOrder[i]] = (uint8_t)in.take(3); }
                if (!zBuild(clh, cl, 19)) return ZE_DATA;
                uint8_t l[286 + 30];
                for (int i = 0; i < nlit + ndist;) {
                    in.refill();
                    int s = zDecode(in, clh);
                    if (s < 0) return ZE_DATA;
                    if (s < 16) { l[i++] = (uint8_t)s; continue; }
                    int rep, val = 0;
                    if (s == 16) { if (!i) return ZE_DATA; val = l[i - 1]; rep = 3 + (int)in.take(2); }
                    else if (s == 17) rep = 3 + (int)in.take(3);
                    else rep = 11 + (int)in.take(7);
                    if (i + rep > nlit + ndist) return ZE_DATA;
                    while (rep--) l[i++] = (uint8_t)val;
                }
                if (!l[256]) return ZE_DATA; // senza il codice di fine blocco
                if (!zBuild(lit, l, nlit) || !zBuild(dist, l + nlit, ndist)) return ZE_DATA;
            }
            for (;;) {
                in.refill();
                if (in.over > 8) return in.ioErr ? ZE_IO : ZE_DATA; // oltre la fine dei dati
                int s = zDecode(in, lit);
                if (s < 0) return ZE_DATA;
                if (s < 256) {
                    if (out.pos == cap && !out.flush()) return ZE_STOP;
                    out.w[out.pos++] = (uint8_t)s;
                    continue;
                }
                if (s == 256) break;
                s -= 257;
                if (s >= 29) return ZE_DATA;
                size_t len = lenBase[s] + in.take(lenExtra[s]);
                int ds = zDecode(in, dist);
                if (ds < 0 || ds >= 30) return ZE_DATA;
                size_t d = distBase[ds] + in.take(distExtra[ds]);
                if (out.pos + 258 > cap && !out.flush()) return ZE_STOP;
                if (d > out.pos) return ZE_DATA; // prima dell'inizio dei dati
                uint8_t* dst = &out.w[out.pos];
                const uint8_t* srcp = dst - d;
                if (d >= len) memcpy(dst, srcp, len);
                else for (size_t k = 0; k < len; k++) dst[k] = srcp[k]; // sovrapposti: si ripete
                out.pos += len;
            }
        } else {
            return ZE_DATA;
        }
        if (final) break;
    }
    if (in.over * 8 > in.nb) return in.ioErr ? ZE_IO : ZE_DATA; // ha usato byte che non c'erano
    return out.flush() ? ZE_OK : ZE_STOP;
}

// Estrae il file node dell'archivio aperto in f verso sink, con il controllo del CRC
static int zipExtract(HANDLE f, const ZipNode& n, const ZipSink& sink) {
    if (n.flags & 1) return ZE_CRYPT;
    if (n.method != 0 && n.method != 8) return ZE_METHOD;
    uint8_t lh[30];
    if (!zipReadAt(f, n.off, lh, 30)) return ZE_IO;
    if (rd32(lh) != 0x04034b50) return ZE_DATA;
    uint64_t data = n.off + 30 + rd16(lh + 26) + rd16(lh + 28);
    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)data;
    if (!SetFilePointerEx(f, li, nullptr, FILE_BEGIN)) return ZE_IO;
    ZBits in;
    in.f = f;
    in.left = n.csize;
    in.buf.resize((size_t)std::min<uint64_t>(std::max<uint64_t>(n.csize, 1), kZipInBuf));
    if (n.method == 0) { // non compresso: i byte passano così come sono
        uint32_t crc = 0;
        uint64_t total = 0;
        while (in.left) {
            if (sink.cancel && sink.cancel(sink.ctx)) return ZE_CANCEL;
            if (!in.fill()) return ZE_IO;
            crc = crc32Update(crc, in.buf.data(), in.end);
            total += in.end;
            if (!sink.put(sink.ctx, in.buf.data(), in.end)) return ZE_STOP;
        }
        return total == n.size && crc == n.crc ? ZE_OK : ZE_DATA;
    }
    ZOut out;
    out.sink = sink;
    out.w.resize(kZipOutBuf + kZipWindow);
    int r = zInflate(in, out, sink);
    if (r == ZE_OK && (out.total != n.size || out.crc != n.crc)) r = ZE_DATA;
    return r;
}

// Fino a max byte del file all'inizio di path (dentro un archivio), in memoria: anteprima e
// miniature. full: il file intero entrava. Ritorna ZE_OK anche se si è fermato al limite.
struct ZipMem { std::string* s; size_t max; bool (*cancel)(void*); void* ctx; };
static int zipReadMem(const std::wstring& path, size_t alen, size_t max, std::string& out, uint64_t* size,
                      bool (*cancel)(void*), void* ctx) {
    ZipRef r = zipResolve(path, alen);
    if (!r.z || r.err) return ZE_IO;
    const ZipNode& n = r.z->nodes[r.node];
    if (r.z->isDir(n)) return ZE_IO;
    if (size) *size = n.size;
    HANDLE f = CreateFileW(longPath(r.z->path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return ZE_IO;
    out.clear();
    out.reserve((size_t)std::min<uint64_t>(n.size, max));
    ZipMem m{ &out, max, cancel, ctx };
    ZipSink sink;
    sink.ctx = &m;
    sink.put = [](void* c, const uint8_t* p, size_t k) {
        ZipMem* m = (ZipMem*)c;
        size_t room = m->max - m->s->size();
        m->s->append((const char*)p, std::min(room, k));
        return k < room;
    };
    sink.cancel = [](void* c) { ZipMem* m = (ZipMem*)c; return m->cancel && m->cancel(m->ctx); };
    int e = zipExtract(f, n, sink);
    CloseHandle(f);
    return e == ZE_STOP ? ZE_OK : e;
}

// --- estrazione in una cartella temporanea ---------------------------------------------
// %TEMP%\z-f\<n>: una cartella per ogni estrazione (aprire, copiare, trascinare), così una copia
// ancora da incollare non si mescola con quella dopo. Le cartelle più vecchie di ZIP_TEMP_KEEP si
// tolgono all'estrazione successiva.
static std::wstring zipTempRoot() {
    wchar_t t[MAX_PATH + 1];
    DWORD n = GetTempPathW(MAX_PATH + 1, t);
    if (!n || n > MAX_PATH) return L"";
    return std::wstring(t, n) + L"z-f";
}

// Cancella una cartella e il suo contenuto. Solo per le nostre cartelle temporanee: non segue i
// collegamenti (una giunzione si toglie, non si attraversa).
static void zipRemoveTree(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((longPath(dir) + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            uint32_t len = (uint32_t)wcslen(fd.cFileName);
            if (isDots(fd.cFileName, len)) continue;
            std::wstring p = dir + L'\\' + fd.cFileName;
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                zipRemoveTree(p);
            } else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                RemoveDirectoryW(longPath(p).c_str());
            } else {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) SetFileAttributesW(longPath(p).c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(longPath(p).c_str()); // un file ancora aperto resta: la prossima volta
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(longPath(dir).c_str());
}

static void zipTempClean(const std::wstring& root) {
    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    uint64_t now = (uint64_t)nowFt.dwHighDateTime << 32 | nowFt.dwLowDateTime;
    uint64_t keep = (uint64_t)ui::ZIP_TEMP_KEEP * 10000ULL;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((root + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<std::wstring> old;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || isDots(fd.cFileName, (uint32_t)wcslen(fd.cFileName))) continue;
        uint64_t c = (uint64_t)fd.ftCreationTime.dwHighDateTime << 32 | fd.ftCreationTime.dwLowDateTime;
        if (c + keep < now) old.push_back(root + L'\\' + fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (const std::wstring& d : old) zipRemoveTree(d);
}

// Cartella nuova e vuota per un'estrazione, "" se non si può creare
static std::wstring zipTempDir() {
    std::wstring root = zipTempRoot();
    if (root.empty()) return L"";
    CreateDirectoryW(root.c_str(), nullptr);
    static volatile LONG counter = 0;
    for (int tries = 0; tries < 100; tries++) {
        wchar_t b[40];
        _snwprintf(b, 39, L"\\%llx-%lx", (unsigned long long)(GetTickCount64() & 0xFFFFFFFFFFull), (unsigned long)InterlockedIncrement(&counter));
        b[39] = 0;
        std::wstring d = root + b;
        if (CreateDirectoryW(d.c_str(), nullptr)) return d;
        if (GetLastError() != ERROR_ALREADY_EXISTS) return L"";
    }
    return L"";
}

// Stato di un'estrazione su file (thread): errori e misure
struct ZipOutState {
    HANDLE out = nullptr;
    bool (*cancel)(void*) = nullptr;
    void* ctx = nullptr;
    uint64_t bytes = 0;
    int failed = 0, firstErr = ZE_OK;
    std::wstring firstName;
};

static bool zipPutFile(void* c, const uint8_t* p, size_t n) {
    ZipOutState* s = (ZipOutState*)c;
    while (n) {
        DWORD w = 0, k = (DWORD)std::min<size_t>(n, 1 << 24);
        if (!WriteFile(s->out, p, k, &w, nullptr) || !w) return false;
        p += w;
        n -= w;
    }
    return true;
}

static void zipFail(ZipOutState& s, int err, const ZipArchive& z, const ZipNode& n) {
    s.failed++;
    if (s.firstErr == ZE_OK) { s.firstErr = err; s.firstName.assign(z.name(n), n.nameLen); }
}

// Estrae il nodo (file o cartella con tutto il contenuto) dentro dir; ritorna il percorso creato
static std::wstring zipExtractNode(HANDLE f, const ZipArchive& z, uint32_t idx, const std::wstring& dir, ZipOutState& s) {
    const ZipNode& n = z.nodes[idx];
    std::wstring dst = dir + L'\\' + std::wstring(z.name(n), n.nameLen);
    if (s.cancel && s.cancel(s.ctx)) return L"";
    if (z.isDir(n)) {
        if (!CreateDirectoryW(longPath(dst).c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) { zipFail(s, ZE_IO, z, n); return L""; }
        for (uint32_t c = n.child; c != UINT32_MAX; c = z.nodes[c].next) zipExtractNode(f, z, c, dst, s);
        return dst;
    }
    HANDLE o = CreateFileW(longPath(dst).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (o == INVALID_HANDLE_VALUE) { zipFail(s, ZE_IO, z, n); return L""; }
    s.out = o;
    ZipSink sink;
    sink.put = zipPutFile;
    sink.ctx = &s;
    if (s.cancel) sink.cancel = [](void* c) { ZipOutState* st = (ZipOutState*)c; return st->cancel(st->ctx); };
    int e = zipExtract(f, n, sink);
    if (e == ZE_OK) {
        FILETIME ft = { (DWORD)n.mtime, (DWORD)(n.mtime >> 32) };
        if (n.mtime) SetFileTime(o, nullptr, nullptr, &ft);
        s.bytes += n.size;
    }
    CloseHandle(o);
    if (e != ZE_OK) {
        DeleteFileW(longPath(dst).c_str()); // niente file a metà
        if (e != ZE_CANCEL) zipFail(s, e, z, n);
        return L"";
    }
    if (n.attr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN))
        SetFileAttributesW(longPath(dst).c_str(), n.attr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN));
    return dst;
}

// --- estrazioni chieste dall'interfaccia (aprire, copiare, trascinare) ------------------
enum { ZJ_OPEN, ZJ_COPY, ZJ_DRAG, ZJ_KINDS };
struct ZipJob {
    HWND hwnd = nullptr;
    int kind = ZJ_OPEN;
    uint64_t gen = 0;
    std::wstring archive;               // l'archivio sul disco
    std::vector<std::wstring> items;    // percorsi completi delle voci, dentro l'archivio
    uint32_t firstAttr = 0;             // attributi della prima (immagine trascinata)
    // risultato
    std::vector<std::wstring> out;      // percorsi estratti (le voci riuscite, nello stesso ordine)
    DWORD openErr = 0;                  // l'archivio non si legge
    int failed = 0, err = ZE_OK;        // voci non estratte, primo motivo e nome
    std::wstring errName;
    bool cancelled = false;
    uint64_t bytes = 0;
    double ms = 0;
};
// Per tipo, l'ultima richiesta: una copia o un trascinamento superati smettono di estrarre
static volatile LONG64 g_zipLatest[ZJ_KINDS] = {};

static DWORD WINAPI zipJobThread(void* p) {
    ZipJob* j = (ZipJob*)p;
    double t0 = nowMs();
    std::wstring root = zipTempRoot();
    if (!root.empty()) zipTempClean(root);
    std::shared_ptr<const ZipArchive> z = zipGet(j->archive, j->openErr);
    std::wstring dir = z ? zipTempDir() : L"";
    if (z && dir.empty()) j->openErr = ERROR_CANNOT_MAKE;
    HANDLE f = INVALID_HANDLE_VALUE;
    if (z && !dir.empty()) {
        f = CreateFileW(longPath(j->archive).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (f == INVALID_HANDLE_VALUE) j->openErr = GetLastError();
    }
    if (f != INVALID_HANDLE_VALUE) {
        ZipOutState s;
        if (j->kind != ZJ_OPEN) {
            s.ctx = j;
            s.cancel = [](void* c) {
                ZipJob* jj = (ZipJob*)c;
                return (uint64_t)InterlockedCompareExchange64(&g_zipLatest[jj->kind], 0, 0) != jj->gen;
            };
        }
        for (const std::wstring& it : j->items) {
            uint32_t n = zipFind(*z, it.c_str() + std::min(it.size(), j->archive.size()));
            if (n == UINT32_MAX || !n) { s.failed++; if (s.firstErr == ZE_OK) { s.firstErr = ZE_IO; s.firstName = it.substr(it.find_last_of(L'\\') + 1); } continue; }
            std::wstring o = zipExtractNode(f, *z, n, dir, s);
            if (!o.empty()) j->out.push_back(o);
            if (s.cancel && s.cancel(s.ctx)) { j->cancelled = true; break; }
        }
        CloseHandle(f);
        j->failed = s.failed;
        j->err = s.firstErr;
        j->errName = s.firstName;
        j->bytes = s.bytes;
    }
    j->ms = nowMs() - t0;
    if (j->ms > ui::LOG_ZIP) hitchLog("zip: estrazione", (unsigned)(j->bytes >> 20), j->ms); // codice: MB estratti
    if (!PostMessageW(j->hwnd, WM_APP_ZIP, 0, (LPARAM)j)) delete j;
    return 0;
}
