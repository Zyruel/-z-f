// [z]f — pannello delle opzioni (⚙ · Ctrl+,).
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
    { { K(M_UP, KA, VK_UP), { L"Backspace", 0 } }, L"Va alla cartella superiore (Backspace: se non c'è un filtro per nome)" },
    // Anche i tasti del browser e i gesti del touchpad (WM_APPCOMMAND, in input.cpp)
    { { K(M_BACK, KA, VK_LEFT), { L"tasto Indietro del mouse o della tastiera", 0 }, { L"gesto Indietro del touchpad", 0 } }, L"Va indietro" },
    { { K(M_FWD, KA, VK_RIGHT), { L"tasto Avanti del mouse o della tastiera", 0 }, { L"gesto Avanti del touchpad", 0 } }, L"Va avanti" },
    { { { L"Clic su un segmento del percorso", 0 } }, L"Va alla cartella di quel segmento" },
    { { { L"Clic su › nel percorso", 0 } }, L"Mostra le sottocartelle di quel segmento" },
    { { { L"Clic sulla destinazione di un collegamento", 0 }, { L"clic centrale", 0 } }, L"Apre il percorso del file (in una nuova scheda)" },
    { { K(M_EDIT_PATH, KC, 'L'), K(M_EDIT_PATH, 0, VK_F4), { L"clic sul percorso vuoto", 0 } }, L"Apre Vai a col percorso della cartella aperta, da modificare" },
    { { K(M_GOTO, KC, 'P'), K(M_GOTO, KC, 'G') }, L"Apre Vai a: cartelle recenti, preferiti, sottocartelle" },
    { { { L"Ctrl+Invio", M_GOTO_NEWTAB }, { L"clic centrale", 0 } }, L"Apre in una nuova scheda la voce scelta in Vai a" },
    { { { L"Tab", M_GOTO_ENTER } }, L"Entra nella cartella scelta in Vai a e ne mostra le sottocartelle" },
    { { K(M_REFRESH, 0, VK_F5), { L"tasto Aggiorna della tastiera", 0 } }, L"Ricarica" },
};
static Shortcut kSel[] = {
    { { { L"↑ ↓", 0 }, { L"PagSu PagGiù", 0 }, { L"Home Fine", 0 } }, L"Muove il cursore e seleziona" },
    { { { L"Shift+frecce", 0 } }, L"Estende la selezione" },
    { { { L"Ctrl+frecce", 0 } }, L"Muove solo il cursore" },
    { { { L"Ctrl+Spazio", 0 } }, L"Seleziona o deseleziona la voce del cursore" },
    { { { L"Ctrl+clic", 0 }, { L"Shift+clic", 0 }, { L"Ctrl+Shift+clic", 0 } }, L"Alterna la voce · seleziona l'intervallo · aggiunge l'intervallo" },
    // Da icona e nome di una voce selezionata parte invece il trascinamento dei file (gruppo File)
    { { { L"Trascina nella lista", 0 }, { L"con Ctrl", 0 }, { L"con Shift", 0 } }, L"Seleziona col rettangolo · alterna · aggiunge" },
    { { { L"Clic sotto le righe", 0 } }, L"Toglie la selezione" },
    { { { L"Ctrl+A", 0 } }, L"Seleziona tutto ciò che è visibile" },
    { { { L"Lettere", 0 } }, L"Filtra i nomi mentre si scrive" },
    { { { L"Backspace", 0 } }, L"Toglie l'ultima lettera del filtro" },
    { { { L"Esc", M_FILTER_CLEAR } }, L"Toglie il filtro per nome, poi quelli delle colonne, poi la selezione (nell'anteprima, poi torna alla lista)" },
};
static Shortcut kFiles[] = {
    { { K(M_COPY, KC, 'C'), K(M_CUT, KC, 'X') }, L"Copia · taglia" },
    // Ctrl+V scritto a mano: nella lista incolla anche un percorso o un testo (smartPaste), il menu solo i file
    { { { L"Ctrl+V", M_PASTE } }, L"Incolla i file; un percorso ci va, un testo filtra" },
    { { K(M_COPYPATH, KC | KS, 'C') }, L"Copia il percorso" },
    { { K(M_NEWFOLDER, KC | KS, 'N') }, L"Crea una nuova cartella" },
    { { K(M_RENAME, 0, VK_F2) }, L"Rinomina" },
    { { K(M_DELETE, 0, VK_DELETE), K(M_DELETE, KS, VK_DELETE) }, L"Sposta nel Cestino · elimina definitivamente" }, // Shift arriva a deleteSelected da CmdCtx::shift
    { { K(M_UNDO, KC, 'Z') }, L"Annulla l'ultima operazione sui file" },
    { { K(M_REDO, KC, 'Y'), K(M_REDO, KC | KS, 'Z') }, L"Ripete l'operazione annullata" },
    { { K(M_PROPS, KA, VK_RETURN) }, L"Apre le proprietà" },
    { { { L"Tasto destro", 0 }, { L"Menu", 0 } }, L"Apre il menu contestuale" },
    { { { L"Shift+tasto destro", 0 }, { L"Shift+F10", M_MORE } }, L"Apre il menu completo della shell (o quello di [z]f, con l'opzione)" },
    { { { L"Trascina una voce selezionata", 0 } }, L"Sposta sulla stessa unità, altrimenti copia" },
    { { { L"Ctrl+trascina", M_DROP_COPY }, { L"Shift+trascina", M_DROP_MOVE }, { L"Alt+trascina", M_DROP_LINK } },
      L"Copia · sposta · crea collegamento" },
    { { { L"Trascina col tasto destro", 0 } }, L"Chiede se copiare, spostare o collegare" },
};
static Shortcut kTabs[] = {
    { { K(M_TAB_NEW, KC, 'T') }, L"Apre una nuova scheda" },
    { { K(M_NEW_WINDOW, KC, 'N') }, L"Apre una nuova finestra sulla stessa cartella" },
    { { K(M_TAB_CLOSE, KC, 'W'), { L"clic centrale sulla scheda", 0 } }, L"Chiude la scheda" },
    { { { L"Ctrl+Tab", 0 }, { L"Ctrl+Shift+Tab", 0 } }, L"Passa alla scheda successiva · precedente" },
    { { { L"Ctrl+1…8", 0 }, { L"Ctrl+9", 0 } }, L"Passa alla scheda n · all'ultima" },
    { { { L"Clic centrale", M_NEWTAB }, { L"Ctrl+clic su un preferito", 0 } }, L"Apre in una nuova scheda in background" },
    { { { L"Trascina su una scheda", 0 } }, L"Passa a quella scheda dopo un momento" },
};
static Shortcut kView[] = {
    { { K(M_SIDEBAR, KC, 'B') }, L"Mostra o nasconde la barra laterale" },
    { { K(M_PREVIEW, KA, 'P') }, L"Mostra o nasconde l'anteprima" },
    { { K(M_PREVIEW_SWAP, KA | KS, 'P') }, L"Sposta l'anteprima a destra o in basso" },
    { { K(M_PANE_KEYS, 0, VK_TAB), { L"clic nel pannello", 0 } }, L"Passa la tastiera alla lista o all'anteprima di una cartella" },
    { { { L"Doppio clic nell'anteprima", 0 } }, L"Apre il file; una cartella in una nuova scheda in background" },
    { { K(M_VIEW_DETAILS, KC | KS, '2'), K(M_VIEW_LIST, KC | KS, '3') }, L"Passa alla vista Dettagli · Elenco" },
    { { K(M_ICONS_S, KC | KS, '4'), K(M_ICONS_M, KC | KS, '5'), K(M_ICONS_L, KC | KS, '6') },
      L"Passa alle icone piccole · medie · grandi (zoom 40 · 70 · 100%)" },
    { { { L"Ctrl+rotellina", 0 } }, L"Cambia lo zoom della lista: Dettagli, Elenco, Icone da 20 a 100% (sull'anteprima: la sua vista)" },
    { { { L"Trascina il bordo di un pannello", 0 }, { L"doppio clic", 0 } }, L"Cambia la larghezza di barra laterale o anteprima (l'altezza, in basso) · torna alla predefinita" },
    { { { L"Clic su ˅ accanto a ◨", 0 } }, L"Mette l'anteprima a destra o in basso" },
    { { { L"Clic sull'intestazione", 0 } }, L"Ordina per colonna (di nuovo: al contrario)" },
    { { { L"Tasto destro sull'intestazione", 0 } }, L"Sceglie le colonne da mostrare e il raggruppamento" },
    { { { L"Clic sull'imbuto accanto al titolo di una colonna", 0 } }, L"Filtra la colonna per i valori presenti" },
    { { { L"Clic sull'intestazione di un gruppo", 0 } }, L"Chiude o apre il gruppo" },
    { { { L"Trascina il bordo di una colonna", 0 }, { L"doppio clic", 0 } }, L"Cambia la larghezza della colonna · la adatta al contenuto" },
    { { K(M_FAV_TOGGLE, KC, 'D') }, L"Aggiunge o toglie dai preferiti" },
    { { { L"Trascina tra i preferiti", 0 } }, L"Aggiunge le cartelle ai preferiti" },
    { { { L"Trascina un preferito", 0 } }, L"Cambia l'ordine dei preferiti" },
    { { { L"Clic su Preferiti", 0 }, { L"sulla freccia di Questo PC", 0 } }, L"Chiude o riapre i preferiti · le unità" },
    { { { L"Ctrl+/−", M_FONT_STEP } }, L"Cambia la dimensione del testo" },
    { { { L"Ctrl+0", M_FONT_RESET } }, L"Riporta il testo alla dimensione predefinita" },
    { { K(M_OPTIONS, KC, VK_OEM_COMMA) }, L"Apre le opzioni" },
    { { { L"F12", 0 } }, L"Mostra statistiche e tempi" },
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
    if (mods & KC) s += L"Ctrl+"; // l'ordine di Windows: Ctrl, Alt, Shift (Alt+Shift+P)
    if (mods & KA) s += L"Alt+";
    if (mods & KS) s += L"Shift+";
    switch (vk) {
    case VK_RETURN:    s += L"Invio"; break;
    case VK_DELETE:    s += L"Canc"; break;
    case VK_UP:        s += L"↑"; break;
    case VK_DOWN:      s += L"↓"; break;
    case VK_LEFT:      s += L"←"; break;
    case VK_RIGHT:     s += L"→"; break;
    case VK_OEM_COMMA: s += L","; break;
    case VK_TAB:       s += L"Tab"; break;
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

// Tutte le alternative di un comando, per i tooltip: "Alt+← · tasto Indietro del mouse o della
// tastiera". La riga intera se il comando è il solo della riga; altrimenti (Ctrl+C · Ctrl+X ·
// Ctrl+V) il suo tasto.
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
             OPT_PREVIEW_POS_PREV, OPT_PREVIEW_POS_NEXT,
             OPT_UNDO_PERSIST, OPT_SHELL_MENU, OPT_DIR_SIZES, OPT_EXT, OPT_DIALOGS, OPT_TIMES,
             OPT_REOPEN, OPT_UPDATES, OPT_BETA, OPT_UPD_ACT, OPT_SEPARATE, OPT_VERSION };
enum { OPT_TIP_ROW = 1 << 16 }; // OptionsPanel::tips: oltre, una riga troncata o con una scorciatoia (sotto, una scorciatoia troncata)

struct OptionsPanel {
    bool open = false;
    double openedAt = 0, closedAt = -1e18; // closedAt: il secondo clic di un doppio clic che le ha chiuse
    TextField search;
    float scroll = 0, maxScroll = 0;
    float x = 0, y = 0, w = 0, h = 0, viewTop = 0, viewBottom = 0;
    std::vector<std::wstring> faces;   // caratteri installati tra quelli proposti
    std::vector<std::wstring> monoFaces; // lo stesso per il testo nell'anteprima
    int defaultState = 0;              // [z]f al posto di Explorer (letto all'apertura)
    struct Hit { float x, y, w, h; int id; };
    std::vector<Hit> hits;             // aree cliccabili dell'ultimo frame
    std::vector<Hit> tips;             // scorciatoie troncate dell'ultimo frame (id: gruppo * 256 + voce): il tooltip ha il testo intero;
                                       // righe troncate o con una scorciatoia (id: OPT_TIP_ROW + indice in rowTips)
    struct RowTip { const wchar_t* label; int cmd, cmd2; }; // cmd: 0 se la riga non ha scorciatoia; cmd2: la riga "Predefinita" (Ctrl+0)
    std::vector<RowTip> rowTips;       // righe dell'ultimo frame troncate o con una scorciatoia: etichetta e comandi
    int hover = OPT_NONE;
    HoverFade fade;
    float knobAnim = 0, knobSide = 0, knobDefault = 0, knobPreview = 0, knobUndo = 0, knobMenu = 0, knobDirSizes = 0, knobDialogs = 0, knobTimes = 0; // posizione animata degli interruttori
    float knobReopen = 0, knobUpdates = 0, knobBeta = 0, knobSeparate = 0, knobExt = 0;
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
    OP.knobExt = A.showExt ? 1.0f : 0.0f;
    OP.knobDialogs = A.dialogJump ? 1.0f : 0.0f;
    OP.knobTimes = A.timesBar ? 1.0f : 0.0f;
    OP.knobReopen = A.reopenTabs ? 1.0f : 0.0f;
    OP.knobSeparate = A.separateWindows ? 1.0f : 0.0f;
    OP.knobUpdates = A.updAuto ? 1.0f : 0.0f;
    OP.knobBeta = updBeta() ? 1.0f : 0.0f;
    OP.faces = installedFaces(kFaceChoices, F.face);
    OP.monoFaces = installedFaces(kMonoChoices, FM.face);
    menuClose();
    tipHide();
    A.renaming = false;
    A.dirty = true;
}

// mouse: chiuse da un clic (fuori, ×, Apri cartella). Solo allora il secondo clic di un doppio clic
// non va alla lista: dopo Esc o Ctrl+, un doppio clic su un file lo deve aprire
static void optionsClose(bool mouse = false) {
    if (!OP.open) return;
    OP.open = false;
    OP.closedAt = mouse ? nowMs() : -1e18;
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

// "Una finestra per ogni cartella aperta da fuori": decide la finestra principale (onCopyData), che
// può essere un altro processo; lo sanno subito tutte le finestre di [z]f, anche quelle di prova
static void separateWindowsToggle() {
    A.separateWindows = !A.separateWindows;
    settingsSave();
    for (HWND w = nullptr; (w = FindWindowExW(nullptr, w, ZF_ID, nullptr)) != nullptr;)
        if (w != A.hwnd) PostMessageW(w, g_msgSeparate, A.separateWindows ? 1 : 0, 0);
}

static void optionsExec(int id) {
    switch (id) {
    case OPT_CLOSE:      optionsClose(true); break; // optionsExec viene solo dal mouse
    case OPT_FONT_MINUS: fontSizeStep(-1); break;
    case OPT_FONT_PLUS:  fontSizeStep(1); break;
    case OPT_FACE_PREV:  optionsFace(-1); break;
    case OPT_FACE_NEXT:  optionsFace(1); break;
    case OPT_ANIM:       A.animations = !A.animations; settingsSave(); break;
    case OPT_SIDEBAR:    sidebarToggle(); break;
    case OPT_PREVIEW:    previewToggle(); break;
    case OPT_PREVIEW_POS_PREV: case OPT_PREVIEW_POS_NEXT: previewPlace(!A.prevBottom); break; // due posizioni: ‹ e › si alternano
    case OPT_MONO_PREV:  optionsMonoFace(-1); break;
    case OPT_MONO_NEXT:  optionsMonoFace(1); break;
    case OPT_DEFAULT:    toggleDefault(); break; // lo stato si rilegge all'esito (WM_APP_DEFAULT)
    case OPT_UNDO_PERSIST: undoPersistToggle(); break;
    case OPT_SHELL_MENU: A.shellMenu = !A.shellMenu; settingsSave(); break;
    case OPT_DIR_SIZES: // filtro o gruppi per Dimensione: le cartelle passano tra "Cartelle" e la fascia
        A.dirSizes = !A.dirSizes; // del totale; la vista prima della coda, che segue il suo ordine
        settingsSave();
        if (viewUsesDirTotals()) refilterQuiet();
        dirSizeQueueCurrent();
        break;
    case OPT_EXT:        A.showExt = !A.showExt; settingsSave(); break;
    case OPT_DIALOGS:    A.dialogJump = !A.dialogJump; settingsSave(); break;
    case OPT_TIMES:      A.timesBar = !A.timesBar; settingsSave(); break;
    case OPT_REOPEN:     A.reopenTabs = !A.reopenTabs; settingsSave(); break;
    case OPT_SEPARATE:   separateWindowsToggle(); break;
    case OPT_UPDATES:    A.updAuto = !A.updAuto; settingsSave(); break;
    case OPT_BETA:       A.updBeta = updBeta() ? 0 : 1; settingsSave(); break;
    case OPT_UPD_ACT:    updAction(); break; // controlla, aggiorna, o apre la Release
    case OPT_VERSION:    updOpenPage(kUpdThisRelease); break; // novità e download di questa versione
    case OPT_FOLDER: {
        std::wstring p = zfDataPath(L"");
        if (!p.empty()) p.pop_back(); // senza "\" finale
        optionsClose(true);
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
// again: secondo clic di un doppio clic. Conta come un clic, tranne su versione, aggiornamento e
// posizione dell'anteprima: aprirebbero la pagina due volte, ripeterebbero l'aggiornamento o
// rimetterebbero l'anteprima dov'era. Se il primo clic le ha chiuse (fuori, ×), il secondo non va
// a ciò che c'è sotto: aprirebbe il file della lista in quel punto
static bool optionsMouseDown(int x, int y, bool again = false) {
    if (!OP.open) return again && nowMs() - OP.closedAt < GetDoubleClickTime();
    if (x < OP.x || x >= OP.x + OP.w || y < OP.y || y >= OP.y + OP.h) { optionsClose(true); return true; } // clic fuori
    int id = optionsHit(x, y);
    if (again && (id == OPT_VERSION || id == OPT_UPD_ACT || id == OPT_PREVIEW_POS_PREV || id == OPT_PREVIEW_POS_NEXT)) return true;
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
    } else { // in cima solo se la ricerca cambia: Shift da solo, ← → o Alt+Shift+P non spostano il pannello
        std::wstring before = OP.search.text;
        OP.search.key(A.hwnd, vk, ctrl, shift);
        if (OP.search.text != before) OP.scroll = 0;
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

static bool optionsIsOpen() { return OP.open; }

// --- tooltip: una scorciatoia troncata, con il testo intero ---------------------------------
static bool optionsTipTarget(int x, int y, int& index) {
    for (const OptionsPanel::Hit& h : OP.tips)
        if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) { index = h.id; return true; }
    return false;
}

static bool optionsTipBuild(int index) {
    if (index >= OPT_TIP_ROW) { // una riga: l'etichetta intera e i suoi tasti, se ne ha (DESIGN §8, Tooltip)
        size_t r = (size_t)(index - OPT_TIP_ROW);
        if (!OP.open || r >= OP.rowTips.size()) return false;
        const OptionsPanel::RowTip& t = OP.rowTips[r];
        T.title = t.label;
        if (t.cmd) tipRow(L"Scorciatoia", shortcutsOf(t.cmd));
        if (t.cmd2) tipRow(L"Predefinita", shortcutsOf(t.cmd2));
        return true;
    }
    int g = index / 256, i = index % 256;
    if (!OP.open || g < 0 || g >= (int)(sizeof(kShortcuts) / sizeof(kShortcuts[0])) || i >= kShortcuts[g].n) return false;
    const Shortcut& s = kShortcuts[g].items[i];
    shortcutsReady();
    T.title = s.what;
    // Un'alternativa per riga: insieme non entrerebbero nel tooltip ("Alt+← · tasto Indietro del
    // mouse o della tastiera · gesto Indietro del touchpad")
    for (const ShortcutKey& k : s.keys)
        if (k.text) tipRow(L"", k.text);
    return true;
}

// --- disegno ---------------------------------------------------------------------------
// Dove needle compare in hay, senza badare a maiuscole e minuscole; -1 se non c'è (0 se needle è vuoto)
static int findCI(const wchar_t* hay, const std::wstring& needle) {
    if (needle.empty()) return 0;
    std::wstring h = hay, n = needle;
    CharLowerBuffW(&h[0], (DWORD)h.size());
    CharLowerBuffW(&n[0], (DWORD)n.size());
    size_t p = h.find(n);
    return p == std::wstring::npos ? -1 : (int)p;
}

static void drawOptions(double dt) {
    if (!OP.open) return;
    float W = (float)G.w, H = (float)G.h;
    PanelEnter e = panelEnter(OP.openedAt, ui::DUR_MENU);
    float k = e.k;
    OP.hits.clear();
    OP.tips.clear();
    OP.rowTips.clear();
    hoverFadeUpdate(OP.fade, OP.hover, dt);
    auto fixed = [](int id) { return id == OPT_CLOSE || id == OPT_FOLDER || id == OPT_UPD_ACT || id == OPT_VERSION; }; // titolo e piede non scorrono
    // Solo la parte dentro l'area visibile: cliccabile, e col fondo sotto il mouse (il pannello non
    // usa il ritaglio, clipBegin: il fondo di una riga a metà finirebbe sopra titolo o piede)
    auto clip = [&](float& y0, float& y1, int id) {
        if (fixed(id)) return;
        y0 = std::max(y0, OP.viewTop);
        y1 = std::min(y1, OP.viewBottom);
    };
    auto hit = [&](float x, float y, float w, float h, int id) {
        float y0 = y, y1 = y + h;
        clip(y0, y1, id);
        if (y1 > y0) OP.hits.push_back({ x, y0, w, y1 - y0, id });
    };
    auto hov = [&](float x, float y, float w, float h, int id) {
        float a = hoverAlpha(OP.fade, id), y0 = y, y1 = y + h;
        clip(y0, y1, id);
        if (a > 0 && y1 > y0) rrect(x, y0, w, y1 - y0, px(ui::R_S), fadeA(C_HOVER_PANEL, a * k));
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
    // Con una versione nuova (il pallino su ⚙) il testo dice cosa fa il clic ("Aggiorna a [z]f …")
    // e si clicca tutto, non solo l'icona: è il solo posto da cui si aggiorna. Altrimenti la
    // versione è un collegamento alla sua Release (novità e download) e l'icona controlla di nuovo
    bool upd = updBadge();
    std::wstring info = updMenuLabel();
    float infoEnd = x + w * 0.6f - updIconW() - px(ui::GAP_M);
    {
        float tx = x + pad, tEnd = std::min(infoEnd, tx + (float)textWidth(info.c_str(), (int)info.size()));
        float iw = updIconW(), ix = tEnd + px(ui::GAP_M);
        float iy = fy + floorf((footH - A.rowH) * 0.5f);
        float ihx = ix - px(ui::GAP_S), ihw = iw + px(ui::GAP_M); // l'icona, con GAP_S per lato
        float thx = tx - px(ui::GAP_S), thw = tEnd + px(ui::GAP_S) - thx; // il testo, con GAP_S per lato
        if (upd) { ihw += ihx - thx; ihx = thx; } // un solo controllo: testo e icona
        else {
            hov(thx, iy, thw, A.rowH, OPT_VERSION);
            hit(thx, iy, thw, A.rowH, OPT_VERSION);
        }
        hov(ihx, iy, ihw, A.rowH, OPT_UPD_ACT);
        drawText(tx, fy + floorf((footH - F.lineH) * 0.5f), info, fadeA(upd ? C_TEXT : C_DIM, k), infoEnd);
        drawUpdateIcon(ix, iy, iw, A.rowH, k);
        hit(ihx, iy, ihw, A.rowH, OPT_UPD_ACT);
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
    // Il fondo di una riga va da rowL a right; dentro, etichetta e controllo stanno a GAP_M dai
    // bordi (inL, inR), come il testo nelle voci di un menu
    float rowL = x + pad, inL = rowL + px(ui::GAP_M), inR = right - px(ui::GAP_M);
    float lh = (float)F.lineH + px(ui::LINE_GAP); // interlinea delle spiegazioni su più righe
    // Solo righe intere: una riga a metà finirebbe sopra titolo o piede (senza ritaglio)
    auto visible = [&](float yy, float hh) { return yy >= OP.viewTop && yy + hh <= OP.viewBottom; };
    auto section = [&](const wchar_t* t) {
        if (visible(cy, rowH)) drawText(rowL, cy + textOff + px(ui::GAP_XS), t, (int)wcslen(t), fadeA(C_DIM, k));
        cy += rowH;
    };
    // L'etichetta di una riga finisce a GAP_M dal suo controllo (ctlX). Il tooltip della riga ha
    // l'etichetta intera, se è troncata, e la scorciatoia (cmd, da kShortcuts), se c'è (DESIGN §8, Tooltip)
    auto label = [&](const wchar_t* t, float ctlX, int cmd, int cmd2) {
        if (!visible(cy, rowH)) return;
        int n = (int)wcslen(t);
        float maxX = ctlX - px(ui::GAP_M);
        drawText(inL, cy + textOff, t, n, fadeA(C_TEXT, k), maxX);
        if (!cmd && floorf(inL) + (float)textWidth(t, n) <= maxX) return; // lo stesso confronto di drawText
        OP.tips.push_back({ rowL, cy, right - rowL, rowH, OPT_TIP_ROW + (int)OP.rowTips.size() });
        OP.rowTips.push_back({ t, cmd, cmd2 });
    };
    auto button = [&](float bx, float by, float bw, const wchar_t* t, int id) {
        if (!visible(by, rowH)) return;
        hov(bx, by + px(ui::GAP_XS), bw, rowH - px(ui::GAP_S), id);
        drawText(bx + floorf((bw - textWidth(t, (int)wcslen(t))) * 0.5f), by + textOff, t, (int)wcslen(t), fadeA(C_DIM, k));
        hit(bx, by, bw, rowH, id);
    };
    // Riga con un interruttore: etichetta, spiegazione sotto (a capo dove non entra: DESIGN §4,
    // niente informazioni troncate) e interruttore a destra. Tutta la riga è cliccabile e prende il
    // fondo sotto il mouse. hintCol: C_ACCENT quando la spiegazione avvisa di uno stato a metà.
    std::vector<std::pair<int, int>> hl; // righe della spiegazione: inizio, fine
    auto opt = [&](const wchar_t* t, const wchar_t* hint, bool on, float& anim, int id, uint32_t hintCol = C_DIM, int cmd = 0) {
        hl.clear();
        int hn = hint ? (int)wcslen(hint) : 0;
        for (int s = 0; s < hn;) {
            int e2 = textLineEnd(hint, hn, s, inR - inL);
            hl.push_back({ s, e2 });
            for (s = e2; s < hn && hint[s] == L' '; s++) {}
        }
        float rh = hl.empty() ? rowH : rowH * 2 - px(ui::GAP_S) + lh * (float)(hl.size() - 1);
        hov(rowL, cy, right - rowL, rh, id);
        label(t, inR - px(ui::SWITCH_W), cmd, 0);
        float hy = cy + rowH - px(ui::GAP_S);
        for (auto& l : hl) {
            if (visible(hy, rowH)) drawText(inL, hy + textOff, hint + l.first, l.second - l.first, fadeA(hintCol, k), inR);
            hy += lh;
        }
        // Interruttore: pillola come in Windows 11, spento contorno, acceso pieno
        anim = approach(anim, on ? 1.0f : 0.0f, dt, ui::TAU_TAB_LINE, 0.01f);
        float tw = px(ui::SWITCH_W), th = px(ui::SWITCH_H), tx = inR - tw, ty = cy + floorf((rowH - th) * 0.5f);
        if (visible(cy, rowH)) {
            rrect(tx, ty, tw, th, th * 0.5f, fadeA(C_DIM, k * (1 - anim)), std::max(1.0f, floorf(A.scale)));
            if (anim > 0) rrect(tx, ty, tw, th, th * 0.5f, fadeA(C_ACCENT, k * anim));
            float in = px(ui::SWITCH_KNOB_INSET), kn = th - in * 2, kx = tx + in + floorf((tw - kn - in * 2) * anim);
            uint32_t kc = anim > 0.5f ? C_KNOB_ON : C_DIM;
            rrect(kx, ty + floorf((th - kn) * 0.5f), kn, kn, kn * 0.5f, fadeA(kc, k));
        }
        hit(rowL, cy, right - rowL, rh, id);
        cy += rh;
    };
    // Selettore tra alcuni valori: "−  13 px  +", "‹  Segoe UI  ›". Su un fondo da casella
    // (C_INPUT), così si vede che il valore si cambia; i pulsanti hanno il loro fondo sotto il mouse
    auto pickRow = [&](const wchar_t* t, const std::wstring& val, float vw, const wchar_t* prev, const wchar_t* next,
                       int prevId, int nextId, int cmd = 0, int cmd2 = 0) {
        float bw = rowH, bx = inR - bw * 2 - vw;
        label(t, bx, cmd, cmd2);
        if (visible(cy, rowH)) rrect(bx, cy + px(ui::GAP_XS), bw * 2 + vw, rowH - px(ui::GAP_S), px(ui::R_S), fadeA(C_INPUT, k));
        button(bx, cy, bw, prev, prevId);
        if (visible(cy, rowH)) {
            float tw = (float)textWidth(val.c_str(), (int)val.size());
            drawText(bx + bw + std::max(0.0f, floorf((vw - tw) * 0.5f)), cy + textOff, val, fadeA(C_TEXT, k), bx + bw + vw);
        }
        button(bx + bw + vw, cy, bw, next, nextId);
        cy += rowH;
    };
    auto namePick = [&](const wchar_t* t, const std::wstring& val, int prevId, int nextId, int cmd = 0) { // tra nomi: caratteri, posizioni
        pickRow(t, val, px(ui::OPT_FACE_W), L"\x2039", L"\x203A", prevId, nextId, cmd);
    };

    // Con la ricerca piena solo le scorciatoie: la casella sta nel loro titolo, così casella e
    // risultati sono in cima e non in fondo, fuori vista
    bool searching = !OP.search.text.empty();
    if (!searching) {
        section(L"Aspetto");
        wchar_t b[16];
        _snwprintf(b, 15, L"%d px", F.px);
        b[15] = 0;
        pickRow(L"Dimensione del testo", b, px(ui::OPT_VALUE_W), L"\x2212", L"+", OPT_FONT_MINUS, OPT_FONT_PLUS, M_FONT_STEP, M_FONT_RESET);
        namePick(L"Carattere", F.face, OPT_FACE_PREV, OPT_FACE_NEXT);
        opt(L"Animazioni", nullptr, A.animations, OP.knobAnim, OPT_ANIM);
        cy += px(ui::GAP_M);

        section(L"Finestra");
        opt(L"Barra laterale", nullptr, A.sideVisible, OP.knobSide, OPT_SIDEBAR, C_DIM, M_SIDEBAR);
        opt(L"Anteprima", nullptr, A.prevOpen, OP.knobPreview, OPT_PREVIEW, C_DIM, M_PREVIEW);
        namePick(L"Posizione dell'anteprima", A.prevBottom ? L"In basso" : L"A destra", OPT_PREVIEW_POS_PREV, OPT_PREVIEW_POS_NEXT, M_PREVIEW_SWAP);
        namePick(L"Carattere dell'anteprima", FM.face, OPT_MONO_PREV, OPT_MONO_NEXT);
        cy += px(ui::GAP_M);

        section(L"Integrazione con Windows");
        // Negli stati a metà l'interruttore è spento (non fa tutto) e la spiegazione, in C_ACCENT
        // (informazione), dice cosa c'è e cosa fa il clic. La build di prova non lo cambia (kDefaultLocked)
        opt(L"[z]f al posto di Explorer",
            kDefaultLocked ? L"Nella build di prova non cambia il registro: lo cambia z-f.exe"
            : OP.defaultState == 2 ? L"Attivo con un altro eseguibile di [z]f: un clic lo assegna a questo"
            : OP.defaultState == 3 ? L"Attivo senza gli zip: un clic li aggiunge"
                                   : L"Doppio clic sulle cartelle e sugli zip, Win+E, programmi che aprono una cartella",
            OP.defaultState == 1, OP.knobDefault, OPT_DEFAULT, kDefaultLocked || OP.defaultState >= 2 ? C_ACCENT : C_DIM);
        opt(L"Una finestra per ogni cartella aperta da fuori",
            L"Invece di una scheda nella finestra principale. Una finestra occupa da 40 a 70 MB, una scheda meno di 1 MB",
            A.separateWindows, OP.knobSeparate, OPT_SEPARATE);
        opt(L"Menu completo di Windows col tasto destro",
            L"Al posto del menu di [z]f; Shift+tasto destro apre quello di [z]f. Si apre in 200\x2013" L"400 ms",
            A.shellMenu, OP.knobMenu, OPT_SHELL_MENU);
        opt(L"Dialoghi Apri e Salva nella cartella di [z]f",
            L"Quando si aprono, e quando si torna al dialogo dopo aver cambiato cartella in [z]f",
            A.dialogJump, OP.knobDialogs, OPT_DIALOGS);
        cy += px(ui::GAP_M);

        section(L"File");
        opt(L"Annullamento anche dopo il riavvio", L"Ctrl+Z ricorda le operazioni anche dopo la chiusura",
            A.undoPersist, OP.knobUndo, OPT_UNDO_PERSIST);
        opt(L"Dimensione delle cartelle nella colonna",
            L"Contata in background, a priorità bassa; unità di rete e ottiche solo nel tooltip e nell'anteprima",
            A.dirSizes, OP.knobDirSizes, OPT_DIR_SIZES);
        opt(L"Estensioni dei file nei nomi",
            L"Spente, l'estensione resta nella colonna Tipo, nel tooltip e nella rinomina",
            A.showExt, OP.knobExt, OPT_EXT);
        cy += px(ui::GAP_M);

        section(L"Avvio e aggiornamenti");
        opt(L"Schede riaperte all'avvio", L"Le schede della finestra principale, com'erano alla chiusura",
            A.reopenTabs, OP.knobReopen, OPT_REOPEN);
        std::wstring last = updLastText();
        std::wstring hint = L"Una volta al giorno da GitHub" + (last.empty() ? std::wstring() : L" \x00B7 ultimo controllo " + last);
        opt(L"Aggiornamenti automatici", hint.c_str(), A.updAuto, OP.knobUpdates, OPT_UPDATES);
        opt(L"Versioni beta", L"Le versioni di prova, prima delle stabili", updBeta(), OP.knobBeta, OPT_BETA);
        cy += px(ui::GAP_M);
    }

    // Scorciatoie, con la ricerca accanto al titolo
    float sTop = cy;
    section(L"Scorciatoie");
    if (visible(sTop, rowH)) {
        float fw = px(ui::OPT_SEARCH_W), fh = A.rowH;
        float fx = inR - fw, fyy = sTop + floorf((rowH - fh) * 0.5f);
        drawField(OP.search, fx, fyy, fw, fh, A.scale, L"Cerca\x2026", k);
    }
    float keysW = 0;
    for (const ShortcutGroup& g : kShortcuts)
        for (int i = 0; i < g.n; i++) {
            std::wstring t = shortcutText(g.items[i]);
            keysW = std::max(keysW, (float)textWidth(t.c_str(), (int)t.size()));
        }
    keysW = std::min(keysW, (inR - inL) * 0.5f);
    const std::wstring& q = OP.search.text;
    auto matches = [&](const Shortcut& s, const std::wstring& keys) {
        return findCI(s.what, q) >= 0 || findCI(keys.c_str(), q) >= 0;
    };
    // Lettere trovate in C_ACCENT, come in "Vai a" (drawTextMarked)
    std::vector<uint8_t> mark;
    auto marked = [&](float tx, float ty, const wchar_t* t, int n, uint32_t col, float maxX) {
        int at = searching ? findCI(t, q) : -1;
        mark.assign((size_t)n + 1, 0);
        if (at >= 0) std::fill(mark.begin() + at, mark.begin() + std::min(n, at + (int)q.size()), (uint8_t)1);
        return drawTextMarked(tx, ty, t, n, fadeA(col, k), maxX, mark.data(), fadeA(C_ACCENT, k));
    };
    float keyOff = floorf((A.rowH - F.lineH) * 0.5f);
    float whatX = inL + px(ui::GAP_M), whatEnd = inR - keysW - px(ui::GAP_L);
    int shown = 0;
    for (int gi = 0; gi < (int)(sizeof(kShortcuts) / sizeof(kShortcuts[0])); gi++) {
        const ShortcutGroup& g = kShortcuts[gi];
        bool any = false;
        for (int i = 0; i < g.n && !any; i++) any = matches(g.items[i], shortcutText(g.items[i]));
        if (!any) continue;
        if (visible(cy, A.rowH)) drawText(inL, cy + keyOff, g.title, (int)wcslen(g.title), fadeA(C_DIM, k));
        cy += A.rowH;
        for (int i = 0; i < g.n; i++) {
            const Shortcut& s = g.items[i];
            std::wstring keys = shortcutText(s);
            if (!matches(s, keys)) continue;
            if (visible(cy, A.rowH)) {
                int wn = (int)wcslen(s.what);
                marked(whatX, cy + keyOff, s.what, wn, C_TEXT, whatEnd);
                marked(inR - keysW, cy + keyOff, keys.c_str(), (int)keys.size(), C_DIM, inR);
                // Troncata (descrizione o tasti): il tooltip ha il testo intero (DESIGN §4)
                if (whatX + textWidth(s.what, wn) > whatEnd || textWidth(keys.c_str(), (int)keys.size()) > keysW)
                    OP.tips.push_back({ rowL, cy, right - rowL, A.rowH, gi * 256 + i });
            }
            cy += A.rowH;
            shown++;
        }
        cy += px(ui::GAP_S);
    }
    if (!shown) {
        if (visible(cy, A.rowH)) drawText(inL, cy + keyOff, L"Nessuna scorciatoia trovata", 27, fadeA(C_DIM, k));
        cy += A.rowH;
    }
    cy += px(ui::GAP_M);

    // Diagnostica dopo tutto il resto: serve a chi misura, non all'uso di ogni giorno
    if (!searching) {
        section(L"Diagnostica");
        opt(L"Barra dei tempi", L"Sopra la barra di stato: lettura, ordinamento, frame, avvio. F12 per tutti i dettagli",
            A.timesBar, OP.knobTimes, OPT_TIMES);
        cy += px(ui::GAP_M);
    }

    // Scorrimento: quanto contenuto c'è oltre l'area visibile, e la barra dei pannelli che lo dice
    float viewH = OP.viewBottom - OP.viewTop;
    float contentH = cy + floorf(OP.scroll) - OP.viewTop;
    OP.maxScroll = std::max(0.0f, contentH - viewH);
    if (OP.scroll > OP.maxScroll) { OP.scroll = OP.maxScroll; A.dirty = true; }
    drawPanelScroll(x + w, OP.viewTop + px(ui::GAP_S), viewH - px(ui::GAP_S) * 2,
                    (int)OP.scroll, (int)viewH, (int)contentH, k);
}
