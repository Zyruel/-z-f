# [z]f — come è fatto

Questo file è per chi legge o modifica il codice: come funziona ogni parte, perché è fatta così e
quanto costa. Cosa sa fare [z]f e le scorciatoie sono nel [README](../README.md); aspetto e
comportamento visibile in [`DESIGN.md`](DESIGN.md); la storia, versione per versione, in
[`NOVITA.md`](../NOVITA.md) e nei commit.

Ogni area ha tre parti: **come funziona** (funzioni, thread, file), **perché così** (le prove
fatte e le alternative scartate) e **Misurato:** (i numeri di oggi). Quando si cambia il codice si
aggiorna la sua area: una misura nuova sostituisce la vecchia, non le si aggiunge accanto.

## Nome tecnico

**[z]f** è il nome dell'applicazione. Dove le parentesi quadre darebbero problemi (file,
cartelle, registro, nomi interni di Windows) il nome tecnico è **z-f**: `z-f.exe`,
`%APPDATA%\z-f`, `z-f-log.txt`; nel codice C++ gli identificatori sono `ZF_...`.

Fino alla 0.15 si chiamava zx: le copie `build\zx-0.N.exe` sono di allora. Al primo avvio
`migrateLegacyData` (`settings.cpp`) sposta `%APPDATA%\zx` in `%APPDATA%\z-f`; quando si attiva o
disattiva "[z]f al posto di Explorer", `cleanupLegacyZx` (`integration.cpp`) toglie
l'integrazione del vecchio zx rimettendo i valori originali.

## Regole comuni del codice

Valgono per ogni area qui sotto, che quindi non le ripete. Le regole complete sono in CLAUDE.md e
in [`DESIGN.md`](DESIGN.md) §10.

- **Il thread dell'interfaccia non tocca mai il disco e non aspetta niente** (disco, shell, rete,
  vsync). Il lavoro lento va su un thread, che consegna il risultato alla finestra con
  `PostMessage` e un contatore di generazione: un risultato superato (l'utente è andato altrove)
  si scarta.
- **Unità lente e file solo online**, decisi in un posto solo (`app.cpp`, solo dal thread UI):
  `slowPath` dice se un percorso è su un'unità di rete o ottica (niente letture in background,
  sempre dopo le locali: barra laterale, "Vai a", miniature, dimensione delle cartelle);
  `onlineOnly` dice se un file è un segnaposto di OneDrive e simili, che non si legge per non
  scaricarlo.
- **Niente lavoro a vuoto:** un frame si disegna solo se `A.dirty` o se un'animazione ha impostato
  `A.animActive`; a riposo [z]f non usa CPU.
- **DLL al primo uso:** shell32, ole32, WIC, mpr, winhttp e bcrypt non si pagano all'avvio, o si
  pagano su un altro thread (dettagli in "Compilare: MSVC e zig").
- **Ogni funzione si misura:** i tempi vanno nel pannello F12, i casi lenti nel registro (vedi
  "Misure e diagnostica").

## Struttura del codice

Si compila un solo file, `src/z-f.cpp`, che include i moduli nell'ordine in cui dipendono l'uno
dall'altro (*unity build*): compilazione veloce, ottimizzazione su tutto il programma, nessun
costo di collegamento. Ogni modulo comincia con una riga che dice cosa fa; le dichiarazioni
condivise sono in `app.h`.

| Modulo | Cosa contiene |
|---|---|
| `base.h` | include di sistema, misura del tempo, registro dei rallentamenti |
| `theme.h` | colori, misure e tempi dell'interfaccia con un nome per ognuno (regole d'uso in `DESIGN.md`) |
| `gfx.cpp` | D3D11: device, swapchain flip-model, shader, batch di quad (rettangoli, glifi, icone in un unico vertex buffer), ritaglio (`clipBegin`) |
| `text.cpp` | glifi rasterizzati con GDI al primo uso in un atlante RGBA (ClearType), disegno del testo, casella di testo, testo negli appunti |
| `listing.cpp` | lettura delle cartelle su thread di lavoro, chiavi di ordinamento naturale |
| `zip.cpp` | archivi zip in sola lettura: directory centrale (zip64), albero delle cartelle in memoria, inflate e CRC nostri, estrazione su un thread |
| `lnk.cpp` | lettore dei collegamenti di Windows (.lnk), senza la shell; percorsi di rete di Windows |
| `app.h` / `app.cpp` | stato; layout, selezione, filtro, navigazione, ricariche |
| `columns.cpp` | colonne della lista: ordine, titoli, testo delle celle, quali si vedono, estensioni nascoste |
| `dirsize.cpp` | dimensione totale delle cartelle su un thread (urgenti per il tooltip, coda di sfondo per la colonna), cache |
| `links.cpp` | colonna Destinazione dei collegamenti: destinazioni lette su un thread, cache, clic ("Apri percorso file") |
| `watch.cpp` | aggiornamento in tempo reale (`ReadDirectoryChangesW`), compresi spostamento ed eliminazione della cartella aperta; la cartella dell'anteprima (`previewWatch`) |
| `icons.cpp` | icone della shell su un thread dedicato, atlante BGRA |
| `render.cpp` | disegno di un frame, voci di una lista (`drawListItems`, anche per l'anteprima), pannelli sovrapposti (`drawPanel`) e pannello F12 |
| `shell.cpp` | menu completo della shell e comandi della shell (su un thread loro), appunti, rinomina nella riga |
| `fileops.cpp` | operazioni sui file su un thread (`IFileOperation` con l'esito di ogni elemento, rinomina, nuova cartella), cronologia per annullare e ripetere |
| `tabs.cpp` | schede: scambio dello stato, barra delle schede, riapertura all'avvio |
| `sidebar.cpp` | barra laterale: preferiti (file in `%APPDATA%\z-f`), unità e percorsi di rete, letti su thread separati |
| `integration.cpp` | [z]f al posto di Explorer: chiavi in HKCU, salvataggio e ripristino dei valori precedenti |
| `update.cpp` | aggiornamento automatico dalle Release di GitHub (WinHTTP, JSON, versioni, impronta e firma con CNG), scambio dell'exe, riavvio di tutte le finestre, icona di stato |
| `filedlg.cpp` | dialoghi Apri e Salva degli altri programmi nella cartella di [z]f (`SetWinEventHook`, campo del nome da un thread) |
| `settings.cpp` | impostazioni e salvataggio atomico in background dei file in `%APPDATA%\z-f` |
| `chrome.cpp` | cornice della finestra: barra del titolo con le schede, pulsanti (anche quelli della riga del percorso), area non client e menu di sistema (`chromeSysMenu`: `DefWindowProc` non lo apre, perché la barra del titolo per lui è area client), icona della finestra |
| `crumbs.cpp` | percorso a segmenti cliccabili, menu delle sottocartelle |
| `tooltip.cpp` | tooltip: contenuto, informazioni lette su un thread (anche per l'anteprima), disegno |
| `preview.cpp` | pannello di anteprima: immagini (WIC), testo, cartelle come seconda lista (`ListPane`), informazioni; lettura e decodifica su un thread |
| `thumbs.cpp` | vista a icone: miniature (cache di Windows, WIC, shell, icone) su thread, atlante sulla GPU, per la lista e l'anteprima (`ThClient`); zoom e vista per cartella (`viste.txt`) |
| `options.cpp` | pannello delle opzioni, tabella delle scorciatoie (anche per i menu), versione |
| `goto.cpp` | "Vai a": cartelle recenti (`recenti.txt`), ricerca fuzzy, sottocartelle lette su un thread, pannello |
| `drag.cpp` | trascinamento dei file: sorgente (oggetto dati della shell), destinazione (IDropTarget), rilascio alla shell su un thread |
| `zipout.cpp` | dallo zip verso fuori: aprire, copiare e trascinare con i file estratti su un thread in `%TEMP%\z-f` |
| `listmouse.cpp` | lista col mouse: selezione col rettangolo, colonne ridimensionabili (trascinamento, adattamento al contenuto), bordi dei pannelli |
| `menu.cpp` | menu contestuale nostro (lista, voci della barra laterale, schede, rilascio col tasto destro, vista) |
| `hybrid.cpp` | WARP all'avvio, poi passaggio alla GPU |
| `input.cpp` | tastiera, mouse, messaggi della finestra |
| `main.cpp` | avvio, istanza unica e ciclo dei messaggi |
| `z-f.rc` / `z-f.ico` / `risorse.h` | risorse dell'exe: l'icona, disegnata da `strumenti\icona.py`; gli ID, in comune con `base.h` |

## Misure e diagnostica

**Come funziona.**

- **Registro dei rallentamenti:** `z-f-log.txt` accanto all'eseguibile, con l'ora di ogni evento:
  - messaggi oltre `LOG_MSG` (25 ms);
  - frame con preparazione oltre `LOG_FRAME` (16 ms) e Present oltre 16 ms;
  - attese del vsync oltre `LOG_VSYNC` (34 ms);
  - oggetti e comandi della shell sul thread UI oltre 25 ms (`hitchLog`), che il conteggio dei
    messaggi non vede;
  - i casi con una soglia loro: menu e comandi della shell sul loro thread (`LOG_MENU`), ricerca
    di "Vai a" (`LOG_GOTO`), lettura ed estrazione degli zip (`LOG_ZIP`).

  Le soglie sono tutte in `theme.h` (`LOG_*`). Le righe vanno in una coda e le scrive un thread:
  il registro stesso non tocca il disco dal thread dell'interfaccia. Il primo Present su una
  swapchain nuova della GPU (15–30 ms per il driver) si chiama "primo Present GPU" ed è normale.
- **Pannello F12:** l'adattatore usato, i tempi di ogni fase dell'avvio e di ogni funzione (quali,
  lo dice ogni area qui sotto), il messaggio più lento e l'attesa del vsync più lunga.
- **Barra dei tempi** (opzione `barra_tempi`, spenta di solito): lettura, ordinamento, filtro,
  costruzione del frame e avvio, dalla creazione del processo al primo frame.
- **Crash:** `crashFilter` (`main.cpp`) scrive codice e indirizzo dell'errore, relativo al modulo,
  in `z-f-crash.txt` accanto all'eseguibile.
- **Diagnostica del rendering:** `--gpu=hw` (solo GPU, attende il driver), `--gpu=warp` (solo
  rendering software).
- **Prove ripetibili** in `prove\` (avvio, istanza unica, dialoghi, appunti, zip, collegamenti,
  file solo online…), citate nelle aree che le usano; come si usano lo dice la skill `/prova`.

## Avvio, finestra e rendering

**Come funziona.**

- **Istanza unica** (`main.cpp`): se [z]f è già aperto, `z-f.exe <cartella>` consegna il percorso
  alla finestra esistente con `WM_COPYDATA` ed esce subito, prima di creare finestre o D3D. La
  finestra apre una scheda e va in primo piano; se la cartella è già aperta in una scheda (anche
  ancora in caricamento) passa a quella. Accetta anche un file (apre la cartella e lo seleziona) e
  la forma di Explorer `/select,<percorso>`. Con un'opzione `--` (`--new-window`, `--gpu=...`) si
  apre comunque una finestra separata, che non riceve le cartelle aperte da fuori. Prova:
  `prove\istanza.bat`.
- **Finestre separate** (opzione `finestre_separate`): la principale risponde a `WM_COPYDATA` con
  2 invece di 1 e chi ha avviato [z]f prosegue come una "Nuova finestra" (non principale), dopo
  aver chiuso il suo handle del mutex: altrimenti, chiusa la principale, il mutex resterebbe in
  vita e l'avvio successivo cercherebbe per 3 secondi una principale che non c'è. Per lo stesso
  motivo la principale chiude il mutex in `WM_DESTROY`, insieme alla proprietà `z-f.istanza`, e
  non a fine processo: uscire può durare secondi (vedi "Barra laterale", le voci lente). Prima di
  chiuderlo scrive i file in coda (`saveFlush`, con dentro `schede.txt`): la principale nuova può
  partire subito, e senza questo leggerebbe i file di prima. La finestra non c'è più e nessuno
  aspetta (eccezione scritta in DESIGN §10). Misurato: 0,2–0,8 ms ("chiusura: file salvati" nel
  registro, a ogni chiusura della principale); con "Schede riaperte all'avvio" la principale chiusa e
  riaperta subito (39 ms dopo) ritrova le sue tre schede. L'attesa di una
  principale non ancora creata è a tempo (`FWD_WAIT`, `FWD_POLL`): a giri di `Sleep(10)` durava
  4,7 s invece di 3, perché ogni `Sleep` dura un passo del timer di Windows (15,6 ms).
  Misurato: principale chiusa e cartella aperta subito dopo, la finestra nuova compare 20–23 ms
  dopo l'avvio del processo (prima 4,7 s). L'opzione la
  conosce solo la principale, che ha già letto le impostazioni: il processo nuovo non tocca il
  disco prima della finestra. L'interruttore può stare in un'altra finestra, che lo comunica a
  tutte con il messaggio registrato `z-f.finestre-separate`.
- **Finestra Win32** con swapchain D3D11 in modalità flip e latenza di un frame
  (`SetMaximumFrameLatency(1)` e l'oggetto d'attesa della swapchain). La finestra compare subito
  con lo sfondo del tema.
- **Rendering ibrido** (predefinito, `hybrid.cpp`): il primo frame lo disegna WARP, il renderer
  software di Direct3D. Quando WARP è pronto, un thread inizializza la GPU; poi il programma passa
  alla GPU senza sfarfallio: disegna su una seconda finestra figlia posta sotto la prima, aspetta
  che il compositore mostri quel frame, poi rimuove la vecchia. Gli atlanti di glifi e icone si
  copiano dal device WARP, che vive nella RAM, a quello della GPU invece di ricostruirli. Se in
  quel momento si sta usando o trascinando la finestra, il passaggio aspetta un momento tranquillo
  (al massimo 2 s).
- **GPU persa** (driver aggiornato o reimpostato): non si disegna finché un thread non crea un
  device nuovo; poi [z]f ricostruisce atlanti e texture invece di cadere.
- **Ciclo principale:** aspetta il vsync insieme ai messaggi (`MsgWaitForMultipleObjectsEx`),
  quindi un input sveglia subito il ciclo invece di restare in coda dietro a un frame. Senza vsync
  per 100 ms (finestra coperta) si disegna comunque.
- **Testo:** tutto disegnato dalla GPU, con un atlante di glifi rasterizzati da GDI al primo uso.
  Se Windows usa ClearType i glifi hanno tre coperture per pixel, una per subpixel, fuse con lo
  sfondo dallo shader con il dual-source blending (come il testo delle app di Windows);
  altrimenti scala di grigi con lo stesso percorso.
- **Icone e testo in un solo shader:** le icone stanno in un atlante BGRA letto dallo stesso
  shader del testo; le UV con x ≥ 1 indicano l'atlante delle icone.
- **Animazioni:** un solo meccanismo, l'avvicinamento esponenziale con una costante di tempo nota
  (`approach`, `ui::TAU_*`), più le durate fisse `ui::DUR_*`; si ridisegna solo finché qualcosa si
  muove. Quali animazioni ci sono e come si spengono: [`DESIGN.md`](DESIGN.md) §5.
- **Icona della finestra:** la classe della finestra registra l'icona generica di Windows;
  `appIconSet` (`chrome.cpp`) carica la nostra dopo il primo frame, come `dropRegister`, con
  `LoadImageW` alla misura grande e piccola del DPI della finestra, e la dà con `WM_SETICON`; di
  nuovo a ogni `WM_DPICHANGED`, liberando le vecchie. In F12 "icona della finestra"; oltre
  `LOG_MSG`, "icona della finestra lenta" nel registro.
- **Il file dell'icona** (`src\z-f.ico`, aspetto in `DESIGN.md`): risorsa dell'exe (`src\z-f.rc`,
  con l'ID `ZF_ICON` in `src\risorse.h`, incluso sia dal .rc sia da `base.h`), compilata da `rc`
  in `build.bat` e da zig stesso in `build_zig.bat`. La scrive `strumenti\icona.py`, uno script
  Pillow usato una volta come `strumenti\firma.cpp`, che legge i due colori da `theme.h`: 16 e 20
  px disegnate a pixel, le altre dalla stessa geometria agganciata ai pixel, disegnata a 4× e
  ridotta con una media esatta (BOX: niente aloni scuri). Ci sono le misure di ogni DPI comune fino
  a 225% (28 e 56 per 175%, 36 e 72 per 225%), tutte in PNG. Misurato (novembre 2026): il .ico
  pesa 8,7 KB, contro 45,7 KB per le sole misure fino a 64 in BMP a 32 bit, e l'exe 37 KB in meno.
  La prima decodifica PNG del processo porta "icona della finestra" da 1,2 a 2,8 ms (mediane di
  10 avvii alternati, `prove\avvio.bat`), dopo il primo frame: il contenuto resta a 52–53 ms. Poi
  ogni misura costa 0,06–0,11 ms, come in BMP (`prove\icona-misure.cpp`). WindowsCodecs.dll non
  si aggiunge, perché la carica comunque la shell. Le misure che mancavano Windows le ricavava
  riducendone un'altra, con bordi sfumati e un alone scuro (le immagini di `prove\icona-misure.cpp`).

**Perché così.**

- WARP non deve caricare nessun driver, quindi il primo frame non aspetta la GPU, che su un PC di
  prova costa circa 140 ms. Su schermi grandi però WARP da solo sarebbe troppo lento (circa 15 ms
  a frame a 2560×1369, contro 0,2 ms della GPU): per questo poi si passa alla GPU.
- Il thread della GPU parte solo *dopo* che WARP è pronto: caricare le DLL del driver tiene
  occupato il loader lock di Windows, e farlo in parallelo bloccava WARP (misurato: da 25 a
  98 ms).
- L'icona nostra si carica dopo il primo frame: le due `LoadImageW` costano 1,3–2,7 ms e prima di
  `CreateWindowEx` ritardavano tutto l'avvio (1,6–2,8 ms, contro 0,0 dell'icona generica).
- Con la finestra coperta l'attesa del vsync può arrivare a 100 ms: per questo il ciclo aspetta
  vsync e messaggi insieme.

**Misurato:**

- RTX 5070 Ti: contenuto visibile a 70–81 ms, passaggio alla GPU intorno ai 200 ms;
- GPU persa (simulata): ripristino in 58 ms, di cui 36 sul thread dell'interfaccia;
- memoria (novembre 2026, RTX 5070 Ti, elenco, `prove\memoria.ps1`): una finestra con una scheda
  occupa 70 MB di working set e 84 MB di memoria privata, più 27 MB sulla GPU; con 11 schede
  (System32, SysWOW64, Program Files…) 78 e 88 MB, quindi meno di 1 MB a scheda. Quasi tutto è il
  driver della GPU caricato nel processo: con `--gpu=warp` la stessa finestra sta in 39 MB di
  working set e 22 di privata. Dopo il passaggio alla GPU WARP si scarica (`D3D10Warp.dll` non
  resta tra i moduli).
  Ogni finestra in più è un processo con il suo driver: circa 63 MB di working set e 75 di
  privata. La griglia aggiunge l'atlante delle miniature (64 MB sulla GPU) e fino a 96 MB di
  copie in RAM (sezione Miniature). Una cartella enorme pesa in proporzione: WinSxS (30.734
  voci, nomi di 95 caratteri in media) porta la finestra da 76 a 98–101 MB di privata. Quasi
  tutto è la lettura (nomi 5,6 MB, chiavi 6,2, minuscole 5,6, voci 1,2: sezione "Lettura delle
  cartelle e lista"); la cache delle dimensioni delle cartelle, con 41.000 voci, pesa da 4 a 6 MB
  (sezione "Dimensione delle cartelle");
- avvio che consegna la cartella alla principale ed esce: 11–13 ms, processo compreso (il primo
  58 ms, a disco freddo): è anche il costo in più di una finestra separata, prima del suo avvio.
- con l'icona l'exe cresce di 46 KB, quasi tutti suoi (1.050.624 → 1.097.728 byte); il contenuto
  visibile non cambia (`prove\avvio.bat`, 20 avvii alternati: mediana 52,5 ms senza, 52,9 con).

## Lettura delle cartelle e lista

**Come funziona.**

- **Lettura** (`listing.cpp`) su un thread: `FindFirstFileExW` con `FindExInfoBasic` e
  `FIND_FIRST_EX_LARGE_FETCH`. Lo stesso thread prepara i nomi in minuscolo per il filtro e le
  chiavi di ordinamento.
- **Memoria della lista:** nomi, minuscole e chiavi stanno in tre vettori contigui, senza una
  stringa per voce. La lista resta in memoria finché la cartella è aperta, quindi alla fine della
  lettura un vettore con più di `kListSlackMax` (1 MB) di spazio vuoto si restringe (`listTrim`).
  Le chiavi partono da 1,25 token per carattere (di solito ne servono 1,1): la stima basta e la
  copia non serve.
- **Ordinamento naturale** con chiavi precalcolate: ogni nome diventa una volta sola una sequenza
  di token da 16 bit (minuscole, accenti rimossi, numeri confrontati per valore), poi `memcmp`.
  Sostituisce `StrCmpLogicalW` (differenze in "Differenze note rispetto a Explorer").
- **Filtro** mentre si scrive: sottostringa sui nomi già in minuscolo.
- **Lista virtualizzata:** si disegnano solo le voci visibili. Dettagli, Elenco e Icone passano
  dalle stesse funzioni (`itemRect`, `itemBg`, `itemAt`, `gridParts`, `listParts` in `app.cpp`):
  le voci stanno su *linee* di `perLine` voci (una riga in Dettagli, una riga di celle in Icone,
  una colonna in Elenco) e lo scorrimento va lungo l'asse delle linee: `lineLen`, `viewLen`,
  `contentLen` e `scrollOffset` dicono quanto è lunga una linea, quanto se ne vede e di quanto si
  spostano le voci in x o in y. Così tastiera, rotellina, barra di scorrimento (orizzontale in
  Elenco), rettangolo, trascinamento e cursore sono un codice solo per le tre viste; il
  rettangolo tiene il punto di partenza in coordinate del contenuto su tutti e due gli assi. Il
  bordo destro delle voci è `itemsRight` (`app.h`): prima della barra verticale, e in Elenco, che
  non ce l'ha, fino al bordo della lista. Quando `layout` cambia la geometria (voci per linea,
  lunghezza della linea o spazio visibile), il cursore che si vedeva resta in vista
  (`cursorInView`, poi `ensureVisible`); se l'utente ha scorso lontano dal cursore, la lista resta
  dov'è. *Perché in `layout`:* le cause sono tante (anteprima, finestra, barra laterale, barra dei
  tempi) e passano tutte da lì.
  *Perché un livello di zoom e non una vista più una misura:* Ctrl+rotellina attraversa le viste
  in un solo gesto (Dettagli → Elenco → Icone), e la cartella si ricorda con un numero.
  *Misurato:* la cartella di prova da 904 file in Elenco, frame 0,05–0,10 ms, come Dettagli.
- **Selezione multipla:** la selezione è un flag per voce, quindi resiste a ordinamento, filtro e
  ricariche automatiche, che la ricostruiscono per nome. Il cursore è separato dalla selezione, con
  un'ancora per gli intervalli. Il filtro tiene le voci selezionate che restano visibili; se la
  selezione era di una sola voce, passa al primo risultato.
- **Selezione col rettangolo** (`listmouse.cpp`): ogni movimento ricalcola la selezione dalla
  copia fatta al clic (memset/memcpy più le righe toccate). Comportamento: `DESIGN.md`,
  "Rettangolo di selezione".
- **Colonne** (`columns.cpp`): una tabella (`kCols`, per `COL_*`) dice titolo, chiave in
  `impostazioni.txt`, allineamento e verso del primo ordinamento; `kColOrder` l'ordine sullo
  schermo, `kColYield` quello in cui cedono con la finestra stretta. Layout, intestazione, celle,
  bordi, tooltip e menu delle colonne girano su queste tabelle: una colonna nuova è una riga lì più
  il suo testo in `colText`. La geometria è un array per colonna (`colX`, `colWd`, 0 = nascosta),
  scambiato con l'anteprima in `paneSwap`. Lo spazio tra due testi sta nella colonna di sinistra,
  tranne dopo una allineata a destra (Dimensione), dove sta in quella di destra (`colStart`,
  `colEnd`). Tipo, Data creazione e Attributi vengono dalla lettura: `FindFirstFileExW` dà già la
  creazione, e l'inizio dell'estensione (`Entry::extOff`) si trova sul thread di lettura, nello
  spazio di allineamento della voce; la data di creazione porta la voce da 40 a 48 byte.
  Le estensioni nascoste accorciano solo il testo disegnato (`shownLen`): nomi, filtro e percorsi
  restano interi.
- **Filtro per colonna** (`columns.cpp`): `colBucket(col, voce, ctx)` dice in quale valore di una
  colonna sta una voce, con una chiave che non dipende dalla lettura (un periodo, un anno, una
  fascia, un bit d'attributo; per Tipo l'estensione come testo, puntata dentro `Listing::lower`).
  Il filtro (`ColFilter`: le chiavi scelte per colonna, le estensioni in ordine) è della lista come
  quello per nome: sta in `App`, si scambia con `TabState` (`tabSwap`) e con `ListPane`
  (`paneSwap`), si azzera in `onListing` e resta con le ricariche, che passano da `applyFilter`.
  `filterInto` applica il filtro per nome e poi quelli delle colonne (`colMatchInit` una volta,
  `colMatches` per voce: le chiavi sotto 64 come bit, le estensioni con la ricerca binaria);
  saltando una colonna dà le voci da contare nel suo menu (`colValues`), così i numeri non cambiano
  mentre si spunta. `ColCtx` prepara una volta per applicazione ciò che serve oltre alla voce: gli
  inizi dei periodi in ora locale come FILETIME UTC (`TzSpecificLocalTimeToSystemTime`, e una
  tabella degli inizi d'anno dal 1980; prima si calcola voce per voce) e i totali delle cartelle già
  contati (`dirSizeTotals`). Con il filtro per Dimensione un totale che arriva può cambiare la vista:
  `onDirSize` arma un timer (al più una volta ogni `WATCH_BATCH`) e `refilterQuiet` rifiltra senza
  far scorrere la lista. I gruppi (fase 3) useranno la stessa `colBucket`.
  *Perché le chiavi e non le etichette:* il confronto per voce resta un bit o un `wcscmp`, e il
  filtro sopravvive a una rilettura della stessa cartella.
  *Perché il menu resta aperto:* `MenuItem::keep` (voce a spunta): `menuExec` cambia la spunta e
  non chiude; il doppio clic su un menu aperto conta come un clic in più (`WM_LBUTTONDBLCLK`).
- **Colonne ridimensionabili** (`listmouse.cpp`): il doppio clic su un bordo adatta la colonna
  senza formattare ogni voce, perché le date hanno tutte la stessa forma, per le dimensioni basta
  il valore più grande di ogni unità, e Tipo e Attributi si misurano una volta per estensione e
  per combinazione di lettere. Le larghezze valgono per tutte le cartelle e si salvano al
  rilascio in `impostazioni.txt`. Dallo stesso codice passano i bordi della barra laterale e
  dell'anteprima.
- **Aggiornamento in tempo reale** (`watch.cpp`): un thread osserva la cartella con
  `ReadDirectoryChangesW`. Le notifiche si raggruppano (al massimo una ricarica ogni
  `WATCH_BATCH`, 50 ms) e la cartella si rilegge in background, conservando selezione, scroll e
  filtro. Lo stesso thread osserva anche la cartella superiore: se la cartella corrente viene
  rinominata o spostata la segue (`GetFinalPathNameByHandleW`); se viene eliminata o finisce nel
  Cestino, risale di un livello. Aprire e chiudere le cartelle osservate lo fa il thread.

**Perché così.**

- Provata anche `GetFileInformationByHandleEx` con un buffer da 1 MB: nessun guadagno misurabile,
  quindi è stata tolta.
- Il filtro non usa `StrStrIW`: su 30.000 nomi lunghi costava circa 100 ms a ogni tasto.
- Il watcher apre e chiude le cartelle sul suo thread perché `CreateFileW` e la chiusura possono
  aspettare fino a 2 s su una condivisione di rete.

**Misurato:**

- filtro 0,4 ms su 30.000 nomi; ordinamento di 30.000 nomi 4,3 ms (MSVC);
- con le colonne nuove (WinSxS, 30.734 voci, tutte le colonne visibili): lettura 19,2 ms, chiavi
  8,9 ms, ordinamento per Attributi 5,7 ms e per Data creazione 5,3 ms; preparazione del frame
  0,49 ms (peggiore 0,64); adattamento di Tipo al contenuto 0,22 ms;
- rettangolo al massimo 0,01 ms a movimento con 30.734 voci (WinSxS); adattamento di una colonna
  0,11 ms sulle stesse voci;
- filtro per colonna su WinSxS (30.734 voci, 30.732 cartelle), MSVC, prima apertura del menu con i
  conteggi e filtro applicato (F12: "menu del filtro di una colonna", "filtro"): Tipo 0,3 e
  0,1 ms; Data modifica 0,7 e 0,5 ms; Dimensione, con i totali delle cartelle, 8,0 e 6,6 ms (era
  11,9 e 11,1 prima che `dsEachDir` continuasse l'hash della cartella invece di costruire ogni
  percorso: il resto sono le ricerche nella cache dei totali). Con il filtro per Dimensione, mentre
  si contano le cartelle la vista si rifiltra al più ogni 50 ms: fino a circa il 13% del thread UI
  su una cartella come WinSxS;
- aggiornamento in tempo reale: circa 54 ms dalla notifica allo schermo; con 1000 file spostati in
  12 s, 272 ricariche e nessun file perso.

## Schede

**Come funziona** (`tabs.cpp`).

- Ogni scheda conserva cartella, selezione, scroll, filtro, ordinamento e cronologia avanti e
  indietro. Lo stato della scheda attiva vive nei campi di sempre, quindi il resto del codice non
  sa nulla delle schede: cambiare scheda scambia quei campi con quelli salvati (O(1), nessuna
  copia).
- Solo la scheda attiva è osservata dal watcher; quando una scheda torna attiva si ricarica in
  background, e una scheda lasciata a metà caricamento riprende quando ci si torna. Le schede
  aperte in background dalla stessa scheda si accodano in ordine, come nei browser.
- **Riapertura all'avvio** (opzione `riapri_schede`, spenta di solito): uscendo (`WM_DESTROY`, e
  `WM_ENDSESSION` quando Windows si spegne, che può chiudere il processo senza `WM_DESTROY`) la
  finestra principale scrive in `%APPDATA%\z-f\schede.txt` (`tabsSerialize`) la scheda attiva e,
  per ogni scheda, cartella e voce selezionata, anche delle schede mai aperte. All'avvio senza
  cartella la prima lettura aspetta il thread veloce della barra laterale, che subito dopo
  `impostazioni.txt` legge `schede.txt` e lo consegna (`WM_APP_START`, `tabsRestore`): si apre la
  scheda attiva, le altre restano in attesa come quelle aperte in background. Avviando [z]f su
  una cartella, le schede riaperte vanno prima e quella cartella resta attiva (senza doppioni).
  Solo la finestra principale: quelle aperte con "Nuova finestra" non si riaprono, e quelle
  aperte con un'opzione `--` non salvano niente.

**Perché così.** La barra delle schede è sempre visibile: se comparisse alla seconda scheda, il
contenuto salterebbe in basso proprio mentre si clicca.

## Icone

**Come funziona** (`icons.cpp`).

- Tre thread chiedono le icone alla shell (`SHGetFileInfo`):
  - `IQ_EXT`, partito da `iconInit` all'avvio: quasi tutte le icone si risolvono per estensione,
    senza toccare il disco;
  - `IQ_FILE`, partito alla prima richiesta: i tipi con un'icona propria per ogni file (exe, lnk,
    ico, url…) e le unità usano il percorso reale;
  - `IQ_SLOW`, partito alla prima richiesta: gli stessi, su un'unità di rete o ottica (`slowPath`;
    per le unità di Questo PC il tipo letto con la lista), come i thread della rete delle miniature.
    Elenco, barra laterale e menu di elenco passano tutti da `iconForKey`, che sceglie la coda.
- Da dove viene l'icona (percorso vero, cartella o estensione) lo decide una funzione sola,
  `iconSource`, usata anche dalle miniature e dall'anteprima. I percorsi di rete di Questo PC hanno
  l'icona di cartella senza chiedere niente alla rete (`iconDrive`).
- Un file solo online ha l'icona del tipo (`iconSource`), anche se il tipo avrebbe un'icona per
  file: la shell gli darebbe comunque quella generica, come in Explorer. Così, quando il file si
  scarica (aprendolo), cambiano gli attributi e con loro la chiave della cache, e l'icona vera
  arriva da sola; con la chiave per percorso restava quella generica fino al riavvio. Costa anche
  meno: una richiesta per tipo invece di una per file (15–40 ms l'una).
- Il secondo e il terzo thread aspettano che il primo abbia preparato le icone di sistema della
  shell (`shellReady`). Lo stesso aspetta l'icona grande dell'anteprima e delle miniature
  (`pvBigIcon`), che usa le stesse icone di sistema su un altro thread.
- L'alfa si ricava disegnando l'icona due volte, su nero e su bianco. Le icone vanno nell'atlante
  BGRA sulla GPU; finché un'icona non è pronta si vede un segnaposto, che l'icona sostituisce con
  una dissolvenza incrociata, e il thread UI non aspetta mai. Per la cache c'è anche una copia
  sulla CPU, così le icone si ricaricano quando si passa da WARP alla GPU.

**Perché così.**

- Il thread a parte per i percorsi reali esiste perché un collegamento su una condivisione
  scollegata non deve fermare le icone di tutto il resto; quello per la rete e le unità ottiche,
  perché un'unità di rete che non risponde non deve fermare le icone dei .exe e .lnk locali (la
  griglia lo faceva già).
- Le cartelle di rete di Questo PC non chiedono l'icona alla rete: una condivisione lenta faceva
  aspettare 20 s la sua icona e quelle delle unità in coda dietro, e passava la sua icona a tutte
  le cartelle.
- I due thread insieme fallivano la prima chiamata: per questo il secondo aspetta il primo.
  Misurato con `prove\icona-grande-prova.cpp` (un processo nuovo per volta, la prima
  `SHGetFileInfo` e l'icona grande di `pvBigIcon` su due thread che partono insieme): una delle
  due chiamate falliva in 40 processi su 40; con l'attesa, l'icona grande arriva in 40 su 40.
- Provato con `prove\online-prova.cpp`, che simula OneDrive con l'API Cloud Files di Windows:
  icona piccola, grande e jumbo di .exe, .ico, .lnk, .png e .jpg, nessuno scaricamento. Leggere i
  primi 256 KB invece scarica il file. Scaricati da fuori un .exe e un .lnk, elenco, griglia e
  anteprima mostrano la loro icona vera entro un secondo, senza riavviare; nessuno scaricamento da
  [z]f.

## Miniature e vista a icone

**Come funziona** (`thumbs.cpp`; comportamento visibile in `DESIGN.md`, "Viste e zoom").

- **Due liste:** la lista principale e l'anteprima di una cartella possono essere in Icone
  insieme. Ognuna è un `ThClient` (cartella, lato del riquadro, voci volute, contatore di
  generazione in `g_thLatest[]`: una richiesta vale se è di una delle due); mappa, code, thread e
  atlante sono in comune, e la chiave di una miniatura è percorso più lato (`thKey`), perché le
  due liste possono avere lati diversi. I posti dell'atlante sono grandi quanto il lato più grande
  in uso (`slotPx`): solo quando quel massimo cresce, o scende alla metà o meno, si ricomincia
  da capo. L'anteprima aperta in Icone conta anche mentre per un momento mostra un file:
  altrimenti, con la lista al 20% e l'anteprima al 100%, scorrendo tra cartelle e file l'atlante
  si rifaceva a ogni passo e le miniature della lista rientravano in dissolvenza. Risultati e potature
  li fa la chiamata della lista principale, una volta per frame; le code di una lista si
  riprendono senza toccare quelle dell'altra. Una voce in comune (stessa chiave: le icone per
  tipo, a pari lato) la chiede una lista sola: se quella la riprende senza volerla più, o smette
  di chiedere (esce da Icone, l'anteprima non mostra più una cartella: `thClientStop`), l'altra si
  risincronizza (`thOthersResync`), altrimenti la voce resterebbe vuota finché non si scorre.

- **Fonti**, in quest'ordine, per miniatura sul thread:
  1. la cache di Windows (`IThumbnailCache`, solo quelle già pronte), creando l'elemento della
     shell dalla cartella già aperta (`IShellFolder`, una volta per thread);
  2. se manca, le immagini con WIC (`pvImage` dell'anteprima);
  3. video e documenti con la shell (`SIIGBF_THUMBNAILONLY`);
  4. altrimenti l'icona grande della shell, metà del riquadro.

  Le miniature fatte con WIC si chiedono poi alla shell a riposo e a bassa priorità (al massimo
  `kThWarmMax`): entrano nella cache di Windows, anche per Explorer. I file solo online non si
  leggono; quando si scaricano (cambiano gli attributi, non data e dimensione) la voce si rifà da
  capo e la miniatura arriva.
- **Thread**, tutti sotto la priorità normale e partiti alla prima miniatura: uno per la cache di
  Windows e le icone, metà dei core fisici per WIC (da 2 a 8; 6 sul Ryzen 9 7900), uno per la
  shell, due per le cartelle di rete. In Questo PC le icone delle unità di rete e ottiche
  (`slowPath`) vanno sui thread della rete. Le code le possiede il thread UI: prima le voci
  visibili, poi una schermata nella direzione dello scorrimento, poi una dall'altra parte; uscendo
  dalla cartella le richieste in corso smettono (`PvReq::superseded`).
- **Memoria:** un atlante `kThAtlas` 4096×4096 sulla GPU (64 MB, creato al primo uso della
  griglia) e le copie in RAM fino a `kThRamMax` (96 MB); se ne vanno le meno usate di recente.
  Quando nessuna scheda è più in griglia si liberano tutte e due; tornando in griglia le miniature
  arrivano dalla cache di Windows. Al massimo `kThUploadMs` (0,25 ms) di caricamenti sulla GPU per
  frame.
- **Zoom per cartella** in `%APPDATA%\z-f\viste.txt`, le `kViewsMax` (500) più recenti: una riga
  "percorso<TAB>elenco" o "percorso<TAB>icone 40" per ogni cartella non in Dettagli. Le righe
  "griglia" delle versioni di prima valgono la misura che aveva la griglia: la vecchia
  `griglia_dimensione` di `impostazioni.txt` (piccole, medie, grandi → 40, 70, 100%; senza, 70%)
  si legge ancora in `settingsApply`, che arriva prima di `viewsLoaded`, e non si riscrive più. Il
  file delle viste si riscrive subito nella forma nuova. Ogni livello di Icone ha la sua misura delle miniature: cambiando zoom l'atlante
  si svuota e le miniature si rifanno alla misura nuova (dalla cache di Windows, meno di 1 ms
  l'una).
- F12 mostra fonti, texture, memoria, il tempo di creazione dell'atlante e la prima schermata
  completa.

**Perché così.**

- L'elemento della shell si crea dalla cartella già aperta: dal percorso intero costava 2,6 ms, di
  cui 2,3 per risolverlo.
- Le miniature WIC si rimandano alla shell perché la volta dopo costano 0,3 ms invece di una
  decodifica.
- I thread di WIC: la prima schermata di foto da 12 MP passa da 1080 ms con un thread a 294 con 4
  e 175 con 8.
- Le unità lente sui thread della rete: un'unità scollegata non ferma le miniature delle cartelle
  aperte dopo.

**Misurato** (Ryzen 9 7900, NVMe), per miniatura sul thread:

- cache di Windows 0,3–0,8 ms; WIC: foto da 12 MP 15–27 ms, JPEG 1080p 5 ms, PNG 4,5 ms; shell
  100–400 ms la prima volta;
- 400 foto mai viste: prima schermata (66) in 306–332 ms; riaperte in un processo nuovo, tutte
  dalla cache, in 103 ms; dopo che l'atlante è stato liberato, meno di 1 ms l'una;
- frame scorrendo veloce mentre arrivano le foto 0,19 ms (peggiore 0,76); WinSxS (30.734 voci) in
  griglia piccola 0,32 ms (peggiore 0,91), di cui risultati e richieste al massimo 0,04 e disegno
  delle voci 0,23;
- `drawThumb` ricostruisce chiave e percorso di ogni cella a ogni frame: misurato nella
  1.0.0-beta.8 (F12, "griglia: disegno delle voci"; RTX 5070 Ti, 2560×1392 a 175%, WinSxS in
  Icone a finestra intera scorrendo) 0,24 ms con tutte le voci, peggiore 0,59; frame 0,39
  (peggiore 0,90). Non pesa: resta così.

## Tooltip e anteprima

**Come funziona: tooltip** (`tooltip.cpp`).

- Compare dopo `TIP_DELAY` (500 ms) fermi, `TIP_DELAY_NEAR` (60 ms) passando tra elementi vicini.
- Le informazioni dal disco le legge `readInfo` su un thread, e il tooltip le aspetta al massimo
  `TIP_INFO_WAIT` (150 ms), così compare già completo. `fileRows` e `driveRows` le mettono in
  righe, anche per l'anteprima.
  - **File:** nome completo, tipo, dimensione in byte, risoluzione delle immagini (dall'intestazione
    PNG/JPEG/GIF/BMP), date di modifica e creazione con il tempo trascorso, attributi, totale della
    selezione.
  - **Cartelle:** il contenuto (al massimo 200 ms di lettura) e la dimensione totale con il numero
    di file, che cresce mentre si conta (`dirsize.cpp`).
  - **Unità file system:** capacità, usato e libero.
  - **Schede, pulsanti, intestazioni e barra dei tempi:** cosa fanno.
- `readInfo` guarda `onlineOnly` prima di aprire un file: dei file solo online non legge né
  l'intestazione delle immagini né la destinazione di un collegamento.

**Come funziona: anteprima** (`preview.cpp`; aspetto in `DESIGN.md`, "Pannello di anteprima").
Mostra la voce sotto il cursore e la segue mentre ci si muove con le frecce.

- **Immagini** (PNG, JPEG, GIF, BMP, TIFF, ICO; WebP, HEIF, AVIF e JPEG XR se c'è il codec di
  Windows) con WIC (`pvImage`), già ridotte alla misura del pannello e disegnate 1:1, mai
  ingrandite, raddrizzate con l'orientamento EXIF. I JPEG si riducono già in decodifica (1/2, 1/4,
  1/8). Sopra `kPvImgMaxPx` (50 megapixel) da decodificare interi si mostrano solo le
  informazioni.
- **Testo** (`pvText`; codice, log, json, ini, md… e i file senza estensione nota che sembrano
  testo): codifica riconosciuta (BOM, UTF-16 anche senza BOM, UTF-8 valido, altrimenti ANSI), al
  massimo 256 KB.
- **Cartelle:** il contenuto ordinato come la lista, al massimo `kPvDirMax` (5000) voci o 300 ms
  di lettura. Il `Listing` letto passa alla **lista dell'anteprima** (`PV.pane`, un `ListPane`),
  che è una seconda lista vera (sotto).
- **La lista dell'anteprima: lo stato scambiato.** Tutto il codice della lista (geometria
  delle voci, disegno, selezione, rettangolo, menu, comandi, trascinamento, rinomina, tastiera)
  lavora sui campi di `App` (`A.cur`, `A.view`, `A.sel`, la geometria). Come le schede
  (`TabState`), l'anteprima tiene i suoi in un `ListPane` e li **scambia** con quelli di `App`
  per la durata di un `PaneScope` (`paneSwap`, O(1): vettori e stringhe si scambiano). Così
  `drawListItems`, `layoutItems`, `itemAt`, `menuOpen`, `mqPress`, `dragPrepare`, `beginRename`,
  `onKey`… valgono anche per l'anteprima senza saperlo.
  - *Regola:* fuori da un `PaneScope` `App` è sempre la lista principale. Quello che riguarda
    solo lei (`navigate`, `requestRefresh`, `layout`) chiesto da dentro uno scope non si fa lì:
    `navigate` mette il lavoro in `g_paneAfter`, eseguito all'uscita dallo scope (e aprire una
    cartella o uno zip dell'anteprima diventa `tabNew` in background: nell'anteprima non si
    scende); `requestRefresh` e F5 rileggono l'anteprima (`previewRefresh`).
  - *Gli stati che durano oltre un messaggio* ricordano a chi appartengono e rientrano nello
    scope quando riprendono: menu (`PopupMenu::preview`, `forPath`: se intanto la cartella è
    cambiata non si esegue, tranne il menu della vista, che è del pannello), rettangolo (`MQ.preview`), trascinamento (`DD.preview`, e
    `DD.targetPreview` per la destinazione), tooltip (`T.preview`), rinomina e nuova cartella
    (`onFsDone` guarda in quale cartella è successo: `PV.selectAfter`, `PV.renameAfter`), menu
    della shell (`onShellMenu`, Aggiorna e Rinomina).
  - *Perché così:* passare la lista come parametro a ogni funzione avrebbe toccato centinaia di
    righe in una dozzina di moduli, col rischio di rompere la lista; lo scambio riusa ogni riga
    (principio 4) e il costo è di un paio di scambi per frame.
  - *Cicli modali:* niente che pompi messaggi gira dentro uno scope. Il trascinamento prepara
    l'oggetto nello scope (`dragPrepare`) e chiama `SHDoDragDrop` dopo averlo chiuso: nel ciclo
    arrivano timer, risultati dell'anteprima e la molla delle schede, che devono trovare `App`
    com'è sempre. Per sicurezza `render`, `layout`, `tabActivate` e `onPreviewResult` dentro uno
    scope rimandano all'uscita (in una prova di sviluppo il disegno nel ciclo, con lo stato
    scambiato, cancellava il `Listing` della lista principale).
  - *Disegno:* `pvPaneLayout` calcola la geometria nello scope (`layoutItems(!A.prevBottom, …)`:
    in Dettagli niente Destinazione, perché i collegamenti dell'anteprima non si leggono, e a
    destra nemmeno Data), poi `drawListItems` disegna dentro un **ritaglio**
    (`clipBegin`/`clipEnd` in `gfx.cpp`: uno stato del rasterizzatore con lo scissor, due draw call
    in più) perché le voci che scorrono, in Elenco anche in orizzontale, non sconfinino.
  - *Tastiera:* `PV.keys` (clic nel pannello, Tab). `onKey` apre lo scope tranne che per i tasti
    della finestra (schede, cronologia, pannelli, Vai a, annulla, F12); `onChar` scrive nel filtro
    di chi ha la tastiera. Il campo del filtro (`fltX`, `fltW`, la ×, `fltLayout`, `fltFieldAt`),
    la sua misura per F12 (`filterMs`) e i contatori della barra (`ViewCounts`, `viewDirs`) sono
    nello stato scambiato, e `drawBarFilter` (`render.cpp`) lo disegna per la lista e, nello
    scope, per l'anteprima.
  - *Aggiornamento:* un secondo osservatore (`previewWatch` in `watch.cpp`, messaggio
    `WM_APP_PVCHANGED`, timer 17 per raggruppare le notifiche) segue la cartella mostrata, solo
    sulle unità locali e non dentro uno zip (lo dice `zipLen` del `Listing`, non il nome: una vera
    cartella "Backup.zip" si osserva); in rete si rilegge dopo le operazioni di [z]f
    (`onFileJob`, anche annulla e ripeti, `onFsDone`) e dopo i comandi della shell e i rilasci
    passati a lei (`onShellMenu` con `SmRes::invoked`, `onShellDrop`). Un osservatore perso
    (`WATCH_LOST`) non riparte da solo, come quello della lista: su un file system che lo perde
    subito rileggerebbe ogni `WATCH_BATCH`; riparte con F5 o con un'altra cartella. `pvPaneSet`
    rilegge con lo stesso codice della ricarica della lista (`listReload`, nel suo PaneScope):
    selezione per nome, cursore, scorrimento, filtro e la rinomina in corso. `previewRefresh` non
    supera una richiesta in corso: se il cursore è già su un'altra voce non fa niente (rifare la
    richiesta di un'immagine grande la annullerebbe a ogni notifica), e se una rilettura è ancora in
    corso la prossima parte quando arriva (`PV.refreshGen`, `PV.refreshAgain`), come
    `refreshAgain` della lista. Il trascinamento premuto nell'anteprima confronta il percorso della
    cartella, non il `Listing`: una rilettura della stessa cartella non lo ferma. F12 mostra
    l'osservatore ("anteprima: aggiornamento automatico", notifiche e riletture) e l'ultima
    rilettura, dalla prima notifica al risultato applicato.
    *Misurato* (1.0.0-beta.8, cartella locale di 34 file, file creati da fuori uno alla volta):
    ultima rilettura 58–71 ms, di cui 50 il raggruppamento `WATCH_BATCH` e 0,1–0,2 la lettura sul
    thread; il resto è il timer e il frame. Come la ricarica della lista (62 ms nella stessa prova).
  - *Misurato* (F12, "anteprima: voci di una cartella"; RTX 5070 Ti, 2560×1392 a 175%): lista in
    Icone medie e anteprima in Icone grandi insieme, frame 0,12 ms (peggiore 0,24 scorrendo
    l'anteprima mentre arrivano le icone), voci dell'anteprima 0,01 ms. Avvio (`prove\avvio.bat`,
    4 giri alternati da 5 con la 1.0.0-beta.6, anteprima aperta): contenuto visibile in mediana a
    78 ms contro 77, prima icona a 105 contro 104, primo fuoco 11 contro 12: invariato.
- **Tutto il resto** e le unità: l'icona grande della shell (`pvBigIcon`) e le righe del tooltip.
  I file solo online non si leggono.
- **Thread:** lettura e decodifica su un thread per richiesta, con un contatore di generazione. Un
  thread alla volta: delle richieste in attesa vale solo l'ultima, quindi tenendo premuta una
  freccia non si accoda niente; dopo `PREVIEW_SLOW` (300 ms) una lettura lenta, per esempio in
  rete, non trattiene la successiva, che parte su un altro thread. WIC si carica solo alla prima
  immagine, sul thread. Sul thread dell'interfaccia resta solo la texture, una per immagine, grande
  al massimo quanto il pannello.
- F12 mostra lettura, decodifica e texture dell'ultima anteprima.
- **Posizione: a destra o in basso** (`A.prevBottom`). `layout` calcola una volta il rettangolo del
  pannello (`prevX`, `prevY`, `prevW`, `prevH`; `prevW` 0 se non si vede, in tutte e due le
  posizioni) e `footY`, il fondo della barra di stato della lista (la finestra, o l'inizio
  dell'anteprima in basso). Il resto chiede a loro: `inPreview` per i punti nel pannello,
  `barTop`/`inBar` per la barra in fondo della lista di `App` (quella principale a `footY`, nel
  `PaneScope` la banda dell'anteprima in fondo alla finestra), `inListFoot` per barra dei tempi e di
  stato; `pvGeom` e `pvPaneLayout` partono dal rettangolo. Il bordo è lo stesso `EDGE_PREVIEW`:
  `edgeVertical` dice se si trascina in verticale (cursore ↕), `colChosen` dà `prevHeight` o
  `prevWidth`, ognuna salvata a parte. In basso il pannello riempie il suo fondo di `C_BG`: le
  voci della lista non sono ritagliate, e una cella di Icone che scorre oltre la barra di stato
  verrebbe vista sotto.
  - *Perché un rettangolo:* prima la posizione era scritta in una ventina di punti come
    `x >= A.listR` e `G.h - A.statusH`; ora è in `layout`, e una terza posizione toccherebbe solo lì.
  - *La scelta:* `CH_PREVIEW_POS` (˅ accanto a ◨, `chrome.cpp`) apre `menuOpenPreview`; le
    voci, il selettore delle opzioni e Alt+Shift+P (`M_PREVIEW_SWAP`, che alterna le due
    posizioni) chiamano `previewPlace`, che salva e rifà il layout.
  - *Misurato* (F12; RTX 5070 Ti, 2560×1392 a 175%, anteprima in basso in Icone medie scorrendo
    una cartella da 2000 file): frame 0,12 ms (peggiore 0,28), voci dell'anteprima 0,02 ms
    (peggiore 0,07): come a destra.

**Perché così.** Leggere l'inizio di un file solo online lo scarica: provato con
`prove\online-prova.cpp` in una cartella di segnaposto (anteprima aperta e cursore su ogni voce,
tooltip su un JPEG), nessun file scaricato.

**Misurato:**

- una foto da 24 MP in 13–15 ms, un PNG 4000×3000 in 11 ms; 256 KB di UTF-8 in 1,5 ms; una
  cartella di 3.001 voci in 2 ms;
- creazione di WIC 1,8 ms; il codec WebP la prima volta 80 ms;
- frame con il pannello aperto su un testo lungo 0,11 ms; texture di un'immagine 0,08 ms.

## Dimensione delle cartelle

**Come funziona** (`dirsize.cpp`; opzione "Dimensione delle cartelle nella colonna",
`dimensione_cartelle`).

- Un thread somma i file sotto una cartella senza seguire junction e collegamenti, come Explorer.
  Parte dopo il primo frame.
- Il tooltip lo chiede subito (`DS_TIP`): il totale si aggiorna ogni
  `DS_PROGRESS` (100 ms, `theme.h`) mentre cresce, e si ferma se il mouse va altrove.
- La colonna Dimensione conta in background, con priorità di I/O bassa, le sottocartelle della
  cartella aperta nell'ordine della vista. Il thread UI tiene gli indici delle voci e passa al
  thread i percorsi a pezzi di `kDsChunk` (256): finito un pezzo, il thread lo dice con
  `WM_APP_DIRSIZE` e `dirSizeRefill` gli dà il successivo. I percorsi tutti insieme erano 8 MB in
  WinSxS.
- I risultati restano in memoria per `DS_FRESH` (un minuto), con le sottocartelle dirette, così
  entrando ci sono già; una copia, uno spostamento, un'eliminazione o una ricarica li dimenticano
  prima.
- La cache ha per chiave un hash a 64 bit del percorso in minuscolo (`dsHash`), non il percorso:
  circa 100–150 byte a voce invece di 350. Due percorsi con lo stesso hash si confonderebbero, con una
  probabilità di circa 3·10⁻¹⁰ su 100.000 cartelle. Oltre `kDsCacheMax` (50.000) voci il thread,
  quando non ha altro da fare, toglie le scadute (`dsTrim`, al massimo ogni `DS_TRIM_EVERY`, 15 s), tranne le
  sottocartelle della cartella aperta, che la colonna mostra. Lo fa a blocchi di bucket, quindi il
  thread UI non aspetta il lock più di un blocco.
- Le unità di rete e ottiche (`slowPath`) si contano solo a richiesta (tooltip).

**Misurato:** tutto C:\ (18 cartelle) in 14,4 s sul thread, con i frame a 0,12 ms (peggiore
0,35). WinSxS (30.734 cartelle): contate in 6 s, 41.000 voci in cache, da 4 a 6 MB (con i
percorsi come chiave e in coda erano circa 24). La pulizia, provata con soglie più basse, ha
tolto 2.842 voci su 10.266 in 0,6 ms, con il lock tenuto al massimo 0,17 ms per blocco.

## Barra laterale

**Come funziona** (`sidebar.cpp`; aspetto e menu in `DESIGN.md`, "Barra laterale").

- **All'avvio**, `sideFastThread` (solo kernel32) legge i file di [z]f (vedi "File di [z]f e
  impostazioni") e le lettere delle unità, così il primo frame mostra già la barra. Nomi
  localizzati ("Documenti"), etichette e spazio libero arrivano dopo il primo frame, in due tempi:
  - `sideThread` legge le voci locali: unità fisse, rimovibili e RAM e i preferiti su di esse;
  - `sideSlowThread` legge le voci lente secondo `slowPath` (unità di rete e ottiche, percorsi di
    rete e preferiti su di loro), una voce alla volta, consegnata appena pronta e abbinata per
    percorso e tipo alle voci attuali. Ne gira uno alla volta: le richieste arrivate intanto ne
    fanno uno alla fine. Intanto queste voci hanno il nome generico o i valori della lettura
    precedente; una lettura che fallisce (disco tolto, server spento) li azzera, come un'unità
    vuota. I nomi dei preferiti cambiati si salvano una volta, a fine giro.
- **Quando si rilegge:** quando si collega o scollega un'unità e quando si torna su [z]f (al
  massimo ogni `SIDE_RELOAD`, 5 s). Un'unità ottica si rilegge solo all'avvio e a
  `WM_DEVICECHANGE` (disco inserito o tolto).
- **Unità di rete:** il nome viene da `SHGetFileInfo` (quello della condivisione o dato
  dall'utente), `\\server\condivisione` da `WNetGetConnection`. mpr.dll si carica solo se c'è
  un'unità di rete o la si usa.
- **Percorsi di rete** di Windows (quelli di "Aggiungi un percorso di rete"): cartelle in
  `%APPDATA%\Microsoft\Windows\Network Shortcuts` con un `target.lnk`, letto dal nostro lettore
  (`netPlacesRead` in `lnk.cpp`, niente shell né rete), anche nella lettura veloce dell'avvio.
  Stanno in `A.drives` con `SideItem::place`, quindi anche in "Questo PC" nella lista, nel menu
  "›" di Questo PC e in "Vai a". Lo spazio lo legge `sideSlowThread`. Il menu di Windows di una di
  queste voci si chiede alla cartella che la contiene (`\\server`), perché Questo PC non sa
  interpretare un percorso intero.
- **Connettere** un'unità di rete apre il dialogo di Windows su un thread, con la finestra di [z]f
  come proprietaria, e poi apre l'unità. **Disconnettere** gira su un thread e non forza: con file
  aperti lo dice. Se la cartella aperta è sull'unità da disconnettere si passa prima a "Questo
  PC", perché il watcher la tiene aperta.
- **Aggiungere un percorso di rete** apre la procedura di Windows ("Aggiunta guidata risorse di
  rete") con il comando che usa Explorer, letto da dove lo legge Explorer:
  `CommandStore\shell\Windows.AddNetworkLocation\command` in HKLM (oggi
  `rundll32 shwebsvc.dll,AddNetPlaceRunDll`). Un thread avvia il processo e aspetta che finisca,
  poi la barra laterale si rilegge (`sideLoad`): la procedura non dice se ha creato qualcosa, e il
  percorso nuovo è una cartella in più in `Network Shortcuts`.
- **Proprietà di Questo PC** apre Impostazioni › Sistema › Informazioni (`ms-settings:about`) su
  un thread.
- **Preferiti** in `%APPDATA%\z-f\preferiti.txt`: una riga per cartella, con il nome mostrato dopo
  un tab (facoltativo, modificabile a mano).
- F12 mostra i due tempi ("barra laterale: voci locali", "rete e ottiche" con il numero di voci).

**Perché così.**

- Una condivisione lenta (provata: 50–60 s per rispondere) non deve trattenere spazio e nomi delle
  unità locali: per questo le voci lente hanno il loro thread, dopo le locali.
- Chiudendo la finestra mentre `sideSlowThread` aspetta la rete (`volumeRead`), il processo resta
  vivo finché quella chiamata non torna: Windows non può fermare un thread dentro il redirector di
  rete, e la richiesta di annullarla, che fa da sé alla fine del processo, il redirector non la
  onora subito. Non si vede: la finestra sparisce subito e il mutex dell'istanza unica è già
  chiuso (`WM_DESTROY`). Dopo "Aggiorna" la pulizia della versione di prima aspetta al massimo
  `UPD_OLD_WAIT` su un thread; se non basta, la copia vecchia si toglie all'avvio dopo. Non c'è
  rimedio leggero: queste chiamate non hanno una versione asincrona, e un processo a parte per
  leggerle andrebbe contro la leggerezza.
  **Misurato** (novembre 2026) con `prove\uscita-prova.cpp`, che legge le pile dei thread del
  processo finché esce, e segni temporanei nel percorso d'uscita: il nostro codice arriva in fondo
  (`atexit`) in 0,2 s, poi resta solo `sideSlowThread` dentro `volumeRead`, e il processo esce
  dopo 3–10 s con un percorso di rete raggiunto via VPN. Nella stessa sessione, a rete sveglia,
  esce in 0,3–0,6 s, con o senza percorsi di rete; anche con `--gpu=warp` era lento, quindi non è
  la GPU.
- Le unità di rete hanno il nome della condivisione e non l'etichetta del disco sul server, che
  spesso è la stessa per tutte.
- Il dialogo per connettere ha [z]f come proprietaria perché alla chiusura [z]f torni davanti: un
  dialogo di un altro thread darebbe il fuoco alla finestra successiva.
- La procedura dei percorsi di rete non ha un'interfaccia documentata: il punto d'ingresso in
  shwebsvc.dll è interno a Windows. Leggendo il comando da `CommandStore` lo si segue se Windows lo
  cambia. Gira in un processo suo, che non può avere [z]f come proprietaria: è una finestra a sé,
  come in Explorer, e non blocca connetti e disconnetti (`g_netBusy` non la conta).
- Un'unità ottica non si rilegge al ritorno sulla finestra: il DVD si avvierebbe ogni volta.
- `ms-settings:about` sul thread dell'interfaccia costava 222 ms.

**Misurato:**

- voci locali: 16,6 ms al primo giro, 7 ms ai successivi; percorsi di rete (`netPlacesRead`)
  0,2–0,35 ms;
- con un server spento simulato (8 s di attesa su un'unità di rete): voci locali a 97–153 ms
  dall'avvio, l'unità di rete da sola dopo 8,1 s;
- "Aggiungi percorso di rete…" (novembre 2026): sul thread dell'interfaccia solo la creazione del
  thread; la procedura compare 30–140 ms dopo l'avvio del processo (tre giri), e quel processo
  occupa 4,9 MB di memoria privata finché resta aperta.

## Percorso a segmenti e "Vai a"

**Come funziona: percorso** (`crumbs.cpp`; comportamento in `DESIGN.md`, "Barra del percorso").
Un clic su un segmento ci va con selezionata la cartella da cui si arriva. Le sottocartelle del
menu "›" le legge `readSubfolders` su un thread, ordinate come la lista; se
la lettura supera `LOADING_AFTER` (150 ms) il menu si apre subito con "caricamento…". I menu lunghi
scorrono (`menuAdd`, `menuPlace`). Lo spazio vuoto apre "Vai a" dalla cartella aperta
(`gotoOpenHere`): il percorso con "\" in fondo è un percorso scritto, e le sottocartelle vengono
dalla `Listing` già aperta (`gotoDirFromListing`), senza disco. Così scrivere un percorso e
cercare una cartella sono un solo componente.

**Come funziona: "Vai a"** (`goto.cpp`; pannello in `DESIGN.md`, "Vai a").

- **Dove cerca:** cartelle recenti, preferiti, unità, cartelle aperte nelle schede e loro
  sottocartelle, e le sottocartelle dei preferiti e delle `READ_RECENTS` (20) cartelle recenti più
  usate.
- **Come:** le lettere scritte devono comparire in ordine, anche non vicine ("dl" trova
  Downloads), senza badare a maiuscole e accenti; contano l'inizio delle parole, le lettere
  consecutive e quanto una cartella è usata (visite e ultima visita). Scrivendo un percorso
  (`C:\…`, `\\server\…`, `%APPDATA%`) elenca le sottocartelle di quel livello.
- **Thread:** il thread dell'interfaccia cerca solo in memoria; le sottocartelle le legge un thread
  all'apertura del pannello (prima le unità locali, poi la rete) e restano in memoria. Una ricerca
  oltre `LOG_GOTO` (8 ms) va nel registro.
- **Recenti** in `%APPDATA%\z-f\recenti.txt` (percorso, visite, ultima visita).

**Misurato:** 0,3 ms a tasto su 20.000 nomi. Il caso peggiore (molte cartelle che arrivano dal
thread, ognuna rifà la ricerca) si misura a parte con `prove\vai-a-misura.cpp`.

## Menu e comandi della shell

**Come funziona.**

- **Menu nostro** (`menu.cpp`), disegnato come il resto dell'interfaccia, con le voci essenziali:
  - sui file: Apri, Apri con, Taglia, Copia, Copia percorso, Rinomina, Sposta nel Cestino, Proprietà;
  - sullo sfondo: Incolla, Annulla (e Ripeti, quando c'è), Nuova cartella (che parte già in rinomina), Aggiorna, Copia
    percorso, Apri terminale qui, Proprietà;
  - in "Questo PC": "Connetti unità di rete…" e "Aggiungi percorso di rete…" sullo sfondo,
    "Disconnetti" su un'unità di rete;
  - sulle schede: Nuova scheda, Nuova finestra, Chiudi scheda, Chiudi le altre schede.

  Non c'è un menu dell'applicazione: ⚙ apre direttamente le opzioni (`chromeExec`), dove stanno
  anche l'integrazione con Windows e la versione con l'aggiornamento.

  Le scorciatoie delle voci vengono da `kShortcuts` (`options.cpp`, `shortcutFor`). Regole del
  menu (voci disattivate o assenti, scambio con `menu_completo`): `DESIGN.md`, "Menu
  contestuale".
- **Indietro e Avanti senza la tastiera normale** (`historyCmd`, `input.cpp`): i tasti laterali
  del mouse alla pressione (`WM_XBUTTONDOWN`); i tasti del browser e i gesti del touchpad
  ("Spostamento indietro/avanti" di Windows) come `WM_APPCOMMAND`. Secondo la documentazione è
  `DefWindowProc` a trasformare quei tasti in `WM_APPCOMMAND`, quindi `WM_KEYDOWN` lascia passare
  `VK_BROWSER_BACK`…`VK_LAUNCH_APP2`; con un `WM_KEYDOWN` simulato (`PostMessage`/`SendMessage`)
  non succede, nemmeno in una finestra qualsiasi: serve il tasto vero.
  `WM_XBUTTONUP` invece si ferma, altrimenti `DefWindowProc` manderebbe un secondo
  `WM_APPCOMMAND` e il tasto laterale navigherebbe due volte.
- **Menu completo della shell** ("Altre opzioni…"; `shell.cpp`): lo costruisce e lo mostra un
  thread suo, con una finestra nascosta sua, mentre l'interfaccia continua a disegnare. Elimina,
  Incolla, Rinomina, Copia e Taglia tornano a [z]f con i percorsi del momento del clic, quindi
  passano dalle operazioni sui file e si annullano come i tasti. Un clic o un tasto mentre il menu
  si costruisce lo annulla. È sempre il menu classico con tutte le voci: il menu ridotto di
  Windows 11 esiste solo dentro Explorer, quindi non serve nessuna modifica di Windows.
- **Comandi della shell sullo stesso thread** (`smSend`): Shift+Canc, le Proprietà, "Apri con…" e
  Copia/Taglia di unità. `shellVerb` agisce sulla selezione con i verbi "delete", "properties",
  "openas", "copy", "cut"; `shellVerbPath` su un percorso intero (Proprietà della cartella aperta
  e delle voci della barra laterale), con l'oggetto chiesto alla cartella che lo contiene. La
  finestra di "Apri con" è di OpenWith.exe, un processo a parte. Se la shell non trova le voci
  (rete che non risponde, cartella sparita) non compare niente: il motivo torna nel risultato
  (`SmRes::verbErr`) e il thread UI dà l'avviso di errore (`smVerbFailed`).
- Nel registro "menu della shell lento" e "comando della shell lento" oltre `LOG_MENU` (50 ms, sul
  thread); in F12 "comando della shell: preparazione".

**Perché così.**

- Il menu completo non è quello predefinito perché costruirlo costa 200–450 ms (la prima volta di
  più).
- Proprietà e "Apri con…" non usano `SHObjectProperties` e `SHOpenWithDialog`: la prima analizza
  il percorso sul thread che la chiama (la finestra ha il suo thread, l'analisi no), la seconda è
  modale, e con la finestra aperta [z]f non si ridisegnava.
- Preparare l'oggetto della shell costa circa 80 µs a voce in locale e 400 µs su una condivisione:
  con molte voci sono secondi, che sul thread del menu non fermano l'interfaccia.

**Misurato:**

- il menu nostro si apre in circa 5 µs più un frame;
- oggetto della shell: 20.000 file 1,6 s in locale, 8 s su una condivisione, sul thread; una voce
  sola 8–10 ms (Proprietà di un file 9,7 ms, "Apri con…" 8,2 ms); un percorso di rete di un NAS,
  la prima volta, 22,4 s di attesa della rete, solo sul thread del menu. Shift+Canc e Proprietà
  di più voci si misurano a parte, con gli stessi passi, con `prove\shell-verbi.cpp`.

## Operazioni sui file e annulla

**Come funziona** (`fileops.cpp`).

- **Copia, sposta, Cestino** passano da `foRun`: `IFileOperation`, il motore di copia di Explorer
  (stesse finestre di avanzamento, conflitti e permessi), su un thread. Un
  `IFileOperationProgressSink` dà l'esito di ogni elemento: il nome finale dopo "Mantieni
  entrambi", gli elementi saltati e, per l'eliminazione, l'elemento esatto nel Cestino (`$R…`).
  Rinomina e nuova cartella passano da `fsStart`.
- **Annulla e ripeti:** ogni operazione si registra con `undoPush`. Si annullano spostamenti
  (trascinamento, Taglia/Incolla), copie (le copie vanno nel Cestino), rinomine, cartelle nuove
  (vuote si rimuovono, altrimenti Cestino; creazione e rinomina subito dopo sono una voce sola),
  eliminazioni nel Cestino (si rimette l'elemento esatto togliendo anche il suo `$I…`) e
  collegamenti creati. Elimina e Incolla del menu completo della shell passano da [z]f come i
  tasti, quindi si annullano. Restano fuori Shift+Canc, le altre voci del menu della shell e gli
  elementi senza percorso (zip, telefoni, allegati), trascinati o incollati.
- **Prima di toccare un elemento** il thread controlla il disco: se l'elemento non c'è più, se il
  suo posto è occupato, se la cartella è sparita, se una copia è stata modificata dopo o aveva
  sostituito un file, lo salta e lo dice; non sostituisce mai niente. Se il motivo può passare
  (file in uso, Annulla nella finestra della shell, permessi), gli elementi non riusciti restano in
  cima alla cronologia e il Ctrl+Z dopo riprova. Gli avvisi dicono cosa e dove: esempi in
  `DESIGN.md`, "Avvisi".
- **Cronologia** per finestra, `UNDO_MAX` (100) operazioni, solo in memoria; con l'opzione
  `annulla_dopo_riavvio` (spenta) si salva in `%APPDATA%\z-f\annulla.txt`.
- **Chiudendo [z]f durante un'operazione** la finestra sparisce subito, ma il processo esce solo
  quando l'operazione è finita; riaprendo [z]f nel frattempo torna la stessa finestra.

**Misurato:** registrare un'operazione costa meno di un microsecondo; annullare l'eliminazione di 5
elementi 3,6 ms sul thread (mandarli nel Cestino 98 ms).

## Trascinamento

**Come funziona** (`drag.cpp`; aspetto in `DESIGN.md`, "Trascinamento dei file").

- **Destinazioni:** una cartella della lista, lo spazio vuoto (la cartella aperta), un preferito o
  un'unità, un segmento del percorso, una scheda (fermi `DRAG_SPRING`, la scheda si apre); tra i
  preferiti le cartelle si aggiungono ai preferiti. Stessa unità sposta, altra unità copia; Ctrl
  copia, Shift sposta, Alt crea un collegamento; col tasto destro, al rilascio, il nostro menu.
- **Mentre il mouse si muove** l'effetto si decide senza disco né shell; il controllo "dentro sé
  stessa" si rifà solo quando cambia la destinazione.
- **Al rilascio** i file veri si copiano o spostano su un thread con `foRun` (`fileops.cpp`),
  e l'esito di ogni elemento entra nella cronologia di Ctrl+Z. L'elenco trascinato si legge da
  `DROPFILES` in un passaggio (`hdropPaths`).
- **Collegamenti ed elementi senza percorso** (telefoni, archivi, allegati) li riceve la shell: il
  suo `IDropTarget`, su un thread, dentro un involucro che non permette l'operazione asincrona (né
  con `IDataObjectAsyncCapability` né con il formato "AsyncFlag" degli appunti), così la chiamata
  torna solo a operazione finita. Lo stesso codice serve a Incolla senza percorsi. I collegamenti
  creati si riconoscono confrontando i .lnk della cartella prima e dopo. L'avviso dice solo quello
  che è arrivato davvero: dopo Annulla, o se l'esito non si può verificare, non dice niente.
- **Trascinare dalla lista:** sotto `kDragFastItems` (500) voci si trascina l'oggetto della shell,
  con tutti i formati di Explorer; da 500 in su un oggetto con i soli percorsi (`CF_HDROP`, come
  Copia e Taglia) e come immagine l'icona del primo elemento. L'oggetto lo prepara
  `dragObjectFor`, che serve anche al trascinamento da uno zip; in F12 "copia, taglia, trascina,
  menu (UI)", nel registro "oggetto dei file lento".
- A fine operazione, con migliaia di voci, i totali delle cartelle si dimenticano in un colpo.
- OLE per ricevere i file (`dropRegister`) si registra dopo il primo frame, non all'avvio.

**Perché così.**

- `DragQueryFileW` riparte dall'inizio a ogni voce: 20.300 voci in 10,4 s, contro 37 ms di
  `hdropPaths`.
- L'oggetto della shell costa circa 25 µs a voce (20.300 voci: 550 ms): per questo la soglia di
  500 voci.
- Dimenticare i totali delle cartelle uno a uno costava 76 ms sul thread dell'interfaccia con
  20.000 file.

**Misurato:**

- sul thread dell'interfaccia la chiamata più lenta è 6 ms (la prima entrata);
- inizio di un trascinamento con l'oggetto dei soli percorsi: 20.000 file in 10,8 ms, 600 in
  1,4 ms;
- `dropRegister` 4,7 ms.

## Appunti e rinomina

**Come funziona.**

- **Copia e Taglia di file** (`clipSetFiles`, `shell.cpp`): i percorsi sono già in memoria, quindi
  negli appunti vanno `CF_HDROP` e "Preferred DropEffect" senza toccare il disco né la shell.
  Uscendo, gli appunti vengono consegnati a Windows (`OleFlushClipboard`), così un Incolla fatto
  dopo funziona ancora. Dopo Taglia e Incolla gli appunti si svuotano, come in Explorer.
- **Incolla** legge `CF_HDROP` in un passaggio (`hdropPaths`). Funziona anche con elementi senza
  percorso (copiati da dentro uno zip in Esplora file, da un telefono o da un allegato): negli
  appunti ci sono solo i formati della shell, e l'oggetto degli appunti va alla cartella come un
  rilascio (il suo `IDropTarget`, sul thread di Incolla), che li estrae.
- **Testo negli appunti** (`text.cpp`): Copia e Taglia nelle caselle e "Copia percorso" scrivono
  con `clipSetText`, Ctrl+V legge con `clipAskText`, tutti e due su un thread. Riprovano ad aprire
  gli appunti ogni `CLIP_RETRY` (50 ms) per al massimo `CLIP_WAIT_MAX` (1 s), come Windows Forms e
  WPF; proprietaria resta la finestra di [z]f. L'esito torna con `WM_APP_CLIPSET`: "Percorso
  copiato" arriva solo se il testo è negli appunti, altrimenti un avviso dice perché ("gli appunti
  sono aperti da rdpclip.exe"), e un Taglia non riuscito rimette il testo nella casella,
  selezionato. Una copia più recente vince: la precedente, se non ha ancora scritto, non scrive più
  (contatore di generazione controllato ad appunti aperti). Nel registro "appunti lenti (testo)"
  oltre 25 ms e "appunti non scritti".
- **Rinomina nella riga:** una casella di testo vera (`TextField`, la stessa della barra del
  percorso: cursore, selezione, Ctrl+frecce, appunti), con il nome preselezionato senza
  estensione; lo spostamento avviene su un thread
  (`fsStart`).

**Perché così.**

- Copia e Taglia di file senza la shell: 20.300 voci in 5,8 ms invece di 545.
- Il testo passa da un thread perché `OpenClipboard` fallisce finché un altro programma tiene
  aperti gli appunti (rdpclip, un gestore degli appunti), `EmptyClipboard` aspetta chi li
  possedeva (un altro processo, che può essere fermo) e chi possiede gli appunti può prepararne il
  contenuto solo quando servono: il thread dell'interfaccia aspetterebbe senza limite.

**Misurato** con `prove\appunti-prova.cpp`: appunti tenuti aperti 0,6 s, la copia riesce in
427 ms; tenuti aperti 3 s, avviso dopo 1,0 s; proprietario fermo 4 s, `EmptyClipboard` aspetta
3,4 s sul thread mentre la lista continua a rispondere.

## Zip

**Come funziona** (`zip.cpp`, `zipout.cpp`; comportamento in `DESIGN.md`, "Archivio zip"). Un
lettore nostro, senza dipendenze e senza zipfldr della shell.

- **Elenco:** si legge la directory centrale in fondo all'archivio (anche zip64, oltre 65.535 voci
  e 4 GB), su un thread; ne nasce un albero di cartelle con i totali già sommati (colonna
  Dimensione, tooltip, anteprima: niente conteggio), indicizzato da una tabella a indirizzamento
  aperto sui nodi, senza allocazioni. Gli ultimi `kZipKeep` (4) archivi restano in memoria, quindi
  entrare nelle sottocartelle non rilegge niente; se l'archivio cambia (dimensione o data) si
  rilegge. Nomi in UTF-8 (bit 11, o il campo Unicode di Info-ZIP) oppure nella codepage OEM del
  sistema, come li scrivono Esplora file e 7-Zip (850 in Italia), e non la 437 della specifica:
  così i nomi sono gli stessi che mostra Esplora file, maiuscole accentate comprese.
- **Percorsi:** il percorso continua oltre l'archivio (`C:\x\foto.zip\2024\mare`), quindi
  navigazione, schede, "Vai a", preferiti, filtro, ordinamento e griglia funzionano come altrove.
  Su un thread questi percorsi li riconosce `zipSplit`/`zipInside` (un `GetFileAttributes` sul
  prefisso che finisce in ".zip", solo se c'è); il thread dell'interfaccia li riconosce dalla
  cartella aperta (`Listing::zipLen`), senza disco. Niente shell su questi percorsi.
- **Decompressione:** inflate scritto da noi (tabella diretta da 10 bit per i codici di Huffman,
  CRC-32 a 8 byte per passo) e il metodo "stored"; ogni file estratto si controlla col CRC. Gli
  altri metodi (deflate64, bzip2, LZMA, zstd) e gli zip cifrati si lasciano a Windows: l'avviso lo
  dice e si apre Esplora file su quella voce.
- **Aprire, copiare, trascinare** (`zipJobStart`): si estrae su un thread in `%TEMP%\z-f\<n>`, una
  cartella per estrazione; quelle più vecchie di `ZIP_TEMP_KEEP` (3 giorni) si tolgono
  all'estrazione dopo. Poi il doppio clic apre il file estratto, Ctrl+C mette negli appunti i file
  estratti (Incolla funziona in [z]f e in Esplora file), il trascinamento parte quando
  l'estrazione è finita, col tasto ancora premuto (rilasciando prima non parte niente e
  l'estrazione si ferma). L'oggetto dei file per il
  trascinamento è quello di `dragObjectFor`. Oltre `LOADING_AFTER` la barra di stato dice
  "Estrazione dall'archivio…", rinnovato ogni `NOTICE_RENEW` finché dura.
- **Uno zip dentro uno zip** si apre in [z]f solo se è l'ultima richiesta di apertura e l'utente è
  ancora nella scheda e nella cartella da cui l'ha chiesto (salvate nel `ZipJob`; la scheda con
  `TabState::id`, che non cambia chiudendone un'altra) e non ne sta
  caricando un'altra; la copia estratta resta in `%TEMP%\z-f` e il doppio clic successivo estrae di
  nuovo. Un file invece si apre sempre: ogni doppio clic è una richiesta, come fuori dagli zip, e
  la cartella in cui si è non conta per un programma esterno.
- **Anteprima e miniature:** immagini e testo si estraggono in memoria (`zipReadMem`; WIC da un
  `IWICStream`; del testo solo i primi 256 KB, poi l'inflate si ferma). Lo zip stesso, selezionato
  in una cartella normale, mostra nell'anteprima il suo contenuto; uno zip dentro uno zip invece
  mostra le informazioni di un file (`pvDir` risponde `ERROR_DIRECTORY` e `pvBuild` ripiega su
  `pvInfo`), perché il suo contenuto non si legge senza estrarlo. Le icone sono per
  estensione (dentro lo zip il file non c'è sul disco). Le miniature vengono sempre da WIC, anche
  quando si rifanno dopo che l'atlante o la RAM le hanno tolte (`thRestart`).
- **Sola lettura:** l'archivio non si osserva; F5 lo rilegge se è cambiato.

**Perché così.**

- zipfldr della shell rilegge l'archivio a ogni passo.
- L'albero usava una mappa di percorsi: 44 ms per 70.000 voci, contro 17 della tabella a
  indirizzamento aperto.
- Le miniature dentro lo zip non usano la cache di Windows: passerebbe dalla cartella della shell
  dentro lo zip, cioè da zipfldr.
- Uno zip nello zip si apre solo se l'utente è ancora lì: dopo un'estrazione lunga [z]f non lo
  porta via da dove è andato nel frattempo.

**Misurato:**

- elenco: 70.000 voci (zip64) in 17 ms la prima volta, poi dalla memoria (un `GetFileAttributesEx`
  per sapere se è cambiato) e l'elenco della cartella in 0,1 ms; 177 voci in 0,3 ms;
- decompressione: 138 MB di registro in 244 ms (538 MB/s), 175 file con binari incomprimibili in
  112 ms; tutti identici agli originali (sha1), anche con nomi accentati e in codepage 850
  (`prove\zip-prova.cpp`);
- uno zip di 722 MB estratto da uno zip in 480 ms; tornando alla cartella superiore o aprendo una
  scheda nuova durante l'estrazione si resta lì; trascinare un file da uno zip 4,2 ms sul thread UI
  la prima volta;
- uno zip di 1.200 foto in griglia grande, fino in fondo e ritorno: nessuna richiesta alla shell,
  2.943 decodifiche WIC da 1,28 ms.

## Collegamenti

**Come funziona** (`lnk.cpp`, `links.cpp`; colonna in `DESIGN.md`, "colonna Destinazione").

- **Lettore nostro** (`linkRead`, solo su un thread), senza la shell: LinkInfo, variabili
  d'ambiente come `%windir%`, percorso relativo. La destinazione non si tocca mai, quindi una
  condivisione irraggiungibile non rallenta niente.
- **Colonna Destinazione** (`colonna_destinazione`): che ci sia lo dice il thread di lettura dai
  nomi, così la lista non cambia forma. Le destinazioni le legge un thread nell'ordine della vista
  e restano in memoria per percorso e data, quindi una ricarica le ritrova subito. `linkOf` dà la
  destinazione già letta di una voce, `linkReveal` fa "Apri percorso file". Tooltip e anteprima di
  un .lnk hanno la riga "Destinazione".
- F12 mostra il tempo del thread.

**Misurato** con `prove\collegamenti-prova.cpp` su tutti i 177 collegamenti del PC: uguali a
`IShellLink` di Windows, 0,09 ms l'uno contro 0,86; la prima lettura dopo l'avvio del PC circa
5 ms, per disco e antivirus, sul thread.

## Dialoghi Apri e Salva

**Come funziona** (`filedlg.cpp`; opzione "Dialoghi Apri e Salva nella cartella di [z]f",
`dialoghi`).

- Nessuna DLL negli altri processi: `SetWinEventHook` fuori contesto dice quale finestra passa in
  primo piano. Se è un dialogo Apri o Salva (nuovo o classico), un thread scrive il percorso nel
  campo del nome, preme Apri/Salva (con una cartella il dialogo ci entra) e rimette il nome com'era.
  La scelta di una cartella non si tocca (confermare la sceglierebbe).
- **Attesa del dialogo:** prima di agire aspetta che il dialogo sia pronto (la vista dei file con
  il suo elenco), controllando ogni `FD_POLL` per al massimo `FD_WAIT_MAX`, più `FD_SETTLE`
  (150 ms) di margine. Le risposte del dialogo si aspettano al massimo `FD_MSG_WAIT` (campo del
  nome) e `FD_OK_WAIT` (Apri/Salva). Tutti i tempi stanno in `theme.h`.
- **La cartella si controlla fuori dal turno** (`fdCheckThread`, un thread per richiesta): che
  esista ancora lo chiede `GetFileAttributesW`, che su una condivisione che non risponde aspetta il
  timeout di rete. La richiesta passa al salto solo se è ancora l'ultima e non è passato
  `FD_WAIT_MAX` da quando è arrivata.
- **Un salto alla volta per dialogo** (`FdWork`): ogni dialogo ha al più un thread del salto, che
  fa l'ultima cartella controllata finché ne arrivano. Se si torna al dialogo con un'altra
  cartella durante un salto, conta l'ultima: il salto in corso, finché non ha toccato il campo del
  nome (attesa del dialogo, margine, lettura del nome), si lascia appena arriva la richiesta
  nuova; dopo va fino in fondo, e l'ultima segue. `FD_WAIT_MAX` vale dalla richiesta, non dall'inizio
  del salto: un salto che arriverebbe più tardi (dialogo lento a costruirsi, salto precedente da
  finire) si lascia, perché intanto l'utente lavora già nel dialogo. Il thread UI scrive la
  richiesta sotto un lock che i thread tengono solo per copiarla, quindi non aspetta.
- Con più finestre di [z]f agisce l'ultima usata (HWND in memoria condivisa). Il tempo per F12
  (`A.fdJumpUs`, in µs) lo scrivono i thread dei dialoghi con `InterlockedExchange64`.

**Perché così.**

- Agire prima che il dialogo sia pronto fa cadere il programma in COMDLG32 (provato).
- Due thread sullo stesso dialogo si intreccerebbero: il secondo leggerebbe come nome "vecchio" il
  percorso messo dal primo, e il suo Apri/Salva potrebbe confermare il nome appena rimesso dal
  primo (Salva salverebbe il file).
- Un salto che ha già toccato il campo del nome va fino in fondo, perché solo così il nome proposto
  torna al suo posto.
- Il controllo della cartella dentro il turno faceva aspettare in fila, dietro una condivisione
  lenta, anche le richieste più nuove. Fuori dal turno i controlli possono essere più d'uno
  insieme, ma toccano solo il disco: sul campo del nome agisce sempre un thread solo.

**Misurato:** notifica sul thread UI al massimo 0,14 ms, salto 290 ms sul thread (quasi tutto
attesa del dialogo). Con `prove\dialoghi-prova.cpp ritorno` (Salva, cartelle locali; `ritorno`
simula il ritorno rapido senza mouse): 220 ms dalla comparsa del dialogo alla cartella, l'ultima in
coda 180 ms dopo la precedente, un salto lasciato a metà non aggiunge niente (235 ms). Con il
controllo della cartella su un thread suo (novembre 2026, stessa prova): notifica sul thread UI al
massimo 0,095 ms (crea il thread di controllo), A 254 ms dopo il ritorno, B 210 ms dopo il suo,
nessuna conferma del nome, nome proposto intatto.

## [z]f al posto di Explorer

**Come funziona** (`integration.cpp`; dalle opzioni o con
`z-f.exe --predefinito=si|no`).

- Doppio clic sulle cartelle e sugli zip del desktop, programmi che aprono una cartella o uno zip,
  Win+E e l'avvio di `explorer.exe` aprono [z]f.
- Scrive solo in HKCU: un verbo `[z]f` reso predefinito in `Classes\Directory\shell`,
  `Classes\Drive\shell` e `Classes\CompressedFolder\shell` (la classe con cui Windows apre gli
  zip in Esplora file; il verbo `open` di Windows non si tocca) e il comando di
  `CLSID\{52205fd8-…}\shell\opennewwindow` per Win+E. I valori che c'erano prima si salvano in
  `HKCU\Software\z-f\Ripristino` e si rimettono alla disattivazione.
- Uno zip passato come argomento si apre come cartella (`resolveStartArg`), non selezionato nella
  cartella che lo contiene. Se gli zip sono associati a un altro programma (7-Zip, WinRAR) Windows
  non usa `CompressedFolder` e il verbo resta inerte. Un'integrazione attivata prima della
  1.0.0-beta.6 non ha gli zip: `defaultState` la riconosce (stato 3) e un clic sull'interruttore
  li aggiunge, senza scritture nel registro all'avvio.
- Cestino, Pannello di controllo e gli altri oggetti virtuali restano a Explorer.
- Si registra l'eseguibile da cui lo si attiva: se lo si sposta o lo si cancella, prima va
  disattivato. L'aggiornamento automatico non cambia il percorso, quindi l'integrazione resta.
- Emergenza senza [z]f: `prove\ripristina-explorer.bat`. Prova: `prove\predefinito.bat`.

## Aggiornamento automatico

**Come funziona** (`update.cpp`; opzioni `aggiornamenti` e `aggiornamenti_beta`; icona di stato in
`DESIGN.md`, "Versione e aggiornamento").

- **Quando:** `UPD_DELAY` (30 s) dopo il primo frame, e poi ogni `UPD_TICK` (un'ora) se l'ultimo
  controllo (`aggiornamenti_ultimo`) ha più di `UPD_EVERY` (un giorno), la finestra principale fa
  partire un thread a priorità bassa (`updCheck`). Il thread dell'interfaccia riceve solo il
  risultato (`WM_APP_UPDATE`, con un contatore di generazione).
- **Controllo:** chiede all'API pubblica di GitHub le ultime 10 Release, senza login, con WinHTTP
  (a caricamento ritardato), il proxy del sistema e tempi massimi `UPD_TIMEOUT` (10 s) e
  `UPD_TIMEOUT_READ` (30 s per leggere). Un lettore JSON nostro tiene solo `tag_name`,
  `prerelease`, `draft`, `html_url` e i file. Sceglie la versione più alta del canale (Semantic
  Versioning: `1.0.0-beta.2` < `1.0.0-beta.10` < `1.0.0-rc.1` < `1.0.0`) più nuova di quella in
  uso.
- **Download e firma:** scarica in memoria `z-f-<versione>.exe` e `z-f-<versione>.exe.sig` e
  controlla dimensione, impronta SHA-256 (il `digest` che GitHub dà per ogni file) e firma ECDSA
  P-256 con la chiave pubblica che [z]f contiene (CNG, `bcrypt.dll`, a caricamento ritardato). La
  chiave privata sta solo sul PC di chi pubblica, cifrata con DPAPI (`strumenti\firma.cpp`).
- **Scambio**, sullo stesso disco: `z-f.exe.nuovo` accanto all'exe, `z-f.exe` → `z-f.old.exe`
  (Windows permette di rinominare un exe in esecuzione), `z-f.exe.nuovo` → `z-f.exe`; se il
  secondo passo fallisce si rimette il primo. Un'altra finestra che controlla dopo lo scambio trova
  già sul disco l'exe giusto (`z-f.old.exe` c'è e l'impronta di `z-f.exe` coincide) e non scarica
  di nuovo. Dopo il primo frame (`updStartup`) la versione nuova cancella `z-f.old.exe` su un
  thread e dice "Aggiornato a [z]f …". Dopo "Aggiorna" la versione di prima la lancia mentre sta
  ancora uscendo, e finché il suo processo c'è il suo exe non si può cancellare: il thread aspetta
  che esca, al massimo `UPD_OLD_WAIT` (10 s).
- **Quando non scambia:** se la cartella non è scrivibile o la Release non ha l'exe firmato,
  l'avviso dice che c'è una versione nuova e un clic apre la sua pagina. Mai su un exe in rete né
  nelle build compilate con `ZF_DEV` (`build.bat dev`, `build_zig.bat`): lì si controlla solo con
  un clic sull'icona (`UpdJob::checkOnly`), per vedere l'icona di stato; ci si ferma prima del
  download e l'ora del controllo non si salva, così quello giornaliero di `z-f.exe`, che usa le
  stesse impostazioni, non si sposta.
- **Riavvio di tutte le finestre** (`updRestartAll`, `restartLaunch`): "Aggiorna" manda alle altre
  finestre di [z]f un messaggio registrato (`z-f.riavvio`). Ognuna salva le sue schede in
  `riavvio-<pid>.txt` e la sua posizione, si chiude (con operazioni sui file in corso aspetta che
  finiscano) e, uscendo, si riavvia con `--riavvio=<pid> --finestra=…`; `--principale` per quella
  che riceve le cartelle da fuori, che tiene anche il mutex dell'istanza unica.
- **Prove:** `--aggiorna-da=<url>` usa un server locale (`<url>/releases.json`) al posto di GitHub,
  e il controllo parte dopo `UPD_DELAY_TEST` (2 s) anche in una finestra secondaria.

**Perché così.**

- Senza login GitHub dà 60 richieste l'ora per indirizzo: ne serve una al giorno.
- La firma, oltre all'impronta, fa sì che chi entrasse nell'account GitHub non possa distribuire un
  exe che [z]f accetta.
- Lo scambio non cambia il percorso, quindi "[z]f al posto di Explorer" e i collegamenti
  continuano a funzionare.
- Su un exe in rete lo scambio lo farebbe la rete, a metà.

**Misurato** con un server locale: controllo 18 ms, download dell'exe e della firma 25 ms, tutto
sul thread; con il server spento l'errore arriva dopo 2 s, sempre sul thread.

## File di [z]f e impostazioni

**Come funziona** (`settings.cpp`).

- **I file**, in `%APPDATA%\z-f`: `impostazioni.txt`, `preferiti.txt`, `recenti.txt`, `viste.txt`,
  e solo con le loro opzioni `annulla.txt` e `schede.txt`; `riavvio-<pid>.txt` per un istante
  durante un aggiornamento. Si scrivono solo con `saveDataAsync`, su un thread, con un file
  temporaneo poi rinominato: un file non resta mai a metà.
- **Le impostazioni** di `impostazioni.txt`, modificabili anche a mano: `dimensione`, `carattere`,
  `barra_laterale`, `barra_laterale_larghezza`, `barra_tempi`, `animazioni`, `preferiti_aperti`,
  `unita_aperte`, `colonne` (quelle mostrate), `colonna_<chiave>` per ogni colonna (`colonna_tipo`,
  `colonna_data`, `colonna_creazione`, `colonna_dimensione`, `colonna_attributi`,
  `colonna_destinazione`), `estensioni`, `anteprima`,
  `anteprima_larghezza`, `anteprima_posizione` (`destra` o `basso`), `anteprima_altezza`,
  `anteprima_vista`, `carattere_anteprima`, `annulla_dopo_riavvio`, `menu_completo`,
  `dimensione_cartelle`, `dialoghi`, `riapri_schede`, `finestre_separate`, `aggiornamenti`,
  `aggiornamenti_beta`, `aggiornamenti_ultimo` (e, solo in lettura, `griglia_dimensione` delle
  versioni fino alla 1.0.0-beta.5). Il pannello delle opzioni (`options.cpp`) le
  cambia con effetto immediato, per sezioni: Aspetto (dimensione e carattere del testo,
  animazioni), Finestra (barra laterale, anteprima, sua posizione e suo carattere), Integrazione
  con Windows ("[z]f al posto di Explorer", "Menu completo di Windows col tasto destro", "Dialoghi
  Apri e Salva nella cartella di [z]f"…), File ("Dimensione delle cartelle nella colonna"…), Avvio
  e aggiornamenti, l'elenco delle scorciatoie con ricerca e, per ultima, Diagnostica (barra dei
  tempi). In fondo ha la versione, che apre la sua
  Release su GitHub (`kUpdThisRelease`, `updOpenPage` su un thread), e un collegamento alla
  cartella delle impostazioni.
  Il pannello si dispone a ogni frame mentre si disegna (`drawOptions`): le aree cliccabili
  (`OP.hits`) e le scorciatoie troncate per il tooltip (`OP.tips`, lette da `optionsTipTarget`)
  sono quelle dell'ultimo frame, e l'altezza del contenuto (per lo scorrimento e la barra) si sa
  alla fine. Le spiegazioni vanno a capo con `textLineEnd` (`text.cpp`), a ogni frame: sono una
  decina di righe. Il pannello non usa il ritaglio del renderer (`clipBegin`, che serve alle voci
  dell'anteprima): si disegnano solo righe intere e i fondi sotto il mouse si tagliano a mano tra
  titolo e piede. Con la ricerca piena si disegnano solo le
  scorciatoie, così i risultati sono in cima senza calcolare dove cade la loro sezione.
  Misurato: frame con il pannello aperto 0,07–0,08 ms (barra dei tempi, z-f-dev, 2560×1440 a
  100%), anche con tutte le scorciatoie in vista.
- **Lettura all'avvio:** impostazioni, preferiti, viste, recenti, annulla e schede li legge
  `sideFastThread`, e il primo frame lo aspetta al massimo 100 ms.
- **Mai sovrascritti prima della lettura:** nessuno di questi file si scrive prima che sia stato
  letto (`A.sideFastDone`), e quando arriva si unisce a ciò che è cambiato intanto:
  - impostazioni: `settingsSave` ricorda solo le chiavi diverse da quelle dell'avvio
    (`settingsMarkStart`, in `sideStartFast`) e `settingsLoaded` ne rimette i valori sopra quelli
    letti;
  - preferiti: quelli aggiunti prima vanno in fondo a quelli del file, o a quelli predefiniti del
    primo avvio (`favAdopt`);
  - viste: le griglie scelte prima restano, come le più recenti (`viewsLoaded`);
  - annulla: le operazioni fatte prima restano le più recenti (`undoLoaded`);
  - recenti: si sommano (`recentLoaded`).

  Dopo l'unione si salva una volta. Un avvio normale non scrive niente.

**Perché così.** Con il disco lento (antivirus all'accesso, avvio a freddo) l'utente può cambiare
qualcosa prima che i file arrivino, e salvare allora riscriveva il file con i valori predefiniti o
con la sola modifica nuova: provato con la lettura ritardata di 4 s, impostazioni tornate ai
predefiniti, tre preferiti diventati uno, due viste diventate una.

**Misurato:** `settingsMarkStart` 0,02–0,04 ms.

## Compilare: MSVC e zig

`build.bat` trova da solo `cl.exe` tramite `vswhere`. Mette anche la cartella di `vswhere` nel PATH,
perché `vcvars64.bat` lo chiama per nome: senza, scrive "vswhere.exe non è riconosciuto" (innocuo, ma
in mezzo all'uscita dove si cercano gli avvisi). Se nel PATH c'è anche `fxc.exe` (incluso nel
Windows SDK), gli shader vengono precompilati e l'avvio è più rapido; altrimenti vengono compilati
all'avvio, e la barra di stato lo segnala. L'icona (`src\z-f.rc`) la compila `rc.exe`, anche lui
del Windows SDK, in `build\z-f.res`, che va al link con il resto.

Oltre alla build di rilascio (`build\z-f.exe`) e a `debug`:

- `build.bat dev` scrive la build di prova `build\z-f-dev.exe`, con le stesse opzioni più `ZF_DEV`
  (non si aggiorna, controlla solo a richiesta): si compila anche mentre `z-f.exe` è aperto;
- `build.bat prova prove\x.cpp <librerie>` compila un programma di prova con le opzioni di tutti, e
  l'exe va in `build\`. Il comando esatto è in cima a ogni file di `prove\`.

Si lancia da Bash con `cmd //c '.\build.bat …'`. Il `.\` serve dove la cartella corrente non è
nella ricerca dei comandi; da PowerShell no, perché non entra in una cartella con le parentesi
quadre.

**Senza Visual Studio:** `winget install zig.zig`, poi `build_zig.bat`, che scrive
`build\z-f-dev.exe` (la build di prova) e non tocca `build\z-f.exe`. Usa gli shader precompilati se
`build.bat` li ha già prodotti in `build\`.

La build consigliata è quella MSVC. Confronto misurato sullo stesso codice e sulla stessa cartella
(WinSxS, 30.000 voci):

| | MSVC | zig |
|---|---|---|
| eseguibile | 324 KB | 668 KB |
| shader | precompilati (0 ms) | compilati all'avvio (circa 4 ms) |
| ordinamento 30k nomi | 4,3 ms | 7,5–8 ms |
| contenuto visibile | 67–73 ms | 69–81 ms (differenza nel rumore) |

**DLL a caricamento ritardato.** Con MSVC, `shell32`, `ole32`, `winhttp` e `bcrypt` si caricano
solo al primo uso (`/DELAYLOAD`):

- `winhttp` e `bcrypt` (l'aggiornamento): il primo uso arriva `UPD_DELAY` (30 s) dopo il primo
  frame;
- `shell32` e `ole32` invece si caricano comunque all'avvio, prima del primo frame: `iconInit`
  (`main.cpp`, dopo Direct3D e prima del primo `render`) avvia il thread delle icone per estensione
  (`iconThread`, `IQ_EXT`), che chiama subito `CoInitializeEx` e `SHGetFileInfoW` per preparare le
  icone di sistema della shell. Il caricamento avviene su quel thread, non su quello
  dell'interfaccia, e così le icone sono pronte prima: per queste due DLL il caricamento ritardato
  sposta il caricamento su un altro thread, ma non lo evita. È un'eccezione voluta: spostare
  l'avvio dei thread delle icone dopo il primo frame è peggio su tutto. Misurato (novembre 2026,
  `prove\avvio.bat`, 2 × 20 avvii alternati, mediane prima e dopo lo spostamento): contenuto
  58–70 → 71–85 ms, prima icona vera 86–99 → 116–131 ms ("prova avvio: icone"). In più il
  processo, chiuso appena arrivata la prima icona, restava vivo circa 7 s invece di 0,2:
  probabilmente il thread delle voci lente ancora sulla rete alla chiusura (vedi "Barra laterale",
  dove è misurato su altre chiusure).

WIC (anteprima e miniature, sul loro thread) e mpr.dll (unità di rete, `LoadLibraryExW`) si
caricano solo al primo uso.

## Controllo prima del push

`.githooks/pre-push` (attivo con `git config core.hooksPath .githooks`, una volta per copia del
repository; solo `sh`, `git`, `grep` e `sed` di Git for Windows) fa due controlli.

- **main:** blocca il push diretto su `main`, che si aggiorna solo con una pull request da `dev`.
- **Dati da non pubblicare** (il repository è pubblico). Per ogni ref in uscita prende i commit che
  il remoto non ha (`remote_sha..local_sha`; per un branch nuovo, quelli che nessun branch del
  remoto contiene) e di ciascuno guarda solo ciò che è nuovo: righe aggiunte del diff (contro il
  primo genitore), messaggio, nomi dei file, nome e email di autore e committer. Blocca:
  - le email che non sono quelle anonime di GitHub (`…@users.noreply.github.com`,
    `noreply@github.com`);
  - i percorsi della cartella di un utente (`X:\Users\…`, `X:/Users/…`, anche con le barre
    raddoppiate dei sorgenti, e `/x/Users/…` di Git Bash); ammessi `utente`, `Public`, `Default` e
    i segnaposto come `<nome>`;
  - i file `.exe`, `.sig`, `.bin`, `.pfx`, `.pem`, `.key`, `.p12`;
  - le parole di `.git/info/parole-private.txt` (una per riga, `#` per i commenti), come parole
    intere (`grep -w`) e senza distinguere maiuscole, così un nome non scatta dentro un'altra
    parola. Quel file sta in `.git`, quindi non si pusha mai, e l'hook, che è pubblico, non
    contiene nessuna parola privata; se manca, l'hook avvisa e non blocca.

  Ogni segnalazione dice commit, file e riga (o messaggio, nome di file, email); il messaggio
  finale chiede di correggere i commit (`git commit --amend`, `git rebase -i`).

**Perché così.** Il diff si percorre riga per riga in `sh`, che è lento, solo se un `grep` su
tutto il diff trova già qualcosa.

**Misurato:** un commit 0,7 s; tutta la storia pubblica (6 commit, il primo con tutto il codice)
2,9 s; il caso peggiore, una parola trovata nel primo commit, 4,9 s. Le prove sono in un
repository finto con un remoto nudo locale (21 casi, tutti passati).

## Differenze note rispetto a Explorer

L'ordinamento naturale qui è ordinale, con minuscole e accenti piegati; Explorer usa invece le
regole linguistiche di Windows. Nei casi comuni il risultato è uguale (numeri, maiuscole e
minuscole, lettere accentate). Può differire sui segni di punteggiatura: ad esempio Explorer tende
a ignorare il trattino in "co-op".
