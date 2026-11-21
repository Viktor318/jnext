#!/usr/bin/env bash
#
# build-windows-cross.sh: build the Hungarian JNEXT (hungarian-ui) for 64-bit
# Windows from Linux or WSL, with MinGW-w64 and the Qt Company's MinGW build of
# Qt 6.8.3, and package it as a ready-to-run folder and a zip.
#
# A JNEXT magyar változatának (hungarian-ui) Windows-fordítása Linuxról vagy
# WSL-ből, MinGW-w64-gyel és a Qt 6.8.3 hivatalos MinGW-s változatával; a
# kimenet egy futtatható mappa és egy zip.
#
# Usage / Használat (Ubuntu 24.04 or newer, in WSL or native; tried on 24.04 and 26.04):
#   git clone -b hungarian-ui https://github.com/Viktor318/jnext ~/jnext
#   ~/jnext/tools/hu/build-windows-cross.sh [nopull] [clean]
#
#   nopull   do not run `git pull --ff-only` first / nincs git pull
#   clean    rebuild jnext from scratch (the toolchain stays) / tiszta fordítás
#
# The first run sets up everything below $JNEXT_WIN_ENV (default
# ~/jnext-win-env): the apt packages (asks for the sudo password), Qt 6.8.3 for
# Windows and its Linux host tools (aqtinstall, from master.qt.io), and zlib,
# libpng and SDL3 built from source. Every step is skipped once done, so an
# interrupted run can simply be started again. Later runs only pull and build.
# Az első futás mindent beállít ($JNEXT_WIN_ENV, alapból ~/jnext-win-env);
# a kész lépéseket kihagyja, megszakítás után újra elindítható.
#
# Result / Eredmény, in $JNEXT_WIN_OUT (default C:\Dev\jnext-bin when /mnt/c/Dev
# exists, otherwise ~/jnext-windows):
#   jnext\                                   the program, run jnext.exe here
#   jnext-<version>-hu-windows-x64-<hash>.zip   the same folder, zipped
#
# The build works from this clone, which should live on the Linux side (~/jnext):
# under /mnt/c the build is many times slower. It never writes into the clone
# beyond git pull; the build tree is $JNEXT_WIN_ENV/build.
# A fordítás a Linux oldali klónból megy (~/jnext), mert a /mnt/c alatt sokkal
# lassabb; a klónba csak a git pull ír.
#
# JNEXT_WERROR is off: a newer MinGW than upstream's CI may warn where theirs
# does not, and a warning must not stop a user's build. The test gates of the
# repository keep -Werror.
#
# Never put a NextZXOS SD-card image into the output: jnext downloads it itself,
# on the Windows machine, into %USERPROFILE%\.jnext\sdcard.
#
set -euo pipefail

QT_VERSION=6.8.3
ZLIB_TAG=v1.3.1
LIBPNG_TAG=v1.6.50
SDL_REF=release-3.2.x
MIN_MINGW_GCC=13   # Qt 6.8's MinGW binaries are built with GCC 13.1

ENV_DIR=${JNEXT_WIN_ENV:-$HOME/jnext-win-env}
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
JOBS=${JNEXT_WIN_JOBS:-$(nproc)}
if [ -n "${JNEXT_WIN_OUT:-}" ]; then OUT=$JNEXT_WIN_OUT
elif [ -d /mnt/c/Dev ]; then OUT=/mnt/c/Dev/jnext-bin
else OUT=$HOME/jnext-windows; fi

PULL=1; CLEAN=0
for a in "$@"; do
    case "$a" in
        nopull) PULL=0 ;;
        clean)  CLEAN=1 ;;
        -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d'; exit 0 ;;
        *) echo "unknown option: $a (nopull, clean)" >&2; exit 2 ;;
    esac
done

QT_DIR=$ENV_DIR/qt
QT_WIN=$QT_DIR/$QT_VERSION/mingw_64
QT_HOST=$QT_DIR/$QT_VERSION/gcc_64
SYSROOT=$ENV_DIR/winsys          # zlib, libpng, SDL3 for Windows
SRC=$ENV_DIR/src
TOOLCHAIN=$ENV_DIR/mingw64-toolchain.cmake
BUILD=$ENV_DIR/build
BUNDLE_ROOT=$ENV_DIR/bundle-sysroot
STAGE=$ENV_DIR/stage

say()  { printf '\n\033[1m== %s\033[0m\n' "$*"; }
die()  { printf '\nHIBA / ERROR: %s\n' "$*" >&2; exit 1; }

case "$REPO" in /mnt/[a-z]/*)
    echo "Note: this clone is under ${REPO%%/[!/]*}/..., the build will be slow." \
         "A ~/jnext clone is recommended. / A klón a Windows-meghajtón van, a fordítás lassú lesz." ;;
esac
mkdir -p "$ENV_DIR" "$SRC"

# ── 1. System packages ───────────────────────────────────────────────────────
PKGS=(g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64
      cmake ninja-build ccache git zip python3-venv python3-pip pkg-config)
missing=()
for p in "${PKGS[@]}"; do dpkg -s "$p" >/dev/null 2>&1 || missing+=("$p"); done
if [ ${#missing[@]} -gt 0 ]; then
    say "apt: ${missing[*]}"
    sudo apt-get update
    sudo apt-get install -y "${missing[@]}"
fi
CXX_W=x86_64-w64-mingw32-g++-posix
gccv=$($CXX_W -dumpversion 2>/dev/null || echo 0)
[ "${gccv%%.*}" -ge "$MIN_MINGW_GCC" ] || die "MinGW GCC $gccv is too old (Qt $QT_VERSION needs $MIN_MINGW_GCC+). Use Ubuntu 24.04 or newer. / A MinGW túl régi: Ubuntu 24.04 vagy újabb kell."

# ── 2. Qt for Windows (+ the Linux host tools: moc, rcc, uic) ─────────────────
if [ ! -f "$QT_WIN/lib/cmake/Qt6/Qt6Config.cmake" ] || [ ! -x "$QT_HOST/libexec/moc" ]; then
    say "Qt $QT_VERSION (aqtinstall, master.qt.io)"
    [ -x "$ENV_DIR/venv/bin/aqt" ] || { python3 -m venv "$ENV_DIR/venv"; "$ENV_DIR/venv/bin/pip" install -q aqtinstall; }
    "$ENV_DIR/venv/bin/aqt" install-qt windows desktop "$QT_VERSION" win64_mingw \
        -m qtimageformats --base https://master.qt.io/ -O "$QT_DIR"
    "$ENV_DIR/venv/bin/aqt" install-qt linux desktop "$QT_VERSION" linux_gcc_64 \
        --base https://master.qt.io/ -O "$QT_DIR"
fi

# ── 3. CMake toolchain file ──────────────────────────────────────────────────
cat > "$TOOLCHAIN.new" <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 $SYSROOT $QT_WIN)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
if cmp -s "$TOOLCHAIN.new" "$TOOLCHAIN"; then rm "$TOOLCHAIN.new"; else mv "$TOOLCHAIN.new" "$TOOLCHAIN"; fi

# ── 4. zlib, libpng, SDL3 from source ────────────────────────────────────────
dep() {   # dep <name> <git url> <ref> <marker file under $SYSROOT> [cmake options...]
    local name=$1 url=$2 ref=$3 marker=$4; shift 4
    [ -f "$SYSROOT/$marker" ] && return 0
    say "$name ($ref)"
    if [ ! -d "$SRC/$name/.git" ]; then
        git clone -q --depth 1 --branch "$ref" "$url" "$SRC/$name"
    fi
    cmake -S "$SRC/$name" -B "$SRC/build-$name" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$SYSROOT" -DCMAKE_PREFIX_PATH="$SYSROOT" "$@" >/dev/null
    cmake --build "$SRC/build-$name" -j"$JOBS"
    cmake --install "$SRC/build-$name" >/dev/null
}
dep zlib   https://github.com/madler/zlib.git    "$ZLIB_TAG"   bin/libzlib.dll    -DZLIB_BUILD_EXAMPLES=OFF
dep libpng https://github.com/pnggroup/libpng.git "$LIBPNG_TAG" bin/libpng16.dll   -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_EXECUTABLES=OFF -DPNG_STATIC=OFF -DPNG_SHARED=ON
dep sdl    https://github.com/libsdl-org/SDL.git  "$SDL_REF"    bin/SDL3.dll       -DSDL_TESTS=OFF -DSDL_STATIC=OFF -DSDL_SHARED=ON

# ── 5. The source ────────────────────────────────────────────────────────────
if [ "$PULL" -eq 1 ]; then
    say "git pull --ff-only ($REPO)"
    [ -z "$(git -C "$REPO" status --porcelain --untracked-files=no)" ] \
        || die "the clone has local changes; commit or stash them, or run with nopull. / Helyi módosítás van a klónban."
    git -C "$REPO" pull --ff-only
fi
HASH=$(git -C "$REPO" rev-parse --short=7 HEAD)
VERSION=$(sed -n 's/^version:[[:space:]]*//p' "$REPO/version.yaml" | head -n1)
[ -n "$VERSION" ] || die "no version in version.yaml"
NAME=jnext-$VERSION-hu-windows-x64-$HASH

# ── 6. Build jnext.exe ───────────────────────────────────────────────────────
[ "$CLEAN" -eq 1 ] && rm -rf "$BUILD"
if [ ! -f "$BUILD/CMakeCache.txt" ] || ! grep -q "CMAKE_HOME_DIRECTORY:INTERNAL=$REPO\$" "$BUILD/CMakeCache.txt"; then
    rm -rf "$BUILD"
    say "cmake configure"
    cmake -S "$REPO" -B "$BUILD" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
        -DQT_HOST_PATH="$QT_HOST" -DCMAKE_PREFIX_PATH="$QT_WIN;$SYSROOT" \
        -DENABLE_QT_UI=ON -DENABLE_TESTS=OFF -DJNEXT_WERROR=OFF
fi
say "build ($JOBS jobs) - $NAME"
cmake --build "$BUILD" --target jnext -j"$JOBS"

# ── 7. Bundle: the exe, its DLLs and Qt plugins, docs, licences ──────────────
say "bundle"
# packaging/windows/bundle-dlls.sh resolves DLLs from ONE sysroot bin directory;
# here they come from three places, so link them into one. The compiler's own
# runtime (libstdc++, libgcc_s, libwinpthread) overrides Qt's copies: the exe is
# linked against the compiler's.
rm -rf "$BUNDLE_ROOT"; mkdir -p "$BUNDLE_ROOT/bin" "$BUNDLE_ROOT/lib/qt6"
for f in "$QT_WIN"/bin/*.dll "$SYSROOT"/bin/*.dll; do ln -sf "$f" "$BUNDLE_ROOT/bin/"; done
for d in libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
    p=$($CXX_W -print-file-name="$d")
    [ -f "$p" ] || p=/usr/x86_64-w64-mingw32/lib/$d
    [ -f "$p" ] || die "runtime DLL not found: $d"
    ln -sf "$p" "$BUNDLE_ROOT/bin/$d"
done
ln -sfn "$QT_WIN/plugins" "$BUNDLE_ROOT/lib/qt6/plugins"
# The repository script, without its licence step: that one reads Fedora's rpm
# database. The licences are collected below instead.
BUNDLER=$ENV_DIR/bundle-dlls-nolicence.sh
awk '/^# --- licences/{skip=1} skip&&/^done$/{skip=0; next} !skip' \
    "$REPO/packaging/windows/bundle-dlls.sh" > "$BUNDLER"
rm -rf "$STAGE"; mkdir -p "$STAGE/$NAME"
D=$STAGE/$NAME
MINGW_SYSROOT=$BUNDLE_ROOT bash "$BUNDLER" "$BUILD/jnext.exe" "$D"
for f in jnext.exe Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll platforms/qwindows.dll SDL3.dll; do
    [ -f "$D/$f" ] || die "missing from the bundle: $f"
done
cp "$REPO"/{LICENSE,README.md,ChangeLog,USAGE.md} "$REPO/data/48rom.map" "$D/"
cp -r "$REPO/doc/user-guide" "$D/user-guide"
L=$D/licenses; mkdir -p "$L"/bundled/{qt6,sdl3,zlib,libpng,gcc-runtime,winpthreads}
while IFS='|' read -r n s; do
    case "$n" in ''|'#'*) continue ;; esac
    cp "$REPO/$s" "$L/$n"
done < "$REPO/packaging/licenses.list"
cp /usr/share/common-licenses/LGPL-3 "$L/bundled/qt6/LGPL-3.0.txt"
cp /usr/share/common-licenses/GPL-3  "$L/bundled/qt6/GPL-3.0.txt"
printf 'Qt %s (Qt Company official MinGW binaries, from download.qt.io), used under the LGPL-3.0.\nSource: https://download.qt.io/archive/qt/%s/%s/single/\n' \
    "$QT_VERSION" "${QT_VERSION%.*}" "$QT_VERSION" > "$L/bundled/qt6/README.txt"
cp "$SRC/sdl/LICENSE.txt"   "$L/bundled/sdl3/"
cp "$SRC/zlib/LICENSE"      "$L/bundled/zlib/"
cp "$SRC/libpng/LICENSE"    "$L/bundled/libpng/"
cp /usr/share/doc/gcc-mingw-w64-base/copyright "$L/bundled/gcc-runtime/copyright.txt"
cp /usr/share/doc/mingw-w64-common/copyright   "$L/bundled/winpthreads/copyright.txt"
# Guard: no SD-card image may ever be shipped.
if find "$D" -iname '*.img' -o -iname '*.mmc' -o -iname '*.hdf' | grep -q .; then
    die "an SD-card image is in the bundle; refusing / SD-kártyakép került a csomagba"
fi

# ── 8. Zip and copy out ──────────────────────────────────────────────────────
say "zip -> $OUT"
(cd "$STAGE" && rm -f "$NAME.zip" && zip -rq "$NAME.zip" "$NAME")
mkdir -p "$OUT"
rm -rf "$OUT/jnext.new"
cp -r "$D" "$OUT/jnext.new"
rm -rf "$OUT/jnext"
mv "$OUT/jnext.new" "$OUT/jnext"
cp "$STAGE/$NAME.zip" "$OUT/"
echo
echo "Kész / Done: $OUT/jnext/jnext.exe"
echo "             $OUT/$NAME.zip"
