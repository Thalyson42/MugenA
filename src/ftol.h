#ifndef MUGEN_FTOL_H
#define MUGEN_FTOL_H


#include "platform/platform.h"
/*
 * The original's two float to int conversions.
 *
 * Casts to int go through __ftol2_sse (0x4DC850): with SSE2 (any machine
 * the game runs on today) that is cvttsd2si, which truncates and gives
 * 0x80000000 for NaN and values out of the int range. A plain cast in this
 * build's CRT makes the same choice; the range check keeps the result
 * independent of the compiler.
 */
static MUGEN_INLINE int Ftol(double x)
{
    if (!(x > -2147483649.0 && x < 2147483648.0)) {
        return (int)0x80000000u;
    }
    return (int)x;
}

/*
 * The 64-bit truncating conversion of the original (x87 fistp, __ftol2): toward zero, and 0x8000000000000000
 * for NaN and for values outside the int64 range. A plain C cast gives that on x86, but C leaves the out-of-range
 * case undefined and arm64 (fcvtzs) saturates instead (R2, docs/android-port/01-analise-do-repo.md), so the range
 * check is spelled out here and the result is the same on every compiler and CPU.
 */
static MUGEN_INLINE MUGEN_INT64 Ftol64(double x)
{
    if (!(x >= -9223372036854775808.0 && x < 9223372036854775808.0)) {
        return (MUGEN_INT64)(-9223372036854775807LL - 1);
    }
    return (MUGEN_INT64)x;
}

/*
 * The conversion the original inlines in a few places (software scaling,
 * parallax and the sound resampler): a truncating 64-bit fistp of which only
 * the low 32 bits are kept.
 */
static MUGEN_INLINE int Trunc64(double x)
{
    return (int)(unsigned int)((unsigned MUGEN_INT64)Ftol64(x) & 0xFFFFFFFFu);
}

#endif /* MUGEN_FTOL_H */
