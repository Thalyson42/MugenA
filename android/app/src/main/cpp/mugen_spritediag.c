/*
 * Android diagnostic (P3b M3, D-017): the System renderer shows small dark dashes (about 6-12 x 2 px) at places that stay
 * fixed for a while (title menu background, fight ground, in front of the fighters) that the PC build does not show.
 * Linked with -Wl,--wrap=<draw function>, this records every write to the screen buffer during a frame (sprites,
 * rectangles, lines, pixels, SDL blits/fills, parallax layers) as an event with its bounding box. At the end of the frame
 * Mugen_DiagScanFrame scans the finished RGB565 buffer for dashes (short one-colour runs that are clearly darker than
 * both neighbours and whose rows above/below match the neighbours) and reports the last events whose box covers the
 * dash, newest first, with the source pixel for sprites. Output goes to mugen.log / logcat, prefix "spritediag".
 * Delete it (and the --wrap loop in cmake/MugenAndroid.cmake) once the dashes are explained.
 * REMOVER-NO-RELEASE (docs/android-port/05-diagnosticos-a-remover.md) item 1.
 */
#include <stdlib.h>
#include "mugen.h"

#define EV_MAX 8000

typedef struct {
    const char *name;
    int x1, y1, x2, y2;        /* inclusive bounding box (generous for sprites) */
    unsigned int colour;       /* primitives: colour argument */
    int blend;
    SpriteSet *set;            /* sprites only */
    int sprite;
    float x, y, sx, sy;
    unsigned int flags;
} DiagEv;

static DiagEv g_ev[EV_MAX];
static int g_evN = 0;
static int g_scans = 0;
static int g_lastScanFrame = 0;

static int target_is_screen(void)
{
    const KeyedArray *ta;
    const Sprite *t;

    if (g_renderTarget == NULL || g_renderTarget->sprites == NULL) {
        return 0;
    }
    ta = g_renderTarget->sprites;
    if (g_renderTargetSprite < 0 || g_renderTargetSprite >= ta->capacity) {
        return 0;
    }
    t = (const Sprite *)(ta->data + ta->elem_size * g_renderTargetSprite);
    return t->w >= 1000;
}

static DiagEv *ev_new(const char *name, int x1, int y1, int x2, int y2)
{
    DiagEv *e;
    int t;

    if (g_evN >= EV_MAX || !target_is_screen()) {
        return NULL;
    }
    if (x1 > x2) {
        t = x1; x1 = x2; x2 = t;
    }
    if (y1 > y2) {
        t = y1; y1 = y2; y2 = t;
    }
    e = &g_ev[g_evN++];
    e->name = name;
    e->x1 = x1 - 1;
    e->y1 = y1 - 1;
    e->x2 = x2 + 1;
    e->y2 = y2 + 1;
    e->colour = 0;
    e->blend = 0;
    e->set = NULL;
    e->sprite = 0;
    return e;
}

static void ev_box(const char *name, int x1, int y1, int x2, int y2, unsigned int colour)
{
    DiagEv *e = ev_new(name, x1, y1, x2, y2);
    if (e != NULL) {
        e->colour = colour;
    }
}

static void ev_all(const char *name)
{
    ev_box(name, 0, 0, 100000, 100000, 0);
}

static void ev_sprite(const char *name, const DrawParams *dp, SpriteSet *set, int sprite)
{
    const KeyedArray *a;
    const Sprite *spr;
    float sx, sy;
    DiagEv *e;

    if (set == NULL || set->sprites == NULL) {
        return;
    }
    a = set->sprites;
    if (sprite < 0 || sprite >= a->capacity || a->slots == NULL || !a->slots[sprite].used) {
        return;
    }
    spr = (const Sprite *)(a->data + a->elem_size * sprite);
    sx = dp->scale_x != 0.0f ? dp->scale_x : 1.0f;
    sy = dp->scale_y != 0.0f ? dp->scale_y : 1.0f;
    if (sx < 0) {
        sx = -sx;
    }
    if (sy < 0) {
        sy = -sy;
    }
    e = ev_new(name, (int)(dp->x - (spr->x + spr->w) * sx), (int)(dp->y - (spr->y + spr->h) * sy),
               (int)(dp->x + (spr->x + spr->w) * sx), (int)(dp->y + (spr->y + spr->h) * sy));
    if (e != NULL) {
        e->set = set;
        e->sprite = sprite;
        e->x = dp->x;
        e->y = dp->y;
        e->sx = dp->scale_x;
        e->sy = dp->scale_y;
        e->flags = dp->flags;
        e->blend = dp->blend;
    }
}

static int luma565(unsigned int c)
{
    return (int)(((c >> 11) & 31) * 24 + ((c >> 5) & 63) * 12 + (c & 31) * 9) / 10;
}

/* Source pixel of a sprite event behind screen pixel (wx, wy): unflipped placement, else mirrored; 0 when outside. */
static int ev_source(const DiagEv *d, const Sprite *spr, int wx, int wy, int *lx, int *ly, int *mirrored)
{
    float sx = d->sx != 0.0f ? d->sx : 1.0f, sy = d->sy != 0.0f ? d->sy : 1.0f;
    float x0 = d->x - spr->x * sx, y0 = d->y - spr->y * sy;
    float fx = (wx - x0) / sx, fy = (wy - y0) / sy;

    *mirrored = 0;
    *lx = (int)fx;
    *ly = (int)fy;
    if (fx >= 0 && fy >= 0 && *lx < spr->w && *ly < spr->h) {
        return 1;
    }
    x0 = d->x + (spr->x - spr->w + 1) * sx;
    fx = (wx - x0) / sx;
    *lx = (int)fx;
    if (fx >= 0 && fy >= 0 && *lx < spr->w && *ly < spr->h) {
        *lx = spr->w - 1 - *lx;
        *mirrored = 1;
        return 1;
    }
    return 0;
}

static void ev_report(int wx, int wy, unsigned short colour)
{
    int i, shown = 0, sprites_shown = 0;

    for (i = g_evN - 1; i >= 0 && shown < 10; i--) {
        const DiagEv *d = &g_ev[i];
        const KeyedArray *a;
        const Sprite *spr;
        const unsigned char *px;
        const unsigned short *pal = NULL;
        int lx = 0, ly = 0, mir = 0, r, c;

        if (wx < d->x1 || wx > d->x2 || wy < d->y1 || wy > d->y2) {
            continue;
        }
        if (d->set == NULL) {
            shown++;
            Log_Printf("spritediag:   event %d of %d: %s box %d,%d..%d,%d colour %04x\n", i, g_evN, d->name, d->x1 + 1,
                       d->y1 + 1, d->x2 - 1, d->y2 - 1, d->colour);
            continue;
        }
        a = d->set->sprites;
        spr = (const Sprite *)(a->data + a->elem_size * d->sprite);
        if (!ev_source(d, spr, wx, wy, &lx, &ly, &mir)) {
            continue;
        }
        shown++;
        px = (const unsigned char *)Sprite_GetPixels(d->set, d->sprite);
        if (spr->pal_index >= 0) {
            Palette *p = SpriteSet_GetPaletteConverted(d->set, spr->pal_index);
            if (p != NULL) {
                pal = p->rgb565;
            }
        }
        Log_Printf("spritediag:   event %d of %d: %s set %p slot %d key %d,%d size %dx%d axis %d,%d bpp %d fmt %d pal %d "
                   "at %.1f,%.1f scale %.2f,%.2f flags %x blend %d -> source %d,%d%s idx %d\n",
                   i, g_evN, d->name, (void *)d->set, d->sprite, a->slots[d->sprite].key1, a->slots[d->sprite].key2,
                   spr->w, spr->h, spr->x, spr->y, spr->bpp, spr->format, spr->pal_index, d->x, d->y, d->sx, d->sy,
                   d->flags, d->blend, lx, ly, mir ? " (mirrored)" : "",
                   (px != NULL && spr->rows != NULL) ? px[spr->rows[ly] + lx] : -1);
        if (pal != NULL && px != NULL && spr->rows != NULL && sprites_shown < 2) {
            int k, found = 0;
            sprites_shown++;
            Log_Printf("spritediag:     palette entries equal to the dash colour:");
            for (k = 0; k < 256; k++) {
                if (pal[k] == colour) {
                    Log_Printf(" %d", k);
                    found++;
                }
            }
            Log_Printf("%s\n", found ? "" : " none");
            for (r = ly - 2; r <= ly + 2; r++) {
                const unsigned char *row;
                if (r < 0 || r >= spr->h) {
                    continue;
                }
                row = px + spr->rows[r];
                Log_Printf("spritediag:     src row %d cols %d..%d (index=rgb565):", r, lx - 4 < 0 ? 0 : lx - 4, lx + 4);
                for (c = lx - 4; c <= lx + 4; c++) {
                    if (c >= 0 && c < spr->w) {
                        Log_Printf(" %d=%04x", row[c], (unsigned)pal[row[c]]);
                    }
                }
                Log_Printf("\n");
            }
        }
    }
    if (shown == 0) {
        Log_Printf("spritediag:   no recorded write covers it (%d events)\n", g_evN);
    }
}

/* 1 on the frames where the finished buffer should be scanned (gfx.c locks the surface only then). */
int Mugen_DiagWantScan(int frame)
{
    return g_scans < 4 && frame >= 20 && frame % 20 == 0 && frame - g_lastScanFrame >= 60;
}

/* Called every frame at the end of the frame; pixels is non-NULL only when Mugen_DiagWantScan said so. */
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
                if (c != 0 || n < 90 ||   /* only black dashes on a bright area (the title-menu ones); the fight "dashes" are sprite shading */
                    luma565(c) + 20 > n ||
                    abs(luma565(*(const unsigned short *)(pixels + (size_t)(y - 1) * pitch + xm * 2)) - n) > 8 ||
                    abs(luma565(*(const unsigned short *)(pixels + (size_t)(y + 3) * pitch + xm * 2)) - n) > 8) {
                    continue;
                }
                Log_Printf("spritediag: frame %d dash at x=%d y=%d len=%d colour %04x around %04x (rows below: %04x %04x)\n",
                           frame, x, y, len, (unsigned)c, (unsigned)row[x - 1],
                           (unsigned)*(const unsigned short *)(pixels + (size_t)(y + 1) * pitch + xm * 2),
                           (unsigned)*(const unsigned short *)(pixels + (size_t)(y + 2) * pitch + xm * 2));
                ev_report(xm, y, c);
                found++;
                x += len;
            }
        }
        if (found > 0) {
            g_scans++;
            g_lastScanFrame = frame;
        }
    }
    g_evN = 0;
}

/* ---- wrappers: record, then forward unchanged ---- */

void __real_Sprite_DrawSoft8(const DrawParams *dp, SpriteSet *set, int sprite);
void __wrap_Sprite_DrawSoft8(const DrawParams *dp, SpriteSet *set, int sprite)
{
    ev_sprite("DrawSoft8", dp, set, sprite);
    __real_Sprite_DrawSoft8(dp, set, sprite);
}

void __real_Sprite_DrawSoft(SpriteSet *set, int sprite, const DrawParams *dp);
void __wrap_Sprite_DrawSoft(SpriteSet *set, int sprite, const DrawParams *dp)
{
    /* surface / non-paletted sprites do not reach Sprite_DrawSoft8: record them whole */
    /* not recorded: the 8-bit sprites are recorded by Sprite_DrawSoft8 with their real box (an "all" box here hid them) */
    __real_Sprite_DrawSoft(set, sprite, dp);
}

void __real_Gfx_SoftClear(unsigned int color);
void __wrap_Gfx_SoftClear(unsigned int color)
{
    ev_box("SoftClear", 0, 0, 100000, 100000, color);
    __real_Gfx_SoftClear(color);
}

#define WRAP_RECT(NAME)                                                                                       \
    void __real_##NAME(int x1, int y1, int x2, int y2, unsigned int color);                                  \
    void __wrap_##NAME(int x1, int y1, int x2, int y2, unsigned int color)                                   \
    {                                                                                                         \
        ev_box(#NAME, x1, y1, x2, y2, color);                                                                 \
        __real_##NAME(x1, y1, x2, y2, color);                                                                 \
    }
WRAP_RECT(Gfx_SoftDrawRect)
WRAP_RECT(Gfx_SoftDrawRectAdd)
WRAP_RECT(Gfx_SoftDrawRectSub)
WRAP_RECT(Gfx_SoftFillRect)
WRAP_RECT(Gfx_SoftFillRectAdd)
WRAP_RECT(Gfx_SoftFillRectSub)

#define WRAP_RECT_AB(NAME)                                                                                    \
    void __real_##NAME(int x1, int y1, int x2, int y2, unsigned int color, int a, int b);                    \
    void __wrap_##NAME(int x1, int y1, int x2, int y2, unsigned int color, int a, int b)                     \
    {                                                                                                         \
        ev_box(#NAME, x1, y1, x2, y2, color);                                                                 \
        __real_##NAME(x1, y1, x2, y2, color, a, b);                                                           \
    }
WRAP_RECT_AB(Gfx_SoftDrawRectBlend)
WRAP_RECT_AB(Gfx_SoftBlendRect)

void __real_Gfx_SoftFillRectHalfAdd(unsigned int color, int x1, int x2, int y1, int y2);
void __wrap_Gfx_SoftFillRectHalfAdd(unsigned int color, int x1, int x2, int y1, int y2)
{
    ev_box("SoftFillRectHalfAdd", x1, y1, x2, y2, color);
    __real_Gfx_SoftFillRectHalfAdd(color, x1, x2, y1, y2);
}

#define WRAP_HLINE_CXY(NAME)                                                                                  \
    void __real_##NAME(unsigned int color, int x1, int x2, int y);                                           \
    void __wrap_##NAME(unsigned int color, int x1, int x2, int y)                                            \
    {                                                                                                         \
        ev_box(#NAME, x1, y, x2, y, color);                                                                   \
        __real_##NAME(color, x1, x2, y);                                                                      \
    }
WRAP_HLINE_CXY(Gfx_SoftHLineAdd)
WRAP_HLINE_CXY(Gfx_SoftHLineHalfAdd)
WRAP_HLINE_CXY(Gfx_SoftHLineSub)

#define WRAP_VLINE_CXY(NAME)                                                                                  \
    void __real_##NAME(unsigned int color, int y1, int y2, int x);                                           \
    void __wrap_##NAME(unsigned int color, int y1, int y2, int x)                                            \
    {                                                                                                         \
        ev_box(#NAME, x, y1, x, y2, color);                                                                   \
        __real_##NAME(color, y1, y2, x);                                                                      \
    }
WRAP_VLINE_CXY(Gfx_SoftVLineAdd)
WRAP_VLINE_CXY(Gfx_SoftVLineHalfAdd)
WRAP_VLINE_CXY(Gfx_SoftVLineSub)

void __real_Gfx_SoftHLine(int x1, int x2, int y, unsigned int color);
void __wrap_Gfx_SoftHLine(int x1, int x2, int y, unsigned int color)
{
    ev_box("SoftHLine", x1, y, x2, y, color);
    __real_Gfx_SoftHLine(x1, x2, y, color);
}

void __real_Gfx_SoftVLine(int y1, int y2, int x, unsigned int color);
void __wrap_Gfx_SoftVLine(int y1, int y2, int x, unsigned int color)
{
    ev_box("SoftVLine", x, y1, x, y2, color);
    __real_Gfx_SoftVLine(y1, y2, x, color);
}

#define WRAP_PIXEL(NAME)                                                                                      \
    void __real_##NAME(int x, int y, unsigned int color);                                                    \
    void __wrap_##NAME(int x, int y, unsigned int color)                                                     \
    {                                                                                                         \
        ev_box(#NAME, x, y, x, y, color);                                                                     \
        __real_##NAME(x, y, color);                                                                           \
    }
WRAP_PIXEL(Gfx_SoftPutPixel)
WRAP_PIXEL(Gfx_SoftAddPixel)

void __real_Gfx_SoftInvertPixel(int x, int y);
void __wrap_Gfx_SoftInvertPixel(int x, int y)
{
    ev_box("SoftInvertPixel", x, y, x, y, 0);
    __real_Gfx_SoftInvertPixel(x, y);
}

#define WRAP_LINE(NAME)                                                                                       \
    void __real_##NAME(float x1, float y1, float x2, float y2, unsigned int color);                          \
    void __wrap_##NAME(float x1, float y1, float x2, float y2, unsigned int color)                           \
    {                                                                                                         \
        ev_box(#NAME, (int)x1, (int)y1, (int)x2, (int)y2, color);                                             \
        __real_##NAME(x1, y1, x2, y2, color);                                                                 \
    }
WRAP_LINE(Gfx_SoftDrawLine)
WRAP_LINE(Gfx_SoftDrawLineAdd)

#define WRAP_BG(NAME)                                                                                         \
    void __real_##NAME(Bg *bg, BgElem *e, float sx, float sy);                                               \
    void __wrap_##NAME(Bg *bg, BgElem *e, float sx, float sy)                                                \
    {                                                                                                         \
        ev_all(#NAME);                                                                                        \
        __real_##NAME(bg, e, sx, sy);                                                                         \
    }
WRAP_BG(BgElem_DrawParallaxSoft)
WRAP_BG(BgElem_DrawParallaxSoftMasked)

int __real_SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
int __wrap_SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
    if (src != NULL && dst != NULL && dst->w >= 1000 && dst->format != NULL && dst->format->BitsPerPixel == 16) {
        int dx = dstrect != NULL ? dstrect->x : 0, dy = dstrect != NULL ? dstrect->y : 0;
        if (!(dx == 0 && dy == 0 && src->w >= dst->w && src->h >= dst->h)) {   /* skip the final present blit */
            ev_box("SDL_UpperBlit", dx, dy, dx + src->w, dy + src->h, 0);
        }
    }
    return __real_SDL_UpperBlit(src, srcrect, dst, dstrect);
}

int __real_SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
int __wrap_SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color)
{
    if (dst != NULL && dst->w >= 1000 && dst->format != NULL && dst->format->BitsPerPixel == 16) {
        if (dstrect != NULL) {
            ev_box("SDL_FillRect", dstrect->x, dstrect->y, dstrect->x + dstrect->w, dstrect->y + dstrect->h, color);
        } else {
            ev_box("SDL_FillRect(all)", 0, 0, 100000, 100000, color);
        }
    }
    return __real_SDL_FillRect(dst, dstrect, color);
}
