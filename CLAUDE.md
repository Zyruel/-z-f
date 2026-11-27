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
  - `docs/ARCHITETTURA.md`: come è fatto, per area (come funziona, perché così, "Misurato:"), con
    la tabella dei moduli. Niente storia (sta in `NOVITA.md`) né comportamento visibile (sta in
    DESIGN e README);
  - `NOVITA.md`: cosa cambia in ogni versione; la sezione del rilascio è anche la nota della Release.
- **Versione:** `ZF_VERSION` in `src/base.h`, con il Semantic Versioning dalla 1.0
  (`1.0.0-beta.N`, `1.0.0-rc.N`, `1.0.0`, poi `1.0.1`, `1.1.0`, `2.0.0`: le regole in `/rilascio`).
  Fino alla 0.28 le versioni erano `0.N`.
- **Repository:** `origin` = github.com/Zyruel/-z-f, **pubblico** (licenza MIT); la storia fino alla
  1.0.0-beta.1 è nel remoto privato `archivio`, dove non si pusha più. Commit e push solo quando
  Zyruel lo chiede, su `dev`. `main` riceve solo i rilasci, con la pull request di `/rilascio`
  (skill in `.claude/skills/rilascio`, la lancia solo Zyruel: se chiede una pull request `dev` →
  `main` senza `/rilascio`, non la si apre e gli si ricorda `/rilascio`). Mai `--no-verify`, push
  su `main` o push forzati: li ferma `.claude/hooks/regole.sh`, e `main` è protetto anche su
  GitHub e da `.githooks/pre-push` (attivo con `git config core.hooksPath .githooks`, una volta
  per copia del repository).
- **Bilancio:** dopo un rilascio, `/bilancio` (skill in `.claude/skills/bilancio`, la lancia solo
  Zyruel) valuta come si è lavorato nel ciclo: processo, decisioni, skill, script, permessi.
  Prende i dati dalle trascrizioni delle sessioni e da git e scrive un resoconto privato in
  `build\bilanci\`; si applicano solo le proposte che Zyruel sceglie.
- **Repository pubblico:** nei file e nei messaggi di commit niente dati personali (l'autore è
  Zyruel, mai il nome di battesimo; niente riferimenti ad amici o altre persone; niente percorsi con
  il nome utente, nomi di server, condivisioni e cartelle di lavoro: negli esempi `\\server\…`,
  `C:\Users\utente`) e niente nomi di altri file manager presi a confronto.
  `.githooks/pre-push` lo controlla nei commit in uscita (email non anonime, percorsi con un nome
  utente, `.exe`/`.sig`/chiavi, parole private, in `.git/info/parole-private.txt`) e blocca il push
  dicendo commit, file e riga: si corregge il commit.

## Compilare e provare

Come si compila e si prova, passo per passo: `/prova` (skill in `.claude/skills/prova`). Le regole:

- **Si compila da Bash:** `cmd //c '.\build.bat dev'` per la build di prova (`build\z-f-dev.exe`),
  `cmd //c '.\build.bat'` per `build\z-f.exe` (la build MSVC che Zyruel usa, registrata come file
  manager predefinito), `build.bat prova` per i programmi di `prove\`. **Zero avvisi** con MSVC
  (attenzione a C4244, int → float, che zig non segnala) e con zig.
- **PowerShell e `[z]f`:** lo strumento PowerShell non entra in una cartella con le parentesi
  quadre (resta in System32) e non le gestisce nei percorsi: compilare, copiare e verificare gli
  exe si fa da Bash (`cp`, `sha1sum`); i percorsi nei file .md si scrivono con lo strumento Edit
  (da Bash i backslash possono rovinarsi).
- **Su un PC vero, non a memoria.** Zyruel preferisce che le prove le faccia Claude quando può (con
  il controllo dello schermo); altrimenti gli si chiede uno screenshot.
- **Rispetto per Zyruel:** se sta usando il PC (con un'app a schermo intero, per esempio), non si
  tocca schermo né tastiera; non si interferisce con le sue finestre di [z]f (le prove in z-f-dev,
  in una finestra separata con `prove\dev.bat`; le finestre di prova si chiudono per PID, mai per
  nome); le prove col mouse nella lista in una cartella di prova con file finti.
- **Non scrivere nel registro** al posto di Zyruel: l'interruttore "[z]f al posto di Explorer"
  lo usa lui. Questa regola e la chiusura per nome le ferma `.claude/hooks/regole.sh`.
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
  - `drawSpinner` (`render.cpp`), l'anello di punti di un lavoro in corso (almeno un giro):
    l'unica animazione continua, solo dove l'utente guarda (DESIGN.md §5);
  - le barre in fondo, della lista e dell'anteprima: `drawBarBand`, `drawBarCounts`,
    `drawBarView`/`barViewW`, `drawBarFilter` (`render.cpp`), disegnate da destra;
  - le colonne di Dettagli (`columns.cpp`): `kCols`, `kColOrder`, `colWanted`, `colStart`/
    `colEnd`, `colText`, `colTitle`; il nome come si mostra, con le estensioni nascoste: `shownLen`;
    il valore di una voce in una colonna (filtro, gruppi): `colBucket` con `ColCtx`, `colValueLabel`,
    `colValues` (i valori con i conteggi), e il filtro `ColFilter` (`colMatchInit`/`colMatches`,
    applicato da `filterInto` in `app.cpp`);
  - i gruppi: `groupsBuild` (`columns.cpp`, la tabella `A.groups` e la vista riordinata) e la
    geometria in `app.cpp`: `groupsPlace`, `itemAlong`, `itemsInSpan` (le voci tra due posizioni
    lungo l'asse, per disegno e rettangolo), `groupHeadAt`, `lineStep`/`pageStep` (tastiera per
    linee attraverso i gruppi); `viewUsesDirTotals` se la vista dipende dai totali delle cartelle;
  - il menu con le voci a spunta che non lo chiudono: `MenuItem::keep`;
  - le voci di una lista, la principale e (nel suo `PaneScope`) l'anteprima di una cartella:
    `drawListItems`, `layoutItems` (`ListPane`, `paneSwap` in `app.h`);
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
- **Modifiche ai file con lo strumento Edit**, anche quando sono tante nello stesso file. Gli
  script (Python, `sed -i`) solo per un cambio meccanico uguale su molti file: nella 1.0.0-beta.6
  gli script di sostituzione hanno dato 6 errori (heredoc non chiuso, testo non trovato, codifica
  della console), Edit 2. Nella 1.0.0-beta.7 la regola è stata ignorata 34 volte in una sessione
  (con i backslash dei `.md` rovinati): ora `.claude/hooks/regole.sh` ferma il Python in linea
  (`python -c`, `python - <<`) che scrive file di `src/` o `.md`; un cambio meccanico su molti
  file si fa con uno script in un file dello scratchpad, con un `assert` per ogni sostituzione.

## Debiti tecnici noti (da sistemare quando si tocca quel codice)

- **Numeri sparsi:** migrati a `theme.h` (colori, misure, segnaposto delle icone `PH_*`). Restano
  qualche `+ 1`/`- 2` di 1 px per le linee, che DESIGN.md ammette, e `ROW_EXTRA` arrotondato con
  `lroundf` invece di `px()`, eccezione voluta e scritta in `theme.h` e DESIGN.md.
- **Vsync:** sul PC di Zyruel a volte il log mostra "attesa vsync lunga" (40–110 ms). Ora non blocca
  più l'input, ma la causa non è dimostrata: forse la cattura dello schermo da remoto, forse le
  transizioni di Windows.

## Prossimi passi (concordati, in quest'ordine)

**Colonne (1.0.0-beta.9): fatte tutte e tre le fasi** (colonne a scelta ed estensioni nascoste,
filtro per colonna, gruppi), da committare quando Zyruel lo chiede. Restano le prove a mano di
`prove\da-verificare.md` (175%, gruppi per Dimensione mentre si contano le cartelle) e le scelte
minori dei gruppi da confermare in `docs/da-sistemare.md`. Il prossimo passo si sceglie con
Zyruel da `docs/idee.md`.

Le altre idee non ancora decise sono in `docs/idee.md`, e quando Zyruel ne sceglie una si scrive qui. Cosa c'è in ogni versione è in `NOVITA.md`. Il monitor di
Zyruel è a 175%.

**Tre elenchi da tenere aggiornati** (si scrive lì appena viene fuori qualcosa, non a memoria):
- `prove\da-verificare.md`: le prove a mano che restano, per sezione;
- `docs/idee.md`: le idee venute in mente e non ancora decise (per esempio modificare gli zip, 7z e rar),
  con cosa servirebbe e cosa le frena rispetto ai principi;
- `docs/da-sistemare.md`: le piccole correzioni e le scelte minori da confermare con Zyruel.

Prima di cominciare una funzione: rileggere `docs/DESIGN.md` e la parte di `docs/ARCHITETTURA.md` che la
riguarda.

**Una funzione grande in più sessioni.** Con un piano di 3 o più fasi, dopo ogni fase compilata
senza avvisi si propone una sessione nuova (contesto "fresh", nella stessa cartella, senza
worktree) con il percorso del piano, le fasi fatte e i file toccati: il lavoro non committato resta
nella cartella. Le prove a schermo di una funzione finita si possono passare allo stesso modo, con
la sua sezione di `prove\da-verificare.md` come brief. Nella 1.0.0-beta.7 una funzione in 4 fasi
fatta tutta in una sessione ha portato il contesto da 80k a 711k token: metà del costo del ciclo.
Una domanda di Zyruel sul modo di lavorare (sessioni, fasi, ordine) ha sempre una risposta scritta
prima di ripresentare il piano.

Dopo:
- misurare e controllare il log (`/prova`);
- aggiornare `docs/ARCHITETTURA.md` (funzione, misure, tabella dei moduli), `NOVITA.md` (sezione della
  versione in corso) e `docs/DESIGN.md`; il README solo se cambia qualcosa per chi usa [z]f (elenco
  "Cosa sa fare", scorciatoie), restando breve;
- zero avvisi con MSVC;
- `/controllo` (skill in `.claude/skills/controllo`) sui file cambiati: le regole di questo file e
  di `docs/DESIGN.md`, controllate sul codice, con un resoconto e senza correzioni automatiche
  (prima di un rilascio, o quando serve, `/controllo tutto`). Si fa sul codice che va nel commit:
  se dopo il controllo si cambia ancora codice (una richiesta in più di Zyruel), si ricontrollano
  i file toccati dopo, prima del commit.
