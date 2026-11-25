/* Win32 side of the platform layer: the headers the engine always used, nothing else. */
#ifndef MUGEN_PLATFORM_WIN32_H
#define MUGEN_PLATFORM_WIN32_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>   /* timeGetTime, timeBeginPeriod, timeEndPeriod */
#include <direct.h>     /* _chdir, _getcwd */

#endif /* MUGEN_PLATFORM_WIN32_H */
