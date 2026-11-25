/*
 * Platform layer of the Android port (docs/android-port, phase P2).
 *
 * Include it before anything else (mugen.h does). It
 *  - selects MUGEN_PLATFORM_WIN32 (the MSVC/x86 reference build) or MUGEN_PLATFORM_ANDROID;
 *  - names the compiler-specific spellings the engine uses (calling conventions, __int64, __inline,
 *    __declspec(noreturn)) so that the sources never write them directly;
 *  - pulls in the OS headers: <windows.h> and friends on Windows, and on Android the small POSIX
 *    replacement of the Win32 / MSVC CRT surface the engine touches (platform_posix.h).
 *
 * On Windows every macro below expands to exactly the keyword it replaced, so the Windows build is
 * unchanged. Everything Android-specific stays behind MUGEN_PLATFORM_ANDROID or in this directory.
 */
#ifndef MUGEN_PLATFORM_H
#define MUGEN_PLATFORM_H

#if defined(_WIN32)
#  define MUGEN_PLATFORM_WIN32 1
#elif defined(MUGEN_PLATFORM_ANDROID) || defined(__ANDROID__)
#  ifndef MUGEN_PLATFORM_ANDROID
#    define MUGEN_PLATFORM_ANDROID 1
#  endif
#else
#  error "Unsupported platform: the engine builds for Win32 (MSVC, x86) or Android (arm64-v8a)"
#endif

#if defined(_MSC_VER)
#  define MUGEN_CDECL    __cdecl
#  define MUGEN_STDCALL  __stdcall
#  define MUGEN_INT64    __int64
#  define MUGEN_INLINE   __inline
#  define MUGEN_NORETURN __declspec(noreturn)
#else
/* Calling conventions do not exist on arm64 (clang would ignore them with a warning). */
#  define MUGEN_CDECL
#  define MUGEN_STDCALL
#  define MUGEN_INT64    long long
#  define MUGEN_INLINE   inline
#  define MUGEN_NORETURN __attribute__((noreturn))
#endif

#include <stdint.h>   /* intptr_t: a pointer-sized integer (int on the 32-bit build) */

#if defined(MUGEN_PLATFORM_WIN32)
#  include "platform_win32.h"
#else
#  include "platform_posix.h"
#endif

#endif /* MUGEN_PLATFORM_H */
