/*****************************************************************************
 * portaudio.c : portaudio audio output plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id$
 *
 * Authors: Frederic Ruget <frederic.ruget@free.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <portaudio.h>

#include "aout_internal.h"

#define FRAME_SIZE 1024              /* The size is in samples, not in bytes */
#define FRAMES_NUM 8

/*****************************************************************************
 * aout_sys_t: portaudio audio output method descriptor
 *****************************************************************************/
struct aout_sys_t
{
    aout_instance_t *p_aout;
    PortAudioStream *p_stream;
    int i_numDevices;
    int i_nbChannels;
    PaSampleFormat sampleFormat;
    int i_sampleSize;
    PaDeviceID i_deviceId;
    PaDeviceInfo deviceInfo;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );
static void Play        ( aout_instance_t * );
static int i_once = 0;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DEVICE_TEXT N_("Output device")
#define DEVICE_LONGTEXT N_("Portaudio identifier for the output device")

vlc_module_begin();
    set_description( N_("PORTAUDIO audio output") );
    add_integer( "portaudio-device", 0, NULL,
                 DEVICE_TEXT, DEVICE_LONGTEXT, VLC_FALSE );
    set_capability( "audio output", 40 );
    set_callbacks( Open, Close );
vlc_module_end();

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int paCallback( void *inputBuffer, void *outputBuffer,
                       unsigned long framesPerBuffer,
                       PaTimestamp outTime, void *p_cookie )
{
    struct aout_sys_t* p_sys = (struct aout_sys_t*) p_cookie;
    aout_instance_t * p_aout = p_sys->p_aout;
    aout_buffer_t *   p_buffer;

    vlc_mutex_lock( &p_aout->output_fifo_lock );
    p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
    vlc_mutex_unlock( &p_aout->output_fifo_lock );

    if ( p_buffer != NULL )
    {
        p_aout->p_vlc->pf_memcpy( outputBuffer, p_buffer->p_buffer,
                                  framesPerBuffer * p_sys->i_sampleSize );
        aout_BufferFree( p_buffer );
    }
    else
    {
      p_aout->p_vlc->pf_memset( outputBuffer, 0,
                                framesPerBuffer * p_sys->i_sampleSize );
    }
    return 0;
}

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/
static int Open ( vlc_object_t * p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;
    PortAudioStream *p_stream;
    vlc_value_t val;
    PaError i_err;
    int i_nb_channels;
    const PaDeviceInfo *p_pdi;
    int i, j;

    msg_Dbg( p_aout, "Entering Open()");

    /* Allocate p_sys structure */
    p_sys = (struct aout_sys_t*) malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return VLC_ENOMEM;
    }
    p_sys->p_aout = p_aout;
    p_aout->output.p_sys = p_sys;

    /* Output device id */
    var_Create( p_this, "portaudio-device",
                VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Get( p_this, "portaudio-device", &val );
    p_sys->i_deviceId = val.i_int;

    if (! i_once)
    {
        i_once = 1;
        i_err = Pa_Initialize();
        if ( i_err != paNoError )
        {
            msg_Err( p_aout, "Pa_Initialize returned %d : %s", i_err, Pa_GetErrorText( i_err ));
            return VLC_EGENERIC;
        }
    }
    p_sys->i_numDevices = Pa_CountDevices();
    if( p_sys->i_numDevices < 0 )
    {
        i_err = p_sys->i_numDevices;
        msg_Err( p_aout, "Pa_CountDevices returned %d : %s", i_err, Pa_GetErrorText( i_err ));
        (void) Pa_Terminate();
        return VLC_EGENERIC;
    }
    msg_Info( p_aout, "Number of devices = %d", p_sys->i_numDevices );
    if ( p_sys->i_deviceId >= p_sys->i_numDevices )
    {
        msg_Err( p_aout, "Device %d does not exist", p_sys->i_deviceId );
        (void) Pa_Terminate();
        return VLC_EGENERIC;
    }
    for( i = 0; i < p_sys->i_numDevices; i++ )
    {
        p_pdi = Pa_GetDeviceInfo( i );
        if ( i == p_sys->i_deviceId )
        {
            p_sys->deviceInfo = *p_pdi;
        }
        msg_Info( p_aout, "---------------------------------------------- #%d", i );
        msg_Info( p_aout, "Name         = %s", p_pdi->name );
        msg_Info( p_aout, "Max Inputs   = %d, Max Outputs = %d",
                 p_pdi->maxInputChannels, p_pdi->maxOutputChannels );
        if( p_pdi->numSampleRates == -1 )
        {
            msg_Info( p_aout, "Sample Rate Range = %f to %f", p_pdi->sampleRates[0], p_pdi->sampleRates[1] );
        }
        else
        {
            msg_Info( p_aout, "Sample Rates =");
            for( j = 0; j < p_pdi->numSampleRates; j++ )
            {
                msg_Info( p_aout, " %8.2f,", p_pdi->sampleRates[j] );
            }
        }
        msg_Info( p_aout, "Native Sample Formats = ");
        if( p_pdi->nativeSampleFormats & paInt8 )        msg_Info( p_aout, "paInt8");
        if( p_pdi->nativeSampleFormats & paUInt8 )       msg_Info( p_aout, "paUInt8");
        if( p_pdi->nativeSampleFormats & paInt16 )       msg_Info( p_aout, "paInt16");
        if( p_pdi->nativeSampleFormats & paInt32 )       msg_Info( p_aout, "paInt32");
        if( p_pdi->nativeSampleFormats & paFloat32 )     msg_Info( p_aout, "paFloat32");
        if( p_pdi->nativeSampleFormats & paInt24 )       msg_Info( p_aout, "paInt24");
        if( p_pdi->nativeSampleFormats & paPackedInt24 ) msg_Info( p_aout, "paPackedInt24");
    }

    msg_Info( p_aout, "----------------------------------------------");


    /*
portaudio warning: Number of devices = 3
portaudio warning: ---------------------------------------------- #0 DefaultInput DefaultOutput
portaudio warning: Name         = PORTAUDIO DirectX Full Duplex Driver
portaudio warning: Max Inputs   = 2, Max Outputs = 2
portaudio warning: Sample Rates =
portaudio warning:  11025.00,
portaudio warning:  22050.00,
portaudio warning:  32000.00,
portaudio warning:  44100.00,
portaudio warning:  48000.00,
portaudio warning:  88200.00,
portaudio warning:  96000.00,
portaudio warning: Native Sample Formats = 
portaudio warning: paInt16
portaudio warning: ---------------------------------------------- #1
portaudio warning: Name         = PORTAUDIO Multimedia Driver
portaudio warning: Max Inputs   = 2, Max Outputs = 2
portaudio warning: Sample Rates =
portaudio warning:  11025.00,
portaudio warning:  22050.00,
portaudio warning:  32000.00,
portaudio warning:  44100.00,
portaudio warning:  48000.00,
portaudio warning:  88200.00,
portaudio warning:  96000.00,
portaudio warning: Native Sample Formats = 
portaudio warning: paInt16
portaudio warning: ---------------------------------------------- #2
portaudio warning: Name         = E-MU PORTAUDIO
portaudio warning: Max Inputs   = 0, Max Outputs = 4
portaudio warning: Sample Rates =
portaudio warning:  44100.00,
portaudio warning:  48000.00,
portaudio warning:  96000.00,
portaudio warning: Native Sample Formats = 
portaudio warning: paInt16
portaudio warning: ----------------------------------------------
     */

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    /* select audio format */
    if( p_sys->deviceInfo.nativeSampleFormats & paFloat32 )
    {
        p_sys->sampleFormat = paFloat32;
        p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');
        p_sys->i_sampleSize = 4;
    }
    else if( p_sys->deviceInfo.nativeSampleFormats & paInt16 )
    {
        p_sys->sampleFormat = paInt16;
        p_aout->output.output.i_format = AOUT_FMT_S16_NE;
        p_sys->i_sampleSize = 2;
    }
    else
    {
        msg_Err( p_aout, "Audio format not supported" );
        (void) Pa_Terminate();
        return VLC_EGENERIC;
    }

    i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );
    msg_Info( p_aout, "nb_channels = %d", i_nb_channels );
    if ( i_nb_channels > p_sys->deviceInfo.maxOutputChannels )
    {
        if ( p_sys->deviceInfo.maxOutputChannels < 1 )
        {
            msg_Err( p_aout, "No channel available" );
            (void) Pa_Terminate();
            return VLC_EGENERIC;
        }
        else if ( p_sys->deviceInfo.maxOutputChannels < 2 )
        {
            p_sys->i_nbChannels = 1;
            p_aout->output.output.i_physical_channels
            = AOUT_CHAN_CENTER;
        }
        else if ( p_sys->deviceInfo.maxOutputChannels < 4 )
        {
            p_sys->i_nbChannels = 2;
            p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        }
        else if ( p_sys->deviceInfo.maxOutputChannels < 6 )
        {
            p_sys->i_nbChannels = 4;
            p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        }
        else
        {
            p_sys->i_nbChannels = 6;
            p_aout->output.output.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
               | AOUT_CHAN_LFE;
        }
    }
    p_sys->i_sampleSize *= p_sys->i_nbChannels;

    /* Open portaudio stream */
    p_aout->output.i_nb_samples = FRAME_SIZE;
    msg_Info( p_aout, "rate = %d", p_aout->output.output.i_rate );
    msg_Info( p_aout, "samples = %d", p_aout->output.i_nb_samples );
    
    i_err = Pa_OpenStream(
              &p_stream,
              paNoDevice, 0, 0, 0,  /* no input device */
              p_sys->i_deviceId,    /* output device */
              p_sys->i_nbChannels,
              p_sys->sampleFormat,
              NULL,
              (double) p_aout->output.output.i_rate,
              (unsigned long) p_aout->output.i_nb_samples,  /* FRAMES_PER_BUFFER */
              FRAMES_NUM, /* number of buffers, if zero then use default minimum */
              paClipOff,  /* we won't output out of range samples so don't bother clipping them */
              paCallback, p_sys );
    if( i_err != paNoError )
    {
        msg_Err( p_aout, "Pa_OpenStream returns %d : %s", i_err,
                 Pa_GetErrorText( i_err ) );
        (void) Pa_Terminate();
        return VLC_EGENERIC;
    }

    p_sys->p_stream = p_stream;
    i_err = Pa_StartStream( p_stream );
    if( i_err != paNoError )
    {
        (void) Pa_CloseStream( p_stream);
        (void) Pa_Terminate();
        return VLC_EGENERIC;
    }

    msg_Dbg( p_aout, "Leaving Open()" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    PortAudioStream *p_stream = p_sys->p_stream;
    PaError i_err;

    msg_Dbg( p_aout, "Entering Close()");

    i_err = Pa_AbortStream( p_stream );
    if ( i_err != paNoError )
    {
        msg_Err( p_aout, "Pa_AbortStream: %d (%s)", i_err, Pa_GetErrorText( i_err ) );
    }
    i_err = Pa_CloseStream( p_stream );
    if ( i_err != paNoError )
    {
        msg_Err( p_aout, "Pa_CloseStream: %d (%s)", i_err, Pa_GetErrorText( i_err ) );
    }
    i_err = Pa_Terminate();
    if ( i_err != paNoError )
    {
        msg_Err( p_aout, "Pa_Terminate: %d (%s)", i_err, Pa_GetErrorText( i_err ) );
    }
    msg_Dbg( p_aout, "Leaving Close()");
}

/*****************************************************************************
 * Play: play sound
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
}
