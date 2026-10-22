@echo off
setlocal
cd /d "%~dp0"
echo Downhill Port - iniciando...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Play-Downhill.ps1"
if errorlevel 1 (
    echo.
    echo Consulte a mensagem acima. Pressione uma tecla para fechar.
    pause >nul
)
