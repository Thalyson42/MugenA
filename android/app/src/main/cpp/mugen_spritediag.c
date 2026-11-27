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


/* ---- per-frame record of the sprite draws on the screen buffer ----
 * Every Sprite_DrawSoft8 on a large (>= 1000 px wide) target is recorded; when the scan finds a dash at the end of the
 * frame, the last recorded draws whose rectangle covers it are reported with the source pixel behind it. */
#define RING_MAX 6000
typedef struct {
    SpriteSet *set;
    int sprite;
    float x, y, sx, sy;
    unsigned int flags;
    int blend;
} DiagDraw;
static DiagDraw g_ring[RING_MAX];
static int g_ringN = 0;

static void ring_add(const DrawParams *dp, SpriteSet *set, int sprite)
{
    DiagDraw *d;
    if (g_ringN >= RING_MAX || g_renderTarget == NULL || g_renderTarget->sprites == NULL) {
        return;
    }
    {
        const KeyedArray *ta = g_renderTarget->sprites;
        const Sprite *t;
        if (g_renderTargetSprite < 0 || g_renderTargetSprite >= ta->capacity) {
            return;
        }
        t = (const Sprite *)(ta->data + ta->elem_size * g_renderTargetSprite);
        if (t->w < 1000) {
            return;
        }
    }
    d = &g_ring[g_ringN++];
    d->set = set;
    d->sprite = sprite;
    d->x = dp->x;
    d->y = dp->y;
    d->sx = dp->scale_x;
    d->sy = dp->scale_y;
    d->flags = dp->flags;
    d->blend = dp->blend;
}

/* Reports the last draws (newest first, at most 4) that cover pixel (wx, wy). */
static void ring_report(int wx, int wy, unsigned short colour)
{
    int i, shown = 0;
    for (i = g_ringN - 1; i >= 0 && shown < 4; i--) {
        const DiagDraw *d = &g_ring[i];
        const KeyedArray *a;
        const Sprite *spr;
        const unsigned char *px;
        const unsigned short *pal = NULL;
        float sx = d->sx != 0.0f ? d->sx : 1.0f, sy = d->sy != 0.0f ? d->sy : 1.0f;
        float x0, y0, fx, fy;
        int lx, ly, r, c, k;

        if (d->set == NULL || d->set->sprites == NULL) {
            continue;
        }
        a = d->set->sprites;
        if (d->sprite < 0 || d->sprite >= a->capacity || a->slots == NULL || !a->slots[d->sprite].used) {
            continue;
        }
        spr = (const Sprite *)(a->data + a->elem_size * d->sprite);
        if (spr->w <= 0 || spr->h <= 0) {
            continue;
        }
        /* unflipped mapping: the axis (spr->x, spr->y) sits at (d->x, d->y), scaled */
        x0 = d->x - spr->x * sx;
        y0 = d->y - spr->y * sy;
        fx = (wx - x0) / sx;
        fy = (wy - y0) / sy;
        lx = (int)fx;
        ly = (int)fy;
        if (fx < 0 || fy < 0 || lx >= spr->w || ly >= spr->h) {
            /* try the horizontally mirrored placement */
            x0 = d->x + (spr->x - spr->w + 1) * sx;
            fx = (wx - x0) / sx;
            lx = (int)fx;
            if (fx < 0 || fy < 0 || lx >= spr->w || ly >= spr->h) {
                continue;
            }
            lx = spr->w - 1 - lx;
            Log_Printf("spritediag: (mirrored placement assumed)\n");
        }
        shown++;
        px = (const unsigned char *)Sprite_GetPixels(d->set, d->sprite);
        if (spr->pal_index >= 0) {
            Palette *p = SpriteSet_GetPaletteConverted(d->set, spr->pal_index);
            if (p != NULL) {
                pal = p->rgb565;
            }
        }
        Log_Printf("spritediag: DASH %d,%d (%04x) covered by draw #%d of %d: set %p slot %d key %d,%d size %dx%d axis "
                   "%d,%d bpp %d fmt %d pal %d at %.1f,%.1f scale %.2f,%.2f flags %x blend %d -> source %d,%d\n",
                   wx, wy, (unsigned)colour, i, g_ringN, (void *)d->set, d->sprite, a->slots[d->sprite].key1,
                   a->slots[d->sprite].key2, spr->w, spr->h, spr->x, spr->y, spr->bpp, spr->format, spr->pal_index,
                   d->x, d->y, d->sx, d->sy, d->flags, d->blend, lx, ly);
        if (px == NULL || spr->rows == NULL) {
            continue;
        }
        if (pal != NULL) {
            int found = 0;
            Log_Printf("spritediag:   palette indices with the dash colour:");
            for (k = 0; k < 256; k++) {
                if (pal[k] == colour) {
                    Log_Printf(" %d", k);
                    found++;
                }
            }
            Log_Printf("%s\n", found ? "" : " none");
        }
        for (r = ly - 3; r <= ly + 3; r++) {
            const unsigned char *row;
            if (r < 0 || r >= spr->h) {
                continue;
            }
            row = px + spr->rows[r];
            Log_Printf("spritediag:   src row %d, cols %d..%d:", r, lx - 6 < 0 ? 0 : lx - 6, lx + 6);
            for (c = lx - 6; c <= lx + 6; c++) {
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
    if (shown == 0) {
        Log_Printf("spritediag: DASH %d,%d (%04x): no recorded sprite draw covers it (%d draws recorded)\n", wx, wy,
                   (unsigned)colour, g_ringN);
    }
}


static int luma565(unsigned int c)
{
    return (int)(((c >> 11) & 31) * 24 + ((c >> 5) & 63) * 12 + (c & 31) * 9) / 10;
}

static int g_scans = 0;
static int g_lastScanFrame = 0;

/* 1 on the frames where the finished buffer should be scanned (gfx.c locks the surface only then). */
int Mugen_DiagWantScan(int frame)
{
    return g_scans < 4 && frame >= 20 && frame % 20 == 0 && frame - g_lastScanFrame >= 60;
}

/* Called every frame from Gfx_AndroidDebugFrame at the end of the frame; pixels is non-NULL only when
 * Mugen_DiagWantScan said so. Resets the draw record for the next frame. */
void Mugen_DiagScanFrame(const unsigned char *pixels, int w, int h, int pitch, int frame)
{
    int x, y, found = 0;

    if (pixels != NULL) {
        for (y = 1; y < h - 4 && found < 8; y++) {
            const unsigned short *row = (const unsigned short *)(pixels + (size_t)y * pitch);
            for (x = 2; x < w - 14 && found < 8; x++) {
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
                if (luma565(c) + 20 > n ||
                    abs(luma565(*(const unsigned short *)(pixels + (size_t)(y - 1) * pitch + xm * 2)) - n) > 8 ||
                    abs(luma565(*(const unsigned short *)(pixels + (size_t)(y + 3) * pitch + xm * 2)) - n) > 8) {
                    continue;
                }
                Log_Printf("spritediag: frame %d dash at x=%d y=%d len=%d colour %04x around %04x\n", frame, x, y, len,
                           (unsigned)c, (unsigned)row[x - 1]);
                ring_report(xm, y, c);
                found++;
                x += len;
            }
        }
        if (found > 0) {
            g_scans++;
            g_lastScanFrame = frame;
        }
    }
    g_ringN = 0;
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
    ring_add(dp, set, sprite);
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

