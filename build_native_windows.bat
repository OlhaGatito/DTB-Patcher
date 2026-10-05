@echo off
setlocal EnableExtensions

title Gatito Dtb Pacher - Native Windows Build

echo.
echo ================================================
echo          Gatito Dtb Pacher - Native C++ Build
echo ================================================
echo.

cd /d "%~dp0"
if errorlevel 1 goto :error_cd

set "PROJECT_DIR=%CD%"
set "MSYS2_ROOT="

echo [INFO] Projeto: %PROJECT_DIR%
echo [INFO] Procurando uma instalacao valida do MSYS2...
echo.

if defined MSYS2_ROOT_ENV if exist "%MSYS2_ROOT_ENV%\usr\bin\bash.exe" set "MSYS2_ROOT=%MSYS2_ROOT_ENV%"

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
echo [INFO] Diagnostico das ferramentas MSYS2:
echo.

"%BASH%" -lc "export PATH=/ucrt64/bin:/usr/bin; printf '  git        '; command -v git || echo FALTANDO; printf '  make       '; command -v make || echo FALTANDO; printf '  flex       '; command -v flex || echo FALTANDO; printf '  bison      '; command -v bison || echo FALTANDO; printf '  pkg-config '; command -v pkg-config || echo FALTANDO; printf '  cmp        '; command -v cmp || echo FALTANDO; printf '  gcc        '; command -v gcc || echo FALTANDO; printf '  g++        '; command -v g++ || echo FALTANDO"
if errorlevel 1 goto :error_deps

echo.
echo [INFO] Verificando pacotes MSYS2 instalados:
echo.

"%BASH%" -lc "pacman -Q git make flex bison pkgconf diffutils mingw-w64-ucrt-x86_64-gcc 2>&1"
if errorlevel 1 (
    echo.
    echo [AVISO] Um ou mais pacotes acima nao estao instalados.
    echo [INFO] O script nao vai instalar nada automaticamente.
    echo [INFO] Para instalar manualmente, use o MSYS2 UCRT64:
    echo        pacman -S --needed git make flex bison pkgconf diffutils mingw-w64-ucrt-x86_64-gcc
    goto :error_deps
)

echo.
echo [OK] Ferramentas e pacotes necessarios encontrados.
echo.
echo [1/3] Baixando e compilando o DTC oficial...
echo        Isso pode levar alguns minutos.
echo.

"%BASH%" -lc "export PATH=/ucrt64/bin:/usr/bin; cd \"$(cygpath -u '%PROJECT_DIR%')\" && rm -rf /tmp/gatito-dtb-pacher-dtc && git clone --depth 1 https://github.com/dgibson/dtc.git /tmp/gatito-dtb-pacher-dtc && cd /tmp/gatito-dtb-pacher-dtc && make NO_YAML=1 NO_PYTHON=1 dtc libfdt"
if errorlevel 1 goto :error_dtc

echo.
echo [OK] DTC compilado.
echo.
echo [2/3] Compilando o Gatito Dtb Pacher...
echo.

"%BASH%" -lc "export PATH=/ucrt64/bin:/usr/bin; cd \"$(cygpath -u '%PROJECT_DIR%')\" && rm -rf build && mkdir -p build && gcc -Dmain=dtc_internal_main -DNO_YAML -I/tmp/gatito-dtb-pacher-dtc -I/tmp/gatito-dtb-pacher-dtc/libfdt -c /tmp/gatito-dtb-pacher-dtc/dtc.c -o build/dtc_embedded.o && gcc -DNO_YAML -I/tmp/gatito-dtb-pacher-dtc -I/tmp/gatito-dtb-pacher-dtc/libfdt -c native/dtc_bridge.c -o build/dtc_bridge.o && g++ -std=c++17 -O2 -mwindows -I/tmp/gatito-dtb-pacher-dtc -I/tmp/gatito-dtb-pacher-dtc/libfdt native/main.cpp native/dtb_model.cpp build/dtc_bridge.o build/dtc_embedded.o /tmp/gatito-dtb-pacher-dtc/checks.o /tmp/gatito-dtb-pacher-dtc/data.o /tmp/gatito-dtb-pacher-dtc/flattree.o /tmp/gatito-dtb-pacher-dtc/fstree.o /tmp/gatito-dtb-pacher-dtc/livetree.o /tmp/gatito-dtb-pacher-dtc/srcpos.o /tmp/gatito-dtb-pacher-dtc/treesource.o /tmp/gatito-dtb-pacher-dtc/util.o /tmp/gatito-dtb-pacher-dtc/dtc-lexer.lex.o /tmp/gatito-dtb-pacher-dtc/dtc-parser.tab.o /tmp/gatito-dtb-pacher-dtc/libfdt/libfdt.a -static -static-libgcc -static-libstdc++ -lgdiplus -lcomctl32 -o build/Gatito Dtb Pacher.exe"
if errorlevel 1 goto :error_compile

if not exist "build\Gatito Dtb Pacher.exe" goto :error_missing

echo.
echo [3/3] Validando o executavel...
echo.
"%BASH%" -lc "export PATH=/ucrt64/bin:/usr/bin; cd \"$(cygpath -u '%PROJECT_DIR%')\" && test -s build/Gatito Dtb Pacher.exe"
if errorlevel 1 goto :error_validate

echo [OK] Gatito Dtb Pacher.exe encontrado e nao esta vazio.
echo.
echo ================================================
echo              BUILD CONCLUIDO
echo ================================================
echo.
echo Executavel:
echo %PROJECT_DIR%\build\Gatito Dtb Pacher.exe
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
echo [ERRO] O MSYS2 foi encontrado, mas falta pelo menos uma dependencia.
echo.
echo Instale no MSYS2 com:
echo pacman -S --needed git make flex bison pkgconf diffutils mingw-w64-ucrt-x86_64-gcc
goto :stop

:error_dtc
echo.
echo [ERRO] Falha ao compilar o DTC.
goto :stop

:error_compile
echo.
echo [ERRO] Falha ao compilar o Gatito Dtb Pacher.
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
echo O terminal permanecera aberto para preservar o erro.
echo.
pause
exit /b 1
