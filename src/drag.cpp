// [z]f — trascinamento dei file: da [z]f verso le altre app e viceversa, tra le schede, verso la
// barra laterale e il percorso.
//
// Sorgente: l'oggetto dati della shell per le voci selezionate (SHDoDragDrop), così le altre app
// ricevono quello che darebbe Explorer, con l'immagine trascinata di Windows.
// Destinazione: la finestra si registra (RegisterDragDrop) dopo il primo frame, perché OLE non
// si carica all'avvio. Mentre il mouse si muove l'effetto si decide qui, senza disco né shell:
// stessa unità = sposta, altra unità = copia; Ctrl copia, Shift sposta, Alt o Ctrl+Shift crea un
// collegamento. Solo al rilascio si chiede alla shell la cartella vera (il suo IDropTarget) e le
// si passa l'effetto scelto: copia, conflitti e avanzamento sono quelli di Explorer.
// Col tasto destro, al rilascio si apre il nostro menu (Copia qui, Sposta qui, Crea collegamento).
// Tra i preferiti (sopra o sotto una voce, o sul titolo) le cartelle si aggiungono ai preferiti.

enum DropKind { DT_NONE, DT_ROW, DT_LIST, DT_SIDE, DT_FAVGAP, DT_CRUMB, DT_TAB };

struct DragState {
    bool registered = false;
    bool over = false;          // un trascinamento (nostro o di un'altra app) è sopra la finestra
    bool own = false;           // il trascinamento è partito da qui (ciclo modale di SHDoDragDrop)
    IDataObject* obj = nullptr;
    IDropTargetHelper* helper = nullptr;
    DWORD allowed = 0;          // effetti permessi dalla sorgente
    DWORD keys = 0;             // ultimo stato di tasti e pulsanti
    bool right = false;         // trascinamento col tasto destro
    bool hasData = false;       // file o elementi della shell da ricevere
    bool hasImage = false;      // la sorgente mostra l'immagine di Windows, con la descrizione
    bool descOk = false;        // la sorgente ha accettato la descrizione dell'azione
    std::vector<std::wstring> src; // percorsi trascinati (CF_HDROP), se ci sono
    // Calcolati una volta, non a ogni DragOver (con 20.300 voci il ciclo costava 2,5 ms a movimento)
    std::wstring srcParent;     // la cartella che contiene tutti i src ("" se sono in cartelle diverse)
    std::wstring selfFor;       // destinazione per cui vale selfHit
    bool selfHit = false;       // la destinazione è uno dei src o è dentro uno di loro
    POINT pt = {};              // coordinate client
    // Destinazione sotto il mouse
    int kind = DT_NONE, index = -1, favAt = -1;
    std::wstring path, name;
    DWORD effect = DROPEFFECT_NONE;
    double tabSince = 0;        // da quando il mouse è su una scheda non attiva
    // Descrizione già data alla sorgente: si riscrive solo se cambia
    int descType = -2;
    std::wstring descMsg, descIns;
    // Tasto sinistro premuto su una riga: il trascinamento parte oltre la soglia di Windows
    bool armed = false;
    DWORD armButton = MK_LBUTTON; // tasto con cui è stato premuto (il destro chiede cosa fare)
    POINT down = {};
    int deferRow = -1;          // clic su una voce già selezionata: vale al rilascio, se non si trascina
    bool deferToggle = false;   // …ed era un Ctrl+clic
    uint64_t zipWait = 0;       // trascinamento da uno zip: estrazione in corso col tasto premuto (zipout.cpp)
} DD;

// --- dati trascinati (gli aiuti sui percorsi sono in fileops.cpp) ----------------------
static bool dataHas(IDataObject* o, const wchar_t* fmt) {
    FORMATETC fe = { (CLIPFORMAT)RegisterClipboardFormatW(fmt), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    return o->QueryGetData(&fe) == S_OK;
}

static void dataRead(IDataObject* o) {
    DD.src.clear();
    hdropPaths(o, DD.src);
    DD.srcParent = DD.src.empty() ? L"" : parentPath(DD.src[0]);
    for (const std::wstring& s : DD.src)
        if (!samePath(parentPath(s), DD.srcParent)) { DD.srcParent.clear(); break; }
    DD.selfFor.clear();
    DD.selfHit = false;
    // Elementi senza percorso (telefoni, archivi, allegati): li riceve la shell, che sa estrarli
    DD.hasData = !DD.src.empty() || dataHas(o, CFSTR_SHELLIDLIST) || dataHas(o, CFSTR_FILEDESCRIPTORW);
    // Se c'è l'immagine trascinata di Windows, l'azione si scrive lì (DropDescription)
    DD.hasImage = dataHas(o, L"DragWindow") || dataHas(o, L"DragImageBits");
}

// Descrizione accanto al cursore ("Sposta in %1"), mostrata da Windows sotto l'immagine trascinata
static void dropSetDesc(int type, const wchar_t* msg, const std::wstring& ins) {
    if (!DD.obj || (type == DD.descType && DD.descMsg == msg && DD.descIns == ins)) return;
    DD.descType = type; DD.descMsg = msg; DD.descIns = ins;
    FORMATETC fe = { (CLIPFORMAT)RegisterClipboardFormatW(CFSTR_DROPDESCRIPTION), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    HGLOBAL h = GlobalAlloc(GHND, sizeof(DROPDESCRIPTION));
    if (!h) { DD.descOk = false; return; }
    DROPDESCRIPTION* d = (DROPDESCRIPTION*)GlobalLock(h);
    d->type = (DROPIMAGETYPE)type;
    wcsncpy(d->szMessage, msg, MAX_PATH - 1);
    wcsncpy(d->szInsert, ins.c_str(), MAX_PATH - 1);
    GlobalUnlock(h);
    STGMEDIUM m = {};
    m.tymed = TYMED_HGLOBAL;
    m.hGlobal = h;
    DD.descOk = SUCCEEDED(DD.obj->SetData(&fe, &m, TRUE));
    if (!DD.descOk) GlobalFree(h);
}

// Testo dell'azione, con il nome della destinazione al posto di %1
static std::wstring dropActionText() {
    if (DD.kind == DT_FAVGAP) return L"Aggiungi ai preferiti";
    const wchar_t* f = DD.effect == DROPEFFECT_MOVE ? L"Sposta in " : DD.effect == DROPEFFECT_COPY ? L"Copia in "
                     : DD.effect == DROPEFFECT_LINK ? L"Crea collegamento in " : nullptr;
    return f ? f + DD.name : L"";
}

static void dropDescribe() {
    if (DD.effect == DROPEFFECT_NONE) { dropSetDesc(DROPIMAGE_NONE, L"", L""); return; }
    if (DD.kind == DT_FAVGAP) { dropSetDesc(DROPIMAGE_LINK, L"Aggiungi ai preferiti", L""); return; }
    int t = DD.effect == DROPEFFECT_MOVE ? DROPIMAGE_MOVE : DD.effect == DROPEFFECT_COPY ? DROPIMAGE_COPY : DROPIMAGE_LINK;
    const wchar_t* m = t == DROPIMAGE_MOVE ? L"Sposta in %1" : t == DROPIMAGE_COPY ? L"Copia in %1" : L"Crea collegamento in %1";
    dropSetDesc(t, m, DD.name);
}

// --- destinazione sotto il mouse --------------------------------------------------------
static std::wstring tabPath(int i) {
    if (i == A.tab) return A.cur ? A.cur->path : L"";
    const TabState& t = A.tabs[i];
    return t.pending ? t.pendingPath : t.cur ? t.cur->path : L"";
}

static void dropTargetAt(int x, int y) {
    DD.kind = DT_NONE; DD.index = DD.favAt = -1;
    DD.path.clear(); DD.name.clear();
    if (OP.open || !A.cur) return;
    auto set = [&](int kind, int index, const std::wstring& path) {
        DD.kind = kind; DD.index = index; DD.path = path; DD.name = tabTitleFor(path);
    };
    if (y < A.tabH) { // scheda: si rilascia nella sua cartella; fermi un momento, si apre
        bool closeHit, plusHit;
        int t = tabHit(x, y, &closeHit, &plusHit);
        if (t >= 0 && !tabPath(t).empty()) set(DT_TAB, t, tabPath(t));
    } else if (y >= A.pathY && y < A.pathY + A.topH) { // segmento del percorso
        int id = crumbHit(x, y);
        if (id >= 0 && id != CRUMB_ELL && !(id & 1) && !CB.segs[id / 2].path.empty()) set(DT_CRUMB, id, CB.segs[id / 2].path);
    } else if (x < A.sideW && y >= A.sideTop) { // barra laterale
        float fy = sideFavY(), h = A.sideItemH;
        int nf = (int)A.favs.size();
        if (y >= fy - sideSectionH() && y < fy) { // titolo "Preferiti": in fondo all'elenco
            DD.kind = DT_FAVGAP; DD.favAt = nf;
        } else if (A.favOpen && y >= fy && y < fy + nf * h) {
            int i = (int)((y - fy) / h);
            float f = (y - fy - i * h) / h;
            if (f < ui::DRAG_GAP) { DD.kind = DT_FAVGAP; DD.favAt = i; }
            else if (f >= 1 - ui::DRAG_GAP) { DD.kind = DT_FAVGAP; DD.favAt = i + 1; }
            else set(DT_SIDE, i, A.favs[i].path);
        } else {
            int si = sideHit(x, y);
            if (si >= nf) if (const SideItem* it = sideItem(si)) if (!it->path.empty()) set(DT_SIDE, si, it->path); // non su "Questo PC"
        }
    } else if (x >= A.listX && x < A.listR - A.sbW && y >= A.listTop - A.headH && y < A.listTop + A.listH) {
        int r = itemAt(x, y);
        if (r >= 0) {
            const Entry& e = A.cur->e[A.view[r]];
            if (e.attr & FILE_ATTRIBUTE_DIRECTORY) set(DT_ROW, r, joinPath(A.cur->path, A.cur->name(e)));
        }
        // su un file o nello spazio vuoto: la cartella aperta (non "Questo PC")
        if (DD.kind == DT_NONE && !A.cur->isDrives && !A.cur->path.empty()) set(DT_LIST, -1, A.cur->path);
    }
    // Dentro uno zip non si rilascia niente: è in sola lettura
    if (DD.kind != DT_NONE && DD.kind != DT_FAVGAP && pathInZip(DD.path)) {
        DD.kind = DT_NONE; DD.index = -1;
        DD.path.clear(); DD.name.clear();
    }
}

// Effetto del rilascio, con le regole di Explorer
static DWORD dropEffectFor() {
    if (!DD.hasData || DD.kind == DT_NONE) return DROPEFFECT_NONE;
    DWORD a = DD.allowed;
    if (DD.kind == DT_FAVGAP) { // servono percorsi veri; alla sorgente si dice "collegato" o "copiato"
        if (DD.src.empty()) return DROPEFFECT_NONE;
        return (a & DROPEFFECT_LINK) ? DROPEFFECT_LINK : (a & DROPEFFECT_COPY) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    }
    if (!samePath(DD.selfFor, DD.path) || DD.selfFor.empty()) { // destinazione nuova: un giro solo
        DD.selfFor = DD.path;
        DD.selfHit = false;
        for (const std::wstring& s : DD.src)
            if (samePath(s, DD.path) || insidePath(DD.path, s)) { DD.selfHit = true; break; }
    }
    if (DD.selfHit) return DROPEFFECT_NONE; // in sé stessa o dentro di sé
    bool sameParent = !DD.srcParent.empty() && samePath(DD.srcParent, DD.path);
    bool ctrl = (DD.keys & MK_CONTROL) != 0, shift = (DD.keys & MK_SHIFT) != 0, alt = (DD.keys & MK_ALT) != 0;
    bool forced = ctrl || shift || alt;
    DWORD fx;
    if (alt || (ctrl && shift)) fx = DROPEFFECT_LINK;
    else if (ctrl) fx = DROPEFFECT_COPY;
    else if (shift) fx = DROPEFFECT_MOVE;
    else if (sameParent) fx = DD.right ? DROPEFFECT_COPY : DROPEFFECT_NONE; // nella cartella da cui vengono: niente
    else fx = !DD.src.empty() && !_wcsicmp(volumeOf(DD.src[0]).c_str(), volumeOf(DD.path).c_str()) ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
    if (sameParent && fx == DROPEFFECT_MOVE) return DROPEFFECT_NONE;
    if (fx && !(fx & a)) {
        if (forced) return DROPEFFECT_NONE;
        fx = (a & DROPEFFECT_COPY) ? DROPEFFECT_COPY : (a & DROPEFFECT_MOVE) ? DROPEFFECT_MOVE : (a & DROPEFFECT_LINK) ? DROPEFFECT_LINK : 0;
    }
    return fx;
}

// Ricalcola destinazione, effetto, evidenziazione e descrizione dal punto e dai tasti correnti
static void dropUpdate() {
    int oldKind = DD.kind, oldIndex = DD.index;
    dropTargetAt(DD.pt.x, DD.pt.y);
    if (DD.kind != oldKind || DD.index != oldIndex) DD.tabSince = nowMs();
    DD.effect = dropEffectFor();
    // La destinazione si evidenzia come un elemento sotto il mouse (C_HOVER, in dissolvenza);
    // il bordo lo aggiunge drawDrop. Durante il trascinamento il mouse non manda messaggi.
    bool can = DD.effect != DROPEFFECT_NONE;
    A.hover = can && DD.kind == DT_ROW ? DD.index : -1;
    A.sideHover = can && DD.kind == DT_SIDE ? DD.index : -1;
    CB.hover = can && DD.kind == DT_CRUMB ? DD.index : -1;
    A.tabHover = can && DD.kind == DT_TAB ? DD.index : -1;
    A.tabCloseHover = A.tabPlusHover = false;
    dropDescribe();
    A.dirty = true;
}

static void dropMove(DWORD keys, POINTL p) {
    DD.keys = keys;
    if (keys & MK_RBUTTON) DD.right = true;
    else if (keys & MK_LBUTTON) DD.right = false;
    POINT c = { p.x, p.y };
    ScreenToClient(A.hwnd, &c);
    DD.pt = c;
    dropUpdate();
}

static void dropEnd() {
    DD.over = false;
    DD.kind = DT_NONE;
    DD.effect = DROPEFFECT_NONE;
    if (DD.obj) { DD.obj->Release(); DD.obj = nullptr; }
    A.hover = A.sideHover = CB.hover = A.tabHover = -1;
    if (!DD.own) KillTimer(A.hwnd, 8);
    A.dirty = true;
}

// --- rilascio ---------------------------------------------------------------------------
// Preferiti: solo le cartelle, e per saperlo serve il disco, quindi su un thread
struct FavDrop { HWND hwnd; std::vector<std::wstring> paths; int at; };

static DWORD WINAPI favDropThread(void* p) {
    FavDrop* f = (FavDrop*)p;
    std::vector<std::wstring> dirs;
    for (const std::wstring& s : f->paths) {
        DWORD a = GetFileAttributesW(longPath(s).c_str());
        if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) dirs.push_back(s);
    }
    f->paths.swap(dirs);
    if (!PostMessageW(f->hwnd, WM_APP_FAVDROP, 0, (LPARAM)f)) delete f;
    return 0;
}

static void onFavDrop(FavDrop* f) {
    int at = f->at;
    for (const std::wstring& p : f->paths) favPut(p, at++);
    if (!f->paths.empty()) favSave();
    if (f->paths.empty()) notice(L"Nei preferiti vanno solo le cartelle", false);
    else if (f->paths.size() == 1) notice(L"Aggiunta ai preferiti", true);
    else notice(std::to_wstring(f->paths.size()) + L" cartelle aggiunte ai preferiti", true);
    delete f;
}

// Col tasto destro: il nostro menu, in un ciclo locale finché non si sceglie (come fa Explorer,
// che durante il rilascio tiene aperto il suo). Restituisce l'effetto scelto, 0 se annullato.
static DWORD dropAsk(POINT screen, DWORD allowed, DWORD preferred) {
    POINT c = screen;
    ScreenToClient(A.hwnd, &c);
    SetForegroundWindow(A.hwnd); // per Esc e per i clic, se il trascinamento veniva da un'altra app
    g_dropChoice = 0;
    menuOpenDrop((float)c.x, (float)c.y, allowed, preferred);
    while (A.menu.open && A.menu.kind == MENU_DROP) {
        if ((A.dirty || animating())) render();
        // Senza il fuoco i nostri messaggi non vedono i clic altrove né Esc: il menu si chiude da sé
        if (GetForegroundWindow() != A.hwnd) {
            POINT p;
            GetCursorPos(&p);
            bool outside = GetAncestor(WindowFromPoint(p), GA_ROOT) != A.hwnd;
            bool press = (GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON)) & 0x8000;
            if ((press && outside) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) menuClose();
        }
        MsgWaitForMultipleObjectsEx(0, nullptr, (A.dirty || animating()) ? 0 : ui::DRAG_TICK, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) { PostQuitMessage((int)m.wParam); menuClose(); break; }
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    return g_dropChoice == M_DROP_COPY ? DROPEFFECT_COPY : g_dropChoice == M_DROP_MOVE ? DROPEFFECT_MOVE
         : g_dropChoice == M_DROP_LINK ? DROPEFFECT_LINK : DROPEFFECT_NONE;
}

// File veri (CF_HDROP), copiati o spostati: li fa fileops.cpp con IFileOperation, che dà l'esito
// di ogni elemento e lo registra per Annulla. Alla shell (il suo IDropTarget, su un thread)
// restano i collegamenti, di cui sceglie il nome ("X - Collegamento"), e gli elementi senza
// percorso (telefoni, archivi, allegati), che sa estrarre lei.
// I dati trascinati passano al thread con il marshaling di COM; le chiamate tornano alla sorgente.
struct ShellDrop {
    HWND hwnd;
    IStream* data;             // IDataObject in viaggio verso il thread
    std::wstring path, name;
    DWORD fx;
    POINTL pt;
    std::vector<std::wstring> src; // percorsi trascinati (CF_HDROP), se ci sono
    bool notify;               // la destinazione non è la cartella aperta: l'effetto non si vede
    bool ok = false;
    HRESULT hr = S_OK;
    int done = -1;             // collegamenti comparsi davvero; -1 se non si sa (elementi senza percorso)
    UndoOp op;                 // collegamenti creati, per Annulla
    double ms = 0;
};

// Involucro dell'oggetto trascinato, passato alla shell al posto dell'originale. Inoltra tutto ma
// non dichiara IDataObjectAsyncCapability, né il formato "AsyncFlag" con cui Explorer dice la
// stessa cosa negli appunti: così la shell esegue l'operazione dentro Drop invece che su un suo
// thread, e Drop torna solo a operazione finita (avanzamento, conflitti, Annulla). In asincrono
// la shell richiama il nostro thread dopo Drop, e un thread che è già uscito non estrae niente
// (Incolla da uno zip: misurato). Drop restituisce S_OK anche se l'utente annulla: l'esito vero
// si legge poi sul disco.
struct SyncData : IDataObject {
    LONG refs = 1;
    IDataObject* in;
    explicit SyncData(IDataObject* o) : in(o) { in->AddRef(); }
    static bool asyncFlag(const FORMATETC* f) {
        static const CLIPFORMAT cf = (CLIPFORMAT)RegisterClipboardFormatW(L"AsyncFlag");
        return f && f->cfFormat == cf;
    }
    virtual ~SyncData() { in->Release(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** p) override {
        if (riid == IID_IUnknown || riid == IID_IDataObject) { *p = this; AddRef(); return S_OK; }
        *p = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refs);
        if (!r) delete this;
        return (ULONG)r;
    }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* f, STGMEDIUM* m) override { return asyncFlag(f) ? DV_E_FORMATETC : in->GetData(f, m); }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC* f, STGMEDIUM* m) override { return in->GetDataHere(f, m); }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* f) override { return asyncFlag(f) ? DV_E_FORMATETC : in->QueryGetData(f); }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC* a, FORMATETC* b) override { return in->GetCanonicalFormatEtc(a, b); }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC* f, STGMEDIUM* m, BOOL r) override { return in->SetData(f, m, r); }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD d, IEnumFORMATETC** e) override { return in->EnumFormatEtc(d, e); }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC* f, DWORD a, IAdviseSink* s, DWORD* c) override { return in->DAdvise(f, a, s, c); }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD c) override { return in->DUnadvise(c); }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA** e) override { return in->EnumDAdvise(e); }
};

static bool nameLess(const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; }
// Collegamenti nella cartella. I nomi li sceglie la shell: quelli nuovi sono quelli creati.
static std::vector<std::wstring> listLinks(const std::wstring& dir) {
    std::vector<std::wstring> v;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(longPath(joinPath(dir, L"*.lnk")).c_str(), FindExInfoBasic, &fd,
                                FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return v;
    do v.push_back(fd.cFileName); while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(v.begin(), v.end(), nameLess);
    return v;
}

// L'oggetto dati rilasciato sulla cartella path dal suo IDropTarget, con l'effetto fx: copia,
// conflitti, avanzamento ed estrazione da archivi e telefoni sono quelli di Explorer. Solo su un
// thread con OLE: torna a operazione finita. Lo usano il rilascio e Incolla senza percorsi.
static bool shellDropInto(HWND hwnd, IDataObject* data, const std::wstring& path, DWORD fx, POINTL pt, HRESULT& hr) {
    bool ok = false;
    IDataObject* obj = new SyncData(data);
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (SUCCEEDED(SHParseDisplayName(path.c_str(), nullptr, &pidl, 0, nullptr))) {
        IShellFolder* parent = nullptr;
        PCUITEMID_CHILD child = nullptr;
        if (SUCCEEDED(SHBindToParent(pidl, IID_IShellFolder, (void**)&parent, &child))) {
            IDropTarget* dt = nullptr;
            if (SUCCEEDED(parent->GetUIObjectOf(hwnd, 1, &child, IID_IDropTarget, nullptr, (void**)&dt))) {
                // L'effetto scelto si impone con i tasti che Explorer associa a ciascuno
                DWORD ks = MK_LBUTTON | (fx == DROPEFFECT_COPY ? MK_CONTROL : fx == DROPEFFECT_MOVE ? MK_SHIFT : MK_CONTROL | MK_SHIFT);
                DWORD e = fx;
                if (SUCCEEDED(dt->DragEnter(obj, ks, pt, &e)) && (e & fx)) {
                    e = fx;
                    hr = dt->Drop(obj, ks, pt, &e);
                    ok = SUCCEEDED(hr);
                } else {
                    dt->DragLeave();
                }
                dt->Release();
            }
            parent->Release();
        }
        CoTaskMemFree(pidl);
    }
    obj->Release();
    return ok;
}

static DWORD WINAPI shellDropThread(void* p) {
    ShellDrop* j = (ShellDrop*)p;
    double t0 = nowMs();
    OleInitialize(nullptr);
    IDataObject* real = nullptr;
    if (SUCCEEDED(CoGetInterfaceAndReleaseStream(j->data, IID_IDataObject, (void**)&real))) {
        bool links = j->fx == DROPEFFECT_LINK && !j->src.empty();
        std::vector<std::wstring> before;
        if (links) before = listLinks(j->path);
        j->ok = shellDropInto(j->hwnd, real, j->path, j->fx, j->pt, j->hr);
        real->Release();
        // Dopo: i collegamenti nuovi, con dimensione e data per riconoscerli quando si annulla
        if (links && j->ok) {
            std::vector<std::wstring> after = listLinks(j->path), added;
            std::set_difference(after.begin(), after.end(), before.begin(), before.end(), std::back_inserter(added), nameLess);
            j->op.what = UW_LINK;
            for (const std::wstring& n : added) {
                UndoItem it;
                it.kind = IK_CREATED;
                it.b = joinPath(j->path, n.c_str());
                bool dir = false;
                if (!statPath(it.b, it.size, it.mtime, dir)) continue;
                j->op.items.push_back(std::move(it));
            }
            j->done = (int)j->op.items.size();
        }
    } else {
        j->data = nullptr;
    }
    OleUninitialize();
    j->ms = nowMs() - t0;
    if (!PostMessageW(j->hwnd, WM_APP_DROPDONE, 0, (LPARAM)j)) delete j;
    return 0;
}

// Restituisce subito l'effetto da dichiarare alla sorgente. Per uno spostamento è
// DROPEFFECT_NONE, come fa Explorer: i file li sposta [z]f, la sorgente non deve cancellare
// niente (con DROPEFFECT_MOVE alcune app cancellerebbero gli originali prima della fine).
static DWORD dropToShell(IDataObject* obj, const std::wstring& path, const std::wstring& name, DWORD fx, POINTL pt, const std::vector<std::wstring>& src) {
    bool notify = !A.cur || !samePath(path, A.cur->path);
    if (!src.empty() && fx != DROPEFFECT_LINK)
        return fileTransfer(fx == DROPEFFECT_MOVE, src, path, name, notify) ? (fx == DROPEFFECT_MOVE ? DROPEFFECT_NONE : fx) : DROPEFFECT_NONE;
    IStream* st = nullptr;
    if (FAILED(CoMarshalInterThreadInterfaceInStream(IID_IDataObject, obj, &st))) return DROPEFFECT_NONE;
    ShellDrop* j = new ShellDrop{ A.hwnd, st, path, name, fx, pt, src, notify };
    HANDLE h = CreateThread(nullptr, 0, shellDropThread, j, 0, nullptr);
    if (!h) { st->Release(); delete j; return DROPEFFECT_NONE; }
    CloseHandle(h);
    UH.running++;
    return fx == DROPEFFECT_MOVE ? DROPEFFECT_NONE : fx;
}

static void onShellDrop(ShellDrop* j) {
    UH.running--;
    A.dropMs = j->ms;
    A.changeAt = nowMs();
    transferNotice(j->fx, j->ok, hrCancelled(j->hr), j->notify, j->done, j->src.size(), j->name);
    undoPush(std::move(j->op));
    delete j;
    A.dirty = true;
    closeIfIdle();
}

// Tempo passato sul thread dell'interfaccia in una chiamata del trascinamento (F12, registro)
static void dropNoteUi(double t0) {
    double d = nowMs() - t0;
    if (d > A.dropUiMax) A.dropUiMax = d;
    if (d > ui::LOG_MSG) hitchLog("trascinamento lento", 0, d);
}

// --- IDropTarget della finestra ---------------------------------------------------------
struct DropTarget : IDropTarget {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** p) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) { *p = this; return S_OK; }
        *p = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }  // statico: vive quanto il programma
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* obj, DWORD keys, POINTL p, DWORD* pe) override {
        double t0 = nowMs();
        tipHide();
        menuClose();
        DD.over = true;
        DD.obj = obj;
        obj->AddRef();
        DD.allowed = *pe;
        DD.right = false;
        DD.descType = -2;
        DD.descOk = false;
        DD.kind = DT_NONE;
        dataRead(obj);
        if (!DD.helper)
            CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_IDropTargetHelper, (void**)&DD.helper);
        dropMove(keys, p);
        *pe = DD.effect;
        POINT sp = { p.x, p.y };
        if (DD.helper) DD.helper->DragEnter(A.hwnd, obj, &sp, *pe);
        SetTimer(A.hwnd, 8, ui::DRAG_TICK, nullptr); // scorrimento, schede, disegno
        dropNoteUi(t0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD keys, POINTL p, DWORD* pe) override {
        double t0 = nowMs();
        dropMove(keys, p);
        *pe = DD.effect;
        POINT sp = { p.x, p.y };
        if (DD.helper) DD.helper->DragOver(&sp, *pe);
        dropNoteUi(t0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override {
        if (DD.helper) DD.helper->DragLeave();
        dropSetDesc(DROPIMAGE_INVALID, L"", L""); // fuori dalla finestra decide chi c'è sotto
        dropEnd();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* obj, DWORD keys, POINTL p, DWORD* pe) override {
        double t0 = nowMs();
        dropMove(keys, p);
        DWORD fx = DD.effect;
        POINT sp = { p.x, p.y };
        if (DD.helper) DD.helper->Drop(obj, &sp, fx);
        int kind = DD.kind, favAt = DD.favAt;
        std::wstring path = DD.path, name = DD.name;
        std::vector<std::wstring> src = std::move(DD.src); // dopo il rilascio non servono più: niente copia di ogni percorso
        bool right = DD.right;
        DWORD allowed = DD.allowed;
        dropEnd();
        dropNoteUi(t0); // il menu del tasto destro è tempo dell'utente: quello che segue si misura a parte
        *pe = DROPEFFECT_NONE;
        if (!fx) return S_OK;
        if (kind == DT_FAVGAP) {
            FavDrop* f = new FavDrop{ A.hwnd, src, favAt };
            HANDLE h = CreateThread(nullptr, 0, favDropThread, f, 0, nullptr);
            if (h) CloseHandle(h); else delete f;
            *pe = fx;
            return S_OK;
        }
        if (right) {
            fx = dropAsk(sp, allowed, fx);
            if (!fx) return S_OK;
        }
        double t1 = nowMs();
        *pe = dropToShell(obj, path, name, fx, p, src);
        dropNoteUi(t1);
        return S_OK;
    }
};
static DropTarget g_dropTarget;

// Dopo il primo frame: OLE e la registrazione costano qualche millisecondo, non all'avvio
static void dropRegister() {
    double t0 = nowMs();
    if (oleReady() && SUCCEEDED(RegisterDragDrop(A.hwnd, &g_dropTarget))) DD.registered = true;
    A.dropRegisterMs = nowMs() - t0;
}

static void dropUnregister() {
    if (DD.registered) RevokeDragDrop(A.hwnd);
    DD.registered = false;
    if (DD.helper) { DD.helper->Release(); DD.helper = nullptr; }
}

// --- sorgente ---------------------------------------------------------------------------
// L'oggetto della shell per la selezione costa circa 25 µs a voce sul thread UI (20.300 voci:
// 550 ms prima che il trascinamento parta). Da kDragFastItems voci in su si trascina un oggetto con
// i soli percorsi (CF_HDROP, come Copia e Taglia), che costa pochi millisecondi. Sotto resta quello
// della shell, che dà alle altre app tutti i formati di Explorer: con migliaia di voci servono a
// poco, l'attesa invece si sente.
static constexpr size_t kDragFastItems = 500; // con l'oggetto della shell circa 12 ms

// Immagine trascinata per l'oggetto con i soli percorsi (senza, Windows disegna un quadrato
// bianco): l'icona grande del primo elemento, per estensione dalla lista delle icone di sistema,
// che il thread delle icone ha già riempito per le voci visibili. Alfa come per le icone della lista.
static void dragSetImage(IDataObject* obj, const std::wstring& first, DWORD attr) {
    SHFILEINFOW fi = {};
    DWORD a = (attr & FILE_ATTRIBUTE_DIRECTORY) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    if (!SHGetFileInfoW(first.c_str(), a, &fi, sizeof(fi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES) || !fi.hIcon) return;
    int S = GetSystemMetrics(SM_CXICON);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = S;
    bi.bmiHeader.biHeight = -S;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = dc ? CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0) : nullptr;
    if (bmp) {
        HGDIOBJ old = SelectObject(dc, bmp);
        std::vector<uint32_t> px;
        iconRaster(dc, bits, S, fi.hIcon, px);
        SelectObject(dc, old);
        for (int i = 0; i < S * S; i++) { // premoltiplicato, come lo vuole l'immagine trascinata
            uint32_t p = px[(size_t)i], al = p >> 24;
            bits[i] = (al << 24) | (((p >> 16 & 255) * al / 255) << 16) | (((p >> 8 & 255) * al / 255) << 8) | ((p & 255) * al / 255);
        }
        IDragSourceHelper2* h = nullptr;
        bool owned = false;
        if (SUCCEEDED(CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_IDragSourceHelper2, (void**)&h))) {
            h->SetFlags(DSH_ALLOWDROPDESCRIPTIONTEXT); // "Sposta in …" sotto l'immagine
            SHDRAGIMAGE sdi = { { S, S }, { S / 2, S / 2 }, bmp, CLR_NONE };
            owned = SUCCEEDED(h->InitializeFromBitmap(&sdi, obj)); // da qui la bitmap è dell'helper
            h->Release();
        }
        if (!owned) DeleteObject(bmp);
    }
    if (dc) DeleteDC(dc);
    DestroyIcon(fi.hIcon);
}

// Oggetto dei file (CF_HDROP) e immagine trascinata per dei percorsi: la shell sul thread UI, quindi
// si misura (DESIGN §10: F12 e registro). Serve ai trascinamenti di molte voci e a quelli da uno zip
static IDataObject* dragObjectFor(const std::vector<std::wstring>& paths, DWORD firstAttr) {
    double t0 = nowMs();
    IDataObject* obj = hdropObject(paths, 0);
    if (obj) dragSetImage(obj, paths[0], firstAttr);
    A.shellObjMs = nowMs() - t0;
    if (A.shellObjMs > ui::LOG_MSG) hitchLog("oggetto dei file lento", (unsigned)paths.size(), A.shellObjMs);
    return obj;
}

// Il trascinamento di Windows, col suo ciclo modale, per un oggetto pronto (che rilascia)
static void dragRun(IDataObject* obj, DWORD allowed) {
    tipHide();
    menuClose();
    DD.own = true;
    SetTimer(A.hwnd, 8, ui::DRAG_TICK, nullptr); // il ciclo principale resta fermo nel ciclo modale
    g_shellCalls++;                              // ciclo modale: il tempo è dell'utente
    DWORD eff = 0;
    SHDoDragDrop(A.hwnd, obj, nullptr, allowed, &eff);
    DD.own = false;
    if (!DD.over) KillTimer(A.hwnd, 8);
    obj->Release();
    A.dirty = true;
}

static void dragStart() {
    DD.armed = false;
    DD.deferRow = -1; // si trascina la selezione così com'è
    if (A.cur && A.cur->zipLen && !A.renaming) { // da uno zip: prima si estrae, col tasto premuto (zipout.cpp)
        DD.zipWait = zipJobStart(ZJ_DRAG, selectedEntries());
        if (!DD.zipWait) ReleaseCapture();
        return;
    }
    ReleaseCapture();
    if (!A.cur || A.renaming) return;
    std::vector<uint32_t> items = selectedEntries();
    if (items.empty()) return;
    IDataObject* obj = nullptr;
    if (items.size() >= kDragFastItems && !A.cur->isDrives && !A.cur->path.empty()) {
        std::vector<std::wstring> paths;
        paths.reserve(items.size());
        for (uint32_t ei : items) paths.push_back(joinPath(A.cur->path, A.cur->name(A.cur->e[ei])));
        obj = dragObjectFor(paths, A.cur->e[items[0]].attr);
    }
    if (!obj) obj = (IDataObject*)shellObjectFor(items, IID_IDataObject);
    if (!obj) return;
    // Le unità di "Questo PC" non si spostano né si copiano: si può solo crearne un collegamento
    dragRun(obj, A.cur->isDrives ? DROPEFFECT_LINK : DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK);
}

// Tasto sinistro premuto su una riga (dopo aver aggiornato la selezione)
static void dragArm(int x, int y, DWORD button) {
    DD.armed = true;
    DD.armButton = button;
    DD.down = { x, y };
    SetCapture(A.hwnd);
}

// Tasto destro premuto su una riga: la seleziona (come Explorer) e può partire un trascinamento.
// Se non si trascina, al rilascio arriva il menu contestuale di sempre.
static void dragPressRight(int x, int y) {
    if (A.menu.open || OP.open || A.renaming || !A.cur || x < A.listX || x >= A.listR - A.sbW) return;
    int r = itemAt(x, y);
    if (r < 0) return;
    if (!isSel(A.view[r])) moveCursorTo(r, false, false);
    else A.sel = r;
    A.dirty = true;
    dragArm(x, y, MK_RBUTTON);
}

// Movimento col tasto premuto: oltre la soglia di Windows parte il trascinamento
static bool dragCheck(int x, int y, WPARAM wp) {
    if (!DD.armed) return false;
    if (!(wp & DD.armButton)) { DD.armed = false; return false; }
    if (abs(x - DD.down.x) <= GetSystemMetrics(SM_CXDRAG) && abs(y - DD.down.y) <= GetSystemMetrics(SM_CYDRAG)) return false;
    dragStart();
    return true;
}

// Tasto sinistro rilasciato senza trascinare: vale il clic rimandato sulla selezione
static void dragRelease() {
    if (DD.armed) { DD.armed = false; ReleaseCapture(); }
    int r = DD.deferRow;
    DD.deferRow = -1;
    if (r < 0 || r >= (int)A.view.size()) return;
    if (DD.deferToggle) { // Ctrl+clic su una voce selezionata: la toglie
        uint32_t ei = A.view[r];
        A.selFlags[ei] = 0;
        A.anchor = (int)ei;
        A.sel = r;
        A.dirty = true;
    } else {
        moveCursorTo(r, false, false);
    }
}

// Timer del trascinamento: scorrimento vicino ai bordi, scheda che si apre, disegno
static void dragTick() {
    if (!DD.own && !DD.over) { KillTimer(A.hwnd, 8); return; }
    if (DD.over) {
        int x = DD.pt.x, y = DD.pt.y;
        if (x >= A.listX && x < A.listR - A.sbW && y >= A.listTop - A.headH && y < A.listTop + A.listH && maxScroll() > 0) {
            // Vicino ai bordi lungo l'asse (in alto anche sull'intestazione; in Elenco a sinistra e a destra)
            int dir = listView() ? (x < A.listX + A.rowH ? -1 : x >= A.listR - A.sbW - A.rowH ? 1 : 0)
                    : (y < A.listTop + A.rowH) ? -1 : (y >= A.listTop + A.listH - A.rowH) ? 1 : 0;
            if (dir) {
                A.scrollTarget += dir * A.rowH * ui::DRAG_SCROLL; // stessa velocità in pixel in ogni vista
                clampScroll();
                A.scrollY = A.scrollTarget;
                dropUpdate();
            }
        }
        if (DD.kind == DT_TAB && DD.index != A.tab && nowMs() - DD.tabSince >= ui::DRAG_SPRING) {
            tabActivate(DD.index);
            dropUpdate();
        }
    }
    if (DD.own && (A.dirty || animating())) render(); // durante il nostro trascinamento il ciclo è di Windows
}

// --- disegno: bordo della destinazione, linea d'inserimento, azione se Windows non la scrive ---
static void drawDrop() {
    if (!DD.over || DD.effect == DROPEFFECT_NONE) return;
    float t = std::max(1.0f, floorf(A.scale)), r = px(ui::R_S);
    switch (DD.kind) {
    case DT_ROW: {
        Box b = itemBg(DD.index);
        float ox, oy;
        scrollOffset(ox, oy);
        float x = b.x - ox, y = A.listTop + b.y - oy;
        if (y >= A.listTop - 1 && y + b.h <= A.listTop + A.listH + 1 && x >= A.listX - 1 && x + b.w <= A.listR - A.sbW + 1)
            rrect(x, y, b.w, b.h, r, C_SEL_EDGE, t);
        break;
    }
    case DT_LIST:
        rrect(A.listX + px(ui::GAP_S), A.listTop, A.listR - A.sbW - A.listX - px(ui::GAP_S) * 2, A.listH, r, C_SEL_EDGE, t);
        break;
    case DT_SIDE:
        rrect(px(ui::GAP_S), sideItemY(DD.index), A.sideW - 1 - px(ui::GAP_M), A.sideItemH, r, C_SEL_EDGE, t);
        break;
    case DT_FAVGAP: // linea d'inserimento tra i preferiti
        drawFavGap(DD.favAt);
        break;
    case DT_CRUMB: {
        const Crumb& c = CB.segs[DD.index / 2];
        rrect(c.x, CB.y, c.w, CB.h, r, C_SEL_EDGE, t);
        break;
    }
    case DT_TAB: {
        float tw = tabWidth(), x = A.tabStripX + DD.index * tw, top = px(ui::TAB_TOP);
        rrect(x + px(ui::GAP_XS), top + px(ui::GAP_XS), tw - 1 - px(ui::GAP_XS) * 2, A.tabH - top - px(ui::GAP_S), r, C_SEL_EDGE, t);
        break;
    }
    }
    // Se la sorgente non mostra l'immagine di Windows con la descrizione, l'azione la scriviamo noi
    if (!(DD.hasImage && DD.descOk)) {
        std::wstring s = dropActionText();
        float pad = px(ui::TIP_PAD);
        float w = textWidth(s.c_str(), (int)s.size()) + pad * 2, h = (float)F.lineH + pad * 2;
        float x = DD.pt.x + px(ui::TIP_OFFSET_X), y = DD.pt.y + px(ui::TIP_OFFSET_Y);
        float m = px(ui::GAP_S);
        if (x + w > G.w - m) x = std::max(m, G.w - m - w);
        if (y + h > G.h - m) y = std::max(m, (float)DD.pt.y - h - px(ui::GAP_M));
        drawPanel(floorf(x), floorf(y), w, h, r, 1);
        drawText(floorf(x) + pad, floorf(y) + pad, s, C_TEXT);
    }
}
