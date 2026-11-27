# Piccole cose da sistemare

Dettagli trovati lavorando, che non meritano una sessione propria: si sistemano quando si tocca
quel codice, o tutti insieme in un momento tranquillo. Quando una voce è fatta si toglie da qui
(la storia resta in git). Le idee nuove sono in `idee.md`, le prove a mano in
`prove\da-verificare.md`, i debiti tecnici più grandi in `CLAUDE.md`.

- [ ] **Gruppi: scelte minori da confermare con Zyruel** (27/11, scritte in DESIGN §7 "Gruppi"):
  - ordine dei gruppi: quello del menu del filtro, rovesciato se la lista è ordinata per la stessa
    colonna nel verso opposto (per Dimensione dal più grande il primo gruppo è "Oltre 4 GB");
  - un gruppo chiuso toglie le sue voci dalla vista come il filtro: contatori e "Filtra N
    elementi" non le contano, Ctrl+A non le prende;
  - da tastiera i gruppi non si chiudono: una scorciatoia (per esempio ← sulla prima voce del
    gruppo) andrebbe decisa insieme;
  - l'intestazione ha l'aspetto del titolo di sezione della barra laterale (niente fondo sotto il
    mouse, freccia nella colonna delle icone invece che a destra);
  - chiudendo un gruppo la lista non scorre e il cursore che c'era dentro passa alla prima voce
    dei gruppi dopo; il clic sull'intestazione vale al rilascio e non toglie la selezione (dallo
    spazio vuoto invece la toglie).

- [ ] **Filtro per Dimensione su cartelle enormi** (27/11, da decidere con Zyruel): con il filtro
  attivo e la dimensione delle cartelle accesa, ogni totale che arriva fa rifiltrare la vista, al
  più ogni `WATCH_BATCH` (50 ms); in WinSxS un rifiltro costa 6,6 ms (le ricerche di 30.732
  cartelle nella cache dei totali, `dsEachDir`), quindi fino al 13% del thread UI finché il conto
  va avanti. Se pesa: un intervallo più lungo solo per questo, o i totali tenuti per indice di voce
  nel `Listing` e aggiornati solo per le cartelle arrivate. Vale anche per i **gruppi per
  Dimensione** (stesso timer, `viewUsesDirTotals`): in WinSxS raggruppare costa 7,6 ms, contro
  0,7–1,0 per Tipo e date.

- [ ] **Hover dopo la rotellina** (27/11, visto provando i gruppi, c'era già): scorrendo con la
  rotellina senza muovere il mouse, il fondo dell'hover resta sulla voce (o sull'intestazione di
  gruppo) di prima, che si allontana dal puntatore, finché il mouse non si muove (`A.hover` e
  `A.groupHover` si aggiornano solo in `WM_MOUSEMOVE`). Lo stesso dopo un filtro scritto o una
  ricarica, che spostano voci e gruppi sotto il puntatore fermo (anche il tooltip di un gruppo
  resta sull'indice di prima). Da decidere se ricalcolarli dalla posizione del mouse quando la
  lista si sposta o cambia.

- [ ] **Gruppi: punti dubbi del controllo** (27/11), da decidere:
  - il clic su un'intestazione si riconosce da `A.cur` e dall'indice del gruppo, come quello sulla
    destinazione di un collegamento: in una cartella che si ricarica di continuo (uno scaricamento
    in Download) una ricarica tra pressione e rilascio fa perdere il clic;
  - tornando a una beta precedente, le righe di `viste.txt` con il raggruppamento in Elenco e
    Dettagli vengono scartate (la vista di quelle cartelle si perde);
  - il menu dei gruppi aperto da tastiera (Invio su "Raggruppa per…") non ha la prima voce attiva;
  - "gruppi lenti" usa la soglia `LOG_MSG`: oltre 25 ms esce anche "messaggio lento" per lo stesso
    evento;
  - Elenco con una sola riga visibile (finestra bassissima, testo grande): con i gruppi l'intestazione
    prende la riga e le voci finirebbero sulla barra di scorrimento.

- [ ] **Punti minori della revisione e del controllo prima della 1.0.0-beta.8** (27/11). Interazione:
  - doppio clic fuori dalle opzioni: il primo clic le chiude, il secondo (`WM_LBUTTONDBLCLK`)
    apre il file sotto (`input.cpp`, prima di `openSelected`); segnare l'ora della chiusura come
    `g_menuClickAt`;
  - con le opzioni aperte ogni tasto che la ricerca non usa (Shift da solo, Alt+Shift+P) riporta
    il pannello in cima (`optionsKey`, ramo `else`): azzerare lo scorrimento solo se il testo cambia;
  - tasto destro con le opzioni o "Vai a" aperti: `PV.keys` va alla lista o all'anteprima nascoste
    (`WM_RBUTTONDOWN`);
  - F5 dato nella lista non riaccende l'osservatore dell'anteprima perso (`refreshNow`): o lo fa,
    o ARCHITETTURA dice "F5 nell'anteprima";
  - trascinamento sopra l'anteprima che passa a un'altra cartella: fino al movimento dopo il bordo
    sta sulla riga con lo stesso indice (`drawDrop`); `dropUpdate` quando l'anteprima cambia;
  - "Sposta nel Cestino" del menu con Shift ancora premuto elimina definitivamente
    (`deleteSelected` legge Shift): il menu dovrebbe andare sempre nel Cestino;
  - `WM_CAPTURECHANGED` non ferma `draggingThumb` (Alt+Tab mentre si trascina la barra: lo
    scorrimento segue il mouse senza tasto); c'era già.

  Avvisi e misure:
  - Alt+Shift+P con l'anteprima chiusa cambia solo la posizione salvata, senza avviso;
  - `previewNoRoom()` è informativo con Alt+P e ◨, errore con Tab (`previewKeysToggle`);
  - F12, "ultima rilettura" dell'anteprima: `A.prevChangeAt` resta se `previewRefresh` esce
    subito, e la rilettura dopo si misura da una notifica vecchia;
  - `drawTextKeepTail` senza riga "Misurato:" in ARCHITETTURA (costo trascurabile).

  Testi e coerenza:
  - etichette delle opzioni limitate sempre a `OPT_LABEL_RESERVE`: lunghe, finiscono in "…" senza
    tooltip; in una finestra stretta passano sotto il fondo dei selettori;
  - le righe delle opzioni con una scorciatoia (Barra laterale, Anteprima, Posizione
    dell'anteprima, Dimensione del testo) non la mostrano in un tooltip (`OP.tips` c'è già);
  - descrizioni di `kShortcuts` (e README) che non cominciano con un verbo: "La apre dopo un
    momento", "Lo sposta tra gli altri", "Vai a: …" (NOVITA della 1.0.0-beta.8 dice "tutte");
  - il titolo del tooltip va a capo a qualsiasi carattere, DESIGN §4 dice dopo l'ultimo spazio
    (`textLineEnd`): usarla anche lì, o correggere DESIGN;
  - "Disco l… (C:)" solo nella barra laterale, DESIGN §4 lo scrive in generale;
  - il commento di `withThousands` (`app.cpp`) non cita contatori e selezionati.

- [ ] **Atlante delle miniature con lista e anteprima in Icone a misure diverse** (`thumbs.cpp`).
  I posti sono grandi quanto il riquadro più grande in uso (calano solo quando quello scende alla
  metà o meno): a 175% l'anteprima in Icone grandi ha riquadri di 448 px, quindi nell'atlante
  stanno 81 miniature; con la lista in Icone piccole a tutto schermo se ne vedono di più, e quelle
  oltre restano vuote. Con una lista sola l'atlante segue lo zoom (provato: da 100 a 20% a tutto
  schermo, tutte le miniature). Da provare a 175% con le due liste; se serve, un atlante per misura
  o righe dell'atlante per misura. Dalla 1.0.0-beta.7 l'anteprima aperta in Icone conta anche
  mentre mostra un file (così le miniature della lista non lampeggiano scorrendo tra cartelle e
  file): il limite vale anche allora.

- [ ] **Collegamenti senza LinkInfo** (colonna Destinazione, `lnk.cpp`). Il nostro lettore prende
  il percorso dalla LinkInfo, dalla variabile d'ambiente o dal percorso relativo; un collegamento
  che ha solo l'elenco di elementi della shell (IDList) resta senza destinazione. Sui 177 del PC
  non ce n'è nessuno (tutti uguali a Windows). Da rivedere se su un altro PC ne compare uno vuoto
  dove le proprietà di Windows mostrano un percorso. Un caso concreto (26/11): Windows scrive solo
  l'IDList quando, alla creazione, la destinazione non è raggiungibile (per esempio un collegamento
  a un'unità di rete creato mentre l'unità non è collegata). Leggerlo vorrebbe dire decodificare
  gli elementi della IDList (unità, poi un nome per cartella).
