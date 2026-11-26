/* Android port, P3b M1b: the interface between the SDL 1.2 mode test and its caller. No SDL header on purpose: the caller
 * (mugen_sdl_probe.c, the SDL_main) must not see any, and the test (mugen_sdl12_probe.c) sees only the SDL 1.2 ones. */
#ifndef MUGEN_SDL12_PROBE_H
#define MUGEN_SDL12_PROBE_H

typedef void (*MugenSayFn)(const char *line);

/* D-002 condition 3, through the SDL 1.2 API only (sdl12-compat): SDL_SetVideoMode(640, 480, 16, SDL_HWSURFACE), the
 * surface must be 16 bpp RGB565 and usable (direct writes, SDL_FillRect, SDL_UpperBlit with SDL_SRCALPHA), a visible test
 * picture is presented with SDL_Flip for `seconds` seconds of foreground time, lifecycle events are logged.
 * Every line goes through `say`. Returns 0 when every check passed, 1 when at least one failed. */
int Mugen_Sdl12ModeTest(MugenSayFn say, int seconds);

#endif
