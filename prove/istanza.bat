@echo off
rem Istanza unica: con z-f-dev gia aperto, queste aprono schede nella finestra esistente
cd /d "%~dp0.."
start "" build\z-f-dev.exe "C:\Windows"
start "" build\z-f-dev.exe "/select,C:\Windows\notepad.exe"
start "" build\z-f-dev.exe "%USERPROFILE%\Documents"
