@echo off
setlocal

if not exist C:\msys64\usr\bin\bash.exe (
  echo MSYS2 nao encontrado em C:\msys64.
  exit /b 1
)

set "PROJECT_DIR=%CD%"
C:\msys64\usr\bin\bash.exe -lc "cd \"$(cygpath -u '%PROJECT_DIR%')\" && rm -rf /tmp/dtb-patcher-dtc && git clone --depth 1 https://github.com/dgibson/dtc.git /tmp/dtb-patcher-dtc && cd /tmp/dtb-patcher-dtc && make NO_YAML=1 NO_PYTHON=1 dtc libfdt && cd \"$(cygpath -u '%PROJECT_DIR%')\" && rm -rf build && mkdir -p build && gcc -Dmain=dtc_internal_main -DNO_YAML -I/tmp/dtb-patcher-dtc -I/tmp/dtb-patcher-dtc/libfdt -c /tmp/dtb-patcher-dtc/dtc.c -o build/dtc_embedded.o && gcc -DNO_YAML -I/tmp/dtb-patcher-dtc -I/tmp/dtb-patcher-dtc/libfdt -c native/dtc_bridge.c -o build/dtc_bridge.o && g++ -std=c++17 -O2 -mwindows -I/tmp/dtb-patcher-dtc -I/tmp/dtb-patcher-dtc/libfdt native/main.cpp native/dtb_model.cpp build/dtc_bridge.o build/dtc_embedded.o /tmp/dtb-patcher-dtc/checks.o /tmp/dtb-patcher-dtc/data.o /tmp/dtb-patcher-dtc/flattree.o /tmp/dtb-patcher-dtc/fstree.o /tmp/dtb-patcher-dtc/livetree.o /tmp/dtb-patcher-dtc/srcpos.o /tmp/dtb-patcher-dtc/treesource.o /tmp/dtb-patcher-dtc/util.o /tmp/dtb-patcher-dtc/dtc-lexer.lex.o /tmp/dtb-patcher-dtc/dtc-parser.tab.o /tmp/dtb-patcher-dtc/libfdt/libfdt.a -static-libgcc -static-libstdc++ -lgdiplus -lcomctl32 -o build/DTB-Patcher.exe"
if errorlevel 1 exit /b 1

if not exist build\DTB-Patcher.exe (
  echo ERRO: DTB-Patcher.exe nao foi gerado.
  exit /b 1
)

echo.
echo Build concluido: build\DTB-Patcher.exe
