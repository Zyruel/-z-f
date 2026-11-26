// [z]f — pannello delle opzioni (Ctrl+, · menu della barra delle schede).
//
// Pannello sovrapposto, come menu e tooltip: si chiude con Esc, con la × o cliccando fuori.
// Ogni opzione ha effetto subito e si salva in impostazioni.txt. Le scorciatoie vengono da una
// tabella unica (kShortcuts): è l'elenco di riferimento anche per i menu (shortcutFor) e il README.
// Quello che si scrive va nella ricerca delle scorciatoie.


// Un tasto o gesto. cmd è la voce di menu (MenuId) che fa la stessa cosa: il menu mostra questo
// testo accanto alla voce, così le scorciatoie sono scritte in un posto solo.
// Un tasto che esegue solo il suo comando ha anche vk e mods (K(...)): onKey lo esegue da qui
// (shortcutCmd, menuRun) e il testo si ricava dal tasto (keyName), quindi ciò che si mostra e ciò
// che succede non possono divergere. I tasti che dipendono dal contesto (frecce, Esc, Backspace,
// Ctrl+V, Ctrl+1…9…) hanno solo il testo e restano scritti in onKey.
enum { KC = 1, KS = 2, KA = 4 }; // Ctrl, Shift, Alt
struct ShortcutKey { const wchar_t* text; int cmd; UINT vk; int mods; };
#define K(cmd, mods, vk) { nullptr, cmd, vk, mods }
// Fino a tre alternative, mostrate in una riga separate da " · "
struct Shortcut { ShortcutKey keys[3]; const wchar_t* what; };
struct ShortcutGroup { const wchar_t* title; Shortcut* items; int n; };

static Shortcut kNav[] = {
    { { K(M_OPEN, 0, VK_RETURN), { L"doppio clic", 0 } }, L"Apre la cartella o il file" },
    { { K(M_UP, KA, VK_UP), { L"Backspace", 0 } }, L"Cartella superiore (Backspace: se non c'è un filtro)" },
    { { K(M_BACK, KA, VK_LEFT), { L"tasto laterale indietro", 0 } }, L"Indietro" },
    { { K(M_FWD, KA, VK_RIGHT), { L"tasto laterale avanti", 0 } }, L"Avanti" },
    { { { L"Clic su un segmento del percorso", 0 } }, L"Va a quella cartella" },
    { { { L"Clic su › nel percorso", 0 } }, L"Sottocartelle" },
    { { { L"Clic sulla destinazione di un collegamento", 0 }, { L"clic centrale", 0 } }, L"Apre il percorso del file (in una nuova scheda)" },
    { { K(M_EDIT_PATH, KC, 'L'), K(M_EDIT_PATH, 0, VK_F4), { L"clic sul percorso vuoto", 0 } }, L"Scrive il percorso" },
    { { K(M_GOTO, KC, 'P'), K(M_GOTO, KC, 'G') }, L"Vai a: cartelle recenti, preferiti, sottocartelle" },
    { { { L"Ctrl+Invio", M_GOTO_NEWTAB }, { L"clic centrale", 0 } }, L"Vai a: apre in una nuova scheda" },
    { { { L"Tab", M_GOTO_ENTER } }, L"Vai a: entra nella cartella e mostra le sottocartelle" },
    { { K(M_REFRESH, 0, VK_F5) }, L"Ricarica" },
};
static Shortcut kSel[] = {
    { { { L"↑ ↓", 0 }, { L"PagSu PagGiù", 0 }, { L"Home Fine", 0 } }, L"Muove il cursore e seleziona" },
    { { { L"Shift+frecce", 0 } }, L"Estende la selezione" },
    { { { L"Ctrl+frecce", 0 } }, L"Muove solo il cursore" },
    { { { L"Ctrl+Spazio", 0 } }, L"Seleziona o deseleziona la voce del cursore" },
    { { { L"Ctrl+clic", 0 }, { L"Shift+clic", 0 }, { L"Ctrl+Shift+clic", 0 } }, L"Alterna · intervallo · aggiunge l'intervallo" },
    // Da icona e nome di una voce selezionata parte invece il trascinamento dei file (gruppo File)
    { { { L"Trascina nella lista", 0 }, { L"con Ctrl", 0 }, { L"con Shift", 0 } }, L"Seleziona col rettangolo · alterna · aggiunge" },
    { { { L"Clic sotto le righe", 0 } }, L"Toglie la selezione" },
    { { { L"Ctrl+A", 0 } }, L"Seleziona tutto ciò che è visibile" },
    { { { L"Lettere", 0 } }, L"Filtro sui nomi" },
    { { { L"Backspace", 0 } }, L"Toglie l'ultima lettera del filtro" },
    { { { L"Esc", M_FILTER_CLEAR } }, L"Toglie il filtro, poi la selezione" },
};
static Shortcut kFiles[] = {
    { { K(M_COPY, KC, 'C'), K(M_CUT, KC, 'X') }, L"Copia · taglia" },
    // Ctrl+V scritto a mano: nella lista incolla anche un percorso o un testo (smartPaste), il menu solo i file
    { { { L"Ctrl+V", M_PASTE } }, L"Incolla i file; un percorso ci va, un testo filtra" },
    { { K(M_COPYPATH, KC | KS, 'C') }, L"Copia percorso" },
    { { K(M_NEWFOLDER, KC | KS, 'N') }, L"Nuova cartella" },
    { { K(M_RENAME, 0, VK_F2) }, L"Rinomina" },
    { { K(M_DELETE, 0, VK_DELETE), K(M_DELETE, KS, VK_DELETE) }, L"Cestino · elimina definitivamente" }, // Shift lo legge deleteSelected
    { { K(M_UNDO, KC, 'Z') }, L"Annulla l'ultima operazione sui file" },
    { { K(M_REDO, KC, 'Y'), K(M_REDO, KC | KS, 'Z') }, L"Ripete l'operazione annullata" },
    { { K(M_PROPS, KA, VK_RETURN) }, L"Proprietà" },
    { { { L"Tasto destro", 0 }, { L"Menu", 0 } }, L"Menu contestuale" },
    { { { L"Shift+tasto destro", 0 }, { L"Shift+F10", M_MORE } }, L"Menu completo della shell (o di [z]f, con l'opzione)" },
    { { { L"Trascina una voce selezionata", 0 } }, L"Sposta sulla stessa unità, altrimenti copia" },
    { { { L"Ctrl+trascina", M_DROP_COPY }, { L"Shift+trascina", M_DROP_MOVE }, { L"Alt+trascina", M_DROP_LINK } },
      L"Copia · sposta · crea collegamento" },
    { { { L"Trascina col tasto destro", 0 } }, L"Chiede se copiare, spostare o collegare" },
};
static Shortcut kTabs[] = {
    { { K(M_TAB_NEW, KC, 'T') }, L"Nuova scheda" },
    { { K(M_TAB_CLOSE, KC, 'W'), { L"clic centrale sulla scheda", 0 } }, L"Chiude la scheda" },
    { { { L"Ctrl+Tab", 0 }, { L"Ctrl+Shift+Tab", 0 } }, L"Scheda successiva · precedente" },
    { { { L"Ctrl+1…8", 0 }, { L"Ctrl+9", 0 } }, L"Scheda n · ultima scheda" },
    { { { L"Clic centrale", M_NEWTAB }, { L"Ctrl+clic su un preferito", 0 } }, L"Apre in una nuova scheda in background" },
    { { { L"Trascina su una scheda", 0 } }, L"La apre dopo un momento" },
};
static Shortcut kView[] = {
    { { K(M_SIDEBAR, KC, 'B') }, L"Mostra o nasconde la barra laterale" },
    { { K(M_PREVIEW, KA, 'P') }, L"Mostra o nasconde l'anteprima" },
    { { K(M_GRID_L, KC | KS, '2'), K(M_GRID_M, KC | KS, '3'), K(M_GRID_S, KC | KS, '4') },
      L"Griglia: miniature grandi · medie · piccole" },
    { { K(M_VIEW_LIST, KC | KS, '6') }, L"Elenco" },
    { { { L"Trascina il bordo di un pannello", 0 }, { L"doppio clic", 0 } }, L"Larghezza di barra laterale o anteprima · predefinita" },
    { { { L"Clic sull'intestazione", 0 } }, L"Ordina per colonna (di nuovo: al contrario)" },
    { { { L"Trascina il bordo di una colonna", 0 }, { L"doppio clic", 0 } }, L"Larghezza della colonna · adatta al contenuto" },
    { { K(M_FAV_TOGGLE, KC, 'D') }, L"Aggiunge o toglie dai preferiti" },
    { { { L"Trascina tra i preferiti", 0 } }, L"Aggiunge le cartelle ai preferiti" },
    { { { L"Trascina un preferito", 0 } }, L"Lo sposta tra gli altri (solo l'ordine)" },
    { { { L"Clic su Preferiti", 0 }, { L"sulla freccia di Questo PC", 0 } }, L"Chiude o riapre i preferiti · le unità" },
    { { { L"Ctrl+rotellina", 0 }, { L"Ctrl+/−", 0 } }, L"Dimensione del testo" },
    { { { L"Ctrl+0", 0 } }, L"Dimensione del testo predefinita" },
    { { { L"Ctrl+rotellina in griglia", 0 } }, L"Dimensione delle miniature" },
    { { K(M_OPTIONS, KC, VK_OEM_COMMA) }, L"Opzioni" },
    { { { L"F12", 0 } }, L"Statistiche e tempi" },
};
#undef K
static const ShortcutGroup kShortcuts[] = {
    { L"Navigazione", kNav, (int)(sizeof(kNav) / sizeof(kNav[0])) },
    { L"Selezione", kSel, (int)(sizeof(kSel) / sizeof(kSel[0])) },
    { L"File", kFiles, (int)(sizeof(kFiles) / sizeof(kFiles[0])) },
    { L"Schede", kTabs, (int)(sizeof(kTabs) / sizeof(kTabs[0])) },
    { L"Vista", kView, (int)(sizeof(kView) / sizeof(kView[0])) },
};

// Nome di un tasto come lo scrive Windows in italiano: "Ctrl+Shift+C", "Alt+↑", "Invio", "Canc"
static std::wstring keyName(UINT vk, int mods) {
    std::wstring s;
    if (mods & KC) s += L"Ctrl+";
    if (mods & KS) s += L"Shift+";
    if (mods & KA) s += L"Alt+";
    switch (vk) {
    case VK_RETURN:    s += L"Invio"; break;
    case VK_DELETE:    s += L"Canc"; break;
    case VK_UP:        s += L"↑"; break;
    case VK_DOWN:      s += L"↓"; break;
    case VK_LEFT:      s += L"←"; break;
    case VK_RIGHT:     s += L"→"; break;
    case VK_OEM_COMMA: s += L","; break;
    default:
        if (vk >= VK_F1 && vk <= VK_F24) s += L"F" + std::to_wstring(vk - VK_F1 + 1);
        else if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) s += (wchar_t)vk;
        else s += L"?"; // un tasto nuovo nella tabella: va aggiunto qui
    }
    return s;
}

// La prima volta: i testi dei tasti con vk, ricavati dal tasto. I testi restano in memoria per
// tutta la vita del processo (i menu e i tooltip tengono i puntatori).
static void shortcutsReady() {
    static bool done = false;
    static std::vector<std::wstring>* names = new std::vector<std::wstring>;
    if (done) return;
    done = true;
    size_t n = 0;
    for (const ShortcutGroup& g : kShortcuts)
        for (int i = 0; i < g.n; i++)
            for (const ShortcutKey& k : g.items[i].keys) n += k.vk && !k.text;
    names->reserve(n); // niente riallocazioni: i puntatori restano validi
    for (const ShortcutGroup& g : kShortcuts)
        for (int i = 0; i < g.n; i++)
            for (ShortcutKey& k : g.items[i].keys)
                if (k.vk && !k.text) { names->push_back(keyName(k.vk, k.mods)); k.text = names->back().c_str(); }
}

// Il comando del tasto vk con esattamente questi modificatori (KC, KS, KA); 0 se nessuno
static int shortcutCmd(UINT vk, int mods) {
    for (const ShortcutGroup& g : kShortcuts)
        for (int i = 0; i < g.n; i++)
            for (const ShortcutKey& k : g.items[i].keys)
                if (k.vk == vk && k.mods == mods && k.cmd) return k.cmd;
    return 0;
}

// Il tasto che il menu mostra accanto alla voce menuId; "" se il comando non ne ha
static const wchar_t* shortcutFor(int menuId) {
    if (!menuId) return L"";
    shortcutsReady();
    for (const ShortcutGroup& g : kShortcuts)
        for (int i = 0; i < g.n; i++)
            for (const ShortcutKey& k : g.items[i].keys)
                if (k.text && k.cmd == menuId) return k.text;
    return L"";
}

// Le alternative in una riga, come le mostra il pannello: "Ctrl+C · Ctrl+X · Ctrl+V"
static std::wstring shortcutText(const Shortcut& s) {
    shortcutsReady();
    std::wstring t;
    for (const ShortcutKey& k : s.keys) {
        if (!k.text) continue;
        if (!t.empty()) t += L" · ";
        t += k.text;
    }
    return t;
}

// Tutte le alternative di un comando, per i tooltip: "Alt+← · tasto laterale indietro". La riga
// intera se il comando è il solo della riga; altrimenti (Ctrl+C · Ctrl+X · Ctrl+V) il suo tasto.
static std::wstring shortcutsOf(int cmd) {
    if (!cmd) return L"";
    shortcutsReady();
    for (const ShortcutGroup& g : kShortcuts)
        for (int i = 0; i < g.n; i++) {
            const Shortcut& s = g.items[i];
            const wchar_t* mine = nullptr;
            bool shared = false;
            for (const ShortcutKey& k : s.keys) {
                if (!k.text) continue;
                if (k.cmd == cmd) mine = k.text;
                else if (k.cmd) shared = true;
            }
            if (mine) return shared ? std::wstring(mine) : shortcutText(s);
        }
    return L"";
}

enum OptId { OPT_NONE = -1, OPT_CLOSE = 1, OPT_FONT_MINUS, OPT_FONT_PLUS, OPT_FACE_PREV, OPT_FACE_NEXT,
             OPT_ANIM, OPT_SIDEBAR, OPT_DEFAULT, OPT_FOLDER, OPT_PREVIEW, OPT_MONO_PREV, OPT_MONO_NEXT,
             OPT_UNDO_PERSIST, OPT_SHELL_MENU, OPT_DIR_SIZES, OPT_DIALOGS, OPT_TIMES,
             OPT_REOPEN, OPT_UPDATES, OPT_BETA, OPT_UPD_ACT };

struct OptionsPanel {
    bool open = false;
    double openedAt = 0;
    TextField search;
    float scroll = 0, maxScroll = 0;
    float x = 0, y = 0, w = 0, h = 0, viewTop = 0, viewBottom = 0;
    std::vector<std::wstring> faces;   // caratteri installati tra quelli proposti
    std::vector<std::wstring> monoFaces; // lo stesso per il testo nell'anteprima
    int defaultState = 0;              // [z]f al posto di Explorer (letto all'apertura)
    struct Hit { float x, y, w, h; int id; };
    std::vector<Hit> hits;             // aree cliccabili dell'ultimo frame
    int hover = OPT_NONE;
    HoverFade fade;
    float knobAnim = 0, knobSide = 0, knobDefault = 0, knobPreview = 0, knobUndo = 0, knobMenu = 0, knobDirSizes = 0, knobDialogs = 0, knobTimes = 0; // posizione animata degli interruttori
    float knobReopen = 0, knobUpdates = 0, knobBeta = 0;
} OP;

static const wchar_t* kFaceChoices[] = { L"Segoe UI", L"Segoe UI Variable Text", L"Calibri", L"Tahoma", L"Verdana", nullptr };
// Testo nell'anteprima: a spaziatura fissa, più il carattere dell'interfaccia per chi lo preferisce
static const wchar_t* kMonoChoices[] = { L"Cascadia Mono", L"Consolas", L"Lucida Console", L"Courier New", L"Segoe UI", nullptr };

// Caratteri proposti che sono davvero installati (la mappatura dei font di GDI è in memoria),
// più quello in uso se è stato scelto a mano in impostazioni.txt
static std::vector<std::wstring> installedFaces(const wchar_t* const* choices, const std::wstring& current) {
    std::vector<std::wstring> out;
    HDC dc = CreateCompatibleDC(nullptr);
    for (int i = 0; choices[i]; i++) {
        HFONT h = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, choices[i]);
        HGDIOBJ old = SelectObject(dc, h);
        wchar_t got[LF_FACESIZE] = {};
        GetTextFaceW(dc, LF_FACESIZE, got);
        SelectObject(dc, old);
        DeleteObject(h);
        if (!_wcsicmp(got, choices[i])) out.push_back(choices[i]);
    }
    DeleteDC(dc);
    bool has = false;
    for (auto& f : out) if (!_wcsicmp(f.c_str(), current.c_str())) has = true;
    if (!has) out.insert(out.begin(), current);
    return out;
}

// Passo d al carattere successivo o precedente dell'elenco
static std::wstring faceStep(const std::vector<std::wstring>& faces, const std::wstring& cur, int d) {
    int n = (int)faces.size(), i = 0;
    for (int k = 0; k < n; k++) if (!_wcsicmp(faces[k].c_str(), cur.c_str())) i = k;
    return n ? faces[(i + d + n) % n] : cur;
}

static std::wstring buildInfo() {
#if defined(_MSC_VER) && !defined(__clang__)
    const wchar_t* comp = L"MSVC";
#else
    const wchar_t* comp = L"zig/clang";
#endif
    // __DATE__ = "Nov 25 2026"
    static const char* mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const char* d = __DATE__;
    int m = 0;
    for (int i = 0; i < 12; i++) if (!strncmp(d, mon[i], 3)) m = i + 1;
    wchar_t b[96];
    std::wstring year(d + 7, d + 11);
    _snwprintf(b, 95, L"[z]f %s · compilato il %02d/%02d/%s con %s", ZF_VERSION, atoi(d + 4), m, year.c_str(), comp);
    b[95] = 0;
    return b;
}

static void optionsOpen() {
    OP.open = true;
    OP.openedAt = nowMs();
    OP.search.set(L"", 0, 0);
    OP.scroll = 0;
    OP.hover = OPT_NONE;
    OP.defaultState = defaultState();
    OP.knobAnim = A.animations ? 1.0f : 0.0f;
    OP.knobSide = A.sideVisible ? 1.0f : 0.0f;
    OP.knobDefault = OP.defaultState == 1 ? 1.0f : 0.0f;
    OP.knobPreview = A.prevOpen ? 1.0f : 0.0f;
    OP.knobUndo = A.undoPersist ? 1.0f : 0.0f;
    OP.knobMenu = A.shellMenu ? 1.0f : 0.0f;
    OP.knobDirSizes = A.dirSizes ? 1.0f : 0.0f;
    OP.knobDialogs = A.dialogJump ? 1.0f : 0.0f;
    OP.knobTimes = A.timesBar ? 1.0f : 0.0f;
    OP.knobReopen = A.reopenTabs ? 1.0f : 0.0f;
    OP.knobUpdates = A.updAuto ? 1.0f : 0.0f;
    OP.knobBeta = updBeta() ? 1.0f : 0.0f;
    OP.faces = installedFaces(kFaceChoices, F.face);
    OP.monoFaces = installedFaces(kMonoChoices, FM.face);
    menuClose();
    tipHide();
    A.renaming = false;
    A.editingPath = false;
    A.dirty = true;
}

static void optionsClose() {
    if (!OP.open) return;
    OP.open = false;
    A.dirty = true;
}

static void optionsFace(int d) {
    if (OP.faces.size() < 2) return;
    F.face = faceStep(OP.faces, F.face, d);
    fontCreate(A.scale);
    layout();
    settingsSave();
}

static void optionsMonoFace(int d) {
    if (OP.monoFaces.size() < 2) return;
    FM.face = faceStep(OP.monoFaces, FM.face, d);
    monoReset(); // si ricrea al prossimo testo dell'anteprima
    settingsSave();
}

// "Cartella delle impostazioni": crearla tocca il disco, quindi qui; la scheda la apre la finestra
static DWORD WINAPI settingsDirThread(void* p) {
    std::wstring* dir = (std::wstring*)p;
    CreateDirectoryW(dir->c_str(), nullptr);
    if (!PostMessageW(A.hwnd, WM_APP_OPENTAB, 0, (LPARAM)dir)) delete dir;
    return 0;
}

static void optionsExec(int id) {
    switch (id) {
    case OPT_CLOSE:      optionsClose(); break;
    case OPT_FONT_MINUS: fontSizeStep(-1); break;
    case OPT_FONT_PLUS:  fontSizeStep(1); break;
    case OPT_FACE_PREV:  optionsFace(-1); break;
    case OPT_FACE_NEXT:  optionsFace(1); break;
    case OPT_ANIM:       A.animations = !A.animations; settingsSave(); break;
    case OPT_SIDEBAR:    sidebarToggle(); break;
    case OPT_PREVIEW:    previewToggle(); break;
    case OPT_MONO_PREV:  optionsMonoFace(-1); break;
    case OPT_MONO_NEXT:  optionsMonoFace(1); break;
    case OPT_DEFAULT:    toggleDefault(); break; // lo stato si rilegge all'esito (WM_APP_DEFAULT)
    case OPT_UNDO_PERSIST: undoPersistToggle(); break;
    case OPT_SHELL_MENU: A.shellMenu = !A.shellMenu; settingsSave(); break;
    case OPT_DIR_SIZES:  A.dirSizes = !A.dirSizes; settingsSave(); dirSizeQueueCurrent(); break;
    case OPT_DIALOGS:    A.dialogJump = !A.dialogJump; settingsSave(); break;
    case OPT_TIMES:      A.timesBar = !A.timesBar; settingsSave(); break;
    case OPT_REOPEN:     A.reopenTabs = !A.reopenTabs; settingsSave(); break;
    case OPT_UPDATES:    A.updAuto = !A.updAuto; settingsSave(); break;
    case OPT_BETA:       A.updBeta = updBeta() ? 0 : 1; settingsSave(); break;
    case OPT_UPD_ACT:    updAction(); break; // controlla, aggiorna, o apre la Release
    case OPT_FOLDER: {
        std::wstring p = zfDataPath(L"");
        if (!p.empty()) p.pop_back(); // senza "\" finale
        optionsClose();
        // La cartella può non esserci ancora (nessuna impostazione salvata): la crea un thread, che
        // poi chiede alla finestra di aprirla (WM_APP_OPENTAB)
        HANDLE h = CreateThread(nullptr, 0, settingsDirThread, new std::wstring(p), 0, nullptr);
        if (h) CloseHandle(h);
        break;
    }
    }
    A.dirty = true;
}

static int optionsHit(int x, int y) {
    for (const OptionsPanel::Hit& h : OP.hits)
        if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) return h.id;
    return OPT_NONE;
}

// --- input: true se l'evento è stato gestito dal pannello ------------------------------
static bool optionsMouseDown(int x, int y) {
    if (!OP.open) return false;
    if (x < OP.x || x >= OP.x + OP.w || y < OP.y || y >= OP.y + OP.h) { optionsClose(); return true; } // clic fuori
    int id = optionsHit(x, y);
    if (id != OPT_NONE) optionsExec(id);
    return true;
}

static bool optionsMouseMove(int x, int y) {
    if (!OP.open) return false;
    int id = optionsHit(x, y);
    if (id != OP.hover) { OP.hover = id; A.dirty = true; }
    return true;
}

static bool optionsWheel(float lines) { // a pixel: le frazioni di scatto dei touchpad contano
    if (!OP.open) return false;
    OP.scroll = std::max(0.0f, std::min(OP.maxScroll, OP.scroll - lines * A.rowH * ui::WHEEL_LINES));
    A.dirty = true;
    return true;
}

static bool optionsKey(WPARAM vk, bool ctrl, bool shift) {
    if (!OP.open) return false;
    if (vk == VK_ESCAPE) { // prima svuota la ricerca, poi chiude
        if (!OP.search.text.empty()) { OP.search.set(L"", 0, 0); OP.scroll = 0; }
        else optionsClose();
    } else if (vk == VK_UP || vk == VK_DOWN) {
        OP.scroll = std::max(0.0f, std::min(OP.maxScroll, OP.scroll + (vk == VK_DOWN ? 1 : -1) * A.rowH));
    } else if (vk == VK_PRIOR || vk == VK_NEXT) {
        float page = std::max(A.rowH, OP.viewBottom - OP.viewTop - A.rowH);
        OP.scroll = std::max(0.0f, std::min(OP.maxScroll, OP.scroll + (vk == VK_NEXT ? page : -page)));
    } else if (ctrl && vk == VK_OEM_COMMA) {
        optionsClose();
    } else {
        OP.search.key(A.hwnd, vk, ctrl, shift);
        OP.scroll = 0;
    }
    A.dirty = true;
    return true;
}

static bool optionsChar(wchar_t c) {
    if (!OP.open) return false;
    OP.search.insert(std::wstring(1, c));
    OP.scroll = 0;
    A.dirty = true;
    return true;
}

// --- disegno ---------------------------------------------------------------------------
static bool containsCI(const wchar_t* hay, const std::wstring& needle) {
    if (needle.empty()) return true;
    std::wstring h = hay, n = needle;
    CharLowerBuffW(&h[0], (DWORD)h.size());
    CharLowerBuffW(&n[0], (DWORD)n.size());
    return h.find(n) != std::wstring::npos;
}

static void drawOptions(double dt) {
    if (!OP.open) return;
    float W = (float)G.w, H = (float)G.h;
    PanelEnter e = panelEnter(OP.openedAt, ui::DUR_MENU);
    float k = e.k;
    OP.hits.clear();
    hoverFadeUpdate(OP.fade, OP.hover, dt);
    auto hit = [&](float x, float y, float w, float h, int id) {
        // solo la parte dentro l'area visibile è cliccabile
        float y0 = std::max(y, OP.viewTop), y1 = std::min(y + h, OP.viewBottom);
        if (id == OPT_CLOSE || id == OPT_FOLDER || id == OPT_UPD_ACT) { y0 = y; y1 = y + h; } // titolo e piede non scorrono
        if (y1 > y0) OP.hits.push_back({ x, y0, w, y1 - y0, id });
    };
    auto hov = [&](float x, float y, float w, float h, int id) {
        float a = hoverAlpha(OP.fade, id);
        if (a > 0) rrect(x, y, w, h, px(ui::R_S), fadeA(C_HOVER, a * k));
    };

    rect(0, 0, W, H, fadeA(C_SHADOW, k)); // il resto della finestra passa in secondo piano
    float pad = px(ui::GAP_L + ui::GAP_S), rowH = A.rowH + px(ui::GAP_S);
    float textOff = floorf((rowH - F.lineH) * 0.5f);
    OP.w = floorf(std::min(px(ui::OPT_W), W - px(ui::OPT_MARGIN)));
    OP.h = floorf(std::min(px(ui::OPT_H), H - px(ui::OPT_MARGIN)));
    OP.x = floorf((W - OP.w) * 0.5f);
    OP.y = floorf(std::max(px(ui::OPT_TOP_MIN), (H - OP.h) * 0.5f)) - e.dy;
    float x = OP.x, y = OP.y, w = OP.w, h = OP.h;
    drawPanel(x, y, w, h, px(ui::R_M), k);

    // Barra del titolo
    float titleH = floorf(A.rowH * ui::PATH_BAR);
    drawText(x + pad, y + floorf((titleH - F.lineH) * 0.5f), L"Opzioni", 7, fadeA(C_TEXT, k));
    float cw = titleH;
    float gap = px(ui::GAP_S);
    hov(x + w - cw + gap, y + gap, cw - gap * 2, titleH - gap * 2, OPT_CLOSE);
    drawUiIcon(x + w - cw, y, cw, titleH, IC_CLOSE, fadeA(C_DIM, k));
    hit(x + w - cw, y, cw, titleH, OPT_CLOSE);
    rect(x + 1, y + titleH - 1, w - 2, 1, fadeA(C_LINE, k));

    // Piede: versione e cartella delle impostazioni
    float footH = rowH + px(ui::GAP_S);
    float fy = y + h - footH;
    rect(x + 1, fy, w - 2, 1, fadeA(C_LINE, k));
    std::wstring info = buildInfo();
    float infoEnd = x + w * 0.6f - updIconW() - px(ui::GAP_M);
    drawText(x + pad, fy + floorf((footH - F.lineH) * 0.5f), info, fadeA(C_FAINT, k), infoEnd);
    {   // dopo la versione, lo stato dell'aggiornamento: un clic controlla, aggiorna o apre la Release
        float iw = updIconW(), ix = std::min(infoEnd, x + pad + (float)textWidth(info.c_str(), (int)info.size())) + px(ui::GAP_M);
        float iy = fy + floorf((footH - A.rowH) * 0.5f);
        hov(ix - px(ui::GAP_S), iy, iw + px(ui::GAP_M), A.rowH, OPT_UPD_ACT);
        drawUpdateIcon(ix, iy, iw, A.rowH, k);
        hit(ix - px(ui::GAP_S), iy, iw + px(ui::GAP_M), A.rowH, OPT_UPD_ACT);
    }
    const wchar_t* fl = L"Cartella delle impostazioni";
    float flw = (float)textWidth(fl, (int)wcslen(fl)) + px(ui::GAP_M) * 2;
    float flx = x + w - pad - flw + px(ui::GAP_M), fly = fy + floorf((footH - A.rowH) * 0.5f);
    hov(flx, fly, flw, A.rowH, OPT_FOLDER);
    drawText(flx + px(ui::GAP_M), fly + floorf((A.rowH - F.lineH) * 0.5f), fl, (int)wcslen(fl), fadeA(C_DIM, k));
    hit(flx, fly, flw, A.rowH, OPT_FOLDER);

    // Contenuto che scorre tra titolo e piede
    OP.viewTop = y + titleH;
    OP.viewBottom = fy;
    float cy = OP.viewTop + px(ui::GAP_M) - floorf(OP.scroll);
    float right = x + w - pad;
    // Solo righe intere: una riga a metà finirebbe sopra titolo o piede (il renderer non ritaglia)
    auto visible = [&](float yy, float hh) { return yy >= OP.viewTop && yy + hh <= OP.viewBottom; };
    auto section = [&](const wchar_t* t) {
        if (visible(cy, rowH)) drawText(x + pad, cy + textOff + px(ui::GAP_XS), t, (int)wcslen(t), fadeA(C_FAINT, k));
        cy += rowH;
    };
    auto label = [&](const wchar_t* t, const wchar_t* hint) {
        if (visible(cy, rowH)) drawText(x + pad + px(ui::GAP_M), cy + textOff, t, (int)wcslen(t), fadeA(C_TEXT, k), right - px(ui::OPT_LABEL_RESERVE));
        if (hint && visible(cy + rowH - px(ui::GAP_S), rowH))
            drawText(x + pad + px(ui::GAP_M), cy + rowH - px(ui::GAP_S) + textOff, hint, (int)wcslen(hint), fadeA(C_FAINT, k), right);
    };
    auto button = [&](float bx, float by, float bw, const wchar_t* t, int id) {
        if (!visible(by, rowH)) return;
        hov(bx, by + px(ui::GAP_XS), bw, rowH - px(ui::GAP_S), id);
        drawText(bx + floorf((bw - textWidth(t, (int)wcslen(t))) * 0.5f), by + textOff, t, (int)wcslen(t), fadeA(C_DIM, k));
        hit(bx, by, bw, rowH, id);
    };
    // Interruttore: binario e pallino, pillole come in Windows 11
    auto sw = [&](bool on, float& anim, int id) {
        anim = approach(anim, on ? 1.0f : 0.0f, dt, ui::TAU_TAB_LINE, 0.01f);
        float tw = px(ui::SWITCH_W), th = px(ui::SWITCH_H), tx = right - tw, ty = cy + floorf((rowH - th) * 0.5f);
        if (visible(cy, rowH)) {
            hov(tx - px(ui::GAP_S), cy + px(ui::GAP_XS), tw + px(ui::GAP_M), rowH - px(ui::GAP_S), id);
            // pillola come gli interruttori di Windows 11: spento contorno, acceso pieno
            rrect(tx, ty, tw, th, th * 0.5f, fadeA(C_DIM, k * (1 - anim)), std::max(1.0f, floorf(A.scale)));
            if (anim > 0) rrect(tx, ty, tw, th, th * 0.5f, fadeA(C_ACCENT, k * anim));
            float in = px(ui::SWITCH_KNOB_INSET), kn = th - in * 2, kx = tx + in + floorf((tw - kn - in * 2) * anim);
            uint32_t kc = anim > 0.5f ? C_KNOB_ON : C_DIM;
            rrect(kx, ty + floorf((th - kn) * 0.5f), kn, kn, kn * 0.5f, fadeA(kc, k));
        }
        hit(x + pad, cy, right - x - pad, rowH, id); // tutta la riga è cliccabile
    };

    section(L"Aspetto");
    {   // dimensione del testo
        label(L"Dimensione del testo", nullptr);
        wchar_t b[16];
        int n = _snwprintf(b, 15, L"%d px", F.px);
        float bw = rowH, vw = px(ui::OPT_VALUE_W);
        float bx = right - bw * 2 - vw;
        button(bx, cy, bw, L"\x2212", OPT_FONT_MINUS);
        if (visible(cy, rowH)) drawText(bx + bw + floorf((vw - textWidth(b, n)) * 0.5f), cy + textOff, b, n, fadeA(C_TEXT, k));
        button(bx + bw + vw, cy, bw, L"+", OPT_FONT_PLUS);
        cy += rowH;
    }
    // Selettore di un carattere: "‹  Segoe UI  ›"
    auto faceRow = [&](const wchar_t* t, const std::wstring& face, int prevId, int nextId) {
        label(t, nullptr);
        float bw = rowH, vw = px(ui::OPT_FACE_W);
        float bx = right - bw * 2 - vw;
        button(bx, cy, bw, L"\x2039", prevId);
        if (visible(cy, rowH)) {
            float tw = (float)textWidth(face.c_str(), (int)face.size());
            drawText(bx + bw + std::max(0.0f, floorf((vw - tw) * 0.5f)), cy + textOff, face, fadeA(C_TEXT, k), bx + bw + vw);
        }
        button(bx + bw + vw, cy, bw, L"\x203A", nextId);
        cy += rowH;
    };
    faceRow(L"Carattere", F.face, OPT_FACE_PREV, OPT_FACE_NEXT);
    label(L"Animazioni", nullptr);
    sw(A.animations, OP.knobAnim, OPT_ANIM);
    cy += rowH;
    label(L"Barra laterale", nullptr);
    sw(A.sideVisible, OP.knobSide, OPT_SIDEBAR);
    cy += rowH;
    label(L"Anteprima", nullptr);
    sw(A.prevOpen, OP.knobPreview, OPT_PREVIEW);
    cy += rowH;
    faceRow(L"Carattere dell'anteprima", FM.face, OPT_MONO_PREV, OPT_MONO_NEXT);
    label(L"Barra dei tempi", L"Sopra la barra di stato: lettura, ordinamento, frame, avvio. F12 per tutti i dettagli");
    sw(A.timesBar, OP.knobTimes, OPT_TIMES);
    cy += rowH * 2 - px(ui::GAP_S);
    cy += px(ui::GAP_M);

    section(L"Integrazione con Windows");
    label(L"Usa [z]f al posto di Explorer",
          OP.defaultState == 2 ? L"Attivo con un altro eseguibile di [z]f: un clic lo assegna a questo"
                               : L"Doppio clic sulle cartelle, Win+E, programmi che aprono una cartella");
    sw(OP.defaultState == 1, OP.knobDefault, OPT_DEFAULT);
    cy += rowH * 2 - px(ui::GAP_S);
    label(L"Menu completo di Windows col tasto destro",
          L"Al posto del menu di [z]f; Shift+tasto destro apre quello di [z]f. Si apre in 200\x2013" L"400 ms");
    sw(A.shellMenu, OP.knobMenu, OPT_SHELL_MENU);
    cy += rowH * 2 - px(ui::GAP_S);
    label(L"Dialoghi Apri e Salva nella cartella di [z]f",
          L"Quando si aprono, e quando ci torni dopo aver cambiato cartella in [z]f");
    sw(A.dialogJump, OP.knobDialogs, OPT_DIALOGS);
    cy += rowH * 2 - px(ui::GAP_S);
    cy += px(ui::GAP_M);

    section(L"File");
    label(L"Annulla dopo il riavvio", L"Ctrl+Z ricorda le operazioni anche dopo la chiusura (annulla.txt)");
    sw(A.undoPersist, OP.knobUndo, OPT_UNDO_PERSIST);
    cy += rowH * 2 - px(ui::GAP_S);
    label(L"Dimensione delle cartelle nella colonna",
          L"Contata in background, a priorità bassa; unità di rete e ottiche solo nel tooltip e nell'anteprima");
    sw(A.dirSizes, OP.knobDirSizes, OPT_DIR_SIZES);
    cy += rowH * 2 - px(ui::GAP_S);
    cy += px(ui::GAP_M);

    section(L"Avvio e aggiornamenti");
    label(L"Riapri le schede all'avvio", L"Le schede della finestra principale, com'erano alla chiusura");
    sw(A.reopenTabs, OP.knobReopen, OPT_REOPEN);
    cy += rowH * 2 - px(ui::GAP_S);
    {
        std::wstring last = updLastText();
        std::wstring hint = L"Una volta al giorno da GitHub" + (last.empty() ? std::wstring() : L" \x00B7 ultimo controllo " + last);
        label(L"Aggiornamenti automatici", hint.c_str());
    }
    sw(A.updAuto, OP.knobUpdates, OPT_UPDATES);
    cy += rowH * 2 - px(ui::GAP_S);
    label(L"Ricevi anche le beta", L"Le versioni di prova, prima delle stabili");
    sw(updBeta(), OP.knobBeta, OPT_BETA);
    cy += rowH * 2 - px(ui::GAP_S);
    cy += px(ui::GAP_M);

    // Scorciatoie, con la ricerca accanto al titolo
    float sTop = cy;
    section(L"Scorciatoie");
    if (visible(sTop, rowH)) {
        float fw = px(ui::OPT_SEARCH_W), fh = A.rowH;
        float fx = right - fw, fyy = sTop + floorf((rowH - fh) * 0.5f);
        drawField(OP.search, fx, fyy, fw, fh, A.scale, L"Cerca\x2026", k);
    }
    float keysW = 0;
    for (const ShortcutGroup& g : kShortcuts)
        for (int i = 0; i < g.n; i++) {
            std::wstring t = shortcutText(g.items[i]);
            keysW = std::max(keysW, (float)textWidth(t.c_str(), (int)t.size()));
        }
    keysW = std::min(keysW, (right - x - pad) * 0.5f);
    auto matches = [&](const Shortcut& s, const std::wstring& keys) {
        return containsCI(s.what, OP.search.text) || containsCI(keys.c_str(), OP.search.text);
    };
    int shown = 0;
    for (const ShortcutGroup& g : kShortcuts) {
        bool any = false;
        for (int i = 0; i < g.n && !any; i++) any = matches(g.items[i], shortcutText(g.items[i]));
        if (!any) continue;
        if (visible(cy, A.rowH)) drawText(x + pad + px(ui::GAP_M), cy + floorf((A.rowH - F.lineH) * 0.5f), g.title, (int)wcslen(g.title), fadeA(C_DIM, k));
        cy += A.rowH;
        for (int i = 0; i < g.n; i++) {
            const Shortcut& s = g.items[i];
            std::wstring keys = shortcutText(s);
            if (!matches(s, keys)) continue;
            if (visible(cy, A.rowH)) {
                float ty = cy + floorf((A.rowH - F.lineH) * 0.5f);
                drawText(x + pad + px(ui::GAP_M) * 2, ty, s.what, (int)wcslen(s.what), fadeA(C_TEXT, k), right - keysW - px(ui::GAP_L));
                drawText(right - keysW, ty, keys, fadeA(C_DIM, k), right);
            }
            cy += A.rowH;
            shown++;
        }
        cy += px(ui::GAP_S);
    }
    if (!shown) {
        if (visible(cy, A.rowH)) drawText(x + pad + px(ui::GAP_M), cy + floorf((A.rowH - F.lineH) * 0.5f), L"Nessuna scorciatoia trovata", 27, fadeA(C_FAINT, k));
        cy += A.rowH;
    }
    cy += px(ui::GAP_M);

    // Scorrimento: quanto contenuto c'è oltre l'area visibile
    float contentH = cy + floorf(OP.scroll) - OP.viewTop;
    OP.maxScroll = std::max(0.0f, contentH - (OP.viewBottom - OP.viewTop));
    if (OP.scroll > OP.maxScroll) { OP.scroll = OP.maxScroll; A.dirty = true; }
}
