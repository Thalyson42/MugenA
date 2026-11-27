/*
 * Android diagnostic (P3b M3, D-017): the System renderer shows small dark dashes (about 8x2 px) at fixed places (the
 * corners of the title menu, the ground between the fighters) that the PC build does not show. Linked with
 * -Wl,--wrap=Sprite_DrawSoft8, this logs once per sprite every draw whose sprite is tiny (width <= 16, height <= 3):
 * which sprite (slot, group, number), its size and axis, where and how it is drawn, and its first pixels with their
 * RGB565 colours. It forwards every call unchanged. Output goes to mugen.log / logcat, prefix "spritediag".
 * Delete it (and the link option in cmake/MugenAndroid.cmake) once the dashes are explained.
 */
#include <stdlib.h>
#include "mugen.h"

void __real_Sprite_DrawSoft8(const DrawParams *dp, SpriteSet *set, int sprite);

#define DIAG_MAX 80

static void diag_note(const DrawParams *dp, SpriteSet *set, int sprite)
{
    static const void *seen_set[DIAG_MAX];
    static int seen_idx[DIAG_MAX];
    static int count = 0;
    const KeyedArray *a;
    const Sprite *spr;
    const unsigned char *px;
    const unsigned short *pal = NULL;
    int i, w;

    if (set == NULL || set->sprites == NULL) {
        return;
    }
    a = set->sprites;
    if (sprite < 0 || sprite >= a->capacity || a->slots == NULL || !a->slots[sprite].used) {
        return;
    }
    spr = (const Sprite *)(a->data + a->elem_size * sprite);
    if (spr->w <= 0 || spr->h <= 0 || spr->w > 16 || spr->h > 3) {
        return;
    }
    for (i = 0; i < count; i++) {
        if (seen_set[i] == (const void *)set && seen_idx[i] == sprite) {
            return;
        }
    }
    if (count >= DIAG_MAX) {
        return;
    }
    seen_set[count] = set;
    seen_idx[count] = sprite;
    count++;

    px = (const unsigned char *)Sprite_GetPixels(set, sprite);
    if (spr->pal_index >= 0) {
        Palette *p = SpriteSet_GetPaletteConverted(set, spr->pal_index);
        if (p != NULL) {
            pal = p->rgb565;
        }
    }
    Log_Printf("\nspritediag: set %p slot %d key %d,%d size %dx%d axis %d,%d bpp %d fmt %d pal %d | draw at %.1f,%.1f "
               "scale %.2f,%.2f blend %d masked %d flags %x\n",
               (void *)set, sprite, a->slots[sprite].key1, a->slots[sprite].key2, spr->w, spr->h, spr->x, spr->y,
               spr->bpp, spr->format, spr->pal_index, dp->x, dp->y, dp->scale_x, dp->scale_y, dp->blend, dp->masked,
               dp->flags);
    if (px != NULL && spr->rows != NULL) {
        int r;
        for (r = 0; r < spr->h; r++) {
            const unsigned char *row = px + spr->rows[r];
            w = spr->w;
            Log_Printf("spritediag:   row %d:", r);
            for (i = 0; i < w; i++) {
                if (pal != NULL) {
                    Log_Printf(" %d=%04x", row[i], (unsigned)pal[row[i]]);
                } else {
                    Log_Printf(" %d", row[i]);
                }
            }
            Log_Printf("\n");
        }
    }
}

/* ---- pixel watch ----
 * Mugen_DiagScanFrame (called from Gfx_AndroidDebugFrame on the finished frame) looks for short horizontal runs of
 * one RGB565 colour whose left and right neighbours match each other (a "dash" in a uniform area) and remembers up to
 * WATCH_MAX of them. From then on every wrapped draw call compares those pixels of the render target before and
 * after, and logs the call that changes them ("spritediag: watch ..."), so the last line per frame names the
 * culprit. Only the first WATCH_LOG lines are logged. */
#define WATCH_MAX 6
#define WATCH_LOG 600
static int g_watchN = 0;
static int g_watchX[WATCH_MAX], g_watchY[WATCH_MAX];
static unsigned short g_watchBefore[WATCH_MAX];
static int g_watchLogged = 0;
static int g_scanDone = 0;

static int g_watchSince = 0;   /* frame at which the watch points were set */

static int luma565(unsigned int c)
{
    return (int)(((c >> 11) & 31) * 24 + ((c >> 5) & 63) * 12 + (c & 31) * 9) / 10;
}

/* 1 on the frames where the finished buffer should be scanned (gfx.c locks the surface only then). */
int Mugen_DiagWantScan(int frame)
{
    return !g_scanDone && frame >= 20 && frame % 20 == 0;
}

/* Called every frame from Gfx_AndroidDebugFrame; the scan runs only when Mugen_DiagWantScan said so (pixels != NULL). */
void Mugen_DiagScanFrame(const unsigned char *pixels, int w, int h, int pitch, int frame)
{
    int x, y, found = 0;

    if (g_scanDone && frame > g_watchSince + 3) {
        g_watchLogged = WATCH_LOG;   /* stop logging three frames after the dashes were found */
        return;
    }
    if (pixels == NULL) {
        return;
    }
    for (y = 1; y < h - 4 && found < 12; y++) {
        const unsigned short *row = (const unsigned short *)(pixels + (size_t)y * pitch);
        for (x = 2; x < w - 14 && found < 12; x++) {
            int len = 1, n, xm;
            unsigned short c = row[x];
            if (row[x - 1] == c) {
                continue;
            }
            while (len < 14 && row[x + len] == c) {
                len++;
            }
            if (len < 4 || len > 12 || row[x - 1] != row[x + len]) {
                continue;
            }
            n = luma565(row[x - 1]);
            xm = x + len / 2;
            /* clearly darker than its surroundings, and the rows above and below are the surroundings again */
            if (luma565(c) + 20 > n ||
                abs(luma565(*(const unsigned short *)(pixels + (size_t)(y - 1) * pitch + xm * 2)) - n) > 8 ||
                abs(luma565(*(const unsigned short *)(pixels + (size_t)(y + 3) * pitch + xm * 2)) - n) > 8) {
                continue;
            }
            Log_Printf("spritediag: frame %d dash at x=%d y=%d len=%d colour %04x around %04x\n", frame, x, y, len,
                       (unsigned)c, (unsigned)row[x - 1]);
            if (g_watchN < WATCH_MAX) {
                g_watchX[g_watchN] = xm;
                g_watchY[g_watchN] = y;
                g_watchN++;
            }
            found++;
            x += len;
        }
    }
    if (found > 0) {
        g_scanDone = 1;
        g_watchSince = frame;
        g_watchLogged = 0;
        Log_Printf("spritediag: frame %d scan found %d dashes\n", frame, found);
    }
}

static unsigned short *watch_px(int i)
{
    const Sprite *spr;
    const KeyedArray *a;
    unsigned char *base;

    if (g_renderTarget == NULL || g_renderTarget->sprites == NULL) {
        return NULL;
    }
    a = g_renderTarget->sprites;
    if (g_renderTargetSprite < 0 || g_renderTargetSprite >= a->capacity) {
        return NULL;
    }
    spr = (const Sprite *)(a->data + a->elem_size * g_renderTargetSprite);
    base = (unsigned char *)Sprite_GetPixels(g_renderTarget, g_renderTargetSprite);
    if (base == NULL || spr->rows == NULL || spr->bpp != 16 || g_watchY[i] >= spr->h || g_watchX[i] >= spr->w) {
        return NULL;
    }
    return (unsigned short *)(base + spr->rows[g_watchY[i]] + g_watchX[i] * 2);
}

static void watch_before(void)
{
    int i;
    if (g_watchLogged >= WATCH_LOG) {
        return;
    }
    for (i = 0; i < g_watchN; i++) {
        unsigned short *p = watch_px(i);
        g_watchBefore[i] = p != NULL ? *p : 0;
    }
}

static void watch_after(const char *what, int a, int b, int c, int d, int e)
{
    int i;
    if (g_watchLogged >= WATCH_LOG) {
        return;
    }
    for (i = 0; i < g_watchN; i++) {
        unsigned short *p = watch_px(i);
        if (p != NULL && *p != g_watchBefore[i]) {
            g_watchLogged++;
            Log_Printf("spritediag: watch %d (%d,%d) %04x -> %04x by %s args %d %d %d %d %d\n", i, g_watchX[i],
                       g_watchY[i], (unsigned)g_watchBefore[i], (unsigned)*p, what, a, b, c, d, e);
        }
    }
}

/* For a sprite draw that changed a watched pixel: which source pixel it was (local coordinates, no flip assumed) and the
 * sprite's data around it, once per (set, slot, watch). */
static void watch_detail(const DrawParams *dp, SpriteSet *set, int sprite)
{
    static const void *seen_set[24];
    static int seen_key[24];
    static int nseen = 0;
    const KeyedArray *a;
    const Sprite *spr;
    const unsigned char *px;
    const unsigned short *pal = NULL;
    int i;

    if (set == NULL || set->sprites == NULL) {
        return;
    }
    a = set->sprites;
    if (sprite < 0 || sprite >= a->capacity || a->slots == NULL || !a->slots[sprite].used) {
        return;
    }
    spr = (const Sprite *)(a->data + a->elem_size * sprite);
    px = (const unsigned char *)Sprite_GetPixels(set, sprite);
    if (px == NULL || spr->rows == NULL) {
        return;
    }
    if (spr->pal_index >= 0) {
        Palette *p = SpriteSet_GetPaletteConverted(set, spr->pal_index);
        if (p != NULL) {
            pal = p->rgb565;
        }
    }
    for (i = 0; i < g_watchN; i++) {
        unsigned short *wp = watch_px(i);
        int lx, ly, k, r, dup = 0;

        if (wp == NULL || *wp == g_watchBefore[i]) {
            continue;
        }
        lx = g_watchX[i] - ((int)dp->x - spr->x);
        ly = g_watchY[i] - ((int)dp->y - spr->y);
        if (lx < 0 || ly < 0 || lx >= spr->w || ly >= spr->h) {
            continue;
        }
        for (k = 0; k < nseen; k++) {
            dup |= (seen_set[k] == (const void *)set && seen_key[k] == sprite * 8 + i);
        }
        if (dup || nseen >= 24) {
            continue;
        }
        seen_set[nseen] = set;
        seen_key[nseen++] = sprite * 8 + i;
        Log_Printf("spritediag: DETAIL watch %d (%d,%d) <- set %p slot %d key %d,%d size %dx%d axis %d,%d bpp %d fmt %d "
                   "pal %d draw %.1f,%.1f scale %.2f,%.2f flags %x local %d,%d result %04x\n",
                   i, g_watchX[i], g_watchY[i], (void *)set, sprite, a->slots[sprite].key1, a->slots[sprite].key2,
                   spr->w, spr->h, spr->x, spr->y, spr->bpp, spr->format, spr->pal_index, dp->x, dp->y, dp->scale_x,
                   dp->scale_y, dp->flags, lx, ly, (unsigned)*wp);
        for (r = ly - 2; r <= ly + 2; r++) {
            const unsigned char *row;
            int c;
            if (r < 0 || r >= spr->h) {
                continue;
            }
            row = px + spr->rows[r];
            Log_Printf("spritediag:   row %d x%d..%d:", r, lx - 4 < 0 ? 0 : lx - 4, lx + 10);
            for (c = lx - 4; c <= lx + 10; c++) {
                if (c < 0 || c >= spr->w) {
                    continue;
                }
                if (pal != NULL) {
                    Log_Printf(" %d=%04x", row[c], (unsigned)pal[row[c]]);
                } else {
                    Log_Printf(" %d", row[c]);
                }
            }
            Log_Printf("\n");
        }
    }
}

void __wrap_Sprite_DrawSoft8(const DrawParams *dp, SpriteSet *set, int sprite)
{
    diag_note(dp, set, sprite);
    watch_before();
    __real_Sprite_DrawSoft8(dp, set, sprite);
    watch_detail(dp, set, sprite);
    watch_after("DrawSoft8", sprite, (int)dp->x, (int)dp->y, dp->blend, (int)(dp->scale_x * 100));
}

void __real_Sprite_DrawSoft(SpriteSet *set, int sprite, const DrawParams *dp);
void __wrap_Sprite_DrawSoft(SpriteSet *set, int sprite, const DrawParams *dp)
{
    watch_before();
    __real_Sprite_DrawSoft(set, sprite, dp);
    watch_after("DrawSoft", sprite, (int)dp->x, (int)dp->y, dp->blend, (int)(dp->scale_x * 100));
}

void __real_Gfx_SoftClear(unsigned int color);
void __wrap_Gfx_SoftClear(unsigned int color)
{
    watch_before();
    __real_Gfx_SoftClear(color);
    watch_after("SoftClear", (int)color, 0, 0, 0, 0);
}

#define WRAP_RECT(NAME)                                                                                       \
    void __real_##NAME(int x1, int y1, int x2, int y2, unsigned int color);                                  \
    void __wrap_##NAME(int x1, int y1, int x2, int y2, unsigned int color)                                   \
    {                                                                                                         \
        watch_before();                                                                                       \
        __real_##NAME(x1, y1, x2, y2, color);                                                                 \
        watch_after(#NAME, x1, y1, x2, y2, (int)color);                                                       \
    }
WRAP_RECT(Gfx_SoftDrawRect)
WRAP_RECT(Gfx_SoftDrawRectAdd)
WRAP_RECT(Gfx_SoftDrawRectSub)
WRAP_RECT(Gfx_SoftFillRect)
WRAP_RECT(Gfx_SoftFillRectAdd)
WRAP_RECT(Gfx_SoftFillRectSub)

void __real_Gfx_SoftHLine(int x1, int x2, int y, unsigned int color);
void __wrap_Gfx_SoftHLine(int x1, int x2, int y, unsigned int color)
{
    watch_before();
    __real_Gfx_SoftHLine(x1, x2, y, color);
    watch_after("SoftHLine", x1, x2, y, (int)color, 0);
}

void __real_Gfx_SoftVLine(int y1, int y2, int x, unsigned int color);
void __wrap_Gfx_SoftVLine(int y1, int y2, int x, unsigned int color)
{
    watch_before();
    __real_Gfx_SoftVLine(y1, y2, x, color);
    watch_after("SoftVLine", y1, y2, x, (int)color, 0);
}

void __real_Gfx_SoftPutPixel(int x, int y, unsigned int color);
void __wrap_Gfx_SoftPutPixel(int x, int y, unsigned int color)
{
    watch_before();
    __real_Gfx_SoftPutPixel(x, y, color);
    watch_after("SoftPutPixel", x, y, (int)color, 0, 0);
}

#define WRAP_BLIT4(NAME, ARGS_DECL, ARGS_CALL)                                                                \
    void __real_##NAME ARGS_DECL;                                                                             \
    void __wrap_##NAME ARGS_DECL                                                                              \
    {                                                                                                         \
        watch_before();                                                                                       \
        __real_##NAME ARGS_CALL;                                                                              \
        watch_after(#NAME, 0, 0, 0, 0, 0);                                                                    \
    }
WRAP_BLIT4(BgElem_DrawParallaxSoft, (Bg *bg, BgElem *e, float sx, float sy), (bg, e, sx, sy))
WRAP_BLIT4(BgElem_DrawParallaxSoftMasked, (Bg *bg, BgElem *e, float sx, float sy), (bg, e, sx, sy))

