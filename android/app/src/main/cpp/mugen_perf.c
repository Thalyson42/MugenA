/*
 * Android performance measurement (P3b -> P5 plan, item 4 of the pending list): how long the present path takes before it
 * is replaced by a direct RGB565 -> GLES texture upload. Linked with -Wl,--wrap=SDL_UpdateRect and --wrap=SDL_Flip, it times
 * both calls (the sdl12-compat converts the 16-bit screen to 32 bits and uploads it inside them), and every 120 presented
 * frames logs: frames per second, the average frame period, the average CPU time of the engine thread per frame (busy
 * vs. sleeping in the frame limiter), and the average time in SDL_UpdateRect and SDL_Flip. Log tag "perf".
 * REMOVER-NO-RELEASE (docs/android-port/05-diagnosticos-a-remover.md) item 8.
 */
#include <time.h>
#include "mugen.h"

void __real_SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h);
int __real_SDL_Flip(SDL_Surface *screen);

static long long g_updNs, g_flipNs;

static long long now_ns(clockid_t id)
{
    struct timespec ts;
    clock_gettime(id, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void __wrap_SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h)
{
    long long t = now_ns(CLOCK_MONOTONIC);
    __real_SDL_UpdateRect(screen, x, y, w, h);
    g_updNs += now_ns(CLOCK_MONOTONIC) - t;
}

int __wrap_SDL_Flip(SDL_Surface *screen)
{
    long long t = now_ns(CLOCK_MONOTONIC);
    int r = __real_SDL_Flip(screen);
    g_flipNs += now_ns(CLOCK_MONOTONIC) - t;
    return r;
}

/* Once per presented frame (Gfx_AndroidDebugFrame). */
void Mugen_PerfFrame(void)
{
    static int n = 0;
    static long long wall0 = 0, cpu0 = 0;

    if (n == 0) {
        wall0 = now_ns(CLOCK_MONOTONIC);
        cpu0 = now_ns(CLOCK_THREAD_CPUTIME_ID);
        g_updNs = g_flipNs = 0;
    }
    if (++n >= 120) {
        long long wall = now_ns(CLOCK_MONOTONIC) - wall0;
        long long cpu = now_ns(CLOCK_THREAD_CPUTIME_ID) - cpu0;
        Log_Printf("\nperf: %d frames in %lld ms = %.1f fps | per frame: period %.2f ms, engine-thread cpu %.2f ms, "
                   "SDL_UpdateRect %.2f ms, SDL_Flip %.2f ms\n",
                   n, wall / 1000000, n * 1e9 / (double)wall, wall / 1e6 / n, cpu / 1e6 / n, g_updNs / 1e6 / n,
                   g_flipNs / 1e6 / n);
        n = 0;
    }
}
