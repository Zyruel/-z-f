@echo off
rem z-f-dev in una finestra separata, anche se [z]f e' gia' aperto (istanza unica)
cd /d "%~dp0.."
start "" build\z-f-dev.exe --new-window %*
