/* Android port, P3b M1b: SDL 1.2 mode test (D-002 condition 3).
 *
 * This file speaks the SDL 1.2 API only, as the engine does: it is compiled against the sdl12-compat headers
 * (cmake/MugenAndroid.cmake, object library mugen_sdl12_probe) and must never see the SDL 2 ones, whose file names are
 * the same. At run time its SDL_* calls must bind to libSDL.so (sdl12-compat), not to libSDL2.so: libmugen.so therefore
 * links libSDL.so only (D-015, item 5 amended).
 *
 * What it proves, in this order:
 *   1. SDL_SetVideoMode(640, 480, 16, SDL_HWSURFACE) returns a surface;
 *   2. the surface is 16 bpp with the RGB565 masks (0xF800, 0x07E0, 0x001F) and a pixel pointer we can write;
 *   3. SDL_MapRGB agrees with RGB565, and the paths the engine uses on its screen work: SDL_FillRect, SDL_UpperBlit from
 *      a 16 bpp SDL_CreateRGBSurface, with and without SDL_SRCALPHA;
 *   4. a test picture (8 colour bars, a red-to-blue gradient, an orange frame, four coloured corner squares, a moving white
 *      bar) written straight into the 16 bpp pixels is presented with SDL_Flip;
 *   5. what that costs: ms of drawing and of SDL_Flip, unpaced (maximum rate) and paced at 60 fps (late frames);
 *   6. the lifecycle events as the 1.2 API reports them (SDL_ACTIVEEVENT), and that the surface still works after a pause.
 * Format failures make the result FAIL; the costs are reported with a BUDGET verdict of their own. */
#include "mugen_sdl12_probe.h"

#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define W 640
#define H 480
#define UNPACED_MS 3000          /* the first seconds run as fast as SDL_Flip lets them: the cost of one presentation */
#define FRAME_MS 16.6667         /* the engine's tick: 60 fps */
#define GAP_MS 250.0             /* a frame this late is a pause (background), not a slow frame */
#define BUDGET_MS 12.0           /* draw + flip of one frame, average, out of 16.7 ms (the rest is the engine's own work) */

static MugenSayFn g_say;
static int g_failed;

static void Say(const char *fmt, ...)
{
    char line[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    g_say(line);
}

static void Check(int ok, const char *what)
{
    Say("  %s  %s", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        ++g_failed;
    }
}

static double NowMs(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static Uint16 P565(int r, int g, int b)
{
    return (Uint16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static Uint16 *Row(SDL_Surface *s, int y)
{
    return (Uint16 *)((Uint8 *)s->pixels + (size_t)y * s->pitch);
}

static void Fill(Uint16 *row, int x0, int x1, Uint16 c)
{
    int x;

    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 > W) {
        x1 = W;
    }
    for (x = x0; x < x1; ++x) {
        row[x] = c;
    }
}

/* the test picture, written straight into the 16 bpp pixels; `frame` moves the white bar */
static void DrawPicture(SDL_Surface *s, int frame)
{
    static const Uint16 bars[8] = {0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000};
    const Uint16 orange = P565(255, 128, 0);
    const int split = H * 5 / 8;
    const int barx = (frame * 4) % W;
    int x, y;

    for (y = 0; y < H; ++y) {
        Uint16 *row = Row(s, y);

        if (y < split) {
            for (x = 0; x < W; ++x) {
                row[x] = bars[x * 8 / W];
            }
        } else {
            for (x = 0; x < W; ++x) {
                const int r = x * 255 / (W - 1);
                row[x] = P565(r, 0, 255 - r);
            }
        }
        Fill(row, barx, barx + 16, 0xFFFF);
        if (y < 4 || y >= H - 4) {
            Fill(row, 0, W, orange);
        } else {
            Fill(row, 0, 4, orange);
            Fill(row, W - 4, W, orange);
        }
        if (y >= 8 && y < 32) {
            Fill(row, 8, 32, 0xF800);               /* top left: red */
            Fill(row, W - 32, W - 8, 0x07E0);       /* top right: green */
        }
        if (y >= H - 32 && y < H - 8) {
            Fill(row, 8, 32, 0x001F);               /* bottom left: blue */
            Fill(row, W - 32, W - 8, 0xFFE0);       /* bottom right: yellow */
        }
    }
}

static Uint16 PixelAt(SDL_Surface *s, int x, int y)
{
    return Row(s, y)[x];
}

static void Lock(SDL_Surface *s)
{
    if (SDL_MUSTLOCK(s)) {
        SDL_LockSurface(s);
    }
}

static void Unlock(SDL_Surface *s)
{
    if (SDL_MUSTLOCK(s)) {
        SDL_UnlockSurface(s);
    }
}

static int FormatOk(SDL_Surface *s)
{
    return s->format != NULL && s->format->BitsPerPixel == 16 && s->format->BytesPerPixel == 2 &&
           s->format->Rmask == 0xF800 && s->format->Gmask == 0x07E0 && s->format->Bmask == 0x001F &&
           s->format->Amask == 0;
}

static void DescribeSurface(const char *name, SDL_Surface *s)
{
    char drv[64] = "?";

    SDL_VideoDriverName(drv, sizeof drv);
    Say("%s: %dx%d, pitch %d, flags 0x%08x%s%s%s, pixels %s, video driver '%s'", name, s->w, s->h, s->pitch,
        (unsigned)s->flags, (s->flags & SDL_HWSURFACE) ? " HWSURFACE" : "", (s->flags & SDL_DOUBLEBUF) ? " DOUBLEBUF" : "",
        (s->flags & SDL_FULLSCREEN) ? " FULLSCREEN" : "", s->pixels != NULL ? "set" : "NULL", drv);
    if (s->format != NULL) {
        Say("%s: %d bpp (%d bytes), masks R=0x%04x G=0x%04x B=0x%04x A=0x%04x, shifts %d/%d/%d, losses %d/%d/%d", name,
            s->format->BitsPerPixel, s->format->BytesPerPixel, (unsigned)s->format->Rmask, (unsigned)s->format->Gmask,
            (unsigned)s->format->Bmask, (unsigned)s->format->Amask, s->format->Rshift, s->format->Gshift,
            s->format->Bshift, s->format->Rloss, s->format->Gloss, s->format->Bloss);
    }
}

/* the paths video.c / prim.c / gfx.c take on the engine's screen surface */
static void CheckSurfaceOps(SDL_Surface *screen)
{
    SDL_Surface *sw;
    SDL_Rect dst;
    Uint16 p;

    Check(SDL_MapRGB(screen->format, 255, 0, 0) == 0xF800, "SDL_MapRGB(255,0,0) == 0xF800");
    Check(SDL_MapRGB(screen->format, 0, 255, 0) == 0x07E0, "SDL_MapRGB(0,255,0) == 0x07E0");
    Check(SDL_MapRGB(screen->format, 0, 0, 255) == 0x001F, "SDL_MapRGB(0,0,255) == 0x001F");
    Check(SDL_MapRGB(screen->format, 255, 255, 255) == 0xFFFF, "SDL_MapRGB(255,255,255) == 0xFFFF");

    /* direct write, then read back through SDL_FillRect */
    Lock(screen);
    Row(screen, 100)[100] = 0x1234;
    Check(PixelAt(screen, 100, 100) == 0x1234, "direct write to screen->pixels reads back");
    Unlock(screen);
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 255));
    Lock(screen);
    Check(PixelAt(screen, 0, 0) == 0x001F && PixelAt(screen, W - 1, H - 1) == 0x001F, "SDL_FillRect fills the whole screen (blue)");
    Unlock(screen);

    /* SDL_CreateRGBSurface (16 bpp, the engine's masks) + SDL_UpperBlit onto the screen */
    sw = SDL_CreateRGBSurface(SDL_SWSURFACE, 64, 64, 16, 0xF800, 0x07E0, 0x001F, 0);
    Check(sw != NULL, "SDL_CreateRGBSurface(64,64,16, 565 masks)");
    if (sw == NULL) {
        return;
    }
    SDL_FillRect(sw, NULL, SDL_MapRGB(sw->format, 255, 0, 0));
    dst.x = 10;
    dst.y = 10;
    dst.w = 64;
    dst.h = 64;
    Check(SDL_UpperBlit(sw, NULL, screen, &dst) == 0, "SDL_UpperBlit (opaque) returns 0");
    Lock(screen);
    p = PixelAt(screen, 20, 20);
    Unlock(screen);
    Say("  blit opaque red onto blue: pixel 0x%04x (want 0xf800)", (unsigned)p);
    Check(p == 0xF800, "opaque blit lands as the same RGB565 value");

    SDL_FillRect(screen, NULL, 0);
    Check(SDL_SetAlpha(sw, SDL_SRCALPHA, 128) == 0, "SDL_SetAlpha(SDL_SRCALPHA, 128) returns 0");
    dst.x = 10;
    dst.y = 10;
    Check(SDL_UpperBlit(sw, NULL, screen, &dst) == 0, "SDL_UpperBlit (alpha 128) returns 0");
    Lock(screen);
    p = PixelAt(screen, 20, 20);
    Unlock(screen);
    Say("  blit red alpha 128 onto black: pixel 0x%04x (want red about 0x0f-0x10 << 11, no green, no blue)", (unsigned)p);
    Check(((p >> 11) >= 14 && (p >> 11) <= 17) && (p & 0x07FF) == 0, "alpha blit blends about half red onto black");
    SDL_FreeSurface(sw);
}

typedef struct {
    int frames;
    double draw, flip, flipMax, wall;
    int late;
} Stats;

static void Report(const char *name, const Stats *s)
{
    if (s->frames == 0) {
        Say("%s: no frames", name);
        return;
    }
    Say("%s: %d frames, draw %.2f ms avg, SDL_Flip %.2f ms avg (max %.2f), %.2f ms per frame by the clock (%.1f fps)%s", name,
        s->frames, s->draw / s->frames, s->flip / s->frames, s->flipMax, s->wall / s->frames,
        s->wall > 0 ? 1000.0 * s->frames / s->wall : 0.0, s->late > 0 ? "" : ", none late");
    if (s->late > 0) {
        Say("%s: %d frames later than 20 ms", name, s->late);
    }
}

static int Run(SDL_Surface *screen, int seconds)
{
    Stats un = {0}, paced = {0};
    int frame = 0, flipFail = 0, quit = 0, mouseLogged = 0;
    double fg = 0.0, last = NowMs(), nextTick = 0.0;
    SDL_Event ev;

    while (!quit && fg < seconds * 1000.0) {
        double t0, t1, t2, now, dt;
        const int pacedPhase = fg >= UNPACED_MS;
        Stats *st = pacedPhase ? &paced : &un;
        int r;

        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                Say("event: SDL_QUIT");
                quit = 1;
                break;
            case SDL_ACTIVEEVENT:
                Say("event: ACTIVEEVENT gain=%d state=0x%x (%s%s%s)", ev.active.gain, ev.active.state,
                    (ev.active.state & SDL_APPMOUSEFOCUS) ? "MOUSEFOCUS " : "", (ev.active.state & SDL_APPINPUTFOCUS) ? "INPUTFOCUS " : "",
                    (ev.active.state & SDL_APPACTIVE) ? "APPACTIVE" : "");
                break;
            case SDL_VIDEOEXPOSE: Say("event: VIDEOEXPOSE"); break;
            case SDL_VIDEORESIZE: Say("event: VIDEORESIZE %dx%d", ev.resize.w, ev.resize.h); break;
            case SDL_KEYDOWN: Say("event: KEYDOWN sym=%d", (int)ev.key.keysym.sym); break;
            case SDL_MOUSEBUTTONDOWN:
                if (mouseLogged++ < 5) {
                    Say("event: MOUSEBUTTONDOWN button=%d at %d,%d (touch arrives as the mouse)", ev.button.button, ev.button.x, ev.button.y);
                }
                break;
            default: break;
            }
        }

        t0 = NowMs();
        Lock(screen);
        DrawPicture(screen, frame);
        Unlock(screen);
        t1 = NowMs();
        r = SDL_Flip(screen);
        t2 = NowMs();
        if (r != 0 && flipFail++ < 3) {
            Say("SDL_Flip returned %d: %s", r, SDL_GetError());
        }
        ++frame;

        now = t2;
        dt = now - last;
        last = now;
        if (dt > GAP_MS) {
            /* the process was paused: the clock does not run in the background, and the surface must have survived */
            Say("gap of %.0f ms between frames (the app was in the background?); surface %s, pixels %s, format %s", dt,
                SDL_GetVideoSurface() == screen ? "same" : "CHANGED", screen->pixels != NULL ? "set" : "NULL",
                FormatOk(screen) ? "still 16 bpp RGB565" : "CHANGED");
            if (SDL_GetVideoSurface() != screen || screen->pixels == NULL || !FormatOk(screen)) {
                Check(0, "the video surface survives a pause");
            }
            nextTick = 0.0;
            continue;
        }
        fg += dt;
        st->frames++;
        st->draw += t1 - t0;
        st->flip += t2 - t1;
        if (t2 - t1 > st->flipMax) {
            st->flipMax = t2 - t1;
        }
        st->wall += dt;
        if (pacedPhase && dt > 20.0) {
            st->late++;
        }

        if (pacedPhase) {
            /* the engine's own pacing: sleep to the next 16.7 ms tick */
            double wait;

            if (nextTick == 0.0) {
                nextTick = now;
            }
            nextTick += FRAME_MS;
            wait = nextTick - NowMs();
            if (wait > 1.0) {
                SDL_Delay((Uint32)wait);
            } else if (wait < -FRAME_MS) {
                nextTick = NowMs();    /* far behind: do not try to catch up */
            }
        }
    }
    Report("unpaced (maximum rate)", &un);
    Report("paced 60 fps", &paced);
    if (paced.frames > 0) {
        const double cost = (paced.draw + paced.flip) / paced.frames;

        Say("BUDGET: draw + SDL_Flip = %.2f ms per frame (limit %.0f of 16.7 ms): %s", cost, BUDGET_MS,
            cost <= BUDGET_MS ? "OK" : "OVER");
    }
    Check(flipFail == 0, "every SDL_Flip returned 0");
    Check(un.frames + paced.frames > 0, "frames were presented");
    return quit;
}

int Mugen_Sdl12ModeTest(MugenSayFn say, int seconds)
{
    const SDL_version *lv;
    const SDL_VideoInfo *vi;
    SDL_Surface *screen, *again;

    g_say = say;
    g_failed = 0;
    Say("M1b: SDL 1.2 mode test (D-002 condition 3), %d s", seconds);
    lv = SDL_Linked_Version();
    if (lv != NULL) {
        Say("SDL 1.2 API: linked %d.%d.%d, headers %d.%d.%d", lv->major, lv->minor, lv->patch, SDL_MAJOR_VERSION,
            SDL_MINOR_VERSION, SDL_PATCHLEVEL);
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        Say("FAIL SDL_Init(SDL_INIT_VIDEO): %s", SDL_GetError());
        Say("M1b RESULT: FAIL (SDL_Init)");
        return 1;
    }

    screen = SDL_SetVideoMode(W, H, 16, SDL_HWSURFACE);
    if (screen == NULL) {
        Say("FAIL SDL_SetVideoMode(640,480,16,SDL_HWSURFACE): %s", SDL_GetError());
        Say("M1b RESULT: FAIL (SDL_SetVideoMode returned NULL; D-002 (B) has to be decided)");
        SDL_Quit();
        return 1;
    }
    DescribeSurface("screen", screen);
    vi = SDL_GetVideoInfo();
    if (vi != NULL) {
        Say("video info: hw_available=%d wm_available=%d blit_hw=%d video_mem=%u KB, vfmt %d bpp", (int)vi->hw_available,
            (int)vi->wm_available, (int)vi->blit_hw, (unsigned)vi->video_mem, vi->vfmt != NULL ? vi->vfmt->BitsPerPixel : -1);
    }
    Check(screen->w == W && screen->h == H, "surface is 640x480");
    Check(screen->format != NULL && screen->format->BitsPerPixel == 16 && screen->format->BytesPerPixel == 2, "surface is 16 bpp (2 bytes)");
    Check(FormatOk(screen), "masks are RGB565 (0xF800, 0x07E0, 0x001F, no alpha)");
    Check(screen->pixels != NULL && screen->pitch >= W * 2, "pixels are writable (pointer set, pitch >= 1280)");

    if (screen->pixels != NULL && FormatOk(screen)) {
        CheckSurfaceOps(screen);
        Say("drawing the test picture: look for 8 colour bars (white yellow cyan green magenta red blue black) on top, a red to blue"
            " gradient below, an orange frame, corner squares red (top left), green (top right), blue (bottom left), yellow"
            " (bottom right), and a white bar sweeping to the right; the whole 640x480 picture must be visible, letterboxed");
        Run(screen, seconds);
    }

    /* the engine asks again when the mode changes (video.c): same call with its own flags, informational */
    again = SDL_SetVideoMode(W, H, 16, SDL_HWSURFACE | SDL_DOUBLEBUF);
    if (again != NULL) {
        DescribeSurface("screen (HWSURFACE|DOUBLEBUF, second call)", again);
        Say("second SDL_SetVideoMode: %s, 16 bpp RGB565 %s", again == screen ? "same surface object" : "a different surface object",
            FormatOk(again) ? "yes" : "NO");
        if (!FormatOk(again)) {
            Check(0, "second SDL_SetVideoMode keeps 16 bpp RGB565");
        } else if (again->pixels != NULL) {
            Lock(again);
            DrawPicture(again, 0);
            Unlock(again);
            Check(SDL_Flip(again) == 0, "SDL_Flip after the second SDL_SetVideoMode returns 0");
        }
    } else {
        Say("second SDL_SetVideoMode(HWSURFACE|DOUBLEBUF) failed: %s", SDL_GetError());
        Check(0, "second SDL_SetVideoMode (engine flags) returns a surface");
    }

    SDL_Quit();
    Say("M1b RESULT: %s (%d failed checks; the picture still has to be confirmed by eye)", g_failed == 0 ? "PASS" : "FAIL", g_failed);
    return g_failed != 0;
}
