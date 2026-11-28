#!/bin/sh
# P4a (D-003): vendors SDL_mixer 1.2 and libresample into third_party/ at pinned commits and applies the
# MUGEN-UNLEASHED patches in tools/p4a/. Run from the repo root (Termux: pkg install git curl tar).
# Safe to re-run: it replaces third_party/SDL_mixer and third_party/libresample.
set -eu
MIXER_SHA=16407659fd7a3af53171d6561fd7681593591509     # libsdl-org/SDL_mixer, branch SDL-1.2
RESAMPLE_SHA=7cb7f9c3f72d4e6774d964dc324af827192df7c3  # minorninth/libresample, master (0.1.5)
[ -f CMakeLists.txt ] && [ -d third_party ] || { echo "run from the repo root"; exit 1; }
ROOT=$(pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

curl -fsSL -o "$TMP/mixer.tgz" "https://codeload.github.com/libsdl-org/SDL_mixer/tar.gz/$MIXER_SHA"
curl -fsSL -o "$TMP/lr.tgz" "https://codeload.github.com/minorninth/libresample/tar.gz/$RESAMPLE_SHA"
mkdir "$TMP/m" "$TMP/l"
tar xzf "$TMP/mixer.tgz" -C "$TMP/m" --strip-components=1
tar xzf "$TMP/lr.tgz" -C "$TMP/l" --strip-components=1

rm -rf third_party/SDL_mixer third_party/libresample
mkdir -p third_party/SDL_mixer third_party/libresample/src third_party/libresample/include
# SDL_mixer: only what the engine's WAV/AIFF/VOC sound effects and the (codec-less) music front end need.
for f in SDL_mixer.h mixer.c music.c effects_internal.c effects_internal.h effect_position.c effect_stereoreverse.c \
         load_aiff.c load_aiff.h load_voc.c load_voc.h wavestream.c wavestream.h COPYING CHANGES; do
  cp "$TMP/m/$f" third_party/SDL_mixer/
done
# the music front end includes the headers of every back end; they are only compiled in when enabled
cp "$TMP"/m/load_*.h "$TMP"/m/music_*.h "$TMP"/m/mp3utils.h "$TMP"/m/dynamic_*.h third_party/SDL_mixer/ 2>/dev/null || true
cp "$TMP"/l/src/*.c "$TMP"/l/src/*.h third_party/libresample/src/
cp "$TMP/l/include/libresample.h" third_party/libresample/include/
cp "$TMP"/l/LICENSE-BSD.txt "$TMP"/l/LICENSE-LGPL.txt "$TMP"/l/README.md third_party/libresample/
rm -f third_party/libresample/src/configtemplate.h
cat > third_party/libresample/config.h <<'EOC'
/* hand-written config.h for the Android build (upstream generates it) */
#define HAVE_INTTYPES_H 1
EOC

cat > third_party/SDL_mixer/README-mugen.md <<EOC
# SDL_mixer (vendored for the Android port, D-003 / D-019)
Origin: https://github.com/libsdl-org/SDL_mixer, branch SDL-1.2, commit $MIXER_SHA. License: zlib (COPYING).
Kept: mixer, music front end, effects (panning), WAV/AIFF/VOC loaders, wavestream. No MP3/Ogg/MIDI/MOD back end
(the engine decodes MP3 itself with libmpg123 and refuses the other formats).
Patch (MUGEN-UNLEASHED PATCH, tools/p4a/sdl_mixer-queryspecex.patch): adds Mix_QuerySpecEx, the call of Elecbyte's
modified SDL_mixer that also reports the chunk size (third_party/sdk/def/SDL_mixer.def).
Regenerate: tools/vendor-p4a.sh.
EOC
cat > third_party/libresample/README-mugen.md <<EOC
# libresample (vendored for the Android port, D-003 / D-019)
Origin: https://github.com/minorninth/libresample, commit $RESAMPLE_SHA (0.1.5). Dual license BSD-3-Clause / LGPL-2.1+:
the BSD terms are used (LICENSE-BSD.txt). Not the library of the original: Elecbyte's libresample.dll is 0.1.3 with
interleaved multi-channel buffers, resample_init/deinit/reset. The engine-facing API is android/app/src/main/cpp/mugen_resample.c,
built on this one (symbols renamed with -Dresample_*=up_resample_* in cmake/MugenAndroid.cmake).
Patch (MUGEN-UNLEASHED PATCH, tools/p4a/libresample-share-tables.patch): the filter tables are built once per quality
and shared by all handles (upstream rebuilds them, a few ms, on every resample_open; the game opens one handle per sound).
Regenerate: tools/vendor-p4a.sh.
EOC

# the patches of the vendored libraries (paths under third_party/), applied right after the copy
for p in tools/p4a/sdl_mixer-queryspecex.patch tools/p4a/libresample-share-tables.patch; do
  git apply --whitespace=nowarn "$p"
done
echo "P4a vendoring done."

# the engine side (cmake/MugenAndroid.cmake, mugen_d003_stubs.c, the new mugen_resample.c); skipped when already applied
if git apply --check --whitespace=nowarn tools/p4a/mugen-integration.patch 2>/dev/null; then
  git apply --whitespace=nowarn tools/p4a/mugen-integration.patch
  echo "integration patch applied."
elif git apply -R --check --whitespace=nowarn tools/p4a/mugen-integration.patch 2>/dev/null; then
  echo "integration patch already applied."
else
  echo "integration patch does NOT apply (files changed since?): send this message to the assistant."; exit 1
fi

# ---- docs: merge the P4a LOG entry, D-019 and the README row (idempotent) -------------------------------------------
D=docs/android-port
if ! grep -q "^## D-019" "$D/03-decisoes.md"; then
  cat tools/p4a/D-019.md >> "$D/03-decisoes.md"
  sed -i '/^## D-003 /a **ATUALIZAÇÃO (2026-11-27): a decisão final de fonte/versão está em D-019.**' "$D/03-decisoes.md"
fi
if ! grep -q "P4a — SDL_mixer 1.2" "$D/LOG.md"; then
  { head -n 4 "$D/LOG.md"; cat tools/p4a/LOG-entry.md; echo; tail -n +5 "$D/LOG.md"; } > "$TMP/LOG.new"
  cp "$TMP/LOG.new" "$D/LOG.md"
fi
if grep -q '^| P4 | Áudio e BGM | não iniciada | — | — | |$' "$D/README.md"; then
  sed -i 's/^| P4 | Áudio e BGM | não iniciada | — | — | |$/| P4 | Áudio e BGM | EM ANDAMENTO (P4a: SDL_mixer + libresample no código, validado no host; P4b mpg123\/BGM e P4c TTF\/zoomSurface pendentes) | `android\/p3b-sdl` | conversa P3b\/P4 #3 | D-019; rodar `tools\/vendor-p4a.sh` |/' "$D/README.md"
fi
echo "docs merged."
