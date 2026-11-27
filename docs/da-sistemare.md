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
  file): il limite vale anche allora. Provato il 28/11 a 100% (riquadri di 256 px, 256 posti):
  lista in Icone a 20% con circa 220 miniature visibili e anteprima in Icone grandi, tutte piene
  anche scorrendo. A 175% resta da provare (`prove\da-verificare.md`): lì i posti sono 81, ma
  anche le miniature visibili sono circa un terzo.

- [ ] **Dialoghi Apri e Salva con z-f-dev e [z]f aperti insieme** (`filedlg.cpp`). Dal 28/11 la
  build di prova ha un suo nome (`ZF_ID`), quindi anche la sua "ultima finestra di [z]f" per i
  dialoghi: se entrambe hanno l'opzione accesa (z-f-dev la ha di base, con le impostazioni
  predefinite), un dialogo può ricevere due salti verso cartelle diverse. Prima saltavano tutte e
  due verso la stessa. Da decidere se la build di prova deve partire con `dialoghi=0`.
