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
  nessuno, ma il processo resta vivo lo stesso.
- [ ] **Collegamenti senza LinkInfo** (colonna Destinazione, `lnk.cpp`). Il nostro lettore prende
  il percorso dalla LinkInfo, dalla variabile d'ambiente o dal percorso relativo; un collegamento
  che ha solo l'elenco di elementi della shell (IDList) resta senza destinazione. Sui 177 del PC
  non ce n'è nessuno (tutti uguali a Windows). Da rivedere se su un altro PC ne compare uno vuoto
  dove le proprietà di Windows mostrano un percorso. Un caso concreto (26/11): Windows scrive solo
  l'IDList quando, alla creazione, la destinazione non è raggiungibile (per esempio un collegamento
  a un'unità di rete creato mentre l'unità non è collegata). Leggerlo vorrebbe dire decodificare
  gli elementi della IDList (unità, poi un nome per cartella).
- [ ] **Icona grande mancante alla prima anteprima dopo l'avvio** (novembre 2026, visto una volta
  provando i file solo online). z-f-dev aperto con l'anteprima già accesa su un PNG solo online:
  righe d'informazione senza l'icona grande; tornandoci dopo, l'icona c'era. Causa probabile: il
  thread dell'anteprima (e quelli delle miniature, `thIconWork`) chiede subito `SHGetImageList`,
  senza aspettare `I.shellReady` come il secondo thread delle icone (`icons.cpp`: "insieme, la
  prima chiamata falliva"). Da riprodurre (avvio con l'anteprima aperta su un file senza
  anteprima) e, se è così, aspettare lo stesso evento in `pvBigIcon`.
- [ ] **Icona di un file solo online dopo lo scaricamento** (novembre 2026, da verificare). Per
  .exe, .lnk, .ico la shell dà al segnaposto l'icona generica, e [z]f la tiene in cache per
  percorso (`iconFor`, `thIconKeyFor`); quando il file si scarica (aprendolo) gli attributi
  cambiano ma la chiave no, quindi l'icona vera arriverebbe solo riavviando. Anche le miniature
  aggiornano `t.online` solo se cambiano data o dimensione. Proposta da decidere con Zyruel: in
  `iconSource` un file solo online ha l'icona per estensione (`ICON_EXT`, la stessa immagine
  generica, una richiesta per tipo invece di una per file da 15–40 ms), così allo scaricamento
  la chiave cambia e l'icona vera arriva da sola.
- [ ] **Spostare la preparazione delle icone dopo il primo frame?** (novembre 2026, dal controllo
  di tutto il codice). `iconInit` (`main.cpp`) avvia prima del primo frame il thread delle icone
  per estensione (`IQ_EXT`), che chiama subito `CoInitializeEx` e `SHGetFileInfoW`: così shell32
  e ole32 si caricano all'avvio nonostante `/DELAYLOAD`, su quel thread, mentre il thread UI fa il
  primo frame con WARP (e, nel rendering ibrido, insieme a `hwThread`, sul loader lock di cui
  parla `main.cpp` per il driver della GPU). Il vantaggio di adesso: le icone vere arrivano prima.
  Da misurare con `prove\avvio.bat` (20 avvii alternati, come per l'icona della finestra) con
  `iconInit` dove è e nel blocco "dopo il primo frame": contenuto visibile, primo fuoco, passaggio
  alla GPU (F12) e quanto prima o dopo compaiono le icone vere al posto dei segnaposto (il lock e
  l'evento di `iconInit` servono già a `iconSetSize` e al primo frame: si sposterebbe solo l'avvio
  del thread). Decide
  Zyruel; se resta dov'è, scriverlo come eccezione voluta in ARCHITETTURA.
- [ ] **Icona: misure piccole in PNG?** (novembre 2026, dal controllo prima della 1.0.0-beta.4).
  In `src\z-f.ico` le misure sotto 256 sono BMP a 32 bit (`strumenti\icona.py`, riga 11) e pesano
  42,8 dei 45,7 KB del file, quasi tutto l'aumento dell'exe; in PNG peserebbero molto meno.
  Windows Vista e successivi leggono PNG a ogni misura. Da misurare prima di decidere: dimensione
  dell'exe e `A.iconMs` in F12 ("icona della finestra": `LoadImageW` deve decomprimere il PNG),
  con le due versioni del .ico. Decide Zyruel.
- [ ] **Icona: misure 28 e 56 px (ed eventualmente 36 e 72)** (novembre 2026, stesso controllo).
  A 175% (il monitor di Zyruel) Windows chiede 28 px per l'icona piccola e 56 per la grande, che il
  .ico non ha (`MISURE` in `strumenti\icona.py`): le ricava riducendo 32 e 64 (o 24 e 48), e i
  bordi agganciati ai pixel escono dai pixel. A 225% servirebbero 36 e 72. Aggiungerle allunga il
  file (vedi la voce sopra: in PNG pesano poco). Da misurare come sopra, e da guardare a 175% la
  barra delle applicazioni e Alt+Tab prima e dopo. Decide Zyruel.
