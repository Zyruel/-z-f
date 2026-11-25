# Idee per [z]f

Cose venute in mente, non ancora decise né in programma. Per ognuna: da dove viene, cosa
servirebbe, cosa la frena rispetto ai principi di `CLAUDE.md`. Quando un'idea diventa lavoro si
sposta in "Prossimi passi" di `CLAUDE.md` e si toglie da qui. Le piccole correzioni sono in
`da-sistemare.md`, le prove a mano in `prove\da-verificare.md`.

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
