@echo off
rem Alternativa senza Visual Studio: zig come compilatore C++ (winget install zig.zig)
rem Scrive la build di prova (z-f-dev.exe), non z-f.exe: quella è la build MSVC, con shell32 e ole32
rem a caricamento ritardato, che zig non fa. Gli shader precompilati da build.bat (build\shader_*.h)
rem si usano se ci sono; altrimenti si compilano all'avvio.
setlocal
cd /d "%~dp0"
where zig >nul 2>nul
if not %errorlevel%==0 (
    echo zig non trovato. Installalo con:  winget install zig.zig
    exit /b 1
)
if not exist build mkdir build
zig c++ -target x86_64-windows-gnu -O2 -std=c++17 -municode -Wno-date-time -DUNICODE -D_UNICODE -Ibuild src\z-f.cpp -o build\z-f-dev.exe -Wl,--subsystem,windows -luser32 -lgdi32 -ld3d11 -lshell32 -lole32 -luuid -ladvapi32 || exit /b 1
echo Fatto: build\z-f-dev.exe
