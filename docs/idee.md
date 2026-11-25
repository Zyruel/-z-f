# Idee per [z]f

Cose venute in mente, non ancora decise né in programma. Per ognuna: da dove viene, cosa
servirebbe, cosa la frena rispetto ai principi di `CLAUDE.md`. Quando un'idea diventa lavoro si
sposta in "Prossimi passi" di `CLAUDE.md` e si toglie da qui. Le piccole correzioni sono in
`da-sistemare.md`, le prove a mano in `prove\da-verificare.md`.

## Distribuzione

### Aggiornamento automatico dalle Release (in progetto, novembre 2026)

Deciso con Zyruel di farlo; restano da decidere i punti segnati "da decidere". [z]f controlla le
Release di GitHub, scarica la versione nuova e sostituisce il proprio exe; la versione nuova parte
al prossimo avvio. Niente installatore, niente servizi, niente processi in più.

- **Da dove:** l'API pubblica delle Release, senza login (60 richieste l'ora per indirizzo IP; ne
  serve una al giorno). `GET /repos/Zyruel/-z-f/releases` dà tutte le versioni, beta comprese
  (`/releases/latest` salta le pre-release); di ognuna servono `tag_name`, `prerelease`, `draft` e
  l'asset `z-f-<versione>.exe` con `browser_download_url`, `size` e `digest` (`sha256:…`, verificato
  sulla 1.0.0-beta.2). Un lettore JSON minimo nostro, solo per questi campi.
- **Rete:** WinHTTP (`winhttp.dll`, in Windows), a caricamento ritardato come shell32 e ole32 e
  usata solo dal thread dell'aggiornamento: all'avvio non si carica niente in più. Proxy come il
  sistema (`WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY`), tempi massimi brevi; il redirect verso il server
  dei file di GitHub lo segue WinHTTP.
- **Quando:** un thread a priorità bassa parte 30 s dopo il primo frame, al massimo una volta ogni
  24 ore (`aggiornamenti_ultimo` in `impostazioni.txt`), e con "Controlla aggiornamenti" (menu ≡ e
  opzioni). Il thread dell'interfaccia riceve solo il risultato con `PostMessage` e un contatore
  di generazione: non tocca mai la rete né il disco.
- **Quale versione:** confronto SemVer (`1.0.0-beta.2` < `1.0.0-beta.10` < `1.0.0-rc.1` < `1.0.0`).
  Impostazione `aggiornamenti=beta|stabili|no`: chi ha una beta segue le beta, chi ha una stabile
  solo le stabili. Mai indietro di versione.
- **Download e verifica:** nella cartella dell'exe come `z-f-<versione>.exe.part` (stesso disco,
  così lo scambio è una rinomina); poi si controllano dimensione e SHA-256 (`BCryptHash`, in
  Windows) con il `digest` dell'API. Se la cartella non è scrivibile (per esempio Programmi), niente
  download: l'avviso dice che c'è una versione nuova.
- **Firma (da decidere):** l'impronta protegge da un download rovinato, non da chi entrasse
  nell'account GitHub. Con una firma ECDSA P-256 (`BCryptVerifySignature`, in Windows) la chiave
  privata resta sul PC di Zyruel, fuori dal repository; ogni Release ha anche
  `z-f-<versione>.exe.sig`, e un programma in `prove\` firma l'exe al rilascio. La chiave pubblica
  è dentro [z]f. Se la chiave si perde, gli exe già distribuiti non accettano più aggiornamenti:
  serve una copia di sicurezza.
- **Scambio:** Windows permette di rinominare un exe in esecuzione: `z-f.exe` → `z-f.old.exe`,
  `.part` → `z-f.exe` (`MoveFileExW`); se il secondo passo fallisce si rimette il primo. Il nome
  dell'exe è quello vero (`GetModuleFileNameW`), qualunque sia. Il percorso non cambia, quindi
  "Usa [z]f al posto di Explorer" continua a funzionare.
- **Dopo (da decidere: solo al prossimo avvio, o anche "Riavvia ora"):** avviso "[z]f 1.0.0-beta.3
  è pronto: sarà attivo al prossimo avvio" (`notice`, nessuna finestra di dialogo). La versione
  nuova, al primo frame riuscito, cancella `z-f.old.exe` da un thread e dice "Aggiornato a [z]f
  1.0.0-beta.3". Se non arriva al primo frame, il vecchio exe resta lì: rimetterlo è una rinomina.
- **Mai su:** `z-f-dev.exe` e le build compilate in locale (un `#define` di `build_zig.bat` e
  `build.bat debug`), le finestre aperte con un'opzione `--`, gli exe su un percorso di rete.
- **Opzioni:** sezione nuova "Aggiornamenti" in fondo al pannello: interruttore "Aggiornamenti
  automatici", interruttore "Ricevi anche le beta", riga "Controlla ora" con lo stato in `C_FAINT`
  ("Ultimo controllo: oggi alle 10:42 · nessuna novità"). Da aggiungere a `DESIGN.md`.
- **Registro:** `z-f-log.txt` scrive ogni controllo, download, verifica e scambio con i tempi.
- **Frena:** l'antivirus può guardare con sospetto un exe senza firma Authenticode che si
  sostituisce da solo, e un certificato costa. Il download con WinHTTP non aggiunge il segno
  "scaricato da Internet", quindi niente SmartScreen. Da misurare: costo del thread, tempo di
  controllo e di download, nessun effetto sull'avvio.
- **Prove:** una Release di prova (pre-release, poi cancellata) per l'intero giro: controllo,
  download, verifica, scambio, riavvio, pulizia; poi i casi brutti (rete assente, file rovinato,
  cartella in sola lettura, due finestre aperte).

## Archivi

### Modificare gli zip (novembre 2026, dopo la 0.26)

Oggi gli zip sono in sola lettura (`zip.cpp`, `zipout.cpp`): Rinomina, Elimina, Taglia, Incolla e
Nuova cartella sono disattivati, e non si rilascia niente dentro uno zip.

- **Cosa servirebbe:** uno zip non si modifica sul posto. Si riscrive un archivio nuovo accanto,
  copiando così come sono (senza decomprimere) le voci che restano, poi lo si sostituisce con una
  rinomina, come fa `saveDataAsync`. Rinominare ed eliminare costano solo la copia dei byte.
  Aggiungere file richiede un compressore deflate nostro (più lungo dell'inflate), oppure si
  salvano con il metodo "stored" (veloce ma senza compressione).
- **Frena:** è lento sugli archivi grandi (ogni modifica riscrive tutto), va annullato con
  `undoPush` come le altre operazioni, e se qualcosa va storto non deve mai lasciare uno zip
  rovinato. Da fare solo se qualcuno lo chiede.

### 7z, rar e altri formati (novembre 2026, dopo la 0.26)

Oggi solo zip (metodi "stored" e deflate). Deflate64, bzip2, LZMA, zstd e gli zip cifrati
passano a Windows.

- **7z:** il formato ha un'intestazione complicata (spesso compressa a sua volta) e usa LZMA/LZMA2.
  Un decodificatore LZMA nostro è fattibile (qualche centinaio di righe, come l'inflate). Il resto
  del formato è il lavoro grosso.
- **rar:** formato proprietario. Il codice ufficiale per estrarre (unrar) ha una licenza che lo
  permette, ma è una dipendenza grande: contro il principio della leggerezza.
- **Strada da verificare:** Windows 11 (dalla 23H2) apre da sé 7z, rar e tar in Esplora file, con
  libarchive (`archiveint.dll`, non documentata). Usarla darebbe tutti i formati senza scrivere
  decodificatori, ma è un'interfaccia non pubblica che può cambiare. In alternativa, come adesso
  per gli zip non supportati, si lasciano a Windows.
- **tar e tar.gz:** facili (tar è un elenco di intestazioni, gz è deflate: l'inflate c'è già).
  Andrebbero letti tutti per fare l'elenco, perché non hanno una directory centrale.

## Rete

### "Aggiungi un percorso di rete…" dal menu di Questo PC (novembre 2026)

I percorsi di rete di Windows ora si vedono sotto Questo PC. Per crearne uno serve oggi Explorer.

- **Cosa servirebbe:** una voce nel menu di Questo PC (barra laterale e sfondo della lista),
  accanto a "Connetti unità di rete…", che apre la procedura di Windows ("Aggiunta guidata
  risorse di rete"), come fa Explorer, su un thread; alla chiusura si rilegge la barra
  (`sideLoad`). Non è un dialogo nostro, come quello per connettere un'unità.
- **Frena:** poco. Va trovato il modo giusto di aprirla (il comando di Windows non è documentato).

### Condivisioni visitate (novembre 2026)

Mostrare tra le unità le condivisioni già aperte (per esempio quelle di un NAS),
ricavate dalle cartelle recenti, con lo spazio libero. Windows non tiene un elenco simile.

- **Cosa servirebbe:** ricavare da `recenti.txt` le radici `\server\condivisione` e mostrarle
  sotto Questo PC dopo i percorsi di rete, con lo spazio letto dal thread dei percorsi di rete.
- **Frena:** una voce che compare da sola va anche tolta (cestino come i preferiti, o una voce di
  menu), e si mescola con i percorsi di rete veri. Forse basta creare un percorso di rete.
