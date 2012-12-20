/*****************************************************************************
 * opensles_android.c : audio output for android native code
 *****************************************************************************
 * Copyright © 2011-2012 VideoLAN
 *
 * Authors: Dominique Martinet <asmadeus@codewreck.org>
 *          Hugo Beauzée-Luyssen <beauze.h@gmail.com>
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

#define CHECK_OPENSL_ERROR( msg )                   \
    if( unlikely( result != SL_RESULT_SUCCESS ) )   \
    {                                               \
        msg_Err( aout, msg" (%lu)", result );       \
        goto error;                                 \
    }

typedef SLresult (*slCreateEngine_t)(
        SLObjectItf*, SLuint32, const SLEngineOption*, SLuint32,
        const SLInterfaceID*, const SLboolean* );

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
 * aout_sys_t: audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    SLObjectItf                     engineObject;
    SLObjectItf                     outputMixObject;
    SLAndroidSimpleBufferQueueItf   playerBufferQueue;
    SLObjectItf                     playerObject;
    SLVolumeItf                     volumeItf;
    SLEngineItf                     engineEngine;
    SLPlayItf                       playerPlay;

    vlc_mutex_t                     lock;
    mtime_t                         length;

    int                             buffers;

    /* audio buffered through opensles */
    block_t                        *p_chain;
    block_t                       **pp_last;

    /* audio not yet buffered through opensles */
    block_t                        *p_buffer_chain;
    block_t                       **pp_buffer_last;

    void                           *p_so_handle;

    slCreateEngine_t                slCreateEnginePtr;
    SLInterfaceID                   SL_IID_ENGINE;
    SLInterfaceID                   SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
    SLInterfaceID                   SL_IID_VOLUME;
    SLInterfaceID                   SL_IID_PLAY;

    audio_sample_format_t           format;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open  (vlc_object_t *);
static void Close (vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_description( N_("OpenSLES audio output") )
    set_shortname( N_("OpenSLES") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )

    set_capability( "audio output", 170 )
    add_shortcut( "opensles", "android" )
    set_callbacks( Open, Close )
vlc_module_end ()


static void Flush(audio_output_t *aout, bool drain)
{
    aout_sys_t *sys = aout->sys;

    if (drain) {
        mtime_t delay;
        vlc_mutex_lock( &sys->lock );
        delay = sys->length;
        vlc_mutex_unlock( &sys->lock );
        msleep(delay);
    } else {
        vlc_mutex_lock( &sys->lock );
        SetPlayState( sys->playerPlay, SL_PLAYSTATE_STOPPED );
        Clear( sys->playerBufferQueue );
        SetPlayState( sys->playerPlay, SL_PLAYSTATE_PLAYING );

        sys->length = 0;
        sys->buffers = 0;

        /* release audio data not yet written to opensles */
        block_ChainRelease( sys->p_buffer_chain );
        sys->p_buffer_chain = NULL;
        sys->pp_buffer_last = &sys->p_buffer_chain;

        /* release audio data written to opensles, but not yet
         * played on hardware */
        block_ChainRelease( sys->p_chain );
        sys->p_chain = NULL;
        sys->pp_last = &sys->p_chain;

        vlc_mutex_unlock( &sys->lock );
    }
}

static int VolumeSet(audio_output_t *aout, float vol)
{
    /* Convert UI volume to linear factor (cube) */
    vol = vol * vol * vol;

    /* millibels from linear amplification */
    int mb = lroundf(2000.f * log10f(vol));
    if (mb < SL_MILLIBEL_MIN)
        mb = SL_MILLIBEL_MIN;
    else if (mb > 0)
        mb = 0; /* maximum supported level could be higher: GetMaxVolumeLevel */

    SLresult r = SetVolumeLevel(aout->sys->volumeItf, mb);
    return (r == SL_RESULT_SUCCESS) ? 0 : -1;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    SLresult r = SetMute(aout->sys->volumeItf, mute);
    return (r == SL_RESULT_SUCCESS) ? 0 : -1;
}

static void Pause(audio_output_t *aout, bool pause, mtime_t date)
{
    (void)date;
    aout_sys_t *sys = aout->sys;
    SetPlayState( sys->playerPlay,
        pause ? SL_PLAYSTATE_PAUSED : SL_PLAYSTATE_PLAYING );
}

static int TimeGet(audio_output_t* aout, mtime_t* restrict drift)
{
    aout_sys_t *sys = aout->sys;

    vlc_mutex_lock( &sys->lock );
    mtime_t delay = sys->length;
    vlc_mutex_unlock( &sys->lock );

    SLAndroidSimpleBufferQueueState st;
    SLresult res = GetState(sys->playerBufferQueue, &st);
    if (unlikely(res != SL_RESULT_SUCCESS)) {
        msg_Err(aout, "Could not query buffer queue state in TimeGet (%lu)", res);
        return -1;
    }

    if (delay == 0 || st.count == 0)
        return -1;

    *drift = delay;
    return 0;
}

static int WriteBuffer(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    block_t *b = sys->p_buffer_chain;
    if (!b)
        return false;

    if (!b->i_length)
        b->i_length = (mtime_t)(b->i_buffer / 2 / sys->format.i_channels) * CLOCK_FREQ / sys->format.i_rate;

    /* If something bad happens, we must remove this buffer from the FIFO */
    block_t **pp_last_saved = sys->pp_last;
    block_t *p_last_saved = *pp_last_saved;
    block_t *next_saved = b->p_next;
    b->p_next = NULL;

    /* Put this block in the list of audio already written to opensles */
    block_ChainLastAppend( &sys->pp_last, b );

    mtime_t len = b->i_length;
    sys->length += len;
    block_t *next = b->p_next;

    vlc_mutex_unlock( &sys->lock );
    SLresult r = Enqueue( sys->playerBufferQueue, b->p_buffer, b->i_buffer );
    vlc_mutex_lock( &sys->lock );

    if (r == SL_RESULT_SUCCESS) {
        /* Remove that block from the list of audio not yet written */
        sys->buffers++;
        sys->p_buffer_chain = next;
        if (!sys->p_buffer_chain)
            sys->pp_buffer_last = &sys->p_buffer_chain;
    } else {
        /* Remove that block from the list of audio already written */
        msg_Err( aout, "error %lu when writing %d bytes, %d/255 buffers occupied %s",
                r, b->i_buffer, sys->buffers,
                (r == SL_RESULT_BUFFER_INSUFFICIENT) ? " (buffer insufficient)" : "");

        sys->pp_last = pp_last_saved;
        *pp_last_saved = p_last_saved;

        b->p_next = next_saved;

        sys->length -= len;
        next = NULL; /* We'll try again next time */
    }

    return next != NULL;
}

/*****************************************************************************
 * Play: play a sound
 *****************************************************************************/
static void Play( audio_output_t *aout, block_t *p_buffer )
{
    aout_sys_t *sys = aout->sys;

    p_buffer->p_next = NULL; /* Make sur our linked list doesn't use old references */
    vlc_mutex_lock(&sys->lock);
    block_ChainLastAppend( &sys->pp_buffer_last, p_buffer );
    while (WriteBuffer(aout))
        ;
    vlc_mutex_unlock( &sys->lock );
}

static void PlayedCallback (SLAndroidSimpleBufferQueueItf caller, void *pContext )
{
    (void)caller;
    block_t *p_block;
    audio_output_t *aout = pContext;
    aout_sys_t *sys = aout->sys;

    assert (caller == sys->playerBufferQueue);

    vlc_mutex_lock( &sys->lock );
    sys->buffers--;

    p_block = sys->p_chain;
    assert( p_block );

    sys->p_chain = sys->p_chain->p_next;
    /* if we exhausted our fifo, we must reset the pointer to the last
     * appended block */
    if (!sys->p_chain)
        sys->pp_last = &sys->p_chain;

    sys->length -= p_block->i_length;

    vlc_mutex_unlock( &sys->lock );

    block_Release( p_block );
}
/*****************************************************************************
 *
 *****************************************************************************/
static void Clean( aout_sys_t *sys )
{
    if( sys->playerObject )
        Destroy( sys->playerObject );
    if( sys->outputMixObject )
        Destroy( sys->outputMixObject );
    if( sys->engineObject )
        Destroy( sys->engineObject );
}

static int Start( audio_output_t *aout, audio_sample_format_t *restrict fmt )
{
    SLresult       result;

    aout_sys_t *sys = aout->sys;

    // configure audio source - this defines the number of samples you can enqueue.
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
        255 // Maximum number of buffers to enqueue.
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
    result = CreateAudioPlayer( sys->engineEngine, &sys->playerObject, &audioSrc,
                                    &audioSnk, sizeof( ids2 ) / sizeof( *ids2 ),
                                    ids2, req2 );
    CHECK_OPENSL_ERROR( "Failed to create audio player" );

    result = Realize( sys->playerObject, SL_BOOLEAN_FALSE );
    CHECK_OPENSL_ERROR( "Failed to realize player object." );

    result = GetInterface( sys->playerObject, sys->SL_IID_PLAY, &sys->playerPlay );
    CHECK_OPENSL_ERROR( "Failed to get player interface." );

    result = GetInterface( sys->playerObject, sys->SL_IID_VOLUME, &sys->volumeItf );
    CHECK_OPENSL_ERROR( "failed to get volume interface." );

    result = GetInterface( sys->playerObject, sys->SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                  &sys->playerBufferQueue );
    CHECK_OPENSL_ERROR( "Failed to get buff queue interface" );

    result = RegisterCallback( sys->playerBufferQueue, PlayedCallback,
                                   (void*)aout);
    CHECK_OPENSL_ERROR( "Failed to register buff queue callback." );

    // set the player's state to playing
    result = SetPlayState( sys->playerPlay, SL_PLAYSTATE_PLAYING );
    CHECK_OPENSL_ERROR( "Failed to switch to playing state" );

    sys->p_chain = NULL;
    sys->pp_last = &sys->p_chain;
    sys->p_buffer_chain = NULL;
    sys->pp_buffer_last = &sys->p_buffer_chain;

    // we want 16bit signed data native endian.
    fmt->i_format              = VLC_CODEC_S16N;
    fmt->i_physical_channels   = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;

    SetPositionUpdatePeriod( sys->playerPlay, AOUT_MIN_PREPARE_TIME * 1000 / CLOCK_FREQ);

    aout_FormatPrepare( fmt );

    sys->format = *fmt;

    return VLC_SUCCESS;

error:
    Clean(sys);
    return VLC_EGENERIC;
}

static void Stop( audio_output_t *aout )
{
    aout_sys_t *sys = aout->sys;

    SetPlayState( sys->playerPlay, SL_PLAYSTATE_STOPPED );
    //Flush remaining buffers if any.
    Clear( sys->playerBufferQueue );
    block_ChainRelease( sys->p_chain );
    block_ChainRelease( sys->p_buffer_chain);
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    Clean(sys);
    dlclose(sys->p_so_handle);
    vlc_mutex_destroy(&sys->lock);
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

    OPENSL_DLSYM( SL_IID_ANDROIDSIMPLEBUFFERQUEUE, "ANDROIDSIMPLEBUFFERQUEUE" );
    OPENSL_DLSYM( SL_IID_ENGINE, "ENGINE" );
    OPENSL_DLSYM( SL_IID_PLAY, "PLAY" );
    OPENSL_DLSYM( SL_IID_VOLUME, "VOLUME" );
#undef OPENSL_DLSYM

    // create engine
    result = sys->slCreateEnginePtr( &sys->engineObject, 0, NULL, 0, NULL, NULL );
    CHECK_OPENSL_ERROR( "Failed to create engine" );

    // realize the engine in synchronous mode
    result = Realize( sys->engineObject, SL_BOOLEAN_FALSE );
    CHECK_OPENSL_ERROR( "Failed to realize engine" );

    // get the engine interface, needed to create other objects
    result = GetInterface( sys->engineObject, sys->SL_IID_ENGINE, &sys->engineEngine );
    CHECK_OPENSL_ERROR( "Failed to get the engine interface" );

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids1[] = { sys->SL_IID_VOLUME };
    const SLboolean req1[] = { SL_BOOLEAN_FALSE };
    result = CreateOutputMix( sys->engineEngine, &sys->outputMixObject, 1, ids1, req1 );
    CHECK_OPENSL_ERROR( "Failed to create output mix" );

    // realize the output mix in synchronous mode
    result = Realize( sys->outputMixObject, SL_BOOLEAN_FALSE );
    CHECK_OPENSL_ERROR( "Failed to realize output mix" );

    vlc_mutex_init( &sys->lock );

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
    Clean(sys);
    if (sys->p_so_handle)
        dlclose(sys->p_so_handle);
    free(sys);
    return VLC_EGENERIC;
}
