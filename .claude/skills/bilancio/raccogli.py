"""Raccoglie i dati del ciclo di lavoro fra due rilasci di [z]f, per /bilancio.

Uso, da Bash nella cartella del progetto:
    python -I .claude/skills/bilancio/raccogli.py <versione> [<versione precedente>]

Legge in sola lettura le trascrizioni delle sessioni di Claude Code (~/.claude/projects) e la
storia di git, e scrive build/bilanci/<versione>-dati.md. Il file e' privato (build/ e' escluso
da git): contiene i messaggi di Zyruel e i dettagli delle sessioni. Le parole private di
.git/info/parole-private.txt vi compaiono mascherate.

Solo libreria standard: lo script vive in un repository pubblico e deve girare ovunque ci sia
Python, senza installare niente.
"""

import collections
import datetime as dt
import json
import pathlib
import re
import subprocess
import sys

MAX_MSG = 400        # caratteri tenuti di ogni messaggio di Zyruel
TOP = 25             # righe delle classifiche
SEQ_MIN, SEQ_MAX = 3, 5


def git(*args):
    r = subprocess.run(["git", *args], capture_output=True, text=True, encoding="utf-8")
    return r.stdout.strip() if r.returncode == 0 else ""


def when(s):
    return dt.datetime.fromisoformat(s.replace("Z", "+00:00"))


# ---------------------------------------------------------------------------- il ciclo

def ciclo(argv):
    """La finestra va dal tag precedente al tag della versione: le date dei tag annotati sono
    il momento del rilascio, mentre quelle dei commit dicono quando il lavoro era pronto."""
    if len(argv) < 2:
        sys.exit("uso: raccogli.py <versione> [<versione precedente>]")
    v = argv[1].lstrip("v")
    tags = git("tag", "--sort=-v:refname").split()
    if argv[2:]:
        prec = argv[2].lstrip("v")
    else:
        tv = "v" + v
        older = tags[tags.index(tv) + 1:] if tv in tags else tags
        if not older:
            sys.exit("nessun tag prima di v" + v)
        prec = older[0][1:]

    def data_tag(t):
        d = git("for-each-ref", "--format=%(creatordate:iso-strict)", "refs/tags/" + t)
        return when(d) if d else None

    lo = data_tag("v" + prec)
    if lo is None:
        sys.exit("il tag v%s non esiste" % prec)
    hi = data_tag("v" + v)
    fino = "v" + v if hi else "dev"
    if hi is None:   # versione non ancora pubblicata: il ciclo arriva a adesso
        hi = dt.datetime.now(dt.timezone.utc)
    return v, prec, lo, hi, fino


# ------------------------------------------------------------------- testo e privacy

def parole_private():
    p = pathlib.Path(git("rev-parse", "--git-dir") or ".git") / "info" / "parole-private.txt"
    try:
        righe = p.read_text(encoding="utf-8").splitlines()
    except OSError:
        return None
    parole = [r.strip() for r in righe if r.strip() and not r.lstrip().startswith("#")]
    if not parole:
        return None
    return re.compile("|".join(re.escape(x) for x in sorted(parole, key=len, reverse=True)), re.I)


PRIVATE = None


def pulito(s):
    s = s or ""
    if PRIVATE:
        s = PRIVATE.sub("[parola privata]", s)
    return s


def corto(s, n):
    s = " ".join((s or "").split())
    return s if len(s) <= n else s[: n - 1] + "…"


def testo_di(content):
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        out = []
        for b in content:
            if isinstance(b, dict):
                if b.get("type") == "text":
                    out.append(b.get("text", ""))
                elif b.get("type") == "image":
                    out.append("[immagine]")
            elif isinstance(b, str):
                out.append(b)
        return "\n".join(out)
    return ""


SYSREM = re.compile(r"<system-reminder>.*?</system-reminder>", re.S)
CMDNAME = re.compile(r"<command-name>(.*?)</command-name>", re.S)
CMDARGS = re.compile(r"<command-args>(.*?)</command-args>", re.S)


def messaggio_umano(o):
    """Il testo scritto da Zyruel, senza i promemoria del sistema; None se il record non e' suo."""
    if o.get("type") != "user" or o.get("isMeta") or o.get("isSidechain"):
        return None
    kind = (o.get("origin") or {}).get("kind")
    if kind != "human" and o.get("turnOrigin") != "human":
        return None
    t = testo_di((o.get("message") or {}).get("content"))
    if "tool_result" in json.dumps((o.get("message") or {}).get("content"))[:200]:
        return None
    m = CMDNAME.search(t)
    if m:   # un comando come /rilascio: si tiene il nome con gli argomenti
        a = CMDARGS.search(t)
        return (m.group(1).strip() + " " + (a.group(1).strip() if a else "")).strip()
    t = SYSREM.sub("", t).strip()
    return t or None


# ------------------------------------------------------------- errori e comandi

# Categorie degli errori gia' viste nei bilanci: il testo grezzo cambia (percorsi, numeri),
# la causa no. Quello che non rientra si raggruppa col testo normalizzato.
CATEGORIE = [
    (r"Bloccato da \.claude/hooks/regole\.sh: ([^.(]*)", "hook regole.sh: {0}"),
    (r"frontmost", "schermo: la finestra giusta non era davanti"),
    (r"granted at tier", "schermo: app con permesso limitato (tier)"),
    (r"is not granted for this session|Call request_access", "schermo: app non concessa (request_access)"),
    (r"request_access.*(not found|non trovat|unknown|not recognized)", "schermo: request_access con nome sbagliato"),
    (r"could not be verified", "schermo: azione non verificata in tempo"),
    (r"Another Claude session is currently using the computer", "schermo: occupato da un'altra sessione"),
    (r"exceeds the coordinate frame|would run with the left mouse", "schermo: batch rifiutato (coordinate o tasti)"),
    (r"No such tool available", "strumento inesistente o non caricato"),
    (r"Blocked: sleep", "Bash: sleep bloccato"),
    (r"Tool call interrupted", "chiamata interrotta dalla fine della sessione"),
    (r"Permission for this action was denied", "permesso negato dal classificatore"),
    (r"doesn't want to proceed|user rejected|rejected by the user", "permesso negato da Zyruel"),
    (r"build\.bat.*(non .* riconosciuto|not recognized)|riconosciuto come comando", "build.bat non trovato (cartella o .\\)"),
    (r"vswhere|vcvars", "Visual Studio non trovato"),
    (r"LNK1104", "link: exe in uso (LNK1104)"),
    (r"error C\d{4}", "compilazione: errore del compilatore"),
    (r"String to replace not found|old_string.*not found|NON TROVATO", "Edit: testo da sostituire non trovato"),
    (r"Found \d+ matches of the string", "Edit: testo da sostituire non univoco"),
    (r"has not been read yet|must Read", "Edit/Write senza Read prima"),
    (r"has been modified since read|modified since", "Edit: file cambiato dopo la lettura"),
    (r"unexpected EOF|syntax error near|unterminated", "Bash: virgolette o sintassi"),
    (r"No such file or directory|cannot find the path|Impossibile trovare il percorso", "percorso inesistente"),
    (r"UnicodeEncodeError|charmap", "Python: codifica della console"),
    (r"InputValidationError", "strumento chiamato con parametri sbagliati"),
    (r"timed out|Timeout|timeout", "tempo scaduto"),
]
CATEGORIE = [(re.compile(p, re.I), l) for p, l in CATEGORIE]


def normalizza(s):
    s = re.sub(r"[A-Za-z]:[\\/][^\s'\"]*|/[a-z]/[^\s'\"]*", "<percorso>", s)
    s = re.sub(r"\b[0-9a-f]{7,}\b", "<id>", s)
    s = re.sub(r"\d+", "N", s)
    return corto(s, 90)


def categoria(err):
    for rx, label in CATEGORIE:
        m = rx.search(err)
        if m:
            return label.format(*[corto(g, 60) for g in m.groups()]) if m.groups() else label
    righe = [r for r in err.splitlines() if r.strip() and not r.startswith("Exit code")]
    testa = err.splitlines()[0] if err.strip() else "(vuoto)"
    return "altro: " + normalizza(testa if testa.startswith("Exit code") and not righe
                                  else (righe[0] if righe else testa))


# Una compilazione vera (non un grep che nomina build.bat) e gli avvisi di MSVC o di zig: gli
# avvisi di git sui fine riga ("warning: in the working copy") non contano.
BUILD = re.compile(r"(?:^|[;&|]\s*|cmd\s+/+c\s+['\"]?)(?:&\s*)?['\"]?\.?[\\/]?build(_zig)?\.bat", re.I)
AVVISO = re.compile(r"\bwarning (C|LNK)\d{4}|\.(cpp|h|hlsl):\d+:\d+: warning:")
# Comandi che leggono o ritoccano testo: sono tanti e sparsi, e nelle sequenze coprirebbero i
# passi che contano (compilare, provare, committare).
LETTURA = {"grep", "rg", "sed", "awk", "cat", "head", "tail", "ls", "file", "wc", "find", "echo",
           "printf", "cut", "sort", "uniq", "diff", "cmp", "stat", "test", "[", "true", "sleep", "cd"}
CD = re.compile(r"^\s*cd\s+(\"[^\"]*\"|'[^']*'|\S+)\s*(&&|;)\s*")


def senza_cd(cmd):
    return CD.sub("", cmd.strip())


def chiave_comando(cmd):
    """Il comando ridotto alla sua forma (programma, sottocomando o script), per contare le
    ripetizioni senza che gli argomenti le separino."""
    segs = re.split(r"\s*(?:&&|\|\||;|\|)\s*", senza_cd(cmd))
    # Le assegnazioni da sole (S="…") e i cd non dicono cosa fa il comando.
    segs = [s for s in segs if s and not re.match(r"^(cd\b|\w+=\S*$)", s)]
    if not segs:
        return "cd"
    s = re.sub(r"^(\w+=\S+\s+)+", "", segs[0])
    w = [x.strip("'\"") for x in s.split()]
    w = [x for x in w if x]
    if not w:
        return "?"
    p = re.sub(r"^.*[\\/]", "", w[0]).lower()
    if p == "cmd" and len(w) > 2:   # cmd //c '.\build.bat dev' -> build.bat dev
        iw = w[2:]
        return re.sub(r"^.*[\\/]", "", iw[0]) + (" " + iw[1] if len(iw) > 1 and re.match(r"^\w+$", iw[1]) else "")
    if p in ("git", "gh", "npm", "winget", "taskkill", "tasklist") and len(w) > 1:
        sub = [x for x in w[1:] if not x.startswith("-")]
        return p + (" " + sub[0] if sub else "") + (" " + sub[1] if p == "gh" and len(sub) > 1 else "")
    if p.startswith("python") or p in ("py", "powershell", "pwsh", "bash", "sh"):
        script = next((x for x in w[1:] if re.search(r"\.(py|ps1|sh|bat)$", x, re.I)), None)
        if script:
            return p + " " + re.sub(r"^.*[\\/]", "", script)
        return p + " (codice in linea)"
    return p


def permessi_progetto():
    out = []
    for nome in ("settings.json", "settings.local.json"):
        try:
            d = json.loads(pathlib.Path(".claude", nome).read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        for regola in d.get("permissions", {}).get("allow", []):
            m = re.match(r"^(Bash|PowerShell)\((.*)\)$", regola)
            if m:
                pat = m.group(2)
                out.append((m.group(1), pat[:-2] if pat.endswith(":*") else pat, pat.endswith(":*")))
    return out


def permesso(tool, cmd, regole):
    c = senza_cd(cmd)
    return any(t == tool and (c.startswith(p) if pref else c == p) for t, p, pref in regole)


# ------------------------------------------------------------------ le trascrizioni

def cartelle_trascrizioni():
    radice = git("rev-parse", "--show-toplevel")
    codice = re.sub(r"[^A-Za-z0-9]", "-", radice).lower()
    base = pathlib.Path.home() / ".claude" / "projects"
    # La cartella del progetto e quelle dei suoi worktree (stesso prefisso).
    return [d for d in sorted(base.iterdir()) if d.is_dir() and d.name.lower().startswith(codice)]


class Sessione:
    def __init__(self, sid):
        self.sid = sid
        self.titolo = None
        self.inizio = self.fine = None
        self.prima = False          # ha record prima della finestra (sessione ripresa)
        self.messaggi = []
        self.strumenti = collections.Counter()
        self.errori = collections.Counter()
        self.skill = collections.Counter()
        self.agenti = collections.Counter()
        self.comandi = []           # (chiave, comando, permesso nelle regole)
        self.interruzioni = 0
        self.blocchi = 0
        self.negati = 0
        self.decisioni = collections.Counter()
        self.build = collections.Counter()
        self.costo = None
        self.sub = 0                # subagenti con record nella finestra
        self.sub_strumenti = 0
        self.sub_errori = 0


def leggi(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        for riga in f:
            try:
                yield json.loads(riga)
            except ValueError:
                continue


def esamina(path, s, lo, hi, regole, sub=False):
    usi = {}            # tool_use_id -> (nome, comando)
    dentro = False
    for o in leggi(path):
        t = o.get("type")
        if t == "custom-title":
            s.titolo = o.get("customTitle") or s.titolo
        if t == "cost-state" and not sub:
            s.costo = o.get("totalCostUSD")
        ts = o.get("timestamp")
        if not ts:
            continue
        w = when(ts)
        if w < lo:
            s.prima = s.prima or not sub
            continue
        if w > hi:
            continue
        dentro = True
        if not sub:
            s.inizio = min(s.inizio or w, w)
            s.fine = max(s.fine or w, w)
            m = messaggio_umano(o)
            if m:
                s.messaggi.append((w, m))
        content = (o.get("message") or {}).get("content")
        if not isinstance(content, list):
            continue
        for b in content:
            if not isinstance(b, dict):
                continue
            if b.get("type") == "tool_use":
                nome, inp = b.get("name", "?"), b.get("input") or {}
                if sub:
                    s.sub_strumenti += 1
                    continue
                s.strumenti[nome] += 1
                cmd = inp.get("command") if nome in ("Bash", "PowerShell") else None
                usi[b.get("id")] = (nome, cmd)
                if nome == "Skill":
                    s.skill[inp.get("skill", "?")] += 1
                elif nome in ("Agent", "Task"):
                    s.agenti[inp.get("subagent_type") or "general-purpose"] += 1
                elif cmd:
                    s.comandi.append((chiave_comando(cmd), cmd, permesso(nome, cmd, regole)))
            elif b.get("type") == "tool_result":
                testo = testo_di(b.get("content"))
                if sub:
                    s.sub_errori += bool(b.get("is_error"))
                    continue
                nome, cmd = usi.get(b.get("tool_use_id"), ("?", None))
                if cmd and BUILD.search(cmd):
                    s.build["avvii"] += 1
                    if b.get("is_error"):
                        s.build["falliti"] += 1
                    if AVVISO.search(testo):
                        s.build["con avvisi"] += 1
                if b.get("is_error"):
                    cat = categoria(testo)
                    s.errori[cat] += 1
                    s.blocchi += cat.startswith("hook")
                    s.negati += cat.startswith("permesso")
            elif b.get("type") == "text" and "[Request interrupted by user" in b.get("text", ""):
                s.interruzioni += 1
        d = o.get("permissionDecision")
        if isinstance(d, dict) and not sub:
            s.decisioni["%s/%s" % (d.get("decision"), d.get("reasonType") or d.get("source"))] += 1
    return dentro


def sessioni(lo, hi, regole):
    tutte = []
    for cart in cartelle_trascrizioni():
        for f in sorted(cart.glob("*.jsonl")):
            s = Sessione(f.stem)
            s.cartella = cart.name
            if not esamina(f, s, lo, hi, regole):
                continue
            for sf in sorted((cart / f.stem / "subagents").glob("*.jsonl")):
                if esamina(sf, s, lo, hi, regole, sub=True):
                    s.sub += 1
            tutte.append(s)
    tutte.sort(key=lambda s: s.inizio or lo)
    return tutte


# ------------------------------------------------------------------------- git

def storia(prec, fino):
    rng = "v%s..%s" % (prec, fino)
    log = git("log", "--reverse", "--no-merges", "--format=@@%h\t%cI\t%s", "--name-only", rng)
    commit, files = [], collections.defaultdict(list)
    for riga in log.splitlines():
        if riga.startswith("@@"):
            h, d, sog = riga[2:].split("\t", 2)
            commit.append([h, d, sog, []])
        elif riga.strip() and commit:
            commit[-1][3].append(riga.strip())
            files[riga.strip()].append(commit[-1][0])
    stat = git("diff", "--shortstat", rng)
    return rng, commit, files, stat


PROCESSO = re.compile(r"^(\.claude/|\.githooks/|CLAUDE\.md|NOVITA\.md|README\.md|docs/|prove/da-verificare\.md|build.*\.bat)")


# ---------------------------------------------------------------------- il resoconto

def tabella(intestazione, righe):
    out = ["| " + " | ".join(intestazione) + " |", "|" + "---|" * len(intestazione)]
    out += ["| " + " | ".join(str(c).replace("|", "\\|") for c in r) + " |" for r in righe]
    return out


def ora(w):
    return w.astimezone().strftime("%d/%m %H:%M") if w else "-"


def scrivi(v, prec, lo, hi, fino, ss, regole):
    rng, commit, files, stat = storia(prec, fino)
    L = []
    a = L.append
    costo = sum(s.costo or 0 for s in ss)
    a("# Dati del ciclo %s → %s" % (prec, v))
    a("")
    a("Finestra: %s → %s (%s). Raccolti il %s con `raccogli.py`. File privato: non va nel "
      "repository." % (ora(lo), ora(hi), "fino al tag" if fino != "dev" else "versione non ancora pubblicata, fino ad adesso",
                       dt.datetime.now().strftime("%d/%m/%Y %H:%M")))
    a("")
    a("## Riepilogo")
    a("")
    tot = lambda f: sum(f(s) for s in ss)
    a("- Sessioni: %d (riprese da prima del ciclo: %d); subagenti: %d" % (len(ss), tot(lambda s: s.prima), tot(lambda s: s.sub)))
    a("- Messaggi di Zyruel: %d" % tot(lambda s: len(s.messaggi)))
    a("- Chiamate agli strumenti: %d (più %d nei subagenti); errori: %d (più %d nei subagenti)"
      % (tot(lambda s: sum(s.strumenti.values())), tot(lambda s: s.sub_strumenti),
         tot(lambda s: sum(s.errori.values())), tot(lambda s: s.sub_errori)))
    a("- Blocchi di regole.sh: %d; permessi negati: %d; interruzioni: %d"
      % (tot(lambda s: s.blocchi), tot(lambda s: s.negati), tot(lambda s: s.interruzioni)))
    a("- Costo dichiarato dalle sessioni: %.2f $ (per le sessioni riprese è il costo intero, anche "
      "di prima del ciclo)" % costo)
    a("- Git (%s): %d commit; %s" % (rng, len(commit), stat or "nessuna differenza"))
    a("")

    a("## Sessioni")
    a("")
    a("Le sessioni cloud non lasciano trascrizioni su questo PC: qui non ci sono.")
    a("")
    righe = []
    for s in ss:
        righe.append([s.sid[:8], pulito(corto(s.titolo or "(senza titolo)", 50)), ora(s.inizio), ora(s.fine),
                      len(s.messaggi), sum(s.strumenti.values()), sum(s.errori.values()), s.sub,
                      ("%.2f" % s.costo if s.costo else "-") + (" *" if s.prima else ""),
                      ", ".join("%s %d" % kv for kv in s.skill.most_common()) or "-"])
    L += tabella(["id", "titolo", "inizio", "fine", "msg", "strum.", "errori", "sub", "costo $", "skill"], righe)
    a("")
    a("`*` sessione ripresa da prima del ciclo: il costo comprende anche il lavoro di prima.")
    a("")

    a("## Skill, comandi e subagenti")
    a("")
    sk, ag, slash = collections.Counter(), collections.Counter(), collections.Counter()
    for s in ss:
        sk.update(s.skill)
        ag.update(s.agenti)
        for _, m in s.messaggi:
            if m.startswith("/"):
                slash[m.split()[0]] += 1
    a("- Skill chiamate da Claude: " + (", ".join("`%s` %d" % kv for kv in sk.most_common()) or "nessuna"))
    a("- Comandi scritti da Zyruel: " + (", ".join("`%s` %d" % kv for kv in slash.most_common()) or "nessuno"))
    a("- Subagenti per tipo: " + (", ".join("`%s` %d" % kv for kv in ag.most_common()) or "nessuno"))
    a("")

    a("## Strumenti più usati")
    a("")
    st = collections.Counter()
    for s in ss:
        st.update(s.strumenti)
    L += tabella(["strumento", "chiamate"], st.most_common(TOP))
    a("")

    a("## Errori per categoria")
    a("")
    er, dove = collections.Counter(), collections.defaultdict(set)
    for s in ss:
        for k, n in s.errori.items():
            er[k] += n
            dove[k].add(s.sid[:8])
    L += tabella(["categoria", "volte", "sessioni"],
                 [[pulito(k), n, "%d: %s" % (len(dove[k]), ", ".join(sorted(dove[k])))] for k, n in er.most_common(40)])
    a("")

    a("## Decisioni sui permessi")
    a("")
    dec = collections.Counter()
    for s in ss:
        dec.update(s.decisioni)
    a(", ".join("`%s` %d" % kv for kv in dec.most_common()) or "nessuna registrata")
    a("")

    a("## Comandi di shell più usati")
    a("")
    a("`nelle regole`: quante volte il comando era già permesso da `.claude/settings.json` "
      "(o `settings.local.json`). Un comando frequente e mai permesso è un candidato a un "
      "permesso o a uno script.")
    a("")
    cm, ok, sess, esempio = collections.Counter(), collections.Counter(), collections.defaultdict(set), {}
    for s in ss:
        for k, c, p in s.comandi:
            cm[k] += 1
            ok[k] += p
            sess[k].add(s.sid[:8])
            esempio.setdefault(k, c)
    L += tabella(["comando", "volte", "nelle regole", "sessioni", "esempio"],
                 [[k, n, ok[k], len(sess[k]), "`%s`" % pulito(corto(senza_cd(esempio[k]), 80)).replace("`", "'")]
                  for k, n in cm.most_common(TOP)])
    a("")

    a("## Sequenze di comandi ripetute")
    a("")
    a("Le stesse %d–%d forme di comando una dopo l'altra, in almeno due sessioni: candidati a uno "
      "script o a una skill. Senza i comandi che leggono o ritoccano testo (grep, sed, cat…)."
      % (SEQ_MIN, SEQ_MAX))
    a("")
    seq, seq_s = collections.Counter(), collections.defaultdict(set)
    for s in ss:
        ks = [k for k, _, _ in s.comandi if (k.split() or ["?"])[0] not in LETTURA]
        for n in range(SEQ_MIN, SEQ_MAX + 1):
            for i in range(len(ks) - n + 1):
                g = tuple(ks[i:i + n])
                if len(set(g)) > 1:   # "git diff" ripetuto non e' una sequenza
                    seq[g] += 1
                    seq_s[g].add(s.sid[:8])
    buone = [(g, c) for g, c in seq.items() if len(seq_s[g]) >= 2 and c >= 3]
    # Si tiene la sequenza piu' lunga quando una piu' corta ha lo stesso numero di ripetizioni.
    buone.sort(key=lambda gc: (-gc[1], -len(gc[0])))
    tenute = []
    for g, c in buone:
        if any(c == c2 and len(g2) > len(g) and any(g2[i:i + len(g)] == g for i in range(len(g2) - len(g) + 1))
               for g2, c2 in buone):
            continue
        tenute.append((g, c))
    L += tabella(["sequenza", "volte", "sessioni"],
                 [[" → ".join(g), c, len(seq_s[g])] for g, c in tenute[:15]]) if tenute else ["nessuna"]
    a("")

    a("## Compilazioni")
    a("")
    b = collections.Counter()
    for s in ss:
        b.update(s.build)
    a("`build.bat`: %d avvii, %d falliti, %d con avvisi nell'uscita." % (b["avvii"], b["falliti"], b["con avvisi"]))
    a("")

    a("## Git")
    a("")
    for h, d, sog, fs in commit:
        a("- `%s` %s %s (%d file)" % (h, ora(when(d)), pulito(sog), len(fs)))
    a("")
    piu = sorted(((f, hs) for f, hs in files.items() if len(hs) > 1), key=lambda x: -len(x[1]))
    a("File toccati da più commit (possibili rifacimenti):")
    a("")
    L += tabella(["file", "commit"], [[f, len(hs)] for f, hs in piu[:TOP]]) if piu else ["nessuno"]
    a("")
    proc = sorted(f for f in files if PROCESSO.match(f))
    a("File di processo e documenti cambiati: " + (", ".join("`%s`" % f for f in proc) or "nessuno"))
    a("")

    a("## Messaggi di Zyruel")
    a("")
    a("Accorciati a %d caratteri, con le parole private mascherate." % MAX_MSG)
    for s in ss:
        if not s.messaggi:
            continue
        a("")
        a("### %s · %s" % (s.sid[:8], pulito(s.titolo or "(senza titolo)")))
        a("")
        for w, m in s.messaggi:
            a("- %s — %s" % (ora(w), pulito(corto(m, MAX_MSG))))
    a("")
    return "\n".join(L)


def main():
    global PRIVATE
    sys.stdout.reconfigure(encoding="utf-8")
    PRIVATE = parole_private()
    v, prec, lo, hi, fino = ciclo(sys.argv)
    regole = permessi_progetto()
    ss = sessioni(lo, hi, regole)
    out = pathlib.Path("build", "bilanci")
    out.mkdir(parents=True, exist_ok=True)
    dest = out / ("%s-dati.md" % v)
    dest.write_text(scrivi(v, prec, lo, hi, fino, ss, regole), encoding="utf-8")
    print("%s → %s: %d sessioni, %d messaggi di Zyruel, %d errori; scritto %s"
          % (prec, v, len(ss), sum(len(s.messaggi) for s in ss), sum(sum(s.errori.values()) for s in ss),
             dest.as_posix()))


if __name__ == "__main__":
    main()
