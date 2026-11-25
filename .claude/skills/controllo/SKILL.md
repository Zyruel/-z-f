---
name: controllo
description: Controlla che il codice di [z]f segua le regole di CLAUDE.md e docs/DESIGN.md (thread UI, PostMessage e generazione, leggerezza, misure, notice, theme.h e px(), componenti da riusare, foRun, saveDataAsync, kShortcuts, commenti, repository pubblico) e scrive un resoconto per gravità, senza toccare il codice. Da usare dopo una funzione nuova, prima di un rilascio, o quando Zyruel chiede un controllo delle regole. Argomento: moduli (thumbs, src/zip.cpp…), "cambiati" (predefinito) o "tutto".
---

# /controllo — le regole di [z]f, controllate sul codice

Il controllo **legge e riferisce, non corregge**: nessuna modifica al codice né ai documenti. Le
correzioni le decide Zyruel; i punti minori, se lui vuole, vanno poi in `docs/da-sistemare.md`.

Prima di cominciare, rileggere per intero `CLAUDE.md` e `docs/DESIGN.md`: questa lista viene da
lì, e se i due documenti sono cambiati valgono loro (e la lista va aggiornata).

## 1. Ambito (l'argomento)

- **moduli**: uno o più nomi (`thumbs`, `src/zip.cpp`, `app.h`); senza estensione vale
  `src/<nome>.cpp` o `.h`. Si leggono **per intero**, insieme a quello che serve per giudicarli
  (le funzioni chiamate da altri moduli, i gestori dei loro `WM_APP_*`).
- **`cambiati`** (anche senza argomento): i file diversi da `origin/main`, compresi quelli non
  ancora committati:
  ```
  git diff --name-only origin/main
  git ls-files --others --exclude-standard
  ```
  Si guardano le righe cambiate (`git diff origin/main -- <file>`), ma si giudicano leggendo la
  funzione intera in cui stanno. Se sono cambiati i documenti, si controlla anche la loro coerenza
  col codice (sezione 3, *documenti*).
- **`tutto`**: tutto `src/`, `shader.hlsl` e i documenti, con quattro subagenti (sezione 2).

Con pochi file (fino a 3–4 moduli) il controllo lo fa la sessione stessa; con di più si dividono
i file in fette come per `tutto`.

## 2. Con `tutto`: quattro subagenti in parallelo

Fette (ogni nome è `src/<nome>.cpp`, salvo dove c'è l'estensione):

- **A**: `main`, `app`, `app.h`, `base.h`, `theme.h`, `gfx`, `render`, `text`, `chrome`,
  `hybrid`, `shader.hlsl`, `settings`, `integration`;
- **B**: `input`, `listmouse`, `menu`, `options`, `goto`, `tooltip`, `sidebar`, `crumbs`, `tabs`;
- **C**: `listing`, `watch`, `icons`, `thumbs`, `preview`, `dirsize`, `links`, `lnk`;
- **D**: `fileops`, `shell`, `drag`, `zip`, `zipout`, `filedlg`, `update`, più la coerenza dei
  documenti (sezione 3, *documenti*).

**Ricalcolare le fette** se in `src/z-f.cpp` (l'elenco dei moduli, in ordine di dipendenza) c'è
un modulo che qui manca o ne manca uno che qui c'è: ogni modulo va in una fetta, tenendo vicini
quelli che lavorano insieme e le fette di dimensioni simili (`wc -l src/*`). Se le fette sono
state cambiate, dirlo nel resoconto.

I subagenti sono **in sola lettura**: un tipo di agente senza strumenti di scrittura se c'è (per
esempio `Plan`), altrimenti `general-purpose` con l'ordine esplicito di non modificare niente.
Partono tutti nello stesso messaggio. Il brief di ognuno, in italiano:

> Controlli, in sola lettura, che questi file di [z]f seguano le regole del progetto: <fetta>.
> Non modificare nessun file. Leggi prima per intero `CLAUDE.md`, `docs/DESIGN.md` e la sezione
> 3 ("Lista di controllo") di `.claude/skills/controllo/SKILL.md`; poi leggi per intero ognuno
> dei tuoi file e applica la lista punto per punto (usa grep per seguire una funzione negli altri
> moduli quando serve a capire su quale thread gira). Le eccezioni scritte nella lista non sono
> violazioni. Per ogni segnalazione dai: `file:riga`, gruppo (pubblico, correttezza, leggerezza,
> velocità, mai interrompere, coerenza, pulizia), la regola (con il documento e la sezione), cosa
> non va in una o due frasi, uno scenario concreto per la correttezza, una proposta di correzione
> in una riga, e la certezza (sicuro / probabile / dubbio). Niente lodi, niente riassunti dei
> file: solo le segnalazioni, più l'elenco delle eccezioni scritte che hai incontrato.

## 3. Lista di controllo

Il **thread UI** è la procedura della finestra e tutto ciò che chiama: `onKey` e il mouse
(`input.cpp`, `listmouse.cpp`), `render()` e le funzioni `draw*`, i gestori dei messaggi
`WM_APP_*` (elenco in `base.h`), i timer, le notifiche di `SetWinEventHook` fuori contesto e i
metodi di `IDropTarget`. Un **thread** è una funzione passata a `CreateThread` (si chiamano
`…Thread`) con ciò che chiama. Se non è chiaro dove gira una funzione, si risalgono i chiamanti
con grep prima di segnalare.

### Repository pubblico (va sistemato prima di un push)

- Dati personali: il nome di battesimo di Zyruel o di altre persone, email non anonime, percorsi
  con un nome utente (negli esempi si usano `C:\Users\utente` e `\\server\…`), nomi di server,
  condivisioni e cartelle di lavoro.
- Nomi di altri file manager presi a confronto (Explorer ed Esplora file sono Windows: ammessi).
- File che non devono esserci: `.exe`, `.sig`, chiavi.
- Come si controlla: per i commit, l'hook del pre-push; con `tutto` su tutta la storia
  (`echo "refs/heads/dev $(git rev-parse HEAD) refs/heads/dev 0000000000000000000000000000000000000000" | sh .githooks/pre-push nessun-remoto`),
  con `cambiati` sui commit che `origin/dev` non ha (lo sha di `origin/dev` al posto degli zeri e
  `origin` come remoto). Per i file non ancora committati, le parole di
  `.git/info/parole-private.txt` cercate con `git grep -i -F -f` (senza le righe `#` e vuote).
  Le parole private non si scrivono mai in un file del repository, nemmeno nel resoconto in
  `build\`: si dice "parola privata" con `file:riga`.

### Correttezza (CLAUDE.md, principio 3; DESIGN §1.3, §10)

- **Disco, shell, rete o attese sul thread UI.** Da cercare: `CreateFile`, `GetFileAttributes*`,
  `FindFirstFile`, `ReadFile`/`WriteFile`, `CopyFile`/`MoveFile`/`DeleteFile`,
  `CreateDirectory`, `GetDiskFreeSpace*`, `GetVolumeInformation`, `SH*` e `IShellFolder`/
  `IShellItem` (anche `SHGetFileInfo`), `ShellExecute*`, `CreateProcess`, `CoCreateInstance` di
  oggetti della shell, `WNet*`, `WinHttp*`, gli appunti (`OpenClipboard`, `OleGetClipboard`:
  possono aspettare un altro processo), `SendMessage` verso finestre di altri processi,
  `WaitFor*`, `Sleep`, `Present` col vsync, `Flush`. Vale anche per le cose piccole: creare una
  cartella, avviare un processo, aprire o chiudere un handle su una cartella, scrivere il log. Un
  lock preso dal thread UI non si tiene mai durante una scrittura su disco.
- **Ciò che deve stare per forza sul thread UI** (OLE: l'oggetto della shell per Copia o per
  trascinare, `shellObjectFor`; `IDropTarget`) si misura con `hitchLog` ("oggetto della shell
  lento"), perché i cicli modali della shell sono esclusi dal registro dei messaggi lenti
  (`g_shellCalls`); il menu completo della shell e i comandi senza menu (Shift+Canc, Proprietà di
  più voci) passano dal thread del menu (`shellVerb` → `smSend` → `smRun`), non dal thread UI.
- **Lavoro su un thread senza `PostMessage` e contatore di generazione.** Il risultato arriva al
  thread UI con `PostMessage` (se fallisce, il risultato si libera: `if (!PostMessageW(…)) delete r;`)
  e il gestore lo scarta se la generazione non è più quella corrente (`gen`, `watchGen`, `sideGen`
  in `app.h`, `PvReq::superseded`). Va segnalato anche:
  - un thread che scrive direttamente nello stato dell'interfaccia (`A.…`), salvo un valore di
    misura atomico per F12 con il commento che lo dice;
  - due lavori dello stesso tipo che possono sovrapporsi sullo stesso oggetto senza che il
    secondo superi il primo (anche quando il thread non consegna niente, ma agisce su una
    finestra o un file).
- **Percorsi lenti:** rete e unità ottiche passano da `slowPath` (niente letture in background,
  dopo le locali); i file solo online da `onlineOnly` (non si leggono); il tipo di unità da
  `driveKind`, le radici di rete da `uncShareRoot`.
- **Zip:** niente shell sui percorsi dentro uno zip (zipfldr). Sul thread `zipSplit`/`zipInside`,
  sul thread UI `Listing::zipLen` (o `zipPathLike` solo dove sbagliare costa un'icona); un file
  si legge con `zipReadMem`; aprire, copiare, trascinare con `zipJobStart`.
- **Collegamenti:** `linkRead` e `netPlacesRead` solo su un thread; la destinazione già letta con
  `linkOf`.
- **Zero CPU a riposo** (DESIGN §1.4): si disegna solo con `A.dirty` o con `A.animActive` mentre
  qualcosa si muove; niente timer o animazioni che restano accesi senza motivo.

### Leggerezza (CLAUDE.md, principio 1; DESIGN §1.1)

- Una libreria nuova in `build.bat`/`build_zig.bat`, o un `LoadLibrary` nuovo, va giustificata.
- shell32, ole32, winhttp e bcrypt sono a caricamento ritardato (`/DELAYLOAD` in `build.bat`); mpr
  si carica al primo uso (`mprProc`, `sidebar.cpp`). Una chiamata a una loro funzione **prima del
  primo frame** (in `main.cpp` o in ciò che l'avvio chiama) le carica all'avvio: è da segnalare.
- Niente processi in più all'avvio; ciò che non serve al primo frame parte dopo, nel blocco
  "dopo il primo frame" di `main.cpp` (barra laterale, `dropRegister`, dimensioni delle cartelle,
  collegamenti, `fdStart`, `updStartup`). Una cosa nuova messa prima di lì va giustificata con
  la misura dell'avvio (`prove\avvio.bat`).

### Velocità (CLAUDE.md, principio 2; DESIGN §1.2, §10)

- Una funzione nuova ha la sua misura: una riga in F12 (`render.cpp`), un `hitchLog` con la soglia
  `LOG_*` di `theme.h` per i casi lenti, o la barra dei tempi; e in `docs/ARCHITETTURA.md` la riga
  "Misurato: …". Con `cambiati`: se c'è codice nuovo senza misura o senza la riga in
  ARCHITETTURA, si segnala.
- Lavoro ripetuto per ogni frame o per ogni voce che potrebbe stare in una cache (testo misurato,
  stringhe ricostruite, ricerche lineari su elenchi grandi).

### Mai interrompere (CLAUDE.md, principio 6; DESIGN §1.7)

- Niente dialoghi nostri (`MessageBox`, `TaskDialog`, `DialogBox`, finestre di conferma): si agisce
  e si avvisa con `notice()`. Le conferme ammesse sono quelle della shell (eliminazione
  definitiva) e le finestre di Windows chieste dall'utente (Connetti unità di rete, Proprietà).
- Ogni azione senza un effetto visibile immediato dà un avviso (DESIGN §7, *Avvisi*); gli errori
  dicono cosa e perché (§9).

### Coerenza (CLAUDE.md, principio 5; DESIGN §2–§10)

- **Numeri visivi fuori da `theme.h`/`px()`:** colori (`0xFF…`) fuori da `theme.h`, misure e
  raggi scritti come numeri, costanti `ui::` usate senza `px()`. Anche i **tempi**: durate delle
  animazioni, ritardi, timer (`SetTimer` con un numero), soglie del log e tempi del lavoro in
  background (DESIGN §10: come `WATCH_BATCH`, `SIDE_RELOAD`, `ZIP_TEMP_KEEP`) stanno in
  `theme.h`, non in costanti locali del modulo. Eccezioni scritte: i `+ 1`/`- 1`/`- 2` di 1 px per
  le linee, `ROW_EXTRA` arrotondato con `lroundf`, le metà per centrare (`* 0.5f`, `/ 2`), la
  scala stessa (`A.scale`). Costanti che non sono visive né tempi (id dei controlli di Windows,
  dimensioni di buffer) restano dove servono.
- **Animazioni** solo con `approach`, `hoverFadeUpdate`/`hoverAlpha`, `easeOut`, `fadeA`, con le
  costanti `ui::TAU_*` e `ui::DUR_*`, sotto i 150 ms; ogni animazione imposta `A.animActive`
  finché si muove; con `animazioni=0` arriva subito all'obiettivo.
- **Pannelli sovrapposti** senza `panelEnter` (ingresso) e `drawPanel` (ombra, fondo, contorno).
- **Voci della lista** (posizione, fondo, voce sotto il mouse) calcolate con `rowH` invece che con
  `itemRect`/`itemBg`/`itemAt`/`gridParts` (`app.cpp`). `A.rowH` in sé è legittimo (altezze di
  barre, menu, barra laterale): si segnala solo dove calcola una voce della lista, perché in
  griglia sbaglierebbe.
- **`drawSpinner`** solo mentre un lavoro è davvero in corso e dove l'utente sta guardando
  (DESIGN §5).
- Stati, colori e testi come DESIGN §2, §6 e §9: gerarchia del testo col colore e mai con
  dimensione o grassetto; un solo carattere (eccezione: il testo nell'anteprima); testi in
  italiano con la sola prima lettera maiuscola, " · " tra i valori, numeri con la virgola
  decimale; ogni elemento interattivo ha il tooltip con la scorciatoia presa da `kShortcuts`
  (`shortcutsOf`).
- Un elemento visibile nuovo è descritto in DESIGN §7.

### Pulizia (CLAUDE.md, principio 4 e "Convenzioni del codice")

- **Codice duplicato o componenti rifatti** invece di riusati. L'elenco è in CLAUDE.md
  ("Componenti da riusare"): primitive (`rect`, `rrect`, `drawText`, `drawUiIcon`, `drawIcon`),
  `panelEnter`/`drawPanel`, `menuAdd`/`menuPlace`, `drawPanelItem`/`drawPanelScroll`,
  `drawTextMarked`, `readSubfolders`, `tipRow`, `readInfo`/`fileRows`/`driveRows`,
  `pvImage`/`pvText`/`pvBigIcon`, `itemRect`/`itemBg`/`itemAt`/`gridParts`,
  `TextField`/`drawField`, `slowPath`/`onlineOnly`/`driveKind`/`uncShareRoot`,
  `clipSetFiles`/`hdropPaths`, `zipSplit`/`zipInside`/`zipReadMem`/`zipJobStart`,
  `linkRead`/`linkOf`/`linkReveal`, `netPlacesRead`, `parentOf`, `notice`, `drawSpinner`,
  `tabsSerialize`/`tabsRestore`, `updCheck`/`updAction`/`drawUpdateIcon`/`updRestartAll`/
  `restartLaunch`, la cornice (`chrome.cpp`). Anche due funzioni nostre quasi uguali in moduli
  diversi.
- **Operazioni sui file** (copia, sposta, Cestino) fuori da `foRun` e senza `undoPush`; rinomina
  e nuova cartella fuori da `fsStart`.
- **File di [z]f** in `%APPDATA%\z-f` scritti fuori da `saveDataAsync` (file temporaneo, poi
  rinominato).
- **Scorciatoie** scritte a mano in `onKey` invece che in `kShortcuts` (`options.cpp`): a mano
  restano solo i tasti che dipendono dal contesto (frecce, Esc, Backspace, Ctrl+V, Ctrl+1…9…).
  Una voce di menu con scorciatoia si collega in `kShortcuts` (`shortcutFor`), non con il testo
  del tasto scritto a mano.
- **Commenti** non in italiano, o che dicono il cosa invece del perché; commenti rimasti veri solo
  per il codice di prima.
- **`shader.hlsl`** solo ASCII: `grep -nP '[^\x00-\x7F]' src/shader.hlsl`.
- Codice morto: funzioni, campi o costanti di `theme.h` che nessuno usa più.

### Documenti (con `tutto`, fetta D; con `cambiati`, se i documenti sono cambiati)

- Ogni componente e ogni funzione nominati in CLAUDE.md, nella sezione 10 di DESIGN.md e in
  questa lista esiste davvero (grep nel codice).
- `kShortcuts` contro la tabella "Scorciatoie" del README: tasti mancanti, in più o descritti in
  modo diverso.
- Ogni modulo di `src/z-f.cpp` è nella tabella dei moduli di `docs/ARCHITETTURA.md`, e viceversa.
- I valori citati in DESIGN.md (`PAD_X` 12, `R_M` 8…) coincidono con `theme.h`.
- Le opzioni di `impostazioni.txt` elencate in ARCHITETTURA coincidono con quelle lette in
  `settings.cpp`.

### Eccezioni scritte (non sono violazioni)

- Il `MessageBox` se Direct3D non parte all'avvio (`main.cpp`; DESIGN §1.7).
- `dwmFlush` nel passaggio WARP → GPU, una volta; l'attesa della GPU con F12 aperto (DESIGN §10).
- L'avvio prima del primo frame (`main.cpp`): la consegna della cartella a un [z]f già aperto,
  l'attesa del thread del driver con "solo GPU", la breve attesa delle prime letture.
- I debiti tecnici noti di CLAUDE.md (l'attesa vsync lunga, i ritocchi da 1 px): non si
  segnalano come nuovi, solo se il codice li peggiora.

Un'eccezione incontrata si cita in fondo al resoconto, in una riga, senza contarla.

## 4. Verifica

Ogni segnalazione (dei subagenti o propria) **si controlla leggendo il codice** a quel
`file:riga`, con le funzioni intorno, prima di riferirla:

- il thread è davvero quello UI? la chiamata tocca davvero il disco o aspetta? il numero è
  davvero visivo o un tempo? il componente rifatto esiste davvero e farebbe lo stesso lavoro?
- se è un'eccezione scritta, o il codice fa già la cosa giusta poco più in là, è un falso
  allarme: si scarta.

Si dice quanti falsi allarmi sono stati scartati (e, in una riga ciascuno, perché, se sono pochi).
Ciò che resta incerto dopo la lettura si riferisce con "da verificare" e il motivo.

## 5. Resoconto

Solo il resoconto: **nessuna modifica**. Ordinato per gravità:

1. Repository pubblico (prima di un push)
2. Correttezza
3. Leggerezza e velocità
4. Mai interrompere
5. Coerenza
6. Pulizia (con i documenti)

Forma:

```
# Controllo delle regole — <ambito>, <aaaa-mm-gg>, commit <sha breve>

<N> file, <M> righe. Segnalazioni: <X> (falsi allarmi scartati: <Y>).

## Correttezza
- `src/modulo.cpp:123` — cosa non va, in una o due frasi (scenario concreto).
  Regola: CLAUDE.md principio 3 / DESIGN §10. Proposta: correzione in una riga.
...

## Eccezioni scritte incontrate
- `src/main.cpp:265` — MessageBox di Direct3D (DESIGN §1.7).
```

Un gruppo senza segnalazioni si scrive con "nessuna". In chat il resoconto si dà intero se è
breve; con `tutto` si scrive anche in `build\controllo-codice.md` (`build/` è escluso da git) e in
chat vanno i numeri e le segnalazioni più gravi, con il percorso del file.

Alla fine si chiede a Zyruel quali correzioni fare e se i punti minori vanno in
`docs/da-sistemare.md`. Non si corregge niente prima della sua risposta.
