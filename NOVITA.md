# [z]f — novità

Cosa cambia in ogni versione, dalla più recente. Dalla 1.0 le versioni seguono il Semantic
Versioning: `1.0.0-beta.N` sono le beta, `1.0.0-rc.N` le candidate al rilascio, `1.0.0` la prima
stabile; poi `1.0.1` per le correzioni, `1.1.0` per le funzioni nuove. Fino alla 0.28 le versioni
erano `0.N`. Ogni rilascio, dalla 1.0.0-beta.2, ha un tag `v<versione>` e una
[Release su GitHub](https://github.com/Zyruel/-z-f/releases) con l'exe. Le prove a mano di ogni
versione sono nelle sezioni di `prove\da-verificare.md`; i dettagli tecnici in
[`ARCHITETTURA.md`](docs/ARCHITETTURA.md).

## 1.0.0-beta.5 (novembre 2026)

- **Gesti del touchpad:** "Spostamento indietro" e "Spostamento avanti" (Impostazioni →
  Bluetooth e dispositivi → Touchpad) vanno indietro e avanti, come i tasti Indietro, Avanti e
  Aggiorna delle tastiere che li hanno. Prima [z]f li ignorava.
- **"Vai a" dal percorso:** il clic sullo spazio vuoto del percorso, Ctrl+L e F4 aprono "Vai a"
  con il percorso della cartella aperta già scritto e selezionato: si vedono subito le sue
  sottocartelle, si continua a scrivere il percorso con i suggerimenti, o si scrive sopra per
  cercare tra tutte le cartelle. Prende il posto della casella di testo nel percorso.
- **Una finestra per ogni cartella aperta da fuori** (opzione, in Integrazione con Windows): il
  doppio clic sul desktop, Win+E e gli altri programmi aprono una finestra nuova invece di una
  scheda nella finestra principale. Ogni finestra occupa da 40 a 70 MB (secondo la scheda video), una scheda meno di 1 MB.
- **Colonna Destinazione mai più larga di Nome:** con la finestra non a tutto schermo Nome restava
  al suo minimo e lo spazio in più andava tutto a Destinazione; ora le due colonne crescono e
  calano insieme. Allargando Data o Dimensione, lo spazio lo cede Destinazione.
- **L'anteprima tiene la sua larghezza:** con la finestra stretta si adattano le colonne; prima
  l'anteprima si prendeva lo spazio lasciato da ogni colonna stretta col mouse, e una colonna non
  si riusciva ad allargare.
- **Collegamenti su un'unità di rete:** la colonna Destinazione mostrava il percorso senza
  l'unità ("cartella\programma.exe" invece di "V:\cartella\programma.exe"), e aprire il percorso
  diceva "Percorso non trovato". Ora c'è l'unità, come nelle proprietà di Windows.

## 1.0.0-beta.4 (novembre 2026)

- **[z]f ha un'icona:** "[z]" nel blu di [z]f su un quadrato scuro, nella barra delle
  applicazioni, in Alt+Tab e sul file dell'exe; nitida anche a 16 px.
- Dopo "Aggiorna" la copia della versione precedente si toglie subito e compare "Aggiornato a
  [z]f …" (prima la versione nuova provava a toglierla mentre la vecchia stava ancora uscendo).
- Griglia: in uno zip con molte immagini, tornando su quelle già viste le miniature si rifanno
  dallo zip, senza passare dalla shell (che lì è lenta).
- Griglia e anteprima di Questo PC: i percorsi di rete hanno l'icona di cartella senza chiedere
  niente alla rete, e le icone delle unità di rete e ottiche si chiedono a parte. Un NAS spento
  non ferma più le icone e le miniature delle altre cartelle, e non cambia l'icona delle cartelle.
  Ora anche nell'elenco, nella barra laterale e nei menu: le icone delle unità di rete e ottiche
  (e di .exe e collegamenti che stanno lì) non fanno più aspettare quelle dei file locali.
- Griglia: in una cartella vera che si chiama come uno zip ("foto.zip"), .exe e collegamenti
  hanno la loro icona invece di quella generica.
- Tooltip e anteprima non scaricano più le immagini solo online di OneDrive (e servizi simili) per
  scriverne la risoluzione: un file solo online resta nel cloud finché non lo si apre.
- Con il disco lento all'avvio (antivirus, PC appena acceso), cambiare la dimensione del testo,
  la barra laterale, una larghezza, aggiungere un preferito o passare alla griglia nei primi
  istanti non cancella più impostazioni, preferiti e viste salvati: il cambiamento si aggiunge a
  quelli letti.
- Dialoghi Apri e Salva: tornando al dialogo due volte di fila con cartelle diverse, il dialogo
  entra nell'ultima e il nome proposto resta. Prima i due salti potevano intrecciarsi, e nel caso
  peggiore il dialogo Salva poteva salvare il file con il nome proposto. Una cartella su una
  condivisione che non risponde non fa più aspettare il salto verso la cartella nuova, e un salto
  che arriverebbe dopo 3 secondi non si fa più: intanto si lavora già nel dialogo.
- Barra laterale: un'unità di rete verso un server spento non trattiene più nome e spazio delle
  altre unità, né il nome di un preferito appena aggiunto. Le unità locali arrivano subito; quelle
  di rete e ottiche e i percorsi di rete dopo, una alla volta, e intanto mostrano il nome generico
  o quello di prima. Un DVD si rilegge solo quando lo si inserisce, non a ogni ritorno su [z]f.
  Un DVD tolto o un server spento non lasciano più nella barra lo spazio libero di prima.
- Copia percorso (Ctrl+Shift+C) e Copia/Taglia nelle caselle di testo: se gli appunti sono
  occupati da un altro programma (Desktop remoto, un gestore degli appunti) [z]f riprova per un
  secondo e, se non ci riesce, lo dice e dice chi li tiene, invece di scrivere "Percorso copiato"
  senza averlo copiato; un Taglia non riuscito rimette il testo dov'era. [z]f non si ferma più se
  il programma che possedeva gli appunti è bloccato. Se nel frattempo è riuscita una copia più
  recente, il tentativo vecchio non dà più l'errore.
- Proprietà (Alt+Invio, anche di una voce sola, della cartella aperta, delle unità e dei percorsi
  di rete nella barra laterale) e "Apri con…" non fermano più [z]f: Windows li prepara sullo
  stesso thread del menu completo, e intanto [z]f continua a rispondere. Prima una cartella su
  una condivisione lenta o spenta bloccava la finestra finché la rete rispondeva, e con "Apri
  con…" aperto [z]f non si ridisegnava. Se Windows non trova l'elemento (un NAS spento, una
  cartella sparita) un avviso dice cosa e perché, invece di non far comparire niente.
- Zip dentro uno zip: se durante l'estrazione si va in un'altra cartella o scheda, [z]f resta lì
  invece di portare di forza nello zip estratto (anche se intanto si chiude una scheda prima di
  quella). Il trascinamento da uno zip compare nei tempi di
  F12 come gli altri.
- Opzioni, Scorciatoie: l'elenco ha anche i gesti col mouse che mancavano (rettangolo di
  selezione con Ctrl e Shift, Ctrl+Shift+clic, clic sotto le righe, bordi delle colonne, sezioni
  della barra laterale, riordino dei preferiti) e nessuna descrizione finisce più in "…". Il
  README ha lo stesso elenco, con le stesse parole.

## 1.0.0-beta.3 (novembre 2026)

- **Aggiornamento automatico:** una volta al giorno [z]f guarda su GitHub se c'è una versione
  nuova, la scarica, ne controlla impronta e firma e la mette al posto di quella in uso; parte al
  prossimo avvio. Accanto alla versione, nel menu ≡ e nelle opzioni, un'icona dice lo stato
  (controllo in corso, aggiornato, versione nuova); con una versione pronta ≡ ha un pallino e
  "Aggiorna" riavvia subito tutte le finestre di [z]f, con le loro schede. Opzioni
  "Aggiornamenti automatici" e "Ricevi anche le beta".
- **Riapri le schede all'avvio** (opzione): la finestra principale si riapre con le schede che
  aveva alla chiusura, anche quando Windows si spegne con [z]f aperto.

## 1.0.0-beta.2 (novembre 2026)

Prima versione pubblica: il codice è su GitHub con licenza MIT e la cronologia del repository
riparte da qui. Per chi usa [z]f è uguale alla 1.0.0-beta.1.

## 1.0.0-beta.1 (novembre 2026)

Prima beta della 1.0. I percorsi di rete di Windows (quelli creati con "Aggiungi un percorso di
rete", per esempio una condivisione del NAS senza lettera) compaiono sotto Questo PC dopo le
unità, come in Explorer: barra laterale, Questo PC nella lista, menu "›" del percorso e "Vai a".
Da questa versione il lavoro di ogni giorno sta nel branch `dev` e `main` contiene solo le
versioni rilasciate.

## 0.28

Cambia la parte bassa della finestra: la barra laterale arriva in fondo e si allarga trascinandone
il bordo, come l'anteprima (doppio clic: larghezza predefinita); il filtro sta a sinistra nella
barra di stato, allineato alla lista, i contatori di cartelle e file vanno a destra nella barra di
stato e i tempi misurati hanno una barra loro, solo con l'opzione "Barra dei tempi". I preferiti
stanno rientrati sotto il loro titolo, si riordinano trascinandoli e si tolgono col cestino che
compare sotto il mouse (due clic: il primo chiede conferma sul posto, senza dialoghi). Sotto
"Questo PC" ci sono le unità, come nell'albero di Explorer (sezioni 0000000000000000000 e
000000000000000000 di `prove\da-verificare.md`).

## 0.27

Porta la destinazione dei collegamenti: nelle cartelle con dei .lnk una colonna
"Destinazione" mostra in grigio dove porta ognuno, e un clic ci va con la voce selezionata ("Apri
percorso file"), il clic centrale in una nuova scheda. Il formato lo legge un lettore nostro,
senza la shell: sui 177 collegamenti del PC uguale a Windows e 9 volte più veloce di IShellLink
(sezione 00000000000000000 di `prove\da-verificare.md`).

## 0.26

Gli zip si aprono come cartelle dentro [z]f, con un
lettore e un inflate nostri, senza la shell (70.000 voci in 17 ms; aprire, copiare, trascinare e
anteprima estraggono su un thread), e Incolla funziona anche con file copiati da dentro uno zip in
Esplora file, da un telefono o da un allegato (sezioni 0000000000000000 e 000000000000000 di
`prove\da-verificare.md`).

## 0.25

Chiude il controllo del codice: Shift+Canc e Proprietà di molte voci passano dal thread del menu
della shell (20.000 file: 1,6 s in meno di attesa dell'interfaccia, 8 s su una condivisione), i
tasti che eseguono un comando vengono dalla tabella delle scorciatoie (con esattamente i
modificatori scritti: Ctrl+Shift+W non chiude più la scheda), gli ultimi numeri sparsi sono in
`theme.h` (sezione 00000000000000 di `prove\da-verificare.md`).

## 0.24

Fa partire subito il trascinamento di migliaia di voci (20.000 file: 10,8 ms invece di 550, con
l'icona del primo elemento come immagine) e toglie l'attesa a fine operazione su migliaia di voci.

## 0.23

Toglie l'ultima attesa lunga del thread dell'interfaccia: il menu completo della shell (200–450 ms
per caricare le estensioni) si costruisce e si mostra su un thread suo, mentre l'interfaccia
continua a disegnare.

## 0.22 (novembre 2026)

Il controllo di tutto il codice rispetto ai principi: 26 bug corretti (tra cui la rinomina dopo
"Nuova cartella", che poteva toccare la voce sbagliata), il thread dell'interfaccia che non tocca
più il disco né aspetta altri processi nei casi trovati (salvataggi, watcher, registro, appunti,
"Usa [z]f al posto di Explorer"), icone su due thread, niente duplicati, colori, misure, tempi e
soglie tutti da `theme.h`, scorciatoie dei tooltip da `kShortcuts`, zero avvisi anche con `/W4`
(sezioni da 000000 a 00000000000 di `prove\da-verificare.md`).

## 0.21

La dimensione totale delle cartelle (tooltip, colonna Dimensione, anteprima) e il salto dei
dialoghi Apri e Salva degli altri programmi nella cartella aperta in [z]f.

## 0.20

"Questo PC" nella barra laterale, connetti e disconnetti
unità di rete, i nomi delle unità di rete come in Explorer e l'opzione per avere il menu completo
di Windows col tasto destro.

## 0.19

La vista a griglia con miniature (Ctrl+Shift+2/3/4) e annulla e ripeti (Ctrl+Z, Ctrl+Y).

## 0.18

"Vai a", il rettangolo di selezione, le colonne ridimensionabili e il pannello di anteprima.

## Fino alla 0.17

Le fondamenta, descritte in [`ARCHITETTURA.md`](docs/ARCHITETTURA.md). Fino alla 0.15 il programma si
chiamava zx.
