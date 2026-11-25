# SDL2 vendored for the Android port (docs/android-port, P3b, D-015)

Upstream: https://github.com/libsdl-org/SDL, tag **release-2.32.10** (commit `5d249570393f7a37e037abf22cd6012a4cc56a71`), zlib license
(`LICENSE.txt`).

Kept: `src/`, `include/`, `cmake/`, `CMakeLists.txt` and the files it reads (`SDL2.spec.in`, `sdl2.pc.in`, `sdl2-config.in`, ...), and the
Java side of SDLActivity in `android-project/app/src/main/java/org/libsdl/app/`.
Left out: `test/`, `docs/`, `Xcode*/`, `VisualC*/`, `build-scripts/`, `src/test/`, and the platform folders of `src/` for
other systems (Windows, Apple, X11/Wayland, consoles...). Android needs only the generic code plus `android/`, `dummy/`,
`pthread/`, `unix/`, `linux/`, `dlopen/` and friends, which are all still here.
`src/hidapi/` keeps only the generic part and the Android folder: HIDAPI is built OFF (see below).
Line endings only: `cmake/CheckCPUArchitecture.cmake` was CRLF upstream and is LF here (so that `git am` patches reproduce the tree).

## Altered source (marked in the files: "MUGEN-UNLEASHED PATCH")
1. `android-project/app/src/main/java/org/libsdl/app/SDLActivity.java`: `mHIDDeviceManager = null;` instead of
   `HIDDeviceManager.acquire(this)`. SDL is built with `SDL_HIDAPI=OFF` (HIDAPI on Android is C++, and the build is C only),
   so the `HIDDevice*` native methods do not exist and acquiring the manager would throw `UnsatisfiedLinkError`.
   Every other use of `mHIDDeviceManager` is null-guarded upstream.

## Updating
Replace the files from a newer 2.32.x tag, prune the same way, re-apply the patch above, and record the new tag in
`docs/android-port/03-decisoes.md` (D-015).
