// [z]f — testo: glifi rasterizzati con GDI al primo uso in un atlante RGBA, disegno del testo,
// casella di testo riutilizzabile (percorso, rinomina) e appunti in formato testo.
//
// Con ClearType attivo in Windows i glifi si rasterizzano in ClearType: ogni pixel ha tre
// coperture, una per subpixel, e lo shader le usa con il dual-source blending. È lo stesso
// risultato del testo delle app di Windows, più nitido della scala di grigi su sfondo scuro.
// Senza ClearType le tre coperture sono uguali (scala di grigi) e il percorso è lo stesso.

struct Glyph { uint16_t x, y, w, h; int16_t ox, adv; };

struct GlyphCache {
    HDC      dc = nullptr;
    HBITMAP  bmp = nullptr;
    HFONT    font = nullptr;
    uint32_t* bits = nullptr;
    int      bw = 0, bh = 0;
    int      lineH = 0;
    Glyph    ascii[128];
    bool     has[128];
    std::unordered_map<uint32_t, Glyph> map;
    int      penX = 8, penY = 0, shelfH = 0;
    std::vector<uint32_t> tmp;
    int      px = ui::FONT_PX;      // dimensione a scala 1 (impostazione "dimensione")
    std::wstring face = L"Segoe UI"; // impostazione "carattere"
    bool     clearType = true;
    float    scale = 1;        // scala DPI dell'ultimo fontCreate
    int      rasterCount = 0;  // statistiche
    double   rasterMs = 0;
} F;

// Icone dell'interfaccia: il font di icone di Windows (Segoe Fluent Icons su Windows 11, Segoe MDL2
// Assets su Windows 10), rasterizzato nello stesso atlante del testo, in scala di grigi (il
// ClearType sulle icone farebbe aloni colorati). Segue la dimensione del testo.
struct IconFont {
    HFONT font = nullptr;
    int lineH = 0, px = 0;
    bool ok = false;
    std::unordered_map<uint32_t, Glyph> map;
} FI;

// Carattere a spaziatura fissa del testo nell'anteprima (codice, log): l'unica eccezione al
// carattere unico dell'interfaccia (DESIGN.md, "Testo"). Si crea al primo uso, non all'avvio, e
// segue la dimensione del testo. Lo si sceglie nelle opzioni (carattere_anteprima).
struct MonoFont {
    HFONT font = nullptr;
    int lineH = 0;
    std::wstring face = L"Cascadia Mono";
    Glyph ascii[128];
    bool has[128];
    std::unordered_map<uint32_t, Glyph> map;
} FM;

// Codici delle icone (uguali nei due font)
enum : uint32_t {
    IC_SETTINGS = 0xE713, IC_BACK = 0xE72B, IC_FORWARD = 0xE72A, IC_UP = 0xE74A, IC_SIDEBAR = 0xE90C, IC_PREVIEW = 0xE90D,
    IC_PREVIEW_BOTTOM = 0xE90E, // DockBottom: l'anteprima in basso
    IC_STAR = 0xE734, IC_STAR_FILL = 0xE735, IC_ADD = 0xE710, IC_CLOSE = 0xE711,
    IC_FILTER = 0xE71C, IC_CHEVRON_DOWN = 0xE70D, IC_CHEVRON_RIGHT = 0xE76C, IC_DELETE = 0xE74D,
    IC_FOLDER = 0xE8B7, IC_PAGE = 0xE7C3, IC_GRID = 0xE80A, IC_DETAILS = 0xE8FD, IC_LIST = 0xE14C, // viste: Icone, Dettagli, Elenco
    IC_CHECK = 0xE73E, // voce scelta in un menu (la vista)
    IC_TEXT = 0xE8A5, IC_IMAGE = 0xE91B, IC_INFO = 0xE946, // contenuto dell'anteprima: testo, immagine, informazioni
    IC_MIN = 0xE921, IC_MAX = 0xE922, IC_RESTORE = 0xE923, IC_WCLOSE = 0xE8BB,
    IC_SYNC = 0xE895, IC_DOWNLOAD = 0xE896, IC_DONE = 0xEC61, // aggiornamento: controlla, scarica, aggiornato
};

static void glyphReset() {
    flushQuads(G.mapped != nullptr); // ciò che è già in coda usa l'atlante vecchio
    F.map.clear();
    FI.map.clear();
    FM.map.clear();
    memset(F.has, 0, sizeof(F.has));
    memset(FM.has, 0, sizeof(FM.has));
    F.penX = 8; F.penY = 0; F.shelfH = 0;
}

// Bitmap su cui GDI rasterizza i glifi: alta quanto la riga più alta dei caratteri in uso
static void glyphDib() {
    F.bh = std::max(F.lineH, std::max(FI.lineH, FM.lineH));
    F.bw = F.bh * 4;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = F.bw;
    bi.bmiHeader.biHeight = -F.bh; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HBITMAP old = F.bmp;
    F.bmp = CreateDIBSection(F.dc, &bi, DIB_RGB_COLORS, (void**)&F.bits, nullptr, 0);
    SelectObject(F.dc, F.bmp);
    if (old) DeleteObject(old); // dopo averla tolta dal DC: selezionata non si cancellerebbe
    F.tmp.resize((size_t)F.bw * F.bh);
}

// Il carattere dell'anteprima si ricrea al prossimo uso (dimensione o scelta cambiata)
static void monoReset() {
    if (FM.font) { DeleteObject(FM.font); FM.font = nullptr; }
    FM.lineH = 0;
    FM.map.clear();
    memset(FM.has, 0, sizeof(FM.has));
}

static DWORD fontQuality() { return F.clearType ? CLEARTYPE_QUALITY : ANTIALIASED_QUALITY; }

static void fontCreate(float scale) {
    if (!F.dc) F.dc = CreateCompatibleDC(nullptr);
    if (F.font) { SelectObject(F.dc, GetStockObject(SYSTEM_FONT)); DeleteObject(F.font); F.font = nullptr; }
    monoReset();
    F.scale = scale;
    int px = (int)lroundf(F.px * scale);
    // ClearType solo se Windows lo usa (Impostazioni → Regola testo ClearType)
    BOOL smooth = FALSE;
    UINT type = 0;
    SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &smooth, 0);
    SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &type, 0);
    F.clearType = smooth && type == FE_FONTSMOOTHINGCLEARTYPE;
    DWORD quality = fontQuality();
    F.font = CreateFontW(-px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, quality, DEFAULT_PITCH, F.face.c_str());
    SelectObject(F.dc, F.font);
    wchar_t got[LF_FACESIZE] = {};
    GetTextFaceW(F.dc, LF_FACESIZE, got);
    if (_wcsicmp(got, F.face.c_str()) != 0 && _wcsicmp(F.face.c_str(), L"Segoe UI") != 0) { // carattere non installato
        SelectObject(F.dc, GetStockObject(SYSTEM_FONT)); // selezionato non si cancellerebbe
        DeleteObject(F.font);
        F.face = L"Segoe UI";
        F.font = CreateFontW(-px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, quality, DEFAULT_PITCH, F.face.c_str());
        SelectObject(F.dc, F.font);
    }
    TEXTMETRICW tm;
    GetTextMetricsW(F.dc, &tm);
    F.lineH = tm.tmHeight;
    if (FI.font) { DeleteObject(FI.font); FI.font = nullptr; }
    FI.px = (int)lroundf(F.px * ui::ICON_FONT * scale); // un po' più grande del testo
    FI.ok = false;
    for (const wchar_t* face : { L"Segoe Fluent Icons", L"Segoe MDL2 Assets" }) {
        FI.font = CreateFontW(-FI.px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
        SelectObject(F.dc, FI.font);
        wchar_t g2[LF_FACESIZE] = {};
        GetTextFaceW(F.dc, LF_FACESIZE, g2);
        if (!_wcsicmp(g2, face)) { FI.ok = true; break; }
        SelectObject(F.dc, F.font);
        DeleteObject(FI.font);
        FI.font = nullptr;
    }
    if (FI.font) { TEXTMETRICW ti; GetTextMetricsW(F.dc, &ti); FI.lineH = ti.tmHeight; }
    SelectObject(F.dc, F.font);
    glyphDib();
    SetTextColor(F.dc, RGB(255, 255, 255));
    SetBkColor(F.dc, RGB(0, 0, 0));
    SetBkMode(F.dc, OPAQUE);
    glyphReset();
}

// Crea il carattere dell'anteprima: quello scelto, altrimenti Cascadia Mono, altrimenti Consolas
static void monoEnsure() {
    if (FM.font) return;
    int px = (int)lroundf(F.px * F.scale);
    for (const wchar_t* face : { FM.face.c_str(), L"Cascadia Mono", L"Consolas" }) {
        FM.font = CreateFontW(-px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, fontQuality(), DEFAULT_PITCH, face);
        SelectObject(F.dc, FM.font);
        wchar_t got[LF_FACESIZE] = {};
        GetTextFaceW(F.dc, LF_FACESIZE, got);
        if (!_wcsicmp(got, face) || !wcscmp(face, L"Consolas")) break; // Consolas c'è da Vista in poi
        SelectObject(F.dc, F.font);
        DeleteObject(FM.font);
        FM.font = nullptr;
    }
    TEXTMETRICW tm;
    GetTextMetricsW(F.dc, &tm);
    FM.lineH = tm.tmHeight;
    SelectObject(F.dc, F.font);
    if (FM.lineH > F.bh) glyphDib(); // riga più alta di quella dell'interfaccia: bitmap più grande
}

static Glyph glyphRasterImpl(uint32_t cp);
static Glyph glyphRaster(uint32_t cp, HFONT font = nullptr) {
    double t0 = nowMs();
    if (font) SelectObject(F.dc, font);
    Glyph g = glyphRasterImpl(cp);
    if (font) SelectObject(F.dc, F.font);
    F.rasterMs += nowMs() - t0;
    F.rasterCount++;
    return g;
}

static Glyph glyphRasterImpl(uint32_t cp) {
    wchar_t s[2];
    int n = 1;
    if (cp >= 0x10000) {
        uint32_t v = cp - 0x10000;
        s[0] = (wchar_t)(0xD800 + (v >> 10));
        s[1] = (wchar_t)(0xDC00 + (v & 0x3FF));
        n = 2;
    } else {
        s[0] = (wchar_t)cp;
    }
    SIZE sz = { 0, 0 };
    GetTextExtentPoint32W(F.dc, s, n, &sz);
    int pad = F.lineH / 4 + 1;
    int w = std::min((int)sz.cx + pad * 2, F.bw);
    int h = F.bh;
    RECT rc = { 0, 0, w, h };
    ExtTextOutW(F.dc, pad, 0, ETO_OPAQUE | ETO_CLIPPED, &rc, s, (UINT)n, nullptr);
    GdiFlush();

    int minx = w, maxx = -1;
    for (int y = 0; y < h; y++) {
        const uint32_t* row = F.bits + (size_t)y * F.bw;
        for (int x = 0; x < w; x++)
            if (row[x] & 0xFFFFFF) { if (x < minx) minx = x; if (x > maxx) maxx = x; }
    }
    Glyph g = {};
    g.adv = (int16_t)sz.cx;
    if (maxx < 0) return g; // spazio o glifo vuoto

    int gw = maxx - minx + 1;
    if (F.penX + gw + 1 > ATLAS) { F.penX = 0; F.penY += F.shelfH + 1; F.shelfH = 0; }
    if (F.penY + h > ATLAS) glyphReset(); // atlante pieno: si riparte da zero

    // DIB: 0x00RRGGBB (copertura per subpixel, bianco su nero) → texture RGBA, alfa = massimo
    for (int y = 0; y < h; y++) {
        const uint32_t* row = F.bits + (size_t)y * F.bw + minx;
        uint32_t* dst = &F.tmp[(size_t)y * gw];
        for (int x = 0; x < gw; x++) {
            uint32_t p = row[x], r = (p >> 16) & 0xFF, gg = (p >> 8) & 0xFF, b = p & 0xFF;
            uint32_t m = std::max(r, std::max(gg, b));
            dst[x] = r | (gg << 8) | (b << 16) | (m << 24);
        }
    }
    D3D11_BOX box = { (UINT)F.penX, (UINT)F.penY, 0, (UINT)(F.penX + gw), (UINT)(F.penY + h), 1 };
    G.ctx->UpdateSubresource(G.atlas, 0, &box, F.tmp.data(), (UINT)gw * 4, 0);

    g.x = (uint16_t)F.penX; g.y = (uint16_t)F.penY;
    g.w = (uint16_t)gw;     g.h = (uint16_t)h;
    g.ox = (int16_t)(minx - pad);
    F.penX += gw + 1;
    if (h > F.shelfH) F.shelfH = h;
    return g;
}

static inline Glyph glyphGet(uint32_t cp) {
    if (cp < 128) {
        if (!F.has[cp]) { F.ascii[cp] = glyphRaster(cp); F.has[cp] = true; }
        return F.ascii[cp];
    }
    auto it = F.map.find(cp);
    if (it != F.map.end()) return it->second;
    Glyph g = glyphRaster(cp);
    F.map[cp] = g;
    return g;
}

static Glyph iconGlyph(uint32_t cp) {
    auto it = FI.map.find(cp);
    if (it != FI.map.end()) return it->second;
    Glyph g = FI.font ? glyphRaster(cp, FI.font) : Glyph{};
    FI.map[cp] = g;
    return g;
}

static Glyph monoGlyph(uint32_t cp) {
    monoEnsure();
    if (cp < 128) {
        if (!FM.has[cp]) { FM.ascii[cp] = glyphRaster(cp, FM.font); FM.has[cp] = true; }
        return FM.ascii[cp];
    }
    auto it = FM.map.find(cp);
    if (it != FM.map.end()) return it->second;
    Glyph g = glyphRaster(cp, FM.font);
    FM.map[cp] = g;
    return g;
}

static void drawGlyph(float x, float y, const Glyph& g, uint32_t col);

// Icona centrata nel rettangolo (x, y, w, h). Senza font di icone non disegna niente.
static void drawUiIcon(float x, float y, float w, float h, uint32_t cp, uint32_t col) {
    if (!FI.ok) return;
    Glyph g = iconGlyph(cp);
    drawGlyph(floorf(x + (w - g.adv) * 0.5f), floorf(y + (h - FI.lineH) * 0.5f), g, col);
}

static inline uint32_t decodeUtf16(const wchar_t* s, int n, int& i) {
    uint32_t c = (uint16_t)s[i++];
    if (c >= 0xD800 && c <= 0xDBFF && i < n) {
        uint32_t d = (uint16_t)s[i];
        if (d >= 0xDC00 && d <= 0xDFFF) { i++; c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00); }
    }
    return c;
}

static int textWidth(const wchar_t* s, int n) {
    int w = 0;
    for (int i = 0; i < n;) w += glyphGet(decodeUtf16(s, n, i)).adv;
    return w;
}

static void drawGlyph(float x, float y, const Glyph& g, uint32_t col) {
    if (!g.w) return;
    const float k = 1.0f / ATLAS;
    float x0 = x + g.ox;
    quad(x0, y, x0 + g.w, y + g.h, g.x * k, g.y * k, (g.x + g.w) * k, (g.y + g.h) * k, col);
}

// Disegna testo con troncamento "…" se supera maxX. Ritorna la x finale.
// marks (facoltativo, uno per carattere UTF-16): i caratteri segnati vanno in markCol (lettere
// trovate da una ricerca).
static float drawTextMarked(float x, float y, const wchar_t* s, int n, uint32_t col, float maxX,
                            const uint8_t* marks, uint32_t markCol) {
    float px = floorf(x), py = floorf(y);
    bool ell = px + textWidth(s, n) > maxX;
    Glyph ge = {};
    float limit = maxX;
    if (ell) { ge = glyphGet(0x2026); limit = maxX - ge.adv; }
    for (int i = 0; i < n;) {
        int at = i;
        Glyph g = glyphGet(decodeUtf16(s, n, i));
        if (ell && px + g.adv > limit) break;
        drawGlyph(px, py, g, marks && marks[at] ? markCol : col);
        px += g.adv;
    }
    if (ell) { drawGlyph(px, py, ge, col); px += ge.adv; }
    return px;
}

static float drawText(float x, float y, const wchar_t* s, int n, uint32_t col, float maxX = 1e9f) {
    return drawTextMarked(x, y, s, n, col, maxX, nullptr, 0);
}

static float drawText(float x, float y, const std::wstring& s, uint32_t col, float maxX = 1e9f) {
    return drawText(x, y, s.c_str(), (int)s.size(), col, maxX);
}

// Testo col carattere dell'anteprima, fino a maxX (le righe arrivano già spezzate: niente "…")
static void drawMono(float x, float y, const wchar_t* s, int n, uint32_t col, float maxX) {
    float gx = floorf(x), gy = floorf(y);
    for (int i = 0; i < n;) {
        Glyph g = monoGlyph(decodeUtf16(s, n, i));
        if (gx + g.adv > maxX + 0.5f) break;
        drawGlyph(gx, gy, g, col);
        gx += g.adv;
    }
}

// Disegna solo i glifi che stanno interi tra clipL e clipR (testo che scorre in una casella)
static void drawTextClipped(float x, float y, const wchar_t* s, int n, uint32_t col, float clipL, float clipR) {
    float px = floorf(x), py = floorf(y);
    for (int i = 0; i < n;) {
        Glyph g = glyphGet(decodeUtf16(s, n, i));
        if (px >= clipL - 0.5f && px + g.adv <= clipR + 0.5f) drawGlyph(px, py, g, col);
        px += g.adv;
        if (px > clipR) break;
    }
}

// ============================================================================
// Casella di testo: cursore, selezione, Ctrl+frecce, appunti.
// Una sola implementazione per la barra del percorso e per la rinomina.
// ============================================================================

// Apre gli appunti riprovando: un altro programma può tenerli aperti per un attimo (rdpclip mentre
// li passa al PC remoto, un gestore degli appunti). Aspetta: solo sui thread degli appunti.
static bool clipOpen(HWND owner) {
    double t0 = nowMs(); // il tempo vero: Sleep(50) dura 62 ms con il timer di sistema a 15,6
    for (;;) {
        if (OpenClipboard(owner)) return true;
        if (nowMs() - t0 >= ui::CLIP_WAIT_MAX) return false;
        Sleep(ui::CLIP_RETRY);
    }
}

static std::wstring clipGetText(HWND h) {
    std::wstring t;
    if (!clipOpen(h)) return t;
    HANDLE d = GetClipboardData(CF_UNICODETEXT);
    if (d) {
        const wchar_t* p = (const wchar_t*)GlobalLock(d);
        if (p) { t = p; GlobalUnlock(d); }
    }
    CloseClipboard();
    size_t nl = t.find_first_of(L"\r\n"); // solo la prima riga
    if (nl != std::wstring::npos) t.resize(nl);
    return t;
}

// Ctrl+V legge il testo su un thread: GetClipboardData, se chi possiede gli appunti li prepara solo
// quando servono (rendering ritardato: Office, per esempio), aspetta un altro processo senza limite.
// Il testo arriva alla finestra qualche millisecondo dopo (WM_APP_CLIPTEXT, onClipText in input.cpp).
struct ClipText { HWND hwnd; uint64_t gen; void* target; std::wstring text; };
static uint64_t g_clipGen = 0; // vale solo l'ultima richiesta

static DWORD WINAPI clipTextThread(void* p) {
    ClipText* c = (ClipText*)p;
    c->text = clipGetText(nullptr);
    if (!PostMessageW(c->hwnd, WM_APP_CLIPTEXT, 0, (LPARAM)c)) delete c;
    return 0;
}

// target: la casella che riceverà il testo; nullptr: Ctrl+V nella lista (smartPaste)
static void clipAskText(HWND h, void* target) {
    ClipText* c = new ClipText{ h, ++g_clipGen, target, L"" };
    HANDLE t = CreateThread(nullptr, 0, clipTextThread, c, 0, nullptr);
    if (t) CloseHandle(t); else delete c;
}

// Anche scrivere il testo (Copia e Taglia nelle caselle, "Copia percorso") lo fa un thread:
// OpenClipboard fallisce finché un altro programma tiene aperti gli appunti, ed EmptyClipboard
// manda WM_DESTROYCLIPBOARD a chi li possedeva, un altro processo che può essere fermo. L'esito
// torna alla finestra (WM_APP_CLIPSET, onClipSet in input.cpp), che solo allora dice "copiato", o
// perché non lo è. Il testo arriva negli appunti pochi millisecondi dopo il tasto: un Ctrl+V così
// vicino non lo fa una persona.
struct ClipSet {
    HWND hwnd;
    std::wstring text;
    const wchar_t* done;  // avviso se riesce (nullptr: nessuno, come Ctrl+C nelle caselle)
    const wchar_t* fail;  // inizio dell'avviso se non riesce: "Impossibile copiare il percorso"
    void* target = nullptr; // Taglia: la casella, dove stava il testo e com'era la casella dopo,
    int at = 0;             // per rimetterlo se gli appunti non lo prendono
    std::wstring after;
    LONG gen = 0;
    bool ok = false, skipped = false;
    DWORD err = 0;
    std::wstring holder; // il programma che teneva aperti gli appunti ("rdpclip.exe"), se si sa
};
static volatile LONG g_clipSetGen = 0; // l'ultima scrittura chiesta: le precedenti non scrivono più

// Il programma che tiene aperti gli appunti, per l'avviso; vuoto se non si sa (aperti senza finestra)
static std::wstring clipHolder() {
    DWORD pid = 0;
    HWND w = GetOpenClipboardWindow();
    if (!w || !GetWindowThreadProcessId(w, &pid)) return L"";
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"";
    wchar_t buf[MAX_PATH];
    DWORD n = MAX_PATH;
    std::wstring r;
    if (QueryFullProcessImageNameW(h, 0, buf, &n)) {
        const wchar_t* s = wcsrchr(buf, L'\\');
        r = s ? s + 1 : buf;
    }
    CloseHandle(h);
    return r;
}

static DWORD WINAPI clipSetThread(void* p) {
    ClipSet* c = (ClipSet*)p;
    double t0 = nowMs();
    // Proprietaria degli appunti è la finestra di [z]f, anche se li apre questo thread
    // (OpenClipboard non chiede che la finestra sia del thread che chiama). Con OpenClipboard(NULL)
    // EmptyClipboard lascerebbe gli appunti senza proprietario, e la documentazione dice che allora
    // SetClipboardData fallisce; una finestra di questo thread sparirebbe con lui. Al proprietario
    // arriva poi solo WM_DESTROYCLIPBOARD, quando un altro scrive (DefWindowProc, un attimo): niente
    // rendering ritardato, quindi niente WM_RENDERFORMAT. Se gli appunti li possedeva l'oggetto OLE
    // di Copia (clipSetPaths), EmptyClipboard aspetta il thread UI, che lo libera senza aspettare.
    if (clipOpen(c->hwnd)) {
        // Si scrive solo se nel frattempo non è partita una copia più recente; il controllo ad
        // appunti aperti tiene l'ordine anche tra due thread che riprovano
        if (c->gen != g_clipSetGen) {
            c->skipped = true;
        } else if (!EmptyClipboard()) {
            c->err = GetLastError();
        } else {
            size_t bytes = (c->text.size() + 1) * sizeof(wchar_t);
            HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, bytes);
            void* m = g ? GlobalLock(g) : nullptr;
            if (m) {
                memcpy(m, c->text.c_str(), bytes);
                GlobalUnlock(g);
                c->ok = SetClipboardData(CF_UNICODETEXT, g) != nullptr; // da qui la memoria è degli appunti
            }
            if (!c->ok) {
                c->err = GetLastError();
                if (g) GlobalFree(g);
            }
        }
        CloseClipboard();
    } else {
        c->err = GetLastError();
        c->holder = clipHolder();
    }
    double ms = nowMs() - t0;
    if (!c->ok && !c->skipped) hitchLog("appunti non scritti", c->err, ms);
    else if (ms > ui::LOG_MSG) hitchLog("appunti lenti (testo)", (unsigned)c->text.size(), ms);
    if (!PostMessageW(c->hwnd, WM_APP_CLIPSET, 0, (LPARAM)c)) delete c;
    return 0;
}

// Prende c (new ClipSet con hwnd, text, done, fail e, per Taglia, target/at/after)
static void clipSetText(ClipSet* c) {
    c->gen = InterlockedIncrement(&g_clipSetGen);
    HANDLE t = CreateThread(nullptr, 0, clipSetThread, c, 0, nullptr);
    if (t) { CloseHandle(t); return; }
    c->err = GetLastError(); // senza thread l'avviso dice comunque che il testo non è negli appunti
    if (!PostMessageW(c->hwnd, WM_APP_CLIPSET, 0, (LPARAM)c)) delete c;
}

struct TextField {
    std::wstring text;
    int caret = 0, anchor = 0; // la selezione va da anchor a caret
    float scroll = 0;          // scorrimento orizzontale (pixel)

    int selA() const { return std::min(anchor, caret); }
    int selB() const { return std::max(anchor, caret); }
    bool hasSel() const { return anchor != caret; }
    void set(const std::wstring& t, int a, int c) { text = t; anchor = a; caret = c; scroll = 0; }
    void eraseSel() {
        if (!hasSel()) return;
        int a = selA();
        text.erase((size_t)a, (size_t)(selB() - a));
        caret = anchor = a;
    }
    void insert(const std::wstring& s) {
        eraseSel();
        text.insert((size_t)caret, s);
        caret += (int)s.size();
        anchor = caret;
    }
    static bool isSep(wchar_t c) { return c == L' ' || c == L'\\' || c == L'/' || c == L'.' || c == L'_' || c == L'-'; }
    int wordLeft(int i) const {
        while (i > 0 && isSep(text[i - 1])) i--;
        while (i > 0 && !isSep(text[i - 1])) i--;
        return i;
    }
    int wordRight(int i) const {
        int n = (int)text.size();
        while (i < n && !isSep(text[i])) i++;
        while (i < n && isSep(text[i])) i++;
        return i;
    }
    void moveTo(int i, bool shift) {
        caret = std::max(0, std::min((int)text.size(), i));
        if (!shift) anchor = caret;
    }
    // Ritorna true se il tasto è stato gestito
    bool key(HWND h, WPARAM vk, bool ctrl, bool shift) {
        switch (vk) {
        case VK_LEFT:
            if (hasSel() && !shift) moveTo(selA(), false);
            else moveTo(ctrl ? wordLeft(caret) : caret - 1, shift);
            return true;
        case VK_RIGHT:
            if (hasSel() && !shift) moveTo(selB(), false);
            else moveTo(ctrl ? wordRight(caret) : caret + 1, shift);
            return true;
        case VK_HOME: moveTo(0, shift); return true;
        case VK_END: moveTo((int)text.size(), shift); return true;
        case VK_BACK:
            if (hasSel()) eraseSel();
            else if (caret > 0) {
                int a = ctrl ? wordLeft(caret) : caret - 1;
                text.erase((size_t)a, (size_t)(caret - a));
                caret = anchor = a;
            }
            return true;
        case VK_DELETE:
            if (hasSel()) eraseSel();
            else if (caret < (int)text.size()) text.erase((size_t)caret, (size_t)((ctrl ? wordRight(caret) : caret + 1) - caret));
            return true;
        }
        if (ctrl) {
            if (vk == 'A') { anchor = 0; caret = (int)text.size(); return true; }
            if (vk == 'C' || vk == 'X') {
                if (hasSel()) {
                    ClipSet* c = new ClipSet{ h, text.substr((size_t)selA(), (size_t)(selB() - selA())), nullptr,
                                              vk == 'X' ? L"Impossibile tagliare il testo" : L"Impossibile copiare il testo" };
                    if (vk == 'X') { // si taglia subito, come sempre: se gli appunti non lo prendono, torna (onClipSet)
                        eraseSel();
                        c->target = this;
                        c->at = caret;
                        c->after = text;
                    }
                    clipSetText(c);
                }
                return true;
            }
            if (vk == 'V') { clipAskText(h, this); return true; } // il testo arriva dopo: onClipText
        }
        return false;
    }
};

// Disegna la casella. Il testo scorre in orizzontale per tenere visibile il cursore.
// hint: testo in C_FAINT a campo vuoto ("Cerca…"), un po' a destra del cursore che lampeggia
// all'inizio; alpha: dissolvenza del pannello che contiene il campo
static void drawField(TextField& f, float x, float y, float w, float h, float scale, const wchar_t* hint = nullptr,
                      float alpha = 1.0f) {
    float pad = floorf(ui::FIELD_PAD * scale), line = std::max(1.0f, floorf(scale));
    float rad = floorf(ui::R_S * scale); // casella di testo di Windows 11: angoli da 4 px, sottolineatura d'accento
    rrect(x, y, w, h, rad, C_INPUT);
    float edge = floorf(ui::EDGE * scale);
    rect(x + rad * 0.5f, y + h - edge, w - rad, edge, C_ACCENT);
    float ty = y + floorf((h - F.lineH) * 0.5f);
    float inner = w - pad * 2;
    float cx = (float)textWidth(f.text.c_str(), f.caret);
    float total = (float)textWidth(f.text.c_str(), (int)f.text.size());
    if (cx - f.scroll > inner - 2) f.scroll = cx - inner + 2;
    if (cx - f.scroll < 0) f.scroll = cx;
    f.scroll = std::max(0.0f, std::min(f.scroll, std::max(0.0f, total - inner + 2)));
    float ox = x + pad - f.scroll, l = x + pad, r = x + w - pad;
    if (f.hasSel()) {
        float a = ox + textWidth(f.text.c_str(), f.selA()), b = ox + textWidth(f.text.c_str(), f.selB());
        a = std::max(a, l);
        b = std::min(b, r);
        if (b > a) rect(a, ty, b - a, (float)F.lineH, C_SEL_TEXT);
    }
    drawTextClipped(ox, ty, f.text.c_str(), (int)f.text.size(), C_TEXT, l, r);
    if (hint && f.text.empty())
        drawTextClipped(l + floorf(ui::GAP_S * scale), ty, hint, (int)wcslen(hint), fadeA(C_FAINT, alpha), l, r);
    rect(floorf(ox + cx), ty, line, (float)F.lineH, C_ACCENT); // cursore
}
