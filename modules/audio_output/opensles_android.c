/*****************************************************************************
 * opensles_android.c : audio output for android native code
 *****************************************************************************
 * Copyright © 2011-2012 VideoLAN
 *
 * Authors: Dominique Martinet <asmadeus@codewreck.org>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *          Rafaël Carré <funman@videolanorg>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <assert.h>
#include <dlfcn.h>
#include <math.h>

// For native audio
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <jni.h>
JNIEnv *android_getEnv(vlc_object_t *p_obj, const char *psz_thread_name);

#define OPENSLES_BUFFERS 255 /* maximum number of buffers */
#define OPENSLES_BUFLEN  VLC_TICK_FROM_MS(10)
/*
 * 10ms of precision when mesasuring latency should be enough,
 * with 255 buffers we can buffer 2.55s of audio.
 */

#define CHECK_OPENSL_ERROR(msg) \
    if (unlikely(result != SL_RESULT_SUCCESS)) \
    { \
        msg_Err(aout, msg" (%" PRIu32 ")", (uint32_t)result); \
        goto error; \
    }

typedef SLresult (*slCreateEngine_t)(
        SLObjectItf*, SLuint32, const SLEngineOption*, SLuint32,
        const SLInterfaceID*, const SLboolean*);

#define Destroy(a) (*a)->Destroy(a);
#define SetPlayState(a, b) (*a)->SetPlayState(a, b)
#define RegisterCallback(a, b, c) (*a)->RegisterCallback(a, b, c)
#define GetInterface(a, b, c) (*a)->GetInterface(a, b, c)
#define Realize(a, b) (*a)->Realize(a, b)
#define CreateOutputMix(a, b, c, d, e) (*a)->CreateOutputMix(a, b, c, d, e)
#define CreateAudioPlayer(a, b, c, d, e, f, g) \
    (*a)->CreateAudioPlayer(a, b, c, d, e, f, g)
#define Enqueue(a, b, c) (*a)->Enqueue(a, b, c)
#define Clear(a) (*a)->Clear(a)
#define GetState(a, b) (*a)->GetState(a, b)
#define SetPositionUpdatePeriod(a, b) (*a)->SetPositionUpdatePeriod(a, b)
#define SetVolumeLevel(a, b) (*a)->SetVolumeLevel(a, b)
#define SetMute(a, b) (*a)->SetMute(a, b)

/*****************************************************************************
 *
 *****************************************************************************/
typedef struct
{
    /* OpenSL objects */
    SLObjectItf                     engineObject;
    SLObjectItf                     outputMixObject;
    SLAndroidSimpleBufferQueueItf   playerBufferQueue;
    SLObjectItf                     playerObject;
    SLVolumeItf                     volumeItf;
    SLEngineItf                     engineEngine;
    SLPlayItf                       playerPlay;

    /* OpenSL symbols */
    void                           *p_so_handle;

    slCreateEngine_t                slCreateEnginePtr;
    SLInterfaceID                   SL_IID_ENGINE;
    SLInterfaceID                   SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
    SLInterfaceID                   SL_IID_VOLUME;
    SLInterfaceID                   SL_IID_PLAY;

    /* */

    vlc_mutex_t                     lock;

    /* audio buffered through opensles */
    uint8_t                        *buf;
    size_t                          samples_per_buf;
    int                             next_buf;

    int                             rate;

    /* if we can measure latency already */
    bool                            started;

    /* audio not yet buffered through opensles */
    block_t                        *p_buffer_chain;
    block_t                       **pp_buffer_last;
    size_t                          samples;
} aout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open  (vlc_object_t *);
static void Close (vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_description("OpenSLES audio output")
    set_shortname("OpenSLES")
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)

    set_capability("audio output", 170)
    add_shortcut("opensles", "android")
    set_callbacks(Open, Close)
vlc_module_end ()

/*****************************************************************************
 *
 *****************************************************************************/

static inline int bytesPerSample(void)
{
    return 2 /* S16 */ * 2 /* stereo */;
}

static int TimeGet(audio_output_t* aout, vlc_tick_t* restrict drift)
{
    aout_sys_t *sys = aout->sys;

    SLAndroidSimpleBufferQueueState st;
    SLresult res = GetState(sys->playerBufferQueue, &st);
    if (unlikely(res != SL_RESULT_SUCCESS)) {
        msg_Err(aout, "Could not query buffer queue state in TimeGet (%" PRIu32 ")", (uint32_t)res);
        return -1;
    }

    vlc_mutex_lock(&sys->lock);
    bool started = sys->started;
    vlc_mutex_unlock(&sys->lock);

    if (!started)
        return -1;

    *drift = OPENSLES_BUFLEN * st.count
        + vlc_tick_from_samples(sys->samples, sys->rate);

    /* msg_Dbg(aout, "latency %"PRId64" ms, %d/%d buffers", *drift / 1000,
        (int)st.count, OPENSLES_BUFFERS); */

    return 0;
}

static void Flush(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    vlc_mutex_lock(&sys->lock);
    SetPlayState(sys->playerPlay, SL_PLAYSTATE_STOPPED);
    Clear(sys->playerBufferQueue);
    SetPlayState(sys->playerPlay, SL_PLAYSTATE_PLAYING);

    /* release audio data not yet written to opensles */
    block_ChainRelease(sys->p_buffer_chain);
    sys->p_buffer_chain = NULL;
    sys->pp_buffer_last = &sys->p_buffer_chain;

    sys->samples = 0;
    sys->started = false;

    vlc_mutex_unlock(&sys->lock);
}

static int VolumeSet(audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;
    if (!sys->volumeItf)
        return -1;

    /* Convert UI volume to linear factor (cube) */
    vol = vol * vol * vol;

    /* millibels from linear amplification */
    int mb = lroundf(2000.f * log10f(vol));
    if (mb < SL_MILLIBEL_MIN)
        mb = SL_MILLIBEL_MIN;
    else if (mb > 0)
        mb = 0; /* maximum supported level could be higher: GetMaxVolumeLevel */

    SLresult r = SetVolumeLevel(sys->volumeItf, mb);
    return (r == SL_RESULT_SUCCESS) ? 0 : -1;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;
    if (!sys->volumeItf)
        return -1;

    SLresult r = SetMute(sys->volumeItf, mute);
    return (r == SL_RESULT_SUCCESS) ? 0 : -1;
}

static void Pause(audio_output_t *aout, bool pause, vlc_tick_t date)
{
    (void)date;
    aout_sys_t *sys = aout->sys;
    SetPlayState(sys->playerPlay,
        pause ? SL_PLAYSTATE_PAUSED : SL_PLAYSTATE_PLAYING);
}

static int WriteBuffer(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    const size_t unit_size = sys->samples_per_buf * bytesPerSample();

    block_t *b = sys->p_buffer_chain;
    if (!b)
        return false;

    /* Check if we can fill at least one buffer unit by chaining blocks */
    if (b->i_buffer < unit_size) {
        if (!b->p_next)
            return false;
        ssize_t needed = unit_size - b->i_buffer;
        for (block_t *next = b->p_next; next; next = next->p_next) {
            needed -= next->i_buffer;
            if (needed <= 0)
                break;
        }

        if (needed > 0)
            return false;
    }

    SLAndroidSimpleBufferQueueState st;
    SLresult res = GetState(sys->playerBufferQueue, &st);
    if (unlikely(res != SL_RESULT_SUCCESS)) {
        msg_Err(aout, "Could not query buffer queue state in %s (%" PRIu32 ")",
                __func__, (uint32_t)res);
        return false;
    }

    if (st.count == OPENSLES_BUFFERS)
        return false;

    size_t done = 0;
    while (done < unit_size) {
        size_t cur = b->i_buffer;
        if (cur > unit_size - done)
            cur = unit_size - done;

        memcpy(&sys->buf[unit_size * sys->next_buf + done], b->p_buffer, cur);
        b->i_buffer -= cur;
        b->p_buffer += cur;
        done += cur;

        block_t *next = b->p_next;
        if (b->i_buffer == 0) {
            block_Release(b);
            b = NULL;
        }

        if (done == unit_size)
            break;
        else
            b = next;
    }

    sys->p_buffer_chain = b;
    if (!b)
        sys->pp_buffer_last = &sys->p_buffer_chain;

    SLresult r = Enqueue(sys->playerBufferQueue,
        &sys->buf[unit_size * sys->next_buf], unit_size);

    sys->samples -= sys->samples_per_buf;

    if (r == SL_RESULT_SUCCESS) {
        if (++sys->next_buf == OPENSLES_BUFFERS)
            sys->next_buf = 0;
        return true;
    } else {
        /* XXX : if writing fails, we don't retry */
        msg_Err(aout, "error %" PRIu32 " when writing %zu bytes %s",
                (uint32_t)r, b->i_buffer,
                (r == SL_RESULT_BUFFER_INSUFFICIENT) ? " (buffer insufficient)" : "");
        return false;
    }
}

/*****************************************************************************
 * Play: play a sound
 *****************************************************************************/
static void Play(audio_output_t *aout, block_t *p_buffer, vlc_tick_t date)
{
    aout_sys_t *sys = aout->sys;

    p_buffer->p_next = NULL; /* Make sur our linked list doesn't use old references */
    vlc_mutex_lock(&sys->lock);

    sys->samples += p_buffer->i_buffer / bytesPerSample();

    /* Hold this block until we can write it into the OpenSL buffer */
    block_ChainLastAppend(&sys->pp_buffer_last, p_buffer);

    /* Fill OpenSL buffer */
    while (WriteBuffer(aout))
        ;

    vlc_mutex_unlock(&sys->lock);
    (void) date;
}

static void PlayedCallback (SLAndroidSimpleBufferQueueItf caller, void *pContext)
{
    (void)caller;
    audio_output_t *aout = pContext;
    aout_sys_t *sys = aout->sys;

    assert (caller == sys->playerBufferQueue);

    vlc_mutex_lock(&sys->lock);
    sys->started = true;
    vlc_mutex_unlock(&sys->lock);
}

static int aout_get_native_sample_rate(audio_output_t *aout)
{
    JNIEnv *p_env;
    if (!(p_env = android_getEnv(VLC_OBJECT(aout), "opensles")))
        return -1;
    jclass cls = (*p_env)->FindClass (p_env, "android/media/AudioTrack");
    if ((*p_env)->ExceptionCheck(p_env))
    {
        (*p_env)->ExceptionClear(p_env);
        return -1;
    }
    jmethodID method = (*p_env)->GetStaticMethodID(p_env, cls,
                                                   "getNativeOutputSampleRate",
                                                   "(I)I");
    /* 3 for AudioManager.STREAM_MUSIC */
    int sample_rate = (*p_env)->CallStaticIntMethod(p_env, cls, method, 3);
    (*p_env)->DeleteLocalRef(p_env, cls);
    msg_Dbg(aout, "%s: %d", __func__, sample_rate);
    return sample_rate;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    if (aout_FormatNbChannels(fmt) == 0 || !AOUT_FMT_LINEAR(fmt))
        return VLC_EGENERIC;

    SLresult       result;

    aout_sys_t *sys = aout->sys;

    // configure audio source - this defines the number of samples you can enqueue.
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
        OPENSLES_BUFFERS
    };

    SLDataFormat_PCM format_pcm;
    format_pcm.formatType       = SL_DATAFORMAT_PCM;
    format_pcm.numChannels      = 2;
    format_pcm.samplesPerSec    = ((SLuint32) fmt->i_rate * 1000) ;
    format_pcm.bitsPerSample    = SL_PCMSAMPLEFORMAT_FIXED_16;
    format_pcm.containerSize    = SL_PCMSAMPLEFORMAT_FIXED_16;
    format_pcm.channelMask      = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    format_pcm.endianness       = SL_BYTEORDER_LITTLEENDIAN;

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {
        SL_DATALOCATOR_OUTPUTMIX,
        sys->outputMixObject
    };
    SLDataSink audioSnk = {&loc_outmix, NULL};

    //create audio player
    const SLInterfaceID ids2[] = { sys->SL_IID_ANDROIDSIMPLEBUFFERQUEUE, sys->SL_IID_VOLUME };
    static const SLboolean req2[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

    if (aout_get_native_sample_rate(aout) >= (int)fmt->i_rate) {
        result = CreateAudioPlayer(sys->engineEngine, &sys->playerObject, &audioSrc,
                                    &audioSnk, sizeof(ids2) / sizeof(*ids2),
                                    ids2, req2);
    } else {
        // Don't try to play back a sample rate higher than the native one,
        // since OpenSL ES will try to use the fast path, which AudioFlinger
        // will reject (fast path can't do resampling), and will end up with
        // too small buffers for the resampling. See http://b.android.com/59453
        // for details. This bug is still present in 4.4. If it is fixed later
        // this workaround could be made conditional.
        result = SL_RESULT_UNKNOWN_ERROR;
    }
    if (unlikely(result != SL_RESULT_SUCCESS)) {
        /* Try again with a more sensible samplerate */
        fmt->i_rate = 44100;
        format_pcm.samplesPerSec = ((SLuint32) 44100 * 1000) ;
        result = CreateAudioPlayer(sys->engineEngine, &sys->playerObject, &audioSrc,
                &audioSnk, sizeof(ids2) / sizeof(*ids2),
                ids2, req2);
    }
    CHECK_OPENSL_ERROR("Failed to create audio player");

    result = Realize(sys->playerObject, SL_BOOLEAN_FALSE);
    CHECK_OPENSL_ERROR("Failed to realize player object.");

    result = GetInterface(sys->playerObject, sys->SL_IID_PLAY, &sys->playerPlay);
    CHECK_OPENSL_ERROR("Failed to get player interface.");

    result = GetInterface(sys->playerObject, sys->SL_IID_VOLUME, &sys->volumeItf);
    CHECK_OPENSL_ERROR("failed to get volume interface.");

    result = GetInterface(sys->playerObject, sys->SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                  &sys->playerBufferQueue);
    CHECK_OPENSL_ERROR("Failed to get buff queue interface");

    result = RegisterCallback(sys->playerBufferQueue, PlayedCallback,
                                   (void*)aout);
    CHECK_OPENSL_ERROR("Failed to register buff queue callback.");

    // set the player's state to playing
    result = SetPlayState(sys->playerPlay, SL_PLAYSTATE_PLAYING);
    CHECK_OPENSL_ERROR("Failed to switch to playing state");

    /* XXX: rounding shouldn't affect us at normal sampling rate */
    sys->rate = fmt->i_rate;
    sys->samples_per_buf = samples_from_vlc_tick(OPENSLES_BUFLEN, fmt->i_rate);
    sys->buf = vlc_alloc(sys->samples_per_buf * bytesPerSample(), OPENSLES_BUFFERS);
    if (!sys->buf)
        goto error;

    sys->started = false;
    sys->next_buf = 0;

    sys->p_buffer_chain = NULL;
    sys->pp_buffer_last = &sys->p_buffer_chain;
    sys->samples = 0;

    // we want 16bit signed data native endian.
    fmt->i_format              = VLC_CODEC_S16N;
    fmt->i_physical_channels   = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;

    SetPositionUpdatePeriod(sys->playerPlay, MS_FROM_VLC_TICK(AOUT_MIN_PREPARE_TIME));

    aout_FormatPrepare(fmt);

    return VLC_SUCCESS;

error:
    if (sys->playerObject) {
        Destroy(sys->playerObject);
        sys->playerObject = NULL;
        sys->playerBufferQueue = NULL;
        sys->volumeItf = NULL;
        sys->playerPlay = NULL;
    }

    return VLC_EGENERIC;
}

static void Stop(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    SetPlayState(sys->playerPlay, SL_PLAYSTATE_STOPPED);
    //Flush remaining buffers if any.
    Clear(sys->playerBufferQueue);

    free(sys->buf);
    block_ChainRelease(sys->p_buffer_chain);

    Destroy(sys->playerObject);
    sys->playerObject = NULL;
    sys->playerBufferQueue = NULL;
    sys->volumeItf = NULL;
    sys->playerPlay = NULL;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    Destroy(sys->outputMixObject);
    Destroy(sys->engineObject);
    dlclose(sys->p_so_handle);
    free(sys);
}

static int Open (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys;
    SLresult result;

    aout->sys = sys = calloc(1, sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->p_so_handle = dlopen("libOpenSLES.so", RTLD_NOW);
    if (sys->p_so_handle == NULL)
    {
        msg_Err(aout, "Failed to load libOpenSLES");
        goto error;
    }

    sys->slCreateEnginePtr = dlsym(sys->p_so_handle, "slCreateEngine");
    if (unlikely(sys->slCreateEnginePtr == NULL))
    {
        msg_Err(aout, "Failed to load symbol slCreateEngine");
        goto error;
    }

#define OPENSL_DLSYM(dest, name)                       \
    do {                                                       \
        const SLInterfaceID *sym = dlsym(sys->p_so_handle, "SL_IID_"name);        \
        if (unlikely(sym == NULL))                             \
        {                                                      \
            msg_Err(aout, "Failed to load symbol SL_IID_"name); \
            goto error;                                        \
        }                                                      \
        sys->dest = *sym;                                           \
    } while(0)

    OPENSL_DLSYM(SL_IID_ANDROIDSIMPLEBUFFERQUEUE, "ANDROIDSIMPLEBUFFERQUEUE");
    OPENSL_DLSYM(SL_IID_ENGINE, "ENGINE");
    OPENSL_DLSYM(SL_IID_PLAY, "PLAY");
    OPENSL_DLSYM(SL_IID_VOLUME, "VOLUME");
#undef OPENSL_DLSYM

    // create engine
    result = sys->slCreateEnginePtr(&sys->engineObject, 0, NULL, 0, NULL, NULL);
    CHECK_OPENSL_ERROR("Failed to create engine");

    // realize the engine in synchronous mode
    result = Realize(sys->engineObject, SL_BOOLEAN_FALSE);
    CHECK_OPENSL_ERROR("Failed to realize engine");

    // get the engine interface, needed to create other objects
    result = GetInterface(sys->engineObject, sys->SL_IID_ENGINE, &sys->engineEngine);
    CHECK_OPENSL_ERROR("Failed to get the engine interface");

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids1[] = { sys->SL_IID_VOLUME };
    const SLboolean req1[] = { SL_BOOLEAN_FALSE };
    result = CreateOutputMix(sys->engineEngine, &sys->outputMixObject, 1, ids1, req1);
    CHECK_OPENSL_ERROR("Failed to create output mix");

    // realize the output mix in synchronous mode
    result = Realize(sys->outputMixObject, SL_BOOLEAN_FALSE);
    CHECK_OPENSL_ERROR("Failed to realize output mix");

    vlc_mutex_init(&sys->lock);

    aout->start      = Start;
    aout->stop       = Stop;
    aout->time_get   = TimeGet;
    aout->play       = Play;
    aout->pause      = Pause;
    aout->flush      = Flush;
    aout->mute_set   = MuteSet;
    aout->volume_set = VolumeSet;

    return VLC_SUCCESS;

error:
    if (sys->outputMixObject)
        Destroy(sys->outputMixObject);
    if (sys->engineObject)
        Destroy(sys->engineObject);
    if (sys->p_so_handle)
        dlclose(sys->p_so_handle);
    free(sys);
    return VLC_EGENERIC;
}
