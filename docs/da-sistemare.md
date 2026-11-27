# Piccole cose da sistemare

Dettagli trovati lavorando, che non meritano una sessione propria: si sistemano quando si tocca
quel codice, o tutti insieme in un momento tranquillo. Quando una voce è fatta si toglie da qui
(la storia resta in git). Le idee nuove sono in `idee.md`, le prove a mano in
`prove\da-verificare.md`, i debiti tecnici più grandi in `CLAUDE.md`.

- [ ] **Atlante delle miniature con lista e anteprima in Icone a misure diverse** (`thumbs.cpp`).
  I posti sono grandi quanto il riquadro più grande in uso (calano solo quando quello scende alla
  metà o meno): a 175% l'anteprima in Icone grandi ha riquadri di 448 px, quindi nell'atlante
  stanno 81 miniature; con la lista in Icone piccole a tutto schermo se ne vedono di più, e quelle
  oltre restano vuote. Con una lista sola l'atlante segue lo zoom (provato: da 100 a 20% a tutto
  schermo, tutte le miniature). Da provare a 175% con le due liste; se serve, un atlante per misura
  o righe dell'atlante per misura. Dalla 1.0.0-beta.7 l'anteprima aperta in Icone conta anche
  mentre mostra un file (così le miniature della lista non lampeggiano scorrendo tra cartelle e
  file): il limite vale anche allora.

- [ ] **Punti minori del controllo prima della 1.0.0-beta.7** (`/controllo cambiati`, 27/11; le
  correzioni principali sono nel commit "Correzioni del controllo prima della 1.0.0-beta.7").
  Correttezza, casi rari o dubbi da verificare:
  - `thumbs.cpp`, `thumbsPreviewIdle` e il ramo `!gridView()`: quando una lista smette di essere in
    Icone, l'altra non viene risincronizzata (`needSync`), e le icone per tipo in comune
    (`.pdf|lato`) possono restare senza richiesta finché non si scorre;
  - `thumbs.cpp`: il `Listing` dell'anteprima ha sempre `gen = 0` e il ramo non-Icone non azzera
    `c.forList`: con un indirizzo riusato una cartella di rete potrebbe sembrare locale
    (`c.remote`). Basta `l->gen = r.gen` in `pvDir`;
  - `watch.cpp`, `previewWatch`: decide con `zipPathLike` invece che con `zipLen` (una vera
    cartella `Backup.zip` nell'anteprima resta senza osservatore); su `WATCH_LOST` riparte subito,
    e con un file system senza notifiche rileggerebbe ogni 50 ms;
  - `drag.cpp`, `drawDrop`: con l'anteprima sparita durante un trascinamento, il bordo della
    destinazione si disegna con la geometria della lista;
  - `input.cpp`: Ctrl+rotellina sull'anteprima di un testo o di un'immagine cambia la vista della
    lista; `pvPaneSet`/`pvPaneClear` azzerano `draggingThumb` senza `ReleaseCapture`; il tasto
    Aggiorna della tastiera (`WM_APPCOMMAND`) rilegge la lista anche con la tastiera
    nell'anteprima (F5 invece l'anteprima);
  - `menu.cpp`: il menu della vista dell'anteprima dà "L'anteprima non mostra più quella
    cartella" se intanto è cambiata, anche se la vista non dipende dalla cartella;
  - `tooltip.cpp`: il tooltip dell'anteprima compare durante una rinomina nella lista.

  Velocità: `A.filterMs` e `A.countsDirty` non sono in `ListPane` (il filtro dell'anteprima finisce
  nella misura della lista e ne sporca i contatori); la rilettura automatica dell'anteprima non
  ha misura in F12 né riga "Misurato:"; `drawThumb` ricostruisce chiavi e percorsi a ogni frame
  (da misurare con `cellsMs`).

  Testi: "La finestra è troppo stretta per l'anteprima" anche in basso (`previewToggle`, dovrebbe
  dire troppo bassa); Tab con l'anteprima nascosta per mancanza di spazio dice "chiusa"; "Nell'anteprima
  si incollano solo file…" è un rifiuto ma esce come informazione; "Filtra 12345 elementi…" senza
  punto delle migliaia (`withThousands`) e "5000" senza "+" per una cartella letta in parte; il
  tooltip del filtro dell'anteprima scrive "(clic qui, Tab)" a mano invece di
  `shortcutsOf(M_PANE_KEYS)`. Da decidere: i comandi della posizione dell'anteprima senza tasto
  (DESIGN §8 dice "ogni comando ha una scorciatoia", ma anche Apri con non ne ha).

  Pulizia: `pvPaneSet` rifà la logica di `applyRefresh`; `fltX`/`fltWant` scritti due volte
  (`layout`, `pvPaneLayout`) e il controllo "punto nel campo del filtro" quattro; la cache dei
  contatori dell'anteprima (`PV.cntFor`) rifà quella della barra di stato; `MENU_PREVIEW` e
  `PopupMenu::preview` si confondono (meglio `MENU_PREVIEW_POS`); `PV.keys = false` inutile in
  `input.cpp` sul campo del filtro; commenti superati (`render.cpp`, `drawListItems`: "preview:
  niente Data"; `options.cpp`: doppio clic "tranne su versione e aggiornamento"; `input.cpp`:
  l'elenco "⚙, ▯, ← → ↑, ☆, ◨" senza ˅ e "larghezza predefinita" anche per l'altezza;
  `options.cpp`, `pickRow`: il parametro `face`).

- [ ] **Collegamenti senza LinkInfo** (colonna Destinazione, `lnk.cpp`). Il nostro lettore prende
  il percorso dalla LinkInfo, dalla variabile d'ambiente o dal percorso relativo; un collegamento
  che ha solo l'elenco di elementi della shell (IDList) resta senza destinazione. Sui 177 del PC
  non ce n'è nessuno (tutti uguali a Windows). Da rivedere se su un altro PC ne compare uno vuoto
  dove le proprietà di Windows mostrano un percorso. Un caso concreto (26/11): Windows scrive solo
  l'IDList quando, alla creazione, la destinazione non è raggiungibile (per esempio un collegamento
  a un'unità di rete creato mentre l'unità non è collegata). Leggerlo vorrebbe dire decodificare
  gli elementi della IDList (unità, poi un nome per cartella).
