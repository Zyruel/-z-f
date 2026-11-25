# [z]f

File explorer per Windows, veloce e leggero: C++ con Win32 e Direct3D 11, senza framework e senza
dipendenze. L'obiettivo è che ogni operazione sembri istantanea. È gratuito e il codice è
libero, con [licenza MIT](LICENSE).

**Versione attuale: 1.0.0-beta.2.** Cosa cambia in ogni versione è in [`NOVITA.md`](NOVITA.md).

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
  le unità e i percorsi di rete; si connettono e disconnettono unità di rete.
- **Percorso a segmenti** cliccabili, con le sottocartelle in un menu su ogni "›"; si può anche
  scrivere il percorso.
- **"Vai a"** (Ctrl+P): cerca mentre scrivi tra cartelle recenti, preferiti e le loro
  sottocartelle.
- **Lista:** filtro mentre scrivi, ordinamento naturale, selezione multipla e col rettangolo,
  colonne ridimensionabili, tooltip con le informazioni di file, cartelle e unità.
- **Anteprima** (Alt+P) di immagini, testo e cartelle, e **griglia con miniature**
  (Ctrl+Shift+2/3/4).
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
  Windows e Win+E aprono [z]f. Scrive solo nelle impostazioni dell'utente e si annulla con lo
  stesso interruttore.
- **Una finestra sola:** aprire una cartella con [z]f già aperto aggiunge una scheda. Con
  l'opzione "Riapri le schede all'avvio" si riparte dalle schede che c'erano alla chiusura.
- **Si aggiorna da solo:** una volta al giorno guarda se c'è una versione nuova, la scarica, ne
  controlla la firma e la installa per il prossimo avvio. Lo stato è accanto alla versione, nel
  menu ≡: da lì "Aggiorna" riavvia subito [z]f con le stesse schede. Si spegne nelle opzioni.
- **Opzioni** (Ctrl+,) con effetto immediato.

## Scorciatoie

| Tasto | Azione |
|---|---|
| ↑ ↓ PagSu PagGiù Home Fine | Muove il cursore e seleziona la voce |
| Shift + frecce/PagSu/PagGiù/Home/Fine | Estende la selezione dall'ancora |
| Ctrl + frecce | Muove solo il cursore |
| Ctrl+Spazio | Seleziona/deseleziona la voce sotto il cursore |
| Clic · Ctrl+clic · Shift+clic | Seleziona · alterna · intervallo (Ctrl+Shift+clic aggiunge l'intervallo) |
| Trascina (non dal nome di una voce selezionata) · con Ctrl · con Shift | Rettangolo di selezione: sostituisce · alterna · aggiunge (Esc annulla) |
| Trascina dal nome di una voce selezionata | Trascina i file selezionati |
| Clic sullo spazio vuoto sotto le righe | Toglie la selezione |
| Ctrl+A | Seleziona tutto ciò che è visibile |
| Invio / doppio clic | Apre la cartella o il file |
| Backspace | Cancella il filtro, oppure sale di livello |
| Alt+↑ | Cartella superiore (dalla radice di un'unità: "Questo PC") |
| Alt+← / Alt+→, tasti laterali del mouse | Indietro / avanti |
| Lettere qualsiasi | Filtro sui nomi |
| Esc | Cancella il filtro; se non c'è, toglie la selezione |
| Ctrl+L, F4, clic sul percorso | Modifica il percorso (accetta `%VARIABILI%` e virgolette) |
| Ctrl+P · Ctrl+G | Vai a: cartelle recenti, preferiti, sottocartelle (Invio apre, Ctrl+Invio in nuova scheda, Tab entra nella cartella) |
| F5 | Ricarica (conserva selezione, scroll e filtro) |
| Tasto destro · tasto Menu | Menu contestuale |
| Shift+clic destro · Shift+F10 | Menu completo della shell (con l'opzione "Menu completo di Windows": il menu di [z]f) |
| Ctrl+Shift+C | Copia percorso |
| Ctrl+Shift+N | Nuova cartella |
| F2 | Rinomina |
| Canc · Shift+Canc | Cestino · elimina definitivamente |
| Ctrl+Z · Ctrl+Y · Ctrl+Shift+Z | Annulla l'ultima operazione sui file · ripete l'operazione annullata |
| Ctrl+C · Ctrl+X | Copia · taglia |
| Ctrl+V | Incolla i file; se negli appunti c'è un percorso assoluto ci va; altro testo diventa il filtro |
| Alt+Invio | Proprietà |
| Clic sull'intestazione | Ordina per colonna (secondo clic: ordine inverso) |
| Clic sulla destinazione di un collegamento · clic centrale | Apre il percorso del file (la cartella, con la destinazione selezionata) · in una nuova scheda |
| Trascina un bordo dell'intestazione · doppio clic | Larghezza della colonna · adatta al contenuto |
| Ctrl+T · pulsante + | Nuova scheda sulla cartella corrente |
| Ctrl+W · × · clic centrale sulla scheda | Chiude la scheda (l'ultima chiude la finestra) |
| Ctrl+Tab · Ctrl+Shift+Tab | Scheda successiva · precedente |
| Ctrl+1…8 · Ctrl+9 | Scheda n · ultima scheda |
| Clic centrale su una cartella o un preferito, Ctrl+clic su un preferito | Apre in una nuova scheda in background |
| Ctrl+D | Aggiunge la cartella corrente ai preferiti, o la toglie |
| Trascina · Ctrl · Shift · Alt | Sposta (stessa unità) o copia · copia · sposta · crea collegamento |
| Trascina col tasto destro | Al rilascio chiede: copia, sposta o collegamento |
| Trascina su una scheda | La apre dopo mezzo secondo |
| Trascina tra i preferiti | Aggiunge le cartelle ai preferiti |
| Ctrl+B | Mostra/nasconde la barra laterale |
| Alt+P · pulsante a destra di ▦ | Mostra/nasconde l'anteprima |
| Ctrl+Shift+2 · 3 · 4 · pulsante ▦ | Griglia con miniature grandi · medie · piccole (← → ↑ ↓ si muovono nella griglia) |
| Ctrl+Shift+6 · pulsante ▦ | Elenco |
| Trascina il bordo dell'anteprima · doppio clic | Larghezza dell'anteprima · predefinita |
| Ctrl+rotellina · Ctrl +/− · Ctrl+0 | Dimensione del testo · predefinita (in griglia Ctrl+rotellina cambia le miniature) |
| Ctrl+, | Opzioni (con l'elenco ricercabile di tutte le scorciatoie) |
| Clic sul titolo di una sezione della barra laterale | La chiude o la riapre |
| F12 | Pannello statistiche: tempi della cartella e avvio scomposto |

L'elenco completo, con la ricerca, è anche nelle opzioni (Ctrl+,).

## Compilare

```bat
build.bat          :: MSVC, consigliato  -> build\z-f.exe
build.bat debug    :: con simboli per il debugger
build_zig.bat      :: senza Visual Studio (winget install zig.zig)  -> build\z-f-dev.exe
```

Si avvia con `build\z-f.exe [cartella]`; senza argomento si apre la cartella utente. Come è fatto
il programma (moduli, rendering, misure, differenze tra MSVC e zig) è in
[`ARCHITETTURA.md`](docs/ARCHITETTURA.md); le regole dell'interfaccia sono in [`DESIGN.md`](docs/DESIGN.md).
