/*
 * TEMPORARY (P3b M2, D-003): link stubs for the libraries the engine calls and the repository does not have yet: SDL_mixer
 * (25 functions, one of them Mix_QuerySpecEx of the original's modified SDL_mixer), SDL_ttf (8), SDL_gfx (zoomSurface) and
 * libresample (6). They let the whole engine (mugen_core) link into libmugen.so so that it can start on the device. The
 * prototypes come from the real headers (src/platform/stubs/SDL_ext, sdl12-compat, third_party/sdk/include/libresample.h), so
 * the compiler checks every signature.
 *
 * What they do, in short: the engine runs without sound and without TrueType fonts.
 *  - Mix_OpenAudio "succeeds" and remembers the requested format, which Mix_QuerySpecEx hands back, so Sound_Init goes on.
 *    Nothing is ever loaded (Mix_LoadWAV_RW, Mix_LoadMUS_RW, Mix_QuickLoad_RAW return NULL), nothing plays (Mix_PlayChannelTimed
 *    and Mix_PlayMusic return -1, Mix_Playing* return 0).
 *  - TTF_Init "succeeds"; TTF_OpenFont returns NULL, so a TrueType font fails to load (bitmap .fnt fonts do not use SDL_ttf).
 *  - zoomSurface returns NULL; resample_open returns NULL.
 * Each stub that matters logs once to logcat ("d003 stub: ..."). All of this goes away in P4 (audio: real SDL_mixer, libresample,
 * BGM) and with the fonts (SDL_ttf + FreeType, SDL_gfx), see D-003.
 */
#include "platform/platform.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <libresample.h>

static void Once(int *flag, const char *what)
{
    char line[160];

    if (!*flag) {
        *flag = 1;
        snprintf(line, sizeof line, "d003 stub: %s (no real library yet; see D-003)", what);
        Mugen_LogWrite(0, line);
    }
}

#ifndef MUGEN_AUDIO_REAL   /* P4a (D-019): the real SDL_mixer replaces these once tools/vendor-p4a.sh has run */
/* ---- SDL_mixer ---------------------------------------------------------------------------------------------- */
static int g_freq = 44100, g_channels = 2, g_chunk = 4096, g_mixChannels;
static Uint16 g_format = AUDIO_S16LSB;
static int g_flagOpen, g_flagLoad, g_flagPlay;

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize)
{
    g_freq = frequency;
    g_format = format;
    g_channels = channels;
    g_chunk = chunksize;
    Once(&g_flagOpen, "SDL_mixer: Mix_OpenAudio reports success, no audio device is opened, nothing will sound");
    return 0;
}

/* not in the standard SDL_mixer 1.2: the original's modified library reports what Mix_OpenAudio really got */
int Mix_QuerySpecEx(int *frequency, Uint16 *format, int *channels, int *chunksize)
{
    if (frequency != NULL) {
        *frequency = g_freq;
    }
    if (format != NULL) {
        *format = g_format;
    }
    if (channels != NULL) {
        *channels = g_channels;
    }
    if (chunksize != NULL) {
        *chunksize = g_chunk;
    }
    return 1;
}

void Mix_CloseAudio(void) { }

int Mix_AllocateChannels(int numchans)
{
    if (numchans >= 0) {
        g_mixChannels = numchans;
    }
    return g_mixChannels;
}

Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *src, int freesrc)
{
    if (src != NULL && freesrc) {
        SDL_RWclose(src);
    }
    Once(&g_flagLoad, "SDL_mixer: Mix_LoadWAV_RW / Mix_LoadMUS_RW / Mix_QuickLoad_RAW return NULL (no sound is loaded)");
    return NULL;
}

Mix_Music *Mix_LoadMUS_RW(SDL_RWops *rw)
{
    (void)rw;
    Once(&g_flagLoad, "SDL_mixer: Mix_LoadWAV_RW / Mix_LoadMUS_RW / Mix_QuickLoad_RAW return NULL (no sound is loaded)");
    return NULL;
}

Mix_Chunk *Mix_QuickLoad_RAW(Uint8 *mem, Uint32 len)
{
    (void)mem;
    (void)len;
    return NULL;
}

void Mix_FreeChunk(Mix_Chunk *chunk) { (void)chunk; }
void Mix_FreeMusic(Mix_Music *music) { (void)music; }

int Mix_PlayChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ticks)
{
    (void)channel;
    (void)chunk;
    (void)loops;
    (void)ticks;
    Once(&g_flagPlay, "SDL_mixer: nothing plays (Mix_PlayChannelTimed / Mix_PlayMusic return -1)");
    return -1;
}

int Mix_PlayMusic(Mix_Music *music, int loops)
{
    (void)music;
    (void)loops;
    Once(&g_flagPlay, "SDL_mixer: nothing plays (Mix_PlayChannelTimed / Mix_PlayMusic return -1)");
    return -1;
}

int Mix_HaltChannel(int channel) { (void)channel; return 0; }
int Mix_HaltMusic(void) { return 0; }
int Mix_Playing(int channel) { (void)channel; return 0; }
int Mix_PlayingMusic(void) { return 0; }
int Mix_PausedMusic(void) { return 0; }
void Mix_PauseMusic(void) { }
void Mix_ResumeMusic(void) { }
void Mix_RewindMusic(void) { }
int Mix_SetMusicPosition(double position) { (void)position; return -1; }
int Mix_SetPanning(int channel, Uint8 left, Uint8 right) { (void)channel; (void)left; (void)right; return 1; }
int Mix_Volume(int channel, int volume) { (void)channel; (void)volume; return MIX_MAX_VOLUME; }
int Mix_VolumeMusic(int volume) { (void)volume; return MIX_MAX_VOLUME; }

void Mix_HookMusic(void (*mix_func)(void *udata, Uint8 *stream, int len), void *arg)
{
    (void)mix_func;
    (void)arg;
}

void Mix_HookMusicFinished(void (*music_finished)(void)) { (void)music_finished; }

#endif /* !MUGEN_AUDIO_REAL */

/* ---- SDL_ttf ------------------------------------------------------------------------------------------------ */
static int g_flagTtf;

int TTF_Init(void) { return 0; }
void TTF_Quit(void) { }

TTF_Font *TTF_OpenFont(const char *file, int ptsize)
{
    (void)file;
    (void)ptsize;
    Once(&g_flagTtf, "SDL_ttf: TTF_OpenFont returns NULL, TrueType fonts do not load");
    return NULL;
}

void TTF_CloseFont(TTF_Font *font) { (void)font; }
int TTF_FontAscent(const TTF_Font *font) { (void)font; return 0; }

int TTF_SizeUTF8(TTF_Font *font, const char *text, int *w, int *h)
{
    (void)font;
    (void)text;
    if (w != NULL) {
        *w = 0;
    }
    if (h != NULL) {
        *h = 0;
    }
    return -1;
}

SDL_Surface *TTF_RenderUTF8_Solid(TTF_Font *font, const char *text, SDL_Color fg)
{
    (void)font;
    (void)text;
    (void)fg;
    return NULL;
}

SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *font, const char *text, SDL_Color fg)
{
    (void)font;
    (void)text;
    (void)fg;
    return NULL;
}

/* ---- SDL_gfx ------------------------------------------------------------------------------------------------ */
SDL_Surface *zoomSurface(SDL_Surface *src, double zoomx, double zoomy, int smooth)
{
    (void)src;
    (void)zoomx;
    (void)zoomy;
    (void)smooth;
    return NULL;
}

#ifndef MUGEN_AUDIO_REAL   /* P4a (D-019): mugen_resample.c replaces these */
/* ---- libresample -------------------------------------------------------------------------------------------- */
int resample_init(int low_quality, int high_quality) { (void)low_quality; (void)high_quality; return 1; }
void resample_deinit(void) { }

void *resample_open(int quality, int channels, double min_factor, double max_factor)
{
    (void)quality;
    (void)channels;
    (void)min_factor;
    (void)max_factor;
    return NULL;
}

int resample_process(void *handle, double factor, float *in, int in_len, int last, int *in_used, float *out, int out_len)
{
    (void)handle;
    (void)factor;
    (void)in;
    (void)in_len;
    (void)last;
    (void)out;
    (void)out_len;
    if (in_used != NULL) {
        *in_used = 0;
    }
    return 0;
}

void resample_reset(void *handle) { (void)handle; }
void resample_close(void *handle) { (void)handle; }
#endif /* !MUGEN_AUDIO_REAL */
