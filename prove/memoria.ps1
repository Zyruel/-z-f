# Memoria di una o più finestre di [z]f (o di qualsiasi processo), per PID:
#   powershell -File prove\memoria.ps1 -Ids 1234,5678 [-Label testo]
# Working set, memoria privata (commit), memoria GPU dedicata e condivisa (contatori "GPU Process
# Memory", con i nomi inglesi anche su Windows in italiano), thread e handle. Il PID della finestra
# di prova si legge con tasklist prima e dopo prove\dev.bat (vedi /prova).
param([int[]]$Ids, [string]$Label = '')
foreach ($id in $Ids) {
    $p = Get-Process -Id $id -ErrorAction SilentlyContinue
    if (-not $p) { "$id non trovato"; continue }
    $ded = 0; $sh = 0
    try {
        $c = Get-Counter -Counter "\GPU Process Memory(pid_$($id)_*)\Dedicated Usage", "\GPU Process Memory(pid_$($id)_*)\Shared Usage" -ErrorAction Stop
        foreach ($s in $c.CounterSamples) {
            if ($s.Path -like '*dedicated usage') { $ded += $s.CookedValue } else { $sh += $s.CookedValue }
        }
    } catch {}
    "{0,-22} pid {1,6}  WS {2,7:N1} MB  privata {3,7:N1} MB  GPU ded {4,6:N1} MB  GPU cond {5,6:N1} MB  thread {6}  handle {7}" -f `
        $Label, $id, ($p.WorkingSet64 / 1MB), ($p.PrivateMemorySize64 / 1MB), ($ded / 1MB), ($sh / 1MB), $p.Threads.Count, $p.HandleCount
}
