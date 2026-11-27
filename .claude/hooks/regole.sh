#!/usr/bin/env bash
# Hook PreToolUse di Claude Code per Bash, PowerShell e due strumenti del controllo dello schermo
# (request_access, open_application; .claude/settings.json): ferma prima che
# partano i comandi che infrangono le regole di CLAUDE.md. Uscita 2 = comando bloccato, e il motivo
# arriva a Claude su stderr. Le regole sono scritte lì; qui solo il controllo, perché a memoria
# venivano dimenticate (finestre chiuse per nome: 20 volte nelle sessioni della 1.0.0-beta.4).
# Prova: bash .claude/hooks/regole.sh --prova

if [ "$1" = "--prova" ]; then
    # comando atteso (0 passa, 2 bloccato) e comando, come arriva nel JSON
    ok=0; ko=0
    while IFS='|' read -r atteso c; do
        printf '{"tool_input":{"command":"%s","description":"x"}}' "$c" | bash "$0" 2>/dev/null
        r=$?
        if [ "$r" = "$atteso" ]; then ok=$((ok+1)); else ko=$((ko+1)); echo "SBAGLIATO ($r invece di $atteso): $c"; fi
    done <<'CASI'
2|taskkill //IM z-f-dev.exe
2|taskkill /F /IM \"z-f.exe\"
2|sleep 1; taskkill //im z-f-dev.exe >/dev/null
2|Stop-Process -Name z-f-dev -Force
2|Get-Process z-f-dev -ErrorAction SilentlyContinue | Stop-Process -Confirm:$false
2|Get-Process z-f | ForEach-Object { $_.CloseMainWindow() }
2|reg add HKCU\\Software\\Classes\\Folder /f
2|Set-ItemProperty -Path HKCU:\\Software\\x -Name a -Value 1
2|git commit --no-verify -m x
2|git push --no-verify origin dev
2|git push origin main
2|git push origin dev:main
2|cd x && git push -u origin main
2|git push --force origin dev
2|git push -f origin dev
0|taskkill //PID 1234
0|taskkill //F //PID 1234 && tasklist //FI \"IMAGENAME eq z-f-dev.exe\"
0|Stop-Process -Id 1234
0|Get-Process z-f, z-f-dev | Select-Object Id, StartTime
0|tasklist //FI \"IMAGENAME eq z-f.exe\"
0|reg query HKCU\\Software\\Classes\\Folder
0|Get-ItemProperty HKCU:\\Software\\x
0|git push origin dev
0|git push -u origin dev
0|git push origin v1.0.0-beta.4
0|git push --force-with-lease origin dev
0|git push --follow-tags origin dev
0|git fetch origin main
0|git merge --ff-only origin/main
0|git commit -q -F - <<'EOF'\nMai --no-verify ne' push su main\nEOF
0|grep -n \"taskkill /IM\" CLAUDE.md
0|cmd //c '.\\build.bat dev'
CASI
    # Controllo dello schermo: JSON intero (atteso|strumento|input)
    while IFS='|' read -r atteso t i; do
        printf '{"session_id":"x","cwd":"C:\\\\x\\\\[z]f","tool_name":"%s","tool_input":%s}' "$t" "$i" | bash "$0" 2>/dev/null
        r=$?
        if [ "$r" = "$atteso" ]; then ok=$((ok+1)); else ko=$((ko+1)); echo "SBAGLIATO ($r invece di $atteso): $t $i"; fi
    done <<'CASI'
2|mcp__computer-use__request_access|{"apps":["z-f-dev"],"reason":"prova"}
2|mcp__computer-use__request_access|{"apps":["[z]f - C:\\Users\\utente"],"reason":"prova"}
2|mcp__computer-use__request_access|{"apps":["File Explorer","Z-f-dev"],"reason":"prova"}
2|mcp__computer-use__open_application|{"app":"z-f-dev.exe"}
2|mcp__computer-use__open_application|{"app":"Z-f-dev"}
0|mcp__computer-use__request_access|{"apps":["z-f-dev.exe","File Explorer"],"reason":"z-f-dev e [z]f"}
0|mcp__computer-use__request_access|{"apps":["C:\\Users\\utente\\[z]f\\build\\z-f-dev.exe"],"reason":"prova"}
0|mcp__computer-use__request_access|{"apps":["dialoghi-prova.exe"],"reason":"z-f-dev"}
0|mcp__computer-use__open_application|{"app":"Rundll32"}
CASI
    echo "prove: $ok giuste, $ko sbagliate"
    [ "$ko" = 0 ]; exit
fi

in=$(cat)
shopt -s nocasematch

blocca() {
    printf 'Bloccato da .claude/hooks/regole.sh: %s\n' "$1" >&2
    [ -n "$cmd" ] && printf '(Se il testo bloccato era solo una stringa, in un grep o in un messaggio, riformulalo.)\n' >&2
    exit 2
}

# Controllo dello schermo (/prova, sezione 4): i nomi che lo strumento non riconosce e la finestra
# di prova in più. Sbagliati anche nelle sessioni che non caricano /prova (1.0.0-beta.6).
tool=$(printf '%s' "$in" | sed -nE 's/.*"tool_name"[[:space:]]*:[[:space:]]*"([^"]*)".*/\1/p')
zf='(z-f|\[z\]f)'
case $tool in
mcp__computer-use__request_access)
    apps=$(printf '%s' "$in" | sed -nE 's/.*"apps"[[:space:]]*:[[:space:]]*\[(("([^"\\]|\\.)*"[[:space:],]*)*)\].*/\1/p')
    while read -r a; do
        a=${a#\"}; a=${a%\"}
        [[ $a =~ $zf && ! $a =~ \.exe$ ]] && blocca "\"$a\" non è un nome che lo strumento riconosce. Per la finestra di prova si chiede [\"z-f-dev.exe\", \"File Explorer\"] dopo averla avviata con prove\\dev.bat (/prova, sezione 4)."
    done < <(printf '%s' "$apps" | grep -oE '"([^"\\]|\\.)*"')
    exit 0 ;;
mcp__computer-use__open_application)
    app=$(printf '%s' "$in" | sed -nE 's/.*"app"[[:space:]]*:[[:space:]]*"(([^"\\]|\\.)*)".*/\1/p')
    [[ $app =~ $zf ]] && blocca "open_application su [z]f avvia un'altra finestra di prova. Per portare davanti z-f-dev si clicca la sua barra del titolo; se davanti c'è il desktop serve \"File Explorer\" in request_access (/prova, sezione 4)."
    exit 0 ;;
esac

# Solo il comando, ancora con gli escape del JSON (\" e \\): il resto del JSON contiene percorsi
# della sessione che hanno "z-f" nel nome
cmd=$(printf '%s' "$in" | sed -nE 's/.*"command"[[:space:]]*:[[:space:]]*"(([^"\\]|\\.)*)".*/\1/p')
[ -z "$cmd" ] && exit 0
chk() { [[ $cmd =~ $1 ]] && blocca "$2"; }

Q="\"'"                              # virgolette doppie e semplici
A="(^|[;&|({${Q}\\]|\\\\n)[[:space:]]*" # inizio di un comando: riga, separatore, virgolette
S="[[:space:]${Q}\\]"                # spazi e virgolette (anche con l'escape del JSON)
N='[^;&|]*'                          # resto dello stesso comando

# Finestre chiuse per nome: chiuderebbero anche quelle di Zyruel (z-f.exe) o le finestre di prova
# di un'altra sessione (z-f-dev.exe). Si chiude per PID.
m="chiude per nome le finestre di [z]f. Si chiude per PID: taskkill //PID <pid> (Bash) o Stop-Process -Id <pid> (CLAUDE.md, \"Compilare e provare\")."
chk "${A}taskkill${N}[/-]+im${S}+z-f" "$m"
chk "${A}stop-process${N}-(process)?name${S}+z-f" "$m"
chk "${A}get-process${N}z-f[^;]*[|][^;]*(stop-process|[.]kill|closemainwindow)" "$m"
chk "${A}(pkill|killall)${N}z-f" "$m"
chk "${A}wmic${N}z-f${N}(terminate|delete)" "$m"

# Registro: l'interruttore "Usa [z]f al posto di Explorer" lo usa Zyruel, non Claude
m="scrive nel registro di Windows. Non si scrive nel registro al posto di Zyruel (CLAUDE.md, \"Compilare e provare\"): glielo si chiede."
chk "${A}reg([.]exe)?[[:space:]]+(add|delete|import|copy|restore|load|unload)([[:space:]]|$)" "$m"
chk "${A}(set|new|remove|rename|clear)-item(property)?${N}(hk(cu|lm|cr|u|cc)|registry::)" "$m"

# Git: main si aggiorna solo con la pull request di /rilascio; i controlli del pre-push non si
# scavalcano; niente push forzati. Il messaggio di un commit (dopo -m "…" o in un heredoc <<) non
# conta.
G="${A}git[[:space:]]+(-C${S}+[^[:space:]]+[[:space:]]+)?"
chk "${G}(commit|push|merge|rebase|cherry-pick|am)[[:space:]]([^;&|<${Q}]*[[:space:]])?--no-verify" \
    "usa --no-verify. I controlli di .githooks non si scavalcano: si corregge il commit (CLAUDE.md, \"Repository\")."
chk "${G}push${N}[[:space:]:+${Q}\\]main([[:space:]${Q}\\;&|]|$)" \
    "pusha su main. main si aggiorna solo con la pull request di /rilascio, che lancia Zyruel (CLAUDE.md, \"Repository\")."
chk "${G}push${N}[[:space:]](--force([[:space:]${Q}\\;&|=]|$)|-[a-z]*f[a-z]*([[:space:]]|$))" \
    "forza il push. Su origin non si riscrive la storia: chiedilo a Zyruel."

exit 0
