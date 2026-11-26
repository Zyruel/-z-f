# Piccole cose da sistemare

Dettagli trovati lavorando, che non meritano una sessione propria: si sistemano quando si tocca
quel codice, o tutti insieme in un momento tranquillo. Quando una voce è fatta si toglie da qui
(la storia resta in git). Le idee nuove sono in `idee.md`, le prove a mano in
`prove\da-verificare.md`, i debiti tecnici più grandi in `CLAUDE.md`.

- [ ] **Codifica dei nomi negli zip vecchi** (0.26, `zip.cpp`, `CP_OEMCP` intorno alla riga 259).
  I nomi senza il bit UTF-8 si leggono nella codepage OEM del sistema (850 in Italia), come li
  scrivono Esplora file e 7-Zip, e non in 437 come dice la specifica. Le accentate minuscole sono
  uguali nelle due; À, Á e simili no. Da confermare con Zyruel, magari dopo aver aperto uno zip
  vecchio creato su un PC americano o con un altro programma.
- [ ] **Il processo a volte resta vivo 5–8 s dopo la chiusura della finestra** (novembre 2026,
  visto durante le prove delle miniature). La finestra sparisce subito, ma `z-f-dev.exe` (e anche
  la build di prima, alla pari) esce dopo 1 s di solito e dopo 5–8 s una volta su tre: misurato
  con `taskkill /PID` sulla cartella di prova, 9 chiusure. Forse un thread che a quel punto sta
  ancora aspettando (spazio dei percorsi di rete, shell). Rivisto il 26/11 con un percorso di rete
  lento (F12 "barra laterale: rete e ottiche" 47 s per una voce): vivo più di 10 s dopo la
  chiusura, poi uscito. Nessun effetto visibile; da capire con
  un dump del processo in quel momento, e da guardare se l'aggiornamento (`updRestartAll`) aspetta
  l'uscita delle finestre. Rivisto ancora il 26/11 (principale chiusa con `WM_CLOSE`, 6 volte):
  uscita dopo 2,5–3,9 s ogni volta. Fino alla correzione del mutex (chiuso ora in `WM_DESTROY`)
  faceva aspettare 4,7 s chi apriva una cartella o uno zip subito dopo; ora non fa più aspettare
  nessuno, ma il processo resta vivo lo stesso. Un indizio (26/11, prova dello spostamento di
  `iconInit`, vedi ARCHITETTURA, "DLL a caricamento ritardato"): con `--prova-avvio`, che chiude
  la finestra appena arriva la prima icona, il processo esce in 0,2 s quando la prima
  `SHGetFileInfoW` parte prima del primo frame, e resta vivo circa 7 s (fino a 14) quando parte
  dopo. L'uscita sembra aspettare qualcosa che la shell avvia in background alla prima chiamata, e
  che si sovrappone o no al resto dell'avvio: da guardare nel dump.
- [ ] **Collegamenti senza LinkInfo** (colonna Destinazione, `lnk.cpp`). Il nostro lettore prende
  il percorso dalla LinkInfo, dalla variabile d'ambiente o dal percorso relativo; un collegamento
  che ha solo l'elenco di elementi della shell (IDList) resta senza destinazione. Sui 177 del PC
  non ce n'è nessuno (tutti uguali a Windows). Da rivedere se su un altro PC ne compare uno vuoto
  dove le proprietà di Windows mostrano un percorso. Un caso concreto (26/11): Windows scrive solo
  l'IDList quando, alla creazione, la destinazione non è raggiungibile (per esempio un collegamento
  a un'unità di rete creato mentre l'unità non è collegata). Leggerlo vorrebbe dire decodificare
  gli elementi della IDList (unità, poi un nome per cartella).
- [ ] **Icona di un file solo online dopo lo scaricamento** (novembre 2026, da verificare). Per
  .exe, .lnk, .ico la shell dà al segnaposto l'icona generica, e [z]f la tiene in cache per
  percorso (`iconFor`, `thIconKeyFor`); quando il file si scarica (aprendolo) gli attributi
  cambiano ma la chiave no, quindi l'icona vera arriverebbe solo riavviando. Anche le miniature
  aggiornano `t.online` solo se cambiano data o dimensione. Proposta da decidere con Zyruel: in
  `iconSource` un file solo online ha l'icona per estensione (`ICON_EXT`, la stessa immagine
  generica, una richiesta per tipo invece di una per file da 15–40 ms), così allo scaricamento
  la chiave cambia e l'icona vera arriva da sola.
