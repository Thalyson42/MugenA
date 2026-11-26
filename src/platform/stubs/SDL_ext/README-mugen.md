# SDL_mixer.h and SDL_ttf.h (Android build, P3b M1c; temporary)

Verbatim copies of the two headers from `third_party/sdk/include/SDL/` (SDL_mixer and SDL_ttf for SDL 1.2, zlib license; the
text of each header stays as it was). They are copied because `third_party/sdk/include/SDL/` also holds the SDL 1.2 headers of the
Windows SDK, which must not be on the Android include path any more (the engine takes the SDL 1.2 headers from
`third_party/sdl12-compat/include/SDL`), and both headers include the SDL ones with quotes ("SDL_types.h"): in this directory
those resolve to sdl12-compat. They go away with D-003 (M2), when SDL_mixer and SDL_ttf come from vendored sources.
