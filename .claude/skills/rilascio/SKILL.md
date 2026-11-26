---
name: rilascio
description: Pubblica una versione di [z]f passo per passo, con le verifiche: dev pulito e controllato, ZF_VERSION, README e NOVITA.md, build MSVC senza avvisi, copia firmata in build\, commit "Versione …", pull request dev → main con merge commit, tag annotato e Release su GitHub con l'exe e la firma. Si ferma e chiede conferma prima di ogni passo che va su GitHub. Argomento facoltativo: la versione (altrimenti la propone secondo il Semantic Versioning).
disable-model-invocation: true
---

# /rilascio — pubblicare una versione di [z]f

Parte solo quando Zyruel scrive `/rilascio` (con la versione, facoltativa: `/rilascio 1.0.0-beta.4`).
Ogni passo ha la sua **verifica**: se non torna, ci si ferma e lo si dice a Zyruel, senza
aggirare il problema.

Regole che valgono per tutta la procedura:

- **Prima di ogni passo che va su GitHub** (push, pull request, unione, tag, Release) ci si ferma
  e si chiede conferma a Zyruel, dicendo cosa sta per uscire. Una conferma vale per quel passo.
- **Mai `--no-verify`**, mai push diretto su `main` (lo bloccano GitHub e `.githooks/pre-push`).
  Se il pre-push ferma un commit, si corregge il commit.
- Si committano **solo i file del rilascio, per nome** (mai `git add -a` o `.`); niente stash,
  reset o cambi di branch: nella cartella possono lavorare altre sessioni.
- Repository pubblico: nei file, nei messaggi di commit, nella pull request e nelle note della
  Release niente dati personali (CLAUDE.md, "Repository pubblico").
- I comandi da Bash si danno dalla cartella del progetto, con percorsi relativi (`build/…`):
  PowerShell non gestisce le parentesi quadre di `[z]f` nei percorsi.

Nei comandi qui sotto `$V` è la versione (`1.0.0-beta.4`), senza la `v`.

## 1. La versione

- Versione attuale: `grep -n ZF_VERSION src/base.h`; ultime Release: `git tag --sort=-v:refname | head -3`
  e `gh release list --limit 3`.
- Con l'argomento, lo si controlla; senza, se ne propone una. Semantic Versioning (CLAUDE.md,
  "Versione"): `1.0.0-beta.N` le beta, `1.0.0-rc.N` le candidate, `1.0.0` la stabile; poi
  `1.0.1` per le correzioni, `1.1.0` per le funzioni nuove, `2.0.0` per i cambiamenti grossi.
  Durante le beta la proposta è la beta successiva; il passaggio a rc o alla stabile lo decide
  Zyruel.
- **Verifica:** la versione è più alta dell'ultima pubblicata e il tag `v$V` non esiste
  (`git ls-remote --tags origin "v$V"` vuoto). Si chiede a Zyruel di confermare la versione
  prima di cambiare qualsiasi file.

## 2. Verifiche prima di cominciare

```
git branch --show-current                         # dev
git fetch origin
git status --porcelain                            # vuoto
git rev-list --count dev..origin/dev              # 0: dev non è indietro
git merge-base --is-ancestor origin/main dev && echo ok   # main è già dentro dev
git config core.hooksPath                         # .githooks
gh auth status
```

- **`dev` pulito:** se ci sono modifiche non committate (anche di un'altra sessione), ci si ferma
  e si chiede a Zyruel cosa farne: non si committano né si mettono da parte file altrui.
- `dev` può essere **avanti** a `origin/dev` (commit non ancora pushati): vanno nel rilascio,
  quindi si elencano a Zyruel (`git log --oneline origin/dev..dev`).
- `NOVITA.md` ha la sezione "Prossima versione" e non è vuota; dice tutto ciò che è cambiato da
  `v<precedente>` (`git log --oneline v<precedente>..dev`).
- **`/controllo cambiati`** (skill `controllo`) sui file diversi da `origin/main`: passato, oppure
  i punti segnalati sono stati accettati da Zyruel (o corretti, con un commit a parte). Prima di un
  rilascio Zyruel può chiedere `/controllo tutto`.
- Le prove a mano ancora aperte in `prove\da-verificare.md` per questa versione si ricordano a
  Zyruel: decide lui se pubblicare lo stesso.

## 3. Versione nei documenti

- `src/base.h`: `#define ZF_VERSION L"$V"`.
- `README.md`: la riga `**Versione attuale: $V.**`.
- `NOVITA.md`: `## Prossima versione (in preparazione)` diventa `## $V (<mese> <anno>)`, con il
  mese in italiano e minuscolo (`## 1.0.0-beta.3 (novembre 2026)`). Non si aggiunge una
  "Prossima versione" vuota: la rimette la prima funzione nuova.
- Se `prove\da-verificare.md` parla della versione come "in preparazione", si aggiorna.
- **Verifica:** `git diff` mostra solo queste righe; `grep -rn "in preparazione" CLAUDE.md
  NOVITA.md README.md` non trova più la versione che si sta pubblicando.

## 4. Build di rilascio (MSVC)

Da Bash, nella cartella del progetto (da PowerShell no: non entra in `[z]f`):

```
cmd //c '.\build.bat'
```

- **Verifica: zero avvisi.** Nell'uscita non c'è nessun `warning` (attenzione a C4244, int →
  float, che zig non segnala) e l'ultima riga è `Fatto: build\z-f.exe  [release]`.
  Un avviso si corregge (commit a parte) e si ricompila.
- Se il link fallisce perché `build\z-f.exe` è in uso (LNK1104), sono le finestre di [z]f di
  Zyruel: ci si ferma e gli si chiede di chiuderle. Non si chiudono al posto suo.
- **Verifica:** l'exe contiene la versione nuova:
  ```
  python -c "import sys; print(sys.argv[2].encode('utf-16-le') in open(sys.argv[1],'rb').read())" build/z-f.exe "$V"
  ```
  deve stampare `True`.

## 5. Copia in `build\z-f-$V.exe`

Con Bash (non PowerShell: le parentesi quadre del percorso):

```
cp build/z-f.exe "build/z-f-$V.exe"
sha1sum build/z-f.exe "build/z-f-$V.exe"
```

- **Verifica:** i due sha1 coincidono. Se `build/z-f-$V.exe` esisteva già (un rilascio ripreso),
  prima di sovrascriverlo si guarda cos'è e lo si dice a Zyruel.

## 6. Firma

```
build/firma.exe "build/z-f-$V.exe"
ls -l --time-style=full-iso "build/z-f-$V.exe" "build/z-f-$V.exe.sig"
```

Se `build/firma.exe` non c'è, si compila prima: `cmd //c '.\build.bat firma'`.
`firma` scrive `build/z-f-$V.exe.sig` (64 byte) e la verifica subito.

- **Verifica:** il comando esce con 0; il `.sig` esiste, è di **64 byte** ed è **più nuovo**
  dell'exe (`[ "build/z-f-$V.exe.sig" -nt "build/z-f-$V.exe" ] && echo nuovo`).
- **La chiave privata è solo sul PC di Zyruel**, cifrata in `%LOCALAPPDATA%\z-f-firma\chiave.bin`
  (`strumenti\firma.cpp`). I programmi avviati dall'app desktop di Claude vedono una
  `%LOCALAPPDATA%` virtualizzata (nella cartella del pacchetto dell'app): la chiave usata per i
  rilasci è lì. Se `firma` dice "Nessuna chiave" (sessione cloud, altro PC, cartella diversa) **ci si
  ferma** e lo si dice a Zyruel. **Mai `firma nuova`:** con una chiave diversa da quella in
  `src\update.cpp` (`kUpdKey`) gli exe già distribuiti rifiutano l'aggiornamento.

## 7. Commit "Versione $V" su `dev`

```
git add src/base.h README.md NOVITA.md        # più prove/da-verificare.md se cambiato al passo 3
git commit                                    # titolo "Versione $V"
git show --stat HEAD
```

- Messaggio: titolo `Versione $V`, poi due o tre righe su cosa porta la versione, poi la riga
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` (o quella indicata dall'attribuzione
  della sessione).
- **Verifica:** `git show --stat HEAD` contiene solo i file del rilascio; `git status --porcelain`
  è di nuovo vuoto.

## 8. Push di `dev` — **chiedere conferma**

Si mostra a Zyruel `git log --oneline origin/dev..dev` e si chiede conferma. Poi:

```
git push origin dev
```

- Il pre-push controlla i dati (email, percorsi con un nome utente, `.exe`/`.sig`/chiavi, parole
  private). **Verifica:** il push riesce e `git rev-parse dev origin/dev` dà lo stesso sha. Se il
  hook blocca, si corregge il commit indicato e si riprova (mai `--no-verify`).

## 9. Pull request `dev` → `main` — **chiedere conferma**

```
gh pr create --base main --head dev --title "Rilascio $V" --body-file <file>
```

- Corpo: i punti della sezione di `NOVITA.md` in breve, una riga su come è stata provata
  (compilata con MSVC senza avvisi, prove fatte), poi la riga
  `🤖 Generated with [Claude Code](https://claude.com/claude-code)`. Il file del corpo si scrive
  nella cartella temporanea della sessione o in `build\` (escluso da git), mai nel repository.
- **Verifica:** `gh pr view --json number,state,baseRefName,headRefName,mergeable` dice `OPEN`,
  `main` ← `dev`, unibile; i controlli, se ci sono, sono verdi (`gh pr checks`).

## 10. Unione con merge commit — **chiedere conferma**

```
gh pr merge <numero> --merge
```

- **Solo `--merge`**: mai `--squash` né `--rebase` (`dev` deve restare allineato), mai `--admin`.
- Poi `dev` si riallinea a `main` e si pusha (stessa conferma, detta prima):
  ```
  git fetch origin
  git merge --ff-only origin/main
  git push origin dev
  ```
- **Verifica:** `gh pr view <numero> --json state,mergeCommit` dice `MERGED`;
  `git rev-parse dev origin/dev origin/main` danno tre sha uguali; `git log -1 --format=%P
  origin/main` ha **due** genitori (è un merge commit).

## 11. Tag annotato `v$V` — **chiedere conferma** prima del push

```
git tag -a "v$V" origin/main -m "[z]f $V"
git rev-parse "v$V^{commit}" origin/main        # stesso sha
git push origin "v$V"
```

- **Verifica:** `git cat-file -t "v$V"` dice `tag` (annotato, non leggero) e
  `git ls-remote --tags origin "v$V"` lo trova.

## 12. Release su GitHub — **chiedere conferma**, mostrando le note

Note (in un file fuori dal repository): la sezione di `NOVITA.md` della versione, per chi la
scarica; se cambia qualcosa per l'installazione lo si dice (la 1.0.0-beta.3 spiegava di
rinominare l'exe in `z-f.exe` e che il `.sig` non serve scaricarlo). Si mostrano a Zyruel prima
di pubblicare.

```
gh release create "v$V" "build/z-f-$V.exe" "build/z-f-$V.exe.sig" \
  --verify-tag --title "[z]f $V" --notes-file <file> --prerelease
```

- `--prerelease` per beta e rc (`-beta.`, `-rc.` nella versione); per una stabile no.
- **I nomi dei file allegati sono quelli esatti**: `z-f-$V.exe` e `z-f-$V.exe.sig`. `update.cpp`
  cerca proprio `z-f-<versione>.exe` e `z-f-<versione>.exe.sig` tra i file della Release più
  nuova del canale (ignorando le bozze): con nomi diversi gli exe distribuiti non si aggiornano.
- Mai `--draft`: una bozza non la vede nessun aggiornamento.

## 13. Verifica finale

```
gh release view "v$V" --json tagName,name,isDraft,isPrerelease,assets \
  --jq '{tagName,name,isDraft,isPrerelease,assets:[.assets[]|{name,size}]}'
stat -c '%n %s' "build/z-f-$V.exe" "build/z-f-$V.exe.sig"
gh api "repos/Zyruel/-z-f/releases/tags/v$V" --jq '.assets[]|{name,size,digest}'
sha256sum "build/z-f-$V.exe"
```

- Due allegati con i nomi esatti; dimensioni uguali a quelle locali (il `.sig` 64 byte);
  `isDraft` falso; `isPrerelease` vero per beta e rc.
- Il `digest` dell'exe (`sha256:…`) coincide con `sha256sum`: [z]f lo controlla prima di
  scaricare, e senza `digest` rifiuta l'aggiornamento.
- Resoconto a Zyruel: versione, link della pull request e della Release, sha di `main`, le
  dimensioni, e le prove a mano che restano in `prove\da-verificare.md`. Gli exe già distribuiti
  sul canale giusto si aggiornano da soli entro un giorno.
