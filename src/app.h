// [z]f — stato dell'applicazione e dichiarazioni condivise tra i moduli.

// Comandi delle voci di menu (menu.cpp). Le scorciatoie accanto alle voci, e nei tooltip, vengono
// da kShortcuts (options.cpp), che le lega a questi id: un id è un comando, quindi due voci che
// mostrano tasti diversi hanno id diversi.
enum MenuId { M_OPEN = 1, M_OPENWITH, M_CUT, M_COPY, M_PASTE, M_COPYPATH, M_RENAME, M_DELETE, M_PROPS, M_MORE,
              M_NEWFOLDER, M_REFRESH, M_TERMINAL,
              M_NEWTAB,      // cartella selezionata, o voce della barra laterale, in una nuova scheda
              M_FAV_TOGGLE,  // cartella corrente nei preferiti (Ctrl+D)
              M_FAV_SEL,     // cartella selezionata nei preferiti
              M_SIDE_OPEN, M_SIDE_UP, M_SIDE_DOWN, M_SIDE_REMOVE, M_SIDE_PROPS,
              M_TAB_NEW, M_TAB_CLOSE, M_TAB_OTHERS, M_SET_DEFAULT, M_OPTIONS, M_NEW_WINDOW,
              M_VERSION,     // riga della versione con lo stato dell'aggiornamento: controlla, aggiorna (update.cpp)
              M_DROP_COPY, M_DROP_MOVE, M_DROP_LINK, M_DROP_CANCEL, // trascinamento col tasto destro
              M_GOTO,        // "Vai a" (goto.cpp)
              M_PREVIEW,     // pannello di anteprima (preview.cpp)
              M_UNDO, M_REDO, // annulla, ripeti (fileops.cpp)
              M_GRID_S, M_GRID_M, M_GRID_L, M_VIEW_LIST, // vista a griglia (miniature piccole, medie, grandi) o elenco
              M_NET_CONNECT, M_NET_DISCONNECT, M_NET_PLACE, // unità e percorsi di rete (sidebar.cpp)
              M_LINK_OPEN,   // collegamento: cartella della destinazione, con la destinazione selezionata (links.cpp)
              // Comandi senza voce di menu: legano i loro tasti in kShortcuts ai tooltip
              M_BACK, M_FWD, M_UP, M_EDIT_PATH, M_SIDEBAR, M_FILTER_CLEAR, M_GOTO_NEWTAB, M_GOTO_ENTER };

// "Questo PC" per la shell: è una cartella virtuale, la si raggiunge col suo CLSID
static const wchar_t kThisPc[] = L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}";

// Menu contestuale nostro: disegnato come il resto dell'interfaccia, si apre in un frame
// id 0 = separatore. icon: 0 nessuna, 1 cartella, 2 unità (path in PopupMenu::paths). dim: elemento nascosto
struct MenuItem { const wchar_t* label; const wchar_t* keys; int id; bool enabled; int icon = 0; bool dim = false; };
enum { MENU_LIST, MENU_SIDE, MENU_TAB, MENU_CRUMB, MENU_DROP };
struct PopupMenu {
    bool open = false, background = false, pressed = false;
    float x = 0, y = 0, w = 0, h = 0, ih = 0, sepH = 0, pad = 0, vpad = 0;
    int hot = -1;
    POINT screenPt = {}; // per "Altre opzioni…"
    int kind = 0;        // MENU_LIST, MENU_SIDE, MENU_TAB
    double openedAt = 0; // per la dissolvenza d'apertura
    int target = -1;     // voce della barra laterale o scheda a cui si riferisce
    std::vector<MenuItem> items;
    // Menu con voci dinamiche (sottocartelle): le etichette vivono qui (deque: indirizzi stabili)
    std::deque<std::wstring> labels;
    std::vector<std::wstring> paths; // percorso di ogni voce, stesso indice di items
    // Scorrimento, se le voci non stanno nella finestra (solo menu senza separatori)
    int first = 0, visible = 0;
    float iconW = 0;
};

// Stato di una scheda. Quello della scheda attiva vive direttamente in App (il codice della vista
// non sa nulla delle schede); cambiare scheda scambia i campi: O(1), nessuna copia.
static uint32_t g_tabIds = 0; // thread UI
struct TabState {
    // Chi è la scheda, anche quando chiudendone una prima cambia il suo indice (l'esito di un lavoro
    // la ritrova così). Ogni scheda costruita ne ha uno nuovo; spostarla lo conserva.
    uint32_t id = ++g_tabIds;
    Listing* cur = nullptr;
    std::vector<uint32_t> order, view;
    std::vector<uint8_t> selFlags;
    int sel = 0, anchor = -1;
    float scrollY = 0, scrollTarget = 0;
    std::wstring filter;
    std::vector<std::wstring> back, fwd;
    int sortCol = COL_NAME;
    bool sortDesc = false;
    bool grid = false;
    // Da caricare quando diventa attiva: scheda aperta in background, o lasciata a metà caricamento
    bool pending = false;
    std::wstring pendingPath; // "" = Questo PC
    std::wstring pendingSelect; // voce da selezionare all'arrivo
    int pendingHistory = 0;
};

// Voce della barra laterale (preferito, unità o percorso di rete). Unità e percorsi di rete stanno
// insieme in A.drives, sotto Questo PC: le unità hanno drive, i percorsi di rete place.
struct SideItem {
    std::wstring label, path;
    std::wstring fs;              // file system dell'unità (NTFS, exFAT, ...)
    std::wstring unc;             // unità o percorso di rete: \\server\condivisione
    bool drive = false;
    bool place = false;           // percorso di rete di Windows (netPlacesRead): path è dove porta
    UINT driveType = 0;
    uint64_t total = 0, free = 0;
};

// Evidenziazione al passaggio del mouse che sfuma: entra la voce nuova, esce la precedente
struct HoverFade { int cur = -1, prev = -1; float a = 0, pa = 0; };

struct App {
    HWND hwnd = nullptr;      // finestra principale (riceve tutto l'input)
    PopupMenu menu;
    bool renameAfterRefresh = false; // nuova cartella: rinomina appena compare
    bool errorIsInfo = false;        // il messaggio in basso è un'informazione, non un errore
    HWND viewWnd = nullptr;   // finestra figlia su cui disegna la swapchain corrente
    HINSTANCE inst = nullptr;
    bool switched = false;    // ibrido: già passati alla GPU?
    bool gpuLost = false;     // device perso (driver reimpostato): non si disegna finché non c'è il nuovo
    bool firstGpuPresent = false; // il prossimo Present è il primo su una swapchain nuova della GPU (log)
    int gpuRecoveries = 0;    // device ricreati dopo una perdita (F12)
    double gpuReadyAt = 0, lastInputAt = 0;
    bool inSizeMove = false;  // la finestra si sta spostando o ridimensionando
    double warpDevMs = 0, warpSwapMs = 0, switchAt = 0, switchCost = 0;
    float scale = 1.0f;

    Listing* cur = nullptr;
    std::vector<uint32_t> order; // ordinamento corrente
    std::vector<uint32_t> view;  // dopo il filtro
    int sel = 0, hover = -1;          // sel = cursore (indice nella vista)
    std::vector<uint8_t> selFlags;    // selezione, per indice di voce: resiste a ordinamento, filtro e ricariche
    int anchor = -1;                  // voce da cui partono gli intervalli con Shift (indice di voce)
    float scrollY = 0, scrollTarget = 0;
    bool draggingThumb = false;
    float dragOffset = 0;

    std::wstring filter;
    bool renaming = false;
    TextField renameField;
    int renameEntry = -1;
    std::wstring selectAfterRefresh; // dopo una rinomina: dove mettere il cursore
    double menuMs = 0, ourMenuMs = 0;
    int menuItems = 0;
    double verbMs = 0;   // comando della shell senza menu (Shift+Canc, Proprietà): preparazione sul thread
    int verbItems = 0;

    std::vector<std::wstring> back, fwd;
    uint64_t gen = 0;
    bool loading = false;
    double loadStart = 0;
    std::wstring loadPath;       // cartella in caricamento (navigazione, non ricarica)
    int pendingHistory = 0;      // 0 = nessuna, 1 = push su back, 2 = da back, 3 = da forward
    // Aggiornamento in tempo reale
    uint64_t watchGen = 0;
    bool watching = false, changePending = false, refreshInFlight = false, refreshAgain = false;
    double changeAt = 0, refreshStart = 0, refreshLatency = 0;
    int refreshCount = 0, notifyCount = 0;
    std::wstring pendingSelect;

    int sortCol = COL_NAME;
    bool sortDesc = false;

    std::wstring error;
    double sortMs = 0, filterMs = 0, frameMs = 0, startupMs = 0;
    double execMs = 0, presentMs = 0, execMax = 0; // misurati solo con F12 aperto
    double gpuMs = 0, gpuMax = 0, prepMax = 0, presentMax = 0;
    double slowMsgMs = 0, waitMax = 0; UINT slowMsg = 0; int slowMsgCount = 0;
    bool showStats = false;       // F12
    bool timesBar = false;        // barra dei tempi sopra la barra di stato (opzione, barra_tempi)
    double statsOpenedAt = 0;     // per la dissolvenza d'apertura del pannello F12
    double dropMs = 0, dropRegisterMs = 0; // trascinamento: ultimo rilascio (sul thread), registrazione OLE
    double dropUiMax = 0;         // trascinamento: chiamata più lenta sul thread dell'interfaccia
    // "Vai a" (F12): apertura, ricerca a ogni tasto, lettura delle sottocartelle sul thread
    double gotoOpenMs = 0, gotoSearchMs = 0, gotoSearchMax = 0, gotoReadMs = 0;
    int gotoCands = 0, gotoReadDirs = 0;
    // Avvio: istanti misurati dalla creazione del processo (ms)
    double stMain = 0, stWindow = 0, stGfx = 0, stFont = 0, stWait = 0, stFirst = 0;
    double stActivate = -1;      // attivazione dopo il primo frame (primo fuoco: Windows avvia il Text Input Framework)
    double iconMs = -1;          // ultima icona della finestra caricata (appIconSet: dopo il primo frame, cambio di DPI)
    bool firstFrameDone = false;
    bool dirty = true;
    double lastFrame = 0;
    bool trackingMouse = false;

    // Animazioni: brevi (≈ 60–150 ms) e solo con uno scopo. Disattivabili (animazioni=0).
    bool animations = true;
    bool animActive = false;          // qualcosa si sta ancora muovendo: serve un altro frame
    bool prevAnimating = false;
    HoverFade rowFade, sideFade, tabFade;
    HoverFade tabBtnFade, fltFade; // + e × delle schede (0 = +, i + 1 = × della scheda i), × del filtro (0)
    float cursorX = 0, cursorY = 0;   // cursore che scivola tra le voci (coordinate del contenuto)
    const void* cursorFor = nullptr;  // con una cartella nuova il cursore salta, non scivola
    float listFade = 1;               // lista nuova che entra in dissolvenza
    float tabLineX = -1, tabLineW = 0;// indicatore della scheda attiva che scorre
    int wheelAcc = 0;                 // Ctrl+rotellina: rotelline ad alta risoluzione

    // Schede: tabs[tab] è un guscio vuoto, lo stato vivo della scheda attiva è nei campi sopra
    std::vector<TabState> tabs = std::vector<TabState>(1);
    int tab = 0, tabHover = -1;
    int tabOpenAfter = -1;            // ultima scheda aperta in background da quella attiva
    bool tabCloseHover = false, tabPlusHover = false;

    // Barra laterale
    bool sideVisible = true;
    float sideWidth = ui::SIDE_W;     // larghezza scelta (px a scala 100%, in impostazioni.txt)
    std::vector<SideItem> favs, drives;
    int sideHover = -1;               // come sideHit: favs, Questo PC, drives; -3 titolo Preferiti, -2 freccia di Questo PC
    bool favOpen = true, drvOpen = true; // barra laterale aperta: preferiti, ramo delle unità sotto Questo PC
    // Preferiti col mouse (sidebar.cpp): premuto (al rilascio ci va; trascinato cambia posto),
    // punto di partenza, posizione d'inserimento durante il trascinamento (0…n, -1 se non si
    // trascina); cestino sotto il mouse e cestino già cliccato una volta (il secondo clic toglie)
    int favPress = -1, favPressX = 0, favPressY = 0, favDropAt = -1;
    int favTrashHover = -1, favTrashArmed = -1;
    float sideScroll = 0;
    uint64_t sideGen = 0;
    double sideLoadedAt = -1e9;
    double sideMs = 0, sideSlowMs = 0; // F12: lettura delle voci locali, ultimo giro di quelle lente (thread)
    int sideSlowN = 0;                 // F12: voci lente lette nell'ultimo giro
    bool favsReady = false;          // preferiti letti dal file (o creati al primo avvio)
    bool sideFastDone = false;        // prima lettura veloce (file e lettere delle unità) arrivata
    bool sideStarted = false;         // il thread della barra parte solo dopo il primo frame

    // Colonne: larghezze scelte (px a scala 100%, in impostazioni.txt) e bordo afferrato
    float colDate = ui::DATE_W, colSize = ui::SIZE_W, colLink = ui::LINK_W;
    int colHover = -1, colDrag = -1;  // bordo sotto il mouse / trascinato (0 = …|Data, 1 = Data|Dimensione, 2 = Nome|Destinazione; EDGE_SIDE, EDGE_PREVIEW: bordi dei pannelli)
    // Destinazione dei collegamenti (links.cpp): sotto il mouse (indice di vista), premuta col tasto
    // sinistro (vale al rilascio, se non è partito il rettangolo), ultimo clic (il doppio clic che
    // segue non apre la voce che intanto è arrivata sotto il mouse)
    int linkHover = -1, linkPress = -1;
    const void* linkPressFor = nullptr;
    double linkClickAt = -1e9;
    HoverFade linkFade;
    int colDragX0 = 0;                // trascinamento di un bordo: x di partenza…
    float colDragW0 = 0, colDragSaved = 0; // …larghezza mostrata allora e valore scelto prima (per Esc)
    double colFitMs = 0;              // doppio clic sul bordo: adattamento al contenuto (F12)
    double mqMs = 0, mqMax = 0;       // selezione col rettangolo: aggiornamento a ogni movimento (F12)
    double crumbMs = 0, crumbMax = 0; // barra del percorso: segmenti e posizioni, a ogni frame (F12)

    // Pannello di anteprima (preview.cpp)
    bool prevOpen = false;            // scelto dall'utente (Alt+P), in impostazioni.txt
    float prevWidth = ui::PREVIEW_W;  // larghezza scelta (px a scala 100%, in impostazioni.txt)
    float prevW = 0;                  // larghezza mostrata: 0 se chiuso o se la finestra è troppo stretta
    // F12: ultima anteprima (lettura e decodifica sul thread, texture sul thread UI), primo caricamento di WIC
    const wchar_t* prevKind = nullptr;
    double prevReadMs = 0, prevDecodeMs = 0, prevTexMs = 0;

    // Vista a griglia (thumbs.cpp): la vista è della cartella (viste.txt), la dimensione vale per tutte
    bool grid = false;                // cartella corrente in griglia (scambiato con le schede)
    int thumbSize = 1;                // 0 piccole, 1 medie, 2 grandi (griglia_dimensione in impostazioni.txt)
    std::unordered_map<std::wstring, uint32_t> gridFolders; // cartelle in griglia (minuscole) → ultimo uso
    uint32_t gridUse = 0;             // contatore dell'ultimo uso (si tengono le più recenti)
    bool viewsReady = false;          // viste.txt letto all'avvio
    // F12: per fonte (0 cache di Windows, 1 WIC, 2 shell, 3 icone) tempo totale e numero sul thread;
    // texture sul thread UI (ultimo frame e peggiore); memoria; prima schermata completa
    struct ThumbStats {
        double ms[4] = {}; int n[4] = {}, miss = 0, failed = 0;
        double texMs = 0, texMax = 0, firstMs = 0, atlasMs = 0; // atlasMs: creazione dell'atlante da 64 MB (UI)
        double syncMs = 0, syncMax = 0, cellsMs = 0, cellsMax = 0; // thread UI: thumbsFrame, disegno delle voci
        int inRam = 0, onGpu = 0, threads = 0;
        double ramMB = 0, vramMB = 0;
    } th;

    // Operazioni sui file e annulla (fileops.cpp)
    bool undoPersist = false;         // la cronologia sopravvive al riavvio (annulla.txt), in impostazioni.txt
    bool shellMenu = false;           // tasto destro: menu completo della shell invece del nostro (Shift inverte)
    bool dirSizes = true;             // colonna Dimensione: totale delle cartelle contato in background (dirsize.cpp)
    bool dialogJump = true;           // dialoghi Apri e Salva nella cartella di [z]f (filedlg.cpp)
    double fdHookMs = 0;               // F12: notifica più lenta dei dialoghi (UI)
    volatile LONG64 fdJumpUs = 0;      // F12: ultimo salto di un dialogo, in µs (scritto dai thread dei dialoghi)
    double fileOpMs = 0;              // F12: ultima copia, spostamento o eliminazione (sul thread)
    double undoRecMs = 0, undoMs = 0; // F12: registrazione nella cronologia (UI), ultimo annulla o ripeti (thread)
    double zipMs = 0;                 // F12: ultima estrazione da uno zip (thread, zipout.cpp)
    uint64_t zipBytes = 0;

    // Layout (pixel, aggiornato a ogni frame)
    float topH, headH, rowH, statusH, listTop, listH, sbW, padX, dateW, sizeW;
    float timesH = 0;                 // barra dei tempi (0 se spenta)
    float footH = 0;                  // sotto lista e anteprima: barra dei tempi più barra di stato
    float dateX = 0, sizeX = 0;       // inizio delle colonne Data e Dimensione
    float linkX = 0, linkW = 0;       // colonna Destinazione (linkW 0: non c'è, e linkX = dateX)
    float nameEnd = 0;                // fine del testo del nome (prima della colonna successiva)
    float iconX = 0, nameX = 0;       // icona e nome nelle righe (da render, che conosce le icone)
    float tabH, tabY, pathY, listX, sideW, sideTop, sideItemH;
    float listR = 0;                  // bordo destro della lista, barra di scorrimento compresa (dopo: l'anteprima)
    // Voci (itemRect, itemAt): in elenco una colonna alta una riga; in griglia cols voci per riga
    int cols = 1;
    float cellX = 0, cellW = 0, cellH = 0, thumbPx = 0;
    float sortX[4] = {};              // griglia: etichette di ordinamento nell'intestazione (inizi e fine)
    float btnW, capW, navW, tabStripX, tabStripW; // barra del titolo e riga del percorso
    float fltX = 0, fltW = 0, fltClearW = 0;      // campo del filtro, a sinistra nella barra di stato
    bool fltClearHover = false, countsDirty = false;
    // Avvio, riapertura delle schede e riavvio dopo un aggiornamento (tabs.cpp, main.cpp)
    bool reopenTabs = false;   // riapri_schede: la finestra principale riapre le sue schede all'avvio
    bool mainInstance = false; // finestra principale: riceve le cartelle aperte da fuori, salva schede.txt
    bool separateWindows = false; // finestre_separate: la principale rimanda le cartelle aperte da fuori a una finestra nuova
    bool startDeferred = false; // avvio senza cartella: la prima aspetta di sapere se ci sono schede da riaprire
    std::wstring startDefault; // …e se non ce ne sono si va qui (la cartella utente)
    DWORD restartPid = 0;      // avviata da un aggiornamento: le schede sono in riavvio-<pid>.txt
    bool restartOnExit = false; // aggiornamento: uscendo, [z]f si riapre con l'exe nuovo
    bool restartActive = false; // …ed era la finestra in primo piano: quella nuova lo sarà
    WINDOWPLACEMENT restartPlace = {};
    // Aggiornamento (update.cpp), in impostazioni.txt
    bool updAuto = true;       // aggiornamenti: controllo automatico una volta al giorno
    int updBeta = -1;          // aggiornamenti_beta: 1 anche le beta, 0 solo le stabili, -1 come la versione in uso
    long long updLast = 0;     // ultimo controllo riuscito, secondi Unix
    bool closePending = false; // chiusa durante un'operazione sui file: nascosta finché non finisce
    bool noticeKeep = false;   // l'avviso resta all'arrivo della prossima cartella (cartella sparita)
    double shellObjMs = 0;     // ultimo oggetto per la selezione sul thread UI (Copia, Taglia, trascinamento)
} A;

// Misura del tema (px a scala 100%) → pixel interi alla scala DPI corrente
static inline float px(float v) { return floorf(v * A.scale); }

// Primo caricamento di WIC nel processo (la DLL e i suoi codec): lo misura preview.cpp, chiunque lo
// chieda per primo (anteprima o miniature della griglia), su quel thread; lo mostra F12
static volatile double g_wicFirstMs = 0;

// Proprietà della finestra principale (istanza unica): la mette main.cpp, la toglie input.cpp alla
// chiusura, e chi avvia [z]f la cerca per consegnarle la cartella
static const wchar_t* kInstanceProp = L"z-f.istanza";
// Mutex "Local\z-f-istanza" della principale: si chiude insieme alla proprietà (input.cpp,
// WM_DESTROY), non a fine processo. Il processo che esce può metterci più di un secondo, e intanto chi
// avvia [z]f troverebbe il mutex senza la finestra e aspetterebbe FWD_WAIT per niente
static HANDLE g_instanceMutex = nullptr;
// "finestre_separate cambiata" tra le finestre di [z]f (RegisterWindowMessage, wParam il valore
// nuovo): la usa la principale, ma l'interruttore può essere in un'altra finestra
static UINT g_msgSeparate = 0;

// Funzioni usate prima della loro definizione (l'ordine dei moduli è in z-f.cpp)
static void startWatch(const std::wstring& path); // watch.cpp
static void requestRefresh();                     // watch.cpp
static void folderGone(std::wstring p);           // watch.cpp
static void beginRename();                        // shell.cpp
static void notice(const std::wstring& text, bool info); // shell.cpp: avviso nella barra di stato
enum FsOp { FS_RENAME, FS_MKDIR };                // fileops.cpp: operazioni dirette su un thread
static void fsStart(int op, const std::wstring& a, const std::wstring& b, const std::wstring& name); // fileops.cpp
static void pasteFiles(const std::wstring& dest = L""); // fileops.cpp: incolla i file degli appunti (vuoto: cartella aperta)
static bool shellDropInto(HWND hwnd, IDataObject* data, const std::wstring& path, DWORD fx, POINTL pt, HRESULT& hr); // drag.cpp: rilascio alla shell, su un thread
static const wchar_t* errWhy(DWORD err);          // fileops.cpp: perché un elemento non si è potuto usare, per gli avvisi
static void deleteSelected();                     // fileops.cpp: selezione nel Cestino
static void recyclePaths(std::vector<std::wstring> src); // fileops.cpp: questi percorsi nel Cestino
static bool closeDeferred();                      // fileops.cpp: WM_CLOSE con operazioni sui file in corso
static void closeIfIdle();                        // fileops.cpp: chiusura rimandata, se non resta niente in corso
static bool shellMenuBusy();                      // shell.cpp: un menu completo della shell, o un suo comando, in corso
static void gpuLost(HRESULT why);                 // hybrid.cpp: Present dice che il device non c'è più
static void undoLoaded(const std::vector<std::wstring>& lines); // fileops.cpp: annulla.txt letto all'avvio
static void undoCounts(int& undo, int& redo);     // fileops.cpp: lunghezza della cronologia (F12)
static DWORD WINAPI openThread(void* p);          // shell.cpp
static uint64_t zipJobStart(int kind, std::vector<uint32_t> items); // zipout.cpp: estrae le voci (ZJ_OPEN, ZJ_COPY, ZJ_DRAG)
static void dirSizeQueueCurrent();                // dirsize.cpp: colonna Dimensione della cartella aperta
static void dirSizeForget(const std::wstring& path); // dirsize.cpp: totale non più valido (e quelli sopra)
static void dirSizeForgetAll();                   // dirsize.cpp: nessun totale vale più (operazioni su migliaia di voci)
static void linksQueueCurrent();                  // links.cpp: destinazioni dei collegamenti della cartella aperta
static void tabNew(const std::wstring& path, bool activate, const std::wstring& select = L""); // tabs.cpp
static void updateTitle();                        // tabs.cpp
static LRESULT onCopyData(const COPYDATASTRUCT* cd); // main.cpp
static void drawTabs(double dt);                   // tabs.cpp
static void drawSidebar(double dt);                // sidebar.cpp
static void drawTooltip();                        // tooltip.cpp
static void tipHide();                            // tooltip.cpp
static void drawOptions(double dt);               // options.cpp
static void drawDrop();                           // drag.cpp
static void drawMarquee();                        // listmouse.cpp
static void drawChromeTitle(double dt);           // chrome.cpp
static void drawChromeToolbar();                  // chrome.cpp
static void optionsOpen();                        // options.cpp
static const wchar_t* shortcutFor(int menuId);    // options.cpp: scorciatoia di una voce di menu
static std::wstring shortcutsOf(int cmd);          // options.cpp: tutte le alternative di un comando (tooltip)
static void drawCrumbs(float x0, float x1, float y, float h, float textY, double dt); // crumbs.cpp
static void recentVisit(const std::wstring& path);   // goto.cpp: cartella aperta (cartelle recenti)
static void recentForget(const std::wstring& path);  // goto.cpp: cartella che non esiste più
static void recentLoaded(const std::vector<std::wstring>& lines); // goto.cpp: recenti.txt letto all'avvio
static void gotoOpen();                              // goto.cpp
static void gotoOpenHere();                          // goto.cpp: dalla cartella aperta (clic sul percorso, Ctrl+L, F4)
static bool gotoIsOpen();                            // goto.cpp
static void drawGoto();                              // goto.cpp
static bool gotoTipTarget(int x, int y, int& index); // goto.cpp: voce sotto il mouse, per il tooltip
static bool gotoTipBuild(int index);                 // goto.cpp: contenuto del tooltip di una voce
static void previewSync();                           // preview.cpp: chiede l'anteprima se la voce è cambiata
static void drawPreview();                           // preview.cpp
static void previewToggle();                         // preview.cpp: Alt+P, pulsante, menu
static void viewSet(bool grid, int size);            // thumbs.cpp: elenco o griglia (size -1: quella di prima)
static void thumbSizeStep(int delta);                // thumbs.cpp: Ctrl+rotellina in griglia
static bool viewIsGrid(const std::wstring& path);    // thumbs.cpp: vista ricordata della cartella
static void viewsLoaded(const std::vector<std::wstring>& lines); // thumbs.cpp: viste.txt letto all'avvio
struct Box;
static void thumbsFrame(int first, int last);         // thumbs.cpp: voci visibili in griglia (e sincronizzazione)
static void drawThumb(int vi, const Box& box, bool hidden, double now); // thumbs.cpp: miniatura o icona nel riquadro
static void thumbsDeviceChanged();                   // thumbs.cpp: passaggio WARP → GPU
static float updIconW();                             // update.cpp: spazio dell'icona di stato dell'aggiornamento
static void drawUpdateIcon(float x, float y, float w, float h, float k); // update.cpp: accanto alla versione
