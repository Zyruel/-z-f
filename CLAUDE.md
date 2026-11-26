# [z]f — istruzioni per Claude

[z]f è il file explorer per Windows di Zyruel: C++17, Win32, Direct3D 11, nessun framework.
È gratuito e open source (licenza MIT). L'obiettivo è essere il miglior file explorer per Windows.

**Nome:** **[z]f** nell'interfaccia e nei testi; **z-f** dove le parentesi quadre darebbero
problemi (file, cartelle, registro, classe della finestra, mutex); `ZF_` negli identificatori C++.
Fino alla 0.15 si chiamava zx (le copie `build\zx-0.N.exe` sono di allora; il codice di
transizione è in `cleanupLegacyZx` e `migrateLegacyData`).

**Lingua:** con Zyruel si parla in italiano. Commenti nel codice, testi dell'interfaccia e
documentazione sono in italiano; `shader.hlsl` è solo ASCII.

## Principi (da prendere sul serio, sempre)

1. **Leggerezza:** nessuna dipendenza, niente processi o DLL in più all'avvio (shell32 e ole32
   sono a caricamento ritardato).
2. **Velocità misurata, non presunta:** barra di stato, pannello F12 e `build\z-f-log.txt`
   (rallentamenti con l'ora) dicono i tempi. Ogni funzione nuova si misura; un peggioramento si
   discute prima di tenerlo. Riferimento: contenuto visibile a 64–80 ms dall'avvio, frame sotto 1 ms.
3. **Correttezza:**
   - il thread dell'interfaccia non tocca mai il disco e non aspetta mai niente: né disco, né shell, né vsync;
   - il lavoro lento va su un thread, che consegna il risultato con `PostMessage` e un contatore di generazione per scartare i risultati superati.
4. **Pulizia e niente duplicazioni:** prima di scrivere qualcosa, cerca se esiste già. Componenti,
   colori, misure e tempi sono in un posto solo.
5. **Coerenza dell'interfaccia:** **`docs/DESIGN.md` è vincolante.** Si inventa solo per elementi davvero
   nuovi, e in quel caso li si aggiunge a `docs/DESIGN.md`. Tutti i valori visivi stanno in `src/theme.h`
   e si usano con `px()`; un numero visivo scritto direttamente nel codice nuovo è un errore.
6. **Mai interrompere l'utente:** niente finestre di dialogo nostre. Si agisce e si avvisa con
   `notice()`.

## Struttura

- **Unity build:** si compila solo `src/z-f.cpp`, che include i moduli in ordine di dipendenza.
  Le dichiarazioni anticipate stanno in `app.h`.
- **Documenti:**
  - `README.md`: breve, per chi usa [z]f (cos'è, dove scaricarlo, cosa sa fare, scorciatoie, come
    compilarlo). Niente misure né dettagli interni;
  - `docs/ARCHITETTURA.md`: come è fatto, funzione per funzione, con le misure e la tabella dei moduli;
  - `NOVITA.md`: cosa cambia in ogni versione; la sezione del rilascio è anche la nota della Release.
- **Versione:** `ZF_VERSION` in `src/base.h`, con il Semantic Versioning (deciso con Zyruel a
  novembre 2026, dopo la 0.28): `1.0.0-beta.N` le beta, `1.0.0-rc.N` le candidate, `1.0.0` la
  stabile; poi `1.0.1` per le correzioni, `1.1.0` per le funzioni nuove, `2.0.0` per i cambiamenti
  grossi. Le versioni fino alla 0.28 erano `0.N`.
- **Repository:** git, `origin` = github.com/Zyruel/-z-f, **pubblico** (licenza MIT) dalla
  1.0.0-beta.2, con una cronologia che riparte da lì. La storia fino alla 1.0.0-beta.1 (con i suoi
  tag e la sua Release) è in un archivio privato (remoto `archivio`): non ci si
  pusha più. Commit e push solo quando Zyruel lo chiede; `build/` è escluso.
- **Repository pubblico:** nei file e nei messaggi di commit niente dati personali (l'autore è
  Zyruel, mai il nome di battesimo; niente riferimenti ad amici o altre persone; niente percorsi con
  il nome utente, nomi di server, condivisioni e cartelle di lavoro: negli esempi `\\server\…`,
  `C:\Users\utente`) e niente nomi di altri file manager presi a confronto.
  `.githooks/pre-push` lo controlla nei commit in uscita (email non anonime, percorsi con un nome
  utente, `.exe`/`.sig`/chiavi, parole private) e blocca il push dicendo commit, file e riga: si
  corregge il commit. Le parole private sono in `.git/info/parole-private.txt`, che non si pusha.
- **Branch:**
  - `dev`: il lavoro di ogni giorno, si committa e si pusha qui;
  - `main`: solo versioni rilasciate, aggiornato con una pull request da `dev` (unione con merge
    commit, non squash, così `dev` resta allineato). Dopo l'unione, `dev` si riallinea a `main`
    (`git merge --ff-only origin/main`);
  - `main` è protetto su GitHub (pull request obbligatoria, anche per gli amministratori) e in
    locale da `.githooks/pre-push`, attivo con `git config core.hooksPath .githooks` (una volta per
    copia del repository), che ferma l'errore prima. Non si scavalca con `--no-verify`.
- **Rilascio:** una pull request `dev` → `main` è un rilascio: se Zyruel la chiede senza
  `/rilascio`, non la si apre e gli si ricorda `/rilascio`. Si fa con `/rilascio [versione]` (skill
  in `.claude/skills/rilascio`, la lancia solo Zyruel): versione nei documenti, build, copia in `build\z-f-<versione>.exe` (fino alla 0.28:
  `build\z-f-0.N.exe`) firmata, pull request `dev` → `main`, tag annotato e Release con l'exe e la
  firma, da cui gli exe distribuiti si aggiornano da soli. La chiave privata di firma è solo sul PC
  di Zyruel: senza, [z]f rifiuta l'aggiornamento, e non se ne crea un'altra.

## Compilare e provare

Come si compila e si prova, passo per passo: `/prova` (skill in `.claude/skills/prova`). Le regole:

- **Zero avvisi** con MSVC (`cmd /c '.\build.bat'`; attenzione a C4244, int → float, che zig non
  segnala) e con zig.
- **Eseguibili:** `build\z-f.exe` è la build MSVC che Zyruel usa, registrata come file manager
  predefinito; `build\z-f-dev.exe` la build di prova.
- **PowerShell e `[z]f`:** PowerShell non gestisce le parentesi quadre nei percorsi: gli exe si
  copiano e verificano con Bash (`cp`, `sha1sum`); i percorsi nei file .md si scrivono con lo
  strumento Edit (da Bash i backslash possono rovinarsi).
- **Su un PC vero, non a memoria.** Zyruel preferisce che le prove le faccia Claude quando può (con
  il controllo dello schermo); altrimenti gli si chiede uno screenshot.
- **Rispetto per Zyruel:** se sta usando il PC (con un'app a schermo intero, per esempio), non si
  tocca schermo né tastiera; non si interferisce con le sue finestre di [z]f (le prove in z-f-dev,
  in una finestra separata con `prove\dev.bat`; le finestre di prova si chiudono per PID, mai
  `taskkill /IM`); le prove col mouse nella lista in una cartella di prova con file finti.
- **Non scrivere nel registro** al posto di Zyruel: l'interruttore "Usa [z]f al posto di Explorer"
  lo usa lui.
- **Dopo ogni prova:** controllare `build\z-f-log.txt` (le voci normali dell'avvio sono in `/prova`).

## Convenzioni del codice

- **Disegno:**
  - primitive: `rect`, `rrect` (angoli arrotondati via shader), `drawText`, `drawUiIcon` (font di icone di Windows), `drawIcon` (icone della shell);
  - un frame si disegna solo se `A.dirty`, o se un'animazione ha impostato `A.animActive`.
- **Animazioni:** solo con `approach` e `hoverFadeUpdate`/`hoverAlpha`, `easeOut` e `fadeA`, con le
  costanti `ui::TAU_*` e `ui::DUR_*`.
- **Componenti da riusare:**
  - i pannelli sovrapposti (`panelEnter` per la dissolvenza d'ingresso, `drawPanel` per ombra,
    fondo e contorno, in `render.cpp`): ogni pannello nuovo passa da lì;
  - il menu (`menuAdd`, `menuPlace`), anche per gli elenchi che scorrono;
  - le voci e la barra di scorrimento degli elenchi nei pannelli (`drawPanelItem`,
    `drawPanelScroll`), usate dal menu e da "Vai a";
  - `drawTextMarked` per evidenziare le lettere trovate da una ricerca;
  - la lettura delle sottocartelle su un thread (`readSubfolders`, in `crumbs.cpp`);
  - il tooltip (`tipRow`); le informazioni di un file (`readInfo` sul thread, `fileRows`,
    `driveRows`) servono anche all'anteprima;
  - l'anteprima (`preview.cpp`): decodifica WIC ridotta con EXIF (`pvImage`), testo con la
    codifica riconosciuta (`pvText`), icona grande della shell (`pvBigIcon`); le usano anche le
    miniature (`thumbs.cpp`, con `PvReq::superseded`);
  - le voci della lista, in elenco e in griglia: `itemRect`, `itemBg`, `itemAt`, `gridParts`
    (`app.cpp`). Il codice nuovo che riguarda una voce passa da lì, mai da `rowH`;
  - `TextField` e `drawField` (anche il suggerimento a campo vuoto, "Cerca…");
  - domande su percorsi e unità, solo dal thread UI (`app.cpp`): `slowPath` (rete o ottica: niente
    letture in background, dopo le locali), `onlineOnly` (file solo online: non si leggono),
    `driveKind` (il tipo di unità con le parole di Windows), `uncShareRoot`;
  - file negli appunti e trascinati: `clipSetFiles` (Copia/Taglia, `shell.cpp`), `hdropPaths`
    (lettura di `CF_HDROP` in un passaggio, `fileops.cpp`); testo negli appunti: `clipSetText` e
    `clipAskText` (`text.cpp`, su un thread, con l'esito alla finestra);
  - percorsi dentro uno zip (`zip.cpp`): `zipSplit`/`zipInside` sul thread, `Listing::zipLen` sul
    thread UI, `zipReadMem` per leggere un file in memoria; aprire, copiare e trascinare da uno zip
    con `zipJobStart` (`zipout.cpp`). Niente shell su questi percorsi (zipfldr è lenta);
  - collegamenti (.lnk): `linkRead` (`lnk.cpp`, lettore nostro, solo su un thread), `linkOf` per
    la destinazione già letta di una voce e `linkReveal` per "Apri percorso file" (`links.cpp`);
    i percorsi di rete di Windows: `netPlacesRead` (`lnk.cpp`, solo su un thread), in `A.drives`
    con `SideItem::place`;
    la cartella che contiene un percorso: `parentOf` (`app.cpp`);
  - `notice`;
  - `drawSpinner` (`render.cpp`), l'anello di punti di un lavoro in corso: l'unica animazione
    continua, solo dove l'utente guarda (DESIGN.md §5);
  - le schede da salvare o riaprire: `tabsSerialize`, `tabsRestore` (`tabs.cpp`);
  - l'aggiornamento (`update.cpp`): `updCheck`, `updAction`, `drawUpdateIcon`, e il riavvio di
    tutte le finestre (`updRestartAll`, `restartLaunch`);
  - il pannello delle opzioni;
  - la cornice (`chrome.cpp`).
- **Operazioni sui file** (`fileops.cpp`): copia, sposta, Cestino passano da `foRun`
  (`IFileOperation` con l'esito di ogni elemento) e si registrano con `undoPush`; rinomina e
  nuova cartella da `fsStart`. Un'operazione nuova sui file passa da lì, così si può annullare.
- **File di [z]f** in `%APPDATA%\z-f` (`preferiti.txt`, `impostazioni.txt`, `recenti.txt`, `viste.txt`, `annulla.txt` e `schede.txt` solo con le loro opzioni, `riavvio-<pid>.txt` per un istante durante un aggiornamento),
  letti all'avvio dal thread veloce della barra laterale e scritti solo con `saveDataAsync`, che
  usa un file temporaneo e poi lo rinomina.
- **Scorciatoie:** la tabella `kShortcuts` in `options.cpp` è l'elenco di riferimento. Ogni tasto
  può indicare la voce di menu (`MenuId`) che fa la stessa cosa: `menuAdd` la mostra da sola
  (`shortcutFor`). Una voce di menu nuova con scorciatoia si collega lì, non si scrive a mano.
  Un tasto che esegue solo un comando si scrive con `K(comando, modificatori, tasto)`: `onKey` lo
  esegue da lì (`shortcutCmd` → `menuRun`, lo stesso codice del menu; i comandi senza voce, come
  `M_BACK`, hanno il loro `case` in `menuRun`) e il nome si ricava dal tasto (`keyName`). In
  `onKey` si scrivono a mano solo i tasti che dipendono dal contesto (frecce, Esc, Backspace,
  Ctrl+V, Ctrl+1…9…).
- **Commenti:** spiegano il perché, non il cosa, in italiano.

## Debiti tecnici noti (da sistemare quando si tocca quel codice)

- **Numeri sparsi:** migrati a `theme.h` (colori, misure, segnaposto delle icone `PH_*`). Restano
  qualche `+ 1`/`- 2` di 1 px per le linee, che DESIGN.md ammette, e `ROW_EXTRA` arrotondato con
  `lroundf` invece di `px()`, eccezione voluta e scritta in `theme.h` e DESIGN.md.
- **Vsync:** sul PC di Zyruel a volte il log mostra "attesa vsync lunga" (40–110 ms). Ora non blocca
  più l'input, ma la causa non è dimostrata: forse la cattura dello schermo da remoto, forse le
  transizioni di Windows.

## Prossimi passi (concordati, in quest'ordine)

La 0.18 ha "Vai a" (`goto.cpp`, Ctrl+P · Ctrl+G), la selezione col rettangolo e le colonne
ridimensionabili (`listmouse.cpp`) e il pannello di anteprima (`preview.cpp`, Alt+P). La 0.19 ha
annulla e ripeti (`fileops.cpp`, Ctrl+Z · Ctrl+Y) e la vista a griglia con miniature (`thumbs.cpp`,
Ctrl+Shift+2/3/4 · Ctrl+Shift+6). La 0.20 ha "Questo PC" nella barra laterale, connetti e
disconnetti unità di rete (`sidebar.cpp`, mpr.dll caricata al primo uso), i nomi delle unità di
rete come in Explorer e l'opzione `menu_completo` (menu della shell col tasto destro). La 0.21 ha
la dimensione totale delle cartelle (`dirsize.cpp`) e il salto dei dialoghi Apri/Salva
(`filedlg.cpp`; prova con `prove\dialoghi-prova.cpp`). Le prove a mano che restano a
Zyruel sono in `prove\da-verificare.md`.

La 0.22 è il controllo di tutto il codice rispetto ai principi (novembre 2026; sezioni da 000000
a 00000000000 di `da-verificare.md`). La 0.23 ha il menu completo della shell su un thread suo
(`shell.cpp`, sezione 000000000000), la 0.24 l'inizio di un trascinamento con molte voci
(`drag.cpp`, sezione 0000000000000). La 0.25 chiude il controllo (sezione 00000000000000):
Shift+Canc e Proprietà di più voci sul thread del menu della shell (`shellVerb` → `smSend`;
misure con `prove\shell-verbi.cpp`), "Vai a" misurato (`prove\vai-a-misura.cpp`: nessun
cambiamento), i ritocchi da 1 px (segnaposto delle icone in `theme.h`, niente `+ 1` in
`drawCrumbs`, `ROW_EXTRA` arrotondato come eccezione scritta) e i tasti ricavati da `kShortcuts`
(decisi con Zyruel). La 0.26 ha gli zip come cartelle in sola lettura (`zip.cpp`, `zipout.cpp`,
sezione 0000000000000000) e Incolla di elementi senza percorso, come i file copiati da uno zip in
Esplora file (sezione 000000000000000). La 0.27 ha la colonna Destinazione dei collegamenti (`lnk.cpp` lettore, `links.cpp` colonna e clic, sezione 00000000000000000). La 0.28 ha la
barra laterale a tutta altezza e ridimensionabile (i bordi dei pannelli passano dal codice dei bordi
delle colonne, `listmouse.cpp`), il filtro nella barra di stato, la barra dei tempi come opzione e
le unità sotto Questo PC (sezioni 0000000000000000000 e 000000000000000000). Il monitor di
Zyruel è a 175%.

La 1.0.0-beta.3 ha l'aggiornamento automatico dalle Release, firmato
(`update.cpp`, `strumenti\firma.cpp`), con l'icona di stato accanto alla versione e il riavvio di
tutte le finestre, e la riapertura delle schede all'avvio (`tabs.cpp`); prove nella sezione
000000000000000000000 di `prove\da-verificare.md`.

**Tre elenchi da tenere aggiornati** (si scrive lì appena viene fuori qualcosa, non a memoria):
- `prove\da-verificare.md`: le prove a mano che restano, per sezione;
- `docs/idee.md`: le idee venute in mente e non ancora decise (per esempio modificare gli zip, 7z e rar),
  con cosa servirebbe e cosa le frena rispetto ai principi;
- `docs/da-sistemare.md`: le piccole correzioni e le scelte minori da confermare con Zyruel.

Prima di cominciare una funzione: rileggere `docs/DESIGN.md` e la parte di `docs/ARCHITETTURA.md` che la
riguarda.
Dopo:
- misurare e controllare il log (`/prova`);
- aggiornare `docs/ARCHITETTURA.md` (funzione, misure, tabella dei moduli), `NOVITA.md` (sezione della
  versione in corso) e `docs/DESIGN.md`; il README solo se cambia qualcosa per chi usa [z]f (elenco
  "Cosa sa fare", scorciatoie), restando breve;
- zero avvisi con MSVC;
- `/controllo` (skill in `.claude/skills/controllo`) sui file cambiati: le regole di questo file e
  di `docs/DESIGN.md`, controllate sul codice, con un resoconto e senza correzioni automatiche
  (prima di un rilascio, o quando serve, `/controllo tutto`).
