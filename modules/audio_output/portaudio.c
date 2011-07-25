/*****************************************************************************
 * portaudio.c : portaudio (v19) audio output plugin
 *****************************************************************************
 * Copyright (C) 2002, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Frederic Ruget <frederic.ruget@free.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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


#include <portaudio.h>

#define FRAME_SIZE 1024              /* The size is in samples, not in bytes */

#ifdef WIN32
#   define PORTAUDIO_IS_SERIOUSLY_BROKEN 1
#endif

/*****************************************************************************
 * aout_sys_t: portaudio audio output method descriptor
 *****************************************************************************/
typedef struct
{
    audio_output_t *p_aout;

    vlc_thread_t thread;
    vlc_cond_t  wait;
    vlc_mutex_t lock_wait;
    bool  b_wait;
    vlc_cond_t  signal;
    vlc_mutex_t lock_signal;
    bool  b_signal;
    bool  b_error;

} pa_thread_t;

struct aout_sys_t
{
    audio_output_t *p_aout;
    PaStream *p_stream;

    PaDeviceIndex i_devices;
    int i_sample_size;
    PaDeviceIndex i_device_id;
    const PaDeviceInfo *deviceInfo;

    bool b_chan_reorder;              /* do we need channel reordering */
    int pi_chan_table[AOUT_CHAN_MAX];
    uint32_t i_channel_mask;
    uint32_t i_bits_per_sample;
    uint32_t i_channels;
};

static const uint32_t pi_channels_out[] =
    { AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
      AOUT_CHAN_CENTER, AOUT_CHAN_LFE,
      AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_REARCENTER,
      AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT, 0 };

#ifdef PORTAUDIO_IS_SERIOUSLY_BROKEN
static bool b_init = 0;
static pa_thread_t *pa_thread;
static void* PORTAUDIOThread( void * );
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );
static void Play        ( audio_output_t * );

static int PAOpenDevice( audio_output_t * );
static int PAOpenStream( audio_output_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DEVICE_TEXT N_("Output device")
#define DEVICE_LONGTEXT N_("Portaudio identifier for the output device")

vlc_module_begin ()
    set_shortname( "PortAudio" )
    set_description( N_("PORTAUDIO audio output") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_integer( "portaudio-audio-device", 0,
                 DEVICE_TEXT, DEVICE_LONGTEXT, false )
        add_deprecated_alias( "portaudio-device" )   /* deprecated since 0.9.3 */
    set_capability( "audio output", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/* This routine will be called by the PortAudio engine when audio is needed.
 * It may called at interrupt level on some machines so don't do anything
 * that could mess up the system like calling malloc() or free().
 */
static int paCallback( const void *inputBuffer, void *outputBuffer,
                       unsigned long framesPerBuffer,
                       const PaStreamCallbackTimeInfo *paDate,
                       PaStreamCallbackFlags statusFlags, void *p_cookie )
{
    VLC_UNUSED( inputBuffer ); VLC_UNUSED( statusFlags );

    struct aout_sys_t *p_sys = (struct aout_sys_t*) p_cookie;
    audio_output_t   *p_aout = p_sys->p_aout;
    aout_buffer_t     *p_buffer;
    mtime_t out_date;

    out_date = mdate() + (mtime_t) ( 1000000 *
        ( paDate->outputBufferDacTime - paDate->currentTime ) );
    p_buffer = aout_OutputNextBuffer( p_aout, out_date, true );

    if ( p_buffer != NULL )
    {
        if( p_sys->b_chan_reorder )
        {
            /* Do the channel reordering here */
            aout_ChannelReorder( p_buffer->p_buffer, p_buffer->i_buffer,
                                 p_sys->i_channels, p_sys->pi_chan_table,
                                 p_sys->i_bits_per_sample );
        }
        vlc_memcpy( outputBuffer, p_buffer->p_buffer,
                    framesPerBuffer * p_sys->i_sample_size );
        /* aout_BufferFree may be dangereous here, but then so is
         * aout_OutputNextBuffer (calls aout_BufferFree internally).
         * one solution would be to link the no longer useful buffers
         * in a second fifo (in aout_OutputNextBuffer too) and to
         * wait until we are in Play to do the actual free.
         */
        aout_BufferFree( p_buffer );
    }
    else
        /* Audio output buffer shortage -> stop the fill process and wait */
    {
        vlc_memset( outputBuffer, 0, framesPerBuffer * p_sys->i_sample_size );
    }
    return 0;
}

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    struct aout_sys_t * p_sys;

    msg_Dbg( p_aout, "entering Open()");

    /* Allocate p_sys structure */
    p_sys = malloc( sizeof(aout_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_sys->p_aout = p_aout;
    p_sys->p_stream = 0;
    p_aout->sys = p_sys;
    p_aout->pf_play = Play;
    p_aout->pf_pause = NULL;

    /* Retrieve output device id from config */
    p_sys->i_device_id = var_CreateGetInteger( p_aout, "portaudio-audio-device" );

#ifdef PORTAUDIO_IS_SERIOUSLY_BROKEN
    if( !b_init )
    {
        int i_err;

        /* Test device */
        if( PAOpenDevice( p_aout ) != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open portaudio device" );
            free( p_sys );
            return VLC_EGENERIC;
        }

        /* Close device for now. We'll re-open it later on */
        if( ( i_err = Pa_Terminate() ) != paNoError )
        {
            msg_Err( p_aout, "closing the device returned %d", i_err );
        }

        b_init = true;

        /* Now we need to setup our DirectSound play notification structure */
        pa_thread = calloc( 1, sizeof(*pa_thread) );
        pa_thread->p_aout = p_aout;
        pa_thread->b_error = false;
        vlc_mutex_init( &pa_thread->lock_wait );
        vlc_cond_init( &pa_thread->wait );
        pa_thread->b_wait = false;
        vlc_mutex_init( &pa_thread->lock_signal );
        vlc_cond_init( &pa_thread->signal );
        pa_thread->b_signal = false;

        /* Create PORTAUDIOThread */
        if( vlc_clone( &pa_thread->thread, PORTAUDIOThread, pa_thread,
                               VLC_THREAD_PRIORITY_OUTPUT ) )
        {
            msg_Err( p_aout, "cannot create PORTAUDIO thread" );
            return VLC_EGENERIC;
        }
    }
    else
    {
        pa_thread->p_aout = p_aout;
        pa_thread->b_wait = false;
        pa_thread->b_signal = false;
        pa_thread->b_error = false;
    }

    /* Signal start of stream */
    vlc_mutex_lock( &pa_thread->lock_signal );
    pa_thread->b_signal = true;
    vlc_cond_signal( &pa_thread->signal );
    vlc_mutex_unlock( &pa_thread->lock_signal );

    /* Wait until thread is ready */
    vlc_mutex_lock( &pa_thread->lock_wait );
    while( !pa_thread->b_wait )
        vlc_cond_wait( &pa_thread->wait, &pa_thread->lock_wait );
    vlc_mutex_unlock( &pa_thread->lock_wait );
    pa_thread->b_wait = false;

    if( pa_thread->b_error )
    {
        msg_Err( p_aout, "PORTAUDIO thread failed" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;

#else

    if( PAOpenDevice( p_aout ) != VLC_SUCCESS )
    {
        msg_Err( p_aout, "cannot open portaudio device" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( PAOpenStream( p_aout ) != VLC_SUCCESS )
    {
        msg_Err( p_aout, "cannot open portaudio device" );
    }

    return VLC_SUCCESS;

#endif
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    aout_sys_t *p_sys = p_aout->sys;

    msg_Dbg( p_aout, "closing portaudio");

#ifdef PORTAUDIO_IS_SERIOUSLY_BROKEN

    /* Signal end of stream */
    vlc_mutex_lock( &pa_thread->lock_signal );
    pa_thread->b_signal = true;
    vlc_cond_signal( &pa_thread->signal );
    vlc_mutex_unlock( &pa_thread->lock_signal );

    /* Wait until thread is ready */
    vlc_mutex_lock( &pa_thread->lock_wait );
    while( !pa_thread->b_wait )
        vlc_cond_wait( &pa_thread->wait, &pa_thread->lock_wait );
    vlc_mutex_unlock( &pa_thread->lock_wait );
    pa_thread->b_wait = false;

#else

    int i_err = Pa_StopStream( p_sys->p_stream );
    if( i_err != paNoError )
    {
        msg_Err( p_aout, "Pa_StopStream: %d (%s)", i_err,
                 Pa_GetErrorText( i_err ) );
    }
    i_err = Pa_CloseStream( p_sys->p_stream );
    if( i_err != paNoError )
    {
        msg_Err( p_aout, "Pa_CloseStream: %d (%s)", i_err,
                 Pa_GetErrorText( i_err ) );
    }

    i_err = Pa_Terminate();
    if( i_err != paNoError )
    {
        msg_Err( p_aout, "Pa_Terminate: %d (%s)", i_err,
                 Pa_GetErrorText( i_err ) );
    }

#endif

    msg_Dbg( p_aout, "portaudio closed");
    free( p_sys );
}

static int PAOpenDevice( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    const PaDeviceInfo *p_pdi;
    PaError i_err;
    vlc_value_t val, text;
    int i;

    /* Initialize portaudio */
    i_err = Pa_Initialize();
    if( i_err != paNoError )
    {
        msg_Err( p_aout, "Pa_Initialize returned %d : %s",
                 i_err, Pa_GetErrorText( i_err ) );

        return VLC_EGENERIC;
    }

    p_sys->i_devices = Pa_GetDeviceCount();
    if( p_sys->i_devices < 0 )
    {
        i_err = p_sys->i_devices;
        msg_Err( p_aout, "Pa_GetDeviceCount returned %d : %s", i_err,
                 Pa_GetErrorText( i_err ) );

        goto error;
    }

    /* Display all devices info */
    msg_Dbg( p_aout, "number of devices = %d", p_sys->i_devices );
    for( i = 0; i < p_sys->i_devices; i++ )
    {
        p_pdi = Pa_GetDeviceInfo( i );
        msg_Dbg( p_aout, "------------------------------------- #%d", i );
        msg_Dbg( p_aout, "Name         = %s", p_pdi->name );
        msg_Dbg( p_aout, "Max Inputs   = %d, Max Outputs = %d",
                  p_pdi->maxInputChannels, p_pdi->maxOutputChannels );
    }
    msg_Dbg( p_aout, "-------------------------------------" );

    msg_Dbg( p_aout, "requested device is #%d", p_sys->i_device_id );
    if( p_sys->i_device_id >= p_sys->i_devices )
    {
        msg_Err( p_aout, "device %d does not exist", p_sys->i_device_id );
        goto error;
    }
    p_sys->deviceInfo = Pa_GetDeviceInfo( p_sys->i_device_id );

    if( p_sys->deviceInfo->maxOutputChannels < 1 )
    {
        msg_Err( p_aout, "no channel available" );
        goto error;
    }

    if( var_Type( p_aout, "audio-device" ) == 0 )
    {
        var_Create( p_aout, "audio-device", VLC_VAR_INTEGER|VLC_VAR_HASCHOICE);
        text.psz_string = _("Audio Device");
        var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

        if( p_sys->deviceInfo->maxOutputChannels >= 1 )
        {
            val.i_int = AOUT_VAR_MONO;
            text.psz_string = _("Mono");
            var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE,
                        &val, &text );
            msg_Dbg( p_aout, "device supports 1 channel" );
        }
        if( p_sys->deviceInfo->maxOutputChannels >= 2 )
        {
            val.i_int = AOUT_VAR_STEREO;
            text.psz_string = _("Stereo");
            var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE,
                        &val, &text );
            var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT,
                        &val, NULL );
            var_Set( p_aout, "audio-device", val );
            msg_Dbg( p_aout, "device supports 2 channels" );
        }
        if( p_sys->deviceInfo->maxOutputChannels >= 4 )
        {
            val.i_int = AOUT_VAR_2F2R;
            text.psz_string = _("2 Front 2 Rear");
            var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE,
                        &val, &text );
            msg_Dbg( p_aout, "device supports 4 channels" );
        }
        if( p_sys->deviceInfo->maxOutputChannels >= 5 )
        {
            val.i_int = AOUT_VAR_3F2R;
            text.psz_string = _("3 Front 2 Rear");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            msg_Dbg( p_aout, "device supports 5 channels" );
        }
        if( p_sys->deviceInfo->maxOutputChannels >= 6 )
        {
            val.i_int = AOUT_VAR_5_1;
            text.psz_string = _("5.1");
            var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE,
                        &val, &text );
            msg_Dbg( p_aout, "device supports 5.1 channels" );
        }

        var_AddCallback( p_aout, "audio-device", aout_ChannelsRestart, NULL );
        var_TriggerCallback( p_aout, "intf-change" );
    }

    /* Audio format is paFloat32 (always supported by portaudio v19) */
    p_aout->format.i_format = VLC_CODEC_FL32;

    return VLC_SUCCESS;

 error:
    if( ( i_err = Pa_Terminate() ) != paNoError )
    {
        msg_Err( p_aout, "Pa_Terminate returned %d", i_err );
    }
    return VLC_EGENERIC;
}

static int PAOpenStream( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    const PaHostErrorInfo* paLastHostErrorInfo = Pa_GetLastHostErrorInfo();
    PaStreamParameters paStreamParameters;
    vlc_value_t val;
    int i_channels, i_err;
    uint32_t i_channel_mask;

    if( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        return VLC_EGENERIC;
    }

    if( val.i_int == AOUT_VAR_5_1 )
    {
        p_aout->format.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
              | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
              | AOUT_CHAN_LFE;
    }
    else if( val.i_int == AOUT_VAR_3F2R )
    {
        p_aout->format.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
            | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
    }
    else if( val.i_int == AOUT_VAR_2F2R )
    {
        p_aout->format.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
            | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
    }
    else if( val.i_int == AOUT_VAR_MONO )
    {
        p_aout->format.i_physical_channels = AOUT_CHAN_CENTER;
    }
    else
    {
        p_aout->format.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }

    i_channels = aout_FormatNbChannels( &p_aout->format );
    msg_Dbg( p_aout, "nb_channels requested = %d", i_channels );
    i_channel_mask = p_aout->format.i_physical_channels;

    /* Calculate the frame size in bytes */
    p_sys->i_sample_size = 4 * i_channels;
    p_aout->i_nb_samples = FRAME_SIZE;
    aout_FormatPrepare( &p_aout->format );
    aout_VolumeSoftInit( p_aout );

    /* Check for channel reordering */
    p_aout->sys->i_channel_mask = i_channel_mask;
    p_aout->sys->i_bits_per_sample = 32; /* forced to paFloat32 */
    p_aout->sys->i_channels = i_channels;

    p_aout->sys->b_chan_reorder =
        aout_CheckChannelReorder( NULL, pi_channels_out,
                                  i_channel_mask, i_channels,
                                  p_aout->sys->pi_chan_table );

    if( p_aout->sys->b_chan_reorder )
    {
        msg_Dbg( p_aout, "channel reordering needed" );
    }

    paStreamParameters.device = p_sys->i_device_id;
    paStreamParameters.channelCount = i_channels;
    paStreamParameters.sampleFormat = paFloat32;
    paStreamParameters.suggestedLatency =
        p_sys->deviceInfo->defaultLowOutputLatency;
    paStreamParameters.hostApiSpecificStreamInfo = NULL;

    i_err = Pa_OpenStream( &p_sys->p_stream, NULL /* no input */,
                &paStreamParameters, (double)p_aout->format.i_rate,
                FRAME_SIZE, paClipOff, paCallback, p_sys );
    if( i_err != paNoError )
    {
        msg_Err( p_aout, "Pa_OpenStream returns %d : %s", i_err,
                 Pa_GetErrorText( i_err ) );
        if( i_err == paUnanticipatedHostError )
        {
            msg_Err( p_aout, "type %d code %ld : %s",
                     paLastHostErrorInfo->hostApiType,
                     paLastHostErrorInfo->errorCode,
                     paLastHostErrorInfo->errorText );
        }
        p_sys->p_stream = 0;
        return VLC_EGENERIC;
    }

    i_err = Pa_StartStream( p_sys->p_stream );
    if( i_err != paNoError )
    {
        msg_Err( p_aout, "Pa_StartStream() failed" );
        Pa_CloseStream( p_sys->p_stream );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Play: play sound
 *****************************************************************************/
static void Play( audio_output_t * p_aout )
{
    VLC_UNUSED( p_aout );
}

#ifdef PORTAUDIO_IS_SERIOUSLY_BROKEN
/*****************************************************************************
 * PORTAUDIOThread: all interactions with libportaudio.a are handled
 * in this single thread.  Otherwise libportaudio.a is _not_ happy :-(
 *****************************************************************************/
static void* PORTAUDIOThread( void *data )
{
    pa_thread_t *pa_thread = (pa_thread_t*)data;
    audio_output_t *p_aout;
    aout_sys_t *p_sys;
    int i_err;
    int canc = vlc_savecancel ();

    for( ;; )
    {
        /* Wait for start of stream */
        vlc_mutex_lock( &pa_thread->lock_signal );
        while( !pa_thread->b_signal )
            vlc_cond_wait( &pa_thread->signal, &pa_thread->lock_signal );
        vlc_mutex_unlock( &pa_thread->lock_signal );
        pa_thread->b_signal = false;

        p_aout = pa_thread->p_aout;
        p_sys = p_aout->sys;

        if( PAOpenDevice( p_aout ) != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open portaudio device" );
            pa_thread->b_error = true;
        }

        if( !pa_thread->b_error && PAOpenStream( p_aout ) != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open portaudio device" );
            pa_thread->b_error = true;

            i_err = Pa_Terminate();
            if( i_err != paNoError )
            {
                msg_Err( p_aout, "Pa_Terminate: %d (%s)", i_err,
                         Pa_GetErrorText( i_err ) );
            }
        }

        /* Tell the main thread that we are ready */
        vlc_mutex_lock( &pa_thread->lock_wait );
        pa_thread->b_wait = true;
        vlc_cond_signal( &pa_thread->wait );
        vlc_mutex_unlock( &pa_thread->lock_wait );

        /* Wait for end of stream */
        vlc_mutex_lock( &pa_thread->lock_signal );
        while( !pa_thread->b_signal )
            vlc_cond_wait( &pa_thread->signal, &pa_thread->lock_signal );
        vlc_mutex_unlock( &pa_thread->lock_signal );
        pa_thread->b_signal = false;

        if( pa_thread->b_error ) continue;

        i_err = Pa_StopStream( p_sys->p_stream );
        if( i_err != paNoError )
        {
            msg_Err( p_aout, "Pa_StopStream: %d (%s)", i_err,
                     Pa_GetErrorText( i_err ) );
        }
        i_err = Pa_CloseStream( p_sys->p_stream );
        if( i_err != paNoError )
        {
            msg_Err( p_aout, "Pa_CloseStream: %d (%s)", i_err,
                     Pa_GetErrorText( i_err ) );
        }
        i_err = Pa_Terminate();
        if( i_err != paNoError )
        {
            msg_Err( p_aout, "Pa_Terminate: %d (%s)", i_err,
                     Pa_GetErrorText( i_err ) );
        }

        /* Tell the main thread that we are ready */
        vlc_mutex_lock( &pa_thread->lock_wait );
        pa_thread->b_wait = true;
        vlc_cond_signal( &pa_thread->wait );
        vlc_mutex_unlock( &pa_thread->lock_wait );
    }
    vlc_restorecancel (canc);
    return NULL;
}
#endif
