/* POSIX / Android implementation of platform_posix.h (phase P2). Not part of the Windows build. */
#include "platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>

static int64_t MonotonicNs(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

DWORD Mugen_TimeGetTime(void)
{
    return (DWORD)(MonotonicNs() / 1000000LL);      /* milliseconds, wraps at 32 bits like timeGetTime */
}

BOOL Mugen_QueryPerformanceCounter(LARGE_INTEGER *out)
{
    out->QuadPart = MonotonicNs();
    return TRUE;
}

BOOL Mugen_QueryPerformanceFrequency(LARGE_INTEGER *out)
{
    out->QuadPart = 1000000000LL;
    return TRUE;
}

void Mugen_Sleep(DWORD ms)
{
    struct timespec ts;

    if (ms == 0) {
        sched_yield();
        return;
    }
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
}

char *Mugen_strlwr(char *s)
{
    char *p;

    for (p = s; *p != '\0'; ++p) {
        *p = (char)tolower((unsigned char)*p);
    }
    return s;
}

/*
 * MSVC's _vsnprintf: writes at most size bytes and returns the length when the text fits in size bytes
 * (a terminator is written only if there is room), or -1 when it does not fit (size bytes written, no terminator).
 */
int Mugen_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    va_list copy;
    int n;
    char *tmp;

    va_copy(copy, ap);
    n = vsnprintf(buf, size, fmt, copy);
    va_end(copy);
    if (n < 0) {
        return -1;
    }
    if ((size_t)n < size) {
        return n;                       /* fits with its terminator */
    }
    /* Does not fit with a terminator: MSVC fills all `size` bytes. vsnprintf kept size-1 and a NUL. */
    if (size == 0) {
        return (size_t)n == 0 ? 0 : -1;
    }
    tmp = (char *)malloc((size_t)n + 1);
    if (tmp != NULL) {
        va_copy(copy, ap);
        vsnprintf(tmp, (size_t)n + 1, fmt, copy);
        va_end(copy);
        memcpy(buf, tmp, size);
        free(tmp);
    }
    return (size_t)n == size ? n : -1;
}

int Mugen_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = Mugen_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

/* The Windows font folder. On Android the system fonts live in /system/fonts. TODO(P3): per-app font directory. */
long Mugen_SHGetFolderPathA(void *hwnd, int csidl, void *token, DWORD flags, char *path)
{
    (void)hwnd;
    (void)token;
    (void)flags;
    if (csidl != CSIDL_FONTS) {
        return -1;
    }
    strcpy(path, "/system/fonts");
    return S_OK;
}

/*
 * ISO 639 language of the user ("pt"), as the two lowercase letters at the start of LC_ALL / LC_MESSAGES / LANG.
 * Returns the size written with the terminator (3), or 0 when unknown. Android does not set these variables for
 * native code, so this normally finds nothing and the engine falls back to "en". TODO(P3/P11): ask the activity.
 */
int Mugen_GetLocaleInfoA(DWORD locale, DWORD type, char *buf, int size)
{
    static const char *const vars[] = { "LC_ALL", "LC_MESSAGES", "LANG" };
    size_t i;

    (void)locale;
    if (type != LOCALE_SISO639LANGNAME || size < 3) {
        return 0;
    }
    for (i = 0; i < sizeof(vars) / sizeof(vars[0]); ++i) {
        const char *v = getenv(vars[i]);

        if (v != NULL && v[0] >= 'a' && v[0] <= 'z' && v[1] >= 'a' && v[1] <= 'z') {
            buf[0] = v[0];
            buf[1] = v[1];
            buf[2] = '\0';
            return 3;
        }
    }
    return 0;
}
