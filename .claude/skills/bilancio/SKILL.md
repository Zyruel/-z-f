---
name: bilancio
description: Bilancio del ciclo di lavoro fra due rilasci di [z]f. Uno script raccoglie i dati dalle trascrizioni delle sessioni e da git (sessioni, costi, errori ripetuti, comandi, skill, compilazioni, commit). Poi si valutano processo, decisioni, skill, skill da creare, passi da affidare a uno script, permessi e hook. Il resoconto, privato, va in build\bilanci\ con proposte numerate, e si applicano solo quelle che Zyruel sceglie. Argomento facoltativo: la versione (altrimenti l'ultimo tag).
disable-model-invocation: true
---

# /bilancio: come si è lavorato da un rilascio all'altro

Parte solo quando Zyruel scrive `/bilancio` (con la versione, facoltativa: `/bilancio 1.0.0-beta.6`),
di solito dopo `/rilascio` e in una sessione nuova, con il contesto libero.

Il bilancio **guarda i processi, non il codice di [z]f**: come si è lavorato, cosa ha fatto
perdere tempo, cosa si ripete a mano. Si propone e si aspetta. Le correzioni toccano solo skill,
`.claude/hooks/regole.sh`, `.claude/settings.json`, gli script di `prove\` e `strumenti\`,
`CLAUDE.md` e i tre elenchi; il codice di [z]f resta per le sessioni di lavoro.

Regole che valgono per tutta la procedura:

- **Repository pubblico:** ciò che viene dalle trascrizioni (messaggi di Zyruel, percorsi, nomi,
  costi) resta in `build\bilanci\`, che è escluso da git. Nei file del repository che il bilancio
  modifica si scrivono solo regole e numeri senza dati personali (CLAUDE.md, "Repository
  pubblico"), come le regole di `/prova` nate dal bilancio della 1.0.0-beta.4.
- **Parole private:** lo script le maschera nel file dei dati; non si ricopiano mai, nemmeno nel
  resoconto in `build\`.
- **Prove, non impressioni:** ogni osservazione cita numeri, sessione (`id` di 8 caratteri) o
  commit. Le cose che sono andate bene si dicono in una riga, senza lodi: servono a non
  cambiarle.

## 1. Il ciclo

- Versione dall'argomento, altrimenti l'ultimo tag (`git tag --sort=-v:refname | head -2`); la
  precedente è il tag prima. Il ciclo va dalla data del tag precedente a quella del tag.
- Senza il tag della versione (rilascio non ancora fatto) il ciclo arriva ad adesso: lo si dice.

## 2. I dati

Da Bash, nella cartella del progetto:

```
python -I -X utf8 .claude/skills/bilancio/raccogli.py <versione> [<versione precedente>]
```

Scrive `build/bilanci/<versione>-dati.md`: riepilogo, sessioni (messaggi, strumenti, errori,
subagenti, token letti e in uscita, picco del contesto, giri oltre 400k, skill; il costo solo se
le trascrizioni lo registrano, cosa che dalla 1.0.0-beta.7 non fanno più), skill e subagenti, strumenti, errori per categoria, decisioni sui
permessi, comandi di shell più usati (con quante volte erano già permessi), sequenze ripetute,
compilazioni, commit e file toccati più volte, e i messaggi di Zyruel.

- **Verifica:** zero sessioni, o una finestra di pochi minuti con molti commit, vuol dire che le
  trascrizioni non sono su questo PC (sessioni cloud, altro PC). Ci si ferma e lo si dice: il
  bilancio si fa solo con git, dicendo che manca metà dei dati.
- Le categorie degli errori sono in `CATEGORIE`, in cima allo script. Se un errore ripetuto
  finisce in "altro:", si aggiunge la sua categoria allo script, in un commit del bilancio.

## 3. Cosa si legge

- Il file dei dati, per intero.
- La sezione della versione in `NOVITA.md`; `CLAUDE.md`; le skill (`.claude/skills/*/SKILL.md`)
  e `.claude/hooks/regole.sh`; `.claude/settings.json`.
- `docs/da-sistemare.md`, `docs/idee.md`, `prove/da-verificare.md`: cosa è stato aggiunto o
  tolto nel ciclo (`git diff v<prec>..v<V> -- <file>`).
- Il bilancio precedente (`build/bilanci/<prec>.md`), se c'è: le proposte scelte e scartate.
- **Le sessioni da approfondire** (quelle con più errori, più costo o più messaggi di
  correzione) si guardano prima dai messaggi di Zyruel nel file dei dati. Se non basta, al
  massimo **2–3 subagenti `Explore`** in sola lettura, uno per sessione, con la domanda precisa
  ("perché nella sessione `59f59f5f` la finestra non era davanti 8 volte?") e il percorso della
  trascrizione (`~/.claude/projects/<cartella del progetto>/<id completo>.jsonl`). Le
  trascrizioni sono grandi: si chiede di cercare con grep, non di leggerle per intero.

## 4. La valutazione, per aree

Per ogni area si risponde alle domande con le prove; un'area senza niente da dire si scrive con
"niente da segnalare".

- **Processo:**
  - In che ordine si è lavorato?
  - Quanti commit di revisione o correzione dopo il push ("Revisione", "Controllo",
    file toccati da molti commit)? Cosa avrebbe trovato prima quei problemi?
  - Le sessioni parallele si sono pestate i piedi (stessi file, controllo dello schermo occupato)?
  - Si sono saltati passi di CLAUDE.md ("Dopo:": misure, documenti, `/controllo`, zero avvisi)?
- **Decisioni:**
  - Quali sono state prese, cambiate o rimandate (git log e messaggi di Zyruel)?
  - Sono finite dove devono stare (CLAUDE.md, DESIGN, `docs/idee.md`, `docs/da-sistemare.md`)?
  - Zyruel ha dovuto ripetere o correggere qualcosa che una regola scritta avrebbe evitato?
- **Skill usate:**
  - Usate quando servivano? Ci sono casi in cui mancavano e sarebbero servite?
  - Ci sono istruzioni ignorate, sbagliate o superate dal codice? Errori ripetuti che la skill
    già vietava, quindi la regola non basta, va spostata o resa automatica?
- **Skill nuove:** un lavoro fatto a mano in più sessioni, sempre con gli stessi passi, che una
  skill renderebbe ripetibile. Si propone solo con almeno due casi nel ciclo (o uno qui e uno nel
  bilancio prima).
- **Script, permessi, hook:**
  - Sequenze di comandi o comandi in linea (`python (codice in linea)`) ripetuti → uno script in
    `prove\`, `strumenti\` o nella skill che li usa.
  - Comandi frequenti mai permessi (`nelle regole` 0) e senza rischi → una regola in
    `.claude/settings.json`.
  - Errori ripetuti che un controllo prima del comando eviterebbe → un blocco in `regole.sh`,
    con il messaggio che dice cosa fare.
- **Costi** (in token: i letti dalla cache sono la voce che pesa; per riferimento, la 1.0.0-beta.6
  ha letto 239 M token, subagenti compresi, per 84,91 $):
  - Quali sessioni sono costate di più e perché (contesto lungo, tentativi ripetuti, subagenti,
    screenshot)? Un picco oltre 400k o molti giri oltre quella misura dicono un contesto lungo.
  - Cosa le avrebbe accorciate?
- **Il bilancio precedente:**
  - Le proposte applicate hanno funzionato (gli errori che dovevano sparire sono spariti)?
  - Quelle scartate si ripropongono solo con prove nuove, dicendo cosa è cambiato.

## 5. Il resoconto

In `build/bilanci/<versione>.md`:

```
# Bilancio del ciclo <prec> → <versione>, <aaaa-mm-gg>

<N> sessioni, <M> messaggi di Zyruel, <X> errori, <C> $, <K> commit (di cui <R> di revisione).

## Cosa ha funzionato
- una riga ciascuna, con il numero che lo dice.

## Osservazioni
### Processo
- osservazione, con le prove (sessione, commit, numeri).
...

## Proposte
1. **Titolo breve** (priorità alta/media/bassa). Prove: … File: `…`, cosa cambiare in una o
   due frasi. Beneficio atteso: … (per esempio "−13 errori della finestra non davanti per ciclo").
2. ...

## Esito
(dopo la risposta di Zyruel) scelte: 1, 3; scartate: 2 (motivo).
```

Le proposte sono al massimo 8–10, ordinate per beneficio; le più piccole si raccolgono in una
sola. In chat: il riepilogo in numeri, le proposte (titolo, priorità, una riga) e il percorso del
file.

## 6. Le proposte scelte

- Si chiede a Zyruel quali applicare e si applicano **solo quelle**, nella stessa sessione.
- Una skill nuova si scrive come le altre (intestazione, passi con la verifica, in italiano) e si
  nomina in `CLAUDE.md`. Uno script nuovo si prova sul ciclo appena chiuso.
- Nel resoconto si compila "Esito", con il motivo delle proposte scartate: il bilancio dopo non
  le ripropone senza prove nuove.
- Commit solo quando Zyruel lo chiede, su `dev`, con i soli file del bilancio, per nome.
