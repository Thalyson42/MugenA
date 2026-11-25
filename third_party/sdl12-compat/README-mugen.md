# sdl12-compat vendored for the Android port (docs/android-port, P3b, D-015)

Upstream: https://github.com/libsdl-org/sdl12-compat, commit **e5fdbd3fe054f19ffe410275435e38ad17ac2bee** (2026-11-20), zlib
license (`LICENSE.txt`). It implements the SDL 1.2 API on top of SDL2, loading `libSDL2.so` with dlopen.

Kept: `src/` (without the macOS/Windows/OS2/MSVC files), `include/` (the SDL 1.2 headers the engine will include), `CMakeLists.txt`
(for reference: the Android build compiles `src/SDL12_compat.c` itself, see `cmake/MugenAndroid.cmake`), `README.md`,
`COMPATIBILITY.md`. Left out: `test/`, `build-scripts/`, packaging files.

## Altered source (marked in the file: "MUGEN-UNLEASHED PATCH")
1. `src/SDL12_compat.c`, `SDL20_LIBNAME`: `"libSDL2.so"` when `__ANDROID__` (upstream's unix branch asks for
   `libSDL2-2.0.so.0`, which an APK does not have).

## Note for the engine (M1c)
Its `SDL_main.h` renames `main` to `SDL_main` only on Windows and macOS: on Android the engine's entry point must be
named `SDL_main` explicitly (SDLActivity looks for that symbol in libmugen.so).
