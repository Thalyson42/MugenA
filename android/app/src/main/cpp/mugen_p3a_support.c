/*
 * TEMPORARY (phase P3a, D-014): what the engine files of the mugen_p3a library (timer.c, log.c, file.c, textring.c,
 * util.c) expect from the rest of the engine, so that they can run on the device before the whole engine is linked.
 * In P3b the whole of mugen_core is linked instead (globals.c, error.c, ini.c, ...) and this file and mugen_p3a go away.
 * The same definitions exist, for the same reason, in tests/platform/test_main.c.
 */
#include "platform/platform.h"
#include "mugen.h"

int g_timerPerformance, g_timerFpsInterval, g_timerFpsFrames, g_timerFrameRate, g_timerInitialized, g_timerResyncs;
int g_frameRate, g_timerTicksPerSecond;
long long g_timerFrameTicks, g_timerStart, g_timerFpsStart, g_timerFrameStart, g_timerFrame, g_timerFrequency;
double g_timerMeasuredFps;
Log *g_log, *g_matchLog;
GameVars *g_game;
int g_outOfMemory;

char g_p3aLastError[256];

void Error_Printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(g_p3aLastError, sizeof(g_p3aLastError), fmt, ap);
    va_end(ap);
    Mugen_LogWrite(1, g_p3aLastError);
}

/* file.c also holds Ini_ReadString, which is not used in P3a */
char *Ini_GetValue(Ini *ini, const char *key) { (void)ini; (void)key; return NULL; }
int Str_Unquote(char *s) { (void)s; return 0; }
