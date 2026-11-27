@echo off
rem Apre cartelle come farebbe un altro programma (ShellExecute) e simula Win+E.
rem Con "[z]f al posto di Explorer" attivo devono finire tutte in [z]f.
start "" "%SystemRoot%\System32"
start "" "C:\"
start "" explorer.exe
