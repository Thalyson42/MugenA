# libresample (vendored for the Android port, D-003 / D-019)
Origin: https://github.com/minorninth/libresample, commit 7cb7f9c3f72d4e6774d964dc324af827192df7c3 (0.1.5). Dual license BSD-3-Clause / LGPL-2.1+:
the BSD terms are used (LICENSE-BSD.txt). Not the library of the original: Elecbyte's libresample.dll is 0.1.3 with
interleaved multi-channel buffers, resample_init/deinit/reset. The engine-facing API is android/app/src/main/cpp/mugen_resample.c,
built on this one (symbols renamed with -Dresample_*=up_resample_* in cmake/MugenAndroid.cmake).
Patch (MUGEN-UNLEASHED PATCH, tools/p4a/libresample-share-tables.patch): the filter tables are built once per quality
and shared by all handles (upstream rebuilds them, a few ms, on every resample_open; the game opens one handle per sound).
Regenerate: tools/vendor-p4a.sh.
