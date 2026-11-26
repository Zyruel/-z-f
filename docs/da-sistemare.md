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
- [ ] **"vswhere.exe non è riconosciuto" durante `build.bat`** (novembre 2026). Compare nella
  compilazione MSVC, che però va a buon fine. Probabilmente viene da `vcvars64.bat` di Visual
  Studio (riga 20 di `build.bat`: solo l'uscita normale va in `nul`, gli errori no), non dalla
  chiamata a vswhere dello script. Da capire, poi zittirlo o correggerlo.
- [ ] **Il processo a volte resta vivo 5–8 s dopo la chiusura della finestra** (novembre 2026,
  visto durante le prove delle miniature). La finestra sparisce subito, ma `z-f-dev.exe` (e anche
  la build di prima, alla pari) esce dopo 1 s di solito e dopo 5–8 s una volta su tre: misurato
  con `taskkill /PID` sulla cartella di prova, 9 chiusure. Forse un thread che a quel punto sta
  ancora aspettando (spazio dei percorsi di rete, shell). Nessun effetto visibile; da capire con
  un dump del processo in quel momento, e da guardare se l'aggiornamento (`updRestartAll`) aspetta
  l'uscita delle finestre.
- [ ] **Avviso C4456 con `/W4` in `zip.cpp`** (0.26, riga 302: un secondo `ascii` nasconde quello
  della riga 262). Con `/W3` di `build.bat` non compare; dalla 0.22 il codice era pulito anche con
  `/W4`. Basta rinominare la variabile interna.
- [ ] **Collegamenti senza LinkInfo** (colonna Destinazione, `lnk.cpp`). Il nostro lettore prende
  il percorso dalla LinkInfo, dalla variabile d'ambiente o dal percorso relativo; un collegamento
  che ha solo l'elenco di elementi della shell (IDList) resta senza destinazione. Sui 177 del PC
  non ce n'è nessuno (tutti uguali a Windows). Da rivedere se su un altro PC ne compare uno vuoto
  dove le proprietà di Windows mostrano un percorso.
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
- [ ] **Anteprima di uno zip dentro uno zip** (novembre 2026, visto provando lo zip nello zip).
  Selezionando `interno.zip` dentro `esterno.zip`, l'anteprima dice "Cartella vuota" e in fondo
  "vuota · 722 MB": dentro lo zip la voce è trattata come una cartella senza figli, perché il suo
  contenuto non si legge senza estrarla. Proposta: per uno zip dentro uno zip, l'anteprima di un
  file (icona, tipo e dimensione) invece di quella di una cartella vuota.
