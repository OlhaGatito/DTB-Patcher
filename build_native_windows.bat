@echo off
setlocal EnableExtensions

title DTB-Patcher - Native Windows Build

echo.
echo ================================================
echo          DTB-Patcher - Native C++ Build
echo ================================================
echo.

rem Always build from the folder where this BAT is located.
cd /d "%~dp0"
if errorlevel 1 goto :error_cd

set "PROJECT_DIR=%CD%"
set "BASH=C:\msys64\usr\bin\bash.exe"

echo [INFO] Projeto: %PROJECT_DIR%
echo [INFO] Verificando MSYS2...
echo.

if not exist "%BASH%" goto :error_msys2

echo [OK] MSYS2 encontrado: %BASH%
echo.
echo [1/3] Baixando e compilando o DTC oficial...
echo        Isso pode levar alguns minutos.
echo.

"%BASH%" -lc "cd \"$(cygpath -u '%PROJECT_DIR%')\" && rm -rf /tmp/dtb-patcher-dtc && git clone --depth 1 https://github.com/dgibson/dtc.git /tmp/dtb-patcher-dtc && cd /tmp/dtb-patcher-dtc && make NO_YAML=1 NO_PYTHON=1 dtc libfdt"
if errorlevel 1 goto :error_dtc

echo.
echo [OK] DTC compilado.
echo.
echo [2/3] Compilando o DTB-Patcher...
echo.

"%BASH%" -lc "cd \"$(cygpath -u '%PROJECT_DIR%')\" && rm -rf build && mkdir -p build && gcc -Dmain=dtc_internal_main -DNO_YAML -I/tmp/dtb-patcher-dtc -I/tmp/dtb-patcher-dtc/libfdt -c /tmp/dtb-patcher-dtc/dtc.c -o build/dtc_embedded.o && gcc -DNO_YAML -I/tmp/dtb-patcher-dtc -I/tmp/dtb-patcher-dtc/libfdt -c native/dtc_bridge.c -o build/dtc_bridge.o && g++ -std=c++17 -O2 -mwindows -I/tmp/dtb-patcher-dtc -I/tmp/dtb-patcher-dtc/libfdt native/main.cpp native/dtb_model.cpp build/dtc_bridge.o build/dtc_embedded.o /tmp/dtb-patcher-dtc/checks.o /tmp/dtb-patcher-dtc/data.o /tmp/dtb-patcher-dtc/flattree.o /tmp/dtb-patcher-dtc/fstree.o /tmp/dtb-patcher-dtc/livetree.o /tmp/dtb-patcher-dtc/srcpos.o /tmp/dtb-patcher-dtc/treesource.o /tmp/dtb-patcher-dtc/util.o /tmp/dtb-patcher-dtc/dtc-lexer.lex.o /tmp/dtb-patcher-dtc/dtc-parser.tab.o /tmp/dtb-patcher-dtc/libfdt/libfdt.a -static-libgcc -static-libstdc++ -lgdiplus -lcomctl32 -o build/DTB-Patcher.exe"
if errorlevel 1 goto :error_compile

if not exist "build\DTB-Patcher.exe" goto :error_missing

echo.
echo [3/3] Validando o executavel...
echo.

"%BASH%" -lc "cd \"$(cygpath -u '%PROJECT_DIR%')\" && test -s build/DTB-Patcher.exe"
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
echo O BAT vai permanecer aberto para voce ver o resultado.
echo.
pause
exit /b 0

:error_cd
echo [ERRO] Nao foi possivel entrar na pasta do projeto.
goto :stop

:error_msys2
echo [ERRO] MSYS2 nao foi encontrado.
echo.
echo Caminho esperado:
echo C:\msys64\usr\bin\bash.exe
echo.
echo Instale/configure o MSYS2 nesse caminho e tente novamente.
goto :stop

:error_dtc
echo.
echo [ERRO] Falha ao compilar o DTC.
echo O erro acima e o diagnostico real do MSYS2/make.
goto :stop

:error_compile
echo.
echo [ERRO] Falha ao compilar o DTB-Patcher.
echo O erro acima e o diagnostico real do compilador.
goto :stop

:error_missing
echo.
echo [ERRO] O compilador terminou, mas build\DTB-Patcher.exe nao foi gerado.
goto :stop

:error_validate
echo.
echo [ERRO] O executavel foi encontrado, mas a validacao falhou.
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
