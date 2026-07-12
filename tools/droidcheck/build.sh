#!/bin/bash
# Build "droidcheck" — a headless runner for the DROID Forge patch problem
# checker. It reuses the Forge's own parser + Patch::updateProblems() so results
# match the GUI's problem panel. GUI/modules code is not linked; a few symbols
# the checker references are provided by the *_stub.cpp files here.
#
# Requirements: Qt 6 (`brew install qt`), a C++17 clang, git.
# Usage:  ./build.sh   then   ./build/droidcheck ../../patches/*.ini
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="$HERE/build"
VENDOR="$HERE/vendor/droidforge"
SRC="$VENDOR/droidforge"

# --- locate Qt6 ------------------------------------------------------------
QT="${QT:-$(brew --prefix qt 2>/dev/null || echo /opt/homebrew/opt/qt)}"
RCC="$QT/share/qt/libexec/rcc"
MOC="$QT/share/qt/libexec/moc"
[ -x "$RCC" ] || { echo "ERROR: rcc not found under $QT (install Qt: brew install qt)"; exit 1; }

# --- fetch the Forge source (GPLv3) if missing -----------------------------
if [ ! -d "$SRC" ]; then
    echo "Cloning DROID Forge source (Zarkuun/droidforge)..."
    git clone --depth 1 https://github.com/Zarkuun/droidforge.git "$VENDOR"
fi

mkdir -p "$BUILD"
cd "$BUILD"

# 1) Embed droidfirmware.json as a Qt resource (:droidfirmware.json).
#    rcc resolves <file> paths relative to the .qrc, so keep both in $BUILD.
cp "$SRC/droidfirmware.json" "$BUILD/droidfirmware.json"
cp "$HERE/check_resources.qrc" "$BUILD/check_resources.qrc"
"$RCC" -o resources_gen.cpp "$BUILD/check_resources.qrc"

# 1b) moc the Clipboard QObject (its body is stubbed in clipboard_stub.cpp)
"$MOC" -I"$SRC/patch" -I"$SRC/patchview" -I"$SRC/main" \
    "$SRC/main/clipboard.h" -o moc_clipboard.cpp

# 2) Forge source files the checker needs (no GUI / no modules).
FORGE_SRCS=(
  "$SRC"/parser/patchparser.cpp
  "$SRC"/parser/generalparseexception.cpp
  "$SRC"/parser/statusdump.cpp
  "$SRC"/utilities/imagecache.cpp
  "$SRC"/utilities/utilities.cpp
  "$SRC"/main/droidfirmware.cpp
  "$SRC"/main/registerlabels.cpp
  "$SRC"/patchview/cursorposition.cpp
  "$SRC"/patchview/selection.cpp
)
# All patch-model files EXCEPT patcheditengine.cpp (GUI edit layer).
for f in "$SRC"/patch/*.cpp; do
  case "$f" in
    *patcheditengine.cpp) ;;   # skip
    *) FORGE_SRCS+=("$f") ;;
  esac
done

# 3) Include paths + Qt framework headers.
INCS=(
  -I"$SRC" -I"$SRC"/parser -I"$SRC"/patch -I"$SRC"/patchview
  -I"$SRC"/modules -I"$SRC"/main -I"$SRC"/utilities -I"$SRC"/pg
  -F"$QT/lib"
  -I"$QT/lib/QtCore.framework/Headers"
  -I"$QT/lib/QtGui.framework/Headers"
  -I"$QT/lib/QtWidgets.framework/Headers"
)

clang++ -std=c++17 -O1 -w \
  "${INCS[@]}" \
  "$HERE"/droidcheck.cpp \
  "$HERE"/modulebuilder_stub.cpp \
  "$HERE"/clipboard_stub.cpp \
  "$BUILD"/moc_clipboard.cpp \
  "$BUILD"/resources_gen.cpp \
  "${FORGE_SRCS[@]}" \
  -F"$QT/lib" -framework QtCore -framework QtGui -framework QtWidgets \
  -o "$BUILD/droidcheck"

echo "BUILD OK -> $BUILD/droidcheck"
echo "Try:      $BUILD/droidcheck $HERE/../../patches/*.ini"
