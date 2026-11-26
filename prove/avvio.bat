@echo off
rem Tempi di avvio: apre [z]f N volte con --prova-avvio (finestra separata, che si chiude da sola
rem quando arriva la prima icona vera, al massimo 2 s dopo il primo frame) e scrive nel registro
rem accanto all'exe "prova avvio: contenuto", "primo fuoco", "icone" (la prima icona vera, dall'avvio
rem del processo) e "icona fin." (l'icona della finestra; il codice e' la misura grande in px).
rem Ogni avvio porta una finestra in primo piano: non usare il PC intanto.
rem Uso: avvio.bat [volte] [exe]      (predefiniti: 10, build\z-f-dev.exe)
setlocal
cd /d "%~dp0.."
set "N=%~1"
if "%N%"=="" set "N=10"
set "EXE=%~2"
if "%EXE%"=="" set "EXE=build\z-f-dev.exe"
for /l %%i in (1,1,%N%) do start "" /wait "%EXE%" --prova-avvio
