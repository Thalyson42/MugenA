/*
 * Phase P3a self-test, run on the device by the diagnostic app (Native.platformSelfTest). It exercises the platform
 * layer and the real engine files timer.c, log.c and file.c inside libmugen.so (compiled with the engine flags,
 * -fsigned-char included) and returns a text report to paste into docs/android-port/LOG.md.
 *
 * Everything it creates lives in .p3a-selftest (in the data directory and in the app's cache directory) and is removed afterwards, except mugen.log, which is
 * written by the real Log_Init in the data directory: that file is the "mugen.log in the defined directory" of P3a.
 */
#include "platform/platform.h"
#include "mugen.h"

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

extern char g_p3aLastError[256];

static char *g_rep;
static size_t g_cap;
static int g_checks, g_failed;

static void Say(const char *fmt, ...)
{
    char line[600];
    size_t used = strlen(g_rep);
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (used + strlen(line) + 2 < g_cap) {
        strcat(g_rep, line);
        strcat(g_rep, "\n");
    }
    if (g_log != NULL) {
        Log_Printf("p3a: %s\n", line);
    }
}

static void Check(int ok, const char *what)
{
    ++g_checks;
    if (!ok) {
        ++g_failed;
    }
    Say("  [%s] %s", ok ? "OK" : "FAIL", what);
}

static long long NowNs(clockid_t id)
{
    struct timespec ts;

    clock_gettime(id, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void Touch(const char *path)
{
    FILE *f = fopen(path, "wb");       /* Mugen_fopen */

    if (f != NULL) {
        fputs("x", f);
        fclose(f);
    }
}

static int Exists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0;
}

static void ListTop(void)
{
    DIR *d = opendir(".");
    struct dirent *de;
    char names[400];
    int n = 0;

    names[0] = '\0';
    if (d == NULL) {
        Say("  (cannot list the data directory)");
        return;
    }
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') {
            continue;
        }
        if (n < 12 && strlen(names) + strlen(de->d_name) + 3 < sizeof(names)) {
            if (n > 0) {
                strcat(names, ", ");
            }
            strcat(names, de->d_name);
        }
        ++n;
    }
    closedir(d);
    Say("  top-level entries: %d%s%s", n, n > 0 ? ": " : "", names);
}

static void GameFile(const char *ref)
{
    char out[512];
    int r = Mugen_ResolvePath(ref, out, sizeof(out));

    Say("  %-22s -> %s", ref, r == 1 ? out : "not found");
}

/* True when the file system under base ignores case (the emulated external storage, /storage/emulated/0, does). */
static int CaseInsensitiveFs(const char *base)
{
    char a[1100], b[1100];
    int ci;

    snprintf(a, sizeof(a), "%s/.p3a-ci-Probe", base);
    snprintf(b, sizeof(b), "%s/.p3a-CI-probe", base);
    Touch(a);
    ci = Exists(b);
    unlink(a);
    return ci;
}

/*
 * Resolver checks inside <base>/.p3a-selftest. On a case-sensitive file system the corrected spelling is checked
 * exactly; on a case-insensitive one the OS already finds every spelling, so only "found" is checked.
 */
static void Paths(const char *base)
{
    char root[1000], p[1100], q[1100], out[1100], want[1100];
    int ci = CaseInsensitiveFs(base);
    FILE *f;
    MugenPathStats st;

    Mugen_PathCacheClear();
    snprintf(root, sizeof(root), "%s/.p3a-selftest", base);
    Say("  on: %s (%s file system)", base, ci ? "case-INSENSITIVE" : "case-sensitive");
#define MK(rel) (snprintf(p, sizeof(p), "%s/" rel, root), p)
    mkdir(root, 0755);
    mkdir(MK("Data"), 0755);
    mkdir(MK("Data/Sprites"), 0755);
    mkdir(MK("Chars"), 0755);
    mkdir(MK("Sound"), 0755);
    Touch(MK("Data/Sprites/Kfm.SFF"));
    Touch(MK("Chars/kfm.DEF"));

    snprintf(q, sizeof(q), "%s\\data\\SPRITES\\kfm.sff", root);       /* '\' after the root, case wrong */
    snprintf(want, sizeof(want), "%s/Data/Sprites/Kfm.SFF", root);
    Check(Mugen_ResolvePath(q, out, sizeof(out)) == 1 && (ci || strcmp(out, want) == 0), "'\\' and case resolved");
    snprintf(q, sizeof(q), "%s\\DATA\\sprites\\KFM.SFF", root);
    f = fopen(q, "rb");
    Check(f != NULL, "fopen with another case in every component");
    if (f != NULL) {
        fclose(f);
    }
    snprintf(q, sizeof(q), "%s/data/sprites/nothing.sff", root);
    Check(fopen(q, "rb") == NULL, "missing file stays missing");
    snprintf(q, sizeof(q), "%s\\CHARS\\KFM.def", root);
    Check(File_Find(q, out, NULL, "") == 1, "File_Find (file.c) ignores case");
    snprintf(q, sizeof(q), "%s/Chars/KFM.DEF", root);
    Check(File_Exists(q), "File_Exists (file.c)");

    snprintf(q, sizeof(q), "%s\\SOUND\\Out.TXT", root);
    f = fopen(q, "wb");
    if (f != NULL) {
        fputs("hello", f);
        fclose(f);
    }
    Check(Exists(MK("Sound/Out.TXT")), "a file created through fopen lands in the existing directory");
    if (!ci) {
        Check(!Exists(MK("SOUND")), "... without creating a second directory");
    }
    snprintf(q, sizeof(q), "%s/sound/out.txt", root);
    Check(Mugen_ResolvePath(q, out, sizeof(out)) == 1, "... and is found at once");

    Mugen_PathCacheStats(&st);
    Say("  directory cache: %ld hits, %ld misses, %ld reloads", st.hits, st.misses, st.reloads);

    unlink(MK("Data/Sprites/Kfm.SFF"));
    unlink(MK("Chars/kfm.DEF"));
    unlink(MK("Sound/Out.TXT"));
    rmdir(MK("Data/Sprites"));
    rmdir(MK("Data"));
    rmdir(MK("Chars"));
    rmdir(MK("Sound"));
    rmdir(root);
#undef MK
    Mugen_PathCacheClear();
}

static void Timing(void)
{
    long long sum = 0, mn = 1LL << 60, mx = 0, t, wall, cpu0, cpu, t0;
    int i, late = 0;

    for (i = 0; i < 20; ++i) {
        t = NowNs(CLOCK_MONOTONIC);
        Mugen_SleepNs(2000000LL);
        t = NowNs(CLOCK_MONOTONIC) - t - 2000000LL;
        sum += t;
        if (t < mn) {
            mn = t;
        }
        if (t > mx) {
            mx = t;
        }
    }
    Say("  sleep(2 ms) overshoot over 20: min %lld us, avg %lld us, max %lld us", mn / 1000, sum / 20 / 1000, mx / 1000);
    Check(mn >= 0, "nanosleep never returns early");

    Timer_Init();
    Timer_SetFps(60);
    t0 = NowNs(CLOCK_MONOTONIC);
    cpu0 = NowNs(CLOCK_THREAD_CPUTIME_ID);
    for (i = 0; i < 120; ++i) {
        if (Timer_WaitFrame() > 0) {
            ++late;
        }
        g_timerFrame++;
    }
    wall = NowNs(CLOCK_MONOTONIC) - t0;
    cpu = NowNs(CLOCK_THREAD_CPUTIME_ID) - cpu0;
    Say("  Timer_WaitFrame, 120 frames at 60 fps: %.1f ms wall (ideal %.1f), %.1f ms cpu of this thread (%.0f%%), %d late",
        wall / 1e6, 119 * g_timerFrameTicks / 1e6, cpu / 1e6, 100.0 * cpu / wall, late);
    Check(wall >= 119 * g_timerFrameTicks, "frame pacing is never early");
}

int P3a_SelfTest(char *report, size_t cap, const char *altDir)
{
    char cwd[512];
    volatile unsigned char u = 0xFF;
    char c = (char)u;

    g_rep = report;
    g_cap = cap;
    g_rep[0] = '\0';
    g_checks = g_failed = 0;

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "?");
    }
    Say("== P3a platform self-test ==");
    Say("data dir: %s", Mugen_GetDataDir()[0] != '\0' ? Mugen_GetDataDir() : "(not set)");
    Say("cwd:      %s", cwd);
    Say("compiled: %zu-bit pointers, long long %zu bytes", sizeof(void *) * 8, sizeof(long long));
    Check(c < 0, "char is signed (-fsigned-char) in libmugen.so");
    Check(Mugen_GetDataDir()[0] != '\0' && strcmp(Mugen_GetDataDir(), cwd) == 0, "the data directory is the current directory");

    if (Log_Init()) {
        Say("mugen.log opened by the real Log_Init (log.c), in the data directory");
    } else {
        Check(0, "Log_Init (could not create mugen.log in the data directory)");
    }

    Say("-- path resolution (real file.c on the platform layer)");
    Paths(".");                                    /* the data directory, relative paths */
    if (altDir != NULL && altDir[0] != '\0') {
        Paths(altDir);                             /* the app's internal cache: case-sensitive, absolute paths */
    }
    Say("-- game files in the data directory (ignoring case)");
    GameFile("data\\mugen.cfg");
    GameFile("data\\fight.def");
    GameFile("chars");
    GameFile("stages");
    GameFile("sound");
    GameFile("font");
    ListTop();
    Say("-- time (real timer.c)");
    Timing();
    Say("RESULT: %d checks, %d failed", g_checks, g_failed);

    if (g_log != NULL) {
        Log_Printf("p3a: self-test finished\n");
        Log_Close();
        g_log = NULL;
    }
    return g_failed == 0;
}
