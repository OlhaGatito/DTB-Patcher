@echo off
setlocal EnableExtensions

title DTB-Patcher - Native Windows Build

echo.
echo ================================================
echo          DTB-Patcher - Native C++ Build
echo ================================================
echo.

cd /d "%~dp0"
if errorlevel 1 goto :error_cd

set "PROJECT_DIR=%CD%"
set "MSYS2_ROOT="

echo [INFO] Projeto: %PROJECT_DIR%
echo [INFO] Procurando uma instalacao valida do MSYS2...
echo.

if defined MSYS2_ROOT_ENV set "MSYS2_ROOT=%MSYS2_ROOT_ENV%"
if not defined MSYS2_ROOT if defined MSYS2_ROOT set "MSYS2_ROOT=%MSYS2_ROOT%"

if not defined MSYS2_ROOT (
    for %%P in (
        "C:\msys64"
        "%ProgramFiles%\msys64"
        "%ProgramFiles(x86)%\msys64"
        "%LOCALAPPDATA%\msys64"
        "%USERPROFILE%\scoop\apps\msys2\current"
    ) do (
        if not defined MSYS2_ROOT if exist "%%~P\usr\bin\bash.exe" if exist "%%~P\usr\bin\pacman.exe" set "MSYS2_ROOT=%%~P"
    )
)

if not defined MSYS2_ROOT goto :error_msys2

set "BASH=%MSYS2_ROOT%\usr\bin\bash.exe"
echo [OK] MSYS2 encontrado: %BASH%
echo.
echo [INFO] Verificando ambiente UCRT64 e dependencias...
echo.

"%BASH%" -lc "export PATH=/ucrt64/bin:/usr/bin; missing=''; for c in git make flex bison pkg-config cmp gcc g++; do command -v $c >/dev/null 2>&1 || missing="$missing $c"; done; if [ -n "$missing" ]; then echo "[ERRO] Dependencias ausentes:$missing"; echo; echo "Instale no MSYS2 com:"; echo "pacman -S --needed git make flex bison pkgconf diffutils mingw-w64-ucrt-x86_64-gcc"; exit 1; fi; echo '[OK] Dependencias encontradas.'"
if errorlevel 1 goto :error_deps

echo.
echo [1/3] Baixando e compilando o DTC oficial...
echo        Isso pode levar alguns minutos.
echo.

"%BASH%" -lc "export PATH=/ucrt64/bin:/usr/bin; cd "$(cygpath -u '%PROJECT_DIR%')" && rm -rf /tmp/dtb-patcher-dtc && git clone --depth 1 https://github.com/dgibson/dtc.git /tmp/dtb-patcher-dtc && cd /tmp/dtb-patcher-dtc && make NO_YAML=1 NO_PYTHON=1 dtc libfdt"
if errorlevel 1 goto :error_dtc

echo.
echo [OK] DTC compilado.
echo.
echo [2/3] Compilando o DTB-Patcher...
echo.

"%BASH%" -lc "export PATH=/ucrt64/bin:/usr/bin; cd "$(cygpath -u '%PROJECT_DIR%')" && rm -rf build && mkdir -p build && gcc -Dmain=dtc_internal_main -DNO_YAML -I/tmp/dtb-patcher-dtc -I/tmp/dtb-patcher-dtc/libfdt -c /tmp/dtb-patcher-dtc/dtc.c -o build/dtc_embedded.o && gcc -DNO_YAML -I/tmp/dtb-patcher-dtc -I/tmp/dtb-patcher-dtc/libfdt -c native/dtc_bridge.c -o build/dtc_bridge.o && g++ -std=c++17 -O2 -mwindows -I/tmp/dtb-patcher-dtc -I/tmp/dtb-patcher-dtc/libfdt native/main.cpp native/dtb_model.cpp build/dtc_bridge.o build/dtc_embedded.o /tmp/dtb-patcher-dtc/checks.o /tmp/dtb-patcher-dtc/data.o /tmp/dtb-patcher-dtc/flattree.o /tmp/dtb-patcher-dtc/fstree.o /tmp/dtb-patcher-dtc/livetree.o /tmp/dtb-patcher-dtc/srcpos.o /tmp/dtb-patcher-dtc/treesource.o /tmp/dtb-patcher-dtc/util.o /tmp/dtb-patcher-dtc/dtc-lexer.lex.o /tmp/dtb-patcher-dtc/dtc-parser.tab.o /tmp/dtb-patcher-dtc/libfdt/libfdt.a -static-libgcc -static-libstdc++ -lgdiplus -lcomctl32 -o build/DTB-Patcher.exe"
if errorlevel 1 goto :error_compile

if not exist "build\DTB-Patcher.exe" goto :error_missing

echo.
echo [3/3] Validando o executavel...
echo.
"%BASH%" -lc "export PATH=/ucrt64/bin:/usr/bin; cd "$(cygpath -u '%PROJECT_DIR%')" && test -s build/DTB-Patcher.exe"
if errorlevel 1 goto :error_validate

echo [OK] DTB-Patcher.exe encontrado e nao esta vazio.
echo.
echo ================================================
echo              BUILD CONCLUIDO
echo ================================================
echo.
echo Executavel:
echo %PROJECT_DIR%\build\DTB-Patcher.exe
echo.
pause
exit /b 0

:error_cd
echo [ERRO] Nao foi possivel entrar na pasta do projeto.
goto :stop

:error_msys2
echo [ERRO] Nenhuma instalacao valida do MSYS2 foi encontrada.
echo.
echo Defina MSYS2_ROOT_ENV se estiver em um caminho personalizado.
goto :stop

:error_deps
echo.
echo [ERRO] O MSYS2 foi encontrado, mas faltam dependencias.
goto :stop

:error_dtc
echo.
echo [ERRO] Falha ao compilar o DTC.
goto :stop

:error_compile
echo.
echo [ERRO] Falha ao compilar o DTB-Patcher.
goto :stop

:error_missing
echo.
echo [ERRO] O executavel nao foi gerado.
goto :stop

:error_validate
echo.
echo [ERRO] A validacao do executavel falhou.
goto :stop

:stop
echo.
echo ================================================
echo              BUILD FALHOU
echo ================================================
echo.
pause
exit /b 1
