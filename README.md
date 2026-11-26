# [z]f

File explorer per Windows, veloce e leggero: C++ con Win32 e Direct3D 11, senza framework e senza
dipendenze. L'obiettivo è che ogni operazione sembri istantanea. È gratuito e il codice è
libero, con [licenza MIT](LICENSE).

**Versione attuale: 1.0.0-beta.5.** Cosa cambia in ogni versione è in [`NOVITA.md`](NOVITA.md).

## Scaricare

L'ultima versione è nelle [Release](https://github.com/Zyruel/-z-f/releases): si scarica
`z-f-<versione>.exe` e si avvia, senza installazione (conviene rinominarlo `z-f.exe` e tenerlo in
una cartella propria: gli aggiornamenti lo sostituiscono lì). Impostazioni, preferiti e cartelle
recenti stanno in `%APPDATA%\z-f`, in file di testo modificabili anche a mano. Serve Windows a 64
bit (provato su Windows 11).

## Cosa sa fare

- **È veloce:** il contenuto compare circa 70 ms dopo l'avvio, un frame si disegna in meno di un
  millisecondo e, quando non succede niente, non usa CPU. Il disco si legge sempre in background,
  quindi l'interfaccia non si blocca mai.
- **Schede** e **barra laterale** con i preferiti (si riordinano trascinandoli) e Questo PC, con
  le unità e i percorsi di rete; si connettono e disconnettono unità di rete e si aggiungono
  percorsi di rete.
- **Percorso a segmenti** cliccabili, con le sottocartelle in un menu su ogni "›".
- **"Vai a"** (Ctrl+P): cerca mentre scrivi tra cartelle recenti, preferiti e le loro
  sottocartelle. Dal percorso (clic, Ctrl+L) parte dalla cartella aperta: si scrive il percorso
  con le sottocartelle suggerite.
- **Lista:** filtro mentre scrivi, ordinamento naturale, selezione multipla e col rettangolo,
  colonne ridimensionabili, tooltip con le informazioni di file, cartelle e unità.
- **Anteprima** (Alt+P) di immagini, testo e cartelle.
- **Viste con lo zoom** (Ctrl+rotellina): Dettagli, Elenco a colonne, Icone con le miniature da
  piccole a molto grandi; la vista si ricorda per cartella.
- **Operazioni sui file** con il motore di Explorer (avanzamento, conflitti, permessi), **annulla e
  ripeti** (Ctrl+Z, Ctrl+Y), trascinamento da e verso le altre app, rinomina nella riga.
- **Zip come cartelle**, in sola lettura: si sfogliano e se ne aprono, copiano e trascinano fuori
  i file.
- **Collegamenti (.lnk):** una colonna mostra dove porta ognuno, e un clic ci va.
- **Dimensione delle cartelle** nel tooltip, nell'anteprima e, con l'opzione, nella colonna.
- **Menu** essenziale e immediato; il menu completo di Windows con Shift+clic destro, o sempre con
  l'opzione "Menu completo di Windows col tasto destro".
- **Dialoghi Apri e Salva** degli altri programmi: entrano nella cartella aperta in [z]f.
- **Al posto di Explorer**, con l'opzione "Usa [z]f al posto di Explorer": le cartelle aperte da
  Windows, gli zip e Win+E aprono [z]f. Scrive solo nelle impostazioni dell'utente e si annulla con lo
  stesso interruttore.
- **Una finestra sola:** aprire una cartella con [z]f già aperto aggiunge una scheda (o, con
  l'opzione "Una finestra per ogni cartella aperta da fuori", apre una finestra nuova). Con
  l'opzione "Riapri le schede all'avvio" si riparte dalle schede che c'erano alla chiusura.
- **Si aggiorna da solo:** una volta al giorno guarda se c'è una versione nuova, la scarica, ne
  controlla la firma e la installa per il prossimo avvio. Lo stato è accanto alla versione, nel
  menu ≡: da lì "Aggiorna" riavvia subito [z]f con le stesse schede. Si spegne nelle opzioni.
- **Opzioni** (Ctrl+,) con effetto immediato.

## Scorciatoie

Le stesse che elencano le opzioni (Ctrl+,), dove si possono anche cercare.

### Navigazione

| Tasto | Azione |
|---|---|
| Invio · doppio clic | Apre la cartella o il file |
| Alt+↑ · Backspace | Cartella superiore (Backspace: se non c'è un filtro) |
| Alt+← · tasto Indietro del mouse o della tastiera · gesto Indietro del touchpad | Indietro |
| Alt+→ · tasto Avanti del mouse o della tastiera · gesto Avanti del touchpad | Avanti |
| Clic su un segmento del percorso | Va a quella cartella |
| Clic su › nel percorso | Sottocartelle |
| Clic sulla destinazione di un collegamento · clic centrale | Apre il percorso del file (in una nuova scheda) |
| Ctrl+L · F4 · clic sul percorso vuoto | Vai a dalla cartella aperta: scrive il percorso |
| Ctrl+P · Ctrl+G | Vai a: cartelle recenti, preferiti, sottocartelle |
| Ctrl+Invio · clic centrale | Vai a: apre in una nuova scheda |
| Tab | Vai a: entra nella cartella e mostra le sottocartelle |
| F5 · tasto Aggiorna della tastiera | Ricarica |

### Selezione

| Tasto | Azione |
|---|---|
| ↑ ↓ · PagSu PagGiù · Home Fine | Muove il cursore e seleziona |
| Shift+frecce | Estende la selezione |
| Ctrl+frecce | Muove solo il cursore |
| Ctrl+Spazio | Seleziona o deseleziona la voce del cursore |
| Ctrl+clic · Shift+clic · Ctrl+Shift+clic | Alterna · intervallo · aggiunge l'intervallo |
| Trascina nella lista · con Ctrl · con Shift | Seleziona col rettangolo · alterna · aggiunge |
| Clic sotto le righe | Toglie la selezione |
| Ctrl+A | Seleziona tutto ciò che è visibile |
| Lettere | Filtro sui nomi |
| Backspace | Toglie l'ultima lettera del filtro |
| Esc | Toglie il filtro, poi la selezione |

### File

| Tasto | Azione |
|---|---|
| Ctrl+C · Ctrl+X | Copia · taglia |
| Ctrl+V | Incolla i file; un percorso ci va, un testo filtra |
| Ctrl+Shift+C | Copia percorso |
| Ctrl+Shift+N | Nuova cartella |
| F2 | Rinomina |
| Canc · Shift+Canc | Cestino · elimina definitivamente |
| Ctrl+Z | Annulla l'ultima operazione sui file |
| Ctrl+Y · Ctrl+Shift+Z | Ripete l'operazione annullata |
| Alt+Invio | Proprietà |
| Tasto destro · Menu | Menu contestuale |
| Shift+tasto destro · Shift+F10 | Menu completo della shell (o di [z]f, con l'opzione) |
| Trascina una voce selezionata | Sposta sulla stessa unità, altrimenti copia |
| Ctrl+trascina · Shift+trascina · Alt+trascina | Copia · sposta · crea collegamento |
| Trascina col tasto destro | Chiede se copiare, spostare o collegare |

### Schede

| Tasto | Azione |
|---|---|
| Ctrl+T | Nuova scheda |
| Ctrl+W · clic centrale sulla scheda | Chiude la scheda |
| Ctrl+Tab · Ctrl+Shift+Tab | Scheda successiva · precedente |
| Ctrl+1…8 · Ctrl+9 | Scheda n · ultima scheda |
| Clic centrale · Ctrl+clic su un preferito | Apre in una nuova scheda in background |
| Trascina su una scheda | La apre dopo un momento |

### Vista

| Tasto | Azione |
|---|---|
| Ctrl+B | Mostra o nasconde la barra laterale |
| Alt+P | Mostra o nasconde l'anteprima |
| Ctrl+rotellina | Zoom della lista: Dettagli, Elenco, Icone da 20 a 100% |
| Ctrl+Shift+2 · Ctrl+Shift+3 · Ctrl+Shift+4 | Icone grandi · medie · piccole |
| Ctrl+Shift+5 · Ctrl+Shift+6 | Elenco · Dettagli |
| Trascina il bordo di un pannello · doppio clic | Larghezza di barra laterale o anteprima · predefinita |
| Clic sull'intestazione | Ordina per colonna (di nuovo: al contrario) |
| Trascina il bordo di una colonna · doppio clic | Larghezza della colonna · adatta al contenuto |
| Ctrl+D | Aggiunge o toglie dai preferiti |
| Trascina tra i preferiti | Aggiunge le cartelle ai preferiti |
| Trascina un preferito | Lo sposta tra gli altri (solo l'ordine) |
| Clic su Preferiti · sulla freccia di Questo PC | Chiude o riapre i preferiti · le unità |
| Ctrl+/− | Dimensione del testo |
| Ctrl+0 | Dimensione del testo predefinita |
| Ctrl+, | Opzioni |
| F12 | Statistiche e tempi |

## Compilare

```bat
build.bat          :: MSVC, consigliato  -> build\z-f.exe
build.bat debug    :: con simboli per il debugger
build.bat dev      :: build di prova, che non si aggiorna da sola  -> build\z-f-dev.exe
build_zig.bat      :: senza Visual Studio (winget install zig.zig)  -> build\z-f-dev.exe
```

Si avvia con `build\z-f.exe [cartella]`; senza argomento si apre la cartella utente. Come è fatto
il programma (moduli, rendering, misure, differenze tra MSVC e zig) è in
[`ARCHITETTURA.md`](docs/ARCHITETTURA.md); le regole dell'interfaccia sono in [`DESIGN.md`](docs/DESIGN.md).
