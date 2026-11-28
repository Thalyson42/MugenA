# SDL_mixer (vendored for the Android port, D-003 / D-019)
Origin: https://github.com/libsdl-org/SDL_mixer, branch SDL-1.2, commit 16407659fd7a3af53171d6561fd7681593591509. License: zlib (COPYING).
Kept: mixer, music front end, effects (panning), WAV/AIFF/VOC loaders, wavestream. No MP3/Ogg/MIDI/MOD back end
(the engine decodes MP3 itself with libmpg123 and refuses the other formats).
Patch (MUGEN-UNLEASHED PATCH, tools/p4a/sdl_mixer-queryspecex.patch): adds Mix_QuerySpecEx, the call of Elecbyte's
modified SDL_mixer that also reports the chunk size (third_party/sdk/def/SDL_mixer.def).
Regenerate: tools/vendor-p4a.sh.
