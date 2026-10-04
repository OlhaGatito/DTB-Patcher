@echo off
setlocal
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
  py -3 -m venv .venv
)
.venv\Scripts\python.exe -m pip install --upgrade pip
.venv\Scripts\python.exe -m pip install pyinstaller
if not exist "tools\dtc.exe" (
  echo [AVISO] tools\dtc.exe nao encontrado. O programa ainda pode abrir, mas nao analisara DTBs.
)
.venv\Scripts\python.exe -m PyInstaller --noconfirm --clean --windowed --name DTB-Patcher --add-data "tools;tools" main.py
echo.
echo Build concluido em dist\DTB-Patcher\
pause
