# Prove da fare a mano — griglia, annulla, anteprima, "Vai a", rettangolo e colonne, pannelli, scorciatoie, trascinamento

Stato: rilascio **0.28** (copia in `build\z-f-0.28.exe`): la **barra laterale a tutta altezza e
ridimensionabile**, il filtro nella barra di stato e la barra dei tempi come opzione (sezione
0000000000000000000), più le **unità sotto Questo PC** (sezione 000000000000000000). La 0.27 aveva
la **colonna Destinazione dei
collegamenti** (sezione 00000000000000000), già provata da Zyruel; resta un altro PC. La 0.26
aveva gli **zip come cartelle** (sezione
0000000000000000) e **Incolla da uno zip** (sezione 000000000000000): sono le prime prove da fare.
Le idee per dopo sono in `docs/idee.md`, le piccole cose da sistemare in `docs/da-sistemare.md` (nella
cartella del progetto). La 0.25 era la **fine del controllo del codice**: Shift+Canc e Proprietà
sul thread del menu della shell, tasti dalla tabella delle scorciatoie (sezione 00000000000000).
La 0.24 aveva l'**inizio di un trascinamento con molte voci** (sezione
0000000000000: le prove verso le altre app restano da fare). La 0.23 aveva
il **menu completo della shell su un thread suo** (sezione 000000000000). La 0.22 era il **controllo del codice**
rispetto ai principi (sezioni da 000000 a 00000000000). La 0.21 aveva portato la
**dimensione delle cartelle** e il **salto dei dialoghi Apri/Salva** (sezione 00000). La 0.20
aveva portato **Questo PC** nella barra laterale, **connetti e disconnetti unità di rete**, i
**nomi delle unità di rete** e l'opzione **"Menu completo di Windows col tasto destro"** (sezione
0000); la 0.19 la **vista a griglia** e **annulla e ripeti**. Le prove delle sezioni 000 e 00
restano da fare. Se qualcosa non va, la 0.20 è in `build\z-f-0.20.exe`.

## 0000000000000000000000000000000000000000000000000. Correzioni della revisione del codice (z-f-dev, novembre 2026)

Sette correzioni dopo la revisione del ramo: atlante delle miniature, risincronizzazione delle due
liste in Icone, trascinamento dall'anteprima, rilettura dopo un'operazione, contatori col filtro,
avviso di Ctrl+V. Provato da Claude in z-f-dev a 175%, finestra ingrandita, in una cartella di PNG
finti (scratchpad):
- [x] lista in Icone grandi, poi Ctrl+rotellina fino al 20%: tutte le miniature visibili (circa
  320); prima i posti restavano grandi come al 100% e ne stavano 81;
- [x] lista e anteprima in basso tutte e due in Icone medie, anteprima scorsa: miniature in tutte e
  due; F12 "griglia: risultati e richieste" 0,00 ms (peggiore 0,02), frame 0,16 ms;
- [x] trascinamento di un file selezionato dall'anteprima su una cartella della lista: spostato; la
  cartella locale dell'anteprima si rilegge dall'osservatore (il file sparisce, 399 file);
- [x] filtro "img-01" nell'anteprima: contatori 0 cartelle, 100 file;
- [x] Ctrl+C del percorso e Ctrl+V con la tastiera nell'anteprima: avviso "Nell'anteprima si
  incollano solo file…";
- [ ] trascinamento premuto nell'anteprima mentre arriva un'altra cartella (si sposta il cursore
  della lista con la tastiera e si preme subito su una voce della vecchia): non deve partire; non
  riprodotto (servono una cartella lenta e il tempo giusto);
- [ ] operazione in una cartella di rete mostrata dall'anteprima: si rilegge da sola (lì non c'è
  l'osservatore).

## 000000000000000000000000000000000000000000000000. Anteprima a destra o in basso (z-f-dev, novembre 2026)

Il pannello di anteprima ha un rettangolo suo (`prevX/Y/W/H`) e la barra di stato della lista
finisce a `footY`. Provato da Claude in z-f-dev, nella cartella di `prove\anteprima-prova.py`, a
175% e finestra piccola; poi il PC è stato preso da Zyruel e il resto è rimasto da fare.
- [x] ˅ accanto a ◨ (al posto del tasto destro, scelta di Zyruel): tooltip "Posizione
  dell'anteprima", clic → menu "A destra" / "In basso" sotto il pulsante con la spunta, ˅
  evidenziato; In basso: lista, barra di stato, anteprima con intestazione e banda in fondo;
  icona DockBottom; ◨ chiude e riapre l'anteprima in basso;
- [x] doppio clic su ˅ (il menu resta aperto) e su ‹ › nelle opzioni (la posizione cambia una
  volta sola); clic su ˅ col suo menu aperto (si chiude);
- [x] cartella in Dettagli in basso: nome, Data e Dimensione; Tab, filtro dell'anteprima (lettere,
  contatori), menu della vista dell'anteprima sopra il suo controllo;
- [x] immagine e testo in basso: l'immagine si rifà alla misura nuova, il testo va a capo su tutta
  la larghezza, la rotellina lo scorre;
- [x] bordo: trascinato in su allarga l'anteprima e la lista tiene in vista il cursore; doppio
  clic → altezza predefinita;
- [x] ritorno a destra con la larghezza di prima; opzioni: "Posizione dell'anteprima ‹ › "
  cambia posizione; riaprendo, la posizione si ricorda (`anteprima_posizione`);
- [ ] corretto ma non riprovato: il menu della vista della **lista** si apriva sotto il
  controllo, sopra l'anteprima (c'era posto); ora si apre sempre sopra;
- [x] cursore ↕ sul bordo, Esc durante il trascinamento (torna all'altezza di prima);
- [ ] finestra bassa: l'anteprima sparisce (resta aperta) e torna allargandola; aprirla senza
  spazio: avviso "troppo bassa";
- [ ] Icone grandi nell'anteprima in basso a 175% a tutto schermo (atlante, `da-sistemare.md`);
  Elenco in basso con la barra di scorrimento orizzontale (il bordo non la prende);
- [ ] trascinamento di file da e verso l'anteprima in basso; rettangolo nella lista accanto al
  bordo; rinomina nell'anteprima in basso;
- [x] tooltip di ◨ (riga Posizione), del filtro della lista e della barra dei tempi con
  l'anteprima in basso;
- [x] F12 con l'anteprima in basso in Icone medie, finestra ingrandita: frame 0,12 ms (peggiore
  0,28); il pannello F12 sta sopra le barre della lista.

## 00000000000000000000000000000000000000000000000. Anteprima di una cartella come seconda lista (z-f-dev, novembre 2026)

La cartella nell'anteprima è una lista vera (`ListPane` scambiato con `App` nei `PaneScope`), con
la sua vista, selezione, menu, trascinamento, tastiera (Tab) e aggiornamento da sola. Compilato
senza avvisi (MSVC). Provato da Claude in z-f-dev in una cartella con file finti
(`prove\anteprima-prova.py`): le voci con [x] e quelle con "fatto". Le correzioni del `/controllo`
venute dopo (atlante che non si rifà passando da un file a una cartella, rilettura con la selezione
al posto della voce eliminata, osservatore che riparte, Ctrl+V e Backspace con la tastiera
nell'anteprima, menu dopo una rilettura) sono compilate ma non riprovate a schermo (PC in uso).
- [x] cursore su una cartella: l'anteprima la mostra come prima in Dettagli (icona, nome,
  dimensione), righe allineate a quelle della lista; hover sulle voci;
- [x] controllo della vista in fondo a destra dell'anteprima: fondo sotto il mouse, clic → menu
  con la spunta sulla vista dell'anteprima; Elenco, Icone piccole/medie/grandi; la rotellina sopra
  il controllo e Ctrl+rotellina sul pannello (`prove\rotella.py`) cambiano la vista; si ricorda
  riaprendo (`anteprima_vista`); con testo e immagini il controllo resta inerte (questo da provare);
- [x] Icone nell'anteprima con le miniature, anche con la lista in Icone a un'altra misura (le
  due insieme: niente miniature che lampeggiano o spariscono); poi una sola delle due;
- [ ] a 175%: lista in Icone piccole a tutto schermo con l'anteprima in Icone grandi (atlante con 81
  posti, `docs/da-sistemare.md`);
- [x] Elenco nell'anteprima scorrendo in orizzontale (fatto: Elenco con 60 voci, senza scorrimento):
  le colonne non sconfinano nella lista;
- [x] clic, Ctrl+clic, Shift+clic, rettangolo (anche con lo scorrimento vicino ai bordi), Esc
  durante il rettangolo;
- [x] tasto destro su una voce (fatto: Ctrl+clic, menu della selezione, Copia percorso) e sullo
  sfondo dell'anteprima: Copia, Taglia, Incolla (nella
  cartella mostrata), Nuova cartella (rinomina subito nella cella), Rinomina, Elimina, Proprietà,
  Altre opzioni…; Shift+tasto destro (menu della shell);
- [x] doppio clic e Invio su un file: si apre; su una sottocartella e su uno zip: nuova scheda in
  background, la lista e l'anteprima restano dove sono; clic centrale: lo stesso;
- [x] trascinamento (fatto: anteprima → sua sottocartella, anteprima → lista) dall'anteprima alla
  barra laterale, fuori da [z]f; dalla lista
  sull'anteprima (nella cartella mostrata e su una sua sottocartella, col bordo della
  destinazione); in uno zip mostrato nell'anteprima: divieto;
- [x] (fatti: Tab, frecce, Fine, Canc, F2, Ctrl+Shift+N, Esc) Tab: la tastiera passa all'anteprima (cursore lì, sparisce dalla lista) e torna; frecce,
  PagSu/PagGiù, Home/Fine, Ctrl+A, Invio, Canc, Ctrl+C/X/V, F2, F5, Alt+Invio, tasto Menu;
  Alt+←, Ctrl+T restano della finestra; Esc toglie la selezione, poi riporta la tastiera alla lista;
- [x] filtro dell'anteprima (fatti da Claude: clic nel campo e lettere, contatori, Backspace, Esc,
  azzeramento cambiando voce, lettere con la tastiera nella lista che filtrano la lista; restano
  ×, tooltip, rilettura e finestra stretta): campo a sinistra nella banda, le lettere con la tastiera
  nell'anteprima filtrano lei (non la lista), contatori del filtro, ×, Esc e Backspace (prima il
  filtro, poi Backspace va alla cartella superiore della lista), clic nel campo dà la tastiera,
  tooltip; resta quando la cartella si rilegge e si toglie passando a un'altra voce; con la
  finestra stretta il campo si accorcia e sparisce;
- [x] un file creato o cancellato da fuori nella cartella mostrata: l'anteprima si aggiorna da
  sola, tenendo selezione e scorrimento; una rinomina in corso nell'anteprima resta;
- [x] tooltip sulle voci dell'anteprima e sul suo controllo della vista;
- [x] F12: frame 0,12 ms (peggiore 0,24) con lista e anteprima in Icone; contenuto visibile a 50 ms;
- [x] tempi d'avvio con `prove\avvio.bat` (Claude, 4 giri alternati da 5 con la 1.0.0-beta.6):
  contenuto in mediana 78 ms contro 77, prima icona 105 contro 104, primo fuoco 11 contro 12;
  invariato. Nel registro `messaggio lento 0x0010` (circa 50 ms, la chiusura della finestra di
  prova) con tutte e due le build;
- [x] registro (`build\z-f-log.txt`) dopo le prove: solo voci normali. Un crash trovato e corretto
  (`buildLower` mancante in `pvDir`: la rilettura ritrova la selezione per nome) e un guasto del
  trascinamento dall'anteprima (ciclo modale dentro lo scambio) corretto.

## 0000000000000000000000000000000000000000000000. Correzioni del controllo prima della 1.0.0-beta.6 (z-f-dev, novembre 2026)

Dal `/controllo cambiati`: doppio clic su ⚙ e sulla versione, Ctrl+rotellina con le opzioni
aperte, menu di sistema sullo spazio vuoto della barra del titolo. Compilato senza avvisi (MSVC).

Provato da Claude in z-f-dev (cartella `build\`):
- [x] doppio clic su ⚙: le opzioni si aprono e restano aperte (prima si richiudevano subito);
- [x] Ctrl+rotellina sopra le opzioni (`WM_MOUSEWHEEL` con `MK_CONTROL` mandato da uno script:
      lo strumento dello schermo non tiene Ctrl con la rotellina): la vista resta Dettagli; a
      opzioni chiuse lo stesso messaggio porta in Elenco;
- [x] tasto destro sullo spazio vuoto della barra del titolo: il menu di sistema (Sposta,
      Ridimensiona, Riduci a icona, Ingrandisci, Chiudi) sotto il mouse; Esc lo chiude;
- [x] registro: solo le voci normali dell'avvio, niente "messaggio lento" per il menu.

Restano:
- [ ] doppio clic sulla versione in fondo alle opzioni: una sola scheda del browser (non provato
      per non aprire pagine nel browser di Zyruel);
- [ ] menu di sistema: con la finestra ingrandita "Ripristina" attivo e Sposta, Ridimensiona,
      Ingrandisci disattivati; una voce scelta (Riduci a icona) funziona.

## 000000000000000000000000000000000000000000000. Versione nelle opzioni come collegamento (z-f-dev, novembre 2026)

Tolti data e compilatore dal piede delle opzioni; la versione apre la sua Release su GitHub;
l'anello del controllo resta almeno un giro. Compilato senza avvisi (MSVC).

Provato da Claude in z-f-dev:
- [x] piede: "[z]f 1.0.0-beta.5" e l'icona subito dopo;
- [x] con `--aggiorna-da=http://127.0.0.1:9` un clic sull'icona mostra l'anello, poi le frecce
      circolari e l'avviso "Aggiornamenti non controllati";
- [x] la Release `v1.0.0-beta.5` esiste (`gh release view`);
- [x] z-f-dev controlla con un clic sull'icona (prima diceva solo "Build di prova"): ↻, l'anello,
      poi il cerchio con la spunta; nel registro "aggiornamento: controllo 0x0002 604.9 ms".
- [x] Zyruel: hover e clic sulla versione vanno ("tutto ok").

Restano:
- [ ] con `build\z-f.exe` un clic sull'icona mostra l'anello per circa un giro, poi la spunta;
- [ ] con una versione nuova pronta, "Aggiorna a [z]f …" si clicca ancora per intero (niente
      collegamento).

## 00000000000000000000000000000000000000000000. Banda in fondo all'anteprima (z-f-dev, novembre 2026)

La riga d'informazione dell'anteprima diventa una banda come la barra di stato: a destra il tipo
di contenuto, prima i dati. Senza il totale della cartella. Compilato senza avvisi (MSVC).

Provato da Claude in z-f-dev, nella cartella di `anteprima-prova.py`, a finestra ingrandita:
- [x] cartella vuota: "🗀 0 🗋 0 │ Dettagli"; molti_file: "🗀 1 🗋 3000 │ Dettagli";
- [x] dati.json: "ASCII · 1 riga · 75 B │ Testo";
- [x] foto_grande.png: "4000 × 3000 · 363 KB · PNG │ Immagine";
- [x] binario.bin: solo "Informazioni", senza barretta;
- [x] la banda continua la barra di stato della lista, divisa dal bordo dell'anteprima;
- [x] registro: solo le voci normali dell'avvio e "attesa vsync lunga".

Restano:
- [ ] una cartella con più di 5000 voci: i numeri col "+";
- [ ] "caricamento…" (cartella lenta, di rete): la banda c'è, vuota;
- [ ] un'unità di Questo PC e una cartella illeggibile: Informazioni, e Dettagli senza contatori;
- [ ] anteprima stretta: i dati di un'immagine finiscono in "…" prima della barretta;
- [ ] a 175% (Zyruel).

## 0000000000000000000000000000000000000000000. Anteprima a tutta altezza (z-f-dev, novembre 2026)

L'anteprima arriva in fondo alla finestra come la barra laterale; barra di stato e barra dei
tempi stanno sotto la lista, tra i due pannelli. Il filtro si accorcia se non ci sta. Compilato
senza avvisi (MSVC).

Provato da Claude in z-f-dev (C:\Windows\Web):
- [x] il separatore dell'anteprima scende fino in fondo; la riga d'informazione ("1 cartella,
      0 file · 1,0 MB") è sulla linea del testo della barra di stato;
- [x] contatori e vista finiscono prima del bordo dell'anteprima;
- [x] il bordo dell'anteprima si trascina all'altezza della barra di stato; lì il doppio clic
      rimette la larghezza predefinita;
- [x] anteprima larga al massimo (la lista al minimo): "Filtra 4 elem…" si accorcia e resta a
      distanza dai contatori; scrivendo "wa" il campo mostra testo e ×, Esc lo toglie;
- [x] barra dei tempi accesa: sotto la sola lista, finisce in "…" prima del bordo; l'anteprima
      non si accorcia;
- [x] registro: solo le voci normali dell'avvio.

Restano:
- [ ] clic sulla vista (menu) e rotellina sopra (zoom) con l'anteprima aperta;
- [ ] rotellina sulla parte bassa dell'anteprima, con un testo o una cartella lunghi: scorre;
- [ ] tooltip della barra dei tempi solo sopra la lista;
- [ ] finestra così stretta che il filtro sparisce (meno di `FILTER_MIN` altezze): scrivendo nella
      lista il filtro funziona lo stesso;
- [ ] anteprima chiusa: barra di stato larga quanto prima;
- [ ] a 175% (Zyruel).

## 000000000000000000000000000000000000000000. ⚙ al posto del menu ≡, ▯ nella riga del percorso (z-f-dev, novembre 2026)

Il menu ≡ non c'è più: il pulsante in alto a sinistra è ⚙ e apre le opzioni. ▯ è sceso nella
riga del percorso, a sinistra di ←, gemello di ◨. "Nuova finestra" ha Ctrl+N ed è nel menu della
scheda; "Ripeti" è nel menu dello sfondo quando c'è. Compilato senza avvisi (MSVC).

Provato da Claude in z-f-dev (C:\Windows\Web, poi C:\):
- [x] barra del titolo: ⚙, schede, "+"; riga del percorso: ▯ ← → ↑ … ☆ ◨, con ▯ e ◨ in
      `C_TEXT` a pannelli aperti;
- [x] ▯ nasconde e rimostra la barra laterale; doppio clic su ◨ la chiude una volta sola (prima
      il secondo clic la riapriva);
- [x] tooltip di ⚙: "Opzioni · Scorciatoia Ctrl+,"; un clic apre le opzioni;
- [x] tasto destro sulla scheda: Nuova scheda Ctrl+T, Nuova finestra Ctrl+N, Chiudi scheda,
      Chiudi le altre schede;
- [x] Ctrl+N apre un'altra finestra sulla stessa cartella;
- [x] registro: solo le voci normali dell'avvio e "attesa vsync lunga".

Restano:
- [ ] **a 175%** (Zyruel): ⚙ e ▯ ben centrati, e la distanza fra ▯ e ← uguale a quella fra ☆ e ◨;
- [ ] **versione nuova pronta** (`aggiornamenti-prova.py`): pallino su ⚙, tooltip con "Aggiorna a
      [z]f …", piede delle opzioni con "Aggiorna a [z]f …" in `C_TEXT` cliccabile insieme alla
      freccia; l'avviso dice "… o subito dalle opzioni";
- [ ] "Ripeti …" nel menu dello sfondo dopo un Ctrl+Z, assente senza niente da ripetere;
- [ ] tasto destro su ⚙ e su "+": nessun menu.

## 00000000000000000000000000000000000000000. Zoom della lista e vista nella barra di stato (z-f-dev, novembre 2026)

Ctrl+rotellina passa per Dettagli (0%), Elenco (10%, nuovo) e Icone da 20 a 100%; il controllo
della vista è nella barra di stato, il pulsante ▦ della riga del percorso non c'è più. Compilato
senza avvisi (MSVC).

Provato da Claude in z-f-dev, nella cartella di prova di `griglia-prova.py` (904 file):
- [x] Ctrl+Shift+5: Elenco, colonne di voci, barra di scorrimento orizzontale in fondo; la
      rotellina scorre di una colonna a scatto.
- [x] Rotellina sul controllo della vista: Elenco → Icone 20% → 50% → 100% (si ferma lì).
- [x] Clic sul controllo: menu sopra, allineato a destra, con la spunta sulla vista attuale e le
      scorciatoie; "Dettagli" ci torna.
- [x] Elenco da tastiera: → di una colonna, ↓ di una voce, Fine fino all'ultima colonna; la
      lista segue il cursore.
- [x] Elenco col rettangolo: prende le voci di due colonne (16 selezionate); Ctrl+A con
      l'anteprima aperta: la colonna tagliata a destra non entra più nell'anteprima (corretto).
- [x] F2 in Elenco: la casella sta nella colonna; Esc annulla. Tooltip del controllo.
- [x] Registro: solo le voci normali dell'avvio e attese vsync note. Frame 0,05–0,10 ms.

Provato da Zyruel, a 175%:
- [x] **Ctrl+rotellina** sulla lista (lo strumento dello schermo non manda il Ctrl con la
      rotellina: Claude l'aveva provato solo dalla rotellina sul controllo, stesso codice).
- [x] La barretta tra contatori e vista (1 px `C_LINE`).
- [x] Le icone di Dettagli (BulletedList) ed Elenco (List) nel font di Windows 11.

Da provare:
- [ ] Elenco: trascinare file da una colonna e su una cartella in un'altra colonna, con lo
      scorrimento vicino ai bordi sinistro e destro.
- [x] Una cartella che era in griglia (viste.txt vecchio) si apre con la misura che aveva la
      griglia, e il file si riscrive nella forma nuova: provato da Claude con `APPDATA` in una
      cartella di prova, `griglia_dimensione=2` e una riga "griglia" → la riga diventa
      "icone 100" (Icone grandi). Senza `griglia_dimensione` resta Icone medie.
- [x] Elenco senza la striscia vuota a destra (dopo la revisione dei push): le colonne arrivano al
      bordo della lista, il mouse prende le voci anche lì (tooltip), Fine mostra l'ultima colonna
      intera; con l'anteprima aperta le colonne si fermano al suo bordo. Provato da Claude in
      z-f-dev, 400 file finti.
- [x] Il cursore resta in vista quando cambia la geometria (`layout`): in Elenco, con la lista in
      fondo, Alt+P lascia visibile la colonna del cursore (prima finiva a destra, fuori vista);
      con la lista scorsa lontano dal cursore, Alt+P non la sposta. Provato da Claude in z-f-dev.
- [ ] Lo stesso ridimensionando la finestra (anche in altezza, che in Elenco cambia le voci per
      colonna) e trascinando il bordo della barra laterale, in Dettagli, Elenco e Icone.

## 0000000000000000000000000000000000000000. "Aggiungi percorso di rete…" in Questo PC (z-f-dev, novembre 2026)

La voce apre la procedura di Windows con il comando di Explorer (letto da `CommandStore`); alla
chiusura la barra laterale si rilegge. Compilato senza avvisi (MSVC).

Provato da Claude in z-f-dev:
- [x] Menu di Questo PC nella barra laterale: Apri, Apri in nuova scheda, Connetti unità di
      rete…, Aggiungi percorso di rete…, Proprietà.
- [x] La procedura si apre in primo piano; creato un percorso verso `\\localhost\C$` (senza
      "Apri il percorso di rete…" all'ultima pagina): dopo Fine compare subito sotto Questo PC.
      Tolto il percorso di prova e riaperta la procedura, con Annulla la barra si rilegge e la
      voce sparisce.
- [x] Registro: niente di nuovo oltre il passaggio WARP → GPU all'avvio.

Resta a Zyruel:
- [ ] La voce nel **menu dello sfondo** di Questo PC (lista): stessa procedura.
- [ ] Con **"Apri il percorso di rete quando si sceglie il pulsante Fine"** spuntata (com'è di
      solito): il percorso si apre (in [z]f se è il file manager predefinito) e compare sotto
      Questo PC.

## 000000000000000000000000000000000000000. File solo online appena scaricati: icona e miniatura (z-f-dev, novembre 2026)

Un file solo online ha ora l'icona del tipo (`iconSource`), così quando si scarica la chiave
cambia e l'icona vera arriva da sola; le miniature si rifanno anche quando cambiano solo gli
attributi (`thEntry`). Compilato senza avvisi (MSVC).

Provato da Claude con `prove\online-prova.cpp attendi` e z-f-dev sulla cartella dei segnaposto:
- [x] All'apertura .exe, .lnk, .ico, .png e .jpg hanno l'icona generica, nessuno scaricamento da
      z-f-dev (4 scaricamenti in tutto, tutti di `cat`, quelli chiesti apposta).
- [x] Scaricati da Bash `regedit.exe` e `AdGuard.lnk`: in elenco, entro un secondo, l'icona vera
      (e la destinazione del .lnk); `z-f.ico`, ancora online, resta generica.
- [x] In griglia, scaricato il .png: la miniatura e l'anteprima grande arrivano da sole.
- [x] Registro: solo il passaggio WARP → GPU all'avvio.

Resta a Zyruel (serve OneDrive vero):
- [ ] In una cartella di OneDrive, un .exe o un .lnk "solo online": icona generica; aperto (si
      scarica), in [z]f l'icona diventa quella vera senza riavviare. Lo stesso per una foto in
      griglia: dopo averla aperta compare la miniatura.

## 00000000000000000000000000000000000000. Icona con le misure di 175% e 225%, in PNG (z-f-dev, novembre 2026)

Compilato senza avvisi (MSVC). Provato da Claude a 100%: la finestra prende 32 e 16 px, 10 avvii
alternati con l'icona di prima, contenuto uguale (52–53 ms), "icona della finestra" 1,2 → 2,8 ms;
le misure 28, 36, 56 e 72 viste con `prove\icona-misure.cpp`, nitide. Lo schermo era al 100%:
a 175% dal vero non l'ha visto nessuno.

- [ ] **A 175%**, con `prove\dev.bat`: l'icona nella barra delle applicazioni, in Alt+Tab e nella
      barra del titolo è nitida, senza bordi sfumati né alone scuro intorno a "[z]" (prima Windows
      riduceva la 32 e la 64).
- [ ] **Esplora file** sulla cartella `build\`, in Icone grandi e in Dettagli: l'icona di
      `z-f-dev.exe` è nitida (Windows può tenere in cache quella di prima: se sembra vecchia,
      basta una copia dell'exe con un altro nome).

## 0000000000000000000000000000000000000. Zip al posto di Explorer (z-f-dev, novembre 2026)

Compilato senza avvisi (MSVC). Provato da Claude: `z-f-dev.exe --new-window <prova.zip>` apre la
finestra dentro lo zip (prima: la cartella che lo contiene, con lo zip selezionato). Log normale.
Il registro non l'ha toccato Claude: l'interruttore lo usa Zyruel.

- [ ] **Con l'integrazione già attiva da prima:** le opzioni dicono "Attivo senza gli zip: un clic
      li aggiunge" (menu ≡: "attivo (senza zip)"); un clic → "attivo", e l'avviso dice "cartelle,
      unità e zip".
- [x] **Doppio clic su uno zip** sul desktop (provato da Zyruel sul desktop) e in Download (anche dall'elenco dei download del
      browser): si apre in [z]f, dentro lo zip, come scheda della finestra principale.
- [ ] **Tasto destro su uno zip** in Esplora file: "Apri in [z]f" in grassetto, "Estrai tutto…"
      ancora lì.
- [ ] **Zip cifrato** aperto in [z]f, doppio clic su un file: si apre ancora in Esplora file (il
      ripiego di `zipout.cpp`), senza tornare in [z]f.
- [x] **Chiudere e riaprire subito** (riprovato da Zyruel col doppio clic sul desktop: risolto) (corretto dopo le prove di Zyruel: una volta su due la
      finestra compariva dopo 4,7 s): doppio clic sullo zip, chiusura, di nuovo doppio clic,
      più volte di fila. Misurato da Claude chiudendo la principale con `WM_CLOSE`: 20–23 ms
      (prima 4,7 s).
- [x] **Schede riaperte dopo una chiusura veloce** (dopo la revisione dei push: la principale
      scrive i file in coda prima di liberare il mutex): provato da Claude con z-f-dev come
      principale (z-f.exe chiuso) e `APPDATA` di prova con `riapri_schede=1`: tre schede, chiusura
      con `WM_CLOSE` e riavvio subito, la principale nuova parte 39 ms dopo e riapre le tre
      schede. "chiusura: file salvati" nel registro: 0,2 e 0,8 ms.
- [ ] Lo stesso con `build\z-f.exe` e le schede vere di Zyruel.
- [ ] **Disattivando:** gli zip tornano a Esplora file (`HKCU\Software\Classes\CompressedFolder\shell`
      senza valore predefinito, senza il verbo `z-f`); idem con `prove\ripristina-explorer.bat`.

## 000000000000000000000000000000000000. Memoria delle cartelle enormi (z-f-dev, novembre 2026)

Compilato senza avvisi (MSVC; zig non è installato). Provato da Claude su WinSxS con
`prove\memoria.ps1`: 126 → 98–101 MB di privata. La colonna Dimensione mostra i totali (screenshot)
e il conto arriva in fondo (30.720 cartelle in 6 s). La pulizia della cache è stata provata con
soglie abbassate apposta (tiene le sottocartelle della cartella aperta). Log normale.

- [ ] **Colonna Dimensione in una cartella grande, nell'uso normale:** i totali compaiono come
      prima, dall'alto verso il basso, anche cambiando scheda a metà conto e tornando.
- [ ] **Pulizia della cache con le soglie vere:** dopo qualche cartella grande (WinSxS, System32,
      un `node_modules`) e più di un minuto, la colonna della cartella aperta resta piena.
      Ordinare per dimensione mette ancora in ordine le cartelle già contate.

## 00000000000000000000000000000000000. Gesti del touchpad e tasti Indietro/Avanti (z-f-dev, novembre 2026)

Compilato senza avvisi (MSVC). Senza schermo (accesso negato), con `WM_APPCOMMAND` mandato alla
finestra di prova: Indietro e Avanti navigano, sia "da tastiera" sia "da mouse". I tasti
Indietro/Avanti simulati con `PostMessage`/`SendMessage` non si possono provare così: Windows li
trasforma in `WM_APPCOMMAND` solo se arrivano da input vero (provato anche con una finestra
qualsiasi). Log normale.

- [x] **Gesti a tre dita** "Spostamento indietro" e "Spostamento avanti" (Impostazioni →
      Bluetooth e dispositivi → Touchpad): Indietro e Avanti come Alt+← e Alt+→. Provato da
      Zyruel su un altro PC con la build di prova: funziona.
- [ ] Gesti sopra le opzioni e "Vai a": non succede niente; con un menu aperto il menu si chiude.
- [ ] **Tasti laterali del mouse:** navigano una volta sola per clic (prima della modifica il
      rilascio non arrivava mai a Windows; ora lo ferma `WM_XBUTTONUP`). Sopra la barra del titolo
      ora navigano anche lì.
- [ ] **Tastiera con i tasti del browser** (Indietro, Avanti, Aggiorna), se c'è: navigano e
      ricaricano. Volume e tasti della musica funzionano come prima.

## 0000000000000000000000000000000000. Destinazione dei collegamenti su un'unità di rete (z-f-dev, novembre 2026)

Compilato senza avvisi (MSVC). Con `build\collegamenti-prova.exe`: un collegamento costruito
come lo scrive Windows per `V:\cartella\programma.exe` (LinkInfo con unità e condivisione, senza
percorso locale) dava `cartella\programma.exe`, ora il percorso intero; i 71 collegamenti di
Desktop e menu Start restano tutti uguali a Windows.

- [x] In una cartella di collegamenti a programmi su un'unità di rete, la colonna Destinazione
      mostra `V:\…` (o `\\server\…` per quelli senza unità), come il campo Destinazione delle
      Proprietà. Provato da Zyruel su un altro PC ("bello, mi piace").
- [ ] Clic sulla destinazione di uno di questi: si apre la sua cartella sull'unità di rete con il
      programma selezionato, senza "Percorso non trovato".
- [ ] `build\collegamenti-prova.exe` su quella cartella: nessun collegamento diverso da Windows.

## 000000000000000000000000000000000. "Vai a" dal percorso (z-f-dev, novembre 2026)

Compilato senza avvisi (MSVC). Provato da Zyruel su un altro PC ("direi che funziona"); Claude
non l'ha provato con lo schermo. Restano da spuntare uno per uno, in z-f-dev (`prove\dev.bat`):

- [ ] Clic sullo spazio vuoto del percorso, Ctrl+L e F4: si apre "Vai a" con il percorso della
  cartella aperta e "\" in fondo, tutto selezionato; sotto, la cartella stessa e le sue
  sottocartelle, subito (vengono dalla lista, niente "caricamento…").
- [ ] Scrivendo subito si cerca su tutto (il percorso selezionato sparisce); Fine e poi lettere:
  si filtrano le sottocartelle; Backspace fino alla "\" precedente: le cartelle del livello sopra.
- [ ] Ctrl+V di un percorso copiato ci va con Invio; un percorso che non esiste: avviso come prima.
- [ ] Con "Vai a" già aperto (Ctrl+P), Ctrl+L o F4 mettono il percorso della cartella aperta;
  Ctrl+P lo richiude.
- [ ] In Questo PC: si apre con il campo vuoto (le cartelle recenti). Dentro uno zip: le
  sottocartelle dello zip aperto.
- [ ] Clic su un segmento o su "›" fa come prima; un secondo clic sul percorso vuoto con il
  pannello aperto lo chiude.
- [ ] Doppio clic sullo spazio vuoto del percorso: "Vai a" si apre e resta aperto (prima della
  correzione il secondo clic lo richiudeva subito).
- [ ] **Misura:** in System32, F4 con F12 aperto: "Vai a: apertura" e "ricerca a ogni tasto"; il
  valore va in ARCHITETTURA ("Vai a", riga "Misurato:"). Non presa da Claude: in questa sessione
  i tasti mandati alla finestra di prova da un altro processo non arrivano.

## 00000000000000000000000000000000. Destinazione mai più larga di Nome (z-f-dev, novembre 2026)

Compilato senza avvisi. Provato da Zyruel su un altro PC con l'anteprima aperta ("decisamente
meglio"); Claude non l'ha provato con il mouse. Restano da spuntare, in una cartella di prova con
collegamenti e la finestra non a tutto schermo:

- [ ] Stringendo e allargando la finestra, Nome e Destinazione crescono e calano insieme (prima
  Nome restava al minimo); a tutto schermo Destinazione è larga quanto scelto, come prima.
- [ ] Bordo Nome|Destinazione: verso destra Nome si allarga e Destinazione si stringe; verso
  sinistra si ferma dove Destinazione sarebbe più larga di Nome.
- [ ] Bordo Destinazione|Data: Data cambia larghezza e lo spazio si divide tra Nome e
  Destinazione; nessun salto al rilascio.
- [ ] Doppio clic sul bordo Nome|Destinazione con percorsi lunghi: si adatta, al massimo a metà.
- [ ] Con l'anteprima aperta (Zyruel: "il pannello deve avere dimensione fissa"): l'anteprima
  tiene la larghezza scelta stringendo la finestra, finché le colonne non sono tutte al minimo;
  nessun bordo delle colonne la allarga o la stringe.
- [ ] Bordo Destinazione|Data verso sinistra: Data si allarga (prima era bloccata) e Destinazione
  cede fino al minimo; Nome non si muove.

## 0000000000000000000000000000000. Una finestra per ogni cartella aperta da fuori (z-f-dev, novembre 2026)

Provato da Claude senza schermo: con l'opzione accesa (simulata col messaggio
`z-f.finestre-separate`) due aperture da fuori hanno aperto due processi nuovi, con l'opzione
spenta una scheda nella principale. Restano:

- [ ] Ctrl+, → Integrazione con Windows: l'interruttore "Una finestra per ogni cartella aperta da
  fuori" si accende e si spegne, la spiegazione si legge per intero a 175%.
- [ ] Con [z]f al posto di Explorer e l'opzione accesa: doppio clic su una cartella del desktop,
  Win+E, "Apri percorso file" da un altro programma. Ognuno apre una finestra nuova, in primo
  piano, con la cartella (e il file selezionato).
- [ ] Interruttore acceso in una finestra aperta con "Nuova finestra": vale subito anche per la
  principale (aprire una cartella da fuori apre una finestra nuova).
- [ ] Chiusa la principale con le finestre separate ancora aperte, l'avvio successivo apre subito
  (non dopo 3 secondi) e diventa la principale.
- [ ] Con l'opzione spenta tutto come prima: una scheda nella finestra esistente.
- [ ] Memoria a confronto, a PC libero: Esplora file aperto come lo apre una persona (Win+E, 10
  volte) e la crescita del processo explorer.exe della shell, contro 10 schede e 10 finestre di
  z-f-dev (`prove\memoria.ps1`). Avviato dall'app di Claude, explorer.exe apre ogni finestra in
  un processo suo anche con `SeparateProcess=0`: quella misura non vale per l'uso normale.

## 000000000000000000000000000000. Correzioni del controllo prima della 1.0.0-beta.4 (z-f-dev, novembre 2026)

I punti del controllo delle modifiche di questa versione: spazio delle unità lente azzerato se la
lettura fallisce (`sidebar.cpp`), avviso quando Proprietà o "Apri con…" non trovano l'elemento
(`shell.cpp`), controllo della cartella dei dialoghi fuori dal turno e `FD_WAIT_MAX` dalla
richiesta (`filedlg.cpp`), niente errore degli appunti da un tentativo superato (`input.cpp`),
icona della griglia in una cartella che si chiama come uno zip e coda delle unità appena
connesse (`thumbs.cpp`), id stabile delle schede per lo zip nello zip (`zipout.cpp`, `app.h`),
coda a parte per le icone di rete e ottiche anche in elenco, barra laterale e menu (`icons.cpp`).

Provato da Claude su z-f-dev, in una cartella di prova con file finti:
- [x] Alt+Invio su un file che la shell non sa trovare (nome con il punto finale, creato con
      `\\?\`): avviso di errore "Impossibile aprire le Proprietà di fine.: non esiste più
      (codice 2)"; F12 "comando della shell: preparazione" 3,1 ms;
- [x] griglia in una cartella vera `x.zip` con due .exe: ognuno ha la sua icona, non quella
      generica;
- [x] due Copia percorso di fila con gli appunti occupati (`appunti-prova occupa 1.6`, tasti
      mandati da uno script): tra la scadenza della prima (nel registro "appunti non scritti",
      1050 ms) e la riuscita della seconda la barra di stato non mostra errori, poi "Percorso
      copiato";
- [x] `dialoghi-prova ritorno` (A e B locali): il dialogo entra in A e poi in B, nessuna conferma
      del nome, nome proposto intatto; notifica sul thread UI al massimo 0,095 ms;
- [x] icone dei .exe in `\\localhost\C$\Windows` (elenco): arrivano dalla coda nuova;
- [x] zip dentro uno zip: si apre come prima;
- [x] registro: solo le voci normali e quelle delle prove; MSVC: zero avvisi.

Da provare a mano (serve un'unità o una rete che sparisce):
- [ ] DVD nell'unità ottica, poi tolto (o un'unità di rete verso un server che si spegne): dopo il
      giro delle voci lente la barra laterale non mostra più spazio libero e barra d'uso, e il
      tooltip non ha più Capacità, Usato, Libero, come un'unità vuota;
- [ ] Proprietà (Alt+Invio) della cartella aperta su un NAS spento: dopo l'attesa della rete,
      l'avviso "Impossibile aprire le Proprietà di …: percorso di rete non trovato (codice 53)" o
      simile, invece di niente;
- [ ] dialogo Apri o Salva con [z]f in una cartella su una condivisione che non risponde, poi
      ritorno con una cartella locale: il dialogo entra subito in quella locale, senza aspettare
      la rete;
- [ ] schede: doppio clic su uno zip dentro uno zip grande e, durante l'estrazione, chiusura di
      una scheda a sinistra: finita l'estrazione si entra lo stesso nello zip interno.

## 00000000000000000000000000000. Zip: zip nello zip e trascinamento misurato (z-f-dev, novembre 2026)

Correzione del controllo del codice (`zipout.cpp`, `drag.cpp`): uno zip dentro uno zip, finita
l'estrazione, si apriva sempre, anche se intanto l'utente era andato altrove; ora solo se è
l'ultima richiesta e la scheda e la cartella sono quelle di partenza (un file si apre sempre).
L'inizio del trascinamento da uno zip (oggetto dei file e immagine, sul thread UI) ora si misura
come quello di molte voci, con la stessa funzione (`dragObjectFor`).

Provato da Claude su z-f-dev, in una cartella di prova con uno zip di 8 MB che contiene uno zip
di 722 MB (estrazione 480 ms):
- [x] doppio clic su interno.zip e attesa: si entra nella copia estratta, come prima;
- [x] doppio clic su interno.zip e subito Backspace: si resta nella cartella superiore anche
      quando l'estrazione finisce (nel registro "zip: estrazione");
- [x] doppio clic su interno.zip e subito Ctrl+T: si resta nella scheda nuova; chiusa quella,
      la prima scheda è ancora nello zip esterno;
- [x] trascinamento di nota.txt da dentro lo zip: parte (immagine e divieto sopra lo zip), F12
      "copia, taglia, trascina, menu (UI)" 4,23 ms; 600 file trascinati e rilasciati nella stessa
      cartella: 1,41 ms, nessun file spostato;
- [x] registro: solo le voci normali e "zip: estrazione"; MSVC: zero avvisi.

Da provare a mano:
- [ ] trascinare da uno zip su un'altra app (il desktop, una cartella di Explorer): la copia
      arriva come prima;
- [ ] doppio clic su due file di fila dentro uno zip: si aprono tutti e due (scelta voluta).

## 0000000000000000000000000000. Proprietà e "Apri con…" sul thread del menu della shell (z-f-dev, novembre 2026)

Correzione del controllo del codice (`shell.cpp`, `menu.cpp`): le Proprietà di una voce sola,
della cartella aperta e delle voci della barra laterale erano `SHObjectProperties` sul thread
dell'interfaccia, che analizza il percorso prima di aprire la finestra (su una condivisione che
non risponde: il timeout di rete, senza traccia nel registro); "Apri con…" era
`SHOpenWithDialog`, modale sul thread dell'interfaccia. Ora passano dal thread del menu della
shell come Shift+Canc e le Proprietà di più voci: i verbi "properties" e "openas" (`shellVerb`, e
`shellVerbPath` per un percorso intero), con la misura in F12 ("comando della shell") e nel
registro oltre 50 ms.

Provato da Claude su z-f-dev, in una cartella di prova con file finti:
- [x] **Alt+Invio** su una cartella e su un file: le Proprietà compaiono (a.txt: 9,7 ms sul
      thread del menu); dal menu dello sfondo, le Proprietà della cartella aperta;
- [x] barra laterale: **Proprietà di Disco locale (C:)** e di un **percorso di rete** sotto Questo
      PC (un NAS: la prima analisi del percorso ha impiegato 22,4 s, nel registro "comando della
      shell lento" e in F12, e intanto l'interfaccia si ridisegnava; poi le Proprietà della
      condivisione). In Questo PC nella lista, Alt+Invio su C:. I preferiti non hanno la voce
      Proprietà;
- [x] **"Apri con…"** su un .txt (8,2 ms sul thread) e su un file senza programma associato: la
      finestra di Windows (OpenWith.exe, un processo a parte) compare; intanto un clic nella lista
      di [z]f seleziona e mostra il tooltip (prima [z]f era fermo finché la finestra restava
      aperta). Il clic la chiude, come fa Windows 11 quando perde il fuoco;
- [x] registro: solo la riga del percorso di rete; MSVC: zero avvisi.

Da provare a mano:
- [ ] **condivisione che non risponde** (server spento o rete staccata): Alt+Invio su una sua
      cartella e Proprietà del percorso di rete o dell'unità di rete nella barra laterale; la lista
      deve rispondere durante l'attesa, poi una riga "comando della shell lento" (se Windows non
      trova il percorso non compare niente, come prima);
- [ ] Proprietà di un'unità ottica senza disco e di un'unità di rete con lettera.

## 000000000000000000000000000. Appunti: Copia percorso e Copia/Taglia nelle caselle su un thread (z-f-dev, novembre 2026)

Correzione del controllo del codice (`text.cpp`, `clipSetText`): il testo si scriveva negli
appunti sul thread dell'interfaccia, che poteva aspettare il programma che li possedeva (se
bloccato), e "Percorso copiato" compariva anche quando gli appunti erano occupati e non si era
copiato niente. Ora scrive un thread, che riprova per un secondo; l'avviso arriva con l'esito, e
un fallimento dice chi tiene gli appunti. Un Taglia non riuscito rimette il testo nella casella,
selezionato. Anche Ctrl+V riprova per un secondo se gli appunti sono occupati.

Provato da Claude su z-f-dev, in una cartella di prova con file finti, con
`prove\appunti-prova.cpp`:
- [x] **Copia percorso** con Ctrl+Shift+C su una voce ("Percorso copiato"), su cinque voci
      ("Percorsi copiati", una per riga) e dal menu col tasto destro; proprietaria degli appunti la
      finestra di [z]f (classe `z-f`); dopo Ctrl+C sui file (oggetto OLE) Copia percorso li
      sostituisce senza ritardi; chiudendo la finestra il testo resta negli appunti;
- [x] **appunti tenuti aperti** 3–5 s: dopo un secondo "Impossibile copiare il percorso: gli
      appunti sono aperti da appunti-prova.exe (codice 5)"; aperti senza finestra: "… da un altro
      programma (codice 5)"; intanto le frecce muovono il cursore. Tenuti aperti 0,6 s: la copia
      riesce (427 ms, nel registro "appunti lenti (testo)");
- [x] **proprietario fermo** 4 s: `EmptyClipboard` aspetta 3,4 s sul thread, la lista risponde e
      alla fine "Percorso copiato" (prima il thread dell'interfaccia sarebbe rimasto fermo 3,4 s);
- [x] **caselle:** barra del percorso (Ctrl+L), rinomina (F2, poi Esc) e "Vai a": Ctrl+C, Ctrl+X e
      Ctrl+V; in "Vai a" Taglia e Incolla rifanno la ricerca. Taglia con gli appunti occupati, nella
      barra del percorso e in "Vai a": il testo torna selezionato (e la ricerca si rifà), con
      "Impossibile tagliare il testo: …";
- [x] registro: solo le voci d'avvio e le righe delle prove; MSVC: zero avvisi.

Da provare a mano:
- [ ] **Desktop remoto** (rdpclip) o un gestore degli appunti (Ditto, la cronologia di Win+V):
      Copia percorso e Ctrl+C nella barra del percorso arrivano negli appunti, e nel PC remoto;
- [ ] Copia percorso con migliaia di voci selezionate (il testo si prepara ancora sul thread
      dell'interfaccia: con 20.000 voci, quanto dura?).

## 00000000000000000000000000. Barra laterale: prima le unità locali, poi rete e ottiche (z-f-dev, novembre 2026)

Correzione del controllo del codice (`sidebar.cpp`): le unità si leggevano tutte nell'ordine
delle lettere, rete e ottiche comprese, e si consegnavano insieme ai nomi dei preferiti. Un'unità
di rete verso un server spento tratteneva etichetta e spazio delle altre unità e il nome di un
preferito appena aggiunto, a ogni ritorno sulla finestra. Ora il thread della barra legge solo le
voci locali (unità fisse, rimovibili, RAM e i preferiti su di esse); unità di rete e ottiche,
percorsi di rete e preferiti su di loro li legge dopo un altro thread, una voce alla volta,
consegnata appena pronta. Intanto si vedono col nome generico ("Unità di rete (G:)") o con quello
della lettura precedente. Un'unità ottica si rilegge solo all'avvio e quando Windows segnala un
disco inserito o tolto, non a ogni ritorno sulla finestra (il DVD si avviava ogni volta).

Provato da Claude senza toccare lo schermo (finestre di prova ridotte a icona), con Y: connessa a
`\\localhost\C$` (tolta alla fine) e un ritardo di 8 s su ogni unità di rete, aggiunto solo nelle
build di prova, per simulare un server spento; le build scrivevano nel registro i nomi della barra
all'arrivo di ogni risultato:
- [x] **build di prima:** C:, D:, Y: e i preferiti arrivano tutti insieme a 8111 ms dall'avvio;
- [x] **build nuova:** C: e D: con nome e spazio e i preferiti locali (anche `C:\Windows`, aggiunto
      per la prova) a 97–153 ms; Y: a 8113–8173 ms, da sola, con nome ("C$ (Y:)") e spazio;
- [x] il percorso di rete (un NAS vero, lento) e il preferito `Y:\Windows` arrivano dopo Y:, sullo
      stesso thread: 3 voci in 23,1 s, senza trattenere niente di locale;
- [x] **ritorno sulla finestra** (`WM_ACTIVATEAPP` mandato alla finestra di prova) durante la
      lettura lenta: le voci locali si rileggono in 7 ms; la lettura lenta non si raddoppia, ne
      riparte una sola alla fine della prima. Un ritorno successivo mostra subito Y: col nome e lo
      spazio letti prima, mentre li rilegge;
- [x] thread delle voci locali: 16,6 ms al primo giro, 7,2 ms ai successivi (nuova riga in F12,
      "barra laterale: voci locali"). Registro: solo le voci d'avvio. MSVC: zero avvisi.

Da provare a mano:
- [ ] **unità di rete vera con il server spento** (o il NAS spento con una lettera connessa): le
      altre unità hanno subito nome e spazio, la chiavetta inserita intanto compare con la sua
      etichetta senza aspettare; l'unità di rete resta "Unità di rete (G:)" (o col nome di prima)
      finché il server risponde o rinuncia;
- [ ] **DVD inserito:** compare con la sua etichetta all'inserimento; tornando su [z]f più volte il
      lettore non si riavvia (Claude non ha potuto mandare `WM_DEVICECHANGE` alla finestra di
      prova, e qui non c'è un lettore ottico);
- [ ] **preferito su una condivisione** aggiunto con Ctrl+D: compare subito col nome della
      cartella; il nome di Windows arriva dopo, senza ritardare quello dei preferiti locali;
- [ ] F12: le righe "barra laterale: voci locali (thread)" e "barra laterale: rete e ottiche
      (thread)", con il numero di voci (non viste a schermo).

## 0000000000000000000000000. Dialoghi Apri/Salva: un salto alla volta (z-f-dev, novembre 2026)

Correzione del controllo del codice (`filedlg.cpp`): tornando a un dialogo due volte di fila con
cartelle diverse partivano due thread sullo stesso dialogo. Il secondo poteva leggere come nome
"vecchio" il percorso messo dal primo, e il suo "Salva" confermare il nome appena rimesso dal
primo: il dialogo Salva avrebbe salvato il file (Apri lo avrebbe aperto). Ora ogni dialogo ha un
thread solo: una cartella arrivata durante un salto aspetta, e vale l'ultima; se il salto in
corso non ha ancora toccato il campo del nome, lo si lascia e si va subito all'ultima. I tempi
del salto sono in `theme.h` (`FD_*`).

Provato da Claude a PC bloccato, senza mouse né tastiera, con il nuovo `dialoghi-prova ritorno`
(`prove\dialoghi-prova.cpp`): apre Salva, consegna la cartella A a z-f-dev (che viene davanti),
torna al dialogo, poi lo stesso con B; scrive le cartelle in cui entra il dialogo e ogni conferma
del nome (bloccata). 13 giri con tempi diversi (gli ultimi 7 con la build del commit):
- [x] B arriva mentre A sta premendo "Salva": il dialogo entra in A e poi in B (B 180 ms dopo A);
- [x] B arriva prima che A tocchi il campo: A lasciato, il dialogo entra direttamente in B,
      235 ms dopo il ritorno (un salto normale: 220 ms);
- [x] sempre: nome nel campo "prova salvataggio.txt", nessuna conferma del nome, dialogo annullato
      alla fine senza salvare. Registro: solo le voci normali dell'avvio. MSVC: zero avvisi.
- La stessa prova con la build di prima non ha mostrato il guasto (17 giri): con cartelle locali
  "Salva" entra nella cartella in pochi ms, e il secondo ritorno più rapido (60 ms) arriva già
  dopo. Con una cartella lenta (rete) la finestra in cui succedeva era più larga.

Da provare a mano:
- [ ] **ritorno rapido** con un dialogo Salva vero (Blocco note, Salva con nome): passa a [z]f,
      cambia cartella, torna al dialogo, subito di nuovo a [z]f su un'altra cartella e di nuovo al
      dialogo. Il dialogo finisce nell'ultima cartella, il nome proposto resta, niente viene
      salvato;
- [ ] **dialogo lento**: lo stesso con una cartella di rete (o un NAS lento) come prima cartella:
      il dialogo finisce comunque nell'ultima e non salva niente;
- [ ] F12, "dialoghi: ultimo salto (thread)": circa 200–250 ms dopo un salto (a PC bloccato
      Claude non ha potuto leggerlo).

## 000000000000000000000000. File di [z]f mai sovrascritti prima della lettura (z-f-dev, novembre 2026)

Correzione del controllo del codice (`settings.cpp`, `settingsSave`): con il disco lento il
thread veloce della barra laterale può non aver ancora letto `impostazioni.txt` quando l'utente
cambia qualcosa (l'attesa all'avvio è al massimo 100 ms), e il salvataggio riscriveva il file con
i valori predefiniti. Lo stesso per `preferiti.txt` (Ctrl+D, trascinamento), `viste.txt` (griglia)
e `annulla.txt` (opzione accesa nei primi istanti). Ora nessuno si scrive prima della lettura;
dopo, il cambiamento si unisce a quello che c'era.

Provato da Claude con una build di prova che ritarda di 4 s la lettura del thread veloce e, subito
dopo l'attesa d'avvio, fa dal codice i cambiamenti dell'utente (dimensione del testo +1,
animazioni spente e riaccese, colonna Dimensione 120, Ctrl+D, griglia), con `APPDATA` in una
cartella di prova (file con dimensione 16, barra laterale 300, colonna Data 200, menu completo e
aggiornamenti spenti, tre preferiti, due griglie); finestra ridotta a icona, chiusa per PID:
- [x] **Prima:** impostazioni tornate ai predefiniti (barra laterale 210, colonna Data 150, menu
      completo e aggiornamenti riaccesi), preferiti ridotti alla sola cartella nuova, viste alla sola griglia nuova.
- [x] **Dopo:** dimensione 14, animazioni accese e colonna Dimensione 120 (le scelte dell'utente,
      anche quella tornata al predefinito), tutto il resto dal file; quattro preferiti (il nuovo in
      fondo); tre griglie.
- [x] Primo avvio (senza `preferiti.txt`) con Ctrl+D prima della lettura: le cartelle predefinite
      e, in fondo, quella aggiunta.
- [x] Avvio normale (senza ritardo): nessun file riscritto. `settingsMarkStart` costa 0,02–0,04 ms
      all'avvio; `prove\avvio.bat` non usato (la differenza è sotto la sua risoluzione).
- [x] Registro: solo le voci normali dell'avvio. MSVC: zero avvisi.

Da provare a mano:
- [ ] `annulla.txt`: con "Annulla dopo il riavvio" accesa nel file, accenderla di nuovo nelle
      opzioni nei primi istanti di un avvio lento non deve cancellare la cronologia (non provato:
      serve un disco lento vero o la build di prova).
- [ ] Su un avvio a freddo vero (PC appena acceso, antivirus), Ctrl+rotellina appena compare la
      finestra: al riavvio le altre impostazioni sono ancora quelle di prima.

## 00000000000000000000000. File solo online: tooltip e anteprima (z-f-dev, novembre 2026)

Correzione del controllo del codice (`tooltip.cpp`, `readInfo`): la risoluzione di un'immagine
si leggeva aprendo il file anche quando è solo online, e così OneDrive lo scaricava. Ora, come già
per i .lnk, il contenuto si legge solo se il file è sul disco (`onlineOnly`).

Sul PC OneDrive non è configurato: la prova la fa `prove\online-prova.cpp`, che crea una radice
di sincronizzazione finta con l'API Cloud Files di Windows (la stessa di OneDrive) e conta ogni
scaricamento con il processo che lo chiede. I segnaposto hanno gli attributi dei file solo online
di OneDrive (`0x00401620`: offline, recall on data access, sparse, reparse point).

Provato da Claude con z-f-dev in `build\online-*` (cartelle di segnaposto: .png, .jpg, .exe,
.lnk, .ico), confrontando con la build di prima:
- [x] **Prima:** solo aprendo la cartella (anteprima aperta, cursore sul PNG) il PNG si scarica;
      col cursore sul JPEG si scarica anche quello (1 MB) e l'anteprima poi lo mostra come
      immagine. 2 scaricamenti, tutti da z-f-dev.exe.
- [x] **Dopo:** anteprima su tutte e cinque le voci, tooltip sul JPEG, griglia grande: nessuno
      scaricamento. Anteprima e tooltip mostrano tipo, dimensione, date e "solo online", senza la
      risoluzione; icone generiche del tipo (quelle che dà la shell, come Explorer).
- [x] **Icone per file** (.exe, .lnk, .ico): `SHGetFileInfo` sul segnaposto (icona della lista,
      icona grande dell'anteprima e della griglia) non scarica niente, la shell dà l'icona
      generica (`online-prova passi`, 0 scaricamenti in 15 chiamate). Nessuna modifica al codice.
- [x] Registro: niente di nuovo (all'avvio `0x8002`, normale; alla chiusura per PID un
      `messaggio lento 0x0010` da 25,8 ms, che il registro ha già 17 volte da prima). MSVC: zero
      avvisi.

Restano a Zyruel (serve OneDrive vero, con un account):
- [ ] In una cartella di OneDrive con foto "solo online" (icona della nuvola in Esplora file):
      cursore sulle foto con Alt+P aperto e tooltip fermo su una foto. Le foto restano con la
      nuvola (non diventano "disponibili su questo dispositivo") e l'anteprima mostra "Solo
      online: si legge solo aprendolo".
- [ ] Lì stesso, un .exe o un .lnk solo online: icona generica, nessuno scaricamento (la radice
      finta non è registrata presso la shell come quella di OneDrive: da confermare che la shell
      si comporti uguale).

## 0000000000000000000000. Miniature: zip, percorsi e unità di rete (z-f-dev, novembre 2026)

Tre correzioni del controllo del codice (`thumbs.cpp`, `preview.cpp`, `icons.cpp`): le miniature
dentro uno zip si rifanno sempre dallo zip (`thRestart`), i percorsi di rete di Questo PC hanno
l'icona di cartella anche in griglia e nell'anteprima (`iconSource`, `iconDrive`), le icone delle
unità di rete e ottiche in Questo PC vanno sui thread della rete.

Provato da Claude con z-f-dev in una cartella di prova, confrontando con la build di prima
(stesso codice senza le correzioni), griglia grande, F12 aperto:
- [x] **Zip con 1.200 foto** (640×480), scorrendo fino in fondo e poi di nuovo in cima: la RAM si
      riempie (89–92 MB, circa 480 miniature) e le prime voci si rifanno. Prima: 456 richieste
      alla cache di Windows (cioè alla shell, dentro lo zip) e 1.656 decodifiche WIC solo
      scendendo. Dopo: nessuna richiesta alla cache di Windows, 1.692 decodifiche WIC scendendo e
      2.943 a giro finito (1,28 ms l'una), tutte le miniature visibili al ritorno.
- [x] **Questo PC in griglia, con un percorso di rete che risponde lento** (una condivisione via
      VPN): prima l'icona del percorso di rete arrivava dopo circa 20 s, le icone delle unità
      aspettavano in coda dietro di lei e l'anteprima restava su "caricamento…"; poi, aprendo
      una cartella in griglia, le sue sottocartelle avevano l'icona della condivisione. Dopo:
      icona di cartella subito, unità subito, anteprima con le informazioni del percorso di rete
      e l'icona di cartella; le sottocartelle hanno l'icona normale.
- [x] Registro: solo "attesa vsync lunga" (debito noto). MSVC: zero avvisi.
- Non provato: le icone delle unità di rete e ottiche sui thread della rete, perché qui non c'è
  un'unità con la lettera di rete o un lettore ottico.

Restano a Zyruel (servono una rete vera):
- [ ] **NAS spento o condivisione irraggiungibile**, con un percorso di rete in Questo PC
      ("Aggiungi un percorso di rete"): Questo PC in griglia mostra subito l'icona di cartella
      per il percorso di rete; aprendo subito dopo una cartella di foto locale, le miniature
      arrivano senza aspettare. Le cartelle della griglia non prendono l'icona della condivisione.
- [ ] **Anteprima (Alt+P) su quel percorso di rete** in Questo PC: icona di cartella, le
      informazioni del percorso di rete, nessuna attesa.
- [ ] **Unità di rete scollegata (Z:)** in Questo PC in griglia: le altre unità hanno subito la
      loro icona; aprendo poi una cartella locale in griglia, le miniature non aspettano Z:.

## 000000000000000000000. Aggiornamento automatico e riapertura delle schede (1.0.0-beta.3, novembre 2026)

Provato da Claude, senza toccare lo schermo, con `prove\aggiornamenti-prova.py` (server locale al
posto di GitHub) e una copia dell'exe in una cartella di prova, ridotta a icona:
- **giro completo:** controllo 18 ms, download dell'exe e della firma 25 ms, impronta e firma
  giuste, scambio fatto (`z-f.exe` uguale alla versione nuova, `z-f.old.exe` la vecchia);
- **riavvio:** col messaggio di "Aggiorna" la finestra salva le schede, esce e si riapre con
  l'exe nuovo (`--riavvio`, ridotta a icona com'era), sulla stessa cartella (dal titolo);
  `riavvio-<pid>.txt` letto e cancellato, `z-f.old.exe` cancellato;
- **tre schede, la seconda attiva:** riaperte con la seconda attiva; il file scritto da un
  riavvio ha le tre schede con cartella e voce selezionata, anche quelle mai aperte;
- **firma falsa:** rifiutata, exe non toccato (ora con un avviso anche nel controllo automatico);
- **server spento:** errore dopo 2 s, tutto sul thread, exe non toccato;
- **cinque giri di fila:** tutti riusciti (controllo 16–20 ms); quando la copia precedente era
  ancora bloccata, lo scambio ha usato `z-f.old-<numero>.exe`, come previsto;
- **a vista** (finestra di prova separata): pallino blu su ≡, "Aggiorna a [z]f 1.0.0-beta.3" con
  la freccia blu, l'anello di punti durante un controllo (portato alla misura delle icone: a 12 px
  sembrava più piccolo delle frecce), le frecce circolari dopo un errore, la sezione "Avvio e
  aggiornamenti" nelle opzioni e la freccia accanto alla versione in fondo al pannello. La
  spiegazione di "Aggiornamenti automatici" era troppo lunga e tagliava l'ora: accorciata.
Ogni tanto un controllo finiva in "errore di rete 12030" (connessione interrotta): era il server
di prova, che chiudeva la connessione di colpo e Windows mandava un reset. Con la chiusura pulita
(ora in `prove\aggiornamenti-prova.py`) sei giri su sei sono riusciti, con l'exe di rilascio della
1.0.0-beta.3 firmato con la chiave vera, arrivato identico. Il registro scrive anche il motivo di
ogni errore ("aggiornamento: <motivo>"); un controllo fallito si ripete da solo un'ora dopo.
MSVC senza avvisi (a `/W4` solo quello noto di `zip.cpp`). L'exe cresce di circa 70 KB (1049 KB
invece di 979): metà è il codice nuovo, il resto il compilatore che con la build unica inserisce
diversamente funzioni già esistenti. winhttp.dll e bcrypt.dll si caricano solo al primo controllo.

Restano a Zyruel:
- [ ] **La spunta:** con GitHub vero, dopo la pubblicazione della 1.0.0-beta.3, un clic sulla
      versione nel menu ≡ mostra l'anello e poi la spunta ("[z]f … è l'ultima versione").
- [ ] **Riapri le schede all'avvio:** accendila, apri tre schede, chiudi [z]f e riaprilo (senza
      cartella, per esempio dal menu Start): tornano le tre schede, con quella attiva di prima.
      Poi riaprilo su una cartella (doppio clic sul desktop): le schede di prima più quella.
- [ ] **Spegnimento:** con l'opzione accesa e [z]f aperto, riavvia Windows: le schede tornano.
- [ ] **Avvio:** `prove\avvio.bat` (a PC libero): il contenuto deve comparire come prima (64–80 ms),
      con l'opzione spenta e accesa.
**Con GitHub vero** (Release 1.0.0-beta.3 pubblicata): una copia della versione precedente con
l'aggiornamento ha trovato la beta.3 (controllo 0,6 s), l'ha scaricata (2,7 s), ha verificato la
firma e l'ha installata identica a quella pubblicata; "Aggiorna" ha riaperto la finestra al suo
posto, in primo piano, sulla stessa cartella. Difetto trovato: la versione nuova provava a
cancellare `z-f.old.exe` mentre la vecchia stava ancora uscendo, e non ci riusciva (lo toglieva
solo all'avvio dopo, e "Aggiornato a …" non compariva). Corretto su `dev` per la prossima
versione: aspetta che il processo di prima sia uscito (al massimo `UPD_OLD_WAIT`, sul thread).
Provato con una beta.4 finta: copia tolta subito dopo il riavvio.

- [ ] **Il primo aggiornamento vero:** dalla 1.0.0-beta.3 alla successiva, quando ci sarà: il
      pallino su ≡, "Aggiorna a [z]f …", il riavvio di tutte le finestre (anche una aperta con
      "Nuova finestra") con le loro schede e posizioni, poi "Aggiornato a [z]f …".

## 00000000000000000000. Percorsi di rete sotto Questo PC (z-f-dev, novembre 2026)

Provato da Claude in z-f-dev con un percorso di rete verso un NAS: la voce compare sotto le
unità con lo spazio libero e la barra, il tooltip dice "Percorso di rete" e il percorso, il clic
la apre ed evidenzia la voce; in "Questo PC" nella lista c'è con tipo "Percorso di rete" e
l'anteprima mostra icona e spazio; "Altre opzioni…" apre il menu di Windows (prima della
correzione non compariva). Quel giorno la condivisione era lentissima (50–60 s per rispondere):
le unità avevano comunque subito il loro spazio, e il menu di Windows è comparso dopo 46 s.
Restano a Zyruel:

- [ ] **Con la condivisione veloce** (in rete locale): spazio e menu di Windows arrivano
      subito.
- [ ] **Un percorso di rete nuovo** creato con "Aggiungi un percorso di rete" mentre [z]f è
      aperto: compare tornando su [z]f.
- [ ] **NAS spento:** la voce resta (senza spazio), [z]f non si blocca, aprirla dà l'errore
      solito.
- [ ] **Condivisione del NAS:** se lo aggiungi come percorso di rete, compare anche lui.

## 0000000000000000000. Barra laterale a tutta altezza, filtro nella barra di stato, barra dei tempi (0.28, novembre 2026)

Chiesto da Zyruel, con uno screenshot: la barra laterale arriva in fondo alla finestra, libera, e
si allarga trascinandone il bordo come l'anteprima; il filtro sta a sinistra nella barra di stato,
allineato alla lista (prima prova: era nell'ultima riga della barra laterale, Zyruel la vuole tutta
libera); i contatori di cartelle e file vanno a destra nella barra di stato; i tempi
misurati hanno una barra loro sopra la barra di stato, solo con l'opzione "Barra dei tempi"
(`barra_tempi`). I bordi di colonne, barra laterale e anteprima ora passano dallo stesso codice
(`colDragStart`/`colDragEnd` in `listmouse.cpp`, `panelEdgeAt` in `app.cpp`); quello vecchio
dell'anteprima è stato tolto. DESIGN, "Barra laterale", "Barra di stato", "Barra dei tempi".

**Già provato da Claude** in z-f-dev, cartella di prova (25/11/2026): bordo della barra laterale
trascinato e doppio clic (torna a 210), selezione accanto ai contatori, interruttore della barra dei
tempi e tooltip dei tempi, Ctrl+B (il filtro va a sinistra della finestra). MSVC senza avvisi; nel
registro solo le due voci normali dell'avvio.

**Da provare:**
- [ ] trascinare il bordo dell'anteprima: si comporta come prima (Esc torna a prima, doppio clic 360);
- [ ] Esc mentre si trascina il bordo della barra laterale: torna alla larghezza di prima;
- [ ] finestra stretta con barra laterale larga e anteprima aperta: cede prima l'anteprima, poi le colonne;
- [ ] barra laterale con molte voci: scorre fino in fondo alla finestra e l'ultima voce si raggiunge;
- [ ] filtro: scrivere nella lista, × per toglierlo, tooltip sul campo;
- [ ] un avviso lungo (per esempio un errore) con molti selezionati: finisce in "…" prima della selezione;
- [ ] con la barra dei tempi accesa, il pannello F12 sta sopra le due barre, non le copre;
- [ ] trascinare file sulla barra laterale in basso: il rilascio sui preferiti funziona fino all'ultima voce.

**Preferiti rientrati, riordinabili, cestino** (chiesto da Zyruel, stessa sera; `sidebar.cpp`,
`favMouseDown`/`favMouseMove`/`favMouseUp`, `favTrashAt`, `drawFavGap` in comune con il rilascio
da fuori). La conferma del cestino è sul posto e non un dialogo, per il principio 6.
**Già provato da Claude** in z-f-dev con i preferiti veri (copia salvata prima e rimessa dopo,
sha1 uguale): rientro, cestino al passaggio del mouse, una voce trascinata tra due preferiti, primo clic sul cestino "Togli?" in rosso, secondo clic tolto con l'avviso, clic semplice
su Desktop apre la cartella.
**Da provare:**
- [ ] Esc durante il trascinamento di un preferito: resta dov'era; Esc con "Togli?": torna il cestino normale;
- [ ] "Togli?" e poi il mouse su un'altra voce: la conferma sparisce;
- [ ] Ctrl+clic e clic centrale su un preferito: nuova scheda;
- [ ] rilasciare un preferito sotto l'ultimo o sopra il primo: va in fondo / in cima;
- [ ] doppio clic sul cestino: non toglie (il secondo clic di un doppio clic non conta), serve un clic in più.

## 000000000000000000. Unità sotto Questo PC nella barra laterale (0.28, novembre 2026)

Chiesto da Zyruel: niente più titolo "Unità"; "Questo PC" è un nodo dell'albero, come in
Explorer, con le unità rientrate sotto (`SIDE_INDENT`) e la freccia ˅/› a destra. Compilato con
MSVC senza avvisi (in `build\z-f-dev.exe`: `build\z-f.exe` era aperto). La prova la fa Zyruel.

- [ ] aspetto a 175%: spazio tra i preferiti e Questo PC (`GAP_L`), rientro delle unità, freccia
  allineata a quella di "Preferiti", nomi lunghi delle unità di rete tagliati prima dello spazio libero;
- [ ] clic sulla freccia: il ramo si chiude e si apre, ricordato al riavvio (`unita_aperte`);
  clic sul nome: va a Questo PC; Ctrl+clic e clic centrale: nuova scheda;
- [ ] a ramo chiuso, stando su C:\…: Questo PC con la pillola più chiara; a ramo aperto la
  pillola è sull'unità;
- [ ] tasto destro sulla freccia: il menu di Questo PC; tooltip sulla freccia: "Comprimi /
  Unità di Questo PC";
- [ ] preferiti vuoti: la riga "Ctrl+D aggiunge" non si sovrappone a Questo PC;
- [ ] trascinare file su un'unità rientrata: l'indicatore è sulla riga giusta;
- [ ] barra laterale più alta della finestra: lo scorrimento arriva fino all'ultima unità.

## 00000000000000000. Colonna Destinazione dei collegamenti (0.27, novembre 2026)

Chiesto da Zyruel: accanto ai collegamenti (.lnk) la loro destinazione, in
grigio, e un clic porta lì ("Apri percorso file"). Scelta con Zyruel: una colonna "Destinazione",
solo nelle cartelle con collegamenti (`lnk.cpp` lettore, `links.cpp` colonna e thread; README,
"collegamenti"; DESIGN, sezione 7).

**Già provato, senza interfaccia** (`prove\collegamenti-prova.cpp`): tutti i 177 collegamenti del PC
(Desktop, menu Start, Recenti) più una cartella di prova (file accentato, cartella, `%windir%`,
`C:\`, `\\server\condivisione\…` che non esiste) letti uguali a IShellLink di Windows. A caldo 0,09 ms
a collegamento contro 0,86 di IShellLink; la prima lettura dopo l'avvio del PC circa 5 ms
(disco e antivirus), sul thread. `computer.lnk` (un computer della rete, `\\computer`): niente
destinazione, come per Windows. MSVC senza avvisi (anche `/W4`, a parte quello già noto di
`zip.cpp`, in `docs/da-sistemare.md`).

**Provato da Zyruel** in z-f-dev sulla cartella di prova (25/11/2026): tutto a posto; la riga di F12
era troppo lunga e passava sotto il valore (accorciata, e ora nel pannello F12 un'etichetta che
non ci sta finisce in "…"). **Resta da provare** su un altro PC, in una cartella con molti collegamenti a percorsi di rete e a un'unità di rete, confrontando con le proprietà di Windows. L'elenco
completo, per riferimento (cartella di prova: si crea come sotto):
- [ ] la colonna c'è solo dove ci sono .lnk; le destinazioni compaiono subito, in grigio; con la
  finestra stretta cede per prima Destinazione, poi Data e Dimensione; il nome tiene il suo minimo;
- [ ] mouse sopra la destinazione: manina e fondo chiaro solo attorno al testo (sullo spazio
  vuoto della colonna la freccia normale), tooltip col percorso intero
  e "Clic: apri percorso file · clic centrale: nuova scheda";
- [ ] clic sulla destinazione: va nella cartella che la contiene, con la destinazione selezionata;
  su `C:\`: Questo PC con l'unità selezionata; su `\\server\…` non raggiungibile: avviso di
  errore, nessun blocco; clic centrale: nuova scheda in background;
- [ ] doppio clic sulla destinazione (per abitudine): va lì una volta sola, non apre il
  collegamento né la voce che arriva sotto il mouse;
- [ ] trascinare partendo dalla destinazione: parte il rettangolo, nessuna navigazione;
  Ctrl+clic e Shift+clic sulla destinazione: solo selezione;
- [ ] intestazione: bordo Nome|Destinazione trascinabile, doppio clic adatta al percorso più
  lungo, larghezza salvata (`colonna_destinazione`); clic su "Destinazione" ordina dalla A (senza
  destinazione in fondo), "Ordina per destinazione" nel tooltip;
- [ ] menu sul collegamento: "Apri percorso file"; tooltip della riga e anteprima: riga
  "Destinazione";
- [ ] creare, rinominare o cambiare un collegamento mentre la cartella è aperta: la destinazione
  si aggiorna senza lampeggiare; F12 "collegamenti (thread)";
- [ ] `build\z-f-log.txt`: nessuna voce nuova.

Per creare la cartella di prova (PowerShell):
`$sh = New-Object -ComObject WScript.Shell; $l = $sh.CreateShortcut("$PWD\prova.lnk"); $l.TargetPath = "C:\Windows\notepad.exe"; $l.Save()`

## 0000000000000000. Zip come cartelle (0.26, novembre 2026)

Aprire uno zip apriva Esplora file. Ora uno zip
si apre dentro [z]f, con un lettore nostro (`zip.cpp`, `zipout.cpp`; README, "archivi zip come
cartelle"; DESIGN, sezione 7).

**Già provato, senza interfaccia** (`prove\zip-prova.cpp`, dati di prova creati con
`Compress-Archive` e `System.IO.Compression`): elenco ed estrazione di zip "Optimal", "Fastest" e
"NoCompression" (175 file, cartelle annidate, nomi accentati, binari incomprimibili, immagini,
notepad.exe): tutti i file identici agli originali (sha1). Zip64 con 70.000 voci: directory centrale
in 17 ms. Un registro da 138 MB: 244 ms (538 MB/s), identico. Nomi nella codepage 850 senza il bit
UTF-8: letti giusti (àèìòù, ü). Zip troncato e file che non è uno zip: "Archivio zip non valido o
non supportato". Un byte rovinato: il CRC lo trova (notepad.exe non estratto, niente file a metà).
Percorsi dentro lo zip, sottocartelle (menu "›") e lettura parziale in memoria (anteprima).
MSVC senza avvisi.

**Da provare a mano** (l'accesso allo schermo non c'era: prima volta in z-f-dev, `prove\dev.bat`,
in una cartella di prova con degli zip; `prove\prova.zip` va bene, oppure se ne crea uno grande con
`Compress-Archive`):
- [ ] doppio clic e Invio su uno zip: si apre come cartella; percorso a segmenti con il nome dello
  zip; "›" sul segmento dello zip elenca le sue cartelle; ↑ e Backspace tornano alla cartella dello
  zip con lo zip selezionato; ← → nella cronologia; titolo della scheda;
- [ ] colonne Data e Dimensione (cartelle col totale), ordinamento, filtro, griglia, nascosti
  attenuati; F12: "zip: directory centrale" con le voci, barra di stato coi tempi di lettura;
- [ ] tooltip su file (tipo, dimensione, risoluzione delle immagini) e cartelle (contenuto,
  dimensione) dentro lo zip;
- [ ] anteprima: immagine, testo, cartella dentro lo zip; lo zip stesso selezionato in una
  cartella normale mostra il contenuto con il totale nel piede;
- [ ] griglia: miniature delle immagini dentro lo zip, icone per estensione degli altri (anche .exe);
- [ ] doppio clic su un file: si apre col suo programma (file in `%TEMP%\z-f\<n>`); su uno zip
  dentro lo zip: si apre in [z]f; F12 "zip: ultima estrazione";
- [ ] Ctrl+C su file e cartelle, poi Incolla in [z]f e in Esplora file: arrivano i file;
  avviso "Estratto e copiato: …";
- [ ] trascinamento verso Esplora file e verso un'altra scheda di [z]f (copia); rilasciare prima
  della fine di un'estrazione lunga (un file grande): niente trascinamento, niente rilascio sbagliato;
- [ ] F2, Canc, Ctrl+X, Ctrl+V, Ctrl+Shift+N dentro lo zip: avviso "L'archivio zip è in sola
  lettura"; menu: Taglia, Rinomina, Elimina, Incolla e Nuova cartella disattivati; niente Proprietà,
  Apri con, terminale, "Altre opzioni…"; Shift+tasto destro apre il nostro menu; trascinare file
  dentro lo zip: cursore di divieto;
- [ ] uno zip cifrato o con un altro metodo (7-Zip, "Deflate64" o "LZMA"): avviso e Esplora file
  selezionato sulla voce (`explorer.exe /select,` con un percorso dentro lo zip: da verificare che
  Esplora file lo accetti);
- [ ] F5 dopo aver cambiato lo zip da fuori (rilegge); zip cancellato mentre è aperto: F5 torna
  alla cartella che lo conteneva;
- [ ] preferito dentro uno zip: nome, clic, nessun rallentamento della barra laterale;
- [ ] `build\z-f-log.txt` dopo le prove: "zip: elenco lento" / "zip: estrazione" solo oltre 100 ms.

**Scelta da confermare:** i nomi senza il bit UTF-8 si leggono nella codepage OEM del sistema (come
li scrivono Esplora file e 7-Zip), non sempre in 437 come dice la specifica: in Italia (850) le
lettere accentate comuni sono uguali, ma À, Á e simili no. Confermata da Zyruel (26/11/2026): si
resta con la OEM, come Esplora file.

## 000000000000000. Incolla da uno zip (0.26, novembre 2026)

Un file copiato con Explorer da dentro uno zip non si incollava in [z]f.
Negli appunti, in quel caso, non c'è `CF_HDROP` ma solo i formati della shell: "Shell IDList
Array", "FileGroupDescriptorW", "FileContents", "AsyncFlag". [z]f guardava solo `CF_HDROP`.
Lo stesso vale per telefoni e allegati.

Ora:
- "Incolla" (Ctrl+V, il nostro menu, il menu completo della shell) è attivo anche con "Shell IDList
  Array" o "FileGroupDescriptorW" (`clipHasFiles`, `shell.cpp`: `IsClipboardFormatAvailable`, non
  apre gli appunti);
- sul thread di Incolla (`runPaste`), se gli appunti non hanno percorsi, l'oggetto degli appunti va
  all'`IDropTarget` della cartella con l'effetto copia, o sposta dopo Taglia: lo stesso codice del
  rilascio, spostato in `shellDropInto` (`drag.cpp`). Con `CF_HDROP` non cambia niente
  (`IFileOperation`, annullabile). Gli elementi senza percorso non si annullano, come nel
  rilascio da uno zip. Dopo Taglia gli appunti restano.

**Trovato provando:** la prima versione chiamava `Drop` (S_OK in 2 ms), ma non estraeva niente.
Negli appunti Explorer dice con il formato "AsyncFlag" che l'operazione può andare in asincrono: la
shell tornava subito da `Drop` e poi richiamava il nostro thread (12 messaggi in circa 100 ms), ma
il thread era già uscito. `SyncData`, l'involucro che già toglieva `IDataObjectAsyncCapability` al
trascinamento, ora nasconde anche "AsyncFlag": la shell estrae dentro `Drop` (124 ms per un file)
e il thread finisce dopo.

**Strumento di controllo dello schermo:** ogni clic o tasto mandato da solo svuota gli appunti,
anche un semplice testo. Dentro un `computer_batch` non succede. Per questo il Ctrl+V finale l'ha
fatto Zyruel.

Provato (z-f-dev, Zyruel): copiato da uno zip con Explorer, Ctrl+V nella cartella di prova: il
file arriva e la lista si aggiorna. Registro: nessun messaggio lento. MSVC senza avvisi.

Da provare a mano:
- [ ] da uno zip, una cartella con dentro dei file, e più voci insieme;
- [ ] "Incolla" del tasto destro (nostro menu) e del menu completo della shell, anche "Incolla" su
      una cartella (finisce lì dentro: niente avviso, perché quanti elementi non si sa);
- [ ] da uno zip con un nome che c'è già: la finestra dei conflitti di Explorer;
- [ ] Taglia dentro uno zip e Incolla in [z]f;
- [ ] da un telefono (MTP) e da un allegato di Outlook, se capita;
- [ ] trascinare da uno zip in [z]f funziona come prima (anche il rilascio passa da `SyncData`).

## 00000000000000. Ultimi punti del controllo del codice (0.25, novembre 2026)

Compilato con MSVC senza avvisi (`/W3` e `/W4 /Zs`). Misure su questo PC (Ryzen 9 7900), con due
programmi a parte che rifanno gli stessi passi di [z]f: `prove\shell-verbi.cpp` e
`prove\vai-a-misura.cpp`.

**1. Shift+Canc e Proprietà di più voci: spostate sul thread del menu della shell.** Prima
`shellVerb` costruiva l'oggetto della shell e chiamava `InvokeCommand` sul thread dell'interfaccia.
Il registro non lo vedeva (escludeva "delete" e "properties"). Misure, cartella di prova con file vuoti:

| voci | prima del comando, locale | prima del comando, `\\localhost\c$` |
|---|---|---|
| 20 | 8 ms (40 la prima volta) | 14 ms |
| 500 | 38 ms | 160 ms |
| 2.000 | 131 ms | 680 ms |
| 20.000 | 1.620 ms (ParseDisplayName 530, QueryContextMenu 1.100) | 8.100 ms (ParseDisplayName 8.070) |

Poi, dentro `InvokeCommand`: Shift+Canc con 20.000 voci conta gli elementi per 1,4 s prima della
conferma, quindi 3,1 s dal tasto alla finestra; risposto "Sì", l'eliminazione stessa gira lì dentro
(21 s per 20.000 file vuoti). Proprietà torna subito (3 ms), la finestra arriva dopo 7 s dal suo
thread. La condivisione era SMB verso questo stesso PC: una rete vera costa di più.

Ora `shellVerb` manda la richiesta al thread del menu (`smSend`, lo stesso del menu completo:
`smRunVerb`), che prepara e invoca il comando con [z]f proprietaria dei dialoghi. Copia e Taglia
di unità e Questo PC (gli unici che passavano ancora dalla shell) vanno sullo stesso thread; gli
appunti si consegnano a Windows dal thread, come per le voci del menu. Un clic durante l'attesa
non annulla il comando (il menu sì). Spariti `shellMenuFor` e `invokeOn`. Nel registro "comando
della shell lento" oltre `LOG_MENU` (tempo sul thread); in F12 la riga "comando della shell:
preparazione (thread)".

Provato (z-f-dev, 20.000 file finti): Ctrl+A, Shift+Canc, F12 premuto mezzo secondo dopo si apre
subito; la conferma "Eliminare questi 20.000 elementi definitivamente?" compare dopo circa 3 s;
"No" non elimina niente. F12: preparazione 1.598 ms (20000 voci), messaggio più lento 6,4 ms.
Alt+Invio: Proprietà con "20.000 file", preparazione 1.547 ms, messaggio più lento 6,6 ms. Ctrl+C
su Disco locale (C:) in Questo PC: negli appunti l'oggetto della shell (Shell IDList Array,
FileDrop). Registro: solo le due righe "comando della shell lento" (1.598 e 1.547 ms, sul thread).

**Successo provando:** la prima versione di `shell-verbi.cpp` chiudeva la prima finestra che
compariva; con 20.000 voci era quella che conta gli elementi, non la conferma, e i 20.000 file
finti della cartella di prova sono stati eliminati davvero. Corretto: aspetta la finestra con "No".

**2. "Vai a": nessun cambiamento, i numeri non lo chiedono.** Caso peggiore teorico: 30 cartelle
attive con 3.000 sottocartelle ciascuna (`DIR_MAX`), 90.000 nomi. Ogni ricerca costa 2,5–5,8 ms
(la peggiore è una ricerca che sta tutta nel percorso delle cartelle, come "users\utente": ogni
sottocartella passa al confronto col percorso intero). Con le cartelle che arrivano una alla volta
la somma è 40–70 ms, ma divisa in 30 messaggi, ognuno sotto la soglia `LOG_GOTO` (8 ms), con
frame in mezzo. Con i dati veri di questo PC (preferiti e 20 recenti: 25 cartelle, 248 nomi):
0,04 ms. Il registro non ha mai segnato "vai a: ricerca lenta". A campo vuoto, il caso più comune
all'apertura, le cartelle che arrivano non entrano nemmeno nella ricerca.

**"messaggio lento 0x0006" all'avvio: guardato, è Windows.** Nel registro 3 avvii su 91 (25,5,
58,9 e 77,6 ms; l'ultimo con dentro "0x0281" 66,9 ms), anche con la 0.24, sempre 40–85 ms dopo
"--- avvio", cioè in `ShowWindow` (`main.cpp`), prima di creare il device D3D. [z]f non gestisce
`WM_ACTIVATE` né `WM_IME_SETCONTEXT` (0x0281): il tempo è in `DefWindowProc`, la prima volta che
una finestra del thread prende il fuoco. Misurato con `prove\ime-avvio.cpp` (finestra nascosta che
prende il fuoco solo nel suo thread: nessuna finestra sullo schermo), 20 processi:
- il primo fuoco costa 14–17 ms (il secondo 0,5 ms): Windows carica e avvia il Text Input
  Framework del thread (`textinputframework.dll`, `CoreMessaging.dll`, `CoreUIComponents.dll`,
  `wintypes.dll`, `Oleacc.dll`, `OLEAUT32.dll`…), che serve a IME, pannello delle emoji, dettatura;
- caricare prima quelle DLL su un altro thread toglie solo circa 3 ms (12–14 ms restano): il resto
  è l'avvio per thread, che si fa solo sul thread della finestra;
- i casi lenti del registro sono le volte in cui questo passo va lento (probabilmente l'attesa di
  un altro processo di Windows, come TextInputHost, o la cache del disco fredda). Disattivare il
  Text Input Framework per [z]f lo toglierebbe, ma toglierebbe anche IME ed emoji: no.

**Fatto (z-f-dev, dopo la 0.25, deciso con Zyruel): l'attivazione va dopo il primo frame.**
`main.cpp` mostra la finestra con `SW_SHOWNOACTIVATE`, crea D3D e disegna il primo frame, poi
chiama `SetForegroundWindow`. Il costo del primo fuoco resta, ma a contenuto già visibile. Se chi
avvia [z]f chiede la finestra ridotta a icona o ingrandita (`STARTUPINFO`), si mostra come prima.
F12 ha la riga nuova "attivazione (dopo il primo frame)".

Misurato con `prove\avvio.bat` e l'opzione nuova `--prova-avvio`, che scrive nel registro
"prova avvio: contenuto" e "prova avvio: primo fuoco" (con 1 se la finestra è in primo piano) e
chiude la finestra. Confronto con una build uguale ma con l'attivazione dentro `ShowWindow`, 16
avvii alternati per build:

| | contenuto visibile (mediana, min–max) | primo fuoco | in primo piano |
|---|---|---|---|
| prima (in `ShowWindow`) | 74 ms (56–120) | 29 ms (16–65), prima del frame | 16 su 16 |
| dopo il primo frame | **51 ms** (37–63) | 11,5 ms (10–14), dopo il frame | 16 su 16 |

Provato anche con `start /min` (resta ridotta a icona, non prende il primo piano), `start /max`
(ingrandita, in primo piano) e normale (in primo piano). Nell'uso normale (tre avvii con
`prove\dev.bat`) il registro ha solo le voci solite dell'avvio. Con `--prova-avvio` la chiusura
subito dopo l'avvio registra "messaggio lento 0x0010" (30–100 ms, con dentro a volte "0x0086"):
è la chiusura a metà del passaggio WARP → GPU, solo nella prova.

**Successo provando:** un comando di prova con `start` scritto male (le virgolette passate da Bash)
ha fatto comparire più volte l'errore di Windows "Impossibile trovare '\\'". Fermato; per queste
prove ora c'è un .bat.

Da provare a mano:
- [ ] [z]f al posto di Explorer: Win+E, una cartella aperta dal desktop, "Apri percorso file" da
      un'altra app: la finestra nuova viene in primo piano (con la 0.25 in `build\z-f.exe` questo
      non c'è ancora: serve il prossimo rilascio);
- [ ] una seconda apertura mentre [z]f è già aperto: la cartella va nella finestra esistente come prima.

**3. Ritocchi da 1 px: fatti come proposto, decisi con Zyruel.** Niente cambia sullo schermo.
- **segnaposto delle icone** (`render.cpp`, il rettangolo che c'è prima che arrivi l'icona): le
  proporzioni sono in `theme.h`: `PH_TAB_Y = 0.12`, `PH_TAB_W = 0.45`, `PH_TAB_H = 0.2`
  (linguetta della cartella); `PH_BODY_Y = 0.25`, `PH_BODY_CUT = 0.3` (corpo: da 0,25, alto il
  lato meno 0,3); `PH_FILE_X = 0.15`, `PH_FILE_W = 0.7` (foglio). Stesse formule di prima;
- **`+ 1` in `drawCrumbs`** tolto. Tutte le posizioni dei segmenti sono intere, quindi il testo ci
  sta lo stesso (provato: i segmenti lunghi come "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" interi);
  cambia solo un segmento troncato perché più largo della barra, che finisce 1 px prima;
- **`ROW_EXTRA`** resta `lroundf(9 × scala)`: con `px()` (per difetto) le righe sarebbero 1 px più
  basse a 150% (14 → 13), 175% (16 → 15, il monitor di Zyruel) e 250% (23 → 22). È l'eccezione
  voluta, scritta in `theme.h`, in `layout` e in DESIGN.md §3: anche così cade su pixel interi.

**4. Tasti ricavati dalla tabella delle scorciatoie: fatto, deciso con Zyruel.** Prima i tasti
stavano in tre posti: `onKey`, `WM_SYSKEYDOWN` (Alt+frecce, Alt+Invio, Alt+P) e `kShortcuts`
(confrontati a mano: coincidevano). Ora in `kShortcuts` ogni tasto che esegue solo un comando si
scrive `K(comando, modificatori, tasto)`:
- `onKey` lo cerca lì (`shortcutCmd`) e lo esegue con lo stesso codice del menu: lo `switch` di
  `menuExec` ora è `menuRun(id, CmdCtx)`, con `case` nuovi per i comandi senza voce (`M_BACK`,
  `M_FWD`, `M_UP`, `M_EDIT_PATH`, `M_SIDEBAR`);
- `WM_SYSKEYDOWN` e `WM_SYSCHAR` passano a `onKey` i tasti Alt della tabella (gli altri, come
  Alt+F4, restano a Windows);
- il nome mostrato si ricava dal tasto (`keyName`: "Ctrl+Shift+N", "Alt+↑", "Canc", "Ctrl+,");
- in `onKey` restano scritti a mano solo i tasti che dipendono dal contesto: frecce, PagSu e
  PagGiù, Home e Fine, Backspace, Esc, Ctrl+Spazio, Ctrl+Tab, Ctrl+1…9, Ctrl+A, Ctrl+V (incolla
  anche un percorso o un testo, il menu solo i file), Ctrl +/−/0, F12, Menu;
- strada facendo, due copie diventate una: F5 e "Aggiorna" dei menu (`refreshNow`, che riprova
  anche il watcher come faceva solo F5) e mostra/nascondi la barra laterale (`sidebarToggle`, era
  scritta in tre posti).

Cambia una cosa: i modificatori devono essere esattamente quelli della tabella. Prima
Ctrl+Shift+W chiudeva la scheda, Ctrl+Shift+T ne apriva una, Ctrl+Invio e Shift+Invio aprivano la
voce, Shift+F5 ricaricava; ora non fanno niente. Alt+P con un menu aperto ora chiude il menu (come
ogni tasto) invece di aprire anche l'anteprima, e con "Vai a" aperto non fa niente (come Alt+frecce).

Provato (z-f-dev, cartella di prova con 2 cartelle e 3 file): Ctrl+B, Alt+P, Ctrl+Shift+3,
Ctrl+Shift+6, Invio, Alt+↑, Alt+←, Alt+→, Ctrl+T, Ctrl+Shift+W (niente, giusto), Ctrl+W, Ctrl+G,
F4, F2, Ctrl+, (nelle scorciatoie i nomi ricavati sono uguali a prima), Ctrl+Shift+N con Ctrl+Z,
Canc con Ctrl+Z e Ctrl+Shift+Z, Shift+Canc (conferma, "No"), Alt+Invio su un file; menu del tasto
destro con Invio, Ctrl+X, Ctrl+C, Ctrl+Shift+C, F2, Canc, Alt+Invio, Shift+F10; tooltip di ← con
"Alt+← · tasto laterale indietro". Registro: nessun messaggio lento, solo due "attesa vsync lunga"
(39 e 59 ms, il debito noto).

Da provare a mano:
- [ ] Shift+Canc su pochi file in una cartella vera: conferma come prima, "Sì" li elimina e la
      lista si aggiorna;
- [ ] Shift+Canc su una cartella di rete vera, con molte voci: l'interfaccia resta libera finché
      compare la conferma;
- [ ] Proprietà di più voci da menu (tasto destro → Proprietà) e da Alt+Invio;
- [ ] Ctrl+C e Ctrl+X su un'unità in Questo PC, poi Incolla in Explorer (ci si aspetta che
      Explorer rifiuti come prima: è la stessa operazione della shell);
- [ ] i tasti non provati qui: Ctrl+P, Ctrl+L, F5, Ctrl+D (aggiunge e toglie dai preferiti),
      Ctrl+Shift+2 e 4, Ctrl+Y, Ctrl+C/X/V su file, Ctrl+Shift+C; e che Alt+F4 chiuda ancora la
      finestra e Alt+Spazio apra il menu di sistema.

## 0000000000000. Inizio di un trascinamento con molte voci (z-f-dev, novembre 2026)

Compilato con MSVC senza avvisi (`/W3` e `/W4`). L'oggetto della shell per la selezione costa
circa 25 µs a voce sul thread dell'interfaccia (20.300 voci: 550 ms prima che il trascinamento
parta). Da 500 voci (`kDragFastItems`, circa 12 ms con l'oggetto della shell) si trascina un
oggetto con i soli percorsi (`CF_HDROP`), lo stesso di Copia e Taglia (`hdropObject` in
`shell.cpp`). Senza immagine Windows disegnava un quadrato bianco: ora è l'icona grande del primo
elemento, per estensione dalla lista delle icone di sistema, con "Sposta in …" sotto. Sotto le 500
voci non cambia niente.

**Trovato provando:** a fine operazione su 20.000 file, `onFileJob` dimenticava i totali delle
cartelle percorso per percorso (60.000 chiamate): "messaggio lento 0x800E 76 ms". Ora, oltre 256
percorsi, `dirSizeForgetAll` li dimentica tutti in un colpo; i totali si rifanno in background.

Provato (cartella di prova, 20.000 file vuoti): selezionati tutti, trascinati sulla scheda di
un'altra cartella (si apre dopo mezzo secondo) e rilasciati nella lista. Inizio del trascinamento
10,8 ms (F12, "copia, taglia, trascina, menu"), immagine con l'icona dei .txt e "Sposta in molti",
tutti i 20.000 file spostati, nessun messaggio lento nel registro. Lo spostamento in sé (88 s per
20.000 file, sul thread di `IFileOperation`) è quello di Explorer.

Da provare a mano:
- [ ] 600 file trascinati su altre app: il desktop, una cartella di Explorer, una cartella zip,
      un allegato di Outlook o del browser (ricevono solo i percorsi);
- [ ] Alt+trascina 600 file sul desktop: crea i collegamenti;
- [ ] 400 file: tutto come prima (immagine della shell con le miniature).

## 000000000000. Menu completo della shell su un thread suo (z-f-dev, novembre 2026)

Compilato con MSVC senza avvisi (`/W3` e `/W4`). `QueryContextMenu` (carica tutte le estensioni)
e `TrackPopupMenuEx` girano su un thread STA con una finestra nascosta sua (`shell.cpp`, "menu
completo della shell, su un thread suo"): non figlia né posseduta da quella di [z]f, perché tra
thread diversi Windows legherebbe le code dell'input. Il menu prende il primo piano per la sua
finestra e lo restituisce a [z]f, se nel frattempo non si è passati a un'altra app. Elimina,
Incolla, Rinomina, Copia e Taglia tornano al thread UI con i percorsi del clic; le altre voci le
esegue la shell sul thread, con [z]f proprietaria dei dialoghi. Il thread resta vivo; se è occupato
(un comando lungo) il menu successivo ne usa un altro, fino a 4. Chiudendo [z]f mentre un comando
della shell è in corso, la finestra sparisce e il processo aspetta, come per le copie.

Provato (cartella di prova):
- Shift+clic destro su un file: il menu compare, scuro; "Invia a" si apre con le sue icone
  (sottomenu disegnato dall'estensione, ora sulla finestra del thread);
- "Elimina" dal menu: il file va nel Cestino tramite [z]f, e Ctrl+Z lo rimette ("Annullata
  l'eliminazione di tre.txt: di nuovo in rinomina");
- "Rinomina" dal menu: la casella si apre sulla voce, "quattro" + Invio la rinomina; la tastiera
  torna a [z]f;
- menu dello sfondo: Incolla (grigio senza file negli appunti) e Aggiorna in cima; Esc lo chiude e
  una lettera dopo filtra;
- registro: "menu della shell lento" 453 ms la prima volta, poi 180–200 ms, **sul thread**; nessun
  "messaggio lento" del thread dell'interfaccia (prima quei tempi erano un blocco dell'interfaccia).

Provato da Zyruel col mouse (tutto passa):
- [x] clic destro e subito un clic altrove (entro un quarto di secondo): il menu non compare dopo;
- [x] menu aperto, clic su un'altra app: il menu si chiude e [z]f non si riprende il primo piano;
- [x] una voce lunga di un'estensione (7-Zip, "Estrai tutto…") e intanto un altro menu: compare;
- [x] "Proprietà" e "Apri con" dal menu: i dialoghi stanno sopra [z]f;
- [x] "Copia come percorso" e Copia su un'unità in Questo PC: gli appunti restano anche chiudendo [z]f.

Nota sulle prove a schermo: lo strumento che controlla lo schermo, a ogni chiamata nuova, riporta
davanti la finestra principale e così chiude il menu (che ha il primo piano su un'altra finestra);
dentro una stessa chiamata il menu resta. Con il mouse vero non dovrebbe succedere: è la prima
cosa da guardare (apri il menu, muoviti tra le voci e i sottomenu senza fretta).

## 00000000000. Chiusura del controllo del codice (z-f-dev, novembre 2026)

Compilato con MSVC senza avvisi, ora anche con `/W4 /Zs` (i 5 di prima corretti). Le voci di
`prompt-dopo-il-controllo.md`, una per una.

**Bug piccoli** (tutti verificati nel codice prima di correggerli):
1. contatori col filtro: `applyFilter` segna i contatori da rifare. Provato: filtro "alfa" (2 cartelle,
   1 file), poi Ctrl+V di "beta" (1 cartella, 2 file, stesse 3 voci): ora 1 e 2, prima restava 2 e 1;
2. Ripeti vecchio dopo un annullamento: le operazioni nuove non aspettano (niente da far aspettare);
   un annullamento finito va tra i Ripeti solo se nel frattempo non ne è stata registrata un'altra
   (`UH.pushes`);
3. totale di una cartella vecchio: se durante un conto arriva `dirSizeForget` per quella cartella o
   una dentro, il conto non si scrive e si rifà;
4. tooltip: `CoInitializeEx` sul suo thread. Provato: il tipo del file c'è;
5. dialoghi Apri/Salva: chiudendo (o nascondendo, con un'operazione in corso) la finestra che era
   l'ultima usata, la memoria condivisa passa alla finestra di [z]f più in alto. Se l'HWND salvato
   non esiste più (processo caduto) decide la più in alto, la stessa per tutti;
6. "Usa [z]f al posto di Explorer": percorso dell'exe anche oltre `MAX_PATH`, e senza percorso
   l'attivazione si rifiuta ("Impossibile modificare il registro");
7. menu "›" e "Vai a": la chiave dell'ordinamento poteva superare il buffer con nomi che si
   scompongono in più lettere (Hangul). Ora un limite unico (`kKeyMax`). Provato: cartelle coreane,
   ordine naturale (나무1, 나무2, 나무10) e menu "›";
8. ricarica in più: `refreshAgain` si azzera a ogni lettura nuova e al cambio di scheda; una
   ricarica superata non azzera più `refreshInFlight` di quella nuova;
9. rinomina dopo una ricarica: la voce si ritrova col suo nome. **Trovato in più:** dopo "Nuova
   cartella" il controllo della rinomina girava dopo `beginRename` e rimetteva la casella sulla voce
   che aveva il cursore prima: confermando, si sarebbe rinominata quella. Provato: cursore su
   uno.txt, Ctrl+Shift+N, "nuova", Invio: si rinomina la cartella nuova, uno.txt resta. Ctrl+Z:
   "Annullata la creazione di nuova";
10. `gfx.cpp`: `QueryInterface`, `GetAdapter`, `GetBuffer` controllati, niente perdite se manca la factory;
11. anteprima: `pvCancelled` ovunque; il thread senza pompa dei messaggi resta così (vive per una
    richiesta sola), con il perché nel commento.

**Thread dell'interfaccia:**
- Ctrl+V (caselle e lista) legge gli appunti su un thread e il testo arriva qualche millisecondo
  dopo. Provato nel filtro e nella casella del percorso;
- "Usa [z]f al posto di Explorer" scrive il registro e avvisa Explorer su un thread; il tempo va
  sempre nel registro ("al posto di Explorer"). **Non provato** (non si tocca il registro al tuo posto);
- icone su due thread (per estensione; dal file o dall'unità). **Trovato provando:** i due thread
  insieme facevano fallire la prima `SHGetFileInfo` (F12: "2 fallite", cartelle col segnaposto e
  preferiti senza icona); ora il secondo aspetta il primo. Dopo: 4 caricate, 0 fallite, contenuto
  visibile a 56 ms, prima icona a 72 ms;
- dimensione delle cartelle iterativa invece che ricorsiva. Provato: 3000 livelli (percorso di
  6124 caratteri), colonna "5,9 KB", giusto.

**Coerenza:** scorciatoie dei tooltip (schede, ← → ↑, ▯, percorso, filtro, "Vai a") da `kShortcuts`
con `shortcutsOf`, separatore " · " ovunque (anche date e piedi dell'anteprima), soglie del registro
e i due tempi di lavoro in `theme.h`, "primi 256 KB" da `kPvTextMax`, il contenuto di una cartella
con una funzione sola (`contentLine`) per tooltip e anteprima. Nel pannello delle opzioni Indietro e
Avanti sono ora due righe, e Backspace sta con la cartella superiore.

**Misure:** il percorso a segmenti (rifatto a ogni frame) costa 0,00 ms, peggiore 0,01 (nuova riga
in F12): niente cache. Le miniature per cella restano a 0,23 ms in WinSxS (misura di prima): sotto
il millesimo di frame che conta, niente cache per ora.

Da provare a mano:
- [ ] Ctrl+V nella rinomina e in "Vai a" (il testo arriva e "Vai a" cerca);
- [ ] "Usa [z]f al posto di Explorer" acceso e spento dalle opzioni: l'interruttore si aggiorna
      all'avviso, nel registro una riga "al posto di Explorer" con il tempo;
- [ ] due finestre di [z]f, chiudi quella usata per ultima: un dialogo Apri salta nella cartella
      dell'altra;
- [ ] icone di .lnk ed .exe su una condivisione lenta: le altre icone arrivano lo stesso.

Lasciati a te (lavori grossi o da decidere):
- menu completo della shell su un thread suo (debito in CLAUDE.md);
- inizio di un trascinamento con molte voci (oggetto della shell come per gli appunti, magari solo
  sopra un certo numero);
- Shift+Canc e Proprietà di più voci: il registro non le misura (aprono finestre di Windows), serve
  una misura apposta;
- "Vai a" che rifà la ricerca a ogni cartella in arrivo: il registro non ha mai segnato "ricerca lenta";
- da decidere: segnaposto delle icone, `+ 1` in `crumbs.cpp`, `ROW_EXTRA` con `lroundf`; tasti
  scritti in `onKey` a parte da `kShortcuts`.

## 0000000000. Velocità: misure e memoria delle miniature (z-f-dev, novembre 2026)

Compilato con MSVC senza avvisi (con `/W4` ne restavano 5, non 4: il filtro nascondeva quello di `std::fill`; ora zero, sezione 00000000000). Cosa dicevano le misure:
- **"Present lento" a ogni avvio:** è il primo `Present` sulla swapchain nuova della GPU, dentro il
  passaggio WARP → GPU (`messaggio lento 0x8002`, che lo contiene). Nelle 30 volte del log cade
  sempre 4–9 ms prima del `0x8002`. Costa 18–26 ms al driver e non è un rallentamento nuovo: ora il
  log lo chiama "primo Present GPU" e CLAUDE.md lo mette tra le voci normali;
- **costi a ogni frame**, misurati a parte con lo stesso codice (`prove\costi-frame.cpp`, il
  minimo su 2000 giri): `selCount` su 30.734 voci 0,020 ms; `dirSizeGet` per 60 righe 0,003 ms; `iconFor`
  per 60 righe 0,001 ms. Con un frame di 0,12–0,32 ms (misurato in [z]f) e un obiettivo sotto
  1 ms non valgono una cache: lasciati così;
- **menu completo della shell** (250–550 ms): è Windows che carica le estensioni, a ogni apertura.
  È un'opzione e il tempo è già misurato. Toglierlo dal thread dell'interfaccia vorrebbe dire
  costruire e mostrare il menu su un thread suo, con una finestra sua: lavoro grosso, segnato tra i
  debiti tecnici di CLAUDE.md con il piano.

Cambiato:
- **miniature fuori dalla griglia:** quando nessuna scheda è più in griglia si libera anche la RAM
  (fino a 96 MB), non solo la VRAM;
- **tempi nuovi in F12:** "miniature: creazione dell'atlante (UI)" (nel registro se supera 16 ms)
  e "primo caricamento di WIC", ora misurato anche quando lo carica la griglia e non l'anteprima;
- il pannello F12 ha posto per 128 righe (erano 80, senza controllo: con tutte le sezioni aperte
  ci si avvicinava).

Non provati a schermo (stavi usando il PC):
- [ ] **memoria:** in una cartella di foto, griglia (Ctrl+Shift+2), scorri un po', poi elenco
      (Ctrl+Shift+6): in Gestione attività la memoria di z-f-dev scende; tornando in griglia le
      miniature ricompaiono subito;
- [ ] **F12 in griglia:** le righe "creazione dell'atlante" e "primo caricamento di WIC" hanno
      valori sensati (se la prima immagine la apre la griglia);
- [ ] **log:** all'avvio "primo Present GPU" al posto di "Present lento".

## 000000000. Coerenza: colori e misure da theme.h (z-f-dev, novembre 2026)

Compilato con MSVC senza avvisi (con `/W4` ne restavano 5, non 4: il filtro nascondeva quello di `std::fill`; ora zero, sezione 00000000000). Nessun `rgba(` fuori da
`theme.h` e nessun `floorf(N * s)`/`px(N)` con un numero:
- **colori nuovi in `theme.h`:** `C_CLOSE` e `C_ON_CLOSE` (pulsante chiudi), `C_KNOB_ON`
  (pallino dell'interruttore acceso), `C_SEL_EDGE_PARENT` (pillola dell'unità che contiene la
  cartella aperta), `C_ICON` e `C_ICON_HIDDEN` (icone e immagini; i file nascosti al 43%). `C_BG`
  non è più ricopiato a mano nello sfondo di D3D e della finestra;
- **misure nuove:** pulsanti e margini delle schede, barra laterale, tooltip, pannello delle
  opzioni e interruttore, campo del filtro, entrata della lista, finestra nuova e minima, font
  delle icone, menu da tastiera; il resto usa nomi che c'erano già (`GAP_*`, `EDGE`, `FIELD_PAD`);
- **hover con dissolvenza** anche per + e × delle schede e × del filtro (erano gli ultimi accesi
  di colpo);
- DESIGN.md diceva `PATH_BAR` 1,6: è 1,45 (il valore di `theme.h`, quello che si vede).

I valori sono gli stessi di prima, quindi a scala 100% l'aspetto non deve cambiare. A scale diverse
cambia al massimo un pixel in tre punti:
- il margine dal bordo della finestra di tooltip e menu, che prima erano 4 e 2 px non scalati;
- il pallino dell'interruttore, ora con lo stesso rientro sui due lati;
- la × e il + delle schede, prima con frazioni di pixel.

Claude non l'ha confrontato a schermo: mentre apriva la 0.21 per il confronto stavi usando il PC, quindi ha lasciato schermo e tastiera. Da guardare,
z-f-dev accanto a `build\z-f-0.21.exe --new-window`:
- [ ] **barra del titolo:** schede, separatori, titolo, + e × (ora sfumano entrando e uscendo col
      mouse), pulsante chiudi rosso col simbolo bianco;
- [ ] **barra laterale:** voci, spazio libero, barra d'uso, pillola dell'unità più chiara, "Ctrl+D
      aggiunge" con i preferiti vuoti;
- [ ] **barra di stato:** campo del filtro e la sua × (sfuma);
- [ ] **opzioni (Ctrl+,):** misura del pannello, interruttori (il pallino), "13 px", carattere, ricerca;
- [ ] **tooltip e menu** vicino al bordo destro e in basso della finestra;
- [ ] **file nascosti** con l'icona attenuata, in elenco, in griglia e nell'anteprima di una cartella.

## 00000000. Niente duplicati (z-f-dev, novembre 2026)

Compilato con MSVC senza avvisi (con `/W4` ne restavano 5, non 4: il filtro nascondeva quello di `std::fill`; ora zero, sezione 00000000000). Un posto solo per:
- **"unità lenta?"** (`slowPath`): erano tre versioni diverse. Ora "Vai a", miniature e dimensione
  delle cartelle trattano allo stesso modo rete e unità ottiche: la dimensione delle cartelle non
  conta più da sola un CD/DVD, come già non faceva con la rete;
- **"solo online?"** (`onlineOnly`): il tooltip non riconosceva `RECALL_ON_OPEN`, anteprima e
  miniature sì; ora dicono la stessa cosa;
- **tipo di unità** (`driveKind`): la lista di Questo PC diceva "Rimovibile" e "CD/DVD", la barra
  laterale "Unità rimovibile" e "Unità ottica". Ora ovunque come Windows: "Unità rimovibile",
  "Unità ottica", "Disco RAM";
- **× del filtro** (`fltClearAt`), **suggerimento del campo vuoto** (in `drawField`: "Vai a una
  cartella…", "Cerca…"), **scorciatoie del menu della shell** (da `kShortcuts`), **righe per
  scatto della rotellina** (`ui::WHEEL_LINES`), **icone della lista** (`ui::LIST_ICON`),
  `ICON_ATLAS`, `kInstanceProp`, le misure del testo (`ui::FONT_*` senza alias);
- tolti: il collegamento a `dxgi.lib` (nessuna funzione usata), `IC_SEARCH`, `IC_SETTINGS`, un
  commento orfano.

`build_zig.bat` ora scrive `build\z-f-dev.exe`, non più `build\z-f.exe` (la build MSVC che usi),
e usa gli shader precompilati se ci sono. Non provato: zig non è installato su questo PC.

Claude l'ha provato a schermo su z-f-dev: × del filtro (si evidenzia e svuota), "Vai a una
cartella…" e "Cerca…", menu della shell sullo sfondo con "Incolla Ctrl+V" e "Aggiorna F5",
Questo PC ("Disco locale"). Log pulito.

- [ ] **una chiavetta o un lettore DVD**: in Questo PC e nella barra laterale lo stesso nome,
      "Unità rimovibile" / "Unità ottica";
- [ ] **un file OneDrive solo online**: tooltip, anteprima e miniatura dicono tutti "solo online".

Resta doppio, di proposito: `kShaderSrc` in `gfx.cpp` è una copia di `shader.hlsl` per quando gli
shader non sono precompilati (zig, o MSVC senza fxc). Toglierla vorrebbe dire non avere shader
senza fxc. Se cambi `shader.hlsl`, va cambiata anche lei.

## 0000000. Il thread dell'interfaccia non tocca il disco (z-f-dev, novembre 2026)

Compilato con MSVC senza avvisi (con `/W4` ne restavano 5, non 4; ora zero, sezione 00000000000). Claude l'ha provato
a schermo su z-f-dev, in cartelle di prova nello scratchpad (una con 20.300 voci finte). Misurato:
- **Ctrl+C e Ctrl+X** su 20.300 voci: da 545 ms a **5,8 ms** (F12, riga "copia, taglia, trascina,
  menu"). Negli appunti 20.300 file e, dopo Taglia, "Preferred DropEffect" = sposta. Copia e Incolla
  di beta.txt, Taglia e Incolla di gamma.txt in un'altra cartella: giusti, e dopo Taglia gli appunti
  si svuotano;
- **ingresso di un trascinamento** con 20.300 voci: da **10.352 ms a 37 ms**. Era
  `DragQueryFileW`, che riparte dall'inizio dell'elenco per ogni voce. Lo stesso codice serve
  Incolla e il rilascio da altre app. Movimento sopra una destinazione: da 2,5 ms a ogni
  `DragOver` a un controllo solo quando la destinazione cambia;
- **watcher** aperto dal suo thread: un file creato da fuori compare da solo;
- **log** scritto da un thread: le righe arrivano (avvio, vsync, le misure nuove);
- **Nuova finestra** (≡) si apre nella stessa cartella; **Cartella delle impostazioni** apre la
  scheda su `%APPDATA%\z-f` (la cartella la crea un thread).

Non provati a schermo:
- [ ] **unità di rete lenta o appesa**: entrare e uscire da una sua cartella non blocca più
      l'interfaccia (il watcher apre e chiude sul suo thread); **Disconnetti** stando dentro
      l'unità funziona ancora (il thread della disconnessione aspetta che il watcher abbia chiuso);
- [ ] **antivirus o disco lento al salvataggio**: le impostazioni si salvano senza bloccare (la
      coda ha un lock suo, separato da quello delle scritture);
- [ ] **chiavetta inserita e tolta più volte di fila**: la barra laterale si aggiorna con un solo
      thread alla volta;
- [ ] **"Annulla dopo il riavvio" con un'operazione enorme** (Ctrl+A e sposta migliaia di file):
      F12 "annulla: registrazione" resta sotto pochi ms;
- [ ] **Incolla in Explorer** di file copiati in [z]f, e Taglia in [z]f + Incolla in Explorer
      (sposta). Ora negli appunti ci sono solo `CF_HDROP` e l'effetto, non più l'oggetto della
      shell: è quello che usano molte app, ma va visto con Explorer, il desktop, una zip, Outlook.

Resta da decidere:
- [ ] **inizio di un trascinamento** con migliaia di voci: l'oggetto della shell costa 550 ms per
      20.300 voci (25 µs a voce: 100 voci 2,5 ms). Si può costruire come per gli appunti, senza
      disco, ma l'immagine trascinata e alcune destinazioni (zip, Outlook, il desktop con Alt)
      potrebbero comportarsi diversamente. Lasciato così finché non decidi.

Durante le prove Claude ha messo in `%APPDATA%\z-f\recenti.txt` cartelle di prova e
`\\localhost\C$`, poi le ha tolte (copia di prima nello scratchpad). La build di prova della GPU ha
riscritto `impostazioni.txt` con `griglia_dimensione=2`: se prima avevi un'altra dimensione della
griglia, rimettila con Ctrl+Shift+3/4.

## 000000. Correzioni del controllo del codice (z-f-dev, novembre 2026)

Compilato con MSVC senza avvisi (`/W3`; con `/W4` restano solo i 6 avvisi che c'erano già).
Claude l'ha provato a schermo su z-f-dev in una cartella di prova nello scratchpad. Ha funzionato:
- **menu completo di Windows → Elimina** su un file: va nel Cestino da [z]f; Ctrl+Z "Annullata
  l'eliminazione di alfa.txt: di nuovo in prova-zf". **Incolla su una cartella** (Shift+tasto
  destro su `destinazione`): "Copiato in destinazione"; Ctrl+Z "Annullata la copia di beta.txt: nel Cestino";
- **annullamento con il file in uso** (tenuto aperto in esclusiva da un altro processo), Annulla
  nella finestra "File in uso" di Windows: "Impossibile annullare la copia: beta.txt: interrotto;
  si può riprovare" in rosso; chiuso il file, Ctrl+Z "Annullata la copia di beta.txt: nel Cestino";
- **chiusura durante un'operazione** (finestra "Sostituisci o ignora" aperta): la finestra di
  [z]f sparisce, quella di Windows resta, il processo è vivo; scelto "Ignora", il processo esce;
- **GPU persa**, simulata in una build a parte (F9 chiamava `gpuLost`, non è nel codice): tre volte,
  in elenco e in griglia; tutto ridisegnato, testo e icone compresi. Log: "GPU persa", "GPU
  ripristinata" (device nuovo in 58 ms sul thread, 36 ms sul thread dell'interfaccia);
- **↑ su `\\localhost\C$`**: la freccia è spenta, Alt+↑ non fa niente (prima andava a `\\localhost` e dava errore);
- **cartella aperta eliminata da fuori**: [z]f risale e l'avviso "La cartella è stata eliminata o
  spostata" ora resta (prima spariva appena arrivava la cartella superiore);
- log: solo le voci solite (avvio, `0x8002`, "attesa vsync lunga", il menu della shell).

Trovato e corretto durante la prova (c'era già prima): Annulla nella finestra "File in uso" mentre
si eliminava faceva credere a [z]f che l'eliminazione fosse avvenuta (la shell risponde con un
esito positivo). Ora conta il disco: valeva anche per Canc, che metteva in cronologia
un'eliminazione mai fatta.

Corretti ma non provati a schermo (servono cose che Claude non può fare, o file veri):
- [ ] **rotellina di un touchpad di precisione** nel menu che scorre, in "Vai a" e nelle opzioni:
      prima non si muovevano, gli scatti parziali si perdevano;
- [ ] **monitor a 144 Hz**: le animazioni della barra delle schede, dei pulsanti del titolo e della
      barra laterale hanno la stessa velocità delle altre (prima andavano 2,4 volte più veloci);
- [ ] **animazioni spente**: anche lo scorrimento con la rotellina salta subito, senza scivolare;
- [ ] **scheda lasciata a metà caricamento** (una cartella di rete lenta): tornandoci, la voce da
      selezionare (la cartella da cui si è saliti, `/select`) è selezionata;
- [ ] **doppio clic sul bordo della colonna Dimensione** con "Dimensione delle cartelle nella
      colonna": la colonna si allarga anche per i totali delle cartelle ("12,3 GB");
- [ ] **avvisi**: dopo "Percorso copiato" (azzurro), un errore di caricamento è rosso;
- [ ] **griglia con migliaia di foto** (oltre 96 MB di miniature): le celle visibili non restano
      vuote; un `.ico` senza miniatura mostra la sua icona;
- [ ] **"Vai a"** dopo aver cambiato cartella in una scheda: le sottocartelle della cartella di
      prima si aggiornano (prima restavano quelle di quando era aperta);
- [ ] **riaprire [z]f mentre si chiude** con un'operazione in corso: torna la stessa finestra.

## 00000. Dimensione delle cartelle e salto dei dialoghi (0.21)

Compilato con MSVC senza avvisi; log pulito. Claude ha provato a schermo:
- **dimensione delle cartelle**: tooltip su una cartella grande con il totale in byte e il numero di file;
  colonna Dimensione di C:\ riempita man mano, parziali attenuati;
  le cartelle illeggibili (Config.Msi, Recovery) restano vuote invece di "0 B". F12: tutto C:\ in
  14,4 s sul thread, frame a 0,12 ms (peggiore 0,35);
- **dialoghi**, con `build\dialoghi-prova.exe` (`prove\dialoghi-prova.cpp`) e z-f-dev su una cartella di prova: 4 giri di Apri, Salva, Apri classico, Salva classico e scelta cartella, tutti e
  20 col programma ancora vivo; Apri e Salva entrano nella cartella, Salva tiene il nome
  proposto, i classici sono nella cartella, la scelta cartella resta dov'era. F12: notifica UI al
  massimo 0,138 ms, salto 290 ms sul thread.
  Attenzione: la prima versione mandava "Apri" appena il dialogo compariva e il programma cadeva
  (COMDLG32, accesso non valido). Ora aspetta la vista dei file più 150 ms.

Restano a te (lo strumento di Claude non lascia cliccare nei dialoghi):
- [ ] **salto al ritorno**: con un dialogo aperto, passa a [z]f, cambia cartella, torna al dialogo
      (clic o Alt+Tab): deve entrare nella cartella nuova. Tornando senza cambiare cartella in [z]f
      non deve muoversi (anche se nel dialogo sei andato altrove);
- [ ] dialoghi veri: Blocco note (Apri, Salva con nome), Paint, un browser (carica file), Office o
      LibreOffice, un programma vecchio; nessuno deve chiudersi o cadere;
- [ ] con due finestre di [z]f agisce l'ultima usata; con l'opzione spenta non succede niente;
- [ ] colonna Dimensione su un disco lento o una cartella enorme: il PC resta reattivo; ordinando
      per Dimensione le cartelle già contate si ordinano per totale; dopo una copia dentro una
      sottocartella il suo totale si aggiorna.

## 0000. Primi commenti (0.20)

Compilato con MSVC senza avvisi. Claude l'ha provato a schermo su z-f-dev, collegando
`\\localhost\D$` su Z: col nuovo "Connetti" e togliendola col nuovo "Disconnetti". Ha funzionato:
- **Questo PC** nella barra laterale, con l'icona della shell (prima mancava: `SHGetFileInfo`
  non accetta `::{CLSID}`, ora il thread delle icone passa dal PIDL); clic ci va ed è evidenziata;
  menu Apri, Apri in nuova scheda, Connetti unità di rete…, Proprietà;
- **Proprietà** apre Impostazioni › Sistema › Informazioni sul sistema (`ms-settings:about`: il
  verbo "properties" sul CLSID non apriva niente). Su un thread: sul thread UI costava 222 ms;
- **Connetti unità di rete…** dal menu dello sfondo di Questo PC e dalla barra laterale: il
  dialogo di Windows davanti a [z]f; connettendo, [z]f apre Z:; Annulla non dà avvisi. Dopo il
  dialogo il fuoco andava a un'altra finestra (il dialogo è di un altro thread): ora [z]f torna davanti;
- **nome**: "D$ (Z:)" e non l'etichetta del disco; tooltip con "Percorso di rete \\localhost\D$";
- **Disconnetti** stando dentro Z: (menu della barra laterale): si passa a Questo PC, avviso
  "Unità Z: disconnessa", Z: sparisce da lista e barra; `net use` vuoto. "Disconnetti" c'è anche
  nel menu di Z: in Questo PC;
- **opzione "Menu completo di Windows col tasto destro"**: accesa, il tasto destro su D: in
  Questo PC apre il menu della shell (254 ms, nel log come "menu contestuale lento": atteso),
  Shift+tasto destro il nostro, con "Altre opzioni…" senza Shift+F10; la barra laterale tiene il
  nostro; spenta di nuovo, `menu_completo=0` in impostazioni.txt;
- log: nessuna voce nuova a parte quella del menu della shell e le solite "attesa vsync lunga".

Attenzione: togliere "Riconnetti all'accesso" nel dialogo cambia anche il predefinito di Windows
(`net use` passa a "non saranno memorizzate"). Claude l'ha rimesso con `net use /persistent:yes`.

Restano a te:
- [ ] **un'unità di rete vera**: deve avere il nome della condivisione, come in Explorer; un'unità non raggiungibile non deve bloccare niente;
- [ ] **Disconnetti con un file aperto** sull'unità (in Notepad): avviso "ci sono file aperti";
- [ ] Ctrl+clic e clic centrale su Questo PC aprono una scheda; con la sezione Unità chiusa sparisce;
- [ ] con l'opzione accesa, il tasto Menu della tastiera apre il menu della shell; l'opzione
      resta dopo il riavvio.

## 000. Vista a griglia con miniature (0.19)

Claude l'ha provata a schermo su z-f-dev (MSVC senza avvisi) a 2560×1392, in cartelle di prova
nello scratchpad (`python prove\griglia-prova.py <cartella>`: circa 3.000 file finti) e, solo
guardando, in WinSxS, Download e Questo PC. Ha funzionato:
- **Ctrl+Shift+2/3/4/6**, pulsante ▦ (tra ☆ e ◨), menu ≡ e menu dello sfondo ("Mostra come
  griglia/elenco" con la scorciatoia); passando da una vista all'altra resta in vista la voce del cursore;
- **vista per cartella** in `viste.txt`: riaprendo z-f-dev le cartelle in griglia tornano in griglia;
- **miniature**: dalla cache di Windows (0,4–0,8 ms), con WIC le foto mai viste (15–27 ms a foto,
  6 thread: prima schermata di 66 foto da 12 MP in 306–332 ms), video dalla shell, icone grandi per
  cartelle, zip, exe, PDF (senza gestore), unità; foto ruotate con l'EXIF dritte; `desktop.ini`
  nascosto attenuato;
- **cache di Windows riempita a riposo**: le foto viste con WIC, riaperte in un processo nuovo,
  arrivano tutte dalla cache (prima schermata 103 ms);
- **tastiera** (← → ↑ ↓, Shift+→), **rettangolo** (20 voci in 5 colonne × 4 righe), **F2** con la
  casella sotto la miniatura, **tooltip** sulle voci, piccole/medie/grandi;
- **frame**: scorrendo veloce mentre arrivano le foto 0,19 ms (peggiore 0,76); WinSxS (30.734 voci)
  in griglia piccola 0,32 ms (peggiore 0,91); entrando in griglia peggiore 0,91. In F12: risultati e
  richieste al massimo 0,04 ms, disegno delle voci 0,23 ms, texture 0,30 ms per frame;
- log: nessuna voce dovuta alla griglia (le "attesa vsync lunga" sono arrivate ingrandendo la finestra).

Restano a te:
- [ ] **Ctrl+rotellina in griglia** cambia piccole ↔ medie ↔ grandi (in elenco resta il testo).
      Claude non riesce a tenere Ctrl con la rotellina.
- [ ] **Trascinamento dalla griglia**: da miniatura o nome di una voce selezionata partono i file;
      da una voce non selezionata il rettangolo. Rilascio su una cartella della griglia (bordo
      attorno alla voce). Claude non l'ha provato per non spostare file veri.
- [ ] **Clic, doppio clic, clic centrale e tasto destro** su una voce della griglia; menu da
      tastiera (tasto Menu) sotto la voce del cursore.
- [ ] **Una cartella di foto vera del telefono** (HEIC, se hai il codec) e una **di rete**: le
      miniature arrivano senza bloccare niente; uscendo dalla cartella di rete le altre restano veloci.
- [ ] **OneDrive solo online**: niente download, solo le miniature che Windows ha già.
- [ ] **Scala 150%** (o un altro monitor): il riquadro e le miniature si rifanno alla misura nuova.
- [ ] **Da decidere:** l'aspetto (riquadri, nome su due righe, cartelle e icone a metà del
      riquadro, cursore) e i 6 thread di WIC, che per un attimo occupano metà dei core.

## 00. Annulla e ripeti (0.19)

Claude l'ha provato a schermo su z-f-dev (MSVC senza avvisi), in una cartella di prova nello
scratchpad con file finti. Ha funzionato:
- **rinomina** con F2 → Ctrl+Z ("Annullata la rinomina: di nuovo c.txt") → Ctrl+Y → Ctrl+Z;
- **Canc** di un file → Ctrl+Z lo rimette al suo posto con il cursore sopra; **Ctrl+A e Canc** di
  5 elementi con una cartella piena → Ctrl+Z li rimette tutti (3,6 ms sul thread). Nel Cestino non
  resta niente (controllato con la shell: nessuna voce vuota);
- **copia con conflitto:** finestra della shell, "Mantieni entrambi" → `b (2).txt`; Ctrl+Z manda
  nel Cestino solo `b (2).txt`, il `b.txt` che c'era resta; Ctrl+Y la riporta dal Cestino;
- **Taglia e Incolla** in un'altra cartella → gli appunti si svuotano → Ctrl+Z ("Annullato lo
  spostamento di a.txt: di nuovo in Sorgente");
- **disco cambiato:** dopo lo spostamento un altro programma crea un `a.txt` dove stava →
  Ctrl+Z non sostituisce niente e dice "Impossibile annullare lo spostamento: in Sorgente c'è già
  un elemento chiamato a.txt"; una copia modificata da un altro programma → "Impossibile annullare
  la copia: e.txt è stato modificato dopo";
- **nuova cartella** (Ctrl+Shift+N) rinominata in Foto → un solo Ctrl+Z ("Annullata la creazione di Foto");
- **trascinamento** (sposta in una cartella) → "Spostato in Foto" → Ctrl+Z;
- menu ≡ ("Annulla nuova cartella Ctrl+Z", "Ripeti spostamento Ctrl+Y") e menu dello sfondo
  ("Annulla …" dopo Incolla); F12; opzione "Annulla dopo il riavvio": accesa, z-f-dev chiuso e
  riaperto, Ctrl+Z ha annullato la rinomina fatta prima;
- log: nessuna voce dovuta alle operazioni sui file.

Restano a te:
- [ ] **Ctrl+trascinamento** (copia) e **Alt+trascinamento** (collegamento) → Ctrl+Z: la copia e il
      .lnk vanno nel Cestino. Claude non può tenere un tasto premuto mentre trascina.
- [ ] **Con Explorer:** Taglia in Explorer e Incolla in [z]f (sposta, gli appunti si svuotano) →
      Ctrl+Z; trascinamento da Explorer a [z]f → Ctrl+Z.
- [ ] **Tra due unità** (C: → D:, file grande): avanzamento di Explorer, poi Ctrl+Z lo riporta
      (anche qui con l'avanzamento); [z]f resta reattivo.
- [ ] **Conferma dell'eliminazione:** se nelle proprietà del Cestino hai "Visualizza conferma
      eliminazione", Canc la chiede come Explorer; altrimenti no.
- [ ] **Unità di rete o chiavetta senza Cestino:** Canc → la shell avvisa che l'eliminazione è
      definitiva; dopo, Ctrl+Z non la annulla (e annulla l'operazione prima, dicendo quale).
- [ ] **Ctrl+Z durante una copia lunga:** "Operazione sui file in corso: si potrà annullare quando finisce".
- [ ] **Ctrl+Z mentre scrivi** (rinomina, percorso, "Vai a", ricerca delle opzioni): non tocca i file.
- [ ] **Da decidere:** un annullamento che non riesce per un motivo definitivo (posto occupato,
      file sparito, modificato dopo) esce dalla cronologia, come in Explorer: il Ctrl+Z dopo annulla
      l'operazione precedente. Per un motivo che può passare (file in uso, Annulla, permessi) ora
      resta e si riprova (sezione 000000). Ti va bene questa divisione?
- [ ] **Da decidere:** cambiano strada Incolla, Canc/Elimina e il rilascio di file veri: ora li fa
      [z]f con `IFileOperation`, non la shell. Usa per qualche giorno z-f-dev al posto di z-f per
      vedere se qualcosa si comporta diversamente da prima.

## 0c. Pannello di anteprima (z-f-dev)

Provato da Claude senza schermo (accesso negato), con un programma che chiama lo stesso codice
del thread su una cartella di prova:
- **immagini:** JPEG 6000×4000 con rotazione EXIF 6 → 360×540 e dritta, 13–15 ms; JPEG 1500×1000
  6 ms; PNG 4000×3000 11 ms; PNG trasparente, BMP, WebP 4–5 ms (la prima WebP 80 ms: Windows
  carica il codec); JPEG rotto → informazioni. Creazione di WIC la prima volta: 1,8 ms;
- **testo:** UTF-8 da 1,3 MB → primi 256 KB, 1,5 ms; ASCII, ANSI (accenti e “virgolette” giusti),
  UTF-16 con e senza BOM, file senza estensione, JS minimizzato su una riga sola (spezzato a 2000
  caratteri), file vuoti: tutti riconosciuti, sotto 1 ms;
- **binari** (.bin, estensione sconosciuta) → icona grande e informazioni, 1,5–2,5 ms;
- **cartelle:** 3.001 voci in 2 ms, cartelle prima; cartella vuota.

Poi Claude l'ha provato a schermo su z-f-dev (2560×1392, scala 100%), nella cartella di prova
che crea `python prove\anteprima-prova.py <cartella>`. Ha funzionato:
- Alt+P, il pulsante ◨ (con il tooltip "Nascondi l'anteprima · Alt+P") e ≡ → "Mostra anteprima
  Alt+P"; lo stato aperto, la larghezza e il carattere si salvano e tornano al riavvio;
- ogni tipo della cartella di prova: cartelle (righe allineate a quelle della lista), binari e
  JPEG rotto (icona grande e righe del tooltip), JSON, C, log, file senza estensione, UTF-16 con e
  senza BOM, ANSI, file vuoto, BMP, PNG grande e trasparente, WebP semitrasparente, foto del
  telefono dritta, testo lungo ("UTF-8 · primi 256 KB di 1,3 MB");
- Questo PC: icona del disco, file system, capacità, usato, libero;
- rotellina sul testo (5 scatti = 15 righe), la lista non si muove;
- bordo trascinato fino a circa 900 px e doppio clic che torna a 360; con la finestra non
  ingrandita il pannello era già più stretto di 360 (cede per primo);
- 80 frecce di fila in una cartella di 3.001 file: l'anteprima finisce sull'ultima voce;
- F12 col pannello aperto su un testo lungo: frame 0,11 ms (peggiore 0,27), anteprima lettura
  0,3 ms, decodifica 0,7 ms, texture 0,08 ms, primo caricamento di WIC 0,7 ms;
- log: niente dovuto all'anteprima (solo le voci solite dell'avvio e un'attesa vsync lunga
  ingrandendo la finestra, il debito già noto).

Corretti durante la prova: una parola più lunga di una riga (JS minimizzato) lasciava "var" da
solo sulla prima riga; il piede contava come righe anche i pezzi spezzati ("85 righe" per un file
di una riga sola); le righe di una cartella partivano 8 px più in basso di quelle della lista.

Restano a te (Claude non può tenere un tasto premuto mentre trascina, né usare la rete):
- [ ] **Esc a metà trascinamento del bordo:** torna la larghezza di prima. Il cursore ↔ sul bordo
      (negli screenshot il cursore non si vede).
- [ ] **Freccia tenuta premuta** davvero (la ripetizione della tastiera) in una cartella con molte
      foto grandi: la lista scorre come prima, l'anteprima non lampeggia.
- [ ] **File lenti:** su un'unità di rete l'interfaccia non si blocca; se tarda si vedono il nome e
      "caricamento…".
- [ ] **Finestra molto stretta:** sotto i 220 px di spazio l'anteprima sparisce e torna
      riallargando; aprendola senza posto compare l'avviso.
- [ ] **Opzioni:** cambiare "Carattere dell'anteprima" (Consolas, Lucida Console, Courier New,
      Segoe UI) cambia subito il testo.
- [ ] **Da decidere:** il nome nell'intestazione del pannello è in `C_TEXT` (è il contenuto, non
      un'etichetta come "Nome ↑"). Ti va bene? L'immagine è centrata anche in verticale: su uno
      schermo alto resta a metà pannello. E 360 px di larghezza predefinita su 2560 ti bastano?

Al primo avvio della nuova z-f.exe il log ha mostrato, oltre al normale `messaggio lento 0x8002`,
un `Present lento 23 ms` 4 ms prima, quindi durante il passaggio WARP → GPU. Si è ripresentato
(18 ms) anche al primo avvio di z-f-dev con "Vai a", sempre durante il passaggio: è da guardare.

## 0. "Vai a" (z-f-dev)

Claude non l'ha potuto provare a schermo (accesso allo schermo negato). Provati senza schermo:
l'avvio (log normale), la scrittura di `%APPDATA%\z-f\recenti.txt` (la cartella iniziale c'è),
l'ordine e i tempi della ricerca su 20.000 nomi (0,3 ms a tasto). Tutto il resto va visto:

- [ ] **Apertura:** Ctrl+P, Ctrl+G e ≡ → "Vai a…" aprono il pannello sotto la riga del percorso,
      in dissolvenza come un menu; lo stesso tasto lo richiude. La voce del menu ≡ mostra "Ctrl+P".
- [ ] **Campo vuoto:** mostra le cartelle recenti (naviga un po' prima: alla prima apertura ci
      sono solo quelle visitate), non quella aperta. Il primo giorno, senza recenti: preferiti e unità.
- [ ] **Ricerca:** "dl" → Download; "doc" → Documenti; le lettere trovate sono in blu, nel nome o
      nel percorso; il percorso è in grigio accanto al nome. Una cartella usata spesso sale.
- [ ] **Sottocartelle:** trova le sottocartelle della cartella aperta, delle altre schede, dei
      preferiti e delle recenti più usate (queste arrivano dal thread un attimo dopo l'apertura:
      la voce attiva non deve saltare).
- [ ] **Percorso scritto:** `C:\` elenca le cartelle di C:; `C:\Us` filtra; Tab sulla voce scende
      (`C:\Users\`); Backspace risale. `%APPDATA%\` funziona. Un percorso che non esiste: Invio
      dà l'errore solito nella barra di stato. Unità di rete lenta: "caricamento…" e niente blocchi.
- [ ] **Tastiera:** ↑ ↓ PagSu PagGiù; Invio apre; Ctrl+Invio apre in una nuova scheda in
      background; Esc svuota, il secondo Esc chiude; Home/Fine/Ctrl+frecce/Ctrl+V nella casella.
- [ ] **Mouse:** la voce sotto il mouse diventa attiva; clic apre; clic centrale o Ctrl+clic in
      una nuova scheda; rotellina scorre; clic fuori chiude senza fare altro; un doppio clic su una
      voce non apre anche il file che nella lista sta sotto.
- [ ] **Tooltip** su una voce: percorso, quante volte è stata aperta e quando, "Tra i preferiti".
- [ ] **F12** con il pannello appena usato: righe "Vai a: apertura", "ricerca a ogni tasto" e
      "sottocartelle (sul thread)" con valori sensati (apertura e ricerca sotto 1 ms).
- [ ] **Aspetto:** misure, colori, larghezza (al massimo 600 px a scala 100%), nome lungo
      troncato con "…". Ti va bene il percorso troncato alla fine ("…") come dice DESIGN.md?
- [ ] **Log:** dopo le prove `build\z-f-log.txt` non ha voci "vai a: ricerca lenta".
- [ ] **Da decidere:** cambiando scheda dalla barra del titolo con il pannello aperto, il
      pannello resta aperto con le voci di prima. Va chiuso?

## 0b. Rettangolo di selezione e colonne (z-f-dev)

Claude l'ha provato a schermo in una cartella di prova con 123 file finti e in WinSxS (solo
selezione). Ha funzionato:
- rettangolo dalla colonna Data, dalla parte vuota della colonna Nome e dallo spazio sotto le righe (anche verso l'alto);
- selezione mentre si muove;
- scorrimento automatico portando il mouse sulla barra di stato;
- clic sullo spazio vuoto che toglie la selezione;
- trascinamento dei due bordi e doppio clic per adattare;
- larghezze salvate in `impostazioni.txt`;
- log pulito.

Misurato: aggiornamento del rettangolo al massimo 0,01 ms e adattamento 0,11 ms su 30.734 voci, frame 0,17 ms.
Le larghezze di prova sono state tolte da `impostazioni.txt`: si riparte da 150 e 100.

Non ha potuto provare quello che richiede un tasto premuto insieme al mouse:

- [ ] **Ctrl+rettangolo** alterna le righe toccate (quelle già selezionate si deselezionano);
      **Shift+rettangolo** le aggiunge alla selezione che c'era.
- [ ] **Esc a metà rettangolo:** il rettangolo sparisce e torna la selezione di prima del clic.
      **Esc a metà trascinamento di un bordo:** torna la larghezza di prima.
- [ ] **Aspetto:** il fondo `C_MARQUEE` (azzurro al 20% circa) e il contorno 1 px ti piacciono?
      Angoli squadrati come Windows: va bene?
- [ ] **Cursore ↔** sopra i bordi dell'intestazione, con la linea verticale sottile (non si vedeva
      negli screenshot). Un clic sul bordo non deve ordinare.
- [ ] **Dove parte cosa:** dal nome di una voce non selezionata parte il
      rettangolo (provato da Claude); da icona e nome di una voce già selezionata il trascinamento
      dei file (provato da Claude: parte, e senza destinazione non fa niente). Per trascinare un
      file non selezionato prima lo si clicca. Prova anche un
      trascinamento vero verso un'altra cartella di prova.
- [ ] **Scorrimento:** la velocità vicino al bordo e oltre ti sembra giusta? Parte già quando il
      mouse è sull'ultima riga visibile: se dà fastidio si riduce la fascia.
- [ ] **Finestra stretta:** stringendo la finestra il nome resta leggibile e cedono prima Data,
      poi Dimensione; riallargandola tornano le larghezze scelte.
- [ ] **Tasto destro e doppio clic** fuori dal nome si comportano come prima (menu, apre).

Corretti dopo le prove (z-f-dev, MSVC senza avvisi), provati da Claude in una cartella di prova:
- dopo Annulla nella finestra "Accesso alla cartella negato" non compare più "Spostato in …".
  La shell rispondeva S_OK anche dopo Annulla. Ora la chiamata dura quanto la finestra di Windows
  (misurato 9,2 s e 4,9 s), poi si conta sul disco cosa è arrivato: 0 elementi, nessun avviso.
  Spostando un file e due file in una cartella normale: "Spostato in dest" e "2 elementi spostati
  in dest" (62 ms);
- con la finestra larga circa 1000 px e 7 elementi selezionati, i tempi spariscono invece di
  sovrapporsi; con una sola voce selezionata tornano.

- [ ] **Copia e collegamento verificati:** Claude non ha potuto tenere premuti Ctrl o Alt durante
      il trascinamento. Prova Ctrl+trascinamento verso un'altra cartella ("Copiato in …"), verso
      una cartella dove il file c'è già e scegli "Salta" (nessun avviso), e Alt+trascinamento
      ("Collegamento creato in …").
- [ ] **Allegati e altre app** (elementi senza percorso): ora dopo il rilascio non compare
      l'avviso, perché l'esito non si può verificare. Va bene così?

Per le prove di trascinamento conviene una cartella di prova con file finti, non file veri.
Dopo ogni prova: guarda `build\z-f-log.txt` (all'avvio è normale solo `messaggio lento 0x8002`).

## 1. Pannelli e scorciatoie

- [ ] **Pannello F12:** ora ha il fondo `C_MENU` (un po' più chiaro di prima), l'ombra e la
      dissolvenza d'ingresso, come menu e tooltip. Va bene così?
- [ ] **Menu identici a prima:** file, sfondo, ≡, scheda, preferito. Ogni voce ha la stessa
      scorciatoia di prima.
- [ ] Nel menu ≡, la riga della versione **non** mostra "Ctrl+,".
- [ ] Nel menu di una cartella selezionata, "Aggiungi ai preferiti" **non** mostra Ctrl+D e
      aggiunge quella cartella, non quella aperta.
- [ ] "Apri in nuova scheda" funziona dalla lista e dal menu di un preferito.
- [ ] Opzioni (Ctrl+,): la riga "Clic centrale — Apre una cartella in una nuova scheda" (prima era
      "Clic centrale su una cartella — La apre in una nuova scheda"); la ricerca trova ancora tutto.

## 2. Trascinamento

Già provato da Claude con due finestre di [z]f affiancate: spostamento, Ctrl+copia, tasto destro
con il menu, aggiunta ai preferiti, rilascio su un preferito, su una scheda, sul percorso.
Restano da provare:

- [ ] **[z]f → Explorer** e **Explorer → [z]f**: sposta sulla stessa unità, copia su un'altra.
- [ ] **[z]f → desktop** e **[z]f → browser** (per esempio un caricamento in una pagina web).
- [ ] **Da un'app che non è la shell** (un allegato da un client di posta, un'immagine dal
      browser): deve copiare; se Windows non mostra la scritta accanto al cursore, la scrive [z]f
      in un piccolo pannello.
- [ ] **Unità di rete o chiavetta:** il rilascio non deve bloccare l'interfaccia (la shell lavora
      su un thread); il log non deve mostrare "trascinamento lento".
- [ ] **Molti file o file grandi:** compare l'avanzamento di Explorer; [z]f resta reattivo.
- [ ] **Conflitto di nomi:** compare la finestra di Explorer per sostituire o saltare.
- [ ] **Tasto destro, menu:** quando si apre, la voce predefinita è evidenziata, ma
      l'evidenziazione sparisce appena muovi il mouse. Ti va bene, o vuoi che resti?
- [ ] **Tasto destro da un'altra app:** clic fuori dal menu o Esc lo chiudono e l'altra app non
      resta bloccata.
- [ ] **Selezione multipla:** selezionare più file, cliccarne uno e trascinare → si trascinano
      tutti; un clic senza trascinare lascia selezionato solo quello.
- [ ] **Scheda:** tenendo il file fermo su una scheda per mezzo secondo, si apre.
- [ ] **Scorrimento:** trascinando vicino al bordo alto o basso della lista, la lista scorre.
- [ ] **F12:** le righe "trascinamento: registrazione (OLE)", "rilascio e operazione (thread)"
      (ora dura tutta l'operazione, finestre di Windows comprese) e "chiamata più lenta (UI)"
      hanno valori sensati.

## 3. Da decidere

- [ ] Riordinare i preferiti trascinandoli: oggi il clic su un preferito naviga alla pressione del
      tasto (più veloce). Per trascinarli bisognerebbe navigare al rilascio.
- [ ] **Menu completo della shell su un thread suo** (250–550 ms sul thread dell'interfaccia a ogni
      apertura): segnato tra i debiti di CLAUDE.md, con il piano. Da fare quando decidi.
- [ ] **Inizio di un trascinamento con migliaia di voci** (550 ms per 20.300): vedi la sezione 0000000.
- [ ] Debiti trovati (in CLAUDE.md): scorciatoie scritte a mano nei tooltip e in "Ctrl+D aggiunge"
      (quelle del menu della shell vengono ora da `kShortcuts`); numeri sparsi in `options.cpp` e
      `tooltip.cpp`.
