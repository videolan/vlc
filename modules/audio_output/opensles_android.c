/*****************************************************************************
 * opensles_android.c : audio output for android native code
 *****************************************************************************
 * Copyright © 2011 VideoLAN
 *
 * Authors: Dominique Martinet <asmadeus@codewreck.org>
 *          Hugo Beauzée-Luyssen <beauze.h@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
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

// For native audio
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

// Maximum number of buffers to enqueue.
#define BUFF_QUEUE  42

/*****************************************************************************
 * aout_sys_t: audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    SLObjectItf                     engineObject;
    SLEngineItf                     engineEngine;
    SLObjectItf                     outputMixObject;
    SLAndroidSimpleBufferQueueItf   playerBufferQueue;
    SLObjectItf                     playerObject;
    SLPlayItf                       playerPlay;
    aout_buffer_t                 * p_buffer_array[BUFF_QUEUE];
    int                             i_toclean_buffer;
    int                             i_toappend_buffer;
    SLInterfaceID                 * SL_IID_ENGINE;
    SLInterfaceID                 * SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
    SLInterfaceID                 * SL_IID_VOLUME;
    SLInterfaceID                 * SL_IID_PLAY;
    void                          * p_so_handle;
};

typedef SLresult (*slCreateEngine_t)(
        SLObjectItf*, SLuint32, const SLEngineOption*, SLuint32,
        const SLInterfaceID*, const SLboolean* );

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );
static void Play        ( audio_output_t * );
static void PlayedCallback ( SLAndroidSimpleBufferQueueItf caller,  void *pContext);

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


#define CHECK_OPENSL_ERROR( res, msg )              \
    if( unlikely( res != SL_RESULT_SUCCESS ) )      \
    {                                               \
        msg_Err( p_aout, msg" (%lu)", res );        \
        goto error;                                 \
    }

#define OPENSL_DLSYM( dest, handle, name )                   \
    dest = (typeof(dest))dlsym( handle, name );              \
    if( dest == NULL )                                       \
    {                                                        \
        msg_Err( p_aout, "Failed to load symbol %s", name ); \
        goto error;                                          \
    }

static void Clear( aout_sys_t *p_sys )
{
    // Destroy buffer queue audio player object
    // and invalidate all associated interfaces
    if( p_sys->playerObject != NULL )
        (*p_sys->playerObject)->Destroy( p_sys->playerObject );

    // destroy output mix object, and invalidate all associated interfaces
    if( p_sys->outputMixObject != NULL )
        (*p_sys->outputMixObject)->Destroy( p_sys->outputMixObject );

    // destroy engine object, and invalidate all associated interfaces
    if( p_sys->engineObject != NULL )
        (*p_sys->engineObject)->Destroy( p_sys->engineObject );

    if( p_sys->p_so_handle != NULL )
        dlclose( p_sys->p_so_handle );

    free( p_sys );
}

/*****************************************************************************
 * Open: open a dummy audio device
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    audio_output_t     *p_aout = (audio_output_t *)p_this;
    SLresult            result;

    /* Allocate structure */
    p_aout->sys = malloc( sizeof( aout_sys_t ) );
    if( unlikely( p_aout->sys == NULL ) )
        return VLC_ENOMEM;

    aout_sys_t * p_sys = p_aout->sys;

    p_sys->playerObject     = NULL;
    p_sys->engineObject     = NULL;
    p_sys->outputMixObject  = NULL;
    p_sys->i_toclean_buffer = 0;
    p_sys->i_toappend_buffer= 0;

    //Acquiring LibOpenSLES symbols :
    p_sys->p_so_handle = dlopen( "libOpenSLES.so", RTLD_NOW );
    if( p_sys->p_so_handle == NULL )
    {
        msg_Err( p_aout, "Failed to load libOpenSLES" );
        goto error;
    }

    slCreateEngine_t    slCreateEnginePtr = NULL;

    OPENSL_DLSYM( slCreateEnginePtr, p_sys->p_so_handle, "slCreateEngine" );
    OPENSL_DLSYM( p_sys->SL_IID_ANDROIDSIMPLEBUFFERQUEUE, p_sys->p_so_handle,
                 "SL_IID_ANDROIDSIMPLEBUFFERQUEUE" );
    OPENSL_DLSYM( p_sys->SL_IID_ENGINE, p_sys->p_so_handle, "SL_IID_ENGINE" );
    OPENSL_DLSYM( p_sys->SL_IID_PLAY, p_sys->p_so_handle, "SL_IID_PLAY" );
    OPENSL_DLSYM( p_sys->SL_IID_VOLUME, p_sys->p_so_handle, "SL_IID_VOLUME" );

    // create engine
    result = slCreateEnginePtr( &p_sys->engineObject, 0, NULL, 0, NULL, NULL );
    CHECK_OPENSL_ERROR( result, "Failed to create engine" );

    // realize the engine in synchronous mode
    result = (*p_sys->engineObject)->Realize( p_sys->engineObject,
                                             SL_BOOLEAN_FALSE );
    CHECK_OPENSL_ERROR( result, "Failed to realize engine" );

    // get the engine interface, needed to create other objects
    result = (*p_sys->engineObject)->GetInterface( p_sys->engineObject,
                                        *p_sys->SL_IID_ENGINE, &p_sys->engineEngine );
    CHECK_OPENSL_ERROR( result, "Failed to get the engine interface" );

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids1[] = { *p_sys->SL_IID_VOLUME };
    const SLboolean req1[] = { SL_BOOLEAN_FALSE };
    result = (*p_sys->engineEngine)->CreateOutputMix( p_sys->engineEngine,
                                        &p_sys->outputMixObject, 1, ids1, req1 );
    CHECK_OPENSL_ERROR( result, "Failed to create output mix" );

    // realize the output mix in synchronous mode
    result = (*p_sys->outputMixObject)->Realize( p_sys->outputMixObject,
                                                 SL_BOOLEAN_FALSE );
    CHECK_OPENSL_ERROR( result, "Failed to realize output mix" );


    // configure audio source - this defines the number of samples you can enqueue.
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
        BUFF_QUEUE
    };

    SLDataFormat_PCM format_pcm;
    format_pcm.formatType       = SL_DATAFORMAT_PCM;
    format_pcm.numChannels      = 2;
    format_pcm.samplesPerSec    = ((SLuint32) p_aout->format.i_rate * 1000) ;
    format_pcm.bitsPerSample    = SL_PCMSAMPLEFORMAT_FIXED_16;
    format_pcm.containerSize    = SL_PCMSAMPLEFORMAT_FIXED_16;
    format_pcm.channelMask      = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    format_pcm.endianness       = SL_BYTEORDER_LITTLEENDIAN;

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {
        SL_DATALOCATOR_OUTPUTMIX,
        p_sys->outputMixObject
    };
    SLDataSink audioSnk = {&loc_outmix, NULL};

    //create audio player
    const SLInterfaceID ids2[] = { *p_sys->SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
    const SLboolean     req2[] = { SL_BOOLEAN_TRUE };
    result = (*p_sys->engineEngine)->CreateAudioPlayer( p_sys->engineEngine,
                                    &p_sys->playerObject, &audioSrc,
                                    &audioSnk, sizeof( ids2 ) / sizeof( *ids2 ),
                                    ids2, req2 );
    CHECK_OPENSL_ERROR( result, "Failed to create audio player" );

    // realize the player
    result = (*p_sys->playerObject)->Realize( p_sys->playerObject,
                                              SL_BOOLEAN_FALSE );
    CHECK_OPENSL_ERROR( result, "Failed to realize player object." );

    // get the play interface
    result = (*p_sys->playerObject)->GetInterface( p_sys->playerObject,
                                                  *p_sys->SL_IID_PLAY, &p_sys->playerPlay );
    CHECK_OPENSL_ERROR( result, "Failed to get player interface." );

    // get the buffer queue interface
    result = (*p_sys->playerObject)->GetInterface( p_sys->playerObject,
                                                  *p_sys->SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                  &p_sys->playerBufferQueue );
    CHECK_OPENSL_ERROR( result, "Failed to get buff queue interface" );

    result = (*p_sys->playerBufferQueue)->RegisterCallback( p_sys->playerBufferQueue,
                                                            PlayedCallback,
                                                            (void*)p_sys);
    CHECK_OPENSL_ERROR( result, "Failed to register buff queue callback." );


    // set the player's state to playing
    result = (*p_sys->playerPlay)->SetPlayState( p_sys->playerPlay,
                                                 SL_PLAYSTATE_PLAYING );
    CHECK_OPENSL_ERROR( result, "Failed to switch to playing state" );

    // we want 16bit signed data little endian.
    p_aout->format.i_format              = VLC_CODEC_S16L;
    p_aout->i_nb_samples                 = 2048;
    p_aout->format.i_physical_channels   = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    p_aout->pf_play                      = Play;
    p_aout->pf_pause = NULL;

    aout_FormatPrepare( &p_aout->format );

    return VLC_SUCCESS;
error:
    Clear( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close our file
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    aout_sys_t      *p_sys = p_aout->sys;

    msg_Dbg( p_aout, "Closing OpenSLES" );

    (*p_sys->playerPlay)->SetPlayState( p_sys->playerPlay, SL_PLAYSTATE_STOPPED );
    //Flush remaining buffers if any.
    if( p_sys->playerBufferQueue != NULL )
        (*p_sys->playerBufferQueue)->Clear( p_sys->playerBufferQueue );
    Clear( p_sys );
}

/*****************************************************************************
 * Play: play a sound
 *****************************************************************************/
static void Play( audio_output_t * p_aout )
{
    aout_sys_t * p_sys = p_aout->sys;
    aout_buffer_t *p_buffer;

    SLresult result;

    p_buffer = aout_FifoPop(&p_aout->fifo);
    if( p_buffer != NULL )
    {
        for (;;)
        {
            result = (*p_sys->playerBufferQueue)->Enqueue(
                            p_sys->playerBufferQueue, p_buffer->p_buffer,
                            p_buffer->i_buffer );
            if( result == SL_RESULT_SUCCESS )
                break;
            if ( result != SL_RESULT_BUFFER_INSUFFICIENT )
            {
                msg_Warn( p_aout, "Dropping invalid buffer" );
                aout_BufferFree( p_buffer );
                return ;
            }

            msg_Err( p_aout, "write error (%lu)", result );

            // Wait a bit to retry. might miss calls to *cancel
            // but this is supposed to be rare anyway
            msleep(CLOCK_FREQ);
        }
        p_sys->p_buffer_array[p_sys->i_toappend_buffer] = p_buffer;
        if( ++p_sys->i_toappend_buffer == BUFF_QUEUE )
            p_sys->i_toappend_buffer = 0;
    }
    else
        msg_Err( p_aout, "nothing to play?" );
}

static void PlayedCallback (SLAndroidSimpleBufferQueueItf caller, void *pContext )
{
    aout_sys_t *p_sys = (aout_sys_t*)pContext;

    assert (caller == p_sys->playerBufferQueue);

    aout_BufferFree( p_sys->p_buffer_array[p_sys->i_toclean_buffer] );
    if( ++p_sys->i_toclean_buffer == BUFF_QUEUE )
        p_sys->i_toclean_buffer = 0;
}
