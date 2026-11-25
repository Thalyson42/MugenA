/* Android port, P3b M1a: the SDL_main that SDLActivity runs while the engine is not wired in yet (D-002, D-015).
 *
 * It speaks the SDL 2 API only (it is compiled against the SDL 2 headers, see cmake/MugenAndroid.cmake) and proves the
 * chain: Java SDLActivity -> libSDL2.so -> libmugen.so:SDL_main -> a window and a renderer that present frames.
 * It writes what it saw to logcat (tag "mugen-sdl") and to <data dir>/sdl-probe.txt, which the diagnostic app shows
 * when the activity ends. It ends by itself after PROBE_SECONDS in the foreground, or when the user leaves (SDL_QUIT).
 * M1b adds the SDL 1.2 mode test (SDL_SetVideoMode, D-002 condition 3) in its own file; M1c replaces this entry point
 * with the engine's own SDL_main. */
#include <SDL.h>              /* SDL 2: renames main to SDL_main on Android */
#include <SDL_system.h>       /* SDL_AndroidGetExternalStoragePath */
#include <android/log.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TAG "mugen-sdl"
#define PROBE_SECONDS 20

/* platform layer of libmugen.so (src/platform/platform_app.c); declared here so that no engine header is needed */
extern int         Mugen_SetDataDir(const char *dir);
extern const char *Mugen_GetDataDir(void);

static FILE *g_report;

static void Say(const char *fmt, ...)
{
    char line[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", line);
    if (g_report != NULL) {
        fprintf(g_report, "%s\n", line);
        fflush(g_report);              /* survives a crash: the last line says how far it got */
    }
}

static const char *WindowEventName(Uint8 e)
{
    switch (e) {
    case SDL_WINDOWEVENT_SHOWN: return "SHOWN";
    case SDL_WINDOWEVENT_HIDDEN: return "HIDDEN";
    case SDL_WINDOWEVENT_EXPOSED: return "EXPOSED";
    case SDL_WINDOWEVENT_MOVED: return "MOVED";
    case SDL_WINDOWEVENT_RESIZED: return "RESIZED";
    case SDL_WINDOWEVENT_SIZE_CHANGED: return "SIZE_CHANGED";
    case SDL_WINDOWEVENT_MINIMIZED: return "MINIMIZED";
    case SDL_WINDOWEVENT_MAXIMIZED: return "MAXIMIZED";
    case SDL_WINDOWEVENT_RESTORED: return "RESTORED";
    case SDL_WINDOWEVENT_FOCUS_GAINED: return "FOCUS_GAINED";
    case SDL_WINDOWEVENT_FOCUS_LOST: return "FOCUS_LOST";
    case SDL_WINDOWEVENT_CLOSE: return "CLOSE";
    default: return "other";
    }
}

int main(int argc, char *argv[])
{
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_version linked;
    SDL_DisplayMode dm;
    SDL_RendererInfo ri;
    const char *ext;
    char path[1024];
    int w = 0, h = 0, rw = 0, rh = 0, running = 1, frames = 0, backgrounded = 0;
    Uint32 t0, tBackgroundStart = 0, tBackground = 0;
    (void)argc;
    (void)argv;

    /* the data directory: the app normally set it (Native.initPlatform); if the process was started straight into this
     * activity, take the same folder from SDL (getExternalFilesDir, D-005) */
    if (Mugen_GetDataDir()[0] == '\0') {
        ext = SDL_AndroidGetExternalStoragePath();
        if (ext != NULL) {
            Mugen_SetDataDir(ext);
        }
    }
    if (Mugen_GetDataDir()[0] != '\0') {
        snprintf(path, sizeof path, "%s/sdl-probe.txt", Mugen_GetDataDir());
        g_report = fopen(path, "w");
    }

    SDL_GetVersion(&linked);
    Say("SDL_main entered (SDL2 %d.%d.%d linked, platform %s)", linked.major, linked.minor, linked.patch,
        SDL_GetPlatform());
    Say("data dir: %s", Mugen_GetDataDir()[0] != '\0' ? Mugen_GetDataDir() : "(none)");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        Say("FAIL SDL_Init: %s", SDL_GetError());
        goto done;
    }
    Say("video driver: %s", SDL_GetCurrentVideoDriver());
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
        Say("display 0: %dx%d @ %d Hz, format %s", dm.w, dm.h, dm.refresh_rate, SDL_GetPixelFormatName(dm.format));
    }

    win = SDL_CreateWindow("MUGEN", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
                           SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN);
    if (win == NULL) {
        Say("FAIL SDL_CreateWindow: %s", SDL_GetError());
        goto done;
    }
    SDL_GetWindowSize(win, &w, &h);
    Say("window: %dx%d", w, h);

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    if (ren == NULL) {
        Say("FAIL SDL_CreateRenderer: %s", SDL_GetError());
        goto done;
    }
    SDL_GetRendererInfo(ren, &ri);
    SDL_GetRendererOutputSize(ren, &rw, &rh);
    Say("renderer: %s, flags 0x%x, max texture %dx%d, output %dx%d", ri.name, (unsigned)ri.flags, ri.max_texture_width,
        ri.max_texture_height, rw, rh);

    t0 = SDL_GetTicks();
    while (running) {
        SDL_Event ev;
        Uint32 now;
        SDL_Rect bar;

        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                Say("event: SDL_QUIT");
                running = 0;
                break;
            case SDL_APP_TERMINATING: Say("event: APP_TERMINATING"); running = 0; break;
            case SDL_APP_LOWMEMORY: Say("event: APP_LOWMEMORY"); break;
            case SDL_APP_WILLENTERBACKGROUND: Say("event: APP_WILLENTERBACKGROUND"); break;
            case SDL_APP_DIDENTERBACKGROUND:
                Say("event: APP_DIDENTERBACKGROUND");
                backgrounded = 1;
                tBackgroundStart = SDL_GetTicks();
                break;
            case SDL_APP_WILLENTERFOREGROUND: Say("event: APP_WILLENTERFOREGROUND"); break;
            case SDL_APP_DIDENTERFOREGROUND:
                Say("event: APP_DIDENTERFOREGROUND");
                if (backgrounded) {
                    tBackground += SDL_GetTicks() - tBackgroundStart;   /* the probe's clock does not run in the background */
                    backgrounded = 0;
                }
                break;
            case SDL_WINDOWEVENT:
                Say("event: WINDOWEVENT %s (%d,%d)", WindowEventName(ev.window.event), ev.window.data1, ev.window.data2);
                break;
            case SDL_RENDER_TARGETS_RESET: Say("event: RENDER_TARGETS_RESET"); break;
            case SDL_RENDER_DEVICE_RESET: Say("event: RENDER_DEVICE_RESET"); break;
            default: break;
            }
        }

        now = SDL_GetTicks();
        if ((int)(now - t0 - tBackground) > PROBE_SECONDS * 1000) {
            Say("probe time is over");
            running = 0;
        }

        /* a colour that changes once per second and a bar that sweeps the width: visible proof of presentation */
        {
            int sec = (int)((now - t0) / 1000);
            static const Uint8 pal[6][3] = {{200, 40, 40}, {40, 160, 60}, {40, 80, 200}, {200, 180, 30}, {160, 50, 170}, {30, 170, 170}};
            const Uint8 *c = pal[sec % 6];

            SDL_SetRenderDrawColor(ren, c[0], c[1], c[2], 255);
            SDL_RenderClear(ren);
            bar.w = rw / 8 > 0 ? rw / 8 : 1;
            bar.h = rh;
            bar.x = (int)(((now - t0) / 4) % (Uint32)(rw > 0 ? rw : 1));
            bar.y = 0;
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderFillRect(ren, &bar);
            SDL_RenderPresent(ren);
        }
        ++frames;
    }

    {
        Uint32 total = SDL_GetTicks() - t0 - tBackground;
        Say("presented %d frames in %u ms foreground (%.1f ms/frame)", frames, (unsigned)total,
            frames > 0 ? (double)total / frames : 0.0);
    }

done:
    if (ren != NULL) {
        SDL_DestroyRenderer(ren);
    }
    if (win != NULL) {
        SDL_DestroyWindow(win);
    }
    SDL_Quit();
    Say("SDL_main done");
    if (g_report != NULL) {
        fclose(g_report);
        g_report = NULL;
    }
    return 0;
}
