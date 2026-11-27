// [z]f — operazioni sui file e cronologia per annullare (Ctrl+Z) e ripetere (Ctrl+Y).
//
// Spostamenti, copie ed eliminazioni nel Cestino fatti da [z]f passano da IFileOperation, il
// motore di copia di Explorer: avanzamento, conflitti e permessi sono le finestre della shell.
// Si lavora su un thread; un IFileOperationProgressSink dà l'esito di ogni elemento: il nome finale
// dopo "Mantieni entrambi", gli elementi saltati e, per l'eliminazione, l'elemento esatto nel
// Cestino. Per questo annullare sa sempre cosa rimettere a posto, senza cercare né indovinare.
//
// La cronologia è una pila di operazioni; ogni operazione è un elenco di elementi e ogni elemento
// dice cosa gli è successo (spostato, creato, nel Cestino, ripristinato, cartella rimossa).
// Annullare esegue l'inverso di ogni elemento e registra a sua volta cosa è successo davvero:
// quella è l'operazione da ripetere, e il suo inverso è di nuovo un'operazione da annullare.
// Prima di toccare un elemento il thread controlla che il disco sia come l'avevamo lasciato; se
// no lo salta e lo dice. Non si sostituisce mai niente.
//
// Rinomina e nuova cartella restano operazioni dirette (MoveFileExW, CreateDirectoryW): l'esito
// lo dà già la chiamata. Il thread dell'interfaccia prepara il lavoro e mostra l'avviso, basta.

#ifndef COPYENGINE_S_USER_IGNORED
#define COPYENGINE_S_USER_IGNORED ((HRESULT)0x00270005L)
#endif
#ifndef COPYENGINE_E_USER_CANCELLED
#define COPYENGINE_E_USER_CANCELLED ((HRESULT)0x80270000L)
#endif

constexpr int UNDO_MAX = 100;            // operazioni ricordate
constexpr size_t UNDO_FILE_MAX = 900000; // annulla.txt: oltre, le operazioni più vecchie non si salvano

// --- aiuti sui percorsi (anche per drag.cpp) ---------------------------------------------
static std::wstring parentPath(const std::wstring& p) {
    size_t k = p.find_last_of(L'\\', p.size() > 1 ? p.size() - 2 : 0);
    if (k == std::wstring::npos) return L"";
    std::wstring r = p.substr(0, k);
    if (r.size() == 2 && r[1] == L':') r += L'\\';
    return r;
}
// child è dentro dir (a qualsiasi profondità)
static bool insidePath(const std::wstring& child, const std::wstring& dir) {
    size_t n = dir.size();
    while (n > 0 && dir[n - 1] == L'\\') n--;
    return child.size() > n + 1 && child[n] == L'\\' && _wcsnicmp(child.c_str(), dir.c_str(), n) == 0;
}
// Radice del volume: "C:" oppure "\\server\condivisione"
static std::wstring volumeOf(const std::wstring& p) {
    if (p.size() >= 2 && p[1] == L':') return p.substr(0, 2);
    if (p.size() > 2 && p[0] == L'\\' && p[1] == L'\\') {
        size_t a = p.find(L'\\', 2);
        size_t b = a == std::wstring::npos ? std::wstring::npos : p.find(L'\\', a + 1);
        return p.substr(0, b);
    }
    return p;
}
static bool pathExists(const std::wstring& p) {
    return GetFileAttributesW(longPath(p).c_str()) != INVALID_FILE_ATTRIBUTES;
}
static const wchar_t* leafOf(const std::wstring& p) {
    size_t k = p.find_last_of(L'\\');
    return k == std::wstring::npos ? p.c_str() : p.c_str() + k + 1;
}
// Nome della cartella che contiene p, per gli avvisi scritti sul thread ("C:\" resta "C:\")
static std::wstring parentName(const std::wstring& p) {
    std::wstring d = parentPath(p);
    const wchar_t* l = leafOf(d);
    return *l ? std::wstring(l) : d;
}
static std::wstring pathKey(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], (DWORD)s.size());
    return s;
}
// Dimensione e data di modifica: per riconoscere un file cambiato dopo la copia
static bool statPath(const std::wstring& p, uint64_t& size, uint64_t& mtime, bool& dir) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(longPath(p).c_str(), GetFileExInfoStandard, &d)) return false;
    size = ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    mtime = ((uint64_t)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime;
    dir = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return true;
}

// Percorsi (CF_HDROP) di un oggetto dati: trascinamento e appunti. DROPFILES si legge direttamente,
// in un passaggio: DragQueryFileW(h, i) riparte dall'inizio dell'elenco a ogni chiamata, e con
// 20.300 voci l'ingresso di un trascinamento costava 10,4 s sul thread UI (misurato).
static void hdropPaths(IDataObject* o, std::vector<std::wstring>& out) {
    FORMATETC fe = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM m = {};
    if (FAILED(o->GetData(&fe, &m))) return;
    SIZE_T cap = GlobalSize(m.hGlobal);
    const DROPFILES* df = (const DROPFILES*)GlobalLock(m.hGlobal);
    if (df && cap > sizeof(DROPFILES) && df->pFiles >= sizeof(DROPFILES) && df->pFiles < cap) {
        const uint8_t* end = (const uint8_t*)df + cap; // i limiti contano: i dati vengono da un altro programma
        if (df->fWide) {
            const wchar_t* p = (const wchar_t*)((const uint8_t*)df + df->pFiles);
            const wchar_t* e = (const wchar_t*)end;
            while (p < e && *p) {
                const wchar_t* s = p;
                while (p < e && *p) p++;
                out.emplace_back(s, (size_t)(p - s));
                p++;
            }
        } else { // vecchi programmi: nomi ANSI
            const char* p = (const char*)df + df->pFiles;
            const char* e = (const char*)end;
            while (p < e && *p) {
                const char* s = p;
                while (p < e && *p) p++;
                int n = MultiByteToWideChar(CP_ACP, 0, s, (int)(p - s), nullptr, 0);
                std::wstring w((size_t)n, L'\0');
                if (n) MultiByteToWideChar(CP_ACP, 0, s, (int)(p - s), &w[0], n);
                out.push_back(std::move(w));
                p++;
            }
        }
    }
    if (df) GlobalUnlock(m.hGlobal);
    ReleaseStgMedium(&m);
}

static const wchar_t* errWhy(DWORD err) {
    return err == ERROR_ALREADY_EXISTS || err == ERROR_FILE_EXISTS ? L"esiste già un elemento con questo nome"
         : err == ERROR_SHARING_VIOLATION                          ? L"il file è in uso"
         : err == ERROR_ACCESS_DENIED                              ? L"accesso negato"
         : err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ? L"non esiste più"
         : err == ERROR_BAD_NETPATH || err == ERROR_BAD_NET_NAME   ? L"percorso di rete non trovato"
         : err == ERROR_NOT_READY                                  ? L"dispositivo non pronto"
                                                                   : L"errore";
}

// --- cronologia ------------------------------------------------------------------------------
enum UndoWhat { UW_MOVE, UW_COPY, UW_DELETE, UW_RENAME, UW_MKDIR, UW_LINK, UW_COUNT };
// Cosa è successo a un elemento. a: dove deve tornare; b: dov'è adesso.
enum ItemKind : uint8_t {
    IK_MOVED,    // spostato o rinominato da a a b
    IK_CREATED,  // creato in b (copia, cartella nuova, collegamento); size e mtime per riconoscerlo
    IK_RECYCLED, // nel Cestino: era in a, ora è b ($R… in $Recycle.Bin)
    IK_RESTORED, // ripristinato dal Cestino in b; size e mtime come IK_CREATED
    IK_RMDIR,    // cartella vuota a rimossa
};
enum : uint8_t {
    IF_DIR = 1,      // è una cartella
    IF_REPLACED = 2, // al suo posto c'era già un elemento con lo stesso nome (sostituito o unito)
    IF_EMPTYOK = 4,  // cartella nuova: se è ancora vuota si rimuove invece di metterla nel Cestino
};
struct UndoItem { uint8_t kind = 0, flags = 0; std::wstring a, b; uint64_t size = 0, mtime = 0; };
struct UndoOp { int what = 0; std::vector<UndoItem> items; };

struct UndoState {
    std::vector<UndoOp> undo, redo;
    int running = 0;   // operazioni sui file in corso: il loro esito non è ancora nella cronologia
    bool busy = false; // annullamento o ripetizione in corso
    uint32_t pushes = 0; // operazioni registrate finora: un annullamento sa se nel frattempo ne è arrivata una
} UH;

static void undoSave();

static void undoTrim(std::vector<UndoOp>& st) {
    if (st.size() > (size_t)UNDO_MAX) st.erase(st.begin(), st.begin() + (st.size() - UNDO_MAX));
}

// Registra un'operazione appena fatta. Costa lo spostamento dei vettori: microsecondi. Con l'opzione
// "Annullamento anche dopo il riavvio" anche la preparazione di annulla.txt, al massimo UNDO_FILE_MAX byte (F12).
static void undoPush(UndoOp&& op) {
    if (op.items.empty()) return;
    double t0 = nowMs();
    UH.redo.clear();
    UH.pushes++;
    // Nuova cartella rinominata subito dopo: una voce sola, come la vede l'utente
    if (op.what == UW_RENAME && op.items.size() == 1 && !UH.undo.empty()) {
        UndoOp& top = UH.undo.back();
        if (top.what == UW_MKDIR && top.items.size() == 1 && top.items[0].kind == IK_CREATED &&
            samePath(top.items[0].b, op.items[0].a)) {
            top.items[0].b = op.items[0].b;
            undoSave();
            A.undoRecMs = nowMs() - t0; // con annulla.txt (opzione) comprende la preparazione del file
            return;
        }
    }
    UH.undo.push_back(std::move(op));
    undoTrim(UH.undo);
    undoSave();
    A.undoRecMs = nowMs() - t0;
}

// Voce di menu: "Annulla spostamento", "Ripeti copia"…; senza niente, "Annulla"/"Ripeti" disattivate
static const wchar_t* undoLabel(bool redo) {
    static const wchar_t* u[UW_COUNT] = { L"Annulla spostamento", L"Annulla copia", L"Annulla eliminazione",
                                          L"Annulla rinomina", L"Annulla nuova cartella", L"Annulla collegamento" };
    static const wchar_t* r[UW_COUNT] = { L"Ripeti spostamento", L"Ripeti copia", L"Ripeti eliminazione",
                                          L"Ripeti rinomina", L"Ripeti nuova cartella", L"Ripeti collegamento" };
    const std::vector<UndoOp>& st = redo ? UH.redo : UH.undo;
    if (st.empty()) return redo ? L"Ripeti" : L"Annulla";
    return (redo ? r : u)[st.back().what];
}
static bool undoCan(bool redo) { return !(redo ? UH.redo : UH.undo).empty(); }
static void undoCounts(int& undo, int& redo) { undo = (int)UH.undo.size(); redo = (int)UH.redo.size(); }

// --- annulla.txt (solo con l'opzione "Annullamento anche dopo il riavvio")------------------------------
// Righe: "u<TAB>cosa" o "r<TAB>cosa" apre un'operazione (da annullare / da ripetere, dalla più
// vecchia); "i<TAB>tipo<TAB>flag<TAB>dimensione<TAB>data<TAB>a<TAB>b" è un suo elemento.
// I nomi dei file non possono contenere tabulazioni.
static void undoSave() {
    // Prima della lettura (opzione accesa nei primi istanti) si riscriverebbe il file con le sole
    // operazioni nuove: ci pensa undoLoaded
    if (!A.undoPersist || !A.sideFastDone) return;
    std::vector<std::string> ops; // dalla più recente, finché il file resta piccolo
    size_t total = 0;
    // Il controllo del limite va fatto elemento per elemento: costruire prima tutta un'operazione
    // enorme (Ctrl+A e sposta 50.000 file) costava megabyte di stringhe a ogni salvataggio successivo,
    // sul thread UI. Così il lavoro resta sotto UNDO_FILE_MAX qualunque sia la cronologia.
    auto add = [&](const UndoOp& op, char tag) {
        std::string s;
        s += tag; s += '\t'; s += std::to_string(op.what); s += "\r\n";
        for (const UndoItem& it : op.items) {
            s += "i\t" + std::to_string(it.kind) + '\t' + std::to_string(it.flags) + '\t' + std::to_string(it.size) + '\t' +
                 std::to_string(it.mtime) + '\t' + toUtf8(it.a) + '\t' + toUtf8(it.b) + "\r\n";
            if (total + s.size() > UNDO_FILE_MAX) return false;
        }
        if (total + s.size() > UNDO_FILE_MAX) return false;
        total += s.size();
        ops.push_back(std::move(s));
        return true;
    };
    // Ordine nel file: ripetibili dalla più vecchia, poi annullabili dalla più vecchia
    std::vector<std::string> redoPart, undoPart;
    for (int i = (int)UH.undo.size() - 1; i >= 0; i--) if (!add(UH.undo[i], 'u')) break;
    undoPart.swap(ops);
    for (int i = (int)UH.redo.size() - 1; i >= 0; i--) if (!add(UH.redo[i], 'r')) break;
    redoPart.swap(ops);
    std::string d;
    for (int i = (int)redoPart.size() - 1; i >= 0; i--) d += redoPart[i];
    for (int i = (int)undoPart.size() - 1; i >= 0; i--) d += undoPart[i];
    saveDataAsync(L"annulla.txt", std::move(d));
}

// annulla.txt letto all'avvio dal thread veloce della barra laterale. Le operazioni fatte prima del
// suo arrivo restano le più recenti.
static void undoLoaded(const std::vector<std::wstring>& lines) {
    if (!A.undoPersist) return;
    std::vector<UndoOp> und, red;
    std::vector<UndoOp>* cur = nullptr;
    for (const std::wstring& l : lines) {
        std::vector<std::wstring> f;
        size_t i = 0;
        while (true) {
            size_t t = l.find(L'\t', i);
            f.push_back(l.substr(i, t == std::wstring::npos ? std::wstring::npos : t - i));
            if (t == std::wstring::npos) break;
            i = t + 1;
        }
        if ((f[0] == L"u" || f[0] == L"r") && f.size() == 2) {
            int w = _wtoi(f[1].c_str());
            if (w < 0 || w >= UW_COUNT) { cur = nullptr; continue; }
            cur = f[0] == L"u" ? &und : &red;
            cur->push_back(UndoOp{ w, {} });
        } else if (f[0] == L"i" && f.size() == 7 && cur) {
            UndoItem it;
            int k = _wtoi(f[1].c_str());
            if (k < IK_MOVED || k > IK_RMDIR) continue;
            it.kind = (uint8_t)k;
            it.flags = (uint8_t)_wtoi(f[2].c_str());
            it.size = _wcstoui64(f[3].c_str(), nullptr, 10);
            it.mtime = _wcstoui64(f[4].c_str(), nullptr, 10);
            it.a = f[5];
            it.b = f[6];
            cur->back().items.push_back(std::move(it));
        }
    }
    bool fresh = UH.undo.empty() && UH.redo.empty(); // un'operazione fatta prima ha già cancellato i "Ripeti"
    for (UndoOp& o : UH.undo) und.push_back(std::move(o));
    UH.undo.swap(und);
    undoTrim(UH.undo);
    if (fresh) UH.redo.swap(red);
    else undoSave(); // le operazioni di prima non sono ancora nel file
}

// Opzione "Annullamento anche dopo il riavvio": spenta, il file si svuota
static void undoPersistToggle() {
    A.undoPersist = !A.undoPersist;
    settingsSave();
    if (A.undoPersist) undoSave();
    else saveDataAsync(L"annulla.txt", std::string());
}

// --- IFileOperation con l'esito di ogni elemento ---------------------------------------------
struct FoItem { std::wstring path, dir, name; }; // dir, name: destinazione (name vuoto = lo stesso)
struct FoOut { bool done = false; std::wstring path; }; // path: dov'è ora; per l'eliminazione, nel Cestino ("" = definitiva)
enum { FK_COPY, FK_MOVE, FK_RECYCLE };

static std::wstring itemPath(IShellItem* it) {
    std::wstring r;
    PWSTR p = nullptr;
    if (it && SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) r = p;
    CoTaskMemFree(p);
    return r;
}

struct OpSink : IFileOperationProgressSink {
    LONG refs = 1;
    std::unordered_map<std::wstring, int> keys; // elementi chiesti (percorso in minuscolo): il resto è il loro contenuto
    std::vector<FoOut> out;
    explicit OpSink(size_t n) : out(n) {}
    virtual ~OpSink() {}
    void note(IShellItem* item, HRESULT hr, IShellItem* created, bool del) {
        if (FAILED(hr) || hr == COPYENGINE_S_USER_IGNORED) return; // errore, o saltato nella finestra dei conflitti
        auto f = keys.find(pathKey(itemPath(item)));
        if (f == keys.end()) return;
        FoOut& o = out[f->second];
        o.path = itemPath(created);
        o.done = del || !o.path.empty();
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** p) override {
        if (riid == IID_IUnknown || riid == __uuidof(IFileOperationProgressSink)) { *p = this; AddRef(); return S_OK; }
        *p = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refs);
        if (!r) delete this;
        return (ULONG)r;
    }
    HRESULT STDMETHODCALLTYPE StartOperations() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE FinishOperations(HRESULT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PreRenameItem(DWORD, IShellItem*, LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PostRenameItem(DWORD, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PreMoveItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PostMoveItem(DWORD, IShellItem* it, IShellItem*, LPCWSTR, HRESULT hr, IShellItem* nw) override {
        note(it, hr, nw, false);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreCopyItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PostCopyItem(DWORD, IShellItem* it, IShellItem*, LPCWSTR, HRESULT hr, IShellItem* nw) override {
        note(it, hr, nw, false);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreDeleteItem(DWORD, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PostDeleteItem(DWORD, IShellItem* it, HRESULT hr, IShellItem* nw) override {
        note(it, hr, nw, true);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreNewItem(DWORD, IShellItem*, LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PostNewItem(DWORD, IShellItem*, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE UpdateProgress(UINT, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ResetTimer() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PauseTimer() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ResumeTimer() override { return S_OK; }
};

// Nel Cestino come Explorer: la conferma segue l'impostazione del Cestino; se il Cestino non c'è o il
// file è troppo grande, la shell avvisa che l'eliminazione è definitiva (FOF_WANTNUKEWARNING)
static DWORD recycleFlags() {
    SHELLSTATE ss = {};
    SHGetSetSettings(&ss, SSF_NOCONFIRMRECYCLE, FALSE);
    return FOF_ALLOWUNDO | FOFX_RECYCLEONDELETE | FOF_WANTNUKEWARNING | (ss.fNoConfirmRecycle ? FOF_NOCONFIRMATION : 0);
}

// Esegue le operazioni (tutte dello stesso tipo) e restituisce l'esito di ogni elemento. Sul thread.
static std::vector<FoOut> foRun(HWND hwnd, int kind, const std::vector<FoItem>& items, HRESULT& hr, bool& aborted) {
    std::vector<FoOut> out(items.size());
    hr = E_FAIL;
    aborted = false;
    IFileOperation* fo = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fo)))) return out;
    fo->SetOwnerWindow(hwnd);
    fo->SetOperationFlags(kind == FK_RECYCLE ? recycleFlags() : FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR);
    OpSink* sink = new OpSink(items.size());
    std::unordered_map<std::wstring, IShellItem*> dirs;
    int queued = 0;
    for (size_t i = 0; i < items.size(); i++) {
        IShellItem* si = nullptr;
        if (FAILED(SHCreateItemFromParsingName(items[i].path.c_str(), nullptr, IID_PPV_ARGS(&si)))) continue;
        sink->keys[pathKey(itemPath(si))] = (int)i; // il percorso come lo scrive la shell, per riconoscerlo dopo
        HRESULT q = E_FAIL;
        if (kind == FK_RECYCLE) {
            q = fo->DeleteItem(si, nullptr);
        } else {
            IShellItem*& d = dirs[pathKey(items[i].dir)];
            if (!d) SHCreateItemFromParsingName(items[i].dir.c_str(), nullptr, IID_PPV_ARGS(&d));
            const wchar_t* nn = items[i].name.empty() ? nullptr : items[i].name.c_str();
            if (d) q = kind == FK_MOVE ? fo->MoveItem(si, d, nn, nullptr) : fo->CopyItem(si, d, nn, nullptr);
        }
        if (SUCCEEDED(q)) queued++;
        si->Release();
    }
    if (queued) {
        DWORD cookie = 0;
        bool adv = SUCCEEDED(fo->Advise(sink, &cookie));
        hr = fo->PerformOperations();
        BOOL ab = FALSE;
        fo->GetAnyOperationsAborted(&ab);
        aborted = ab != FALSE || hr == COPYENGINE_E_USER_CANCELLED || hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) || hr == E_ABORT;
        if (adv) fo->Unadvise(cookie);
    }
    for (auto& d : dirs) if (d.second) d.second->Release();
    fo->Release();
    out.swap(sink->out);
    sink->Release();
    // Un'eliminazione annullata nella finestra "File in uso" arriva al sink con un esito positivo:
    // conta il disco, non la shell. Se l'elemento è ancora al suo posto, non è stato eliminato.
    if (kind == FK_RECYCLE && queued)
        for (size_t i = 0; i < items.size(); i++)
            if (out[i].done && pathExists(items[i].path)) { out[i].done = false; out[i].path.clear(); }
    // Uno spostamento che la shell non ha raccontato: se l'originale non c'è più ed è arrivato, è fatto
    if (kind == FK_MOVE && queued)
        for (size_t i = 0; i < items.size(); i++) {
            if (out[i].done) continue;
            std::wstring to = joinPath(items[i].dir, items[i].name.empty() ? leafOf(items[i].path) : items[i].name.c_str());
            if (!pathExists(items[i].path) && pathExists(to)) { out[i].done = true; out[i].path = to; }
        }
    return out;
}

// --- lavori sul thread ---------------------------------------------------------------------
enum { J_COPY, J_MOVE, J_PASTE, J_RECYCLE, J_INVERT };
struct FileJob {
    HWND hwnd = nullptr;
    int kind = 0;
    std::vector<std::wstring> src;  // elementi su cui agire (J_PASTE: letti dagli appunti sul thread)
    std::wstring dest, destName;    // cartella di destinazione e il suo nome per l'avviso
    bool notify = false;            // la destinazione non è la cartella aperta: l'esito non si vede
    bool redo = false;              // J_INVERT: ripetere invece di annullare
    bool move = false;              // J_PASTE: gli appunti vengono da Taglia
    bool shell = false, shellOk = false; // J_PASTE: elementi senza percorso, incollati dalla shell; riuscito
    bool aborted = false;
    HRESULT hr = S_OK;
    UndoOp op;                      // J_INVERT: da invertire; alla fine, per tutti: cosa è successo
    UndoOp left;                    // J_INVERT: elementi non riusciti per un motivo che può passare (da riprovare)
    int asked = 0, done = 0;        // elementi chiesti, fatti davvero
    std::vector<std::wstring> fails; // perché qualche elemento non è stato fatto (annulla, ripeti)
    uint32_t pushMark = 0;          // J_INVERT: UH.pushes alla partenza
    double ms = 0;
};

// Copia o spostamento verso j->dest
static void runTransfer(FileJob* j, bool move) {
    std::vector<FoItem> items;
    std::vector<char> existed;
    for (const std::wstring& s : j->src) {
        if (move && samePath(parentPath(s), j->dest)) continue; // già lì: niente da fare (incolla dopo Taglia)
        items.push_back(FoItem{ s, j->dest, L"" });
        existed.push_back(pathExists(joinPath(j->dest, leafOf(s))));
    }
    j->asked = (int)items.size();
    j->op.what = move ? UW_MOVE : UW_COPY;
    if (items.empty()) return;
    std::vector<FoOut> out = foRun(j->hwnd, move ? FK_MOVE : FK_COPY, items, j->hr, j->aborted);
    for (size_t i = 0; i < items.size(); i++) {
        if (!out[i].done) continue;
        j->done++;
        UndoItem it;
        it.kind = move ? IK_MOVED : IK_CREATED;
        if (move) it.a = items[i].path;
        it.b = out[i].path;
        bool dir = false;
        statPath(it.b, it.size, it.mtime, dir);
        if (dir) it.flags |= IF_DIR;
        // Stesso nome di un elemento che c'era già: l'ha sostituito o ci si è unito
        if (existed[i] && samePath(it.b, joinPath(j->dest, leafOf(items[i].path)))) it.flags |= IF_REPLACED;
        j->op.items.push_back(std::move(it));
    }
}

static void runRecycle(FileJob* j) {
    std::vector<FoItem> items;
    for (const std::wstring& s : j->src) items.push_back(FoItem{ s, L"", L"" });
    j->asked = (int)items.size();
    j->op.what = UW_DELETE;
    std::vector<FoOut> out = foRun(j->hwnd, FK_RECYCLE, items, j->hr, j->aborted);
    for (size_t i = 0; i < items.size(); i++) {
        if (!out[i].done) continue;
        j->done++;
        if (out[i].path.empty()) continue; // eliminato definitivamente (la shell l'ha chiesto): non si annulla
        UndoItem it;
        it.kind = IK_RECYCLED;
        it.a = items[i].path;
        it.b = out[i].path;
        j->op.items.push_back(std::move(it));
    }
}

// Incolla: gli appunti si leggono qui, perché il proprietario (Explorer, un'altra app) può tardare
static void runPaste(FileJob* j) {
    DWORD seq = GetClipboardSequenceNumber();
    IDataObject* d = nullptr;
    if (FAILED(OleGetClipboard(&d)) || !d) return;
    hdropPaths(d, j->src);
    FORMATETC fe = { (CLIPFORMAT)RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM m = {};
    if (SUCCEEDED(d->GetData(&fe, &m))) {
        DWORD* e = (DWORD*)GlobalLock(m.hGlobal);
        if (e) j->move = (*e & DROPEFFECT_MOVE) && !(*e & DROPEFFECT_COPY); // Taglia: solo "sposta"
        GlobalUnlock(m.hGlobal);
        ReleaseStgMedium(&m);
    }
    // Senza percorsi (dentro uno zip, telefoni, allegati): come un rilascio, li estrae la shell.
    // Quanti siano non si sa e non si annulla, come il rilascio da uno zip. Dopo Taglia gli
    // appunti restano: Drop dice S_OK anche se l'utente annulla, e svuotarli perderebbe il Taglia.
    if (j->src.empty()) {
        j->shell = true;
        j->shellOk = shellDropInto(j->hwnd, d, j->dest, j->move ? DROPEFFECT_MOVE : DROPEFFECT_COPY, POINTL{}, j->hr);
        d->Release();
        return;
    }
    d->Release();
    runTransfer(j, j->move);
    // Come Explorer: dopo Taglia e Incolla gli appunti si svuotano (se nessuno li ha cambiati intanto)
    if (j->move && j->done > 0 && GetClipboardSequenceNumber() == seq && OpenClipboard(nullptr)) {
        EmptyClipboard();
        CloseClipboard();
    }
}

// Ripristinato un elemento del Cestino ($R…): via anche i suoi dati ($I…, percorso d'origine e data),
// altrimenti il Cestino mostrerebbe ancora una voce vuota
static void recycleInfoDelete(const std::wstring& r) {
    const wchar_t* l = leafOf(r);
    if (wcsncmp(l, L"$R", 2) != 0) return;
    DeleteFileW(longPath(joinPath(parentPath(r), (L"$I" + std::wstring(l + 2)).c_str())).c_str());
}

// Annulla o ripete: l'inverso di ogni elemento, dopo aver controllato che il disco sia come l'avevamo
// lasciato. Quello che riesce è la nuova operazione (da ripetere, o da annullare di nuovo).
// Quello che non riesce per un motivo che può passare (file in uso, annullato nella finestra della
// shell, permessi) resta da annullare: buttarlo vorrebbe dire non poterlo più rimettere a posto.
// Quello che non potrà mai riuscire (modificato dopo, sparito, sostituito) si lascia andare,
// altrimenti resterebbe in cima alla pila e la cronologia più vecchia diventerebbe irraggiungibile.
static void runInvert(FileJob* j) {
    UndoOp in = std::move(j->op);
    j->op = UndoOp{ in.what, {} };
    j->left = UndoOp{ in.what, {} };
    j->asked = (int)in.items.size();
    auto fail = [&](std::wstring s) { j->fails.push_back(std::move(s)); };
    auto keep = [&](const UndoItem& it) { j->left.items.push_back(it); };
    auto gone = [&](const std::wstring& p) { return std::wstring(leafOf(p)) + L" non esiste più"; };
    // Il posto dove l'elemento torna: la cartella deve esserci, il nome deve essere libero
    auto placeFree = [&](const std::wstring& to) {
        std::wstring dir = parentPath(to);
        if (!pathExists(dir)) { fail(L"la cartella " + std::wstring(leafOf(dir)) + L" non esiste più"); return false; }
        if (pathExists(to)) { fail(L"in " + parentName(to) + L" c'è già un elemento chiamato " + leafOf(to)); return false; }
        return true;
    };
    std::vector<FoItem> moves, recycles;
    std::vector<int> moveRestore; // per ogni spostamento con la shell: 1 se è un ripristino dal Cestino
    std::vector<const UndoItem*> moveSrc, recycleSrc; // l'elemento d'origine, da tenere se la shell non ce la fa
    for (const UndoItem& it : in.items) {
        switch (it.kind) {
        case IK_MOVED:
        case IK_RECYCLED: {
            bool restore = it.kind == IK_RECYCLED;
            if (!pathExists(it.b)) { fail(restore ? std::wstring(leafOf(it.a)) + L" non è più nel Cestino" : gone(it.b)); break; }
            if (!restore && (it.flags & IF_REPLACED) && (it.flags & IF_DIR)) {
                fail(std::wstring(leafOf(it.b)) + L" era stata unita a una cartella che c'era già");
                break;
            }
            if (!placeFree(it.a)) break;
            // Sullo stesso volume è un cambio di nome: immediato e senza sostituire niente.
            // Altrimenti (o se serve un permesso, o un file è in uso) lo fa la shell, con le sue finestre.
            if (MoveFileExW(longPath(it.b).c_str(), longPath(it.a).c_str(), 0)) {
                if (restore) recycleInfoDelete(it.b);
                j->done++;
                UndoItem r;
                r.kind = restore ? IK_RESTORED : IK_MOVED;
                r.flags = it.flags & IF_DIR;
                if (!restore) r.a = it.b;
                r.b = it.a;
                bool dir = false;
                if (restore) statPath(r.b, r.size, r.mtime, dir); // per riconoscerlo se cambia
                j->op.items.push_back(std::move(r));
            } else {
                DWORD e = GetLastError();
                if (e == ERROR_ALREADY_EXISTS || e == ERROR_FILE_EXISTS) { fail(L"in " + parentName(it.a) + L" c'è già un elemento chiamato " + leafOf(it.a)); break; }
                moves.push_back(FoItem{ it.b, parentPath(it.a), leafOf(it.a) });
                moveRestore.push_back(restore);
                moveSrc.push_back(&it);
            }
            break;
        }
        case IK_CREATED:
        case IK_RESTORED: {
            if (!pathExists(it.b)) { fail(gone(it.b)); break; }
            if (it.kind == IK_CREATED && (it.flags & IF_REPLACED)) {
                fail(std::wstring(leafOf(it.b)) + L" aveva sostituito un elemento che c'era già");
                break;
            }
            uint64_t sz = 0, mt = 0;
            bool dir = false;
            statPath(it.b, sz, mt, dir);
            // Un file cambiato dopo essere stato copiato (o ripristinato) è lavoro dell'utente: non si tocca
            if (!dir && (sz != it.size || mt != it.mtime)) {
                fail(std::wstring(leafOf(it.b)) + L" è stato modificato dopo");
                break;
            }
            if (dir && (it.flags & IF_EMPTYOK) && RemoveDirectoryW(longPath(it.b).c_str())) {
                j->done++;
                UndoItem r;
                r.kind = IK_RMDIR;
                r.flags = it.flags;
                r.a = it.b;
                j->op.items.push_back(std::move(r));
                break;
            }
            recycles.push_back(FoItem{ it.b, L"", L"" });
            recycleSrc.push_back(&it);
            break;
        }
        case IK_RMDIR: {
            if (!placeFree(it.a)) break;
            if (CreateDirectoryW(longPath(it.a).c_str(), nullptr)) {
                j->done++;
                UndoItem r;
                r.kind = IK_CREATED;
                r.flags = IF_DIR | IF_EMPTYOK;
                r.b = it.a;
                j->op.items.push_back(std::move(r));
            } else {
                DWORD e = GetLastError();
                fail(std::wstring(leafOf(it.a)) + L": " + errWhy(e) + L" (codice " + std::to_wstring(e) + L")");
                if (e != ERROR_ALREADY_EXISTS && e != ERROR_PATH_NOT_FOUND) keep(it); // permessi, rete: può passare
            }
            break;
        }
        }
    }
    HRESULT hr;
    bool aborted;
    if (!moves.empty()) {
        std::vector<FoOut> out = foRun(j->hwnd, FK_MOVE, moves, hr, aborted);
        for (size_t k = 0; k < moves.size(); k++) {
            if (!out[k].done) { fail(moves[k].name + (aborted ? L": interrotto" : L": non spostato")); keep(*moveSrc[k]); continue; }
            j->done++;
            UndoItem r;
            r.b = out[k].path;
            if (moveRestore[k]) {
                recycleInfoDelete(moves[k].path);
                r.kind = IK_RESTORED;
                bool dir = false;
                statPath(r.b, r.size, r.mtime, dir);
            } else {
                r.kind = IK_MOVED;
                r.a = moves[k].path;
            }
            j->op.items.push_back(std::move(r));
        }
    }
    if (!recycles.empty()) {
        std::vector<FoOut> out = foRun(j->hwnd, FK_RECYCLE, recycles, hr, aborted);
        for (size_t k = 0; k < recycles.size(); k++) {
            if (!out[k].done) { fail(std::wstring(leafOf(recycles[k].path)) + (aborted ? L": interrotto" : L": non eliminato")); keep(*recycleSrc[k]); continue; }
            j->done++;
            if (out[k].path.empty()) continue; // senza Cestino: eliminato per sempre, non si ripete
            UndoItem r;
            r.kind = IK_RECYCLED;
            r.a = recycles[k].path;
            r.b = out[k].path;
            j->op.items.push_back(std::move(r));
        }
    }
}

static DWORD WINAPI fileJobThread(void* p) {
    FileJob* j = (FileJob*)p;
    double t0 = nowMs();
    HRESULT ci = OleInitialize(nullptr); // STA, e gli appunti OLE per Incolla
    switch (j->kind) {
    case J_COPY:    runTransfer(j, false); break;
    case J_MOVE:    runTransfer(j, true); break;
    case J_PASTE:   runPaste(j); break;
    case J_RECYCLE: runRecycle(j); break;
    case J_INVERT:  runInvert(j); break;
    }
    if (SUCCEEDED(ci)) OleUninitialize();
    j->ms = nowMs() - t0;
    if (!PostMessageW(j->hwnd, WM_APP_FILEOP, 0, (LPARAM)j)) delete j;
    return 0;
}

static bool fileJobStart(FileJob* j) {
    j->hwnd = A.hwnd;
    HANDLE h = CreateThread(nullptr, 0, fileJobThread, j, 0, nullptr);
    if (!h) return false;
    CloseHandle(h);
    if (j->kind == J_INVERT) UH.busy = true;
    else UH.running++;
    return true;
}

// --- comandi (thread dell'interfaccia) -----------------------------------------------------
// Rilascio di file veri: copia o sposta con l'esito di ogni elemento
static bool fileTransfer(bool move, const std::vector<std::wstring>& src, const std::wstring& dest, const std::wstring& destName, bool notify) {
    if (!oleReady()) return false;
    FileJob* j = new FileJob;
    j->kind = move ? J_MOVE : J_COPY;
    j->src = src;
    j->dest = dest;
    j->destName = destName;
    j->notify = notify;
    if (fileJobStart(j)) return true;
    delete j;
    return false;
}

// Ctrl+V, Incolla: i file degli appunti nella cartella aperta, o in dest ("Incolla" su una cartella
// nel menu completo; l'avviso dice dove sono finiti, perché non si vedono)
static void pasteFiles(const std::wstring& dest) {
    if (!A.cur || A.cur->isDrives || A.cur->path.empty() || zipReadOnly() || !oleReady()) return;
    FileJob* j = new FileJob;
    j->kind = J_PASTE;
    j->dest = dest.empty() ? A.cur->path : dest;
    j->destName = tabTitleFor(j->dest);
    if (!fileJobStart(j)) delete j;
}

// Canc, "Sposta nel Cestino" del menu: nel Cestino. Shift+Canc resta alla shell: è definitivo e lo chiede lei.
static void deleteSelected() {
    if (!A.cur || zipReadOnly()) return;
    if (A.cur->isDrives || GetKeyState(VK_SHIFT) < 0 || !oleReady()) { shellVerb("delete"); return; }
    std::vector<std::wstring> src;
    for (uint32_t ei : selectedEntries()) src.push_back(joinPath(A.cur->path, A.cur->name(A.cur->e[ei])));
    recyclePaths(std::move(src));
}

// Questi percorsi nel Cestino (anche "Elimina" del menu completo della shell, col suo thread)
static void recyclePaths(std::vector<std::wstring> src) {
    if (src.empty() || !oleReady()) return;
    FileJob* j = new FileJob;
    j->kind = J_RECYCLE;
    j->src = std::move(src);
    if (!fileJobStart(j)) delete j;
}

// Ctrl+Z (redo = false), Ctrl+Y · Ctrl+Shift+Z (redo = true)
static void undoRun(bool redo) {
    if (UH.busy) { notice(redo ? L"Ripetizione in corso" : L"Annullamento in corso", true); return; }
    if (UH.running) {
        notice(redo ? L"Operazione sui file in corso: si potrà ripetere quando finisce"
                    : L"Operazione sui file in corso: si potrà annullare quando finisce", true);
        return;
    }
    std::vector<UndoOp>& st = redo ? UH.redo : UH.undo;
    if (st.empty()) { notice(redo ? L"Niente da ripetere" : L"Niente da annullare", true); return; }
    if (!oleReady()) return;
    FileJob* j = new FileJob;
    j->kind = J_INVERT;
    j->redo = redo;
    j->pushMark = UH.pushes;
    j->op = std::move(st.back());
    st.pop_back();
    if (!fileJobStart(j)) { st.push_back(std::move(j->op)); delete j; }
}

// --- avvisi --------------------------------------------------------------------------------
// La shell dice che l'utente ha annullato: lo sa già, niente avviso
static bool hrCancelled(HRESULT hr) { return hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) || hr == E_ABORT; }

// Esito di un rilascio o di un incolla in un'altra cartella: solo quello che è arrivato davvero
static void transferNotice(DWORD fx, bool ok, bool cancelled, bool notify, int done, size_t asked, const std::wstring& name) {
    wchar_t b[300];
    b[0] = 0;
    const wchar_t* what = fx == DROPEFFECT_MOVE ? L"spostare" : fx == DROPEFFECT_COPY ? L"copiare" : L"collegare";
    if (!ok) {
        if (!cancelled) _snwprintf(b, 299, L"Impossibile %s in %s", what, name.c_str()); // annullato: l'utente lo sa già
    } else if (notify && done > 0) {
        int fi = fx == DROPEFFECT_MOVE ? 0 : fx == DROPEFFECT_COPY ? 1 : 2;
        static const wchar_t* one[3]  = { L"Spostato in %s", L"Copiato in %s", L"Collegamento creato in %s" };
        static const wchar_t* oneOf[3] = { L"1 elemento spostato in %s", L"1 elemento copiato in %s", L"1 collegamento creato in %s" };
        static const wchar_t* many[3] = { L"%d elementi spostati in %s", L"%d elementi copiati in %s", L"%d collegamenti creati in %s" };
        if (done > 1) _snwprintf(b, 299, many[fi], done, name.c_str());
        else _snwprintf(b, 299, asked == 1 ? one[fi] : oneOf[fi], name.c_str());
    }
    b[299] = 0;
    if (b[0]) notice(b, ok);
}

// Esito di un annullamento o di una ripetizione: cosa è stato fatto, dove, e cosa no
static void invertNotice(const FileJob* j) {
    static const wchar_t* noun[UW_COUNT] = { L"lo spostamento", L"la copia", L"l'eliminazione", L"la rinomina",
                                             L"la creazione", L"il collegamento" };
    static const bool fem[UW_COUNT] = { false, true, true, true, true, false };
    int w = j->op.what;
    std::wstring head = std::wstring(j->redo ? (fem[w] ? L"Ripetuta " : L"Ripetuto ") : (fem[w] ? L"Annullata " : L"Annullato ")) + noun[w];
    std::wstring why;
    if (!j->fails.empty()) {
        why = j->fails[0];
        if (j->fails.size() > 1) why += L" (e altri " + std::to_wstring(j->fails.size() - 1) + L")";
        if (!j->left.items.empty()) why += L"; si può riprovare";
    }
    if (j->done == 0) {
        std::wstring t = std::wstring(j->redo ? L"Impossibile ripetere " : L"Impossibile annullare ") + noun[w];
        notice(why.empty() ? t : t + L": " + why, false);
        return;
    }
    if (j->done < j->asked) {
        notice(head + L" solo in parte (" + std::to_wstring(j->done) + L" su " + std::to_wstring(j->asked) + L")" +
               (why.empty() ? L"" : L": " + why), false);
        return;
    }
    // Tutto fatto. Dove sono finiti gli elementi: il primo basta (vengono da una cartella sola)
    const UndoItem* first = j->op.items.empty() ? nullptr : &j->op.items[0];
    std::wstring name = first ? std::wstring(leafOf(first->kind == IK_RMDIR ? first->a : first->kind == IK_RECYCLED ? first->a : first->b)) : L"";
    std::wstring where = first && first->kind != IK_RECYCLED && first->kind != IK_RMDIR ? tabTitleFor(parentPath(first->b)) : L"";
    int n = j->done;
    bool inBin = n == (int)j->op.items.size(); // senza Cestino (rete) l'eliminazione è definitiva
    for (const UndoItem& it : j->op.items) if (it.kind != IK_RECYCLED) inBin = false;
    std::wstring nEl = std::to_wstring(n) + L" elementi";
    std::wstring t;
    switch (w) {
    case UW_MOVE:
    case UW_DELETE:
    case UW_COPY:
        if (inBin) t = n == 1 ? head + L" di " + name + L": nel Cestino" : head + L": " + nEl + L" nel Cestino";
        else if (!first) t = n == 1 ? head + L": elemento eliminato" : head + L": " + nEl + L" eliminati";
        else t = n == 1 ? head + L" di " + name + L": di nuovo in " + where : head + L": " + nEl + L" di nuovo in " + where;
        break;
    case UW_RENAME:
        t = head + L": di nuovo " + name;
        break;
    case UW_MKDIR:
        t = head + L" di " + name;
        if (first && first->kind == IK_RECYCLED) t += L": nel Cestino perché non era vuota";
        break;
    case UW_LINK:
        if (inBin) t = n == 1 ? head + L" " + name + L": nel Cestino" : head + L": " + std::to_wstring(n) + L" collegamenti nel Cestino";
        else t = n == 1 ? head + L" " + name + L": di nuovo in " + where : head + L": " + std::to_wstring(n) + L" collegamenti di nuovo in " + where;
        break;
    }
    notice(t, true);
}

// --- chiusura con operazioni in corso --------------------------------------------------------
// Uscire ucciderebbe i thread a metà di PerformOperations: copie parziali, e l'operazione non
// arriverebbe nella cronologia. La finestra sparisce subito (l'utente ha chiesto di chiudere) e
// il processo esce quando l'ultima operazione è finita. La finestra di avanzamento di Windows
// resta visibile: un proprietario nascosto (non ridotto a icona) non nasconde le sue finestre.
static bool closeDeferred() {
    if (!UH.running && !UH.busy && !shellMenuBusy()) return false; // anche un comando della shell dal suo menu
    A.closePending = true;
    ShowWindow(A.hwnd, SW_HIDE);
    fdRelease(); // nascosta: i dialoghi tornano alle altre finestre di [z]f
    return true;
}

// Dopo ogni operazione finita: se la finestra aspettava di chiudersi ed è l'ultima, si chiude
static void closeIfIdle() {
    if (A.closePending && !UH.running && !UH.busy && !shellMenuBusy()) DestroyWindow(A.hwnd);
}

// Fine di un lavoro (WM_APP_FILEOP)
static void onFileJob(FileJob* j) {
    A.changeAt = nowMs();
    // Le dimensioni delle cartelle toccate (e di quelle che le contengono) vanno ricontate. Con
    // migliaia di voci, una per una costava 76 ms sul thread UI (20.000 file spostati): si dimentica
    // tutto, e i totali si rifanno in background
    if (j->src.size() + 2 * j->op.items.size() > 256) {
        dirSizeForgetAll();
    } else {
        for (const std::wstring& s : j->src) dirSizeForget(s);
        dirSizeForget(j->dest);
        for (const UndoItem& it : j->op.items) { dirSizeForget(it.a); dirSizeForget(it.b); }
    }
    // Nell'anteprima di una cartella in rete non c'è l'osservatore: si rilegge qui (in locale lo fa
    // lui, e rileggere anche qui la leggerebbe due volte). Le stesse cartelle che previewWatch salta
    ListPane* pp = previewPaneFor(j->dest);
    if (!pp && !j->src.empty()) pp = previewPaneFor(parentPath(j->src[0]));
    // Annulla e ripeti non hanno src né dest: valgono le cartelle dei loro elementi, e il cursore va
    // sull'elemento tornato, come nella lista (qui anche in locale: la rilettura dell'osservatore
    // può essere partita prima che l'elemento tornasse)
    bool pvSelect = false;
    if (j->kind == J_INVERT && pvPane()) {
        for (const UndoItem& it : j->op.items) {
            if (it.kind == IK_RECYCLED || it.kind == IK_RMDIR || it.b.empty() || !previewPaneFor(parentPath(it.b))) continue;
            PV.selectAfter = leafOf(it.b);
            pp = pvPane();
            pvSelect = true;
            break;
        }
        for (size_t i = 0; !pp && i < j->op.items.size(); i++) {
            const UndoItem& it = j->op.items[i];
            if (!it.b.empty()) pp = previewPaneFor(parentPath(it.b));
            if (!pp && !it.a.empty()) pp = previewPaneFor(parentPath(it.a));
        }
    }
    if (pp && (pvSelect || slowPath(pp->cur->path))) previewRefresh();
    if (j->kind == J_INVERT) {
        UH.busy = false;
        A.undoMs = j->ms;
        invertNotice(j);
        // Il cursore sull'elemento tornato nella cartella aperta
        for (const UndoItem& it : j->op.items) {
            if (it.kind == IK_RECYCLED || it.kind == IK_RMDIR || !A.cur || !samePath(parentPath(it.b), A.cur->path)) continue;
            A.selectAfterRefresh = leafOf(it.b);
            requestRefresh();
            break;
        }
        // Un annullamento va tra i Ripeti solo se nel frattempo non è stata fatta un'operazione
        // nuova: quella ha già svuotato i Ripeti, e questo verrebbe ripetuto sopra di lei. Le
        // operazioni nuove non aspettano l'annullamento: niente da far aspettare all'utente.
        if (!j->op.items.empty() && (j->redo || UH.pushes == j->pushMark)) {
            std::vector<UndoOp>& st = j->redo ? UH.undo : UH.redo;
            st.push_back(std::move(j->op));
            undoTrim(st);
        }
        if (!j->left.items.empty()) { // da riprovare: torna in cima alla pila da cui veniva
            std::vector<UndoOp>& st = j->redo ? UH.redo : UH.undo;
            st.push_back(std::move(j->left));
        }
        undoSave();
    } else {
        UH.running--;
        A.fileOpMs = j->ms;
        if (j->kind != J_RECYCLE) {
            DWORD fx = (j->kind == J_MOVE || (j->kind == J_PASTE && j->move)) ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
            bool ok = j->done > 0 || (SUCCEEDED(j->hr) && j->asked > 0);
            bool notify = j->notify || (A.cur && !samePath(j->dest, A.cur->path)); // nel frattempo si è cambiata cartella
            if (j->shell) transferNotice(fx, j->shellOk, hrCancelled(j->hr), notify, -1, 0, j->destName);
            else if (j->asked > 0) transferNotice(fx, ok, j->aborted, notify, j->done, (size_t)j->asked, j->destName);
        }
        undoPush(std::move(j->op));
    }
    delete j;
    A.dirty = true;
    closeIfIdle();
}

// --- rinomina e nuova cartella: chiamate dirette, l'esito è certo ---------------------------
struct FsJob { HWND hwnd; int op; std::wstring a, b, name; };

static DWORD WINAPI fsThread(void* p) {
    FsJob* j = (FsJob*)p;
    BOOL ok = j->op == FS_RENAME ? MoveFileExW(longPath(j->a).c_str(), longPath(j->b).c_str(), 0)
                                 : CreateDirectoryW(longPath(j->a).c_str(), nullptr);
    DWORD err = ok ? 0 : GetLastError();
    if (!PostMessageW(j->hwnd, WM_APP_FSDONE, err, (LPARAM)j)) delete j;
    return 0;
}

static void fsStart(int op, const std::wstring& a, const std::wstring& b, const std::wstring& name) {
    FsJob* j = new FsJob{ A.hwnd, op, a, b, name };
    HANDLE h = CreateThread(nullptr, 0, fsThread, j, 0, nullptr);
    if (h) { CloseHandle(h); UH.running++; } else delete j;
}

static void onFsDone(DWORD err, FsJob* j) {
    UH.running--;
    if (err) {
        wchar_t buf[256];
        _snwprintf(buf, 255, L"%s: %s (codice %lu)", j->op == FS_RENAME ? L"Impossibile rinominare" : L"Impossibile creare la cartella",
                   errWhy(err), (unsigned long)err);
        buf[255] = 0;
        notice(buf, false);
    } else {
        UndoOp op;
        UndoItem it;
        if (j->op == FS_RENAME) {
            op.what = UW_RENAME;
            it.kind = IK_MOVED;
            it.a = j->a;
            it.b = j->b;
        } else {
            op.what = UW_MKDIR;
            it.kind = IK_CREATED;
            it.flags = IF_DIR | IF_EMPTYOK;
            it.b = j->a;
        }
        op.items.push_back(std::move(it));
        undoPush(std::move(op));
        // Nella cartella dell'anteprima (rinomina o nuova cartella fatte lì): si rilegge quella
        std::wstring dir = parentPath(j->op == FS_RENAME ? j->b : j->a);
        if (previewPaneFor(dir)) {
            PV.selectAfter = j->name;
            PV.renameAfter = j->op == FS_MKDIR;
            previewRefresh();
        } else if (A.cur && samePath(dir, A.cur->path)) {
            A.selectAfterRefresh = j->name; // il watcher ricarica: il cursore va sulla voce nuova
            A.renameAfterRefresh = j->op == FS_MKDIR;
            A.changeAt = nowMs();
            requestRefresh();
        }
    }
    delete j;
    A.dirty = true;
    closeIfIdle();
}
