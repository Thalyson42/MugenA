/*
 * P2 STUB, Android build only: the SDL 1.2 configuration used while compiling the engine for arm64.
 *
 * The SDL 1.2 headers in third_party/sdk/include/SDL are those of the Windows SDK. For any platform other than
 * Windows their SDL_config.h picks SDL_config_minimal.h, which typedefs int8_t..uintptr_t and size_t itself
 * (unsigned int) and so clashes with <stdint.h>/<stddef.h> on a 64-bit target. Defining the include guard of
 * SDL_config.h first (platform_posix.h includes this file before any SDL header) replaces it with the
 * definitions below. Temporary: D-002 (sdl12-compat or SDL2) brings its own headers in P3 and this file goes away.
 * See D-013 in docs/android-port/03-decisoes.md.
 */
#ifndef _SDL_config_h
#define _SDL_config_h

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define HAVE_LIBC          1
#define HAVE_STDINT_H      1
#define HAVE_STDIO_H       1
#define HAVE_STDLIB_H      1
#define HAVE_STRING_H      1
#define HAVE_CTYPE_H       1
#define HAVE_MATH_H        1
#define HAVE_SIGNAL_H      1
#define HAVE_INTTYPES_H    1
#define HAVE_MALLOC        1
#define HAVE_CALLOC        1
#define HAVE_REALLOC       1
#define HAVE_FREE          1
#define HAVE_GETENV        1
#define HAVE_PUTENV        1
#define HAVE_QSORT         1
#define HAVE_ABS           1
#define HAVE_MEMSET        1
#define HAVE_MEMCPY        1
#define HAVE_MEMMOVE       1
#define HAVE_MEMCMP        1
#define HAVE_STRLEN        1
#define HAVE_STRCHR        1
#define HAVE_STRRCHR       1
#define HAVE_STRSTR        1
#define HAVE_STRTOL        1
#define HAVE_STRTOUL       1
#define HAVE_STRTOD        1
#define HAVE_ATOI          1
#define HAVE_ATOF          1
#define HAVE_STRCMP        1
#define HAVE_STRNCMP       1
#define HAVE_STRCASECMP    1
#define HAVE_STRNCASECMP   1
#define HAVE_SNPRINTF      1
#define HAVE_VSNPRINTF     1
#define HAVE_SSCANF        1

#endif /* _SDL_config_h */
