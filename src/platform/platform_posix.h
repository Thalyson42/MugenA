/*
 * POSIX / Android side of the platform layer (phase P2): the part of the Win32 API and of the MSVC CRT that
 * the engine uses, under the Win32 names, so that the engine sources keep one spelling for both builds.
 *
 * This is the P2 "compile it" layer. It is deliberately thin and is replaced piece by piece in P3 (time, files,
 * dialogs, logging) and P4 (plugin loading). What each piece does and how faithful it is:
 *   - time:    CLOCK_MONOTONIC. timeGetTime wraps at 32 bits like the original; QueryPerformanceCounter counts ns.
 *   - Sleep:   nanosleep (Sleep(0) yields).
 *   - modules: dlopen/dlsym (the BGM plugins of the original are .dll files and will not load; see P4, D-006).
 *   - CRT:     _stricmp & co. map to their POSIX names; _snprintf/_vsnprintf keep the MSVC contract
 *              (no terminator when the text does not fit, -1 on truncation).
 * Nothing here is compiled into the Windows build.
 */
#ifndef MUGEN_PLATFORM_POSIX_H
#define MUGEN_PLATFORM_POSIX_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <dlfcn.h>

#include "sdl_config_posix.h"   /* before any SDL header; see that file */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Win32 types ------------------------------------------------------------------------------------------ */
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t  BYTE;
typedef int      BOOL;
typedef int32_t  LONG;      /* Win32 LONG is 32 bits (a C long on Windows, not on LP64) */
typedef void    *HMODULE;
typedef void    *HINSTANCE;
typedef void    *HWND;

/* The saved window geometry of the fullscreen toggle (g_savedWindowRect...). The window itself is P3. */
typedef struct { LONG left, top, right, bottom; } RECT;

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* The original's path buffers are 260 bytes (Win32 MAX_PATH); kept so truncation behaves the same. */
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

typedef union {
    struct { uint32_t LowPart; int32_t HighPart; } u;
    int64_t QuadPart;
} LARGE_INTEGER;

/* ---- time and sleep --------------------------------------------------------------------------------------- */
DWORD Mugen_TimeGetTime(void);
BOOL  Mugen_QueryPerformanceCounter(LARGE_INTEGER *out);
BOOL  Mugen_QueryPerformanceFrequency(LARGE_INTEGER *out);
void  Mugen_Sleep(DWORD ms);

#define timeGetTime()                  Mugen_TimeGetTime()
#define timeBeginPeriod(ms)            ((void)(ms))
#define timeEndPeriod(ms)              ((void)(ms))
#define QueryPerformanceCounter(p)     Mugen_QueryPerformanceCounter(p)
#define QueryPerformanceFrequency(p)   Mugen_QueryPerformanceFrequency(p)
#define Sleep(ms)                      Mugen_Sleep(ms)

/* ---- system folders and locale (system.c, config.c) ------------------------------------------------------- */
#define S_OK                  0
#define CSIDL_FONTS           0x0014
#define LOCALE_USER_DEFAULT   0x0400
#define LOCALE_SISO639LANGNAME 0x0059

long Mugen_SHGetFolderPathA(void *hwnd, int csidl, void *token, DWORD flags, char *path);
int  Mugen_GetLocaleInfoA(DWORD locale, DWORD type, char *buf, int size);

#define SHGetFolderPathA(hwnd, csidl, token, flags, path) Mugen_SHGetFolderPathA(hwnd, csidl, token, flags, path)
#define GetLocaleInfoA(locale, type, buf, size)           Mugen_GetLocaleInfoA(locale, type, buf, size)

/* ---- dynamic libraries ------------------------------------------------------------------------------------ */
#define LoadLibraryA(name)             dlopen((name), RTLD_NOW)
#define GetProcAddress(lib, name)      dlsym((lib), (name))
#define FreeLibrary(lib)               dlclose(lib)

/* ---- MSVC CRT names --------------------------------------------------------------------------------------- */
#define _stricmp   strcasecmp
#define _strnicmp  strncasecmp
#define _isnan     isnan
#define _chdir     chdir
#define _getcwd    getcwd

char *Mugen_strlwr(char *s);
int   Mugen_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int   Mugen_snprintf(char *buf, size_t size, const char *fmt, ...);

#define _strlwr    Mugen_strlwr
#define _vsnprintf Mugen_vsnprintf
#define _snprintf  Mugen_snprintf

#ifdef __cplusplus
}
#endif

#endif /* MUGEN_PLATFORM_POSIX_H */
