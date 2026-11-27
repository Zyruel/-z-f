# Piccole cose da sistemare

Dettagli trovati lavorando, che non meritano una sessione propria: si sistemano quando si tocca
quel codice, o tutti insieme in un momento tranquillo. Quando una voce è fatta si toglie da qui
(la storia resta in git). Le idee nuove sono in `idee.md`, le prove a mano in
`prove\da-verificare.md`, i debiti tecnici più grandi in `CLAUDE.md`.

- [ ] **Atlante delle miniature con lista e anteprima in Icone a misure diverse** (`thumbs.cpp`).
  I posti sono grandi quanto il riquadro più grande in uso (e non calano finché qualcuno è in
  Icone): a 175% l'anteprima in Icone grandi ha riquadri di 448 px, quindi nell'atlante stanno 81
  miniature; con la lista in Icone piccole a tutto schermo se ne vedono di più, e quelle oltre
  restano vuote. Con una lista sola non cambia niente. Da provare a 175%; se serve, un atlante per
  misura o righe dell'atlante per misura.

- [ ] **Collegamenti senza LinkInfo** (colonna Destinazione, `lnk.cpp`). Il nostro lettore prende
  il percorso dalla LinkInfo, dalla variabile d'ambiente o dal percorso relativo; un collegamento
  che ha solo l'elenco di elementi della shell (IDList) resta senza destinazione. Sui 177 del PC
  non ce n'è nessuno (tutti uguali a Windows). Da rivedere se su un altro PC ne compare uno vuoto
  dove le proprietà di Windows mostrano un percorso. Un caso concreto (26/11): Windows scrive solo
  l'IDList quando, alla creazione, la destinazione non è raggiungibile (per esempio un collegamento
  a un'unità di rete creato mentre l'unità non è collegata). Leggerlo vorrebbe dire decodificare
  gli elementi della IDList (unità, poi un nome per cartella).
