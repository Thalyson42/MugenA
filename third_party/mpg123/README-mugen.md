# libmpg123 1.33.7 (vendored for the Android port, D-003 / D-019 / D-020)
Origin: libs/mpg123 of https://github.com/wine-mirror/wine, commit 6a4bd25bb98dd0a840bef4a5ee8e8f4e07f3a987 (upstream mpg123 1.33.7, decoder library only).
License: LGPL-2.1 (LICENSE, AUTHORS). It is built as the SHARED library libmpg123.so (cmake/MugenAndroid.cmake) and loaded at
run time by the engine's BGM plugin through the name "libmpg123-0.dll" (Mugen_LoadLibraryA), so the user can replace it (D-006).
config.h is ours; the compile definitions are in cmake/MugenAndroid.cmake. Regenerate: tools/vendor-p4b.sh.
