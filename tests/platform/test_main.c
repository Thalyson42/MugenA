/*
 * Host tests of the Android platform layer (phase P3a): path resolution (platform_path.c), the data directory,
 * the stdout/stderr capture and system-log hook (platform_app.c), and the real engine files timer.c, log.c and
 * file.c running on top of them. Results go to the original stdout (saved before the capture test redirects it).
 */
#include "platform/platform.h"
#include "mugen.h"

#include <pthread.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ---- the globals and callbacks that the engine files under test expect from the rest of the engine ------------- */
int g_timerPerformance, g_timerFpsInterval, g_timerFpsFrames, g_timerFrameRate, g_timerInitialized, g_timerResyncs;
int g_frameRate, g_timerTicksPerSecond;
long long g_timerFrameTicks, g_timerStart, g_timerFpsStart, g_timerFrameStart, g_timerFrame, g_timerFrequency;
double g_timerMeasuredFps;
Log *g_log, *g_matchLog;
GameVars *g_game;
int g_outOfMemory;

/* file.c also holds Ini_ReadString, which the tests do not use */
char *Ini_GetValue(Ini *ini, const char *key) { (void)ini; (void)key; return NULL; }
int Str_Unquote(char *s) { (void)s; return 0; }

static char g_lastError[256];
void Error_Printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(g_lastError, sizeof(g_lastError), fmt, ap);
    va_end(ap);
}

/* ---- tiny test harness ---------------------------------------------------------------------------------------- */
static int g_out;
static int g_checks, g_failed;

static void Say(const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    (void)!write(g_out, buf, strlen(buf));
}

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            Say("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                  \
        }                                                                        \
    } while (0)

#define CHECK_STR(got, want)                                                     \
    do {                                                                         \
        ++g_checks;                                                              \
        if (strcmp((got), (want)) != 0) {                                        \
            ++g_failed;                                                          \
            Say("FAIL %s:%d: got \"%s\", want \"%s\"\n", __FILE__, __LINE__, (got), (want)); \
        }                                                                        \
    } while (0)

static void Touch(const char *path)
{
    FILE *f = fopen("/dev/null", "r");     /* (macro: Mugen_fopen) just to prove /dev/null is reachable */
    (void)f;
    {
#undef fopen
        FILE *w = fopen(path, "wb");
#define fopen(path, mode) Mugen_fopen(path, mode)
        if (w != NULL) {
            fputs("x", w);
            fclose(w);
        }
    }
}

static int Exists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0;
}

static void MakeTree(const char *root)
{
    char p[512];
    const char *dirs[] = { "Data", "Data/Sprites", "Chars", "Chars/KFM", "Dup", "Sound" };
    size_t i;

    for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
        snprintf(p, sizeof(p), "%s/%s", root, dirs[i]);
        mkdir(p, 0755);
    }
    snprintf(p, sizeof(p), "%s/Data/Sprites/Kfm.SFF", root);
    Touch(p);
    snprintf(p, sizeof(p), "%s/Chars/KFM/kfm.def", root);
    Touch(p);
    snprintf(p, sizeof(p), "%s/Dup/a.TXT", root);
    Touch(p);
    snprintf(p, sizeof(p), "%s/Dup/A.txt", root);
    Touch(p);
}

/* ---- path resolution ------------------------------------------------------------------------------------------- */
static void TestResolve(const char *root)
{
    char out[512], tiny[8];
    MugenPathStats st;
    int r;

    CHECK(Mugen_SetDataDir(root) == 1);
    CHECK(!Mugen_SetDataDir("/nonexistent/dir/for/test"));
    CHECK_STR(Mugen_GetDataDir(), root);
    Mugen_PathCacheClear();

    r = Mugen_ResolvePath("data\\sprites\\KFM.sff", out, sizeof(out));
    CHECK(r == 1);
    CHECK_STR(out, "Data/Sprites/Kfm.SFF");

    r = Mugen_ResolvePath("Data/Sprites/Kfm.SFF", out, sizeof(out));            /* exact spelling: fast path */
    CHECK(r == 1);
    CHECK_STR(out, "Data/Sprites/Kfm.SFF");

    snprintf(out, sizeof(out), "%s/chars/KFM/KFM.DEF", root);                     /* absolute */
    {
        char in[512];
        strcpy(in, out);
        r = Mugen_ResolvePath(in, out, sizeof(out));
    }
    CHECK(r == 1);
    {
        char want[512];
        snprintf(want, sizeof(want), "%s/Chars/KFM/kfm.def", root);
        CHECK_STR(out, want);
    }

    r = Mugen_ResolvePath("DATA//sprites\\\\kfm.sff/", out, sizeof(out));         /* repeated and trailing separators */
    CHECK(r == 1);
    CHECK_STR(out, "Data/Sprites/Kfm.SFF");

    r = Mugen_ResolvePath("./chars/../CHARS/kfm/KFM.DEF", out, sizeof(out));      /* "." and ".." are kept */
    CHECK(r == 1);
    CHECK_STR(out, "./Chars/../Chars/KFM/kfm.def");

    r = Mugen_ResolvePath("DATA\\Sprites\\New.SFF", out, sizeof(out));            /* new file in an existing dir */
    CHECK(r == 0);
    CHECK_STR(out, "Data/Sprites/New.SFF");

    r = Mugen_ResolvePath("nope\\x\\y.def", out, sizeof(out));
    CHECK(r == 0);
    CHECK_STR(out, "nope/x/y.def");

    r = Mugen_ResolvePath("data/nope/Y.def", out, sizeof(out));                  /* first missing part, then as written */
    CHECK(r == 0);
    CHECK_STR(out, "Data/nope/Y.def");

    r = Mugen_ResolvePath("dup/A.TXT", out, sizeof(out));                          /* two candidates: the smaller name */
    CHECK(r == 1);
    CHECK_STR(out, "Dup/A.txt");
    r = Mugen_ResolvePath("dup/a.TXT", out, sizeof(out));                          /* exact spelling wins */
    CHECK(r == 1);
    CHECK_STR(out, "Dup/a.TXT");

    r = Mugen_ResolvePath("data/sprites/kfm.sff", tiny, sizeof(tiny));
    CHECK(r == -1);
    CHECK(Mugen_ResolvePath(NULL, out, sizeof(out)) == -1);

    CHECK_STR(Mugen_ResolvePathTmp("CHARS\\kfm\\KFM.def"), "Chars/KFM/kfm.def");

    Mugen_PathCacheStats(&st);
    CHECK(st.reloads > 0);
    CHECK(st.hits > 0);                                                           /* the Data dir was read once, reused */
    Say("  path cache: %ld hits, %ld misses, %ld reloads\n", st.hits, st.misses, st.reloads);
}

static void TestFopen(const char *root)
{
    FILE *f;
    char out[512];

    (void)root;
    f = fopen("DATA\\sprites\\KFM.SFF", "rb");
    CHECK(f != NULL);
    if (f != NULL) {
        fclose(f);
    }
    CHECK(fopen("data/sprites/missing.sff", "rb") == NULL);

    /* a file created through fopen lands in the case-corrected directory and is found afterwards */
    f = fopen("SOUND\\Out.TXT", "wb");
    CHECK(f != NULL);
    if (f != NULL) {
        fputs("hello", f);
        fclose(f);
    }
    CHECK(Exists("Sound/Out.TXT"));
    CHECK(!Exists("SOUND"));
    CHECK(Mugen_ResolvePath("sound/OUT.txt", out, sizeof(out)) == 1);
    CHECK_STR(out, "Sound/Out.TXT");
    f = fopen("sound/out.TXT", "rb");
    CHECK(f != NULL);
    if (f != NULL) {
        fclose(f);
    }

    /* a file created behind the platform's back is seen too (directory mtime changes) */
    usleep(30000);
    Touch("Sound/Later.WAV");
    CHECK(Mugen_ResolvePath("sound/later.wav", out, sizeof(out)) == 1);
    CHECK_STR(out, "Sound/Later.WAV");
    usleep(30000);
    unlink("Sound/Later.WAV");
    CHECK(Mugen_ResolvePath("sound/later.wav", out, sizeof(out)) == 0);           /* and its removal */
}

static void *Hammer(void *arg)
{
    int i, bad = 0;
    char out[512];

    (void)arg;
    for (i = 0; i < 3000; ++i) {
        if (Mugen_ResolvePath((i & 1) ? "chars\\KFM\\KFM.DEF" : "DATA/sprites/kfm.sff", out, sizeof(out)) != 1) {
            ++bad;
        }
        if (strstr(out, "Kfm.SFF") == NULL && strstr(out, "kfm.def") == NULL) {
            ++bad;
        }
    }
    return (void *)(intptr_t)bad;
}

static void TestThreads(void)
{
    pthread_t th[4];
    int i;

    Mugen_PathCacheClear();
    for (i = 0; i < 4; ++i) {
        pthread_create(&th[i], NULL, Hammer, NULL);
    }
    for (i = 0; i < 4; ++i) {
        void *r;
        pthread_join(th[i], &r);
        CHECK((intptr_t)r == 0);
    }
}

/* ---- file.c, the real File_Find ------------------------------------------------------------------------------- */
static void TestFileFind(void)
{
    char out[512], dir[512];
    int found;

    g_game = (GameVars *)calloc(1, sizeof(GameVars));
    CHECK(g_game != NULL);
    g_game->motif_dir[0] = '\0';

    found = File_Find("chars\\KFM\\KFM.DEF", out, dir, "");
    CHECK(found == 1);
    CHECK(File_Exists(out));                                      /* the returned name still has the engine's case */
    found = File_Find("DATA\\sprites\\kfm.sff", out, dir, "data/");
    CHECK(found == 0 || found == 1);                              /* base "data/" + "DATA/sprites/..": just must not crash */
    found = File_Find("c:\\chars\\kfm\\kfm.def", out, dir, "");    /* the drive letter is cut off, as in the original */
    CHECK(found == 1);
    found = File_Find("chars\\nobody\\x.def", out, dir, "");
    CHECK(found == 0);
    CHECK(strstr(g_lastError, "Can't find file") != NULL);
    CHECK(File_Exists("DATA\\SPRITES\\kfm.sff"));
    CHECK(!File_Exists("DATA\\SPRITES\\nothing.sff"));
}

/* ---- log.c on top of the platform layer ----------------------------------------------------------------------- */
static int g_sinkLines, g_sinkSawStatus, g_sinkErrLines;
static void Sink(int fd, const char *line)
{
    ++g_sinkLines;
    if (strstr(line, "status log") != NULL) {
        g_sinkSawStatus = 1;
    }
    /* on the host Mugen_LogWrite writes "[E] text" to stderr (on Android it calls logcat directly) */
    if (fd == 2 && strncmp(line, "[E] ", 4) == 0) {
        ++g_sinkErrLines;
    }
}

static void TestLog(void)
{
    char buf[4096];
    FILE *f;
    size_t n;
    int i;

    /* an existing MUGEN.LOG of another case is the file the engine writes to, like on Windows */
    Touch("MUGEN.LOG");
    Mugen_PathCacheClear();
    CHECK(Mugen_StdioToLog(Sink) == 1);                          /* from here on stdout/stderr are captured */
    CHECK(Log_Init() == 1);
    Log_Printf("Reading configuration file...");
    Log_Printf("OK\n");
    Log_Printf("%s\n", "line with percent %s and a tab\t.");
    Log_Close();
    g_log = NULL;
    for (i = 0; i < 200 && g_sinkLines < 5; ++i) {               /* the capture thread is asynchronous */
        usleep(10000);
    }

    CHECK(Exists("MUGEN.LOG"));
    CHECK(!Exists("mugen.log"));
    f = fopen("mugen.log", "rb");
    CHECK(f != NULL);
    n = 0;
    if (f != NULL) {
        n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
    }
    buf[n] = '\0';
    CHECK(strstr(buf, "status log") != NULL);
    CHECK(strstr(buf, "Reading configuration file...OK") != NULL);
    CHECK(g_sinkSawStatus);                                      /* the same text reached the system log */
    CHECK(g_sinkLines >= 5);
    CHECK(g_sinkErrLines == 0);                                  /* status lines are info, not errors */

    Mugen_ShowError("M.U.G.E.N Error", "Error detected.\n\nCan't find file x\nClipboard tail:\n");
    for (i = 0; i < 200 && g_sinkErrLines < 3; ++i) {
        usleep(10000);
    }
    CHECK(g_sinkErrLines >= 3);                                  /* title + one entry per line, as errors */
}

/* ---- time ---------------------------------------------------------------------------------------------------- */
static long long NowNs(clockid_t id)
{
    struct timespec ts;

    clock_gettime(id, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void TestTime(void)
{
    long long t0, wall, cpu0, cpu;
    int i;

    t0 = NowNs(CLOCK_MONOTONIC);
    Mugen_SleepNs(20 * 1000000LL);
    CHECK(NowNs(CLOCK_MONOTONIC) - t0 >= 20 * 1000000LL);
    Mugen_SleepNs(-5);                                           /* no-op */

    CHECK(Timer_Init() == 1);
    CHECK(g_timerFrequency == 1000000000LL);
    Timer_SetFps(60);
    CHECK(g_timerFrameTicks == 16666666LL);                      /* (2f + 1) / 60 / 2, the original's rounding */
    t0 = NowNs(CLOCK_MONOTONIC);
    cpu0 = NowNs(CLOCK_PROCESS_CPUTIME_ID);
    for (i = 0; i < 60; ++i) {
        Timer_WaitFrame();
        g_timerFrame++;
    }
    wall = NowNs(CLOCK_MONOTONIC) - t0;
    cpu = NowNs(CLOCK_PROCESS_CPUTIME_ID) - cpu0;
    Say("  60 frames at 60 fps: %.1f ms wall, %.1f ms cpu (%.0f%%)\n", wall / 1e6, cpu / 1e6, 100.0 * cpu / wall);
    /* the 60th call returns when frame 59 is due: 59 frame times, never early, a little late at worst */
    CHECK(wall >= 59 * g_timerFrameTicks && wall <= 59 * g_timerFrameTicks + 30000000LL);
    CHECK(Timer_GetFrameTimeLeft() <= 0 || Timer_GetFrameTimeLeft() < 16666667LL);
    {
        long long ticks = Timer_GetTicks();                      /* 1/100 s since Timer_Init: about 98 */
        CHECK(ticks >= 98 && ticks <= 105);
    }
    Timer_Reset();
    CHECK(Timer_GetFps() == -1.0f);
}

int main(void)
{
    char tmpl[] = "/tmp/mugen-p3a-XXXXXX";
    char *root;

    g_out = dup(1);
    CHECK(sizeof(char) == 1);
    {
        volatile unsigned char u = 0xFF;
        char c = (char)u;
        CHECK(c < 0);                                           /* -fsigned-char is in effect */
    }

    root = mkdtemp(tmpl);
    if (root == NULL) {
        Say("mkdtemp failed\n");
        return 2;
    }
    MakeTree(root);
    TestResolve(root);
    TestFopen(root);
    TestThreads();
    TestFileFind();
    TestTime();
    TestLog();

    Say("%d checks, %d failed\n", g_checks, g_failed);
    {
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", root);
        (void)!system(cmd);
    }
    return g_failed != 0;
}
