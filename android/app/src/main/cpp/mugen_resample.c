/*
 * P4a (D-003, D-019): the engine-facing libresample API on Android.
 *
 * The libresample.dll of M.U.G.E.N 1.1 is libresample 0.1.3 modified by Elecbyte: interleaved multi-channel buffers
 * (resample_open takes a channel count), resample_init/resample_deinit for shared filter tables and resample_reset
 * (third_party/sdk/include/libresample.h; the prototypes come from that header, so the compiler checks them).
 * The upstream library we can vendor (third_party/libresample, 0.1.5) is mono and has none of the three. This file
 * implements the Elecbyte API on top of it:
 *   - one upstream mono resampler per channel; the input is split into the channels, each goes through its own
 *     resampler with the same factor, and the outputs are interleaved again. The channels stay in lockstep: same input
 *     length, same factor, so every channel consumes and produces the same number of frames;
 *   - resample_init/deinit are no-ops: the upstream filter tables are shared by all handles and built on the first
 *     resample_open of each quality (MUGEN-UNLEASHED PATCH in third_party/libresample/src/resample.c);
 *   - resample_reset closes and reopens the per-channel resamplers.
 * The upstream symbols are renamed up_resample_* at build time (-Dresample_open=up_resample_open ... in
 * cmake/MugenAndroid.cmake), because they would collide with the names of this API.
 */
#include <stdlib.h>
#include <string.h>

#include "platform/platform.h"   /* __cdecl for the header below */
#include <libresample.h>

void *up_resample_open(int highQuality, double minFactor, double maxFactor);
int up_resample_process(void *handle, double factor, float *inBuffer, int inBufferLen, int lastFlag, int *inBufferUsed,
                        float *outBuffer, int outBufferLen);
void up_resample_close(void *handle);

#define MUGEN_RS_MAX_CHANNELS 8

typedef struct MugenResampler {
    int channels;
    int high_quality;
    double min_factor;
    double max_factor;
    void *mono[MUGEN_RS_MAX_CHANNELS];
    float *in_split;    /* one channel of the input */
    float *out_split;   /* one channel of the output */
    int in_cap;
    int out_cap;
} MugenResampler;

static int Rs_OpenMono(MugenResampler *rs)
{
    int c;

    for (c = 0; c < rs->channels; c++) {
        rs->mono[c] = up_resample_open(rs->high_quality, rs->min_factor, rs->max_factor);
        if (rs->mono[c] == NULL) {
            while (c-- > 0) {
                up_resample_close(rs->mono[c]);
                rs->mono[c] = NULL;
            }
            return 0;
        }
    }
    return 1;
}

static void Rs_CloseMono(MugenResampler *rs)
{
    int c;

    for (c = 0; c < rs->channels; c++) {
        if (rs->mono[c] != NULL) {
            up_resample_close(rs->mono[c]);
            rs->mono[c] = NULL;
        }
    }
}

int resample_init(int low_quality, int high_quality)
{
    (void)low_quality;
    (void)high_quality;
    return 1;
}

void resample_deinit(void)
{
}

void *resample_open(int quality, int channels, double min_factor, double max_factor)
{
    MugenResampler *rs;

    if (channels < 1 || channels > MUGEN_RS_MAX_CHANNELS) {
        return NULL;
    }
    rs = calloc(1, sizeof(*rs));
    if (rs == NULL) {
        return NULL;
    }
    rs->channels = channels;
    rs->high_quality = quality >= 1;
    rs->min_factor = min_factor;
    rs->max_factor = max_factor;
    if (!Rs_OpenMono(rs)) {
        free(rs);
        return NULL;
    }
    return rs;
}

int resample_process(void *handle, double factor, float *in, int in_len, int last, int *in_used, float *out, int out_len)
{
    MugenResampler *rs = handle;
    int used = 0;
    int produced = 0;
    int c, i, n = rs->channels;

    if (n == 1) {
        return up_resample_process(rs->mono[0], factor, in, in_len, last, in_used, out, out_len);
    }
    if (in_len > rs->in_cap) {
        float *p = realloc(rs->in_split, (size_t)in_len * sizeof(float));
        if (p == NULL) {
            return -1;
        }
        rs->in_split = p;
        rs->in_cap = in_len;
    }
    if (out_len > rs->out_cap) {
        float *p = realloc(rs->out_split, (size_t)out_len * sizeof(float));
        if (p == NULL) {
            return -1;
        }
        rs->out_split = p;
        rs->out_cap = out_len;
    }
    for (c = 0; c < n; c++) {
        int u = 0;
        int r;

        for (i = 0; i < in_len; i++) {
            rs->in_split[i] = in[(size_t)i * n + c];
        }
        r = up_resample_process(rs->mono[c], factor, rs->in_split, in_len, last, &u, rs->out_split, out_len);
        if (r < 0) {
            return r;
        }
        for (i = 0; i < r; i++) {
            out[(size_t)i * n + c] = rs->out_split[i];
        }
        if (c == 0) {
            used = u;
            produced = r;
        }
    }
    if (in_used != NULL) {
        *in_used = used;
    }
    return produced;
}

void resample_reset(void *handle)
{
    MugenResampler *rs = handle;

    Rs_CloseMono(rs);
    Rs_OpenMono(rs);
}

void resample_close(void *handle)
{
    MugenResampler *rs = handle;

    if (rs == NULL) {
        return;
    }
    Rs_CloseMono(rs);
    free(rs->in_split);
    free(rs->out_split);
    free(rs);
}
