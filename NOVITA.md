# [z]f — novità

Cosa cambia in ogni versione, dalla più recente. Dalla 1.0 le versioni seguono il Semantic
Versioning: `1.0.0-beta.N` sono le beta, `1.0.0-rc.N` le candidate al rilascio, `1.0.0` la prima
stabile; poi `1.0.1` per le correzioni, `1.1.0` per le funzioni nuove. Fino alla 0.28 le versioni
erano `0.N`. Ogni rilascio, dalla 1.0.0-beta.2, ha un tag `v<versione>` e una
[Release su GitHub](https://github.com/Zyruel/-z-f/releases) con l'exe. Le prove a mano di ogni
versione sono nelle sezioni di `prove\da-verificare.md`; i dettagli tecnici in
[`ARCHITETTURA.md`](docs/ARCHITETTURA.md).

## 1.0.0-beta.8 (in corso)

- **Pannello delle opzioni più leggibile:**
  - scrivendo, la ricerca mostra solo le scorciatoie trovate, in cima al pannello e con le
    lettere trovate evidenziate (prima i risultati restavano in fondo, fuori vista);
  - una barra di scorrimento dice che il pannello continua sotto;
  - le spiegazioni vanno a capo invece di essere tagliate; una scorciatoia troppo lunga per la
    riga ha il testo intero nel tooltip;
  - il passaggio del mouse si vede, su tutta la riga che si clicca; i selettori (dimensione del
    testo, caratteri, posizione dell'anteprima) hanno un fondo da casella;
  - spiegazioni, titoli delle sezioni e versione in un grigio più leggibile; "[z]f al posto di
    Explorer" attivo a metà (con un altro eseguibile, o senza gli zip) lo dice in azzurro;
  - sezioni nuove: "Aspetto" (testo, carattere, animazioni) e "Finestra" (barra laterale e
    anteprima); la barra dei tempi in fondo, in "Diagnostica", dopo le scorciatoie;
  - le opzioni si chiamano con il nome di ciò che accendono: "[z]f al posto di Explorer" (era
    "Usa [z]f…"), "Annullamento anche dopo il riavvio" (era "Annulla dopo il riavvio"), "Schede
    riaperte all'avvio" (era "Riapri le schede…"), "Versioni beta" (era "Ricevi anche le beta");
  - le descrizioni delle scorciatoie cominciano tutte con un verbo ("Va indietro", "Apre le
    proprietà", "Mostra le sottocartelle di quel segmento").
- **"Sposta nel Cestino"** nel menu al posto di "Elimina": lo stesso nome della scorciatoia
  (Canc · Shift+Canc, "Sposta nel Cestino · elimina definitivamente").
- Le note dei tooltip e dell'anteprima ("Clic centrale: nuova scheda", "Accesso negato") sono
  nello stesso grigio delle etichette, più leggibile; così anche "Cartella vuota", "Nessun
  risultato per il filtro", le note di "Vai a", il titolo "Preferiti", lo spazio libero delle
  unità, "Ctrl+D aggiunge", il piede di F12 e la barra dei tempi.
- **Date e dimensioni di una voce selezionata** in un grigio più chiaro, che si legge sul blu
  della selezione (anche la scorciatoia della voce attiva di un menu e la cartella della voce
  attiva di "Vai a").
- **Il nome di un'unità non perde la lettera:** nella barra laterale stretta "Disco locale (C:)"
  diventa "Disco l… (C:)" invece di "Disco local…".
- **Il suggerimento del filtro** ha il punto delle migliaia e il singolare
  per un elemento ("Filtra 1 elemento…", "Filtra 12.345 elementi…"); in una cartella vuota non
  c'è; nell'anteprima di una cartella letta in parte ha il "+" ("Filtra 5.000+ elementi…"), come i
  contatori, che ora hanno anche loro il punto delle migliaia.
- **Alt+Shift+P** sposta l'anteprima da destra in basso e viceversa; il menu ˅ e il suo tooltip
  mostrano la scorciatoia.
- **Correzioni nell'anteprima di una cartella:**
  - Ctrl+rotellina sopra l'anteprima di un testo o di un'immagine non cambia più la vista della
    lista;
  - il tasto Aggiorna della tastiera, con la tastiera nell'anteprima, rilegge l'anteprima come F5
    (prima rileggeva la lista);
  - una vista scelta dal menu dell'anteprima vale anche se intanto l'anteprima mostra un'altra
    cartella (prima diceva "L'anteprima non mostra più quella cartella");
  - durante una rinomina nella lista non compaiono più i tooltip delle voci dell'anteprima;
  - una vera cartella con un nome come "Backup.zip" si aggiorna da sola anche nell'anteprima;
  - se la cartella cambia mentre si trascina la sua barra di scorrimento, il mouse torna libero.
- **Una cartella che si aggiorna da sola tiene la selezione com'è**, nella lista e
  nell'anteprima, con lo stesso codice: senza nessuna voce selezionata (tolte con Ctrl+clic) non
  seleziona più da sé quella del cursore; se le voci selezionate spariscono, si seleziona quella
  che ne prende il posto, come prima.
- **Messaggi dell'anteprima più precisi:** con l'anteprima in basso e la finestra troppo bassa
  dice "troppo bassa" (non "troppo stretta"); Tab con l'anteprima nascosta per mancanza di spazio
  dice perché, invece di "L'anteprima è chiusa"; il testo incollato nell'anteprima, che non si
  accetta, è segnalato in rosso come un rifiuto; il tooltip del filtro dell'anteprima mostra le
  scorciatoie per darle la tastiera (Tab · clic nel pannello).

## 1.0.0-beta.7 (novembre 2026)

- **L'anteprima di una cartella è una seconda lista.** Non si scende nelle sue sottocartelle, ma
  sulle voci si agisce come nella lista: selezione (clic, Ctrl, Shift, rettangolo), tooltip, menu
  col tasto destro, Copia, Taglia, Incolla, Elimina, Rinomina, Proprietà, trascinamento verso
  fuori e rilascio sopra (su una sottocartella o nella cartella mostrata).
- **La vista dell'anteprima si sceglie:** il controllo in fondo a destra del pannello, prima
  inerte, apre il menu delle viste (Icone grandi, medie, piccole, Elenco, Dettagli, con le
  miniature; nello stesso ordine anche il menu della lista, con Dettagli accanto al controllo);
  anche Ctrl+rotellina sopra il pannello. Una vista per il pannello, uguale per tutte le
  cartelle, che si ricorda.
- **Scorciatoie delle viste nell'ordine del menu, dal basso:** Ctrl+Shift+2 Dettagli, 3 Elenco,
  4 Icone piccole, 5 medie, 6 grandi (prima erano al contrario, 2 Icone grandi … 6 Dettagli).
- **Doppio clic nell'anteprima:** un file si apre; una cartella si apre in una nuova scheda in
  background, come col clic centrale. Nell'anteprima non si scende e la lista resta dov'è.
- **Tab** passa la tastiera tra la lista e l'anteprima (anche un clic nel pannello; Esc la
  riporta alla lista). Il cursore si vede solo dove c'è la tastiera.
- **Filtro nell'anteprima:** in basso a sinistra del pannello, come nella barra di stato della
  lista. Con la tastiera nell'anteprima si scrive lì, e filtra solo le sue voci.
- L'anteprima di una cartella si aggiorna da sola quando la cartella cambia (sulle unità locali).
- **L'anteprima a destra o in basso:** dal nuovo ˅ accanto a ◨ (un piccolo menu con le due
  posizioni) o nelle opzioni ("Posizione dell'anteprima"); ◨ continua ad aprirla e chiuderla dove
  sta, e diventa ⬓ quando è in basso. In basso sta sotto la barra di stato della
  lista, larga quanto la lista, con la sua altezza (il bordo si trascina in verticale, doppio
  clic per quella predefinita); una cartella lì mostra anche la colonna Data. Ognuna delle due
  posizioni ricorda la sua misura.

## 1.0.0-beta.6 (novembre 2026)

- **L'anteprima ha la sua barra in fondo**, fatta come la barra di stato della lista e alla stessa
  altezza: a destra il tipo di contenuto (Dettagli per una cartella, Testo, Immagine,
  Informazioni) e prima i dati, cioè i contatori di cartelle e file con le icone, oppure codifica,
  righe e dimensione di un testo, o misure, dimensione e formato di un'immagine. Il totale di una
  cartella non c'è più nell'anteprima (resta nella colonna Dimensione e nel tooltip), e
  l'anteprima non lo fa più contare.
- **Anteprima a tutta altezza:** come la barra laterale, arriva fino in fondo alla finestra. La
  barra di stato (filtro, contatori, vista) e la barra dei tempi stanno sotto la lista, tra i due
  pannelli, perché parlano della lista; la riga d'informazione dell'anteprima è alla stessa altezza
  della barra di stato. Con la lista stretta il filtro si accorcia invece di finire sotto i
  contatori.
- **Niente più menu ≡:** il pulsante in alto a sinistra è ora ⚙ e apre direttamente le opzioni.
  Le voci del menu stavano già altrove: nuova scheda con "+" e Ctrl+T, "Vai a…" con Ctrl+P o un
  clic sul percorso, anteprima con ◨ e Alt+P, vista nella barra di stato, annulla e ripeti con
  Ctrl+Z e Ctrl+Y o dal menu dello sfondo (dove ora c'è anche "Ripeti", quando serve), "Usa [z]f
  al posto di Explorer" e versione nelle opzioni. Una versione nuova si annuncia con il pallino su
  ⚙, e in fondo alle opzioni "Aggiorna a [z]f …" si clicca per intero. Il tasto destro sullo
  spazio vuoto della barra del titolo apre ora il menu di sistema di Windows.
- **La versione in fondo alle opzioni porta alla sua Release** su GitHub, con le novità e i
  download; data e compilatore della build non ci sono più. Un clic sull'icona accanto controlla
  gli aggiornamenti, e l'anello di punti resta almeno un giro, così si vede anche quando GitHub
  risponde in un attimo. Anche la build di prova controlla, ma solo con quel clic, e non scarica.
- **Nuova finestra con Ctrl+N** (come in Explorer), e nel menu delle schede accanto a "Nuova
  scheda". Prima era solo nel menu ≡.
- **▯ accanto a ←:** il pulsante della barra laterale è sceso dalla barra del titolo alla riga del
  percorso, all'altro capo rispetto a ◨. I due pulsanti dei pannelli sono sulla stessa riga, ognuno
  sopra il pannello che apre. Il doppio clic su ▯, ☆ e ◨ non annulla più il primo clic.
- **Zoom della lista con Ctrl+rotellina:** non cambia più il testo di tutta la finestra, ma solo
  la lista, passando per le viste: Dettagli (0%), **Elenco** (10%, nuova: icona e nome in colonne
  che scorrono in orizzontale, quante ne servono per l'altezza della finestra) e Icone da 20% a
  100%, con nove misure di miniature invece di tre. Lo zoom si ricorda per cartella, come prima la
  griglia; le cartelle che erano in griglia si aprono in Icone piccole, medie o grandi, con la
  misura che aveva la griglia. Il testo si ingrandisce
  ancora con Ctrl +/−.
- **Il cursore resta in vista** quando la lista cambia misura (anteprima aperta o chiusa,
  finestra ridimensionata, barra laterale allargata), se prima si vedeva: in Elenco la sua
  colonna non finisce più fuori vista a destra.
- **La vista è nella barra di stato**, a destra dei contatori di cartelle e file, dopo una
  barretta: icona e nome della vista; un clic apre il menu delle viste (Dettagli, Elenco, Icone
  piccole, medie e grandi), la rotellina lì sopra cambia lo zoom. Il pulsante ▦ della barra del
  percorso non c'è più. Nuova scorciatoia Ctrl+Shift+5 per Elenco; Ctrl+Shift+6 resta Dettagli.
- **Aggiungi percorso di rete…** nel menu di Questo PC (barra laterale e sfondo della lista),
  accanto a "Connetti unità di rete…": apre la procedura di Windows, come Explorer, e quando la si
  chiude il percorso nuovo compare sotto Questo PC. Prima per crearne uno serviva Explorer.
- **Zip al posto di Explorer:** con "Usa [z]f al posto di Explorer" anche il doppio clic su uno
  zip fuori da [z]f (desktop, Download, il browser che apre un file scaricato) lo apre in [z]f
  come una cartella, invece che in Esplora file. Chi aveva già attivato l'opzione vede
  "Attivo senza gli zip": un clic sull'interruttore li aggiunge. Se gli zip sono associati a un
  altro programma (7-Zip, WinRAR) restano a quello.
- **Aprire una cartella subito dopo aver chiuso [z]f:** la finestra compariva dopo quasi 5
  secondi (doppio clic sul desktop, Win+E, una cartella aperta da un altro programma). Ora compare
  subito, e con "Riapri le schede" ritrova quelle appena chiuse.
- **Meno memoria nelle cartelle enormi:** in WinSxS (30.734 cartelle) una finestra occupava 126 MB
  e ora da 98 a 101; nella cartella utente resta sui 76. La lista non tiene più spazio vuoto
  riservato durante la lettura, e il conto delle dimensioni delle cartelle non tiene più in
  memoria un percorso completo per ogni cartella. Inoltre la sua memoria non cresce più con ogni
  cartella grande visitata finché la finestra resta aperta.
- **Anteprima di uno zip dentro uno zip:** diceva "Cartella vuota"; ora mostra icona, tipo,
  dimensione e data, come per gli altri file dentro uno zip.
- **Icona grande nell'anteprima all'avvio:** con l'anteprima già aperta, a volte il primo file
  senza anteprima restava senza l'icona grande (e così una miniatura della griglia). Ora c'è
  sempre.
- **Icona più nitida a 175% e 225%:** l'icona di [z]f ha ora anche le misure di questi schermi,
  invece di essere ridotta da Windows con i bordi sfumati. Il file dell'icona pesa 8,7 KB invece
  di 45,7, e l'exe è più piccolo di 37 KB.
- **File solo online (OneDrive e simili) appena scaricati:** un programma, un collegamento o
  un'icona restavano con l'icona generica, e una foto senza miniatura, fino al riavvio di [z]f.
  Ora l'icona vera e la miniatura arrivano da sole appena il file si scarica.
- Il codice torna senza avvisi anche con `/W4`.

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
