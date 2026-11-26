@echo off
rem Compila [z]f con MSVC.  Uso:  build.bat          (release)
rem                             build.bat debug    (debug con simboli)
rem                             build.bat dev      (build di prova: build\z-f-dev.exe, con ZF_DEV)
rem                             build.bat firma    (strumenti\firma.cpp: firma dei rilasci)
rem                             build.bat prova prove\x.cpp [librerie e opzioni di cl]
rem                                                (un programma di prova: build\x.exe)
rem Da Bash, nella cartella del progetto:  cmd //c '.\build.bat dev'
rem (con ".\": dove la cartella corrente non e' nella ricerca dei comandi, "build.bat" da solo non
rem si trova; da PowerShell no: non entra in una cartella con le parentesi quadre)
setlocal
cd /d "%~dp0"

set "MODE=%~1"
if "%MODE%"=="" set "MODE=release"

where cl >nul 2>nul
if %errorlevel%==0 goto :havecl

rem cl.exe non e' nel PATH: cerco Visual Studio con vswhere e carico vcvars64.
rem "call" serve: il percorso contiene (x86) e senza, cmd toglierebbe le virgolette al comando.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :novs
rem vcvars64 chiama vswhere per nome: senza la sua cartella nel PATH scrive "vswhere.exe non e'
rem riconosciuto" (innocuo, ma rumore nell'uscita, dove si cercano gli avvisi)
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto :nocpp
if not exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" goto :nocpp
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if not %errorlevel%==0 goto :nocpp

:havecl
if not exist build mkdir build
if /i "%MODE%"=="prova" goto :prova
if /i "%MODE%"=="firma" (
    cl /nologo /std:c++17 /utf-8 /EHsc /W3 /O2 /DUNICODE /D_UNICODE /Fo:build\ /Fe:build\firma.exe strumenti\firma.cpp || goto :fail
    echo Fatto: build\firma.exe
    exit /b 0
)

rem Shader precompilati: avvio piu' rapido. Senza fxc si compilano a runtime.
where fxc >nul 2>nul
if not %errorlevel%==0 goto :nofxc
fxc /nologo /T vs_4_0 /E vs /O3 /Fh build\shader_vs.h /Vn g_vs src\shader.hlsl >nul || goto :fail
fxc /nologo /T ps_4_0 /E ps /O3 /Fh build\shader_ps.h /Vn g_ps src\shader.hlsl >nul || goto :fail
goto :compile
:nofxc
echo [avviso] fxc non trovato: gli shader saranno compilati all'avvio.
del /q build\shader_vs.h build\shader_ps.h 2>nul

:compile
rem Risorse dell'exe (l'icona): rc e' nel Windows SDK, come fxc, ma non e' facoltativo: senza, la
rem compilazione si ferma (l'exe resterebbe senza icona)
rc /nologo /fo build\z-f.res src\z-f.rc || goto :fail
rem shell32 e ole32 si caricano solo al primo uso (icone, menu, appunti), winhttp e bcrypt al primo
rem controllo degli aggiornamenti: avvio piu' leggero
set "DELAY=delayimp.lib /DELAYLOAD:shell32.dll /DELAYLOAD:ole32.dll /DELAYLOAD:winhttp.dll /DELAYLOAD:bcrypt.dll"
rem La build di prova (dev) e' quella di rilascio con ZF_DEV, che non si aggiorna da sola, scritta
rem in z-f-dev.exe: si compila anche mentre Zyruel usa z-f.exe
set "OUT=build\z-f.exe"
set "DEV="
if /i "%MODE%"=="dev" set "OUT=build\z-f-dev.exe"
if /i "%MODE%"=="dev" set "DEV=/DZF_DEV"
set "COMMON=/nologo /std:c++17 /utf-8 /EHsc /W3 /DUNICODE /D_UNICODE /Ibuild /Fo:build\ /Fe:%OUT%"
if /i "%MODE%"=="debug" (
    cl %COMMON% /Od /Zi /MTd /Fd:build\ src\z-f.cpp build\z-f.res /link /SUBSYSTEM:WINDOWS /DEBUG %DELAY% || goto :fail
) else (
    cl %COMMON% %DEV% /O2 /Oi /GS- /Gy /MT /DNDEBUG src\z-f.cpp build\z-f.res /link /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF %DELAY% || goto :fail
)
echo.
echo Fatto: %OUT%  [%MODE%]
exit /b 0

:prova
rem Gli argomenti dopo "prova": il sorgente e cio' che gli serve (librerie, /Isrc...). Oggetti ed
rem exe vanno in build\ (escluso da git), con le stesse opzioni per tutti i programmi di prova
set "ARGS=%*"
call set "ARGS=%%ARGS:*prova=%%"
if "%ARGS%"=="" (
    echo Uso: build.bat prova prove\x.cpp [librerie e opzioni di cl]
    exit /b 1
)
cl /nologo /std:c++17 /utf-8 /EHsc /W3 /O2 /MT /DUNICODE /D_UNICODE /Fo:build\ /Fe:build\ %ARGS% || goto :fail
echo.
echo Fatto: programma di prova in build\
exit /b 0

:novs
echo Visual Studio non trovato.
echo Installa "Build Tools per Visual Studio 2022" con "Sviluppo di applicazioni desktop con C++",
echo oppure usa build_zig.bat.
exit /b 1

:nocpp
echo Visual Studio c'e', ma senza il compilatore C++ (cl.exe).
echo Apri "Visual Studio Installer" ^> Modifica ^> spunta "Sviluppo di applicazioni desktop con C++".
echo In alternativa: build_zig.bat.
for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -products * -property displayName`) do echo   trovato: %%i
exit /b 1

:fail
echo.
echo Compilazione fallita.
exit /b 1
