/*
 * Application environment of the POSIX / Android platform layer (phase P3a): data directory (D-005), system log,
 * stdout/stderr capture, the fatal error hook and sleep. Plain POSIX plus <android/log.h> on Android; also builds
 * on the host for tests/platform.
 */
#include "platform.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#undef fopen

#define LOG_TAG "mugen"

/* ---- sleep ---------------------------------------------------------------------------------------------------- */
void Mugen_SleepNs(int64_t ns)
{
    struct timespec ts;

    if (ns <= 0) {
        return;
    }
    ts.tv_sec = (time_t)(ns / 1000000000LL);
    ts.tv_nsec = (long)(ns % 1000000000LL);
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
    }
}

/* ---- data directory ------------------------------------------------------------------------------------------- */
static char g_dataDir[4096];

int Mugen_SetDataDir(const char *dir)
{
    if (dir == NULL || dir[0] == '\0' || strlen(dir) >= sizeof(g_dataDir)) {
        return 0;
    }
    if (chdir(dir) != 0) {
        return 0;
    }
    strcpy(g_dataDir, dir);
    Mugen_PathCacheClear();
    return 1;
}

static volatile int g_startMode = MUGEN_START_ENGINE;

void Mugen_SetStartMode(int mode)
{
    g_startMode = mode;
}

int Mugen_GetStartMode(void)
{
    return g_startMode;
}

const char *Mugen_GetDataDir(void)
{
    return g_dataDir;
}

/* ---- system log ----------------------------------------------------------------------------------------------- */
void Mugen_LogWrite(int priority, const char *text)
{
#if defined(__ANDROID__)
    int prio = priority >= 2 ? ANDROID_LOG_ERROR : priority == 1 ? ANDROID_LOG_WARN : ANDROID_LOG_INFO;
    __android_log_write(prio, LOG_TAG, text);
#else
    fprintf(stderr, "[%s] %s\n", priority >= 2 ? "E" : priority == 1 ? "W" : "I", text);
#endif
}

/* ---- stdout / stderr to the log ------------------------------------------------------------------------------- */
typedef struct StdioPipe {
    int read_fd;
    int orig_fd;        /* 1 or 2 */
} StdioPipe;

static MugenLineSink g_sink;
static int g_stdioStarted;

static void DeliverLine(int fd, const char *line)
{
    if (g_sink != NULL) {
        g_sink(fd, line);
    } else {
        Mugen_LogWrite(fd == 2 ? 2 : 0, line);
    }
}

static void *StdioThread(void *arg)
{
    StdioPipe *p = (StdioPipe *)arg;
    char buf[2048];
    size_t n = 0;
    char c;

    for (;;) {
        ssize_t r = read(p->read_fd, &c, 1);

        if (r < 0 && errno == EINTR) {
            continue;
        }
        if (r <= 0) {
            break;
        }
        if (c == '\n' || n == sizeof(buf) - 1) {
            buf[n] = '\0';
            if (n > 0) {
                DeliverLine(p->orig_fd, buf);
            }
            n = 0;
            if (c != '\n') {
                buf[n++] = c;
            }
        } else {
            buf[n++] = c;
        }
    }
    if (n > 0) {
        buf[n] = '\0';
        DeliverLine(p->orig_fd, buf);
    }
    return NULL;
}

int Mugen_StdioToLog(MugenLineSink sink)
{
    static StdioPipe pipes[2];
    int fds[2], i;

    if (g_stdioStarted) {
        return 0;
    }
    g_sink = sink;
    for (i = 0; i < 2; ++i) {
        pthread_t th;

        if (pipe(fds) != 0) {
            return 0;
        }
        setvbuf(i == 0 ? stdout : stderr, NULL, _IOLBF, 0);
        if (dup2(fds[1], i + 1) < 0) {
            close(fds[0]);
            close(fds[1]);
            return 0;
        }
        close(fds[1]);
        pipes[i].read_fd = fds[0];
        pipes[i].orig_fd = i + 1;
        if (pthread_create(&th, NULL, StdioThread, &pipes[i]) != 0) {
            return 0;
        }
        pthread_detach(th);
    }
    g_stdioStarted = 1;
    return 1;
}

/* ---- fatal error screen --------------------------------------------------------------------------------------- */
static MugenErrorHandler g_errorHandler;

void Mugen_SetErrorHandler(MugenErrorHandler handler)
{
    g_errorHandler = handler;
}

void Mugen_ShowError(const char *title, const char *text)
{
    const char *line = text;

    Mugen_LogWrite(2, title);
    /* logcat truncates long entries (about 4 KB) and the text is a multi-line clipboard dump: one entry per line */
    while (*line != '\0') {
        const char *nl = strchr(line, '\n');
        size_t len = nl != NULL ? (size_t)(nl - line) : strlen(line);
        char buf[1024];

        if (len >= sizeof(buf)) {
            len = sizeof(buf) - 1;
        }
        memcpy(buf, line, len);
        buf[len] = '\0';
        if (len > 0) {
            Mugen_LogWrite(2, buf);
        }
        line += len;
        if (*line == '\n') {
            ++line;
        } else if (nl == NULL) {
            break;
        } else {
            /* line longer than buf: continue with the rest */
        }
    }
    if (g_errorHandler != NULL) {
        g_errorHandler(title, text);
    }
}
