@echo off
setlocal
cd /d "%~dp0"
set "PYTHONUTF8=1"
where py >nul 2>&1
if not errorlevel 1 (
    py -3 "%~dp0Build-GUI.py"
) else (
    where python >nul 2>&1
    if errorlevel 1 (
        echo Python 3 is required to use the Downhill Port build interface.
        echo Download it from https://www.python.org/downloads/
        pause
        exit /b 1
    )
    python "%~dp0Build-GUI.py"
)
if errorlevel 1 (
    echo.
    echo The build interface exited with an error. Check your Python installation.
    pause
)
