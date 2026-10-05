#!/usr/bin/env bash
set -euo pipefail

SCRIPT_PATH="${BASH_SOURCE[0]}"
SCRIPT_DIR="${SCRIPT_PATH%/*}"
if [[ "$SCRIPT_DIR" != /* ]]; then
  SCRIPT_DIR="$PWD/$SCRIPT_DIR"
fi
PROJECT_DIR="${1:-${GATITO_PROJECT_DIR:-${SCRIPT_DIR%/native}}}"
DTC_DIR="${2:-/tmp/gatito-dtb-pacher-dtc}"
DTC_REPO="https://github.com/dgibson/dtc.git"
DTC_COMMIT="7a1e017926004ecff5fce62d62d42ce9f3e00082"

export PATH="/ucrt64/bin:/usr/bin:${PATH:-}"

rm -rf "$DTC_DIR"
mkdir -p "$DTC_DIR"

echo "[DTC] Fetching pinned upstream commit $DTC_COMMIT"
git init -q "$DTC_DIR"
git -C "$DTC_DIR" remote add origin "$DTC_REPO"
git -C "$DTC_DIR" fetch --quiet --depth 1 origin "$DTC_COMMIT"
git -C "$DTC_DIR" checkout --quiet --detach "$DTC_COMMIT"

cp "$PROJECT_DIR/native/dtc_bridge.h" "$DTC_DIR/dtc_bridge.h"

# The bridge needs access to the upstream private full-path helper. The source
# is pinned above, so this deterministic mechanical adaptation is reproducible.
sed -i \
  -e 's/fill_fullpaths/dtbp_fill_fullpaths/g' \
  -e 's/^static void dtbp_fill_fullpaths/void dtbp_fill_fullpaths/' \
  "$DTC_DIR/dtc.c"

printf '%s\n' '#define DTC_VERSION "DTC native 7a1e0179"' > "$DTC_DIR/version_gen.h"

cd "$DTC_DIR"
bison -d -o dtc-parser.tab.c dtc-parser.y
flex -o dtc-lexer.lex.c dtc-lexer.l

core_sources=(
  checks.c data.c flattree.c fstree.c livetree.c srcpos.c treesource.c util.c
)
for src in "${core_sources[@]}"; do
  obj="${src%.c}.o"
  gcc -DNO_YAML -Dexit=dtbp_dtc_exit -include dtc_bridge.h \
      -I. -Ilibfdt -c "$src" -o "$obj"
done

gcc -DNO_YAML -Dexit=dtbp_dtc_exit -include dtc_bridge.h \
    -I. -Ilibfdt -c dtc-lexer.lex.c -o dtc-lexer.lex.o
gcc -DNO_YAML -Dexit=dtbp_dtc_exit -include dtc_bridge.h \
    -I. -Ilibfdt -c dtc-parser.tab.c -o dtc-parser.tab.o

libfdt_sources=(
  fdt.c fdt_ro.c fdt_wip.c fdt_sw.c fdt_rw.c fdt_strerror.c
  fdt_empty_tree.c fdt_addresses.c fdt_overlay.c fdt_check.c
)
for src in "${libfdt_sources[@]}"; do
  obj="${src%.c}.o"
  gcc -I. -Ilibfdt -c "libfdt/$src" -o "libfdt/$obj"
done

ar rcs libfdt/libfdt.a libfdt/*.o

echo "[DTC] Build complete: $DTC_DIR"
