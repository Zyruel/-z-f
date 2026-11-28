// [z]f — lettore dei collegamenti di Windows (.lnk, formato [MS-SHLLINK]), senza la shell.
//
// Il file (di solito meno di 4 KB) si legge in un colpo e il percorso della destinazione viene,
// nell'ordine, dalla variabile d'ambiente (%ProgramFiles%\…), dalla LinkInfo (unità locale, unità
// di rete col suo nome "V:", oppure \server\condivisione), dalla IDList (se la LinkInfo manca) o dal
// percorso relativo. La destinazione non si tocca mai: niente rete, niente risoluzione.
// IShellLink farebbe lo stesso caricando COM e
// la shell; il confronto con lui è in prove\collegamenti-prova.cpp. I collegamenti senza percorso
// (app dello Store, Pannello di controllo) non hanno destinazione: target vuoto.
// Lo usano la colonna Destinazione (links.cpp) e tooltip e anteprima (readInfo), sui loro thread.

static constexpr size_t kLinkMaxBytes = 64 * 1024; // un .lnk normale sta in pochi KB; oltre non si legge il resto

static uint32_t lkU32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint16_t lkU16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }

// Testo terminato da zero in [p, end): nella codepage di sistema (come lo scrive Windows) o UTF-16
static std::wstring lkAnsi(const uint8_t* p, const uint8_t* end, bool zeroEnded = true) {
    const uint8_t* q = p;
    if (zeroEnded) while (q < end && *q) q++;
    else q = end;
    if (q <= p) return L"";
    int n = MultiByteToWideChar(CP_ACP, 0, (const char*)p, (int)(q - p), nullptr, 0);
    std::wstring s((size_t)std::max(n, 0), 0);
    if (n > 0) MultiByteToWideChar(CP_ACP, 0, (const char*)p, (int)(q - p), &s[0], n);
    return s;
}
static std::wstring lkWide(const uint8_t* p, const uint8_t* end) {
    std::wstring s;
    for (; p + 1 < end; p += 2) {
        wchar_t c = (wchar_t)lkU16(p);
        if (!c) break;
        s += c;
    }
    return s;
}

// base + "\" + suffix (il suffisso della LinkInfo è il resto del percorso dopo l'unità o la condivisione).
// Senza base il suffisso da solo non è un percorso: vuoto, così si prova la base successiva (su un'unità
// di rete il percorso locale manca e la base è "V:")
static std::wstring lkJoin(std::wstring base, const std::wstring& suffix) {
    if (base.empty()) return L"";
    if (suffix.empty()) return base;
    if (base.back() != L'\\') base += L'\\';
    return base + suffix;
}

// Nome di una cartella o di un file nella IDList: quello lungo, dal blocco 0xBEEF0004 (il suo offset
// dall'inizio della voce è negli ultimi 2 byte della voce), altrimenti quello primario dopo i 14 byte
// fissi (8.3 nella codepage di sistema, o UTF-16 con il bit 0x04 del tipo)
static std::wstring lkIdName(const uint8_t* p, const uint8_t* ie, uint8_t type) {
    size_t sz = (size_t)(ie - p), ext = lkU16(ie - 2);
    if (ext >= 14 && ext + 8 <= sz) {
        const uint8_t* x = p + ext;
        size_t xs = lkU16(x), ver = lkU16(x + 2);
        if (lkU32(x + 4) == 0xBEEF0004 && xs <= (size_t)(ie - x)) {
            // dove comincia il nome lungo dipende dalla versione del blocco (Windows XP 3, Vista 7, 7 8, 8+ 9)
            size_t off = ver >= 9 ? 46 : ver >= 8 ? 42 : ver >= 7 ? 38 : ver >= 3 ? 20 : 0;
            if (off && off + 2 <= xs) {
                std::wstring s = lkWide(x + off, x + xs);
                if (!s.empty()) return s;
            }
        }
    }
    return (type & 0x04) ? lkWide(p + 14, ie) : lkAnsi(p + 14, ie);
}

// Percorso dalla IDList (l'elenco di elementi della shell), per i collegamenti senza LinkInfo: Windows
// scrive solo la IDList quando, alla creazione, la destinazione non è raggiungibile (un'unità di rete
// non collegata). Si capiscono solo gli elementi di un percorso: Computer o Rete come radice, l'unità
// ("C:\"), il computer e la condivisione di rete ("\\server\condivisione"), poi una voce per cartella.
// Qualunque altro elemento (cartelle speciali, app, Pannello di controllo): nessun percorso.
static std::wstring lkIdList(const uint8_t* p, const uint8_t* end) {
    static const uint8_t kComputer[16] = { 0xE0, 0x4F, 0xD0, 0x20, 0xEA, 0x3A, 0x69, 0x10,
                                           0xA2, 0xD8, 0x08, 0x00, 0x2B, 0x30, 0x30, 0x9D };
    static const uint8_t kNetwork[16] = { 0x60, 0x2C, 0x8D, 0x20, 0xEA, 0x3A, 0x69, 0x10,
                                          0xA2, 0xD7, 0x08, 0x00, 0x2B, 0x30, 0x30, 0x9D };
    std::wstring path;
    while (p + 2 <= end) {
        size_t sz = lkU16(p);
        if (!sz) break; // fine dell'elenco
        if (sz < 4 || sz > (size_t)(end - p)) return L"";
        const uint8_t* ie = p + sz;
        uint8_t type = p[2];
        if (type == 0x1F) { // radice
            if (sz < 20 || (memcmp(p + 4, kComputer, 16) && memcmp(p + 4, kNetwork, 16))) return L"";
        } else if ((type & 0x70) == 0x20) { // unità, in testo: "C:\"
            if (sz < 6 || p[4] != ':' || !iswalpha(p[3])) return L"";
            path = lkAnsi(p + 3, ie);
        } else if ((type & 0x70) == 0x40) { // rete: "\\server", poi "\\server\condivisione"
            // "Rete di Microsoft Windows" e il dominio, prima del computer, non sono percorsi: si saltano.
            // Il byte 4 sono i flag (descrizione e commento dopo il nome), il nome comincia al byte 5
            std::wstring s = sz > 5 ? lkAnsi(p + 5, ie) : L"";
            if (s.size() > 2 && s[0] == L'\\' && s[1] == L'\\') path = s;
        } else if ((type & 0x70) == 0x30) { // cartella o file
            if (path.empty() || sz < 16) return L"";
            std::wstring name = lkIdName(p, ie, type);
            if (name.empty()) return L"";
            path = lkJoin(path, name);
        } else {
            return L"";
        }
        p = ie;
    }
    return path;
}

// Destinazione dai byte del collegamento. false: non è un collegamento
static bool linkParse(const uint8_t* b, size_t n, const std::wstring& lnkPath, std::wstring& target, bool& dir) {
    static const uint8_t kClsid[16] = { 0x01, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 };
    target.clear();
    dir = false;
    if (n < 0x4C || lkU32(b) != 0x4C || memcmp(b + 4, kClsid, 16)) return false;
    uint32_t flags = lkU32(b + 0x14);
    dir = (lkU32(b + 0x18) & FILE_ATTRIBUTE_DIRECTORY) != 0; // attributi della destinazione quando è stato creato
    size_t pos = 0x4C;
    const uint8_t *ids = nullptr, *idsEnd = nullptr;
    if (flags & 0x1) { // HasLinkTargetIDList: si legge solo se manca la LinkInfo, che dice lo stesso come percorso
        if (pos + 2 > n) return true;
        size_t len = lkU16(b + pos);
        ids = b + pos + 2;
        idsEnd = b + std::min(n, pos + 2 + len);
        pos += 2 + len;
    }
    std::wstring local, device, net, suffix;
    if ((flags & 0x2) && pos + 0x1C <= n) { // HasLinkInfo
        const uint8_t* li = b + pos;
        uint32_t size = lkU32(li), hdr = lkU32(li + 4), lf = lkU32(li + 8);
        if (size < 0x1C || pos + size > n) return true;
        // Con ForceNoLinkInfo la LinkInfo c'è ma non vale: non se ne legge niente, si salta per
        // arrivare ai testi che seguono
        if (flags & 0x100) lf = 0;
        const uint8_t* le = li + size;
        auto at = [&](uint32_t off) -> const uint8_t* { return off && off < size ? li + off : nullptr; };
        const uint8_t* p;
        if (lf & 1) { // VolumeIDAndLocalBasePath
            if (hdr >= 0x24 && (p = at(lkU32(li + 0x1C)))) local = lkWide(p, le);
            else if ((p = at(lkU32(li + 0x10)))) local = lkAnsi(p, le);
        }
        if ((lf & 2) && (p = at(lkU32(li + 0x14))) && p + 0x14 <= le) { // CommonNetworkRelativeLink
            uint32_t cs = lkU32(p), cf = lkU32(p + 4), netOff = lkU32(p + 8), devOff = lkU32(p + 12);
            const uint8_t* ce = cs <= (uint32_t)(le - p) ? p + cs : le;
            bool uni = netOff > 0x14 && p + 0x1C <= ce; // ci sono anche gli offset dei testi UTF-16
            auto in = [&](uint32_t off) -> const uint8_t* { return off && p + off < ce ? p + off : nullptr; };
            const uint8_t* q;
            if (uni && (q = in(lkU32(p + 0x14)))) net = lkWide(q, ce);
            else if ((q = in(netOff))) net = lkAnsi(q, ce);
            if (cf & 1) { // ValidDevice: l'unità di rete con cui è stato creato ("V:"), come lo mostra Explorer
                if (uni && (q = in(lkU32(p + 0x18)))) device = lkWide(q, ce);
                else if ((q = in(devOff))) device = lkAnsi(q, ce);
            }
        }
        if (lf & 3) { // senza base il suffisso non serve
            if (hdr >= 0x24 && (p = at(lkU32(li + 0x20)))) suffix = lkWide(p, le);
            else if ((p = at(lkU32(li + 0x18)))) suffix = lkAnsi(p, le);
        }
        pos += size;
    }
    // StringData: nome, percorso relativo, cartella di lavoro, argomenti, icona (in quest'ordine)
    bool unicode = (flags & 0x80) != 0;
    std::wstring rel;
    for (uint32_t bit = 0x4; bit <= 0x40; bit <<= 1) {
        if (!(flags & bit)) continue;
        if (pos + 2 > n) { pos = n; break; }
        size_t cnt = lkU16(b + pos), bytes = cnt * (unicode ? 2 : 1);
        pos += 2;
        if (pos + bytes > n) { pos = n; break; }
        if (bit == 0x8) {
            if (unicode) { rel.resize(cnt); if (cnt) memcpy(&rel[0], b + pos, bytes); }
            else rel = lkAnsi(b + pos, b + pos + bytes, false);
        }
        pos += bytes;
    }
    // ExtraData: blocchi (dimensione, firma); interessa solo EnvironmentVariableDataBlock
    std::wstring env;
    while (pos + 8 <= n) {
        uint32_t bs = lkU32(b + pos), sig = lkU32(b + pos + 4);
        if (bs < 8 || bs > n - pos) break; // il blocco finale ha dimensione < 4
        if (sig == 0xA0000001 && bs >= 0x314) { // TargetAnsi[260], poi TargetUnicode[260]
            const uint8_t* d = b + pos + 8;
            env = lkWide(d + 260, d + 260 + 520);
            if (env.empty()) env = lkAnsi(d, d + 260);
        }
        pos += bs;
    }
    if ((flags & 0x200) && !env.empty()) { // HasExpString: %ProgramFiles%\… vale più del resto
        wchar_t buf[2048];
        DWORD k = ExpandEnvironmentStringsW(env.c_str(), buf, 2048);
        if (k && k <= 2048) target = buf;
    }
    if (target.empty()) target = lkJoin(local, suffix);
    if (target.empty()) target = lkJoin(device, suffix);
    if (target.empty()) target = lkJoin(net, suffix);
    if (target.empty() && ids) target = lkIdList(ids, idsEnd);
    if (target.empty() && !rel.empty()) { // relativo alla cartella del collegamento (solo testo, niente disco)
        std::wstring base = lnkPath.substr(0, lnkPath.find_last_of(L'\\') + 1) + rel;
        wchar_t full[2048];
        DWORD k = GetFullPathNameW(base.c_str(), 2048, full, nullptr);
        target = k && k < 2048 ? full : base;
    }
    // Un computer della rete (\\server, senza condivisione) non è un percorso da aprire: per Windows
    // non ha destinazione, e la sua cartella superiore non esiste
    if (target.size() > 2 && target[0] == L'\\' && target[1] == L'\\' && target.find(L'\\', 2) == std::wstring::npos)
        target.clear();
    return true;
}

// Legge il collegamento dal disco: solo su un thread (colonna Destinazione, tooltip, anteprima)
static bool linkRead(const std::wstring& path, std::wstring& target, bool& dir) {
    target.clear();
    dir = false;
    HANDLE f = CreateFileW(longPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    std::vector<uint8_t> b(kLinkMaxBytes);
    DWORD n = 0;
    if (!ReadFile(f, b.data(), (DWORD)b.size(), &n, nullptr)) n = 0;
    CloseHandle(f);
    return linkParse(b.data(), n, path, target, dir);
}

// Percorsi di rete di Windows ("Aggiungi un percorso di rete", in Explorer sotto Questo PC): ognuno
// è una cartella in %APPDATA%\Microsoft\Windows\Network Shortcuts con dentro un target.lnk verso
// \\server\condivisione. Si leggono con il lettore qui sopra: niente shell e niente rete, decimi di
// millisecondo (si fa anche nella lettura veloce dell'avvio). Solo cartelle: i siti web e FTP che
// la stessa procedura crea [z]f non li apre. Ordine per nome, come in Explorer.
static std::vector<NetPlace> netPlacesRead() {
    std::vector<NetPlace> out;
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    if (!n || n >= MAX_PATH) return out;
    std::wstring dir = std::wstring(buf) + L"\\Microsoft\\Windows\\Network Shortcuts\\";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((dir + L"*").c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        std::wstring target;
        bool isDir = false;
        if (!linkRead(dir + fd.cFileName + L"\\target.lnk", target, isDir)) continue;
        bool unc = target.size() > 2 && target[0] == L'\\' && target[1] == L'\\';
        bool local = target.size() >= 3 && target[1] == L':' && target[2] == L'\\';
        if (unc || local) out.push_back({ fd.cFileName, target });
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end(), [](const NetPlace& a, const NetPlace& b) { return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });
    return out;
}

