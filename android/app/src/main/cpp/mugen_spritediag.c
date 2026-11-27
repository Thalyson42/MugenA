/*
 * Android diagnostic (P3b M3, D-017): the System renderer shows small dark dashes (about 8x2 px) at fixed places (the
 * corners of the title menu, the ground between the fighters) that the PC build does not show. Linked with
 * -Wl,--wrap=Sprite_DrawSoft8, this logs once per sprite every draw whose sprite is tiny (width <= 16, height <= 3):
 * which sprite (slot, group, number), its size and axis, where and how it is drawn, and its first pixels with their
 * RGB565 colours. It forwards every call unchanged. Output goes to mugen.log / logcat, prefix "spritediag".
 * Delete it (and the link option in cmake/MugenAndroid.cmake) once the dashes are explained.
 */
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

void __wrap_Sprite_DrawSoft8(const DrawParams *dp, SpriteSet *set, int sprite)
{
    diag_note(dp, set, sprite);
    __real_Sprite_DrawSoft8(dp, set, sprite);
}
