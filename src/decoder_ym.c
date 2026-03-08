/*
  SDL_mixer:  An audio mixer library based on the SDL library
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* YM file decoder using the StSound library (Atari ST music format).
   StSound by Arnaud Carre (aka Leonard/Oxygene). */

#ifdef DECODER_YM

#include "SDL_mixer_internal.h"

#include <StSoundLibrary.h>
#include <YmTypes.h>

typedef struct
{
    YMMUSIC *music;
    int freq;
} YM_TrackData;


static bool SDLCALL YM_init(void)
{
    return true;
}

static void SDLCALL YM_quit(void)
{
}

static bool SDLCALL YM_init_audio(SDL_IOStream *io, SDL_AudioSpec *spec, SDL_PropertiesID props, Sint64 *duration_frames, void **audio_userdata)
{
    char magic[8];
    if (SDL_ReadIO(io, magic, sizeof(magic)) != sizeof(magic)) {
        return false;
    }

    /* YM files start with "YM" or may be LZH-compressed with "-lh5-" at offset 2 */
    if (SDL_memcmp(magic, "YM", 2) != 0 && SDL_memcmp(magic + 2, "-lh5-", 5) != 0) {
        return SDL_SetError("Not a YM audio file");
    }

    bool copied = false;
    size_t datalen = 0;
    Uint8 *data = (Uint8 *) MIX_SlurpConstIO(io, &datalen, &copied);
    if (!data) {
        return false;
    }

    YMMUSIC *ym = ymMusicCreate();
    if (!ym) {
        if (copied) {
            SDL_free(data);
        }
        return SDL_SetError("Failed to create YM music instance");
    }

    if (!ymMusicLoadMemory(ym, (void *) data, (ymu32) datalen)) {
        const char *err = ymMusicGetLastError(ym);
        ymMusicDestroy(ym);
        if (copied) {
            SDL_free(data);
        }
        return SDL_SetError("Failed to load YM data: %s", err);
    }

    if (copied) {
        SDL_free(data);
    }

    ymMusicInfo_t info;
    ymMusicGetInfo(ym, &info);

    if (info.pSongName && *info.pSongName) {
        SDL_SetStringProperty(props, MIX_PROP_METADATA_TITLE_STRING, info.pSongName);
    }
    if (info.pSongAuthor && *info.pSongAuthor) {
        SDL_SetStringProperty(props, MIX_PROP_METADATA_ARTIST_STRING, info.pSongAuthor);
    }

    if (info.musicTimeInMs > 0) {
        *duration_frames = MIX_MSToFrames(spec->freq, info.musicTimeInMs);
    } else {
        *duration_frames = -1;
    }

    ymMusicDestroy(ym);

    spec->format = SDL_AUDIO_S16;
    spec->channels = 1;

    *audio_userdata = NULL;

    return true;
}

static bool SDLCALL YM_init_track(void *audio_userdata, SDL_IOStream *io, const SDL_AudioSpec *spec, SDL_PropertiesID props, void **track_userdata)
{
    (void) audio_userdata;
    (void) props;

    bool copied = false;
    size_t datalen = 0;
    Uint8 *data = (Uint8 *) MIX_SlurpConstIO(io, &datalen, &copied);
    if (!data) {
        return false;
    }

    YMMUSIC *ym = ymMusicCreateWithRate(spec->freq);
    if (!ym) {
        if (copied) {
            SDL_free(data);
        }
        return SDL_SetError("Failed to create YM music instance");
    }

    if (!ymMusicLoadMemory(ym, (void *) data, (ymu32) datalen)) {
        const char *err = ymMusicGetLastError(ym);
        ymMusicDestroy(ym);
        if (copied) {
            SDL_free(data);
        }
        return SDL_SetError("Failed to load YM data: %s", err);
    }

    if (copied) {
        SDL_free(data);
    }

    ymMusicSetLoopMode(ym, YMFALSE);
    ymMusicPlay(ym);

    YM_TrackData *track = (YM_TrackData *) SDL_calloc(1, sizeof(YM_TrackData));
    if (!track) {
        ymMusicDestroy(ym);
        return false;
    }
    track->music = ym;
    track->freq = spec->freq;

    *track_userdata = track;
    return true;
}

static bool SDLCALL YM_decode(void *track_userdata, SDL_AudioStream *stream)
{
    YM_TrackData *track = (YM_TrackData *) track_userdata;

    if (ymMusicIsOver(track->music)) {
        return false;
    }

    ymsample samples[512];
    ymMusicCompute(track->music, samples, SDL_arraysize(samples));

    SDL_PutAudioStreamData(stream, samples, sizeof(samples));

    return true;
}

static bool SDLCALL YM_seek(void *track_userdata, Uint64 frame)
{
    YM_TrackData *track = (YM_TrackData *) track_userdata;
    ymMusicRestart(track->music);
    if (frame > 0) {
        ymu32 ms = (ymu32)((frame * 1000) / (Uint64) track->freq);
        ymMusicSeek(track->music, ms);
    }
    return true;
}

static void SDLCALL YM_quit_track(void *track_userdata)
{
    YM_TrackData *track = (YM_TrackData *) track_userdata;
    if (track) {
        if (track->music) {
            ymMusicDestroy(track->music);
        }
        SDL_free(track);
    }
}

static void SDLCALL YM_quit_audio(void *audio_userdata)
{
    (void) audio_userdata;
}

const MIX_Decoder MIX_Decoder_YM = {
    "YM",
    YM_init,
    YM_init_audio,
    YM_init_track,
    YM_decode,
    YM_seek,
    YM_quit_track,
    YM_quit_audio,
    YM_quit
};

#endif
