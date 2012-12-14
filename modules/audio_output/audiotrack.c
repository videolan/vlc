/*****************************************************************************
 * audiotrack.c: Android native AudioTrack audio output module
 *****************************************************************************
 * Copyright © 2012 VLC authors and VideoLAN
 *
 * Authors: Ming Hu <tewilove@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

#include <dlfcn.h>
#include <assert.h>

#define SIZE_OF_AUDIOTRACK 256

/* From AudioSystem.h */
#define MUSIC 3

enum pcm_sub_format {
    PCM_SUB_16_BIT          = 0x1, // must be 1 for backward compatibility
    PCM_SUB_8_BIT           = 0x2  // must be 2 for backward compatibility
};

enum audio_format {
    PCM                 = 0x00000000, // must be 0 for backward compatibility
    PCM_16_BIT          = (PCM|PCM_SUB_16_BIT),
    PCM_8_BIT           = (PCM|PCM_SUB_8_BIT)
};

enum audio_channels {
    CHANNEL_OUT_FRONT_LEFT            = 0x4,
    CHANNEL_OUT_FRONT_RIGHT           = 0x8,
    CHANNEL_OUT_FRONT_CENTER          = 0x10,
    CHANNEL_OUT_LOW_FREQUENCY         = 0x20,
    CHANNEL_OUT_BACK_LEFT             = 0x40,
    CHANNEL_OUT_BACK_RIGHT            = 0x80,
    CHANNEL_OUT_FRONT_LEFT_OF_CENTER  = 0x100,
    CHANNEL_OUT_FRONT_RIGHT_OF_CENTER = 0x200,
    CHANNEL_OUT_BACK_CENTER           = 0x400,
    CHANNEL_OUT_MONO = CHANNEL_OUT_FRONT_LEFT,
    CHANNEL_OUT_STEREO = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT)
};

// _ZN7android11AudioSystem19getOutputFrameCountEPii
typedef int (*AudioSystem_getOutputFrameCount)(int *, int);
// _ZN7android11AudioSystem16getOutputLatencyEPji
typedef int (*AudioSystem_getOutputLatency)(unsigned int *, int);
// _ZN7android11AudioSystem21getOutputSamplingRateEPii
typedef int (*AudioSystem_getOutputSamplingRate)(int *, int);

// _ZN7android10AudioTrack16getMinFrameCountEPiij
typedef int (*AudioTrack_getMinFrameCount)(int *, int, unsigned int);

// _ZN7android11AudioSystem17getRenderPositionEPjS1_i
typedef int (*AudioTrack_getRenderPosition)(uint32_t *, uint32_t *, int);
// _ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii
typedef void (*AudioTrack_ctor)(void *, int, unsigned int, int, int, int, unsigned int, void (*)(int, void *, void *), void *, int, int);
// _ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_i
typedef void (*AudioTrack_ctor_legacy)(void *, int, unsigned int, int, int, int, unsigned int, void (*)(int, void *, void *), void *, int);
// _ZN7android10AudioTrackD1Ev
typedef void (*AudioTrack_dtor)(void *);
// _ZNK7android10AudioTrack9initCheckEv
typedef int (*AudioTrack_initCheck)(void *);
// _ZN7android10AudioTrack5startEv
typedef int (*AudioTrack_start)(void *);
// _ZN7android10AudioTrack4stopEv
typedef int (*AudioTrack_stop)(void *);
// _ZN7android10AudioTrack5writeEPKvj
typedef int (*AudioTrack_write)(void *, void  const*, unsigned int);
// _ZN7android10AudioTrack5flushEv
typedef int (*AudioTrack_flush)(void *);
// _ZN7android10AudioTrack5pauseEv
typedef int (*AudioTrack_pause)(void *);

struct aout_sys_t {
    float soft_gain;
    bool soft_mute;

    int rate;
    uint32_t samples_written;
    uint32_t initial;
    int bytes_per_frame;

    void *libmedia;
    void *AudioTrack;

    AudioSystem_getOutputFrameCount as_getOutputFrameCount;
    AudioSystem_getOutputLatency as_getOutputLatency;
    AudioSystem_getOutputSamplingRate as_getOutputSamplingRate;

    AudioTrack_getMinFrameCount at_getMinFrameCount;
    AudioTrack_ctor at_ctor;
    AudioTrack_ctor_legacy at_ctor_legacy;
    AudioTrack_dtor at_dtor;
    AudioTrack_initCheck at_initCheck;
    AudioTrack_start at_start;
    AudioTrack_stop at_stop;
    AudioTrack_write at_write;
    AudioTrack_flush at_flush;
    AudioTrack_pause at_pause;
    AudioTrack_getRenderPosition at_getRenderPosition;
};

/* Soft volume helper */
#include "volume.h"

static void *InitLibrary(struct aout_sys_t *p_sys);

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);
static void Play(audio_output_t*, block_t*);
static void Pause (audio_output_t *, bool, mtime_t);
static void Flush (audio_output_t *, bool);

vlc_module_begin ()
    set_shortname("AudioTrack")
    set_description(N_("Android AudioTrack audio output"))
    set_capability("audio output", 225)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_sw_gain()
    add_shortcut("android")
    set_callbacks(Open, Close)
vlc_module_end ()

static void *InitLibrary(struct aout_sys_t *p_sys)
{
    /* DL Open libmedia */
    void *p_library;
    p_library = dlopen("libmedia.so", RTLD_NOW|RTLD_LOCAL);
    if (!p_library)
        return NULL;

    /* Register symbols */
    p_sys->as_getOutputFrameCount = (AudioSystem_getOutputFrameCount)(dlsym(p_library, "_ZN7android11AudioSystem19getOutputFrameCountEPii"));
    p_sys->as_getOutputLatency = (AudioSystem_getOutputLatency)(dlsym(p_library, "_ZN7android11AudioSystem16getOutputLatencyEPji"));
    if(p_sys->as_getOutputLatency == NULL) {
        /* 4.1 Jellybean prototype */
        p_sys->as_getOutputLatency = (AudioSystem_getOutputLatency)(dlsym(p_library, "_ZN7android11AudioSystem16getOutputLatencyEPj19audio_stream_type_t"));
    }
    p_sys->as_getOutputSamplingRate = (AudioSystem_getOutputSamplingRate)(dlsym(p_library, "_ZN7android11AudioSystem21getOutputSamplingRateEPii"));
    p_sys->at_getMinFrameCount = (AudioTrack_getMinFrameCount)(dlsym(p_library, "_ZN7android10AudioTrack16getMinFrameCountEPiij"));
    if(p_sys->at_getMinFrameCount == NULL) {
        /* 4.1 Jellybean prototype */
        p_sys->at_getMinFrameCount = (AudioTrack_getMinFrameCount)(dlsym(p_library, "_ZN7android10AudioTrack16getMinFrameCountEPi19audio_stream_type_tj"));
    }
    p_sys->at_ctor = (AudioTrack_ctor)(dlsym(p_library, "_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii"));
    p_sys->at_ctor_legacy = (AudioTrack_ctor_legacy)(dlsym(p_library, "_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_i"));
    p_sys->at_dtor = (AudioTrack_dtor)(dlsym(p_library, "_ZN7android10AudioTrackD1Ev"));
    p_sys->at_initCheck = (AudioTrack_initCheck)(dlsym(p_library, "_ZNK7android10AudioTrack9initCheckEv"));
    p_sys->at_start = (AudioTrack_start)(dlsym(p_library, "_ZN7android10AudioTrack5startEv"));
    p_sys->at_stop = (AudioTrack_stop)(dlsym(p_library, "_ZN7android10AudioTrack4stopEv"));
    p_sys->at_write = (AudioTrack_write)(dlsym(p_library, "_ZN7android10AudioTrack5writeEPKvj"));
    p_sys->at_flush = (AudioTrack_flush)(dlsym(p_library, "_ZN7android10AudioTrack5flushEv"));
    p_sys->at_pause = (AudioTrack_pause)(dlsym(p_library, "_ZN7android10AudioTrack5pauseEv"));

    /* this symbol can have different names depending on the mangling */
    p_sys->at_getRenderPosition = (AudioTrack_getRenderPosition)(dlsym(p_library, "_ZN7android11AudioSystem17getRenderPositionEPjS1_i"));
    if (!p_sys->at_getRenderPosition)
        p_sys->at_getRenderPosition = (AudioTrack_getRenderPosition)(dlsym(p_library, "_ZN7android11AudioSystem17getRenderPositionEPjS1_19audio_stream_type_t"));

    /* We need the first 3 or the last 1 */
    if (!((p_sys->as_getOutputFrameCount && p_sys->as_getOutputLatency && p_sys->as_getOutputSamplingRate)
        || p_sys->at_getMinFrameCount)) {
        dlclose(p_library);
        return NULL;
    }

    // We need all the other Symbols
    if (!((p_sys->at_ctor || p_sys->at_ctor_legacy) && p_sys->at_dtor && p_sys->at_initCheck &&
           p_sys->at_start && p_sys->at_stop && p_sys->at_write && p_sys->at_flush)) {
        dlclose(p_library);
        return NULL;
    }
    return p_library;
}

static int TimeGet(audio_output_t *p_aout, mtime_t *restrict delay)
{
    aout_sys_t *p_sys = p_aout->sys;
    uint32_t hal, dsp;

    if (!p_sys->at_getRenderPosition)
        return -1;

    if (p_sys->at_getRenderPosition(&hal, &dsp, MUSIC))
        return -1;

    hal = (uint32_t)((uint64_t)hal * p_sys->rate / 44100);

    if (p_sys->samples_written == 0) {
        p_sys->initial = hal;
        return -1;
    }

    hal -= p_sys->initial;
    if (hal == 0)
        return -1;

    if (delay)
        *delay = ((mtime_t)p_sys->samples_written - hal) * CLOCK_FREQ / p_sys->rate;

    return 0;
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    struct aout_sys_t *p_sys = aout->sys;

    int status, size;
    int afSampleRate, afFrameCount, afLatency, minBufCount, minFrameCount;
    int stream_type, channel, rate, format;

    /* 4000 <= frequency <= 48000 */
    rate = fmt->i_rate;
    if (rate < 4000)
        rate = 4000;
    if (rate > 48000)
        rate = 48000;

    stream_type = MUSIC;

    /* We can only accept U8 and S16N */
    if (fmt->i_format != VLC_CODEC_U8 && fmt->i_format != VLC_CODEC_S16N)
        fmt->i_format = VLC_CODEC_S16N;
    format = (fmt->i_format == VLC_CODEC_S16N) ? PCM_16_BIT : PCM_8_BIT;

    /* TODO: android supports more channels */
    fmt->i_original_channels = fmt->i_physical_channels;
    switch(aout_FormatNbChannels(fmt))
    {
    case 1:
        channel = CHANNEL_OUT_MONO;
        fmt->i_physical_channels = AOUT_CHAN_CENTER;
        break;
    case 2:
    default:
        channel = CHANNEL_OUT_STEREO;
        fmt->i_physical_channels = AOUT_CHANS_STEREO;
        break;
    }

    /* Get the minimum buffer value */
    if (!p_sys->at_getMinFrameCount) {
        status = p_sys->as_getOutputSamplingRate(&afSampleRate, stream_type);
        status ^= p_sys->as_getOutputFrameCount(&afFrameCount, stream_type);
        status ^= p_sys->as_getOutputLatency((uint32_t*)(&afLatency), stream_type);
        if (status != 0) {
            msg_Err(aout, "Could not query the AudioStream parameters");
            return VLC_EGENERIC;
        }
        minBufCount = afLatency / ((1000 * afFrameCount) / afSampleRate);
        if (minBufCount < 2)
            minBufCount = 2;
        minFrameCount = (afFrameCount * rate * minBufCount) / afSampleRate;
    }
    else {
        status = p_sys->at_getMinFrameCount(&minFrameCount, stream_type, rate);
        if (status != 0) {
            msg_Err(aout, "Could not query the AudioTrack parameters");
            return VLC_EGENERIC;
        }
    }

    size = minFrameCount * (channel == CHANNEL_OUT_STEREO ? 2 : 1) * 4;

    /* Sizeof(AudioTrack) == 0x58 (not sure) on 2.2.1, this should be enough */
    p_sys->AudioTrack = malloc(SIZE_OF_AUDIOTRACK);
    if (!p_sys->AudioTrack)
        return VLC_ENOMEM;

    *((uint32_t *) ((uint32_t)p_sys->AudioTrack + SIZE_OF_AUDIOTRACK - 4)) = 0xbaadbaad;
    // Higher than android 2.2
    if (p_sys->at_ctor)
        p_sys->at_ctor(p_sys->AudioTrack, stream_type, rate, format, channel, size, 0, NULL, NULL, 0, 0);
    // Higher than android 1.6
    else if (p_sys->at_ctor_legacy)
        p_sys->at_ctor_legacy(p_sys->AudioTrack, stream_type, rate, format, channel, size, 0, NULL, NULL, 0);

    assert( (*((uint32_t *) ((uint32_t)p_sys->AudioTrack + SIZE_OF_AUDIOTRACK - 4)) == 0xbaadbaad) );

    /* And Init */
    status = p_sys->at_initCheck(p_sys->AudioTrack);

    /* android 1.6 uses channel count instead of stream_type */
    if (status != 0) {
        channel = (channel == CHANNEL_OUT_STEREO) ? 2 : 1;
        p_sys->at_ctor_legacy(p_sys->AudioTrack, stream_type, rate, format, channel, size, 0, NULL, NULL, 0);
        status = p_sys->at_initCheck(p_sys->AudioTrack);
    }
    if (status != 0) {
        msg_Err(aout, "Cannot create AudioTrack!");
        free(p_sys->AudioTrack);
        return VLC_EGENERIC;
    }

    aout_SoftVolumeStart(aout);

    aout->sys = p_sys;
    aout->time_get = NULL;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->time_get = TimeGet;

    p_sys->rate = rate;
    p_sys->samples_written = 0;
    p_sys->bytes_per_frame = aout_FormatNbChannels(fmt) * (format == PCM_16_BIT) ? 2 : 1;

    p_sys->at_start(p_sys->AudioTrack);
    TimeGet(aout, NULL); /* Gets the initial value of DAC samples counter */

    fmt->i_rate = rate;

    return VLC_SUCCESS;
}

static void Stop(audio_output_t* p_aout)
{
    aout_sys_t *p_sys = p_aout->sys;

    p_sys->at_stop(p_sys->AudioTrack);
    p_sys->at_flush(p_sys->AudioTrack);
    p_sys->at_dtor(p_sys->AudioTrack);
    free(p_sys->AudioTrack);
}

static void Play(audio_output_t* p_aout, block_t* p_buffer)
{
    aout_sys_t *p_sys = p_aout->sys;

    while (p_buffer->i_buffer) {
        int ret = p_sys->at_write(p_sys->AudioTrack, p_buffer->p_buffer, p_buffer->i_buffer);
        if (ret < 0) {
            msg_Err(p_aout, "Write failed (error %d)", ret);
            break;
        }

        p_sys->samples_written += ret / p_sys->bytes_per_frame;
        p_buffer->p_buffer += ret;
        p_buffer->i_buffer -= ret;
    }

    block_Release( p_buffer );
}

static void Pause(audio_output_t *p_aout, bool pause, mtime_t date)
{
    VLC_UNUSED(date);

    aout_sys_t *p_sys = p_aout->sys;

    if (pause) {
        p_sys->at_pause(p_sys->AudioTrack);
    } else {
        p_sys->at_start(p_sys->AudioTrack);
    }
}

static void Flush (audio_output_t *p_aout, bool wait)
{
    aout_sys_t *p_sys = p_aout->sys;
    if (wait) {
        mtime_t delay;
        if (!TimeGet(p_aout, &delay))
            msleep(delay);
    } else {
        p_sys->at_stop(p_sys->AudioTrack);
        p_sys->at_flush(p_sys->AudioTrack);
        p_sys->samples_written = 0;
        p_sys->at_start(p_sys->AudioTrack);
    }
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = malloc(sizeof (*sys));

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->libmedia = InitLibrary(sys);
    if (sys->libmedia == NULL) {
        msg_Err(aout, "Could not initialize libmedia.so!");
        free(sys);
        return VLC_EGENERIC;
    }

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout_SoftVolumeInit(aout);
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    dlclose(sys->libmedia);
    free(sys);
}
