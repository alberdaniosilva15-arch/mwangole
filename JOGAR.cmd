@echo off
setlocal
cd /d "%~dp0"

where python >nul 2>nul
if errorlevel 1 (
  echo Python nao encontrado no PATH.
  pause
  exit /b 1
)

start "MWANGOLÉ server" /min python tools\serve.py
timeout /t 1 /nobreak >nul
start "" "http://127.0.0.1:8090/?dev"

endlocal
