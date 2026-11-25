---
name: prova
description: Come si prova [z]f sul PC: compilazione MSVC senza avvisi (o zig, o incrociata da Linux), finestra di prova separata con prove\dev.bat, gli altri script di prove\, le regole del controllo dello schermo (Zyruel al PC, le sue finestre, cartella di prova, zoom prima di premere, chiusura per PID), la copia sul PC da una sessione cloud, il registro build\z-f-log.txt con le voci normali, le misure di avvio e dove scrivere le prove a mano che restano. Da usare prima di provare una modifica o misurare.
---

# /prova — provare [z]f sul PC

Le prove si fanno **su un PC vero, non a memoria.** Zyruel preferisce che le faccia Claude quando
può (con il controllo dello schermo); altrimenti gli si chiede uno screenshot. Il monitor di Zyruel
è a 175%: l'aspetto si giudica anche lì.

## 1. Compilare

- **Riferimento, MSVC:** `build.bat`, da PowerShell, nella cartella del progetto:
  ```
  cmd /c '.\build.bat'
  ```
  Trova Visual Studio con vswhere, precompila gli shader con fxc, compila l'icona (`src\z-f.rc`)
  con `rc` e scrive `build\z-f.exe`. `build.bat debug` fa la build con i simboli, `build.bat
  firma` il programma di firma dei rilasci.
- **Deve uscire senza avvisi.** Attenzione a C4244 (int → float), che zig non segnala. Il
  messaggio "vswhere.exe non è riconosciuto" di `vcvars64` non viene dal codice.
- **Gli eseguibili:**
  - `build\z-f.exe`: la build MSVC, quella che Zyruel usa ed è registrata come file manager
    predefinito. Se è aperta, il link non può riscriverla: **non si chiudono le sue finestre**;
    si compila nella build di prova (`build\z-f-dev.exe`, con le stesse opzioni di `build.bat`
    e `/Fe:build\z-f-dev.exe`) o si chiede a lui.
  - `build\z-f-dev.exe`: la build di prova. Una build MSVC appena fatta si copia lì con Bash
    (`cp build/z-f.exe build/z-f-dev.exe`, verificata con `sha1sum`), con le finestre di prova
    chiuse.
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
    con `--new-window`;
  - qualsiasi opzione `--` apre una finestra separata.
- Le finestre avviate dall'app desktop di Claude vedono una `%APPDATA%` virtualizzata (nella
  cartella del pacchetto dell'app): leggono e scrivono impostazioni, preferiti e schede lì, non in
  quelli veri di Zyruel.
- Si annota il **PID** della finestra di prova appena avviata (per esempio
  `tasklist //FI "IMAGENAME eq z-f-dev.exe"` prima e dopo l'avvio): serve per chiuderla.
- **Chiudere:** per PID (`taskkill //PID <pid>`) o per cartella, **mai per nome**:
  `taskkill /IM z-f.exe` chiuderebbe anche le finestre di Zyruel.

## 3. Le altre prove in `prove\`

- `istanza.bat`: istanza unica e `/select`;
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
  `ime-avvio.cpp` (come si compilano è scritto in cima a ognuno).

## 4. Controllo dello schermo: le regole

- **Rispetto per Zyruel:**
  - se Zyruel sta usando il PC (con un'app a schermo intero, per esempio), **non si tocca
    schermo né tastiera**;
  - non si interferisce con le sue finestre di [z]f: per le prove si usa z-f-dev in una finestra
    separata.
- **Non scrivere nel registro** al posto di Zyruel: l'interruttore "Usa [z]f al posto di
  Explorer" lo usa lui.
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
  "al ritorno" (`filedlg.cpp`) lo prova Zyruel.

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
