/*
 * Bring-up watchdog (P3b): the engine calls Mugen_WatchdogBeat once per presented frame. When no frame has been
 * presented for 3 s (and again at 8 s), every thread of the process is sent a real-time signal and logs its own native
 * backtrace (pc relative to its library, so it can be symbolized against the unstripped libmugen.so of the same build,
 * see android/tools/symbolize-addrs.txt and the CI step), plus its kernel state and wait channel. Diagnostics only:
 * it never changes the engine's behaviour. Log tag: mugen-wd.
 * REMOVER-NO-RELEASE (docs/android-port/05-diagnosticos-a-remover.md) item 3.
 */
#define _GNU_SOURCE
#include <android/log.h>
#include <dirent.h>
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unwind.h>

#define WD_TAG "mugen-wd"
#define WD_SIG (SIGRTMIN + 3)
#define WD_MAX_FRAMES 28

static atomic_uint g_beats;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

struct wd_bt {
    int n;
};

static _Unwind_Reason_Code wd_unwind(struct _Unwind_Context *ctx, void *arg)
{
    struct wd_bt *b = (struct wd_bt *)arg;
    uintptr_t pc = _Unwind_GetIP(ctx);
    Dl_info di;
    const char *mod = "?";
    const char *sym = "";
    uintptr_t off = pc;

    if (pc == 0 || b->n >= WD_MAX_FRAMES) {
        return _URC_END_OF_STACK;
    }
    if (dladdr((void *)pc, &di) && di.dli_fbase != NULL) {
        off = pc - (uintptr_t)di.dli_fbase;
        if (di.dli_fname != NULL) {
            const char *slash = strrchr(di.dli_fname, '/');
            mod = slash != NULL ? slash + 1 : di.dli_fname;
        }
        if (di.dli_sname != NULL) {
            sym = di.dli_sname;
        }
    }
    __android_log_print(ANDROID_LOG_WARN, WD_TAG, "  #%02d pc %016lx %s %s", b->n, (unsigned long)off, mod, sym);
    b->n++;
    return _URC_NO_REASON;
}

static void wd_on_signal(int sig)
{
    struct wd_bt b = { 0 };
    char name[20] = "?";

    (void)sig;
    prctl(PR_GET_NAME, name, 0, 0, 0);
    __android_log_print(ANDROID_LOG_WARN, WD_TAG, "tid %d (%s) backtrace:", (int)syscall(SYS_gettid), name);
    _Unwind_Backtrace(wd_unwind, &b);
}

static void wd_read_line(const char *path, char *out, size_t n)
{
    FILE *f = fopen(path, "r");
    out[0] = 0;
    if (f != NULL) {
        if (fgets(out, (int)n, f) == NULL) {
            out[0] = 0;
        }
        fclose(f);
    }
    out[strcspn(out, "\n")] = 0;
}

static void wd_dump_all(void)
{
    DIR *d = opendir("/proc/self/task");
    struct dirent *e;
    int self = (int)syscall(SYS_gettid);
    int pid = (int)getpid();

    if (d == NULL) {
        return;
    }
    while ((e = readdir(d)) != NULL) {
        int tid = atoi(e->d_name);
        char path[64], stat[256], wchan[96], comm[32], *st;
        if (tid <= 0 || tid == self) {
            continue;
        }
        snprintf(path, sizeof(path), "/proc/self/task/%d/stat", tid);
        wd_read_line(path, stat, sizeof(stat));
        snprintf(path, sizeof(path), "/proc/self/task/%d/wchan", tid);
        wd_read_line(path, wchan, sizeof(wchan));
        snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
        wd_read_line(path, comm, sizeof(comm));
        st = strrchr(stat, ')');
        __android_log_print(ANDROID_LOG_WARN, WD_TAG, "thread %d (%s) state %c wchan '%s'", tid, comm,
                            st != NULL && st[1] == ' ' ? st[2] : '?', wchan);
        syscall(SYS_tgkill, pid, tid, WD_SIG);
        usleep(150 * 1000);
    }
    closedir(d);
}

static void *wd_thread(void *arg)
{
    unsigned last = atomic_load(&g_beats);
    int stall = 0;

    (void)arg;
    prctl(PR_SET_NAME, "mugen-watchdog", 0, 0, 0);
    for (;;) {
        unsigned now;
        sleep(1);
        now = atomic_load(&g_beats);
        if (now != last) {
            if (stall >= 3) {
                __android_log_print(ANDROID_LOG_WARN, WD_TAG, "frames resumed after %d s", stall);
            }
            last = now;
            stall = 0;
            continue;
        }
        stall++;
        if (stall == 3 || stall == 8) {
            __android_log_print(ANDROID_LOG_WARN, WD_TAG, "no frame presented for %d s (frame %u): dumping threads", stall,
                                now);
            wd_dump_all();
            __android_log_print(ANDROID_LOG_WARN, WD_TAG, "dump done");
        }
    }
    return NULL;
}

static void wd_start(void)
{
    struct sigaction sa;
    pthread_t t;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = wd_on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(WD_SIG, &sa, NULL);
    if (pthread_create(&t, NULL, wd_thread, NULL) == 0) {
        pthread_detach(t);
    }
}

void Mugen_WatchdogBeat(void)
{
    atomic_fetch_add(&g_beats, 1);
    pthread_once(&g_once, wd_start);
}
