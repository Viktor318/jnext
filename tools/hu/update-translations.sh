#!/usr/bin/env bash
#
# update-translations.sh [lupdate-only]
#
# Refresh the interface translation of the Hungarian fork (hungarian-ui):
#   1. lupdate collects every tr()/translate() text of src/gui and
#      src/debugger into src/gui/translations/jnext_hu.ts (new texts come in
#      as "unfinished", texts gone from the code are dropped);
#   2. lrelease compiles it into jnext_hu.qm, which the build embeds
#      (src/gui/translations/jnext_i18n.qrc).
# Both files are committed, so a build needs neither tool.
#
# The tools are Qt 6's: LUPDATE / LRELEASE override the paths, otherwise they
# are looked up on PATH and then under /usr/lib/qt6/bin.
#
# -locations none: no file names or line numbers in the .ts, so a change in
# the code that moves a text does not rewrite the whole file.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TS="$ROOT/src/gui/translations/jnext_hu.ts"
QM="$ROOT/src/gui/translations/jnext_hu.qm"

find_tool() {
    local name=$1 override=$2
    if [ -n "$override" ]; then echo "$override"; return; fi
    if command -v "$name" >/dev/null 2>&1; then command -v "$name"; return; fi
    if [ -x "/usr/lib/qt6/bin/$name" ]; then echo "/usr/lib/qt6/bin/$name"; return; fi
    echo "error: $name not found (set ${name^^}=/path/to/$name)" >&2
    exit 1
}
LUPDATE_BIN=$(find_tool lupdate "${LUPDATE:-}")
LRELEASE_BIN=$(find_tool lrelease "${LRELEASE:-}")

"$LUPDATE_BIN" -silent -locations none -no-obsolete -source-language en -target-language hu_HU \
    "$ROOT/src/gui" "$ROOT/src/debugger" -ts "$TS"
[ "${1:-}" = "lupdate-only" ] && exit 0
"$LRELEASE_BIN" -silent "$TS" -qm "$QM"
echo "updated: ${TS#"$ROOT"/} and ${QM#"$ROOT"/}"
