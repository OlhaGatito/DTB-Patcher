@echo off
setlocal
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
  echo [ERRO] Ambiente .venv nao existe.
  echo Execute: py -3 -m venv .venv
  pause
  exit /b 1
)
.venv\Scripts\python.exe main.py
