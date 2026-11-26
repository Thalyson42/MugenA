/* Android port, P3b M1a/M1b/M2: the SDL 1.2 mode test, run instead of the engine when the app asks for it (D-002, D-015).
 * Since M2 the exported SDL_main is the engine's (src/sdlmain.c), which calls Mugen_SdlProbeMain when the start mode is
 * MUGEN_START_SDL_PROBE. What follows is the M1b text, still true for the test itself:
 *
 * SDLActivity (Java) loads libSDL2.so, libSDL.so (sdl12-compat) and this library, then libSDL2.so calls the exported
 * function SDL_main on its own thread. This file includes NO SDL header and calls NO SDL function: libmugen.so links
 * libSDL.so only (the SDL 1.2 API), and every SDL_* name it uses must bind there, while libSDL2.so exports the same names
 * with the SDL 2 ABI (D-015, item 5 amended). The M1a SDL 2 phase (window + renderer, lifecycle) was verified on the
 * A065M and is not repeated here; it is in the git history (commit 567cf4a).
 *
 * It runs the SDL 1.2 mode test of M1b (mugen_sdl12_probe.c) and writes what it saw to logcat (tag "mugen-sdl") and to
 * <data dir>/sdl-probe.txt, which the diagnostic app shows when the activity ends. M1c replaces this entry point with the
 * engine's own SDL_main (sdlmain.c). */
#include "mugen_sdl12_probe.h"

#include <android/log.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TAG "mugen-sdl"
#define PROBE_SECONDS 15

/* platform layer of libmugen.so (src/platform/platform_app.c); declared here so that no engine header is needed */
extern int         Mugen_SetDataDir(const char *dir);
extern const char *Mugen_GetDataDir(void);

static FILE *g_report;

static void SayLine(const char *line)
{
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", line);
    if (g_report != NULL) {
        fprintf(g_report, "%s\n", line);
        fflush(g_report);              /* survives a crash: the last line says how far it got */
    }
}

static void Say(const char *fmt, ...)
{
    char line[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    SayLine(line);
}

/* The app normally sets the data directory (Native.initPlatform). If the process was started straight into this
 * activity, ask SDL2 for the same folder (getExternalFilesDir, D-005); by dlsym on the already loaded libSDL2.so, which is
 * how sdl12-compat reaches it too, so that this file needs no SDL 2 header and no SDL 2 link. */
static void DataDirFromSdl2(void)
{
    typedef const char *(*PathFn)(void);
    void *lib = dlopen("libSDL2.so", RTLD_NOW | RTLD_NOLOAD);
    PathFn fn = lib != NULL ? (PathFn)dlsym(lib, "SDL_AndroidGetExternalStoragePath") : NULL;
    const char *ext = fn != NULL ? fn() : NULL;

    if (ext != NULL) {
        Mugen_SetDataDir(ext);
    }
}

int Mugen_SdlProbeMain(void)
{
    char path[1024];
    int rc;

    if (Mugen_GetDataDir()[0] == '\0') {
        DataDirFromSdl2();
    }
    if (Mugen_GetDataDir()[0] != '\0') {
        snprintf(path, sizeof path, "%s/sdl-probe.txt", Mugen_GetDataDir());
        g_report = fopen(path, "w");
    }

    Say("Mugen_SdlProbeMain entered (P3b M1b, SDL 1.2 API through sdl12-compat)");
    Say("data dir: %s", Mugen_GetDataDir()[0] != '\0' ? Mugen_GetDataDir() : "(none)");

    rc = Mugen_Sdl12ModeTest(SayLine, PROBE_SECONDS);

    Say("Mugen_SdlProbeMain done (result %s)", rc == 0 ? "PASS" : "FAIL");
    if (g_report != NULL) {
        fclose(g_report);
        g_report = NULL;
    }
    return rc;
}
