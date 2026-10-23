@echo off
setlocal
cd /d "%~dp0"
echo Downhill Revived - Player Hosted relay (port 8787)
echo Keep this window open while friends are playing.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0online\player-host.ps1"
if errorlevel 1 (
    echo.
    echo [ERROR] The local relay could not start. See the message above.
) else (
    echo.
    echo The local relay has stopped.
)
pause
