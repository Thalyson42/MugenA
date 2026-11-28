#!/bin/sh
# P4b (D-020): vendors libmpg123 1.33.7 into third_party/mpg123 and applies the engine side (cmake + the DLL-name mapping).
# Run from the repo root (Termux: pkg install git). Safe to re-run: it replaces third_party/mpg123.
# Source: the copy of mpg123 1.33.7 that Wine keeps in libs/mpg123 (wine-mirror/wine on GitHub, pinned commit): it is the upstream
# source of the decoder library without programs, modules or network code. mpg123.org and SourceForge were not reachable from
# the environment where this was tested; the build recipe (generic C decoder, float core) is Wine's, config.h is ours.
set -eu
WINE_SHA=6a4bd25bb98dd0a840bef4a5ee8e8f4e07f3a987
[ -f CMakeLists.txt ] && [ -d third_party ] || { echo "run from the repo root"; exit 1; }
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

mkdir "$TMP/wine"
(
  cd "$TMP/wine"
  git init -q
  git remote add origin https://github.com/wine-mirror/wine.git
  git sparse-checkout set libs/mpg123
  git fetch -q --depth 1 --filter=blob:none origin "$WINE_SHA"
  git checkout -q FETCH_HEAD
)
[ -f "$TMP/wine/libs/mpg123/src/libmpg123/libmpg123.c" ] || { echo "download failed"; exit 1; }

rm -rf third_party/mpg123
mkdir -p third_party/mpg123
cp -r "$TMP/wine/libs/mpg123/src" third_party/mpg123/src
cp "$TMP/wine/libs/mpg123/LICENSE" "$TMP/wine/libs/mpg123/AUTHORS" third_party/mpg123/
cat > third_party/mpg123/config.h <<'EOC'
/* hand-written config.h of the Android build of libmpg123 (generic C decoder, float core, no network/modules) */
#define ACCURATE_ROUNDING 1
#define FIFO 1
#define FRAME_INDEX 1
#define GAPLESS 1
#define HAVE_ATOLL 1
#define HAVE_DIRENT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIBM 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRERROR 1
#define HAVE_STRING_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define IEEE_FLOAT 1
#define INDEX_SIZE 1000
#define PACKAGE "mpg123"
#define PACKAGE_NAME "mpg123"
#define PACKAGE_STRING "mpg123 1.33.7"
#define PACKAGE_VERSION "1.33.7"
#define SIZEOF_OFF_T 8
#define STDC_HEADERS 1
#define USE_NEW_HUFFTABLE 1
#define VERSION "1.33.7"
EOC
cat > third_party/mpg123/README-mugen.md <<EOC
# libmpg123 1.33.7 (vendored for the Android port, D-003 / D-019 / D-020)
Origin: libs/mpg123 of https://github.com/wine-mirror/wine, commit $WINE_SHA (upstream mpg123 1.33.7, decoder library only).
License: LGPL-2.1 (LICENSE, AUTHORS). It is built as the SHARED library libmpg123.so (cmake/MugenAndroid.cmake) and loaded at
run time by the engine's BGM plugin through the name "libmpg123-0.dll" (Mugen_LoadLibraryA), so the user can replace it (D-006).
config.h is ours; the compile definitions are in cmake/MugenAndroid.cmake. Regenerate: tools/vendor-p4b.sh.
EOC
echo "P4b vendoring done."

# the engine side (cmake/MugenAndroid.cmake, platform_posix.[ch]); skipped when already applied
if git apply --check --whitespace=nowarn tools/p4b/mugen-integration.patch 2>/dev/null; then
  git apply --whitespace=nowarn tools/p4b/mugen-integration.patch
  echo "integration patch applied."
elif git apply -R --check --whitespace=nowarn tools/p4b/mugen-integration.patch 2>/dev/null; then
  echo "integration patch already applied."
else
  echo "integration patch does NOT apply (files changed since?): send this message to the assistant."; exit 1
fi

# ---- docs: merge the P4b LOG entry and D-020 (idempotent) ------------------------------------------------------------
D=docs/android-port
if ! grep -q "^## D-020" "$D/03-decisoes.md"; then
  cat tools/p4b/D-020.md >> "$D/03-decisoes.md"
fi
if ! grep -q "P4b — libmpg123" "$D/LOG.md"; then
  { head -n 4 "$D/LOG.md"; cat tools/p4b/LOG-entry.md; echo; tail -n +5 "$D/LOG.md"; } > "$TMP/LOG.new"
  cp "$TMP/LOG.new" "$D/LOG.md"
fi
if grep -q '^| P4 | Áudio e BGM | EM ANDAMENTO (P4a: SDL_mixer + libresample no código, validado no host; P4b mpg123/BGM e P4c TTF/zoomSurface pendentes)' "$D/README.md"; then
  sed -i 's|P4a: SDL_mixer + libresample no código, validado no host; P4b mpg123/BGM e P4c TTF/zoomSurface pendentes|P4a SDL_mixer + libresample: som de efeitos OK no A065M; P4b libmpg123/BGM: código pronto, falta testar; P4c TTF/zoomSurface pendente|' "$D/README.md"
fi
echo "docs merged."
