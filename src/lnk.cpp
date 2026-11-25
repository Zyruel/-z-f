// [z]f — lettore dei collegamenti di Windows (.lnk, formato [MS-SHLLINK]), senza la shell.
//
// Il file (di solito meno di 4 KB) si legge in un colpo e il percorso della destinazione viene,
// nell'ordine, dalla variabile d'ambiente (%ProgramFiles%\…), dalla LinkInfo (unità locale, unità
// di rete col suo nome "V:", oppure \server\condivisione) o dal percorso relativo. La destinazione
// non si tocca mai: niente rete, niente risoluzione. IShellLink farebbe lo stesso caricando COM e
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

// base + "\" + suffix (il suffisso della LinkInfo è il resto del percorso dopo l'unità o la condivisione)
static std::wstring lkJoin(std::wstring base, const std::wstring& suffix) {
    if (suffix.empty() || base.empty()) return base + suffix;
    if (base.back() != L'\\') base += L'\\';
    return base + suffix;
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
    if (flags & 0x1) { // HasLinkTargetIDList: si salta (la LinkInfo dice lo stesso in forma di percorso)
        if (pos + 2 > n) return true;
        pos += 2 + (size_t)lkU16(b + pos);
    }
    std::wstring local, device, net, suffix;
    if ((flags & 0x2) && !(flags & 0x100) && pos + 0x1C <= n) { // HasLinkInfo, senza ForceNoLinkInfo
        const uint8_t* li = b + pos;
        uint32_t size = lkU32(li), hdr = lkU32(li + 4), lf = lkU32(li + 8);
        if (size < 0x1C || pos + size > n) return true;
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
        if (hdr >= 0x24 && (p = at(lkU32(li + 0x20)))) suffix = lkWide(p, le);
        else if ((p = at(lkU32(li + 0x18)))) suffix = lkAnsi(p, le);
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

