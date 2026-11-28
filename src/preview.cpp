// [z]f — pannello di anteprima (Alt+P · pulsante ◨ a destra nella riga del percorso).
//
// A destra della lista mostra la voce sotto il cursore: immagini (decodificate con WIC), testo (con
// la codifica riconosciuta), il contenuto delle cartelle; per tutto il resto l'icona grande della
// shell e le stesse informazioni del tooltip.
//
// Il thread dell'interfaccia non tocca il disco: ogni richiesta va su un thread, come per tooltip
// e "›", e il risultato torna con PostMessage e un contatore di generazione. Un risultato superato
// (il cursore si è già spostato) si scarta, e il thread che se ne accorge smette prima. Tenendo
// premuta una freccia non si accoda niente: un thread alla volta, e delle richieste in attesa vale
// solo l'ultima. Se una lettura è lenta (rete), dopo PREVIEW_SLOW la successiva parte comunque su
// un altro thread. Finché il contenuto nuovo non c'è resta il precedente; se tarda più di
// LOADING_AFTER si vedono il nome e "caricamento…".
//
// WIC (windowscodecs.dll) si carica solo alla prima immagine, sul thread. Le immagini arrivano già
// ridotte alla misura del pannello: la texture non è mai più grande del pannello e si disegna 1:1.
// Limiti espliciti: 256 KB di testo, 5000 voci o 300 ms per una cartella, 50 megapixel per
// un'immagine da decodificare intera (i JPEG si riducono già in decodifica, quindi non contano).

enum { PV_NONE, PV_TEXT, PV_IMAGE, PV_DIR, PV_INFO };

static const DWORD kPvTextMax = 256 * 1024;     // byte di testo letti
static const DWORD kPvSniff = 8192;             // byte per capire se un file sconosciuto è testo
static const int kPvLineMax = 2000;             // caratteri per riga: oltre si va a capo comunque (file minimizzati)
static const int kPvTab = 4;                    // tabulazione
static const int kPvDirMax = 5000;              // voci lette di una cartella
static const double kPvDirMs = 300;             // tempo massimo di lettura di una cartella
static const uint64_t kPvImgMaxPx = 50000000;   // pixel oltre cui un'immagine non si decodifica intera
static const int kPvThreads = 3;                // thread insieme, al massimo (letture lente)
static const int kPvIconSrc = 256;              // icone grandi della shell (SHIL_JUMBO)

// GUID scritti qui: le librerie che li definiscono cambiano tra MSVC e MinGW
static const GUID kClsidWic = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const GUID kWicBGR24 = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0c } };
static const GUID kWicBGR32 = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0e } };
static const GUID kWicBGRA = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f } };
static const GUID kWicPBGRA = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10 } };
static const GUID kIidImageList = { 0x46eb5926, 0x582e, 0x4017, { 0x9f, 0xdf, 0xe8, 0x99, 0x8d, 0xaa, 0x09, 0x50 } };

struct PvReq {
    HWND hwnd;
    uint64_t gen;
    std::wstring path;   // percorso completo ("C:\" per un'unità)
    uint32_t attr;
    bool drive;          // voce di Questo PC: niente letture, solo le informazioni
    int icon;            // da dove viene l'icona grande (iconSource)
    int boxW, boxH;      // spazio per un'immagine (pixel)
    int iconPx;          // lato dell'icona grande
    bool (*superseded)(uint64_t gen) = nullptr; // chi ha chiesto non la vuole più (nullptr: l'anteprima)
};

struct PvRes {
    uint64_t gen = 0;
    int kind = PV_NONE;
    DWORD err = 0;                     // errore di lettura (file o cartella)
    // testo: senza a capo; la riga i va da lines[i] a lines[i + 1]
    std::wstring text;
    std::vector<uint32_t> lines;
    int lineCount = 0;                 // righe vere del file (lines conta anche quelle spezzate a kPvLineMax)
    const wchar_t* enc = L"";
    bool truncated = false;
    uint64_t fileSize = 0;
    // immagine o icona grande: BGRA non premoltiplicato, w × h (già orientata)
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    int imgW = 0, imgH = 0;            // immagine originale (orientata)
    const wchar_t* format = L"";
    bool tooBig = false;
    // cartella
    Listing* dir = nullptr;
    bool more = false;
    // informazioni come nel tooltip (e conteggi della cartella)
    TipInfo info;
    double readMs = 0, decodeMs = 0;
    PvRes() = default;
    PvRes(const PvRes&) = delete;
    PvRes& operator=(const PvRes&) = delete;
    ~PvRes() { delete dir; }
};

// Generazione più recente: un thread superato lo vede e smette prima
static volatile LONG64 g_pvLatest = 0;
static bool pvSuperseded(uint64_t gen) { return (uint64_t)InterlockedCompareExchange64(&g_pvLatest, 0, 0) != gen; }
// La richiesta r è superata: anteprima o miniature della griglia (thumbs.cpp), ognuna col suo contatore
static bool pvCancelled(const PvReq& r) { return r.superseded ? r.superseded(r.gen) : pvSuperseded(r.gen); }
static bool pvCancelThunk(void* r) { return pvCancelled(*(const PvReq*)r); } // per l'estrazione da uno zip
static const uint64_t kPvZipMax = (uint64_t)128 << 20; // immagine dentro uno zip: oltre, niente estrazione in memoria
// Decodifiche intere oltre kPvBigPx: una alla volta, anche con più thread di miniature (memoria)
static const uint64_t kPvBigPx = 16000000;
static SRWLOCK g_pvBigLock = SRWLOCK_INIT;

// Riferimento COM che si rilascia da solo
template <class T> struct ComRef {
    T* p = nullptr;
    ~ComRef() { if (p) p->Release(); }
    T* operator->() const { return p; }
    T** put() { if (p) { p->Release(); p = nullptr; } return &p; }
    void** putv() { return (void**)put(); }
};

// --- tipi di file ---------------------------------------------------------------------------
static bool extIn(const wchar_t* ext, const wchar_t* const* list) {
    for (int i = 0; list[i]; i++) if (!_wcsicmp(ext, list[i])) return true;
    return false;
}

// Formati che WIC legge (WebP, HEIC, AVIF solo con il codec di Windows: senza, si vedono le informazioni)
static const wchar_t* const kImgExt[] = { L".png", L".jpg", L".jpeg", L".jpe", L".jfif", L".gif", L".bmp", L".dib",
    L".tif", L".tiff", L".ico", L".webp", L".heic", L".heif", L".avif", L".jxr", L".wdp", L".hdp", nullptr };
static const wchar_t* const kTextExt[] = { L".txt", L".md", L".markdown", L".log", L".json", L".jsonc", L".ini", L".cfg",
    L".conf", L".config", L".toml", L".yaml", L".yml", L".xml", L".csv", L".tsv", L".c", L".h", L".cpp", L".hpp", L".cc",
    L".cxx", L".hh", L".inl", L".cs", L".java", L".kt", L".js", L".mjs", L".cjs", L".ts", L".tsx", L".jsx", L".py", L".rb",
    L".go", L".rs", L".swift", L".php", L".pl", L".lua", L".sh", L".bash", L".ps1", L".psm1", L".bat", L".cmd", L".sql",
    L".html", L".htm", L".css", L".scss", L".less", L".hlsl", L".glsl", L".vue", L".svg", L".gitignore", L".gitattributes",
    L".editorconfig", L".env", L".properties", L".reg", L".srt", L".vtt", L".nfo", L".diff", L".patch", L".tex", L".rst",
    L".asm", L".cmake", L".sln", L".vcxproj", L".csproj", L".props", L".targets", L".manifest", L".inf", L".url", L".m3u", nullptr };
static const wchar_t* const kBinExt[] = { L".exe", L".dll", L".sys", L".msi", L".zip", L".7z", L".rar", L".gz", L".tar",
    L".xz", L".bz2", L".zst", L".iso", L".img", L".vhd", L".vhdx", L".pdf", L".doc", L".docx", L".xls", L".xlsx", L".ppt",
    L".pptx", L".odt", L".ods", L".mp3", L".wav", L".flac", L".ogg", L".m4a", L".aac", L".mp4", L".mkv", L".avi", L".mov",
    L".wmv", L".webm", L".ttf", L".otf", L".woff", L".woff2", L".pdb", L".obj", L".lib", L".bin", L".dat", L".db",
    L".sqlite", L".jar", L".class", L".pyc", L".psd", L".lnk", L".cab", L".apk", nullptr };

// 1: testo, -1: binario, 0: da capire leggendo i primi byte
static int pvTextKind(const wchar_t* ext) {
    if (!ext) return 0;
    if (extIn(ext, kTextExt)) return 1;
    if (extIn(ext, kBinExt)) return -1;
    return 0;
}

static const wchar_t* pvFormatName(const wchar_t* ext) {
    if (!ext) return L"";
    if (!_wcsicmp(ext, L".jpg") || !_wcsicmp(ext, L".jpeg") || !_wcsicmp(ext, L".jpe") || !_wcsicmp(ext, L".jfif")) return L"JPEG";
    if (!_wcsicmp(ext, L".tif") || !_wcsicmp(ext, L".tiff")) return L"TIFF";
    if (!_wcsicmp(ext, L".heic") || !_wcsicmp(ext, L".heif")) return L"HEIF";
    if (!_wcsicmp(ext, L".jxr") || !_wcsicmp(ext, L".wdp") || !_wcsicmp(ext, L".hdp")) return L"JPEG XR";
    if (!_wcsicmp(ext, L".png")) return L"PNG";
    if (!_wcsicmp(ext, L".gif")) return L"GIF";
    if (!_wcsicmp(ext, L".bmp") || !_wcsicmp(ext, L".dib")) return L"BMP";
    if (!_wcsicmp(ext, L".ico")) return L"ICO";
    if (!_wcsicmp(ext, L".webp")) return L"WebP";
    if (!_wcsicmp(ext, L".avif")) return L"AVIF";
    return L"";
}

// --- pixel ----------------------------------------------------------------------------------
// Lato lungo nel riquadro bw × bh, senza mai ingrandire
static void pvFit(int w, int h, int bw, int bh, int& fw, int& fh) {
    double k = std::min(1.0, std::min((double)std::max(1, bw) / w, (double)std::max(1, bh) / h));
    fw = std::max(1, (int)lround(w * k));
    fh = std::max(1, (int)lround(h * k));
}

static void pvUnpremultiply(std::vector<uint32_t>& px) {
    for (uint32_t& p : px) {
        uint32_t a = p >> 24;
        if (a == 255) continue;
        if (!a) { p = 0; continue; }
        uint32_t r = std::min(255u, ((p >> 16) & 255) * 255 / a), g = std::min(255u, ((p >> 8) & 255) * 255 / a),
                 b = std::min(255u, (p & 255) * 255 / a);
        p = a << 24 | r << 16 | g << 8 | b;
    }
}

// Orientamento EXIF (1–8) applicato all'immagine già ridotta: (X, Y) nel risultato viene da (x, y)
static void pvOrient(std::vector<uint32_t>& px, int& w, int& h, int o) {
    if (o <= 1 || o > 8) return;
    int W = o >= 5 ? h : w, H = o >= 5 ? w : h;
    std::vector<uint32_t> d((size_t)W * H);
    for (int Y = 0; Y < H; Y++)
        for (int X = 0; X < W; X++) {
            int x, y;
            switch (o) {
            case 2:  x = w - 1 - X; y = Y; break;             // specchiata
            case 3:  x = w - 1 - X; y = h - 1 - Y; break;     // capovolta
            case 4:  x = X; y = h - 1 - Y; break;             // specchiata in verticale
            case 5:  x = Y; y = X; break;                     // trasposta
            case 6:  x = Y; y = h - 1 - X; break;             // ruotata di 90° in senso orario
            case 7:  x = w - 1 - Y; y = h - 1 - X; break;     // trasversa
            default: x = w - 1 - Y; y = X; break;             // 8: ruotata di 90° in senso antiorario
            }
            d[(size_t)Y * W + X] = px[(size_t)y * w + x];
        }
    px.swap(d);
    w = W; h = H;
}

// Riduzione con la media dei pixel, pesata con l'alfa (ingresso e uscita non premoltiplicati)
static void pvScaleDown(const uint32_t* src, int sw, int sh, int stride, std::vector<uint32_t>& dst, int dw, int dh) {
    dst.assign((size_t)dw * dh, 0);
    for (int y = 0; y < dh; y++) {
        int y0 = y * sh / dh, y1 = std::max(y0 + 1, (y + 1) * sh / dh);
        for (int x = 0; x < dw; x++) {
            int x0 = x * sw / dw, x1 = std::max(x0 + 1, (x + 1) * sw / dw);
            uint64_t a = 0, r = 0, g = 0, b = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    uint32_t p = src[(size_t)yy * stride + xx], pa = p >> 24;
                    a += pa; r += ((p >> 16) & 255) * pa; g += ((p >> 8) & 255) * pa; b += (p & 255) * pa;
                }
            if (!a) continue;
            uint64_t n = (uint64_t)(y1 - y0) * (x1 - x0);
            dst[(size_t)y * dw + x] = (uint32_t)(a / n) << 24 | (uint32_t)(r / a) << 16 | (uint32_t)(g / a) << 8 | (uint32_t)(b / a);
        }
    }
}

// --- lavoro sul thread ----------------------------------------------------------------------
// Icona grande della shell (SHIL_JUMBO), ritagliata sul disegno e ridotta a iconPx: per i file
// senza anteprima, le unità e (in griglia) le cartelle. Le icone che non hanno la misura grande la shell le mette in un
// angolo di 256 px: il ritaglio le riporta alla loro misura (non si ingrandiscono).
static void pvBigIcon(const PvReq& r, PvRes& o) {
    // Come i thread delle icone (iconThread): finché il thread IQ_EXT non ha fatto la prima
    // SHGetFileInfo, una chiamata insieme alla sua fallisce (all'avvio con l'anteprima aperta
    // restava senza icona; prove\icona-grande-prova.cpp: 40 processi su 40 senza l'attesa, 0 con).
    WaitForSingleObject(I.shellReady, INFINITE);
    const wchar_t* ext = fileExt(r.path);
    bool dir = r.icon == ICON_DIR, real = r.icon == ICON_REAL;
    std::wstring fake = dir ? std::wstring(L"cartella") : std::wstring(L"file") + (ext ? ext : L"");
    SHFILEINFOW sfi = {};
    if (!SHGetFileInfoW(real ? r.path.c_str() : fake.c_str(), real ? 0 : dir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                        SHGFI_SYSICONINDEX | (real ? 0 : SHGFI_USEFILEATTRIBUTES)))
        return;
    IImageList* il = nullptr;
    if (FAILED(SHGetImageList(SHIL_JUMBO, kIidImageList, (void**)&il)) || !il) return;
    HICON hi = nullptr;
    il->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hi);
    il->Release();
    if (!hi) return;
    const int S = kPvIconSrc;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = S;
    bi.bmiHeader.biHeight = -S;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    std::vector<uint32_t> big;
    if (bmp) {
        HGDIOBJ old = SelectObject(dc, bmp);
        iconRaster(dc, bits, S, hi, big);
        SelectObject(dc, old);
        DeleteObject(bmp);
    }
    DeleteDC(dc);
    DestroyIcon(hi);
    if (big.empty()) return;
    int x0 = S, y0 = S, x1 = -1, y1 = -1;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++)
            if (big[(size_t)y * S + x] >> 24) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
    if (x1 < 0) return;
    int cw = x1 - x0 + 1, ch = y1 - y0 + 1, fw, fh;
    pvFit(cw, ch, r.iconPx, r.iconPx, fw, fh);
    const uint32_t* src = big.data() + (size_t)y0 * S + x0;
    if (fw == cw && fh == ch) {
        o.px.resize((size_t)cw * ch);
        for (int y = 0; y < ch; y++) memcpy(&o.px[(size_t)y * cw], src + (size_t)y * S, (size_t)cw * 4);
    } else {
        pvScaleDown(src, cw, ch, S, o.px, fw, fh);
    }
    o.w = fw; o.h = fh;
}

// Orientamento EXIF di una foto (1 se non c'è)
static int pvOrientation(IWICBitmapFrameDecode* fr) {
    ComRef<IWICMetadataQueryReader> q;
    if (FAILED(fr->GetMetadataQueryReader(q.put()))) return 1;
    for (const wchar_t* key : { L"/app1/ifd/{ushort=274}", L"/ifd/{ushort=274}" }) { // JPEG, TIFF
        PROPVARIANT v;
        PropVariantInit(&v);
        if (SUCCEEDED(q->GetMetadataByName(key, &v))) {
            int o = v.vt == VT_UI2 ? v.uiVal : 1;
            PropVariantClear(&v);
            if (o >= 1 && o <= 8) return o;
        }
    }
    return 1;
}

// Immagine con WIC, ridotta a r.boxW × r.boxH. false: non decodificabile (si mostrano le informazioni)
static bool pvImage(const PvReq& r, PvRes& o, const wchar_t* ext) {
    double t0 = nowMs();
    ComRef<IWICImagingFactory> f;
    HRESULT hr = CoCreateInstance(kClsidWic, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), f.putv());
    // La prima volta nel processo si carica la DLL: si misura quella, chiunque la chieda per primo
    // (anteprima o miniature della griglia). Scritto una volta sola da quel thread, letto da F12.
    static volatile LONG wicLoaded = 0;
    if (InterlockedExchange(&wicLoaded, 1) == 0) g_wicFirstMs = nowMs() - t0;
    if (FAILED(hr)) return false;
    double t1 = nowMs();
    // Dentro uno zip: il file estratto in memoria e decodificato da lì (zip.cpp)
    size_t alen = 0;
    std::string mem;
    ComRef<IWICStream> ws;
    HANDLE fh = INVALID_HANDLE_VALUE;
    if (zipInside(r.path, alen)) {
        uint64_t size = 0;
        if (zipReadMem(r.path, alen, kPvZipMax, mem, &size, pvCancelThunk, (void*)&r) != ZE_OK || mem.size() < size)
            return false; // troppo grande o non estraibile: le informazioni
        if (FAILED(f->CreateStream(ws.put())) || FAILED(ws->InitializeFromMemory((BYTE*)&mem[0], (DWORD)mem.size()))) return false;
    } else {
        // Aperto da noi, con tutte le condivisioni: anche un file che un altro programma sta scrivendo
        fh = CreateFileW(longPath(r.path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, 0, nullptr);
        if (fh == INVALID_HANDLE_VALUE) { o.err = GetLastError(); return false; }
    }
    struct Closer { HANDLE h; ~Closer() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); } } closer{ fh }; // dopo gli oggetti WIC che lo usano
    ComRef<IWICBitmapDecoder> dec;
    HRESULT hd = ws.p ? f->CreateDecoderFromStream(ws.p, nullptr, WICDecodeMetadataCacheOnDemand, dec.put())
                      : f->CreateDecoderFromFileHandle((ULONG_PTR)fh, nullptr, WICDecodeMetadataCacheOnDemand, dec.put());
    if (FAILED(hd)) return false;
    UINT frames = 0, pick = 0;
    dec->GetFrameCount(&frames);
    if (frames > 1 && !_wcsicmp(ext, L".ico")) { // icona: la misura più grande
        UINT best = 0;
        for (UINT i = 0; i < frames && i < 32; i++) {
            ComRef<IWICBitmapFrameDecode> fi;
            UINT w = 0, h = 0;
            if (SUCCEEDED(dec->GetFrame(i, fi.put())) && SUCCEEDED(fi->GetSize(&w, &h)) && w * h > best) { best = w * h; pick = i; }
        }
    }
    ComRef<IWICBitmapFrameDecode> fr;
    if (FAILED(dec->GetFrame(pick, fr.put()))) return false;
    UINT w = 0, h = 0;
    if (FAILED(fr->GetSize(&w, &h)) || !w || !h) return false;
    int orient = pvOrientation(fr.p);
    bool swap = orient >= 5;
    o.imgW = (int)(swap ? h : w);
    o.imgH = (int)(swap ? w : h);
    o.format = pvFormatName(ext);
    int fw, fhh;
    pvFit(o.imgW, o.imgH, r.boxW, r.boxH, fw, fhh);
    UINT sw = (UINT)(swap ? fhh : fw), sh = (UINT)(swap ? fw : fhh); // misura finale, prima di orientare
    IWICBitmapSource* src = fr.p;
    UINT cw = w, ch = h;
    // JPEG: il decodificatore riduce da solo (1/2, 1/4, 1/8) senza decodificare l'immagine intera
    ComRef<IWICBitmapSourceTransform> st;
    ComRef<IWICBitmap> reduced;
    if ((w >= sw * 2 || h >= sh * 2) && SUCCEEDED(fr->QueryInterface(__uuidof(IWICBitmapSourceTransform), st.putv()))) {
        UINT d = 8;
        while (d > 1 && ((w + d - 1) / d < sw || (h + d - 1) / d < sh)) d /= 2;
        UINT tw = (w + d - 1) / d, th = (h + d - 1) / d;
        WICPixelFormatGUID pf = kWicBGRA;
        if (d > 1 && SUCCEEDED(st->GetClosestSize(&tw, &th)) && SUCCEEDED(st->GetClosestPixelFormat(&pf)) &&
            tw >= sw && th >= sh && tw < w) {
            UINT bpp = (pf == kWicBGRA || pf == kWicBGR32 || pf == kWicPBGRA) ? 4 : pf == kWicBGR24 ? 3 : 0;
            if (bpp && !pvCancelled(r)) {
                UINT stride = (tw * bpp + 3) & ~3u;
                std::vector<uint8_t> buf((size_t)stride * th);
                if (SUCCEEDED(st->CopyPixels(nullptr, tw, th, &pf, WICBitmapTransformRotate0, stride, (UINT)buf.size(), buf.data())) &&
                    SUCCEEDED(f->CreateBitmapFromMemory(tw, th, pf, stride, (UINT)buf.size(), buf.data(), reduced.put()))) {
                    src = reduced.p;
                    cw = tw; ch = th;
                }
            }
        }
    }
    if ((uint64_t)cw * ch > kPvImgMaxPx) { o.tooBig = true; return false; }
    if (pvCancelled(r)) return false;
    struct BigLock { bool on; BigLock(bool b) : on(b) { if (on) AcquireSRWLockExclusive(&g_pvBigLock); } ~BigLock() { if (on) ReleaseSRWLockExclusive(&g_pvBigLock); } }
        big((uint64_t)cw * ch > kPvBigPx);
    if (big.on && pvCancelled(r)) return false;
    ComRef<IWICFormatConverter> cv;
    if (FAILED(f->CreateFormatConverter(cv.put())) ||
        FAILED(cv->Initialize(src, kWicPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        return false;
    IWICBitmapSource* last = cv.p;
    ComRef<IWICBitmapScaler> sc; // la riduzione si fa premoltiplicata: niente aloni scuri sui bordi trasparenti
    if (cw != sw || ch != sh) {
        if (FAILED(f->CreateBitmapScaler(sc.put())) || FAILED(sc->Initialize(cv.p, sw, sh, WICBitmapInterpolationModeFant)))
            return false;
        last = sc.p;
    }
    std::vector<uint32_t> px((size_t)sw * sh);
    if (FAILED(last->CopyPixels(nullptr, sw * 4, (UINT)(px.size() * 4), (BYTE*)px.data()))) return false;
    pvUnpremultiply(px);
    int ow = (int)sw, oh = (int)sh;
    pvOrient(px, ow, oh, orient);
    o.px.swap(px);
    o.w = ow; o.h = oh;
    o.kind = PV_IMAGE;
    o.decodeMs = nowMs() - t1;
    return true;
}

// Legge fino a want byte (a pezzi: una richiesta superata smette prima). false se superata o in errore
static bool pvReadUpTo(HANDLE f, std::string& buf, size_t want, const PvReq& r) {
    size_t have = buf.size();
    buf.resize(want);
    while (have < want) {
        if (pvCancelled(r)) return false;
        DWORD got = 0, chunk = (DWORD)std::min<size_t>(want - have, 64 * 1024);
        if (!ReadFile(f, &buf[have], chunk, &got, nullptr)) { buf.resize(have); return false; }
        if (!got) break;
        have += got;
    }
    buf.resize(have);
    return true;
}

enum { ENC_BINARY, ENC_UTF8BOM, ENC_UTF16LE, ENC_UTF16BE, ENC_UTF8, ENC_ASCII, ENC_ANSI };

// Codifica del testo, o ENC_BINARY. complete: il buffer è tutto il file (altrimenti una sequenza
// UTF-8 tagliata alla fine non conta come errore)
static int pvEncoding(const uint8_t* p, size_t n, bool complete) {
    if (n >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) return ENC_UTF8BOM;
    if (n >= 2 && p[0] == 0xFF && p[1] == 0xFE) return ENC_UTF16LE;
    if (n >= 2 && p[0] == 0xFE && p[1] == 0xFF) return ENC_UTF16BE;
    size_t m = std::min(n, (size_t)kPvSniff);
    // UTF-16 senza BOM: un testo in alfabeto latino ha uno zero in quasi ogni byte dispari (o pari)
    if (m >= 16) {
        size_t z0 = 0, z1 = 0, half = m / 2;
        for (size_t i = 0; i + 1 < m; i += 2) { z0 += !p[i]; z1 += !p[i + 1]; }
        if (z1 * 10 > half * 4 && z0 * 20 < half) return ENC_UTF16LE;
        if (z0 * 10 > half * 4 && z1 * 20 < half) return ENC_UTF16BE;
    }
    // Binario: uno zero, o più del 5% di caratteri di controllo
    size_t ctl = 0;
    for (size_t i = 0; i < m; i++) {
        uint8_t c = p[i];
        if (!c) return ENC_BINARY;
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != 0x0C && c != 0x1B) ctl++;
    }
    if (ctl * 20 > m) return ENC_BINARY;
    // UTF-8 valido (sequenze più lunghe del necessario e surrogati non lo sono)? Altrimenti ANSI.
    bool ascii = true;
    for (size_t i = 0; i < n;) {
        uint8_t c = p[i];
        if (c < 0x80) { i++; continue; }
        ascii = false;
        size_t len = (c >= 0xC2 && c <= 0xDF) ? 2 : (c >= 0xE0 && c <= 0xEF) ? 3 : (c >= 0xF0 && c <= 0xF4) ? 4 : 0;
        if (!len) return ENC_ANSI;
        if (i + len > n) return complete ? ENC_ANSI : ENC_UTF8; // tagliata dalla lettura
        for (size_t j = 1; j < len; j++) if ((p[i + j] & 0xC0) != 0x80) return ENC_ANSI;
        uint8_t c1 = p[i + 1];
        if ((c == 0xE0 && c1 < 0xA0) || (c == 0xED && c1 >= 0xA0) || (c == 0xF0 && c1 < 0x90) || (c == 0xF4 && c1 >= 0x90))
            return ENC_ANSI;
        i += len;
    }
    return ascii ? ENC_ASCII : ENC_UTF8;
}

// Testo decodificato → righe: a capo di Windows, Unix e Mac; tabulazioni in spazi; caratteri di
// controllo in spazi; righe troppo lunghe spezzate a kPvLineMax
static void pvLines(const std::wstring& w, PvRes& o) {
    o.text.reserve(w.size());
    o.lines.push_back(0);
    int col = 0, newlines = 0;
    bool lastNewline = false;
    for (size_t i = 0; i < w.size(); i++) {
        wchar_t c = w[i];
        if (c == L'\r') { if (i + 1 < w.size() && w[i + 1] == L'\n') i++; c = L'\n'; }
        if (c == L'\n') { o.lines.push_back((uint32_t)o.text.size()); col = 0; newlines++; lastNewline = true; continue; }
        lastNewline = false;
        if (c == 0xFEFF) continue;
        if (col >= kPvLineMax) { o.lines.push_back((uint32_t)o.text.size()); col = 0; }
        if (c == L'\t') { int n = kPvTab - col % kPvTab; o.text.append((size_t)n, L' '); col += n; continue; }
        if (c < 0x20 || c == 0x7F) c = L' ';
        o.text.push_back(c);
        col++;
    }
    // "a\nb\n" sono due righe, non tre
    if (o.lines.size() > 1 && o.lines.back() == o.text.size() && !o.truncated) o.lines.pop_back();
    o.lineCount = newlines + 1 - (lastNewline && newlines > 0 ? 1 : 0);
}

// Testo. known: estensione di testo; altrimenti si guarda prima l'inizio. false: non è testo
static bool pvText(const PvReq& r, PvRes& o, bool known) {
    double t0 = nowMs();
    uint64_t size = 0;
    std::string buf;
    bool ok;
    int enc;
    size_t alen = 0;
    if (zipInside(r.path, alen)) { // dentro uno zip: l'inizio del file, estratto in memoria (zip.cpp)
        ok = zipReadMem(r.path, alen, kPvTextMax, buf, &size, pvCancelThunk, (void*)&r) == ZE_OK;
        enc = ok ? pvEncoding((const uint8_t*)buf.data(), buf.size(), buf.size() >= size) : ENC_BINARY;
    } else {
        HANDLE f = CreateFileW(longPath(r.path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (f == INVALID_HANDLE_VALUE) { o.err = GetLastError(); return false; }
        LARGE_INTEGER sz = {};
        GetFileSizeEx(f, &sz);
        size = (uint64_t)sz.QuadPart;
        size_t limit = (size_t)std::min<uint64_t>(size, kPvTextMax);
        ok = pvReadUpTo(f, buf, known ? limit : std::min(limit, (size_t)kPvSniff), r);
        enc = ok ? pvEncoding((const uint8_t*)buf.data(), buf.size(), buf.size() >= size) : ENC_BINARY;
        if (ok && enc != ENC_BINARY && buf.size() < limit) { // sembra testo: il resto, fino al limite
            ok = pvReadUpTo(f, buf, limit, r);
            if (ok) enc = pvEncoding((const uint8_t*)buf.data(), buf.size(), buf.size() >= size);
        }
        CloseHandle(f);
    }
    o.readMs = nowMs() - t0;
    if (!ok || enc == ENC_BINARY) return false;
    double t1 = nowMs();
    o.truncated = buf.size() < size;
    o.fileSize = size;
    const char* p = buf.data();
    size_t n = buf.size();
    std::wstring w;
    switch (enc) {
    case ENC_UTF16LE: case ENC_UTF16BE: {
        size_t off = (n >= 2 && ((uint8_t)p[0] == 0xFF || (uint8_t)p[0] == 0xFE) && ((uint8_t)p[1] == 0xFE || (uint8_t)p[1] == 0xFF)) ? 2 : 0;
        w.resize((n - off) / 2);
        for (size_t i = 0; i < w.size(); i++) {
            uint8_t a = (uint8_t)p[off + i * 2], b = (uint8_t)p[off + i * 2 + 1];
            w[i] = (wchar_t)(enc == ENC_UTF16LE ? (a | b << 8) : (b | a << 8));
        }
        if (o.truncated && !w.empty() && w.back() >= 0xD800 && w.back() <= 0xDBFF) w.pop_back(); // coppia tagliata
        o.enc = enc == ENC_UTF16LE ? L"UTF-16 LE" : L"UTF-16 BE";
        break;
    }
    case ENC_ANSI: {
        int k = MultiByteToWideChar(CP_ACP, 0, p, (int)n, nullptr, 0);
        w.resize((size_t)k);
        if (k) MultiByteToWideChar(CP_ACP, 0, p, (int)n, &w[0], k);
        o.enc = L"ANSI";
        break;
    }
    default: { // UTF-8 (con o senza BOM) e ASCII
        size_t off = enc == ENC_UTF8BOM ? 3 : 0;
        if (o.truncated) { // una sequenza tagliata dalla lettura non diventa un carattere sbagliato
            size_t e = n, back = 0;
            while (e > off && back < 3 && ((uint8_t)p[e - 1] & 0xC0) == 0x80) { e--; back++; }
            if (e > off && (uint8_t)p[e - 1] >= 0xC0) {
                uint8_t lead = (uint8_t)p[e - 1];
                size_t len = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
                if (back + 1 < len) n = e - 1;
            }
        }
        int k = MultiByteToWideChar(CP_UTF8, 0, p + off, (int)(n - off), nullptr, 0);
        w.resize((size_t)k);
        if (k) MultiByteToWideChar(CP_UTF8, 0, p + off, (int)(n - off), &w[0], k);
        o.enc = enc == ENC_UTF8BOM ? L"UTF-8 con BOM" : enc == ENC_ASCII ? L"ASCII" : L"UTF-8";
        break;
    }
    }
    pvLines(w, o);
    o.kind = PV_TEXT;
    o.decodeMs = nowMs() - t1;
    return true;
}

// Contenuto di una cartella, ordinato come la lista (cartelle prima, ordine naturale)
static void pvDir(const PvReq& r, PvRes& o) {
    double t0 = nowMs();
    Listing* l = new Listing;
    l->path = r.path;
    l->gen = r.gen; // come la lista: un Listing nuovo allo stesso indirizzo non sembra lo stesso (le miniature, c.remote)
    o.dir = l;
    o.kind = PV_DIR;
    size_t alen = 0;
    if (zipSplit(r.path, alen)) { // uno zip, o una cartella dentro: dall'archivio in memoria (zip.cpp)
        l->zipLen = (uint32_t)alen;
        ZipRef z = zipResolve(r.path, alen);
        if (!z.z || z.err) {
            o.err = z.err ? z.err : ERROR_BAD_FORMAT;
        } else if (!z.z->isDir(z.z->nodes[z.node])) {
            // Uno zip dentro uno zip è un file dell'archivio esterno: il suo contenuto non si legge senza
            // estrarlo, e pvBuild ripiega sulle informazioni di un file (non "Cartella vuota")
            o.err = ERROR_DIRECTORY;
        } else {
            const ZipNode& n = z.z->nodes[z.node];
            zipFill(l, *z.z, z.node, kPvDirMax, &o.more);
            o.info.dirs = (int)n.dirs;
            o.info.files = (int)n.files;
            o.info.zip = true;
        }
        o.readMs = nowMs() - t0;
        double t1 = nowMs();
        buildKeys(l);
        buildLower(l); // come la lista: la selezione si ritrova per nome alla rilettura (pvPaneSet)
        sortListing(l, l->order, COL_NAME, false);
        o.decodeMs = nowMs() - t1;
        return;
    }
    std::wstring q = longPath(r.path);
    if (q.back() != L'\\') q += L'\\';
    q += L'*';
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(q.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    int dirs = 0, files = 0;
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (e != ERROR_FILE_NOT_FOUND) o.err = e; // radice vuota: nessun errore
    } else {
        do {
            uint32_t len = (uint32_t)wcslen(fd.cFileName);
            if (isDots(fd.cFileName, len)) continue;
            if ((int)l->e.size() >= kPvDirMax ||
                ((l->e.size() & 255) == 255 && (nowMs() - t0 > kPvDirMs || pvCancelled(r)))) { o.more = true; break; }
            uint64_t size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            addEntry(l, fd.cFileName, len, fd.dwFileAttributes, size, fileTime64(fd.ftLastWriteTime), fileTime64(fd.ftCreationTime));
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) dirs++; else files++;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    o.info.dirs = dirs;
    o.info.files = files;
    o.readMs = nowMs() - t0;
    double t1 = nowMs();
    buildKeys(l);
    buildLower(l);
    sortListing(l, l->order, COL_NAME, false);
    o.decodeMs = nowMs() - t1;
}

// Tutto il resto: le informazioni del tooltip e l'icona grande
static void pvInfo(const PvReq& r, PvRes& o) {
    o.kind = PV_INFO;
    double t0 = nowMs();
    if (!r.drive) readInfo(r.path, false, false, &o.info);
    o.readMs += nowMs() - t0;
    double t1 = nowMs();
    pvBigIcon(r, o);
    o.decodeMs += nowMs() - t1;
}

static void pvBuild(const PvReq& r, PvRes& o) {
    if (r.drive) { pvInfo(r, o); return; }
    if (r.attr & FILE_ATTRIBUTE_DIRECTORY) { pvDir(r, o); return; }
    // File solo online (OneDrive e simili): leggerlo lo scaricherebbe. Solo informazioni.
    bool online = onlineOnly(r.attr);
    const wchar_t* ext = fileExt(r.path);
    if (!online && ext && !_wcsicmp(ext, L".zip")) { // uno zip: il contenuto, come una cartella
        pvDir(r, o);
        if (!o.err) return;
        delete o.dir; // non si legge: le informazioni
        o.dir = nullptr;
        o.kind = PV_NONE;
        o.err = 0;
        o.info = TipInfo();
    } else if (!online) {
        if (ext && extIn(ext, kImgExt)) {
            if (pvImage(r, o, ext)) return;
        } else {
            int t = pvTextKind(ext);
            if (t >= 0 && pvText(r, o, t > 0)) return;
        }
    }
    if (pvCancelled(r)) return;
    int imgW = o.imgW, imgH = o.imgH;
    pvInfo(r, o);
    if (imgW > 0 && !o.info.imgW) { o.info.imgW = imgW; o.info.imgH = imgH; } // misura letta da WIC (troppo grande)
}

static DWORD WINAPI pvThread(void* p) {
    PvReq* r = (PvReq*)p;
    PvRes* o = new PvRes;
    o->gen = r->gen;
    // STA senza pompa dei messaggi, a differenza dei thread delle icone e delle miniature: questo vive
    // per una richiesta sola, e le finestre nascoste che la shell crea qui spariscono con lui
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); // WIC e shell
    if (!pvCancelled(*r)) pvBuild(*r, *o);
    if (SUCCEEDED(co)) CoUninitialize();
    // Anche superato, il risultato torna: la finestra conta i thread in corso
    if (!PostMessageW(r->hwnd, WM_APP_PREVIEW, 0, (LPARAM)o)) delete o;
    delete r;
    return 0;
}

// --- stato sul thread dell'interfaccia ------------------------------------------------------
struct PvItem {
    std::wstring path, name;
    uint32_t attr = 0;
    uint64_t size = 0, mtime = 0;
    bool drive = false;          // voce di Questo PC (anche un percorso di rete)
    int icon = ICON_EXT;         // iconSource: un percorso di rete ha l'icona di cartella
};

struct Preview {
    // Voce sotto il cursore: a ogni frame si confrontano cartella e indice (economico); solo se
    // cambiano si guarda la voce vera (percorso, data, dimensione)
    const Listing* keyList = nullptr;
    uint64_t keyGen = 0;
    int keyEntry = -2;
    bool want = false;          // c'è una voce da mostrare
    PvItem item;                // voce chiesta
    PvItem shown;               // voce di cui è il contenuto mostrato
    uint64_t gen = 0;           // ultima richiesta
    bool stale = false;         // il contenuto mostrato non è della voce chiesta
    double staleSince = 0;
    PvReq* pending = nullptr;   // in attesa di un thread (vale solo l'ultima)
    int running = 0;            // thread in corso
    double lastStart = 0;
    uint64_t refreshGen = 0;    // la richiesta (gen) di una rilettura della cartella ancora in corso…
    bool refreshAgain = false;  // …e un cambiamento arrivato intanto: si rilegge quando arriva
    PvRes* res = nullptr;       // contenuto mostrato (con la copia dei pixel della texture)
    // Scorrimento del testo: riga logica in cima e riga visiva dentro di essa
    int top = 0, topSub = 0, visRows = 0;
    float wrapW = 0, wheelAcc = 0;
    // Una cartella è una lista come quella principale (ListPane: voci, selezione, vista), che si
    // scambia con App nei PaneScope per disegnarla e agirci. Possiede il Listing (preso da PvRes).
    ListPane pane;
    bool paneShown = false;     // disegnata in questo frame (non "caricamento…"): il mouse la può usare
    bool keys = false;          // la tastiera è dell'anteprima (clic nel pannello, Tab)
    std::wstring selectAfter;   // dopo un'operazione nell'anteprima: la voce su cui mettere il cursore…
    bool renameAfter = false;   // …e da rinominare (nuova cartella)
    int keepFirst = -1;         // vista cambiata: la prima voce visibile da tenere in vista
    float viewX = 0, viewW = 0; // controllo della vista nella banda (solo per una cartella)
    bool viewHover = false;
    HoverFade viewFade;
} PV;

// Perché l'anteprima aperta non si vede: la finestra non le lascia spazio, a destra o in basso
static const wchar_t* previewNoRoom() {
    return A.prevBottom ? L"La finestra è troppo bassa per l'anteprima in basso" : L"La finestra è troppo stretta per l'anteprima";
}

// Tab: la tastiera passa tra la lista e l'anteprima di una cartella. Nell'anteprima il cursore
// compare dov'era (la prima voce, la prima volta)
static void previewKeysToggle() {
    if (PV.keys) { PV.keys = false; A.dirty = true; return; }
    ListPane* p = PV.paneShown && !g_inPane && PV.pane.cur ? &PV.pane : nullptr;
    if (!p) {
        notice(A.prevW > 0 ? L"L'anteprima non mostra una cartella" : A.prevOpen ? previewNoRoom() : L"L'anteprima è chiusa", false);
        return;
    }
    PV.keys = true;
    PaneScope ps(p);
    if (!A.view.empty()) ensureVisible();
    tipHide();
    A.dirty = true;
}

// La lista dell'anteprima, da usare in un PaneScope: c'è una cartella e non si è già dentro uno scope
static ListPane* pvPane() { return !g_inPane && PV.pane.cur ? &PV.pane : nullptr; }
// L'altra lista è in PV.pane: fuori da uno scope l'anteprima, dentro (una voce dell'anteprima) la principale
static bool listsBusy() { return A.renaming || A.draggingThumb || PV.pane.renaming || PV.pane.draggingThumb; }
static bool previewHasKeys() { return PV.keys && !g_inPane && PV.pane.cur && A.prevW > 0; }
static void previewKeysFor(bool preview) {
    if (PV.keys != preview) A.dirty = true;
    PV.keys = preview;
}
static ListPane* previewPaneFor(const std::wstring& folder) {
    ListPane* p = pvPane();
    return p && !_wcsicmp(p->cur->path.c_str(), folder.c_str()) ? p : nullptr;
}

// Texture del contenuto mostrato, sul device corrente (anche dopo il passaggio WARP → GPU)
static void previewUpload() {
    safeRelease(G.prevSrv);
    safeRelease(G.prevTex);
    if (!G.dev || !PV.res || PV.res->px.empty()) return;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)PV.res->w;
    td.Height = (UINT)PV.res->h;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd = { PV.res->px.data(), (UINT)PV.res->w * 4, 0 };
    if (SUCCEEDED(G.dev->CreateTexture2D(&td, &sd, &G.prevTex))) G.dev->CreateShaderResourceView(G.prevTex, nullptr, &G.prevSrv);
}

static void pvSetResult(PvRes* o) {
    delete PV.res;
    PV.res = o;
    double t0 = nowMs();
    previewUpload();
    if (o && !o->px.empty()) A.prevTexMs = nowMs() - t0;
}

// Geometria del pannello (il rettangolo di layout, dopo il bordo di 1 px: a sinistra, o sopra in
// basso): contenuto tra x0 e x1, da bodyTop a bodyBot; banda in fondo da footY
struct PvGeom { float x0, x1, top, bodyTop, bodyBot, footY, bot; };
static PvGeom pvGeom() {
    PvGeom g;
    float edgeX = A.prevBottom ? 0.0f : 1.0f, edgeY = A.prevBottom ? 1.0f : 0.0f;
    g.x0 = A.prevX + edgeX + A.padX;
    g.x1 = A.prevX + A.prevW - A.padX;
    g.top = A.prevY + edgeY;
    g.bot = A.prevY + A.prevH; // fino in fondo, come la barra laterale: a destra la banda in fondo
                               // continua la barra di stato della lista
    g.bodyTop = g.top + A.headH + px(ui::GAP_M);
    g.footY = g.bot - A.statusH;
    g.bodyBot = g.footY - px(ui::GAP_S);
    return g;
}
static void pvBox(int& w, int& h) {
    PvGeom g = pvGeom();
    w = std::max(1, (int)(g.x1 - g.x0));
    h = std::max(1, (int)(g.bodyBot - g.bodyTop));
}

static void pvPump();

// Nuova richiesta per PV.item. refresh: stessa voce in un'altra misura, il contenuto resta visibile
static void pvRequest(bool refresh) {
    PV.gen++;
    InterlockedExchange64(&g_pvLatest, (LONG64)PV.gen);
    int bw, bh;
    pvBox(bw, bh);
    PvReq* r = new PvReq{ A.hwnd, PV.gen, PV.item.path, PV.item.attr, PV.item.drive, PV.item.icon, bw, bh, (int)px(ui::PREVIEW_ICON), nullptr };
    if (!refresh) PV.refreshAgain = false; // un'altra voce: la rilettura di quella di prima non serve più
    delete PV.pending;
    PV.pending = r;
    if (!refresh && !PV.stale) {
        PV.stale = true;
        PV.staleSince = nowMs();
        SetTimer(A.hwnd, 12, ui::LOADING_AFTER, nullptr); // poi nome e "caricamento…"
    }
    pvPump();
}

// Fa partire la richiesta in attesa se c'è un thread libero, o se quello in corso è lento
static void pvPump() {
    if (!PV.pending) return;
    double now = nowMs();
    if (PV.running > 0 && (PV.running >= kPvThreads || now - PV.lastStart < ui::PREVIEW_SLOW)) {
        if (PV.running < kPvThreads) SetTimer(A.hwnd, 11, (UINT)std::max(1.0, ui::PREVIEW_SLOW - (now - PV.lastStart)), nullptr);
        return; // altrimenti riparte quando arriva un risultato
    }
    PvReq* r = PV.pending;
    PV.pending = nullptr;
    HANDLE h = CreateThread(nullptr, 0, pvThread, r, 0, nullptr);
    if (!h) { delete r; return; }
    CloseHandle(h);
    PV.running++;
    PV.lastStart = now;
}

static void previewOnTimer(UINT_PTR id) {
    KillTimer(A.hwnd, id);
    if (id == 11) pvPump();
    A.dirty = true; // 12: è passato LOADING_AFTER, si mostra "caricamento…"
}

// Rilegge la cartella mostrata (un'operazione fatta lì, F5, un cambiamento da fuori): il contenuto
// resta finché arriva il nuovo, e la lista tiene selezione e scorrimento (pvPaneSet)
static void previewRefresh() {
    if (g_inPane) { g_paneAfter.push_back(previewRefresh); return; } // pvRequest misura il pannello con la geometria di App
    // Uscendo senza rilettura la notifica resta senza seguito: la sua ora non deve valere per una
    // rilettura più tardi (F12 la misurerebbe da lì). Resta solo per refreshAgain, che la serve
    if (!PV.want || !PV.pane.cur) { A.prevChangeAt = 0; return; }
    // Il cursore è già su un'altra voce (un file grande che si decodifica): rifarne la richiesta la
    // annullerebbe, e con una cartella che cambia di continuo non arriverebbe mai
    if (PV.stale || PV.item.path != PV.shown.path) { A.prevChangeAt = 0; return; }
    // Una rilettura in corso non si supera (una cartella lenta da leggere non si aggiornerebbe mai):
    // la prossima parte quando arriva questa, come refreshAgain della lista
    if (PV.refreshGen == PV.gen) { PV.refreshAgain = true; return; }
    A.prevRefreshStart = A.prevChangeAt > 0 ? A.prevChangeAt : nowMs(); // F12: dalla notifica, o da adesso (F5, operazioni)
    A.prevChangeAt = 0;
    pvRequest(true);
    PV.refreshGen = PV.gen;
}

// Vista delle cartelle nell'anteprima (menu della sua vista, Ctrl+rotellina e scorciatoie con la
// tastiera nell'anteprima): una per il pannello, salvata in impostazioni.txt. Dentro il PaneScope
// dell'anteprima o fuori.
static void previewZoom(int zoom) {
    zoom = std::max((int)ZOOM_DETAILS, std::min((int)ZOOM_MAX, zoom));
    if (zoom == A.prevZoom) return; // la lista dell'anteprima ha sempre questa vista (pvPaneSet)
    A.prevZoom = zoom;
    settingsSave();
    tipHide();
    A.dirty = true;
    ListPane* p = pvPane();
    if (!p && !g_inPane) return; // nessuna cartella mostrata: vale dalla prossima
    PaneScope ps(p); // fuori da uno scope ci si entra; dentro, A è già l'anteprima
    PV.keepFirst = firstVisibleItem(); // la geometria nuova la fa il prossimo frame (pvPaneLayout)
    A.zoom = zoom;
    A.hover = -1;
    A.rowFade = HoverFade();
    A.cursorFor = nullptr; // il cursore salta, non scivola da una forma all'altra
}

// La barra della lista dell'anteprima non si trascina più (la lista cambia o sparisce): il mouse,
// catturato alla pressione, torna libero
static void pvThumbDragStop() {
    if (PV.pane.draggingThumb && GetCapture() == A.hwnd) ReleaseCapture();
    PV.pane.draggingThumb = false;
}

// La cartella non è più mostrata: via la sua lista (e la tastiera torna alla lista principale)
static void pvPaneClear() {
    previewWatch(nullptr);
    pvThumbDragStop();
    delete PV.pane.cur;
    PV.pane = ListPane();
    PV.keys = false;
    PV.selectAfter.clear();
    PV.renameAfter = false;
    PV.keepFirst = -1;
    dropRetarget(); // un trascinamento sopra l'anteprima non punta più a una cartella che non si vede
}

// Una cartella letta (l, dal thread) diventa la lista dell'anteprima. same: è la stessa cartella
// riletta (un'operazione, un cambiamento da fuori): restano selezione per nome, cursore, scorrimento,
// filtro e tastiera, con lo stesso codice della ricarica della lista principale (listReload).
// partial: lettura fermata prima del fondo (PvRes::more)
static void pvPaneSet(Listing* l, bool same, bool partial) {
    ListPane& p = PV.pane;
    previewWatch(l); // cambiamenti da fuori o dalle operazioni: si rilegge (stessa cartella: niente di nuovo)
    dropRetarget(); // un trascinamento fermo sopra l'anteprima: la destinazione è nella lista nuova
    p.hover = p.linkHover = -1;
    p.rowFade = HoverFade();
    p.zoom = A.prevZoom;
    if (same && p.cur) { // la geometria dell'ultimo frame resta: serve a tenere in vista il cursore
        PaneScope ps(&PV.pane);
        listReload(l, PV.selectAfter, PV.renameAfter);
        // La voce nuova o rinominata può stare oltre il punto dove la lettura si è fermata: non la si
        // aspetta, una rilettura molto più tardi la selezionerebbe (e aprirebbe la rinomina) senza motivo
        if (partial) { PV.selectAfter.clear(); PV.renameAfter = false; }
        return;
    }
    delete p.cur;
    p.cur = l;
    p.order.swap(l->order);
    std::vector<uint32_t>().swap(l->order); // come la lista (listReload): non resta l'ordine della cartella di prima
    p.filter.clear(); // i filtri sono della cartella mostrata
    p.colFilter.clear();
    p.view = p.order;
    p.counts.dirty = true; // l può stare all'indirizzo del Listing di prima
    p.selFlags.assign(l->e.size(), 0);
    p.sel = 0;
    p.anchor = -1;
    p.renaming = false;
    pvThumbDragStop();
    p.scrollY = p.scrollTarget = 0;
    p.cursorFor = nullptr;
    PV.keys = false;
    PV.keepFirst = -1;
    PV.selectAfter.clear();
    PV.renameAfter = false;
}

// Niente da mostrare (cartella vuota, pannello chiuso): le richieste in corso diventano superate
static void pvClear() {
    PV.want = false;
    PV.item = PvItem();
    PV.shown = PvItem();
    PV.gen++;
    InterlockedExchange64(&g_pvLatest, (LONG64)PV.gen);
    delete PV.pending;
    PV.pending = nullptr;
    PV.refreshAgain = false;
    PV.stale = false;
    KillTimer(A.hwnd, 12);
    pvSetResult(nullptr);
    pvPaneClear();
    PV.top = PV.topSub = 0;
}

static void previewSync() {
    if (A.prevW <= 0) { // chiuso o senza spazio: niente letture, niente osservatore; torna con lo spazio
        if (PV.want) { pvClear(); PV.keyList = nullptr; PV.keyEntry = -2; }
        return;
    }
    const Listing* l = A.cur;
    int ei = (l && A.sel >= 0 && A.sel < (int)A.view.size()) ? (int)A.view[A.sel] : -1;
    if (l != PV.keyList || (l && l->gen != PV.keyGen) || ei != PV.keyEntry) {
        PV.keyList = l;
        PV.keyGen = l ? l->gen : 0;
        PV.keyEntry = ei;
        if (ei < 0) {
            if (PV.want) pvClear();
        } else {
            const Entry& e = l->e[ei];
            PvItem it;
            it.name.assign(l->name(e), e.nameLen);
            it.path = joinPath(l->path, it.name.c_str());
            it.attr = e.attr;
            it.drive = l->isDrives;
            it.icon = iconSource(it.name.c_str(), it.attr, iconDrive(l, e), l->zipLen != 0);
            it.size = l->isDrives ? 0 : e.size; // per le unità size è il tipo di unità
            it.mtime = e.mtime;
            // Una ricarica della cartella non rilegge un file che non è cambiato
            if (!PV.want || it.path != PV.item.path || it.mtime != PV.item.mtime || it.size != PV.item.size || it.attr != PV.item.attr) {
                PV.item = std::move(it);
                PV.want = true;
                pvRequest(false);
            }
        }
    }
    // Immagine ridotta per un'altra misura del pannello: la si rifà, a misura finita
    if (PV.res && PV.res->kind == PV_IMAGE && !PV.stale && A.colDrag < 0 && !A.inSizeMove && PV.gen == PV.res->gen) {
        int bw, bh, fw, fh;
        pvBox(bw, bh);
        pvFit(PV.res->imgW, PV.res->imgH, bw, bh, fw, fh);
        if (abs(fw - PV.res->w) > 2 || abs(fh - PV.res->h) > 2) pvRequest(true);
    }
}

static void onPreviewResult(PvRes* o) {
    // Dentro un PaneScope (un ciclo modale futuro) PV.pane è la lista principale: si applica all'uscita
    if (g_inPane) { g_paneAfter.push_back([o] { onPreviewResult(o); }); return; }
    PV.running--;
    if (o->gen != PV.gen || !PV.want) { delete o; pvPump(); return; } // superato
    bool same = PV.shown.path == PV.item.path;
    if (!same) PV.top = PV.topSub = 0; // stessa voce (un log che cresce): si resta dove si era
    PV.shown = PV.item;
    // Una cartella: il suo Listing passa alla lista dell'anteprima (PvRes resta per contatori ed errore)
    if (o->kind == PV_DIR && o->dir && !o->err) { pvPaneSet(o->dir, same, o->more); o->dir = nullptr; }
    else pvPaneClear();
    pvSetResult(o);
    PV.stale = false;
    KillTimer(A.hwnd, 12);
    static const wchar_t* kinds[] = { L"", L"testo", L"immagine", L"cartella", L"informazioni" };
    A.prevKind = kinds[o->kind];
    A.prevReadMs = o->readMs;
    A.prevDecodeMs = o->decodeMs;
    if (o->gen == PV.refreshGen) { // una rilettura, con selezione e scorrimento rimessi (pvPaneSet)
        A.prevRefreshCount++;
        A.prevRefreshLatency = nowMs() - A.prevRefreshStart;
    }
    A.dirty = true;
    pvPump();
    PV.refreshGen = 0;
    if (PV.refreshAgain) { PV.refreshAgain = false; previewRefresh(); }
}

static void previewToggle() {
    A.prevOpen = !A.prevOpen;
    if (!A.prevOpen) { // niente resta in memoria; riaprendo si rilegge
        pvClear();
        PV.keyList = nullptr;
        PV.keyEntry = -2;
    }
    settingsSave();
    layout();
    if (A.prevOpen && !A.prevW) notice(previewNoRoom(), true);
    A.hover = -1;
    A.dirty = true;
}

// A destra o in basso: ognuna con la sua misura. Il contenuto resta; un'immagine si rifà alla
// misura nuova (previewSync), la lista di una cartella si dispone di nuovo al frame
static void previewPlace(bool bottom) {
    if (A.prevBottom == bottom) return;
    A.prevBottom = bottom;
    settingsSave();
    layout();
    if (A.prevOpen && !A.prevW) notice(previewNoRoom(), true);
    A.hover = -1;
    A.dirty = true;
}

// --- testo che va a capo --------------------------------------------------------------------
static int pvLineCount() { return (int)PV.res->lines.size(); }
static int pvLineStart(int L) { return (int)PV.res->lines[(size_t)L]; }
static int pvLineEnd(int L) { return L + 1 < pvLineCount() ? pvLineStart(L + 1) : (int)PV.res->text.size(); }

// Fine della riga visiva che comincia in s (la riga logica finisce in e): a capo dopo l'ultimo
// spazio che ci sta, altrimenti dove finisce lo spazio
static int pvWrap(int s, int e, float width) {
    const wchar_t* t = PV.res->text.c_str();
    float w = 0;
    int lastSpace = -1;
    for (int i = s; i < e;) {
        int j = i;
        uint32_t cp = decodeUtf16(t, e, j);
        float a = (float)monoGlyph(cp).adv;
        if (w + a > width && i > s) {
            if (lastSpace <= s) return i;
            // Una parola più lunga di una riga intera (file minimizzati) andrebbe a capo da sola
            // lasciando una riga quasi vuota: la si spezza qui. Basta misurarla fino a width.
            float ww = 0;
            for (int k = lastSpace; k < e && t[k] != L' ' && ww <= width;) ww += (float)monoGlyph(decodeUtf16(t, e, k)).adv;
            return ww > width ? i : lastSpace;
        }
        w += a;
        if (cp == L' ') lastSpace = j;
        i = j;
    }
    return e;
}

static int pvRows(int L) { // righe visive della riga logica L (almeno una, anche vuota)
    int s = pvLineStart(L), e = pvLineEnd(L), n = 0;
    do { s = pvWrap(s, e, PV.wrapW); n++; } while (s < e);
    return n;
}
static bool pvStepDown(int& L, int& sub) {
    if (sub + 1 < pvRows(L)) { sub++; return true; }
    if (L + 1 < pvLineCount()) { L++; sub = 0; return true; }
    return false;
}
static bool pvStepUp(int& L, int& sub) {
    if (sub > 0) { sub--; return true; }
    if (L > 0) { L--; sub = pvRows(L) - 1; return true; }
    return false;
}

// Sposta l'inizio del testo di delta righe visive; l'ultima riga non sale oltre il fondo
static void pvTextScroll(int delta) {
    int L = PV.top, sub = PV.topSub;
    for (int i = 0; i < abs(delta); i++)
        if (!(delta > 0 ? pvStepDown(L, sub) : pvStepUp(L, sub))) break;
    int mL = pvLineCount() - 1, mS = pvRows(mL) - 1;
    for (int k = 1; k < PV.visRows && pvStepUp(mL, mS); k++) {}
    if (L > mL || (L == mL && sub > mS)) { L = mL; sub = mS; }
    PV.top = L;
    PV.topSub = sub;
}

// --- input ----------------------------------------------------------------------------------
static bool previewWheel(int x, int y, float lines) {
    if (!inPreview(x, y)) return false;
    if (!PV.res) return true;
    if (ListPane* p = PV.paneShown ? pvPane() : nullptr) { // una cartella: scorre come la lista
        PaneScope ps(p);
        listWheel(lines);
        return true;
    }
    PV.wheelAcc -= lines * ui::WHEEL_LINES; // righe a scatto come la lista; i touchpad danno frazioni
    int d = (int)PV.wheelAcc;
    PV.wheelAcc -= (float)d;
    if (!d) return true;
    if (PV.res->kind == PV_TEXT && pvLineCount() > 0 && PV.wrapW > 0) pvTextScroll(d);
    A.dirty = true;
    return true;
}

// Corpo dell'anteprima di una cartella sotto il punto (fuori da un PaneScope): la sua lista, per i
// PaneScope del mouse; nullptr altrove
static ListPane* pvPaneAt(int x, int y) {
    ListPane* p = PV.paneShown ? pvPane() : nullptr;
    if (!p || x < p->listX || x >= p->listR || y < p->listTop || y >= p->listTop + p->listH) return nullptr;
    return p;
}
// Controllo della vista nella banda dell'anteprima (solo per una cartella)
static bool pvViewAt(int x, int y) {
    return PV.viewW > 0 && inPreview(x, y) && y >= G.h - A.statusH && x >= PV.viewX && x < PV.viewX + PV.viewW;
}

// --- disegno --------------------------------------------------------------------------------
// Banda in fondo, con le parti della barra di stato della lista (render.cpp): a destra il tipo
// di contenuto, disegnato come la vista della lista; per una cartella è la sua vista e si sceglie
// come quella (choose: fondo sotto il mouse e col menu aperto), per gli altri contenuti dipende
// dalla voce e non ha fondo. Prima i dati, se ci sono (data). Restituisce il bordo destro libero.
static float pvBar(const PvGeom& g, uint32_t icon, const wchar_t* name, bool data, bool choose = false) {
    float w = barViewW(name), right = g.x1 + px(ui::GAP_S), ha = 0;
    if (choose) {
        PV.viewX = right - w;
        PV.viewW = w;
        ha = A.menu.open && A.menu.kind == MENU_VIEW && A.menu.preview ? 1.0f : hoverAlpha(PV.viewFade, 0);
    }
    return drawBarView(right, g.footY, w, icon, name, ha, data);
}

// Geometria della lista dell'anteprima: dal bordo con la lista principale al bordo della finestra,
// sotto l'intestazione (a destra le righe sono allineate a quelle della lista), fino alla banda in
// fondo. A destra, stretto, in Dettagli ha solo Dimensione; in basso è largo come la lista e ha le
// colonne scelte (colWanted), tranne Destinazione: i collegamenti dell'anteprima non si leggono
static void pvPaneLayout() {
    PvGeom g = pvGeom();
    float x0 = g.x0 - A.padX, x1 = g.x1 + A.padX, top = g.top + A.headH, bottom = g.footY;
    PaneScope ps(&PV.pane);
    LayoutKeep k = layoutKeep();
    A.listX = x0;
    A.listR = x1;
    A.listTop = top;
    A.listH = std::max(0.0f, bottom - top);
    fltLayout(); // nella banda in fondo, come nella barra di stato della lista
    layoutItems(!A.prevBottom, k);
    if (PV.keepFirst >= 0) { scrollToItem(PV.keepFirst); PV.keepFirst = -1; }
    clampScroll();
}

// Dati prima del tipo di contenuto, in C_DIM come i contatori, allineati a destra; se non ci stanno
// finiscono in "…"
static void pvBarData(const PvGeom& g, float right, const std::wstring& t) {
    float w = (float)textWidth(t.c_str(), (int)t.size());
    drawText(std::max(g.x0, right - w), g.footY + floorf((A.statusH - F.lineH) * 0.5f), t, C_DIM, right);
}

static void pvNote(const PvGeom& g, const wchar_t* t) {
    drawText(g.x0, g.bodyTop, t, (int)wcslen(t), C_DIM, g.x1);
}

static void drawPvText(const PvGeom& g) {
    const PvRes& o = *PV.res;
    monoEnsure();
    float lh = (float)FM.lineH + px(ui::LINE_GAP);
    int rows = std::max(1, (int)((g.bodyBot - g.bodyTop) / lh));
    PV.visRows = rows;
    PV.wrapW = g.x1 - g.x0;
    std::wstring foot = o.enc;
    wchar_t b[64], lim[32];
    fmtSize(o.fileSize, b);
    fmtSize(kPvTextMax, lim);
    if (o.truncated) foot += L" \x00B7 primi " + std::wstring(lim) + L" di " + b;
    else if (!o.text.empty()) foot += L" \x00B7 " + withThousands((uint64_t)o.lineCount) + (o.lineCount == 1 ? L" riga" : L" righe") + L" \x00B7 " + b;
    pvBarData(g, pvBar(g, IC_TEXT, L"Testo", true), foot);
    if (o.text.empty() && pvLineCount() <= 1) { pvNote(g, L"File vuoto"); return; }
    int n = pvLineCount();
    PV.top = std::max(0, std::min(PV.top, n - 1));
    PV.topSub = std::max(0, std::min(PV.topSub, pvRows(PV.top) - 1)); // la larghezza può essere cambiata
    int L = PV.top, e = pvLineEnd(L), s = pvLineStart(L);
    for (int k = 0; k < PV.topSub; k++) s = pvWrap(s, e, PV.wrapW);
    float y = g.bodyTop;
    int whole = 0; // righe logiche finite nella vista (per la barra di scorrimento)
    for (int r = 0; r < rows && L < n; r++) {
        int q = pvWrap(s, e, PV.wrapW);
        drawMono(g.x0, y, o.text.c_str() + s, q - s, C_TEXT, g.x1);
        y += lh;
        if (q >= e) {
            whole++;
            if (++L < n) { s = pvLineStart(L); e = pvLineEnd(L); }
        } else {
            s = q;
        }
    }
    drawPanelScroll(A.prevX + A.prevW, g.bodyTop, rows * lh, PV.top, std::max(1, whole), n, 1);
}

static void drawPvImage(const PvGeom& g) {
    const PvRes& o = *PV.res;
    float bw = g.x1 - g.x0, bh = g.bodyBot - g.bodyTop;
    // Il pannello si è appena stretto: l'immagine si riduce in attesa di quella rifatta alla misura nuova
    float k = std::min(1.0f, std::min(bw / o.w, bh / o.h));
    float w = floorf(o.w * k), h = floorf(o.h * k);
    float x = floorf(g.x0 + (bw - w) * 0.5f), y = floorf(g.bodyTop + (bh - h) * 0.5f);
    if (G.prevSrv) quad(x, y, x + w, y + h, 2.0f, 0.0f, 3.0f, 1.0f, C_ICON); // u + 2: texture dell'anteprima
    wchar_t b[32];
    fmtSize(PV.shown.size, b);
    std::wstring foot = std::to_wstring(o.imgW) + L" \x00D7 " + std::to_wstring(o.imgH) + L" \x00B7 " + b;
    if (o.format[0]) foot += L" \x00B7 " + std::wstring(o.format);
    pvBarData(g, pvBar(g, IC_IMAGE, L"Immagine", true), foot);
}

static void drawPreviewBody(double dt);

// Una cartella: la stessa lista della finestra (drawListItems nel PaneScope dell'anteprima), nella
// vista del pannello; in Dettagli icona, nome e dimensione. Ritagliata al corpo del pannello: le
// voci che scorrono non sconfinano nell'intestazione, nella banda o (in Elenco) nella lista.
static void drawPvDir(const PvGeom& g, double dt, double now) {
    const PvRes& o = *PV.res;
    if (o.err) pvNote(g, errText(o.err));
    else if (PV.pane.cur) {
        pvPaneLayout();
        const ListPane& p = PV.pane;
        double t0 = nowMs();
        clipBegin(p.listX, p.listTop, p.listR, p.listTop + p.listH);
        {
            PaneScope ps(&PV.pane);
            A.scrollY = approach(A.scrollY, A.scrollTarget, dt, ui::TAU_SCROLL, 0.5f); // rotellina: come la lista
            clampScroll();
            drawListItems(dt, now, true, PV.keys);
        }
        clipEnd();
        PV.paneShown = true;
        A.prevPaneMs = nowMs() - t0; // F12: geometria, scambi, ritaglio, voci e miniature dell'anteprima
        A.prevPaneMax = std::max(A.prevPaneMax, A.prevPaneMs);
    }
    // Prima della vista i contatori, come la lista (il totale della cartella no: sta nella colonna
    // Dimensione e nel tooltip): con un filtro, quelli delle voci che si vedono. A sinistra il filtro
    int z = A.prevZoom;
    float right = pvBar(g, viewIcon(z), viewName(z), !o.err, !o.err);
    if (o.err || !PV.pane.cur) return;
    PaneScope ps(&PV.pane);
    int dirs = o.info.dirs, files = o.info.files; // senza filtro quelli della lettura (di uno zip, tutti)
    if (!A.filter.empty()) { dirs = viewDirs(); files = (int)A.view.size() - dirs; }
    right = drawBarCounts(right, g.footY, dirs, files, o.more);
    drawBarFilter(right, g.footY, dt, o.more);
}

static void drawPvInfo(const PvGeom& g) {
    const PvRes& o = *PV.res;
    pvBar(g, IC_INFO, L"Informazioni", false); // i dati sono già nelle righe
    float y = g.bodyTop + px(ui::GAP_L);
    if (G.prevSrv && o.w > 0) {
        float x = floorf(g.x0 + (g.x1 - g.x0 - o.w) * 0.5f);
        quad(x, y, x + o.w, y + o.h, 2.0f, 0.0f, 3.0f, 1.0f, C_ICON);
        y += o.h + px(ui::GAP_XL);
    }
    InfoRows rows;
    if (PV.shown.drive) {
        if (const SideItem* d = driveInfo(PV.shown.path)) driveRows(rows, *d);
    } else {
        fileRows(rows, PV.shown.path, PV.shown.attr, PV.shown.size, PV.shown.mtime, o.info);
        if (o.tooBig) infoRow(rows, L"", L"Immagine troppo grande per l'anteprima");
        if (onlineOnly(PV.shown.attr)) infoRow(rows, L"", L"Solo online: si legge solo aprendolo");
        if (o.err == ERROR_SHARING_VIOLATION || o.err == ERROR_LOCK_VIOLATION) infoRow(rows, L"", L"In uso da un altro programma");
        else if (o.err == ERROR_ACCESS_DENIED) infoRow(rows, L"", L"Accesso negato");
        else if (o.err) infoRow(rows, L"", L"Impossibile leggere il file (codice " + std::to_wstring(o.err) + L")");
    }
    // Come nel tooltip: etichette e note in C_DIM, valori in C_TEXT
    float lh = (float)F.lineH + px(ui::LINE_GAP), labW = 0;
    for (auto& r : rows) labW = std::max(labW, (float)textWidth(r.first.c_str(), (int)r.first.size()));
    float vx = g.x0 + labW + px(ui::TIP_COL_GAP);
    for (auto& r : rows) {
        if (y + F.lineH > g.bodyBot) break;
        if (r.first.empty()) drawText(g.x0, y, r.second, C_DIM, g.x1);
        else { drawText(g.x0, y, r.first, C_DIM); drawText(vx, y, r.second, C_TEXT, g.x1); }
        y += lh;
    }
}

static void drawPreview(double dt) {
    PV.paneShown = false;
    PV.viewW = 0;
    drawPreviewBody(dt);
    if (!PV.paneShown) thumbsPreviewIdle(); // in Icone la sua lista chiede miniature solo quando si vede
}

static void drawPreviewBody(double dt) {
    if (A.prevW <= 0) return;
    hoverFadeUpdate(PV.viewFade, PV.viewHover ? 0 : -1, dt);
    float x = A.prevX, W = A.prevX + A.prevW;
    PvGeom g = pvGeom();
    // Bordo con la lista (C_SEL_EDGE mentre lo si trascina): a sinistra per tutta l'altezza, o sopra.
    // In basso il fondo copre le voci della lista che scorrono oltre la barra di stato
    uint32_t edge = A.colDrag == EDGE_PREVIEW ? C_SEL_EDGE : C_LINE;
    float bx = x;
    if (A.prevBottom) {
        rect(x, A.prevY, A.prevW, A.prevH, C_BG);
        rect(x, A.prevY, A.prevW, 1, edge);
    } else {
        rect(x, g.top, 1, g.bot - g.top, edge);
        bx = x + 1;
    }
    // Intestazione: a destra allineata a quella delle colonne
    rect(bx, g.top, W - bx, A.headH, C_HEADER);
    rect(bx, g.top + A.headH - 1, W - bx, 1, C_LINE);
    drawBarBand(bx, W, g.footY, A.statusH); // sempre, come la barra di stato; il contenuto la riempie
    if (!PV.want) return;
    // Il contenuto precedente resta finché arriva il nuovo; dopo LOADING_AFTER si vedono il nome
    // e "caricamento…". Senza contenuto precedente il nome si vede subito.
    bool late = PV.stale && nowMs() - PV.staleSince >= ui::LOADING_AFTER;
    bool showRes = PV.res && !late;
    const PvItem& it = showRes ? PV.shown : PV.item;
    std::wstring name = it.name;
    if (it.drive) {
        if (const SideItem* d = driveInfo(it.path)) name = d->label;
    }
    drawText(g.x0, g.top + floorf((A.headH - F.lineH) * 0.5f), name, C_TEXT, g.x1);
    if (!showRes) {
        if (late) pvNote(g, L"caricamento\x2026");
        return;
    }
    switch (PV.res->kind) {
    case PV_TEXT:  drawPvText(g); break;
    case PV_IMAGE: drawPvImage(g); break;
    case PV_DIR:   drawPvDir(g, dt, nowMs()); break;
    case PV_INFO:  drawPvInfo(g); break;
    }
}
