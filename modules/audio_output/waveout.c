/*****************************************************************************
 * waveout.c : Windows waveOut plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: waveout.c,v 1.8 2002/10/11 10:08:06 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */

#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include "aout_internal.h"

#include <windows.h>
#include <mmsystem.h>

#define FRAME_SIZE 2048              /* The size is in samples, not in bytes */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );             
static void Close        ( vlc_object_t * );                   
static void Play         ( aout_instance_t * );

/* local functions */
static int OpenWaveOut   ( aout_instance_t *p_aout, int i_format,
                           int i_channels, int i_rate );
static int PlayWaveOut   ( aout_instance_t *, HWAVEOUT, WAVEHDR *,
                           aout_buffer_t * );
static void CALLBACK WaveOutCallback ( HWAVEOUT h_waveout, UINT uMsg,
                                       DWORD _p_aout,
                                       DWORD dwParam1, DWORD dwParam2 );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Win32 waveOut extension module") ); 
    set_capability( "audio output", 50 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * aout_sys_t: waveOut audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the waveOut specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    HWAVEOUT h_waveout;                        /* handle to waveout instance */

    WAVEFORMATEX waveformat;                                 /* audio format */

    WAVEHDR waveheader[2];

    int i_buffer_size;

    byte_t *p_silence_buffer;               /* buffer we use to play silence */
};

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************
 * This function opens and setups Win32 waveOut
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{   
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    aout_buffer_t *p_buffer;

    /* Allocate structure */
    p_aout->output.p_sys = malloc( sizeof( aout_sys_t ) );

    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return 1;
    }

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    /* FIXME */
    if ( p_aout->output.output.i_channels > 2 )
    {
        msg_Warn( p_aout, "only two channels are supported at the moment" );
        /* Trigger downmixing */
        p_aout->output.output.i_channels = 2;
    }

    /* We need to open the device with default values to be sure it is
     * available */
    if ( OpenWaveOut( p_aout, WAVE_FORMAT_PCM,
                      p_aout->output.output.i_channels,
                      p_aout->output.output.i_rate ) )
    {
        msg_Err( p_aout, "cannot open waveout audio device with output "
                         "rate (%i)",
                          p_aout->output.output.i_rate );
        return 1;

        if ( OpenWaveOut( p_aout, WAVE_FORMAT_PCM,
                          p_aout->output.output.i_channels,
                          44100 ) )
        {
            msg_Err( p_aout, "cannot open waveout audio device with output "
                             "rate (44100)" );
            return 1;
        }
    }

    waveOutReset( p_aout->output.p_sys->h_waveout );

    /* calculate the frame size in bytes */
    p_aout->output.p_sys->i_buffer_size = FRAME_SIZE * sizeof(s16)
                                  * p_aout->output.p_sys->waveformat.nChannels;
    /* Allocate silence buffer */
    p_aout->output.p_sys->p_silence_buffer =
        calloc( p_aout->output.p_sys->i_buffer_size, 1 );
    if( p_aout->output.p_sys->p_silence_buffer == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return 1;
    }

    p_aout->output.output.i_format = AOUT_FMT_S16_NE;
    p_aout->output.i_nb_samples = FRAME_SIZE;

    /* We need to kick off the playback in order to have the callback properly
     * working */
    PlayWaveOut( p_aout, p_aout->output.p_sys->h_waveout,
                 &p_aout->output.p_sys->waveheader[0], NULL );

    p_buffer = aout_OutputNextBuffer( p_aout,
        mdate() + 1000000 / p_aout->output.output.i_rate * FRAME_SIZE,
        VLC_FALSE );
    PlayWaveOut( p_aout, p_aout->output.p_sys->h_waveout,
                 &p_aout->output.p_sys->waveheader[1], p_buffer );

    return 0;
}

/*****************************************************************************
 * Play: play a sound buffer
 *****************************************************************************
 * This doesn't actually play the buffer. This just stores the buffer so it
 * can be played by the callback thread.
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{       
    aout_instance_t *p_aout = (aout_instance_t *)p_this;

    /* Before calling waveOutClose we must reset the device */
    waveOutReset( p_aout->output.p_sys->h_waveout );

    /* Close the device */
    if( waveOutClose( p_aout->output.p_sys->h_waveout ) != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutClose failed" );
    }

    /* Free silence buffer */
    free( p_aout->output.p_sys->p_silence_buffer );

    if( p_aout->output.p_sys != NULL )
    { 
        free( p_aout->output.p_sys );
        p_aout->output.p_sys = NULL;
    }
}

/*****************************************************************************
 * OpenWaveOut: open the waveout sound device
 ****************************************************************************/
static int OpenWaveOut( aout_instance_t *p_aout, int i_format,
                        int i_channels, int i_rate )
{
    MMRESULT result;

    /* Set sound format */
    p_aout->output.p_sys->waveformat.wFormatTag = i_format;
    p_aout->output.p_sys->waveformat.nChannels = i_channels;
    p_aout->output.p_sys->waveformat.nSamplesPerSec = i_rate;
    p_aout->output.p_sys->waveformat.wBitsPerSample = 16;
    p_aout->output.p_sys->waveformat.nBlockAlign =
        p_aout->output.p_sys->waveformat.wBitsPerSample / 8 * i_channels;
    p_aout->output.p_sys->waveformat.nAvgBytesPerSec  =
        p_aout->output.p_sys->waveformat.nSamplesPerSec *
            p_aout->output.p_sys->waveformat.nBlockAlign;

    /* Open the device */
    result = waveOutOpen( &p_aout->output.p_sys->h_waveout, WAVE_MAPPER,
                          &p_aout->output.p_sys->waveformat,
                          (DWORD_PTR)WaveOutCallback, (DWORD_PTR)p_aout,
                          CALLBACK_FUNCTION );
    if( result == WAVERR_BADFORMAT )
    {
        msg_Err( p_aout, "waveOutOpen failed WAVERR_BADFORMAT" );
        return( 1 );
    }
    if( result != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutOpen failed" );
        return 1;
    }

    return 0;
}

/*****************************************************************************
 * PlayWaveOut: play a buffer through the WaveOut device
 *****************************************************************************/
static int PlayWaveOut( aout_instance_t *p_aout, HWAVEOUT h_waveout,
                        WAVEHDR *p_waveheader, aout_buffer_t *p_buffer )
{
    MMRESULT result;

    /* Prepare the buffer */
    if( p_buffer != NULL )
        p_waveheader->lpData = p_buffer->p_buffer;
    else
        /* Use silence buffer instead */
        p_waveheader->lpData = p_aout->output.p_sys->p_silence_buffer;

    p_waveheader->dwUser = (DWORD_PTR)p_buffer;
    p_waveheader->dwBufferLength = p_aout->output.p_sys->i_buffer_size;
    p_waveheader->dwFlags = 0;

    result = waveOutPrepareHeader( h_waveout, p_waveheader, sizeof(WAVEHDR) );
    if( result != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutPrepareHeader failed" );
        return 1;
    }

    /* Send the buffer to the waveOut queue */
    result = waveOutWrite( h_waveout, p_waveheader, sizeof(WAVEHDR) );
    if( result != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutWrite failed" );
        return 1;
    }

    return 0;
}

/*****************************************************************************
 * WaveOutCallback: what to do once WaveOut has played its sound samples
 *****************************************************************************/
static void CALLBACK WaveOutCallback( HWAVEOUT h_waveout, UINT uMsg,
                                      DWORD _p_aout,
                                      DWORD dwParam1, DWORD dwParam2 )
{
    aout_instance_t *p_aout = (aout_instance_t *)_p_aout;
    WAVEHDR *p_waveheader = (WAVEHDR *)dwParam1;
    aout_buffer_t *p_buffer;

    if( uMsg != WOM_DONE ) return;

    /* Unprepare and free the buffer which has just been played */
    waveOutUnprepareHeader( h_waveout, p_waveheader, sizeof(WAVEHDR) );
    if( p_waveheader->dwUser )
        aout_BufferFree( (aout_buffer_t *)p_waveheader->dwUser );

    /* Take into account WaveOut latency instead of just mdate() */
    p_buffer = aout_OutputNextBuffer( p_aout,
        mdate() + 1000000 / p_aout->output.output.i_rate * FRAME_SIZE,
        VLC_FALSE );

    PlayWaveOut( p_aout, h_waveout, p_waveheader, p_buffer );
}
