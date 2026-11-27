# Piccole cose da sistemare

Dettagli trovati lavorando, che non meritano una sessione propria: si sistemano quando si tocca
quel codice, o tutti insieme in un momento tranquillo. Quando una voce è fatta si toglie da qui
(la storia resta in git). Le idee nuove sono in `idee.md`, le prove a mano in
`prove\da-verificare.md`, i debiti tecnici più grandi in `CLAUDE.md`.

- [ ] **Filtro per Dimensione su cartelle enormi** (27/11, da decidere con Zyruel): con il filtro
  attivo e la dimensione delle cartelle accesa, ogni totale che arriva fa rifiltrare la vista, al
  più ogni `WATCH_BATCH` (50 ms); in WinSxS un rifiltro costa 6,6 ms (le ricerche di 30.732
  cartelle nella cache dei totali, `dsEachDir`), quindi fino al 13% del thread UI finché il conto
  va avanti. Se pesa: un intervallo più lungo solo per questo, o i totali tenuti per indice di voce
  nel `Listing` e aggiornati solo per le cartelle arrivate. Vale anche per i **gruppi per
  Dimensione** (stesso timer, `viewUsesDirTotals`): in WinSxS raggruppare costa 7,6 ms, contro
  0,7–1,0 per Tipo e date.

- [ ] **Punti minori della revisione e del controllo prima della 1.0.0-beta.8** (27/11; quelli di
  interazione senza scelte fatti il 27/11, nella 1.0.0-beta.9). Scelte da confermare:
  - F5 dato nella lista non riaccende l'osservatore dell'anteprima perso (`refreshNow`): o lo fa,
    o ARCHITETTURA dice "F5 nell'anteprima";
  - Alt+Shift+P con l'anteprima chiusa cambia solo la posizione salvata, senza avviso;
  - `previewNoRoom()` è informativo con Alt+P e ◨, errore con Tab (`previewKeysToggle`);
  - `drawTextKeepTail` senza riga "Misurato:" in ARCHITETTURA (costo trascurabile; misurarlo da
    solo vuole l'atlante dei glifi).

  Testi e coerenza:
  - etichette delle opzioni limitate sempre a `OPT_LABEL_RESERVE`: lunghe, finiscono in "…" senza
    tooltip; in una finestra stretta passano sotto il fondo dei selettori;
  - descrizioni di `kShortcuts` (e README) che non cominciano con un verbo: "La apre dopo un
    momento", "Lo sposta tra gli altri", "Vai a: …" (NOVITA della 1.0.0-beta.8 dice "tutte");
  - il titolo del tooltip va a capo a qualsiasi carattere, DESIGN §4 dice dopo l'ultimo spazio
    (`textLineEnd`): usarla anche lì, o correggere DESIGN;
  - "Disco l… (C:)" solo nella barra laterale, DESIGN §4 lo scrive in generale.

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
