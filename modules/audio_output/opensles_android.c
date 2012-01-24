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

    SLPlayItf                       playerPlay;

    vlc_mutex_t                     lock;
    block_t                        *p_chain;
    block_t                       **pp_last;

    void                           *p_so_handle;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );

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


static void Clean( aout_sys_t *p_sys )
{
    if( p_sys->playerObject )
        Destroy( p_sys->playerObject );
    if( p_sys->outputMixObject )
        Destroy( p_sys->outputMixObject );
    if( p_sys->engineObject )
        Destroy( p_sys->engineObject );

    if( p_sys->p_so_handle )
        dlclose( p_sys->p_so_handle );

    free( p_sys );
}

/*****************************************************************************
 * Play: play a sound
 *****************************************************************************/
static void Play( audio_output_t *p_aout, block_t *p_buffer )
{
    aout_sys_t *p_sys = p_aout->sys;
    int tries = 5;
    block_t **pp_last, *p_block;;

    vlc_mutex_lock( &p_sys->lock );

    /* If something bad happens, we must remove this buffer from the FIFO */
    pp_last = p_sys->pp_last;
    p_block = *pp_last;

    block_ChainLastAppend( &p_sys->pp_last, p_buffer );
    vlc_mutex_unlock( &p_sys->lock );

    for (;;)
    {
        SLresult result = Enqueue( p_sys->playerBufferQueue, p_buffer->p_buffer,
                            p_buffer->i_buffer );

        switch (result)
        {
        case SL_RESULT_BUFFER_INSUFFICIENT:
            msg_Err( p_aout, "buffer insufficient");

            if (tries--)
            {
                // Wait a bit to retry.
                msleep(CLOCK_FREQ);
                continue;
            }

        default:
            msg_Warn( p_aout, "Error %lu, dropping buffer", result );
            vlc_mutex_lock( &p_sys->lock );
            p_sys->pp_last = pp_last;
            *pp_last = p_block;
            vlc_mutex_unlock( &p_sys->lock );
            block_Release( p_buffer );
        case SL_RESULT_SUCCESS:
            return;
        }
    }
}

static void PlayedCallback (SLAndroidSimpleBufferQueueItf caller, void *pContext )
{
    block_t *p_block;
    aout_sys_t *p_sys = pContext;

    assert (caller == p_sys->playerBufferQueue);

    vlc_mutex_lock( &p_sys->lock );

    p_block = p_sys->p_chain;
    assert( p_block );

    p_sys->p_chain = p_sys->p_chain->p_next;
    /* if we exhausted our fifo, we must reset the pointer to the last
     * appended block */
    if (!p_sys->p_chain)
        p_sys->pp_last = &p_sys->p_chain;

    vlc_mutex_unlock( &p_sys->lock );

    block_Release( p_block );
}
/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    SLresult       result;
    SLEngineItf    engineEngine;

    /* Allocate structure */
    p_aout->sys = calloc( 1, sizeof( aout_sys_t ) );
    if( unlikely( p_aout->sys == NULL ) )
        return VLC_ENOMEM;

    aout_sys_t *p_sys = p_aout->sys;

    //Acquiring LibOpenSLES symbols :
    p_sys->p_so_handle = dlopen( "libOpenSLES.so", RTLD_NOW );
    if( p_sys->p_so_handle == NULL )
    {
        msg_Err( p_aout, "Failed to load libOpenSLES" );
        goto error;
    }

    typedef SLresult (*slCreateEngine_t)(
            SLObjectItf*, SLuint32, const SLEngineOption*, SLuint32,
            const SLInterfaceID*, const SLboolean* );
    slCreateEngine_t    slCreateEnginePtr = NULL;

    SLInterfaceID *SL_IID_ENGINE;
    SLInterfaceID *SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
    SLInterfaceID *SL_IID_VOLUME;
    SLInterfaceID *SL_IID_PLAY;

#define OPENSL_DLSYM( dest, handle, name )                   \
    dest = dlsym( handle, name );                            \
    if( dest == NULL )                                       \
    {                                                        \
        msg_Err( p_aout, "Failed to load symbol %s", name ); \
        goto error;                                          \
    }

    OPENSL_DLSYM( slCreateEnginePtr, p_sys->p_so_handle, "slCreateEngine" );
    OPENSL_DLSYM( SL_IID_ANDROIDSIMPLEBUFFERQUEUE, p_sys->p_so_handle,
                 "SL_IID_ANDROIDSIMPLEBUFFERQUEUE" );
    OPENSL_DLSYM( SL_IID_ENGINE, p_sys->p_so_handle, "SL_IID_ENGINE" );
    OPENSL_DLSYM( SL_IID_PLAY, p_sys->p_so_handle, "SL_IID_PLAY" );
    OPENSL_DLSYM( SL_IID_VOLUME, p_sys->p_so_handle, "SL_IID_VOLUME" );


#define CHECK_OPENSL_ERROR( msg )                   \
    if( unlikely( result != SL_RESULT_SUCCESS ) )   \
    {                                               \
        msg_Err( p_aout, msg" (%lu)", result );     \
        goto error;                                 \
    }

    // create engine
    result = slCreateEnginePtr( &p_sys->engineObject, 0, NULL, 0, NULL, NULL );
    CHECK_OPENSL_ERROR( "Failed to create engine" );

    // realize the engine in synchronous mode
    result = Realize( p_sys->engineObject, SL_BOOLEAN_FALSE );
    CHECK_OPENSL_ERROR( "Failed to realize engine" );

    // get the engine interface, needed to create other objects
    result = GetInterface( p_sys->engineObject, *SL_IID_ENGINE, &engineEngine );
    CHECK_OPENSL_ERROR( "Failed to get the engine interface" );

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids1[] = { *SL_IID_VOLUME };
    const SLboolean req1[] = { SL_BOOLEAN_FALSE };
    result = CreateOutputMix( engineEngine, &p_sys->outputMixObject, 1, ids1, req1 );
    CHECK_OPENSL_ERROR( "Failed to create output mix" );

    // realize the output mix in synchronous mode
    result = Realize( p_sys->outputMixObject, SL_BOOLEAN_FALSE );
    CHECK_OPENSL_ERROR( "Failed to realize output mix" );


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
    const SLInterfaceID ids2[] = { *SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
    static const SLboolean req2[] = { SL_BOOLEAN_TRUE };
    result = CreateAudioPlayer( engineEngine, &p_sys->playerObject, &audioSrc,
                                    &audioSnk, sizeof( ids2 ) / sizeof( *ids2 ),
                                    ids2, req2 );
    CHECK_OPENSL_ERROR( "Failed to create audio player" );

    result = Realize( p_sys->playerObject, SL_BOOLEAN_FALSE );
    CHECK_OPENSL_ERROR( "Failed to realize player object." );

    result = GetInterface( p_sys->playerObject, *SL_IID_PLAY, &p_sys->playerPlay );
    CHECK_OPENSL_ERROR( "Failed to get player interface." );

    result = GetInterface( p_sys->playerObject, *SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                  &p_sys->playerBufferQueue );
    CHECK_OPENSL_ERROR( "Failed to get buff queue interface" );

    result = RegisterCallback( p_sys->playerBufferQueue, PlayedCallback,
                                   (void*)p_sys);
    CHECK_OPENSL_ERROR( "Failed to register buff queue callback." );


    // set the player's state to playing
    result = SetPlayState( p_sys->playerPlay, SL_PLAYSTATE_PLAYING );
    CHECK_OPENSL_ERROR( "Failed to switch to playing state" );

    vlc_mutex_init( &p_sys->lock );
    p_sys->p_chain = NULL;
    p_sys->pp_last = &p_sys->p_chain;

    // we want 16bit signed data little endian.
    p_aout->format.i_format              = VLC_CODEC_S16L;
    p_aout->format.i_physical_channels   = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    p_aout->pf_play                      = Play;
    p_aout->pf_pause                     = NULL;
    p_aout->pf_flush                     = NULL;

    aout_FormatPrepare( &p_aout->format );

    return VLC_SUCCESS;
error:
    Clean( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    audio_output_t *p_aout = (audio_output_t*)p_this;
    aout_sys_t     *p_sys = p_aout->sys;

    SetPlayState( p_sys->playerPlay, SL_PLAYSTATE_STOPPED );
    //Flush remaining buffers if any.
    Clear( p_sys->playerBufferQueue );
    block_ChainRelease( p_sys->p_chain );
    vlc_mutex_destroy( &p_sys->lock );
    Clean( p_sys );
}
