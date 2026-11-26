---
name: prova
description: Come si prova [z]f sul PC: compilazione MSVC senza avvisi da Bash (build.bat dev per la build di prova, build.bat prova per i programmi di prove\; o zig, o incrociata da Linux), finestra di prova separata con prove\dev.bat, gli altri script di prove\, le regole del controllo dello schermo (Zyruel al PC, le sue finestre, cartella di prova, zoom prima di premere, chiusura per PID), la copia sul PC da una sessione cloud, il registro build\z-f-log.txt con le voci normali, le misure di avvio e dove scrivere le prove a mano che restano. Da usare prima di provare una modifica o misurare.
---

# /prova — provare [z]f sul PC

Le prove si fanno **su un PC vero, non a memoria.** Zyruel preferisce che le faccia Claude quando
può (con il controllo dello schermo); altrimenti gli si chiede uno screenshot. Il monitor di Zyruel
è a 175%: l'aspetto si giudica anche lì.

## 1. Compilare

- **Riferimento, MSVC:** `build.bat`, **da Bash**, nella cartella del progetto (la cartella
  corrente di Bash):
  ```
  cmd //c '.\build.bat dev'      # la build di prova: build\z-f-dev.exe, con ZF_DEV
  cmd //c '.\build.bat'          # la build di Zyruel: build\z-f.exe
  ```
  - **Mai da PowerShell:** lo strumento PowerShell non riesce a entrare in una cartella con le
    parentesi quadre e resta in `C:\Windows\System32`, dove `build.bat` non c'è (29 errori nelle
    sessioni della 1.0.0-beta.4).
  - **Con `.\`:** la cartella corrente non è nella ricerca dei comandi, quindi `build.bat` da solo
    "non è riconosciuto".
  - `build.bat` trova Visual Studio con vswhere, precompila gli shader con fxc, compila l'icona
    (`src\z-f.rc`) con `rc`. `build.bat debug` fa la build con i simboli (in `build\z-f.exe`),
    `build.bat firma` il programma di firma dei rilasci.
  - **I programmi di `prove\` e `strumenti\`:**
    `cmd //c '.\build.bat prova prove\x.cpp <librerie>'`, con le opzioni di tutti e l'exe in
    `build\`. Il comando esatto è in cima a ogni file. Non si scrivono script con il percorso di
    Visual Studio.
- **Deve uscire senza avvisi.** Attenzione a C4244 (int → float), che zig non segnala. L'uscita
  normale è solo `z-f.cpp`, una riga vuota e `Fatto: …`.
- **Gli eseguibili:**
  - `build\z-f.exe`: la build MSVC, quella che Zyruel usa ed è registrata come file manager
    predefinito. Se è aperta, il link non può riscriverla: **non si chiudono le sue finestre**.
    Per le prove basta `build.bat dev`; per riscriverla si chiede a lui.
  - `build\z-f-dev.exe`: la build di prova, da `build.bat dev`, con le finestre di prova chiuse
    (per PID). Non si aggiorna da sola (`ZF_DEV`).
- **Alternativa senza Visual Studio:** `build_zig.bat` (`winget install zig.zig`), che scrive
  `build\z-f-dev.exe` con `ZF_DEV` e non tocca `build\z-f.exe`.
- **Compilazione incrociata da Linux** (sessione cloud), equivalente:
  ```
  python3 -m ziglang c++ -target x86_64-windows-gnu -O2 -std=c++17 -municode -Wall -Wextra \
    -Wno-unused-parameter -Wno-date-time -DUNICODE -D_UNICODE -DZF_DEV src/z-f.cpp src/z-f.rc -o build/z-f-dev.exe \
    -Wl,--subsystem,windows -luser32 -lgdi32 -ld3d11 -lshell32 -lole32 -luuid -ladvapi32 -lwinhttp -lbcrypt
  ```
  Anche qui: zero avvisi. `ZF_DEV` (anche in `build_zig.bat`): la build di prova non si aggiorna
  da sola. `src/z-f.rc` (l'icona) lo compila zig stesso, dalla 0.12; MSVC con `rc` in `build.bat`.
- **Percorsi con `[z]f`:** PowerShell non gestisce le parentesi quadre nei percorsi (Copy-Item,
  Get-FileHash): per copiare e verificare gli exe si usa Bash (`cp`, `sha1sum`). Dalla shell Bash
  i backslash nei testi passati a script (heredoc, sed) possono dimezzarsi o diventare caratteri
  di controllo: per scrivere percorsi nei file .md si usa lo strumento Edit.

## 2. Una finestra di prova separata

- **Istanza unica:** se [z]f è già aperto, avviare un altro [z]f consegna la cartella alla
  finestra esistente ed esce. Quindi:
  - per una finestra di prova separata: `prove\dev.bat [cartella]`, che avvia `build\z-f-dev.exe`
    con `--new-window`; da Bash `cmd //c '.\prove\dev.bat <cartella>'`;
  - qualsiasi opzione `--` apre una finestra separata.
- Le finestre avviate dall'app desktop di Claude vedono una `%APPDATA%` virtualizzata (nella
  cartella del pacchetto dell'app): leggono e scrivono impostazioni, preferiti e schede lì, non in
  quelli veri di Zyruel.
- Si annota il **PID** della finestra di prova appena avviata (per esempio
  `tasklist //FI "IMAGENAME eq z-f-dev.exe"` prima e dopo l'avvio): serve per chiuderla.
- **Chiudere:** per PID (`taskkill //PID <pid>`, `Stop-Process -Id <pid>`) o per cartella,
  **mai per nome**: `taskkill /IM z-f.exe` chiuderebbe anche le finestre di Zyruel, e
  `z-f-dev.exe` quelle di prova di un'altra sessione. Lo ferma `.claude/hooks/regole.sh`.

## 3. Le altre prove in `prove\`

- `istanza.bat`: istanza unica e `/select`;
- `memoria.ps1 -Ids <pid>[,<pid>…]`: working set, memoria privata, memoria GPU, thread e handle
  di ogni PID (da Bash: `powershell -NoProfile -File 'prove\memoria.ps1' -Ids <pid>`);
- `avvio.bat [volte] [exe]`: tempi di avvio con `--prova-avvio` (nel registro "prova avvio:
  contenuto" e "prova avvio: primo fuoco"; predefiniti 10 volte e `build\z-f-dev.exe`). Ogni
  avvio porta una finestra in primo piano: **solo a PC libero**;
- `predefinito.bat`: [z]f al posto di Explorer (apre cartelle come un altro programma e simula
  Win+E); `ripristina-explorer.bat` è l'emergenza che rimette Explorer;
- `hw.bat`, `warp.bat`, `ibrido.bat`: GPU (WinSxS con `--gpu=hw`, `--gpu=warp`, ibrido);
  `msvc.bat` apre WinSxS con `build\z-f.exe`; `home.bat`, `desktop.bat`, `system32.bat` aprono
  z-f-dev lì;
- `aggiornamenti-prova.py <cartella>`: un server locale che fa da GitHub, per provare
  l'aggiornamento con `--aggiorna-da=http://127.0.0.1:8766` su una **copia** dell'exe in una
  cartella di prova (l'exe viene sostituito);
- cartelle di prova con file finti: `griglia-prova.py` (griglia e miniature),
  `anteprima-prova.py` (anteprima);
- misure e confronti a parte, senza interfaccia: `zip-prova.cpp`, `collegamenti-prova.cpp`,
  `dialoghi-prova.cpp`, `shell-verbi.cpp`, `vai-a-misura.cpp`, `costi-frame.cpp`,
  `ime-avvio.cpp` (come si compilano è scritto in cima a ognuno);
- `online-prova.cpp`: file solo online senza OneDrive (una radice di sincronizzazione finta con
  l'API Cloud Files di Windows). `passi` misura le chiamate di [z]f su segnaposto nuovi;
  `attendi <secondi>` resta in ascolto mentre si prova z-f-dev sulla cartella con
  `prove\dev.bat`, e alla fine dice quanti file ha scaricato ogni processo.
- `appunti-prova.cpp`: gli appunti occupati da un altro programma. `occupa <secondi>
  [senza-finestra]` li tiene aperti, `fermo <secondi>` li possiede e non risponde ai messaggi
  (chi li svuota lo aspetta), `chi` dice proprietario e testo; si prova intanto Copia percorso o
  Taglia in una casella di z-f-dev.

## 4. Controllo dello schermo: le regole

- **Rispetto per Zyruel:**
  - se Zyruel sta usando il PC (con un'app a schermo intero, per esempio), **non si tocca
    schermo né tastiera**;
  - non si interferisce con le sue finestre di [z]f: per le prove si usa z-f-dev in una finestra
    separata.
- **Non scrivere nel registro** al posto di Zyruel: l'interruttore "Usa [z]f al posto di
  Explorer" lo usa lui (anche questo lo ferma `.claude/hooks/regole.sh`).
- **Il permesso (`request_access`):** prima si avvia la finestra di prova (`prove\dev.bat`), poi
  si chiede `"z-f-dev.exe"` o il percorso completo dell'exe. `"z-f-dev"` e `"[z]f"` non sono
  riconosciuti, e nemmeno l'exe se non è in esecuzione (39 richieste fallite nelle sessioni della
  1.0.0-beta.4). Lo stesso per i programmi di prova (`"dialoghi-prova.exe"`, avviato prima).
- **Prima dei tasti, la finestra davanti:** un clic su z-f-dev (al centro della barra del titolo,
  lontano dai pulsanti) all'inizio di ogni batch che preme tasti. Altrimenti i tasti finiscono
  nel desktop o nella finestra di Claude e il batch si ferma (44 volte). Per riportarla davanti
  si clicca, non si usa `open_application`.
- **Trascinamenti nel batch:** fra `left_mouse_down` e `left_mouse_up` solo movimenti e attese,
  niente tasti (il batch viene rifiutato). Una breve `wait` prima del primo movimento evita "could
  not be verified this soon".
- **Prove col mouse nella lista:** si fanno in una cartella di prova con file finti, mai in
  cartelle vere. Il trascinamento dei file parte da icona e nome di una voce selezionata, e in
  una cartella con nomi lunghi il nome occupa tutta la colonna: un punto che sembra vuoto può
  avviare uno spostamento vero (è successo in WinSxS, annullato). **Prima di premere, uno zoom
  sul punto.**
- Attenzione a dove si clicca nella barra del titolo di [z]f: a destra ci sono riduci,
  ingrandisci e chiudi.
- Non usare `open_application` per un programma di confronto già aperto, perché ne apre una
  finestra nuova: per portarlo davanti basta cliccarlo.
- Lo strumento che controlla lo schermo non lascia cliccare nei dialoghi Apri/Salva: il salto
  "al ritorno" (`filedlg.cpp`) con un dialogo vero lo prova Zyruel. Senza mouse né tastiera lo
  simula `dialoghi-prova ritorno <pid di z-f-dev> <A> <B> [ms] [ms in [z]f]`, che funziona anche a
  PC bloccato (porta davanti le finestre e legge il primo piano dalla notifica).

## 5. Sessione cloud con il PC collegato

- **Copia sul PC:**
  - si copia con `device_commit_files` e si verifica sempre con `sha1sum`;
  - la copia appena scritta in `outputs` a volte non è ancora pronta: se lo sha1 non torna, si
    ripete il commit;
  - un exe in esecuzione è bloccato: va chiuso prima di sovrascriverlo (la finestra di prova, per
    PID; mai quelle di Zyruel).
- Valgono le stesse regole del controllo dello schermo (sezione 4).

## 6. Dopo ogni prova: il registro

Si legge `build\z-f-log.txt` (rallentamenti con l'ora, accanto all'exe). Voci **normali**, da non
segnalare:

- all'avvio, due voci dello stesso evento, il passaggio WARP → GPU: `primo Present GPU`
  (15–30 ms, il driver prepara la swapchain nuova; fino a novembre 2026 si chiamava
  `Present lento`) e `messaggio lento 0x8002`, che lo contiene (circa 30 ms);
- può comparire `messaggio lento 0x0006`, con dentro `0x0281`: è il primo fuoco della finestra,
  in cui Windows avvia il Text Input Framework (di solito 12 ms, a volte 25–80;
  `prove\ime-avvio.cpp`). Dopo la 0.25 succede dopo il primo frame (F12, "attivazione"), quindi
  non ritarda più il contenuto (`prove\da-verificare.md` sezione 00000000000000);
- `attesa vsync lunga` (40–110 ms) è un debito tecnico noto (CLAUDE.md): si segnala solo se
  cambia.

Tutto il resto si legge e si spiega.

## 7. Misurare

- Riferimento (CLAUDE.md, principio 2): contenuto visibile a 64–80 ms dall'avvio, frame sotto
  1 ms. I tempi li danno la barra di stato, il pannello F12 e il registro.
- **Avvio:** `prove\avvio.bat [volte] [exe]`, solo a PC libero; per un confronto, giri alternati
  della build di prima e di quella nuova, e si confrontano le mediane di "prova avvio: contenuto".
- Un peggioramento si discute con Zyruel prima di tenerlo.

## 8. Cosa resta

Le prove a mano che restano (perché servono Zyruel, un dialogo Apri/Salva, una rete o un disco
che non c'è) si scrivono subito in `prove\da-verificare.md`, nella sezione della funzione, con
cosa è già stato provato e da chi. Le misure vanno in `docs/ARCHITETTURA.md` ("Misurato: …").
