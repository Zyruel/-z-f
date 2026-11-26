# [z]f — regole di interfaccia e di design

Questo documento descrive come è fatto [z]f e come deve continuare a essere fatto. Serve a non
reinventare niente: ogni elemento nuovo si costruisce con i componenti, i colori, le misure e i
comportamenti che ci sono già. Si inventa qualcosa solo quando serve un elemento davvero nuovo, e
in quel caso lo si aggiunge qui.

I valori stanno in `src/theme.h`, con un nome per ognuno. Questo file spiega quando usarli. Se i
due non coincidono, va corretto uno dei due nello stesso momento.

---

## 1. Principi

1. **Leggerezza.** Niente framework, niente dipendenze, niente processi in più. Ogni byte e ogni
   millisecondo all'avvio va giustificato.
2. **Prestazioni misurate, non presunte.** Ogni operazione importante ha un tempo misurato (barra
   di stato, F12, `z-f-log.txt`). Un cambiamento che peggiora un tempo si discute prima di tenerlo.
3. **Il thread dell'interfaccia non tocca mai il disco** e non aspetta mai niente di lento: shell,
   rete, unità. Tutto ciò che può bloccare va su un thread; l'interfaccia mostra quello che ha.
4. **Zero CPU a riposo.** Si ridisegna solo dopo un input o finché qualcosa si muove.
5. **Pulizia.** Poco testo, poche linee, nessuna decorazione senza funzione. Se un elemento non
   aiuta a capire o a fare qualcosa, non c'è.
6. **Coerenza.** Lo stesso concetto ha sempre lo stesso aspetto, lo stesso nome e la stessa
   scorciatoia, ovunque compaia.
7. **Mai interrompere.** Niente finestre di dialogo nostre e niente conferme: si agisce e si
   avvisa nella barra di stato. Le uniche conferme sono quelle della shell di Windows, per
   esempio l'eliminazione definitiva. Anche chiudere non chiede niente: con un'operazione sui file
   in corso la finestra sparisce subito e il processo esce quando l'operazione è finita.
   Eccezione: se Direct3D non parte all'avvio, un `MessageBox` lo dice (non c'è altro modo).

---

## 2. Colori

Tema scuro unico. Sono ammessi solo i colori di `theme.h`: un colore nuovo si aggiunge lì, con un
nome che dice a cosa serve.

| Nome | Uso |
|---|---|
| `C_INPUT` | fondo delle caselle di testo |
| `C_TABBAR` | barra delle schede, barra laterale (contorno, più scuro dello sfondo) |
| `C_BG` | sfondo della lista |
| `C_HEADER` | intestazione delle colonne |
| `C_BAR` | barra del percorso, barra di stato, scheda attiva |
| `C_MENU` | menu, tooltip, pannelli sovrapposti |
| `C_LINE` | separatori e bordi, sempre 1 px |
| `C_SHADOW` | ombra dei pannelli sovrapposti |
| `C_HOVER` | elemento sotto il mouse |
| `C_HOVER_STRONG` | sotto il mouse, su un controllo piccolo dentro un elemento già evidenziato (la × della scheda, la destinazione di un collegamento) |
| `C_SEL` | selezionato; elemento corrente (cartella corrente nella barra laterale, voce attiva di un menu) |
| `C_SEL_HOVER` | selezionato e sotto il mouse |
| `C_SEL_EDGE` | cursore da tastiera |
| `C_SEL_TEXT` | testo selezionato dentro una casella di testo |
| `C_MARQUEE` | fondo del rettangolo di selezione (`C_SEL_EDGE` trasparente) |
| `C_SCROLL` | barra di scorrimento, barra d'uso delle unità |
| `C_TEXT` | testo primario |
| `C_DIM` | testo secondario: date, dimensioni, etichette, scorciatoie |
| `C_FAINT` | testo terziario, voci disattivate, file nascosti, suggerimenti |
| `C_ACCENT` | stato attivo, informazione, filtro, scheda attiva |
| `C_ERR` | errori, spazio su disco oltre il 90% |
| `C_FOLDER`, `C_FILE` | segnaposto delle icone |
| `C_ICON`, `C_ICON_HIDDEN` | icone e immagini come sono (bianco, che moltiplica la texture); file nascosti al 43% |
| `C_CLOSE`, `C_ON_CLOSE` | pulsante chiudi della finestra sotto il mouse: il rosso di Windows (#C42B1C), simbolo bianco |
| `C_KNOB_ON` | pallino di un interruttore acceso, sul fondo `C_ACCENT` |
| `C_SEL_EDGE_PARENT` | pillola dell'unità che contiene la cartella aperta (`C_SEL_EDGE` al 60%) |

**Gerarchia del testo: si fa con il colore** (`C_TEXT` → `C_DIM` → `C_FAINT`), mai con la
dimensione o con il grassetto.

---

## 3. Misure

Tutte in pixel a scala 100%, usate nel codice con `px()`: moltiplica per la scala DPI e
arrotonda per difetto, così i bordi cadono su pixel interi.

- **Spaziature:** `GAP_XS` 2 · `GAP_S` 4 · `GAP_M` 8 · `GAP_L` 12 · `GAP_XL` 16. Un elemento
  nuovo usa questi valori; un valore fuori scala va motivato.
- **Riga:** altezza del testo + `ROW_EXTRA` (9). Unica misura arrotondata al pixel più vicino
  invece che per difetto (`px()`): così le righe restano alte come sono sempre state a 150%, 175%
  e 250%. È l'unità verticale di tutto:
  - righe della lista, voci di menu, voci della barra laterale, intestazione e barra di stato sono alte una riga;
  - la barra del percorso è alta `PATH_BAR` (1,45) righe, la barra delle schede `TAB_BAR` (1,35).
- **Margine orizzontale dei contenuti:** `PAD_X` (12).
- **Barre d'accento** (cursore, scheda attiva, barra d'uso): `EDGE` (2).
- **Linee e bordi:** 1 px, anche ad alta risoluzione.
- **Forme: angoli arrotondati come Windows 11** (le linee guida Fluent, "Geometry in Windows 11"):
  - `R_M` (8): contenitori sovrapposti e di primo livello: menu, pannello delle opzioni, pannello F12, scheda attiva (solo gli angoli in alto);
  - `R_S` (4): elementi nella pagina: sfondo delle righe e delle voci, pulsanti, caselle di testo, campo del filtro; anche i tooltip, perché piccoli;
  - pillole (raggio = metà dell'altezza): interruttori, barre di scorrimento, barre d'uso, indicatore del cursore e della scheda attiva;
  - eccezioni: i pulsanti di sistema (riduci, ingrandisci, chiudi) restano squadrati e a filo, come in Windows; il rettangolo di selezione è squadrato, come in Windows, perché è una forma che segue il mouse e non un elemento della pagina;
  - i rettangoli arrotondati li disegna lo shader (`rrect`: distanza dal bordo, 1 px di antialiasing), anche solo come contorno.
- **Pannelli sovrapposti** (menu, tooltip, pannello delle opzioni, pannello F12, "Vai a"): fondo `C_MENU`,
  bordo 1 px `C_LINE`, ombra spostata di `SHADOW_X`/`SHADOW_Y` (2, 3) in `C_SHADOW`. Li disegna
  tutti `drawPanel`; compaiono con `panelEnter` (sezione 5).

---

## 4. Testo

- **Un solo carattere e una sola dimensione** in tutta l'interfaccia: Segoe UI, 13 px a scala
  100%. L'utente può cambiarli: Ctrl +/−, Ctrl+0, `impostazioni.txt` (Ctrl+rotellina è lo zoom
  della lista, non il testo).
- **Unica eccezione: il testo dentro l'anteprima** (codice, log) usa un carattere a spaziatura
  fissa, Cascadia Mono (altrimenti Consolas), alla stessa dimensione: in Segoe UI il codice perde
  l'allineamento. È contenuto del file, non interfaccia; si sceglie nelle opzioni
  (`carattere_anteprima`). Intestazione, banda in fondo e note dell'anteprima restano in
  Segoe UI.
- **Testo su più righe** (tooltip, testo nell'anteprima): interlinea `LINE_GAP` (3).
- **ClearType** se Windows lo usa, altrimenti scala di grigi.
- **Testo troncato** con "…" alla fine. Il testo completo è nel tooltip.
- **Allineamento:** numeri a destra (dimensioni), tutto il resto a sinistra.

---

## 5. Movimento

Le animazioni ci sono **solo con uno scopo**: dire da dove arriva una cosa, dove è andata, o che
qualcosa è cambiato. Mai per decorazione, e mai così lente da farsi aspettare.

- **Durata:** sotto i 150 ms. L'utente non deve mai aspettare un'animazione per continuare: l'input
  funziona sempre, anche a metà.
- **Due meccanismi soltanto:**
  - *avvicinamento esponenziale* (`approach`, con costante di tempo `TAU_*`) per i valori che
    inseguono un obiettivo: hover, cursore, scorrimento, indicatore della scheda;
  - *durata fissa con ease-out* (`easeOut`, `DUR_*`) per ciò che compare: pannelli (`panelEnter`), icone.
- **Cosa compare:** dissolvenza più uno spostamento di `ENTER_DY` (4 px).
- **Cosa sparisce:** di colpo. Non si aspetta mai che una cosa se ne vada.
- **Salti lunghi:** niente animazione. Se il cursore deve fare più di una schermata e mezza, si
  sposta e basta.
- **Dopo una pausa** il primo frame avanza di un vsync, non del tempo trascorso, altrimenti le
  animazioni partirebbero già finite.
- **Tutto è disattivabile** con `animazioni=0`: i valori arrivano subito all'obiettivo.
- **Un'eccezione, l'anello di punti** (`drawSpinner`, `render.cpp`): l'unica animazione continua.
  Si usa solo mentre un lavoro è davvero in corso, e almeno per un giro (`SPIN_PERIOD`), così un
  lavoro breve non è un lampo; e si vede dove l'utente sta guardando (il controllo degli
  aggiornamenti accanto alla versione). `SPIN_DOTS` (8) punti tondi fermi su un
  cerchio, gira solo la luminosità (dal 100% della testa a `SPIN_MIN_ALPHA` della scia); niente
  glifi ruotati, che con l'atlante rasterizzato diritto si vedrebbero male. Con `animazioni=0` è
  fermo.

| Cosa | Valore |
|---|---|
| hover: entrata / uscita | τ 20 / 50 ms |
| cursore tra le righe | τ 24 ms |
| cartella nuova che entra | τ 35 ms, parte da 25% di opacità e 6 px più in basso |
| cambio di scheda | come la cartella nuova, da 55% di opacità |
| indicatore della scheda attiva | τ 30 ms |
| scorrimento con la rotellina | τ 45 ms |
| menu / tooltip / icona | 90 / 80 / 110 ms |
| anello di punti: un giro | 900 ms, finché il lavoro è in corso e almeno un giro |

---

## 6. Stati di un elemento

Valgono per righe, voci della barra laterale, schede e voci di menu.

| Stato | Aspetto |
|---|---|
| normale | nessun fondo |
| sotto il mouse | `C_HOVER`, in dissolvenza |
| selezionato | `C_SEL`; sotto il mouse `C_SEL_HOVER` |
| corrente (cartella aperta, scheda attiva) | `C_SEL` nella barra laterale; per le schede fondo `C_BAR` e barra `C_ACCENT` in alto |
| cursore da tastiera | pillola verticale `PILL_W` a sinistra in `C_SEL_EDGE` (metà dell'altezza della riga), più un contorno 1 px arrotondato se l'elemento non è selezionato |
| disattivato | testo `C_FAINT`, nessun hover |
| nascosto (file) | testo `C_FAINT`, icona al 43% di opacità |
| in caricamento | "caricamento…" nella barra di stato, solo dopo `LOADING_AFTER` (150 ms) |

---

## 7. Componenti

Ogni elemento nuovo si costruisce con questi. Se serve un componente nuovo, prima si guarda se
uno esistente basta con una piccola estensione.

### Composizione della finestra

```
[⚙] [scheda][scheda][+]  ·····trascina·····              [–][□][×]    barra del titolo (C_TABBAR)
[▯][←][→][↑]  Questo PC › Disco locale (C:) › Users             [☆][◨]    riga del percorso (C_BAR)
Preferiti ˅  │ Nome ↑              Data       Dimensione │ foto.jpg         intestazione (C_HEADER)
  Desktop    │ …                                         │                  lista (C_BG) │ anteprima (C_BG)
Unità ˅      │                                           │  [immagine]
  Disco (D:) │ lettura 0,18 ms · … · avvio 93 ms · F12   │                  barra dei tempi (C_BAR, opzione)
             │ [⏷ Filtra 72 elementi…]  🗀 53 🗋 19 │ ☰ Dettagli │ 4032 × 3024 · … │ 🖼 Immagine   barre in fondo (C_BAR)
```

Barra laterale e anteprima sono le due colonne della finestra e arrivano in fondo, libere. Barra
dei tempi e barra di stato (con il filtro a sinistra, contatori e vista a destra) parlano della
lista e stanno sotto di lei, tra le due. L'anteprima ha in fondo la sua banda, fatta delle stesse
parti e alla stessa altezza: le due bande si leggono come una sola, divisa dal bordo del pannello.

Sopra, quando servono: menu, tooltip, pannello delle opzioni, "Vai a" (sotto la riga del percorso), F12.

Nessuna barra del titolo di Windows: [z]f disegna la sua (`chrome.cpp`). Ogni riga ha un compito;
i comandi stanno dove servono. La barra del titolo è della finestra (⚙ per le opzioni, le schede,
i pulsanti di sistema); la riga del percorso di quello che si guarda (← → ↑ ☆ per la cartella, ▯ e
◨ per i pannelli, ognuno sopra il bordo del pannello che apre); filtro e vista in basso. Non c'è un
menu dell'applicazione: ogni comando ha il suo posto, e un menu che li raccoglie ne sarebbe un
doppione.

- **Lista:**
  - righe alte una riga: icona a `PAD_X`, poi nome, data, dimensione;
  - lo sfondo di hover e selezione è arrotondato `R_S`, con un margine di `GAP_S` ai lati;
  - virtualizzata: si disegnano solo le righe visibili;
  - colonne: Nome prende lo spazio che avanza, Data e Dimensione sono allineate a destra e larghe
    quanto scelto dall'utente (predefinite `DATE_W` 150 e `SIZE_W` 100, minimo `COL_MIN` 48). Il
    nome tiene sempre almeno `COL_NAME_MIN` (160) dall'inizio della lista: se la finestra si
    stringe cedono prima Destinazione (se c'è), poi Data e poi Dimensione, fino al minimo. Il testo
    che non ci sta finisce in "…";
  - **colonna Destinazione** (`lnk.cpp`, `links.cpp`): tra Nome e Data, solo nelle cartelle con
    almeno un collegamento (.lnk), larga `LINK_W` (300) o quanto scelto (`colonna_destinazione`),
    ma mai più larga di Nome (`LINK_MAX`, metà dello spazio tra l'inizio della lista e Data): con
    la finestra stretta le due colonne crescono e calano insieme, e trascinando il bordo
    Nome|Destinazione verso sinistra ci si ferma a metà.
    Ogni collegamento mostra il percorso a cui porta, in `C_DIM` come le date; i collegamenti
    senza percorso (app dello Store) e gli altri file lasciano la cella vuota. Sopra il testo
    della destinazione (non sullo spazio vuoto della colonna) il fondo `C_HOVER_STRONG`,
    arrotondato `R_S`, largo il testo più `GAP_S` per lato e alto la riga meno `GAP_XS` sopra e
    sotto: è un controllo piccolo dentro la riga. Lì il cursore è la manina di Windows
    (`IDC_HAND`), l'unico posto della lista dove un testo è un collegamento da seguire (non durante
    il rettangolo o un trascinamento). Clic: la cartella che contiene la destinazione,
    con la destinazione selezionata ("Apri percorso file", anche nel menu del collegamento);
    clic centrale: in una nuova scheda; il doppio clic non apre altro. Il clic vale al rilascio,
    quindi trascinando da lì parte il rettangolo; Ctrl e Shift selezionano come altrove.
    Tooltip: il percorso intero e "Clic: apri percorso file · clic centrale: nuova scheda". Si
    ordina dalla A, senza destinazione in fondo in tutti e due i versi;
  - **dimensione delle cartelle** (`dirsize.cpp`, opzione `dimensione_cartelle`): nella colonna il
    totale di tutto il contenuto, in `C_DIM` come i file; mentre si conta il parziale è in
    `C_FAINT`; una cartella illeggibile resta vuota. Si conta in background, una cartella alla
    volta nell'ordine della vista; ordinando per Dimensione le cartelle usano il totale già noto.
    Nel tooltip la riga "Dimensione" ("2,9 GB  (3.128.245.592 byte) · 17.812 file"; mentre
    conta "almeno 1,2 GB…"; "accesso negato").
- **Intestazione colonne:** testo `C_DIM`, freccia ↑/↓ sulla colonna di ordinamento. Clic per
  ordinare, di nuovo per invertire.
  - **bordi tra le colonne** (Nome|Data e Data|Dimensione; con i collegamenti Nome|Destinazione e
    Destinazione|Data), a metà dello spazio tra i testi; si
    afferrano solo qui, in una fascia di `COL_GRIP` (4) per lato. Sopra la fascia: cursore ↔ di
    Windows e una linea verticale 1 px `C_LINE`, che diventa `C_SEL_EDGE` mentre la si trascina.
    Un clic sulla fascia non ordina;
  - ogni bordo comanda la colonna alla sua destra e segue il mouse (trascinando Data|Dimensione
    Data si sposta intera); allargando Data o Dimensione, lo spazio lo cede Destinazione, fino al
    suo minimo. Esc durante il trascinamento rimette la larghezza di prima;
  - doppio clic sul bordo: la colonna si adatta al testo più largo tra intestazione (con la
    freccia) e voci della vista;
  - le larghezze valgono per tutte le cartelle e le schede e si salvano al rilascio
    (`colonna_data`, `colonna_dimensione`, `colonna_destinazione` in `impostazioni.txt`, px a
    scala 100%);
  - clic su un'intestazione: Data e Dimensione partono dal più recente e dal più grande, Nome e
    Destinazione dalla A.
- **Viste e zoom** (Ctrl+rotellina · controllo della vista nella barra di stato · Ctrl+Shift+2/3/4
  per le icone, 5 per Elenco, 6 per Dettagli · menu dello sfondo → "Mostra come
  icone"/"Mostra come dettagli", `thumbs.cpp`): un solo livello di zoom per cartella sceglie la
  vista. **0% Dettagli** (la lista con le colonne, sopra), **10% Elenco**, **da 20% a 100%
  Icone**, a passi di 10% (`ZOOM_*`, `THUMB_PX`). Ctrl+rotellina cambia un livello per scatto e
  tocca solo la lista: barre, menu e testo restano come sono;
  - la stessa lista in altre forme: stesse voci, stesso ordine, filtro, selezione, cursore,
    rettangolo, trascinamento, rinomina, tooltip e anteprima accanto. Le voci stanno su linee
    (righe; in Elenco colonne) e tutte le viste passano dalle stesse funzioni (`itemRect`,
    `itemAt`, `itemBg`);
  - **Elenco**: icona e nome come in una riga di Dettagli (stesse posizioni, stesso segnaposto),
    in colonne larghe `LIST_COL_W` (300) con tante righe quante ne stanno nell'altezza della lista
    sopra la barra di scorrimento, che è orizzontale e in fondo alla lista; a destra non c'è la
    barra verticale, e le colonne arrivano fino al bordo della lista. Si scorre in
    orizzontale: la rotellina di una colonna a scatto; ↑ ↓ una voce, ← → una colonna; nomi lunghi
    in "…" alla fine della colonna. Fondo di hover e selezione con `GAP_XS` di margine ai lati (le
    colonne non si toccano, le righe sì); il rettangolo prende le voci di cui tocca il fondo; vicino
    ai bordi sinistro e destro la lista scorre, come in alto e in basso nelle altre viste.
    Intestazione come in Icone; le colonne tagliate a destra non entrano nell'anteprima;
  - **Icone, voce**: riquadro quadrato di lato `THUMB_PX` (da 64 a 256 per i nove livelli; 96, 160
    e 256 per piccole, medie e grandi) a `GAP_M` dai
    bordi della voce, poi a `GAP_S` il nome su due righe centrate (va a capo dopo uno spazio, un
    punto, un trattino o un trattino basso; la seconda riga finisce in "…"). Le voci di una riga
    si dividono in parti uguali la larghezza della lista: niente spazio avanzato a destra;
  - **miniatura** centrata in orizzontale e appoggiata al fondo del riquadro, mai ingrandita, senza
    cornice. Entra con la dissolvenza delle icone (`DUR_ICON`); lo spazio è fisso, quindi niente
    si sposta quando arriva. Per i tipi che avranno una miniatura (immagini, video, documenti) il
    riquadro resta vuoto finché arriva; per gli altri tipi, e se la miniatura non c'è, l'icona
    grande della shell larga `THUMB_ICON` (metà) del riquadro, centrata;
  - stati come le righe (sezione 6): hover e selezione arrotondati `R_S` su tutta la voce, con un
    margine di `GAP_XS`; cursore con la pillola `PILL_W` a sinistra della voce più il contorno se
    non è selezionata; nascosti col nome in `C_FAINT` e l'immagine al 43%;
  - **intestazione**: resta, allineata a quella dell'anteprima, con "Nome · Data modifica ·
    Dimensione" a sinistra, a `GAP_XL` l'uno dall'altro, in `C_DIM` con la freccia sull'ordinamento.
    Clic ordina, di nuovo inverte. Nessun bordo da trascinare;
  - **tastiera**: ← → una voce, ↑ ↓ una riga (dalla prima o dall'ultima riga non si esce), PagSu
    PagGiù una schermata, Home Fine; Shift e Ctrl come nell'elenco;
  - **mouse**: il trascinamento dei file parte da miniatura e nome di una voce selezionata; da ogni
    altro punto parte il rettangolo, che prende le voci di cui tocca miniatura o nome;
  - rotellina: una riga di voci a scatto;
  - lo zoom si ricorda per cartella (`viste.txt`); le cartelle mai cambiate sono in Dettagli.
- **Archivio zip** (`zip.cpp`, `zipout.cpp`): non è un elemento nuovo, è una cartella. Doppio clic
  o Invio su uno zip lo apre nella lista; il percorso continua oltre l'archivio ("Disco locale (C:) ›
  Download › foto.zip › 2024") e tutto il resto (segmenti, "›", ↑, schede, filtro, ordinamento,
  viste, tooltip, anteprima) è quello di sempre:
  - colonna Dimensione: le cartelle hanno il totale dall'archivio, in `C_DIM` (non c'è un conteggio
    in corso), se è accesa l'opzione delle dimensioni delle cartelle;
  - **sola lettura**: nel menu Taglia, Rinomina, Elimina, Incolla e Nuova cartella restano,
    disattivate (si applicano, ma qui non si può); Proprietà, Apri con, "Apri terminale qui" e
    "Altre opzioni…" non ci sono (la shell non vede dentro lo zip), e Shift+tasto destro apre il
    nostro menu. Da tastiera l'avviso di errore "L'archivio zip è in sola lettura" o "Non
    disponibile dentro un archivio zip". Un trascinamento sopra lo zip non ha destinazione (divieto);
  - aprire, copiare e trascinare usano copie estratte su un thread: se l'estrazione supera
    `LOADING_AFTER`, l'avviso informativo "Estrazione dall'archivio…" resta finché finisce. Copia
    dà "Estratto e copiato: foto.jpg" / "Estratti e copiati 3 elementi"; un errore dice cosa e perché
    ("Impossibile estrarre a.txt: è danneggiato (e altri 2)"). Il trascinamento parte quando i file
    ci sono, col tasto ancora premuto, ed è sempre una copia. Uno zip dentro uno zip si apre nella
    lista solo se è l'ultima apertura chiesta e l'utente è ancora nella scheda e nella cartella da
    cui l'ha chiesto (l'esito di un lavoro non lo porta mai via da dove è andato nel frattempo); un
    file si apre comunque;
  - un file cifrato o compresso in un modo che [z]f non conosce si lascia a Windows: avviso
    informativo ("a.txt è cifrato: si apre in Esplora file") ed Esplora file selezionato sulla voce.
- **Rettangolo di selezione** (`listmouse.cpp`):
  - parte trascinando da qualsiasi punto della lista, **nome compreso**, tranne
    icona e nome di una voce **già selezionata**: da lì parte il trascinamento dei file (per
    trascinare un file non selezionato prima lo si clicca). Comincia oltre la soglia di
    trascinamento di Windows: prima è un clic normale, che vale subito;
  - prende le righe che tocca in altezza e le seleziona mentre si muove. Senza tasti sostituisce
    la selezione, Ctrl alterna le righe toccate, Shift le aggiunge. Alla fine il cursore è sulla
    riga del mouse e l'ancora su quella di partenza;
  - aspetto: fondo `C_MARQUEE`, contorno 1 px `C_SEL_EDGE`, angoli squadrati; disegnato dentro la
    lista, sopra le righe. Durante il rettangolo niente hover e niente tooltip;
  - vicino ai bordi (una riga) o oltre la lista scorre di `DRAG_SCROLL` righe a passo, più veloce
    quanto più il mouse è lontano, fino a `MARQUEE_SCROLL_MAX` volte. Il punto di partenza resta
    attaccato alla sua riga;
  - Esc lo annulla e rimette la selezione di prima del clic;
  - clic sullo spazio vuoto sotto le righe, senza trascinare: toglie la selezione (il cursore
    resta), come Explorer.
- **Barra del titolo** (sempre visibile): a sinistra ⚙ (`IC_SETTINGS`), che apre direttamente le
  opzioni, senza menu (tooltip "Opzioni · Ctrl+,"); poi le schede e "+"; a destra i pulsanti
  di sistema, larghi `CAPTION_W` (46), con la chiusura in rosso Windows (#C42B1C) al passaggio del mouse.
  - tasto destro su ⚙ e "+": niente menu; sulle schede il loro (nuova scheda, nuova finestra,
    chiudi); sullo spazio vuoto quello di sistema;
  - lo spazio vuoto trascina la finestra (doppio clic: ingrandisci; tasto destro: menu di sistema); ne resta sempre almeno `DRAG_MIN`;
  - il bordo superiore ridimensiona; su Windows 11 il pulsante ingrandisci mostra gli snap layout;
  - scheda attiva in `C_BAR` con barra `C_ACCENT` in alto; schede inattive in `C_TABBAR`, separate da un trattino `C_LINE`;
  - la × compare solo sulla scheda attiva o sotto il mouse.
- **Versione e aggiornamento** (`update.cpp`): nel piede delle opzioni, a sinistra, la versione
  ("[z]f 1.0.0-beta.5") in `C_FAINT`, seguita da un'icona di stato larga quanto il font di icone.
  La versione è un collegamento alla sua Release su GitHub (novità e download): sotto il mouse il
  fondo `C_HOVER` arrotondato `R_S`, largo il testo più `GAP_S` per lato, e la manina di Windows
  (`IDC_HAND`), come la destinazione di un collegamento nella lista. L'icona di stato:
  - non ancora controllato o errore: frecce circolari (`IC_SYNC`) in `C_FAINT`;
  - controllo in corso: l'anello di punti in `C_DIM` (§5), per almeno un giro (`SPIN_PERIOD`):
    un controllo dura 300–800 ms e altrimenti si vedrebbe solo un lampo;
  - aggiornato: cerchio pieno con la spunta (`IC_DONE`) in `C_DIM`;
  - versione nuova pronta: freccia di download (`IC_DOWNLOAD`) in `C_ACCENT`, e il testo diventa
    "Aggiorna a [z]f …" (da scaricare a mano: "Scarica [z]f …") in `C_TEXT`, cliccabile insieme
    all'icona. Sul pulsante ⚙ compare un pallino `C_ACCENT` di `BADGE` (6) px, a `BADGE_INSET`
    (8) px dall'angolo in alto a destra, e il suo tooltip aggiunge la stessa frase.
  Un clic sull'icona controlla di nuovo con le opzioni aperte (l'anello, poi l'icona del
  risultato); con una versione pronta chiude e riapre tutte le finestre di [z]f con le loro
  schede. L'esito arriva come avviso nella barra di stato ("… o subito dalle opzioni"), mai con un
  dialogo.
- **Pulsanti a icona** (⚙, ▯, ← → ↑, ☆, ◨, pulsanti di sistema): quadrati o larghi `NAV_W` righe,
  icona centrata, hover come le voci. Disattivati in `C_FAINT` (← senza cronologia). Uno stato
  attivo si mostra col colore dell'icona (☆ piena in `C_ACCENT` se la cartella è tra i preferiti).
  Il doppio clic ripete il comando solo su ← → ↑ (due passi); su ⚙ ▯ ☆ ◨ conta come un clic.
  Nel pannello delle opzioni il secondo clic conta come un clic, tranne sulla versione e
  sull'icona dell'aggiornamento (aprirebbero la pagina due volte).
- **Icone:** dal font di icone di Windows (Segoe Fluent Icons, Segoe MDL2 Assets su Windows 10),
  scala di grigi, dimensione 1,15 × testo; codici in `text.cpp` (`IC_*`). Niente immagini nel
  programma. Le icone di file e cartelle restano quelle della shell.
- **Barra del percorso:** percorso a segmenti ("Questo PC › Disco locale (C:) › Users › utente").
  - segmenti con hover come le voci; antenati in `C_DIM`, cartella corrente in `C_TEXT`, separatore "›" in `C_FAINT`;
  - clic su un segmento: ci va; su "›": menu delle sottocartelle (il "›" resta evidenziato finché il menu è aperto); clic centrale: nuova scheda; spazio vuoto: "Vai a" dalla cartella aperta (vedi "Vai a");
  - nomi come altrove: unità con la loro etichetta, preferiti con il loro nome;
  - se non c'è spazio i primi livelli finiscono in "…", che li elenca in un menu;
  - a sinistra ▯ ← → ↑, a destra ☆ ◨. I due pannelli ai due capi della riga, ognuno sopra il
    bordo del pannello che apre: ▯ (barra laterale, icona DockLeft) e ◨ (anteprima, DockRight),
    gemelli, in `C_TEXT` se il pannello è aperto e `C_DIM` se è chiuso. La vista non sta qui: è
    nella barra di stato.
- **Pannello di anteprima** (Alt+P · ◨, `preview.cpp`): non è un
  pannello sovrapposto ma una parte della finestra, a destra della lista, dall'intestazione al
  fondo della finestra, come la barra laterale; lista, barra dei tempi e barra di stato finiscono
  alla sua sinistra (`A.listR`).
  - fondo `C_BG` come la lista, separatore verticale 1 px `C_LINE`; in alto un'intestazione alta
    una riga e allineata a quella delle colonne (`C_HEADER`, linea `C_LINE` sotto) con il nome
    della voce in `C_TEXT` (è il contenuto, non un'etichetta);
  - contenuto a `PAD_X` dai lati e `GAP_M` sotto l'intestazione;
  - **banda in fondo**, sempre, come la barra di stato e con le sue stesse parti (`drawBarBand`,
    `drawBarCounts`, `drawBarView` in `render.cpp`): fondo `C_BAR` alto `statusH`, linea `C_LINE`
    in alto, alla stessa altezza della barra di stato. All'estremo destro il **tipo di contenuto**,
    disegnato come il controllo della vista ma senza fondo sotto il mouse, perché non si sceglie:
    "Dettagli" (cartella, con l'icona di Dettagli: le righe sono quelle), "Testo" (Document),
    "Immagine" (Photo), "Informazioni" (Info: gli altri file e le unità); largo quanto il più
    largo dei quattro, così i dati prima non si spostano. Prima, dopo la barretta, i **dati** in
    `C_DIM`: per una cartella i contatori di cartelle e file con le icone, come la lista ("+"
    dopo i numeri se la lettura si è fermata prima del fondo); per un testo codifica · righe ·
    dimensione; per un'immagine larghezza × altezza · dimensione · formato. Informazioni non ha
    dati (sono già nelle righe), e senza dati non c'è la barretta. Il totale di una cartella non
    c'è: sta nella colonna Dimensione e nel tooltip;
  - **immagine**: centrata nello spazio, 1:1 (la decodifica la riduce già alla misura), mai
    ingrandita; nessuna cornice, nessuna scacchiera dietro la trasparenza;
  - **testo**: carattere dell'anteprima (sezione 4), `C_TEXT`, a capo sulle parole, interlinea
    `LINE_GAP`; "File vuoto" in `C_FAINT`;
  - **cartella**: righe alte una riga come la lista (icona, nome, dimensione a destra in `C_DIM`),
    nascoste in `C_FAINT` con l'icona attenuata; nessun hover né selezione: non si clicca;
  - **altri file, unità**: icona grande della shell (`PREVIEW_ICON`, 96, mai ingrandita) centrata
    in alto, poi le righe del tooltip (etichetta `C_DIM`, valore `C_TEXT`, note `C_FAINT`);
  - testo e cartella scorrono con la rotellina sopra il pannello (tre righe a scatto, senza
    animazione, come i menu che scorrono) e mostrano la barra `EDGE` di `drawPanelScroll`;
  - **cambio di voce senza animazione**: con le frecce sarebbe solo rumore. Il contenuto
    precedente resta finché arriva il nuovo; se tarda più di `LOADING_AFTER` si vedono il nome e
    "caricamento…" in `C_FAINT`;
  - **larghezza**: predefinita `PREVIEW_W` (360), minima `PREVIEW_MIN` (220), al massimo
    `PREVIEW_MAX` (metà) della finestra. Il bordo con la lista si afferra come quelli delle
    colonne (fascia `COL_GRIP` verso il pannello, `GAP_XS` verso la lista per non prendere la
    barra di scorrimento): cursore ↔, separatore in `C_SEL_EDGE` mentre lo si trascina, Esc torna
    a prima, doppio clic torna a `PREVIEW_W`; si salva al rilascio (`anteprima_larghezza`);
  - **finestra stretta**: il pannello tiene la larghezza scelta e cedono le colonne (prima
    Destinazione, poi Data e Dimensione, come sopra); il pannello si stringe solo quando le colonne
    sono tutte al minimo, e sotto `PREVIEW_MIN` sparisce, ma resta aperto e torna quando c'è
    spazio. Aprendolo senza spazio: avviso. (Scelta con Zyruel: se il pannello cedesse per primo,
    ogni colonna stretta col mouse lo allargherebbe, e allargare una colonna sarebbe impossibile.)
- **Barra laterale:**
  - la sezione "Preferiti" ha il titolo in `C_FAINT` e una freccia ˅/› a destra; clic sul titolo: la sezione si chiude o si apre (ricordato);
    le voci stanno sotto il titolo rientrate di `SIDE_INDENT`, come le unità sotto Questo PC (lo
    sfondo di hover e selezione resta largo quanto la barra);
  - **preferiti col mouse**: il clic vale al rilascio, così una voce si trascina tra le altre
    (oltre la soglia di trascinamento di Windows): linea d'inserimento `C_SEL_EDGE` alta `EDGE`,
    la stessa di quando si rilasciano cartelle da fuori; Esc annulla. Cambia solo l'ordine, mai la
    cartella vera. Sotto il mouse la voce ha a destra, nella colonna delle frecce dei titoli, un
    cestino (`C_DIM`, `C_TEXT` sopra di esso con il fondo `C_HOVER_STRONG`, in dissolvenza con
    l'hover). **Conferma sul posto, niente dialoghi**: il primo clic lo fa diventare `C_ERR` con
    "Togli?" accanto; il secondo toglie la voce (avviso "Tolta dai preferiti"); uscendo dalla voce
    o con Esc la conferma si annulla;
  - voci con icona e nome; le unità hanno lo spazio libero a destra e la barra d'uso sotto;
  - dopo i preferiti, a `GAP_L`, "Questo PC" (icona della shell) come nodo dell'albero, come
    nella navigazione di Explorer, con le unità sotto rientrate di `SIDE_INDENT`. Ha a destra la
    freccia ˅/› dei titoli (`C_FAINT`, `C_DIM` sotto il mouse): clic sulla freccia, il ramo delle
    unità si chiude o si apre (ricordato); clic sul resto della voce, ci va. È evidenziata quando
    è aperta; a ramo chiuso porta la pillola `C_SEL_EDGE_PARENT` se la cartella aperta è su
    un'unità. Non riceve file trascinati. Menu (anche col tasto destro sulla freccia): Apri, Apri
    in nuova scheda, Connetti unità di rete…, Aggiungi percorso di rete…, Proprietà;
  - preferiti vuoti: una riga con la scorciatoia che li aggiunge, in `C_FAINT`;
  - unità di rete: il nome della condivisione come in Explorer ("Condivisione (G:)", o quello dato
    dall'utente), non l'etichetta del disco sul server; `\\server\condivisione` nel tooltip
    ("Percorso di rete"). Nel loro menu, prima di Proprietà, "Disconnetti";
  - unità di rete e ottiche, percorsi di rete e preferiti su di loro arrivano dopo le voci locali,
    una alla volta: intanto hanno il nome generico ("Unità di rete (G:)", senza spazio né barra
    d'uso) o quello della lettura precedente. Niente "caricamento…": la voce c'è già e si completa;
  - **percorsi di rete** di Windows (una condivisione senza lettera, da "Aggiungi un percorso di
    rete"): dopo le unità, sotto Questo PC, come in Explorer. Icona di cartella (quella di
    Explorer, e niente rete per chiederla), il nome dato in Windows, spazio libero e barra d'uso
    come le unità quando arrivano; la pillola come le unità, sulla voce che contiene la cartella
    aperta. Tooltip: Tipo "Percorso di rete", `\\server\condivisione`, spazio. Menu: Apri, Apri in
    nuova scheda, Proprietà. In "Questo PC" nella lista hanno il tipo "Percorso di rete";
  - arriva in fondo alla finestra, con il bordo `C_LINE` per tutta l'altezza, e resta tutta per
    le sue voci;
  - **larghezza**: predefinita `SIDE_W` (210), minima `SIDE_MIN` (150), al massimo `SIDE_MAX`
    della finestra. Il bordo si afferra come quello dell'anteprima e delle colonne (fascia
    `COL_GRIP` per lato, per tutta l'altezza): cursore ↔, bordo in `C_SEL_EDGE` mentre lo si
    trascina, Esc torna a prima, doppio clic torna a `SIDE_W`; si salva al rilascio
    (`barra_laterale_larghezza`). Una barra più larga toglie spazio prima all'anteprima.
- **Barra di stato** (sotto la lista, tra barra laterale e anteprima):
  - a sinistra, a `GAP_S` dall'inizio della lista, il **campo del filtro**: largo `FILTER_W` (al
    massimo `FILTER_MAX` della barra), ma mai sopra i contatori, a `GAP_L` da loro; con la lista
    tanto stretta da lasciargli meno di `FILTER_MIN` (2) altezze del campo non si disegna (il filtro si scrive
    lo stesso). Fondo `C_INPUT`; l'imbuto sta nella colonna
    delle icone della lista e il testo in quella dei nomi (`A.iconX`, `A.nameX`); vuoto mostra
    "Filtra N elementi…" in `C_FAINT`; attivo ha la sottolineatura `C_ACCENT` e la × per
    toglierlo. Si scrive in qualsiasi punto della lista;
  - all'estremo destro, prima del bordo dell'anteprima, il **controllo della vista**: icona della vista (BulletedList, List,
    ViewAll) in `C_FAINT` e nome ("Dettagli", "Elenco", "Icone") in `C_DIM`, largo sempre
    quanto il nome più largo, così cambiando vista non si sposta niente. Sotto il mouse, e finché
    il suo menu è aperto, fondo `C_HOVER` arrotondato `R_S` come i pulsanti della riga del
    percorso. Clic: menu delle viste, sopra il controllo e allineato al suo bordo destro (Dettagli,
    Elenco, Icone piccole, medie, grandi con le scorciatoie; la vista attuale col segno di
    spunta). La rotellina sopra il controllo cambia lo zoom come Ctrl+rotellina. Nessuna
    percentuale: lo zoom si vede dalla lista;
  - prima, a `GAP_M`, una barretta verticale 1 px `C_LINE` alta quanto il campo del filtro meno
    `GAP_XS` per parte; poi, a `GAP_L`, i contatori di cartelle e file (con le icone, della vista:
    rispettano il filtro) e, prima, la selezione in `C_ACCENT` ("3 selezionati  12 KB   ·   ");
  - dopo il filtro, a `GAP_L`, avvisi e "caricamento…", che finiscono in "…" a `GAP_L` dalla
    parte destra.
- **Barra dei tempi** (opzione "Barra dei tempi", `barra_tempi`, spenta di solito): sopra la barra
  di stato, alta una riga, stessa larghezza; i tempi misurati in `C_FAINT` da `PAD_X`, con il
  tooltip che li spiega. Accesa, la lista si accorcia di una riga; l'anteprima no.
- **Avvisi** (`notice`): una riga a sinistra nella barra di stato.
  - informazione in `C_ACCENT` per 2,5 s, errore in `C_ERR` per 4 s;
  - ogni azione che non ha un effetto visibile immediato dà un avviso ("Percorso copiato", "Aggiunta ai preferiti");
  - **annulla e ripeti** danno sempre un avviso, perché possono agire in un'altra cartella: cosa
    e dove ("Annullato lo spostamento: 3 elementi di nuovo in Download", "Annullata la copia di
    a.txt: nel Cestino", "Ripetuta la rinomina: di nuovo foto.jpg"). Se qualcosa non si è potuto
    fare, è un errore che dice cosa e perché, con il primo motivo e "(e altri N)":
    "Annullato lo spostamento solo in parte (2 su 3): a.txt non esiste più", "Impossibile
    annullare la copia: e.txt è stato modificato dopo". Se il motivo può passare (file in uso,
    Annulla nella finestra della shell, permessi) gli elementi restano da annullare e l'avviso
    finisce con "; si può riprovare";
  - un avviso che spiega un cambio di cartella ("La cartella è stata eliminata o spostata") resta
    anche dopo l'arrivo della cartella nuova; gli altri spariscono quando si cambia cartella.
- **Menu di elenco** (sottocartelle, livelli nascosti): stesso pannello del menu contestuale, con
  icona per voce; gli elementi nascosti in `C_FAINT`; l'elemento corrente già evidenziato. Se le
  voci non stanno nella finestra il menu scorre voce per voce (rotellina, frecce, PagSu/PagGiù),
  con una barra di scorrimento `EDGE` a destra.
- **Menu contestuale:**
  - pannello `C_MENU`; voci alte una riga, con l'etichetta a sinistra e la scorciatoia a destra in `C_DIM`;
  - separatori `C_LINE`; voce attiva `C_SEL`;
  - le voci che non si applicano a quel tipo di elemento non compaiono (Taglia su un'unità); quelle
    che si applicano ma ora non sono possibili restano, disattivate (Incolla senza file negli appunti);
  - si apre in un frame. Contiene solo le azioni essenziali; il menu completo della shell sta
    dietro "Altre opzioni…", sempre ultima voce dove esiste. Con l'opzione "Menu completo di
    Windows col tasto destro" (`menu_completo`) i ruoli si scambiano nella lista: il tasto destro
    apre il menu della shell, Shift+tasto destro il nostro; schede, barra laterale e percorso
    tengono sempre il nostro;
  - il menu completo della shell compare quando Windows l'ha costruito (200–450 ms): lo fa un
    thread suo e intanto l'interfaccia continua. Un clic o un tasto in quell'attesa lo annulla:
    l'utente è passato ad altro e un menu che compare dopo sarebbe una sorpresa;
  - Shift+Canc, Proprietà (di una o più voci, della cartella aperta, delle voci della barra
    laterale) e "Apri con…" passano dallo stesso thread: la finestra di Windows (conferma,
    Proprietà, Apri con) compare quando la shell è pronta (20.000 file: circa 3 s per la
    conferma; una condivisione lenta: quanto la rete) e l'interfaccia intanto continua. Un clic
    nell'attesa non la annulla: l'ha chiesta l'utente, ed è una finestra che aspetta. Se Windows
    non trova l'elemento la finestra non compare, e un avviso di errore dice cosa e perché
    ("Impossibile aprire le Proprietà di Condivisione (G:): percorso di rete non trovato (codice 53)");
  - **dialoghi Apri e Salva degli altri programmi** (`filedlg.cpp`, opzione `dialoghi`): non
    sono nostri e non si disegnano, ma entrano nella cartella aperta nell'ultima finestra di [z]f
    usata, appena si aprono e quando ci si torna dopo aver cambiato cartella in [z]f. Nessun avviso
    (è un'azione che si vede nel dialogo stesso); la scelta di una cartella non si tocca;
  - in "Questo PC": "Connetti unità di rete…" e "Aggiungi percorso di rete…" nel menu dello
    sfondo, "Disconnetti" su un'unità di rete. Il dialogo per connettere è quello di Windows (con
    la nostra finestra come proprietaria); dopo la connessione si apre l'unità, come in Explorer.
    Anche la procedura per aggiungere un percorso di rete è quella di Windows, in una finestra a
    sé come in Explorer; quando la si chiude la barra laterale si rilegge e il percorso nuovo
    compare sotto Questo PC. I nomi delle due voci sono quelli di Windows;
  - **annulla e ripeti:** la voce dice cosa annulla ("Annulla spostamento", "Annulla nuova
    cartella"); senza niente da annullare è "Annulla", disattivata. Nel menu dello sfondo stanno
    subito dopo Incolla, come in Explorer: "Annulla …" sempre, "Ripeti …" solo quando c'è.
- **Tooltip:**
  - pannello `C_MENU`: titolo in `C_TEXT`, se serve su più righe; poi coppie etichetta (`C_DIM`) / valore (`C_TEXT`); le note senza etichetta in `C_FAINT`;
  - larghezza massima `TIP_MAX_W` (520).
- **Casella di testo:** fondo `C_INPUT`, selezione `C_SEL_TEXT`, margine interno `FIELD_PAD` (6).
  Una sola implementazione (`TextField`) per rinomina, ricerca delle opzioni, "Vai a" e
  ogni campo futuro. Il suggerimento a casella vuota è in `C_FAINT`, `GAP_S` dopo il cursore.
- **Barra di scorrimento:** solo il cursore, `C_SCROLL`, largo `SCROLL_W` (10) con margine; alto
  almeno `THUMB_MIN` (24).
- **Pannello delle opzioni** (⚙ · Ctrl+,): pannello sovrapposto
  centrato, con il resto della finestra attenuato da `C_SHADOW`.
  - barra del titolo con ×; sezioni col titolo in `C_FAINT`; righe con etichetta a sinistra e
    controllo a destra, eventuale spiegazione sotto in `C_FAINT`;
  - sezioni: Aspetto, Integrazione con Windows ("Usa [z]f al posto di Explorer", "Una finestra per
    ogni cartella aperta da fuori", menu completo, dialoghi), File ("Annulla dopo il riavvio"), Avvio e
    aggiornamenti ("Riapri le schede all'avvio", "Aggiornamenti automatici" con l'ora
    dell'ultimo controllo nella spiegazione, "Ricevi anche le beta"), Scorciatoie;
  - in fondo la versione in `C_FAINT` (collegamento alla sua Release), seguita dall'icona di stato
    dell'aggiornamento (cliccabile, vedi "Versione e aggiornamento");
  - si chiude con Esc (prima svuota la ricerca), con la × o cliccando fuori.
- **Interruttore:** pillola 32×16 come in Windows 11: spento è un contorno `C_DIM` con il pallino
  `C_DIM`; acceso è pieno `C_ACCENT` con il pallino scuro. Il pallino scorre (τ come l'indicatore
  della scheda). Tutta la riga è cliccabile.
- **Selettore:** "−  13 px  +" o "‹  Segoe UI  ›": pulsanti quadrati alti una riga, testo
  `C_DIM`, hover come le voci; il valore al centro in `C_TEXT`.
- **Trascinamento dei file** (`drag.cpp`):
  - la destinazione sotto il mouse prende l'hover del suo elemento (`C_HOVER`, in dissolvenza) e un
    bordo 1 px `C_SEL_EDGE` arrotondato `R_S`; lo spazio vuoto della lista (la cartella aperta) ha
    solo il bordo, attorno a tutta la lista;
  - tra i preferiti, una **linea d'inserimento**: pillola alta `EDGE` in `C_SEL_EDGE`, larga come le
    voci; si inserisce nella fascia `DRAG_GAP` (un quarto) in alto o in basso di una voce, o sul
    titolo "Preferiti" (in fondo); al centro della voce si rilascia nella cartella;
  - l'azione si scrive accanto al cursore con le parole di Windows ("Sposta in %1", "Copia in %1",
    "Crea collegamento in %1", "Aggiungi ai preferiti"), sotto l'immagine trascinata di Windows. Se
    la sorgente non la mostra, la scriviamo noi in un pannello come il tooltip;
  - l'immagine trascinata da [z]f è quella della shell (le miniature delle voci); da 500 voci in su,
    dove la shell impiegherebbe mezzo secondo a prepararla, è l'icona grande del primo elemento;
  - dove non si può rilasciare: niente evidenziazione, cursore di divieto;
  - fermi `DRAG_SPRING` (500 ms) su una scheda, la scheda si apre; vicino ai bordi la lista scorre
    (`DRAG_SCROLL` righe a passo);
  - col tasto destro, al rilascio, il menu Copia qui / Sposta qui / Crea collegamento qui / Annulla,
    con la voce che farebbe un trascinamento normale già evidenziata.
- **Pannello F12:** diagnostica, stile dei pannelli sovrapposti, largo `STATS_W` (440); compare
  come il menu.
- **"Vai a"** (Ctrl+P · Ctrl+G · clic sul percorso vuoto, `goto.cpp`): pannello sovrapposto (`drawPanel`,
  `R_M`, compare come il menu) centrato in orizzontale, appena sotto la riga del percorso, largo
  al massimo `GOTO_W` (600). Il resto della finestra non si attenua: è un'azione rapida, come un menu.
  - in alto una casella di testo (`TextField`, `drawField`) a `GAP_M` dai bordi; vuota mostra
    "Vai a una cartella…" in `C_FAINT`;
  - sotto, le voci come quelle di un menu di elenco (`drawPanelItem`): alte una riga, icona della
    cartella o dell'unità, voce attiva `C_SEL`; al massimo `GOTO_ROWS` (12) visibili, poi si scorre
    con la barra `EDGE` (`drawPanelScroll`). Il pannello è alto quanto le voci;
  - ogni voce: il nome in `C_TEXT`, poi a `GAP_M` la cartella che lo contiene in `C_DIM` (il
    percorso intero se il nome mostrato non è quello della cartella, come "Documenti"). Il nome
    prende al massimo `GOTO_NAME_MAX` (60%) dello spazio. Nascoste in `C_FAINT`;
  - **lettere trovate in `C_ACCENT`** (il colore del filtro), nel nome o nel percorso
    (`drawTextMarked`). È l'unico posto dove una parte di un testo cambia colore;
  - senza voci una riga in `C_FAINT`: "Nessuna cartella", "caricamento…" (dopo `LOADING_AFTER`),
    "Impossibile leggere la cartella";
  - campo vuoto: le cartelle recenti, dalla più usata e recente;
  - **dal percorso** (clic sullo spazio vuoto della barra del percorso, Ctrl+L, F4): si apre con
    il percorso della cartella aperta e "\" in fondo, tutto selezionato, quindi la cartella stessa
    e le sue sottocartelle. Scrivendo si sostituisce (ricerca su tutto); Fine o un clic per
    continuare il percorso. Con il pannello già aperto queste scorciatoie rimettono il percorso;
    in Questo PC il campo resta vuoto. È l'unico modo di scrivere un percorso: niente casella
    nella barra;
  - tastiera: ↑ ↓ PagSu PagGiù muovono la voce attiva, il resto va alla casella; Invio apre,
    Ctrl+Invio apre in una nuova scheda in background, Tab completa con la voce attiva e ne mostra
    le sottocartelle, Esc prima svuota il testo e poi chiude;
  - mouse: la voce sotto il mouse diventa attiva; clic apre, clic centrale o Ctrl+clic in una
    nuova scheda; clic fuori chiude e basta, come i menu; tooltip con il percorso completo.
- **Icona dell'applicazione** (`src\z-f.ico`, risorsa di `src\z-f.rc`): "[z]" in `C_ACCENT` su un
  quadrato arrotondato `C_BG`, gli stessi due colori dell'interfaccia. Forma geometrica, senza font:
  parentesi quadre con asta e grazie dello stesso spessore, "z" minuscola centrata fra le due.
  - misure: quelle di ogni DPI comune, piccola e grande (100% 16/32, 125% 20/40, 150% 24/48, 175%
    28/56, 200% 32/64, 225% 36/72), più 256, tutte in PNG: una misura che manca Windows la
    ricaverebbe riducendone un'altra, con i bordi sfumati;
  - 16 e 20 hanno un disegno a pixel fatto a mano, tratti di 2 px tranne le grazie delle
    parentesi, di 1 px: ridotto da quello
    grande, "[z]" diventerebbe una macchia. Dalla 24 in su la stessa geometria con i bordi agganciati ai
    pixel, angolo `0,22 ×` la misura; dalla 32 un margine trasparente di 1/32 della misura;
  - la finestra la prende alla misura del suo schermo (`appIconSet`, `chrome.cpp`), dopo il
    primo frame e a ogni cambio di DPI;
  - si rigenera con `python strumenti\icona.py` (serve Pillow; `--anteprima <cartella>` scrive
    le misure ingrandite in un PNG). I due colori lo script li legge da `theme.h`: se cambiano
    lì, basta rigenerarla.

---

## 8. Interazione

### Mouse
| Gesto | Significato, ovunque |
|---|---|
| clic | seleziona o attiva |
| doppio clic | apre |
| clic sulla destinazione di un collegamento | la cartella che la contiene, con la destinazione selezionata |
| clic centrale | apre in una nuova scheda in background; su una scheda la chiude |
| Ctrl+clic | nella lista alterna la selezione; su un preferito apre in nuova scheda |
| tasto destro | il nostro menu contestuale |
| Shift+tasto destro | il menu completo della shell (con "Menu completo di Windows": il nostro) |
| trascina da icona o nome di una voce selezionata | sposta sulla stessa unità, altrimenti copia; Ctrl copia, Shift sposta, Alt collega |
| trascina da un altro punto della lista (anche dal nome di una voce non selezionata) | rettangolo di selezione; Ctrl alterna, Shift aggiunge |
| trascina un bordo nell'intestazione | larghezza della colonna; doppio clic: adatta al contenuto |
| trascina il bordo della barra laterale o dell'anteprima | larghezza del pannello; doppio clic: predefinita |
| trascina un preferito | lo sposta tra gli altri preferiti (solo l'ordine); Esc annulla |
| cestino a destra di un preferito | primo clic: "Togli?"; secondo clic: lo toglie dai preferiti |
| trascina col tasto destro | al rilascio, un menu chiede cosa fare |
| rotellina | scorre l'elemento sotto il mouse, non quello attivo; le frazioni di scatto dei touchpad si sommano (negli elenchi a righe intere) |
| Ctrl+rotellina | zoom della lista: Dettagli, Elenco, Icone da 20 a 100% (solo la lista; il testo con Ctrl +/−) |
| tasti laterali; tasti Indietro/Avanti della tastiera; gesti del touchpad "Spostamento indietro/avanti" di Windows | indietro / avanti (sopra le opzioni e "Vai a": niente) |

### Tastiera
- **Ogni comando ha una scorciatoia**, mostrata nel menu e nel tooltip ed elencata in `kShortcuts`
  (`options.cpp`): la tabella che il pannello delle opzioni mostra e cerca. Una scorciatoia nuova
  si aggiunge lì; se esegue una voce di menu, la si collega alla voce e il menu la mostra da solo.
  Un tasto che esegue solo un comando si scrive nella tabella col suo codice e i modificatori
  (`K(M_..., KC | KS, 'N')`): la tabella lo esegue (con lo stesso codice della voce di menu) e ne
  scrive il nome ("Ctrl+Shift+N"). Vale solo con esattamente quei modificatori (Ctrl+Shift+W non
  chiude la scheda). In `onKey` restano scritti a mano solo i tasti che dipendono dal contesto.
- **Si seguono le convenzioni di Windows e dei browser:** Ctrl+C/X/V, F2, Canc, Alt+frecce,
  Ctrl+T/W/Tab, Ctrl+L, F5, Alt+P (il riquadro di anteprima di Explorer), Ctrl+Shift+2/3/4/5/6 (le
  viste di Explorer: grandi, medie, piccole, elenco, dettagli), Ctrl+Z e Ctrl+Y
  (anche Ctrl+Shift+Z) per annullare e ripetere le operazioni sui file. Dentro una casella di
  testo Ctrl+Z non tocca i file.
- **Esc toglie un livello alla volta:** rettangolo, bordo o preferito trascinato (si torna a prima), conferma del cestino → menu →
  modifica in corso → filtro → selezione. Nei pannelli
  con una ricerca (opzioni, "Vai a"): prima svuota la ricerca, poi chiude.
- **Digitare filtra subito:** nessun campo da attivare prima.

### Tooltip
- Compaiono dopo `TIP_DELAY` (500 ms) fermi su un elemento; passando a un elemento vicino entro
  `TIP_NEAR_WINDOW`, dopo `TIP_DELAY_NEAR` (60 ms).
- Spariscono a qualsiasi clic, tasto o rotellina.
- Le informazioni lente (dal disco) si leggono su un thread; il tooltip le aspetta al massimo
  `TIP_INFO_WAIT` (150 ms), così compare già completo invece di allargarsi sotto gli occhi.
- Ogni elemento interattivo ha un tooltip che dice cosa fa e la sua scorciatoia. La scorciatoia
  viene da `kShortcuts`, come nei menu: `shortcutsOf` dà tutte le alternative di un comando
  ("Alt+← · tasto Indietro del mouse o della tastiera"). Un comando senza voce di menu ha comunque un id (`M_BACK`,
  `M_SIDEBAR`…) per legarlo ai suoi tasti; nel tooltip va in una riga con l'etichetta
  ("Scorciatoia", "Modifica", "Togli il filtro").

---

## 9. Testi

- **Italiano**, frasi brevi, solo la prima lettera maiuscola ("Copia percorso", non "Copia Percorso").
- **Voci di menu:** il verbo all'imperativo o all'infinito, senza articoli: "Apri", "Nuova
  cartella", "Apri in nuova scheda". I puntini "…" solo se la voce apre altra interfaccia
  ("Apri con…", "Altre opzioni…").
- **Avvisi:** participio, cosa è successo: "Percorso copiato", "Tolta dai preferiti".
- **Errori:** cosa non va e su cosa, senza scuse né codici in evidenza. Il codice va tra
  parentesi alla fine, se c'è.
- **Scorciatoie:** `Ctrl+Shift+C` (tasti col "+", nessuno spazio); più alternative separate da
  " · ".
- **Separatore tra valori** su una riga (tooltip, piede dell'anteprima, versione): lo stesso " · ",
  con uno spazio per parte ("25/11/2026 14:18 · 3 min fa", "UTF-8 · 120 righe · 4 KB"). La barra
  di stato ha la sua spaziatura, perché lì separa zone e non valori.
- **Numeri:**
  - virgola decimale e punto per le migliaia (`1,5 MB`, `212.388 byte`);
  - unità con spazio (`207 KB`, `16 px`); percentuali senza spazio (`70%`).
- **Date:** `gg/mm/aaaa hh:mm`; nei tooltip anche il tempo trascorso ("16 giorni fa").
- **Nomi di Windows** come li mostra Windows nella lingua dell'utente: "Documenti", "Disco
  locale", "Questo PC".

---

## 10. Codice

- **Valori visivi solo da `theme.h`**, letti con `px()`. Un numero visivo scritto direttamente nel
  codice nuovo è un errore. I valori sparsi rimasti nel codice più vecchio si migrano quando si
  tocca quel codice.
- **Componenti riusati:** `rect`, `drawText` (`drawTextMarked` per evidenziare lettere), `TextField`,
  `drawPanel`, il menu (`menuAdd`/`menuPlace`), le voci e la barra di scorrimento degli elenchi nei
  pannelli (`drawPanelItem`, `drawPanelScroll`), il tooltip (`tipRow`), `notice`. Niente seconde
  implementazioni dello stesso componente.
- **Animazioni** solo con `approach`, `hoverFadeUpdate`/`hoverAlpha`, `easeOut` e `fadeA`.
  Ogni animazione imposta `A.animActive` finché si muove: è l'unico modo per avere un altro frame.
- **Mai aspettare bloccando il thread dell'interfaccia:** né il disco né il vsync. Il ciclo principale
  aspetta il vsync insieme ai messaggi (`MsgWaitForMultipleObjectsEx`). Eccezioni volute e misurate:
  `dwmFlush` nel passaggio WARP → GPU (una volta), l'attesa della GPU con F12 aperto (per misurarla),
  e la scrittura dei file in coda (`saveFlush`) quando la finestra non c'è più: alla fine del
  processo, in `WM_ENDSESSION` e nel `WM_DESTROY` della principale, prima di liberare il mutex
  dell'istanza unica (0,2–0,8 ms, "chiusura: file salvati" nel registro).
- **GPU persa** (driver aggiornato o reimpostato): non si disegna finché un thread non crea un
  device nuovo (GPU, altrimenti WARP); poi glifi e icone si rifanno al primo uso, anteprima e
  miniature tornano dalle copie in RAM (`hybrid.cpp`). Nel log: "GPU persa", "GPU ripristinata".
- **Lavoro lento su un thread,** con il risultato consegnato alla finestra via `PostMessage` e un
  contatore di generazione per scartare i risultati superati. Vale anche per le cose piccole che
  toccano il disco o un altro processo: creare una cartella, avviare un processo, aprire o chiudere
  un handle su una cartella (il watcher), scrivere il log. Un lock preso dal thread UI non si tiene
  mai durante una scrittura su disco.
- **Soglie del registro dei rallentamenti** (`LOG_*`) e tempi del lavoro in background
  (`WATCH_BATCH`, `SIDE_RELOAD`, `ZIP_TEMP_KEEP`, i tempi della dimensione delle cartelle
  `DS_*`, le attese del salto dei dialoghi `FD_*`, i
  tentativi sugli appunti aperti da un altro programma `CLIP_*`) stanno
  in `theme.h` come gli altri tempi.
  `LOG_ZIP` (100 ms) segna la lettura di una directory centrale ("zip: elenco lento") e
  un'estrazione ("zip: estrazione", col codice in MB) sui loro thread.
- **Percorsi dentro uno zip** (`zip.cpp`): su un thread li riconosce `zipSplit`/`zipInside` (un
  `GetFileAttributes` solo sui prefissi che finiscono in ".zip"); sul thread UI la cartella aperta
  (`Listing::zipLen`), o `zipPathLike` a vista dove sbagliare costa solo un'icona generica. Una
  funzione nuova che legge un file dal suo percorso deve passare da lì (lettura in memoria con
  `zipReadMem`), e non chiedere niente alla shell su questi percorsi: la shell userebbe zipfldr.
- **Ciò che la shell fa sul thread UI si misura a parte** (`hitchLog` con "oggetto della shell"): i
  messaggi che entrano nei cicli modali della shell sono esclusi dal conteggio dei messaggi lenti
  (`g_shellCalls`), quindi senza questa misura il tempo non si vedrebbe. Il menu completo della
  shell e i comandi della shell senza menu (Shift+Canc, Proprietà, Apri con: `shellVerb`, o
  `shellVerbPath` per un percorso intero) non stanno sul thread UI: li fa il thread del menu
  (`smRun`), che segna "menu della shell lento" e "comando della shell lento" oltre `LOG_MENU`; F12
  mostra i due tempi. Niente `SHObjectProperties` né `SHOpenWithDialog` sul thread UI.
- **File di [z]f** in `%APPDATA%\z-f`, scritti con `saveDataAsync`: file temporaneo, poi
  rinominato.
- **Ogni modulo nuovo** entra in `z-f.cpp` (unity build), nella tabella dei moduli di
  `ARCHITETTURA.md` e qui, se ha una parte visibile.
- **Per ogni funzione nuova:** misurare i tempi, controllare `z-f-log.txt`, provare sul PC vero,
  aggiornare `ARCHITETTURA.md`, `NOVITA.md`, DESIGN e, se cambia l'uso, il README.

---

## 11. Elementi previsti

Nessuno al momento. Un elemento previsto si descrive qui prima di costruirlo, poi passa nella
sezione 7.
