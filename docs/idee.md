# Idee per [z]f

Cose venute in mente, non ancora decise né in programma. Per ognuna: da dove viene, cosa
servirebbe, cosa la frena rispetto ai principi di `CLAUDE.md`. Quando un'idea diventa lavoro si
sposta in "Prossimi passi" di `CLAUDE.md` e si toglie da qui. Le piccole correzioni sono in
`da-sistemare.md`, le prove a mano in `prove\da-verificare.md`.

## Interfaccia

Le tre idee che seguono vengono dalla revisione di design del 27/11 (dopo la 1.0.0-beta.7); le
correzioni piccole della stessa revisione sono in `da-sistemare.md`.

### Il pannello delle opzioni dalla tastiera (novembre 2026)

Nel pannello funzionano solo Esc, ↑↓, PagSu/PagGiù e la ricerca (`optionsKey`): un interruttore o
un selettore si cambia solo col mouse, mentre il resto di [z]f si usa tutto da tastiera.

- **Cosa servirebbe:** un controllo attivo (indice nell'elenco `OP.hits`, senza titolo e piede),
  Tab / Shift+Tab per spostarlo, Spazio o Invio per cambiarlo, ← → sui selettori, un contorno
  `C_SEL_EDGE` sul controllo attivo (lo stesso colore del cursore della lista), e lo scorrimento
  che lo tiene in vista. Le lettere restano alla ricerca.
- **Frena:** oggi ogni tasto che non è di navigazione va alla ricerca: Spazio e Tab andrebbero
  tolti alla casella. Poco codice, ma un comportamento nuovo da scrivere in DESIGN §8.

### Colonne vicine al nome nelle finestre larghe (novembre 2026)

A finestra ingrandita su uno schermo da 2560 px, i nomi stanno a sinistra e Data modifica e
Dimensione all'estremo destro: fra "esempio.cpp" e "22 B" ci sono circa 2.000 px vuoti, e si
segue la riga con l'occhio per capire quale data è di quale file. La riga sotto il mouse aiuta, ma
una voce alla volta.

- **Cosa servirebbe:** la colonna Nome larga quanto il nome più lungo visibile, con un massimo, e
  le altre colonne subito dopo (lo spazio in più a destra resta vuoto); oppure una fascia
  leggerissima a righe alterne, solo oltre una certa larghezza.
- **Frena:** la colonna Nome che cambia larghezza da una cartella all'altra fa saltare le colonne
  a ogni cambio; misurare il nome più lungo costa `textWidth` su tutte le voci (da misurare con
  le cartelle grandi). Le righe alterne sono decorazione, che DESIGN §1.5 chiede di evitare.

### Cartella vuota e filtro senza risultati: cosa fare (novembre 2026)

"Cartella vuota" e "Nessun risultato per il filtro" dicono bene lo stato, in `C_FAINT` in alto a
sinistra (`render.cpp`). Potrebbero dire anche cosa si può fare: "Nessun risultato per «zzq» · Esc
toglie il filtro", "Cartella vuota · Ctrl+Shift+N per una nuova cartella".

- **Cosa servirebbe:** solo i testi, con le scorciatoie prese da `shortcutsOf` (non scritte a
  mano).
- **Frena:** DESIGN §1.5 ("poco testo"): chi usa [z]f ogni giorno lo sa già, e il testo in più si
  vede in ogni cartella vuota. È una scelta di Zyruel.

## Finestre

### Tutte le finestre in un solo processo, con i profili (novembre 2026, dopo la 1.0.0-beta.5)

Oggi ogni finestra è un processo con il suo device Direct3D: circa 75 MB di memoria privata a
finestra, quasi tutti del driver della GPU (`ARCHITETTURA.md`, misure della memoria). Zyruel
preferisce lavorare con tutto in una finestra e con una gestione dei profili, che non c'è ancora
(cosa contiene un profilo è da definire con lui).

- **Cosa servirebbe:** lo stesso device per tutte le finestre, ognuna con la sua swapchain. Un
  processo con un device per finestra risparmierebbe poco, se la memoria del driver è per device:
  va misurato prima con un programma in `prove\` che crea due device nello stesso processo. Poi
  lo stato di ogni finestra (`A`, `G` e gli `static` dei moduli) separato, per esempio un thread
  per finestra con `thread_local`, e le cache di icone e miniature condivise.
- **Frena:** un crash chiuderebbe tutte le finestre (si attenua riaprendole con le schede,
  `tabsSerialize`), e con `thread_local` un thread di lavoro che legge `A` troverebbe una copia
  vuota senza che nessuno se ne accorga. Ristrutturazione grande.

## Anteprima

### Navigazione dentro l'anteprima (novembre 2026, durante la 1.0.0-beta.7)

Ora l'anteprima di una cartella è una seconda lista. Il doppio clic su una sua cartella la apre in
una nuova scheda in background, perché nell'anteprima non si scende. Zyruel ha pensato di farci
scendere, lasciando la lista principale dov'è, ma lo ha rimandato: per non aggiungere elementi a
caso.

- **Cosa servirebbe:**
  - una barra di navigazione nell'anteprima, al posto del titolo, quando mostra una cartella. Da
    decidere: se il percorso parte dalla voce sotto il cursore della lista o da Questo PC, se ha i
    segmenti cliccabili, ← → ↑ e i menu "›";
  - cosa succede alla discesa quando il cursore della lista si sposta;
  - Backspace e Alt+↑ con la tastiera nell'anteprima;
  - `PV.item` diverso dalla voce sotto il cursore (oggi `previewSync` li tiene uguali);
  - la barra del percorso (`crumbs.cpp`) usabile anche per una seconda riga, oggi legata a `CB`.
- **Frena:** è un elemento nuovo in un pannello stretto (DESIGN §1: si inventa solo per elementi
  davvero nuovi), e due percorsi visibili insieme possono confondere su quale cartella agiscono
  Incolla e Nuova cartella.

## Archivi

### Modificare gli zip (novembre 2026, dopo la 0.26)

Oggi gli zip sono in sola lettura (`zip.cpp`, `zipout.cpp`): Rinomina, Elimina, Taglia, Incolla e
Nuova cartella sono disattivati, e non si rilascia niente dentro uno zip.

- **Cosa servirebbe:** uno zip non si modifica sul posto. Si riscrive un archivio nuovo accanto,
  copiando così come sono (senza decomprimere) le voci che restano, poi lo si sostituisce con una
  rinomina, come fa `saveDataAsync`. Rinominare ed eliminare costano solo la copia dei byte.
  Aggiungere file richiede un compressore deflate nostro (più lungo dell'inflate), oppure si
  salvano con il metodo "stored" (veloce ma senza compressione).
- **Frena:** è lento sugli archivi grandi (ogni modifica riscrive tutto), va annullato con
  `undoPush` come le altre operazioni, e se qualcosa va storto non deve mai lasciare uno zip
  rovinato. Da fare solo se qualcuno lo chiede.

### 7z, rar e altri formati (novembre 2026, dopo la 0.26)

Oggi solo zip (metodi "stored" e deflate). Deflate64, bzip2, LZMA, zstd e gli zip cifrati
passano a Windows.

- **7z:** il formato ha un'intestazione complicata (spesso compressa a sua volta) e usa LZMA/LZMA2.
  Un decodificatore LZMA nostro è fattibile (qualche centinaio di righe, come l'inflate). Il resto
  del formato è il lavoro grosso.
- **rar:** formato proprietario. Il codice ufficiale per estrarre (unrar) ha una licenza che lo
  permette, ma è una dipendenza grande: contro il principio della leggerezza.
- **Strada da verificare:** Windows 11 (dalla 23H2) apre da sé 7z, rar e tar in Esplora file, con
  libarchive (`archiveint.dll`, non documentata). Usarla darebbe tutti i formati senza scrivere
  decodificatori, ma è un'interfaccia non pubblica che può cambiare. In alternativa, come adesso
  per gli zip non supportati, si lasciano a Windows.
- **tar e tar.gz:** facili (tar è un elenco di intestazioni, gz è deflate: l'inflate c'è già).
  Andrebbero letti tutti per fare l'elenco, perché non hanno una directory centrale.

## Rete

### Condivisioni visitate (novembre 2026)

Mostrare tra le unità le condivisioni già aperte (per esempio quelle di un NAS),
ricavate dalle cartelle recenti, con lo spazio libero. Windows non tiene un elenco simile.

- **Cosa servirebbe:** ricavare da `recenti.txt` le radici `\server\condivisione` e mostrarle
  sotto Questo PC dopo i percorsi di rete, con lo spazio letto dal thread dei percorsi di rete.
- **Frena:** una voce che compare da sola va anche tolta (cestino come i preferiti, o una voce di
  menu), e si mescola con i percorsi di rete veri. Forse basta creare un percorso di rete.
