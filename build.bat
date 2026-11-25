@echo off
rem Compila [z]f con MSVC.  Uso:  build.bat          (release)
rem                             build.bat debug    (debug con simboli)
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
for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto :nocpp
if not exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" goto :nocpp
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if not %errorlevel%==0 goto :nocpp

:havecl
if not exist build mkdir build

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
rem shell32 e ole32 si caricano solo al primo uso (icone, menu, appunti): avvio piu' leggero
set "DELAY=delayimp.lib /DELAYLOAD:shell32.dll /DELAYLOAD:ole32.dll"
set "COMMON=/nologo /std:c++17 /utf-8 /EHsc /W3 /DUNICODE /D_UNICODE /Ibuild /Fo:build\ /Fe:build\z-f.exe"
if /i "%MODE%"=="debug" (
    cl %COMMON% /Od /Zi /MTd /Fd:build\ src\z-f.cpp /link /SUBSYSTEM:WINDOWS /DEBUG %DELAY% || goto :fail
) else (
    cl %COMMON% /O2 /Oi /GS- /Gy /MT /DNDEBUG src\z-f.cpp /link /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF %DELAY% || goto :fail
)
echo.
echo Fatto: build\z-f.exe  [%MODE%]
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
