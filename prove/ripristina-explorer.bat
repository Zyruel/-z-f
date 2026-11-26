@echo off
rem Emergenza: riporta Explorer come file manager anche se [z]f non c'e' piu'.
rem Toglie solo le chiavi scritte da [z]f e dal vecchio zx (in HKCU). Con [z]f disponibile e' meglio
rem   z-f.exe --predefinito=no
rem che ripristina anche eventuali valori personalizzati che c'erano prima.
for %%v in (z-f zx) do (
  reg delete "HKCU\Software\Classes\Directory\shell\%%v" /f >nul 2>nul
  reg delete "HKCU\Software\Classes\Drive\shell\%%v" /f >nul 2>nul
  reg query "HKCU\Software\Classes\Directory\shell" /ve 2>nul | find "%%v" >nul && reg delete "HKCU\Software\Classes\Directory\shell" /ve /f >nul
  reg query "HKCU\Software\Classes\Drive\shell" /ve 2>nul | find "%%v" >nul && reg delete "HKCU\Software\Classes\Drive\shell" /ve /f >nul
  reg delete "HKCU\Software\Classes\CompressedFolder\shell\%%v" /f >nul 2>nul
  reg query "HKCU\Software\Classes\CompressedFolder\shell" /ve 2>nul | find "%%v" >nul && reg delete "HKCU\Software\Classes\CompressedFolder\shell" /ve /f >nul
  reg query "HKCU\Software\Classes\CLSID\{52205fd8-5dfb-447d-801a-d0b52f2e83e1}\shell\opennewwindow\command" /ve 2>nul | find /i "%%v" >nul && reg delete "HKCU\Software\Classes\CLSID\{52205fd8-5dfb-447d-801a-d0b52f2e83e1}" /f >nul
  reg delete "HKCU\Software\%%v\Ripristino" /f >nul 2>nul
)
echo Explorer ripristinato.
pause
