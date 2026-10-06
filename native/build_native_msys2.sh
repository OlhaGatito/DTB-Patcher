#!/usr/bin/env bash
set -euo pipefail

SCRIPT_PATH="${BASH_SOURCE[0]}"
SCRIPT_DIR="${SCRIPT_PATH%/*}"
if [[ "$SCRIPT_DIR" != /* ]]; then
  SCRIPT_DIR="$PWD/$SCRIPT_DIR"
fi
PROJECT_DIR="${1:-${GATITO_PROJECT_DIR:-${SCRIPT_DIR%/native}}}"
DTC_DIR="${2:-/tmp/gatito-dtb-patcher-dtc}"
export PATH="/ucrt64/bin:/usr/bin:${PATH:-}"

cd "$PROJECT_DIR"
rm -rf build
mkdir -p build

echo "[BUILD] Compiling embedded DTC"
gcc -Dmain=dtc_internal_main -DNO_YAML -Dexit=dtbp_dtc_exit \
    -include "$DTC_DIR/dtc_bridge.h" -I"$DTC_DIR" -I"$DTC_DIR/libfdt" \
    -c "$DTC_DIR/dtc.c" -o build/dtc_embedded.o

gcc -DNO_YAML -I"$DTC_DIR" -I"$DTC_DIR/libfdt" \
    -c native/dtc_bridge.c -o build/dtc_bridge.o

echo "[BUILD] Compiling native GUI"
g++ -std=c++17 -O2 -mwindows \
    -I"$DTC_DIR" -I"$DTC_DIR/libfdt" \
    native/main.cpp native/dtb_model.cpp \
    build/dtc_bridge.o build/dtc_embedded.o \
    "$DTC_DIR/checks.o" "$DTC_DIR/data.o" "$DTC_DIR/flattree.o" \
    "$DTC_DIR/fstree.o" "$DTC_DIR/livetree.o" "$DTC_DIR/srcpos.o" \
    "$DTC_DIR/treesource.o" "$DTC_DIR/util.o" \
    "$DTC_DIR/dtc-lexer.lex.o" "$DTC_DIR/dtc-parser.tab.o" \
    "$DTC_DIR/libfdt/libfdt.a" \
    -static -static-libgcc -static-libstdc++ \
    -lgdiplus -lcomctl32 -o build/Gatito-DTB-Patcher.exe

echo "[TEST] Native DTC smoke test"
g++ -std=c++17 -O2 -I"$DTC_DIR" -I"$DTC_DIR/libfdt" \
    native/test_dtc.cpp build/dtc_bridge.o build/dtc_embedded.o \
    "$DTC_DIR/checks.o" "$DTC_DIR/data.o" "$DTC_DIR/flattree.o" \
    "$DTC_DIR/fstree.o" "$DTC_DIR/livetree.o" "$DTC_DIR/srcpos.o" \
    "$DTC_DIR/treesource.o" "$DTC_DIR/util.o" \
    "$DTC_DIR/dtc-lexer.lex.o" "$DTC_DIR/dtc-parser.tab.o" \
    "$DTC_DIR/libfdt/libfdt.a" \
    -static-libgcc -static-libstdc++ -o build/test_dtc.exe
./build/test_dtc.exe tests/native-smoke.dts build/smoke

echo "[TEST] Semantic transfer model"
g++ -std=c++17 -O2 native/test_model.cpp native/dtb_model.cpp -o build/test_model.exe
./build/test_model.exe tests/semantic-donor.dts tests/semantic-receiver.dts

echo "[TEST] DTS parser regression"
g++ -std=c++17 -O2 native/test_parser.cpp native/dtb_model.cpp -o build/test_parser.exe
./build/test_parser.exe tests/parser-regression.dts

test -s build/Gatito-DTB-Patcher.exe
echo "[OK] Native build and all smoke tests passed."
