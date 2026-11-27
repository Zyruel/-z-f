# [z]f

File explorer per Windows, veloce e leggero: C++ con Win32 e Direct3D 11, senza framework e senza
dipendenze. L'obiettivo è che ogni operazione sembri istantanea. È gratuito e il codice è
libero, con [licenza MIT](LICENSE).

**Versione attuale: 1.0.0-beta.8.** Cosa cambia in ogni versione è in [`NOVITA.md`](NOVITA.md).

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
  tooltip con le informazioni di file, cartelle e unità.
- **Colonne** a scelta (tasto destro sull'intestazione): Tipo (l'estensione), Data modifica, Data
  creazione, Dimensione, Attributi; ridimensionabili e adattabili al contenuto, e filtrabili per
  valore con l'imbuto accanto al titolo (estensioni, periodi come Oggi e Ieri, fasce di dimensione, attributi).
  Le estensioni nei nomi si possono nascondere.
- **Anteprima** (Alt+P) di immagini, testo e cartelle, a destra o in basso (˅ accanto a ◨, Alt+Shift+P);
  una cartella nell'anteprima è una seconda lista, con la sua vista e il suo filtro, su cui si
  seleziona, si copia, si trascina e si rinomina.
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
- **Al posto di Explorer**, con l'opzione "[z]f al posto di Explorer": le cartelle aperte da
  Windows, gli zip e Win+E aprono [z]f. Scrive solo nelle impostazioni dell'utente e si annulla con lo
  stesso interruttore.
- **Una finestra sola:** aprire una cartella con [z]f già aperto aggiunge una scheda (o, con
  l'opzione "Una finestra per ogni cartella aperta da fuori", apre una finestra nuova). Con
  l'opzione "Schede riaperte all'avvio" si riparte dalle schede che c'erano alla chiusura.
- **Si aggiorna da solo:** una volta al giorno guarda se c'è una versione nuova, la scarica, ne
  controlla la firma e la installa per il prossimo avvio. Un pallino su ⚙ lo segnala, e in fondo
  alle opzioni "Aggiorna" riavvia subito [z]f con le stesse schede. Si spegne nelle opzioni.
- **Opzioni** (⚙ in alto a sinistra, Ctrl+,) con effetto immediato.

## Scorciatoie

Le stesse che elencano le opzioni (Ctrl+,), dove si possono anche cercare.

### Navigazione

| Tasto | Azione |
|---|---|
| Invio · doppio clic | Apre la cartella o il file |
| Alt+↑ · Backspace | Va alla cartella superiore (Backspace: se non c'è un filtro) |
| Alt+← · tasto Indietro del mouse o della tastiera · gesto Indietro del touchpad | Va indietro |
| Alt+→ · tasto Avanti del mouse o della tastiera · gesto Avanti del touchpad | Va avanti |
| Clic su un segmento del percorso | Va alla cartella di quel segmento |
| Clic su › nel percorso | Mostra le sottocartelle di quel segmento |
| Clic sulla destinazione di un collegamento · clic centrale | Apre il percorso del file (in una nuova scheda) |
| Ctrl+L · F4 · clic sul percorso vuoto | Apre Vai a col percorso della cartella aperta, da modificare |
| Ctrl+P · Ctrl+G | Apre Vai a: cartelle recenti, preferiti, sottocartelle |
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
| Ctrl+clic · Shift+clic · Ctrl+Shift+clic | Alterna la voce · seleziona l'intervallo · aggiunge l'intervallo |
| Trascina nella lista · con Ctrl · con Shift | Seleziona col rettangolo · alterna · aggiunge |
| Clic sotto le righe | Toglie la selezione |
| Ctrl+A | Seleziona tutto ciò che è visibile |
| Lettere | Filtra i nomi mentre si scrive |
| Backspace | Toglie l'ultima lettera del filtro |
| Esc | Toglie il filtro, poi la selezione (nell'anteprima, poi torna alla lista) |

### File

| Tasto | Azione |
|---|---|
| Ctrl+C · Ctrl+X | Copia · taglia |
| Ctrl+V | Incolla i file; un percorso ci va, un testo filtra |
| Ctrl+Shift+C | Copia il percorso |
| Ctrl+Shift+N | Crea una nuova cartella |
| F2 | Rinomina |
| Canc · Shift+Canc | Sposta nel Cestino · elimina definitivamente |
| Ctrl+Z | Annulla l'ultima operazione sui file |
| Ctrl+Y · Ctrl+Shift+Z | Ripete l'operazione annullata |
| Alt+Invio | Apre le proprietà |
| Tasto destro · Menu | Apre il menu contestuale |
| Shift+tasto destro · Shift+F10 | Apre il menu completo della shell (o quello di [z]f, con l'opzione) |
| Trascina una voce selezionata | Sposta sulla stessa unità, altrimenti copia |
| Ctrl+trascina · Shift+trascina · Alt+trascina | Copia · sposta · crea collegamento |
| Trascina col tasto destro | Chiede se copiare, spostare o collegare |

### Schede

| Tasto | Azione |
|---|---|
| Ctrl+T | Apre una nuova scheda |
| Ctrl+N | Apre una nuova finestra sulla stessa cartella |
| Ctrl+W · clic centrale sulla scheda | Chiude la scheda |
| Ctrl+Tab · Ctrl+Shift+Tab | Passa alla scheda successiva · precedente |
| Ctrl+1…8 · Ctrl+9 | Passa alla scheda n · all'ultima |
| Clic centrale · Ctrl+clic su un preferito | Apre in una nuova scheda in background |
| Trascina su una scheda | La apre dopo un momento |

### Vista

| Tasto | Azione |
|---|---|
| Ctrl+B | Mostra o nasconde la barra laterale |
| Alt+P | Mostra o nasconde l'anteprima |
| Alt+Shift+P | Sposta l'anteprima a destra o in basso |
| Tab · clic nel pannello | Passa la tastiera alla lista o all'anteprima di una cartella |
| Doppio clic nell'anteprima | Apre il file; una cartella in una nuova scheda in background |
| Ctrl+Shift+2 · Ctrl+Shift+3 | Passa alla vista Dettagli · Elenco |
| Ctrl+Shift+4 · Ctrl+Shift+5 · Ctrl+Shift+6 | Passa alle icone piccole · medie · grandi (zoom 40 · 70 · 100%) |
| Ctrl+rotellina | Cambia lo zoom della lista: Dettagli, Elenco, Icone da 20 a 100% (sull'anteprima: la sua vista) |
| Trascina il bordo di un pannello · doppio clic | Cambia la larghezza di barra laterale o anteprima (l'altezza, in basso) · torna alla predefinita |
| Clic su ˅ accanto a ◨ | Mette l'anteprima a destra o in basso |
| Clic sull'intestazione | Ordina per colonna (di nuovo: al contrario) |
| Tasto destro sull'intestazione | Sceglie le colonne da mostrare |
| Trascina il bordo di una colonna · doppio clic | Cambia la larghezza della colonna · la adatta al contenuto |
| Ctrl+D | Aggiunge o toglie dai preferiti |
| Trascina tra i preferiti | Aggiunge le cartelle ai preferiti |
| Trascina un preferito | Lo sposta tra gli altri (solo l'ordine) |
| Clic su Preferiti · sulla freccia di Questo PC | Chiude o riapre i preferiti · le unità |
| Ctrl+/− | Cambia la dimensione del testo |
| Ctrl+0 | Riporta il testo alla dimensione predefinita |
| Ctrl+, | Apre le opzioni |
| F12 | Mostra statistiche e tempi |

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
