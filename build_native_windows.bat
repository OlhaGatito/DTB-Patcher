@echo off
setlocal EnableExtensions

title Gatito DTB-Patcher - Native Windows Build

echo.
echo ================================================
echo          Gatito DTB-Patcher - Native C++ Build
echo ================================================
echo.

cd /d "%~dp0"
if errorlevel 1 goto :error_cd
set "PROJECT_DIR=%CD%"
set "MSYS2_ROOT="

echo [INFO] Projeto: %PROJECT_DIR%
echo [INFO] Procurando uma instalacao valida do MSYS2...
echo.

rem Avoid a parenthesized FOR block here. %ProgramFiles(x86)% contains
rem parentheses and CMD parses those before executing a compound block.
if defined MSYS2_ROOT_ENV if exist "%MSYS2_ROOT_ENV%\usr\bin\bash.exe" set "MSYS2_ROOT=%MSYS2_ROOT_ENV%"
if not defined MSYS2_ROOT if exist "C:\msys64\usr\bin\bash.exe" set "MSYS2_ROOT=C:\msys64"
if not defined MSYS2_ROOT if exist "%ProgramFiles%\msys64\usr\bin\bash.exe" set "MSYS2_ROOT=%ProgramFiles%\msys64"
if not defined MSYS2_ROOT if defined ProgramFiles(x86) if exist "%ProgramFiles(x86)%\msys64\usr\bin\bash.exe" set "MSYS2_ROOT=%ProgramFiles(x86)%\msys64"
if not defined MSYS2_ROOT if exist "%LOCALAPPDATA%\msys64\usr\bin\bash.exe" set "MSYS2_ROOT=%LOCALAPPDATA%\msys64"
if not defined MSYS2_ROOT if exist "%USERPROFILE%\scoop\apps\msys2\current\usr\bin\bash.exe" set "MSYS2_ROOT=%USERPROFILE%\scoop\apps\msys2\current"

if not defined MSYS2_ROOT goto :error_msys2

set "BASH=%MSYS2_ROOT%\usr\bin\bash.exe"
for /f "delims=" %%U in ('"%MSYS2_ROOT%\usr\bin\cygpath.exe" -u "%PROJECT_DIR%"') do set "PROJECT_DIR_UNIX=%%U"

echo [OK] MSYS2 encontrado: %BASH%
echo.
echo [INFO] Verificando ambiente UCRT64 e dependencias...
echo.

"%BASH%" -lc "export PATH=/ucrt64/bin:/usr/bin; command -v git >/dev/null && command -v flex >/dev/null && command -v bison >/dev/null && command -v gcc >/dev/null && command -v g++ >/dev/null && command -v ar >/dev/null"
if errorlevel 1 goto :error_deps

"%BASH%" -lc "pacman -Q git make flex bison pkgconf diffutils mingw-w64-ucrt-x86_64-gcc >/dev/null 2>&1"
if errorlevel 1 goto :error_deps

echo [OK] Ferramentas e pacotes necessarios encontrados.
echo.
echo [1/3] Baixando e compilando o DTC oficial em commit fixo...
echo.

"%BASH%" native/build_dtc_msys2.sh "%PROJECT_DIR_UNIX%" /tmp/gatito-dtb-patcher-dtc
if errorlevel 1 goto :error_dtc

echo.
echo [2/3] Compilando o Gatito DTB-Patcher e executando os testes...
echo.

"%BASH%" native/build_native_msys2.sh "%PROJECT_DIR_UNIX%" /tmp/gatito-dtb-patcher-dtc
if errorlevel 1 goto :error_tests

if not exist "build\Gatito-DTB-Patcher.exe" goto :error_missing

echo.
echo [3/3] Validando o executavel...
echo.
"%BASH%" -lc "test -s build/Gatito-DTB-Patcher.exe"
if errorlevel 1 goto :error_validate

echo.
echo [OK] Build e todos os testes concluidos.
echo.
echo Executavel:
echo %PROJECT_DIR%\build\Gatito-DTB-Patcher.exe
echo.
echo ================================================
echo              BUILD CONCLUIDO
echo ================================================
echo.
pause
exit /b 0

:error_cd
echo [ERRO] Nao foi possivel entrar na pasta do projeto.
goto :stop

:error_msys2
echo [ERRO] Nenhuma instalacao valida do MSYS2 foi encontrada.
goto :stop

:error_deps
echo [ERRO] O MSYS2 foi encontrado, mas falta uma dependencia de build.
echo [INFO] Instale no MSYS2 UCRT64:
echo        pacman -S --needed git make flex bison pkgconf diffutils mingw-w64-ucrt-x86_64-gcc
goto :stop

:error_dtc
echo [ERRO] Falha ao compilar o DTC oficial.
goto :stop

:error_tests
echo [ERRO] Falha no build nativo ou em um teste de regressao.
goto :stop

:error_missing
echo [ERRO] O executavel nao foi gerado.
goto :stop

:error_validate
echo [ERRO] O executavel gerado nao passou na validacao basica.
goto :stop

:stop
echo.
echo ================================================
echo              BUILD FALHOU
echo ================================================
echo.
echo O terminal permanecera aberto para preservar o erro.
echo.
pause
exit /b 1
