/*****************************************************************************
 * aout.c : QNX audio output
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 *
 * Authors: Henri Fallon <henri@videolan.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Pascal Levesque <pascal.levesque@mindready.com>
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
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>

#ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#endif

#include <vlc/aout.h>
#include "aout_internal.h"

#include <sys/asoundlib.h>

struct aout_sys_t
{
    snd_pcm_t  * p_pcm_handle;
    int          i_card;
    int          i_device;

    byte_t *     p_silent_buffer;
};

#define DEFAULT_FRAME_SIZE 2048

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int            E_(OpenAudio)    ( vlc_object_t *p_this );
void           E_(CloseAudio)   ( vlc_object_t *p_this );
static int     GetBufInfo       ( aout_instance_t * );
static void    Play             ( aout_instance_t * );
static int     QNXaoutThread    ( aout_instance_t * );

/*****************************************************************************
 * Open : creates a handle and opens an alsa device
 *****************************************************************************
 * This function opens an alsa device, through the alsa API
 *****************************************************************************/
int E_(OpenAudio)( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    int i_ret;
    int i_bytes_per_sample;
    int i_nb_channels;
    snd_pcm_channel_info_t pi;
    snd_pcm_channel_params_t pp;
    aout_instance_t *p_aout = (aout_instance_t *)p_this;

    /* allocate structure */
    p_aout->output.p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return -1;
    }

    /* open audio device */
    if( ( i_ret = snd_pcm_open_preferred( &p_aout->output.p_sys->p_pcm_handle,
                                          &p_aout->output.p_sys->i_card,
                                          &p_aout->output.p_sys->i_device,
                                          SND_PCM_OPEN_PLAYBACK ) ) < 0 )
    {
        msg_Err( p_aout, "unable to open audio device (%s)",
                         snd_strerror( i_ret ) );
        free( p_aout->output.p_sys );
        return -1;
    }

    /* disable mmap */
    if( ( i_ret = snd_pcm_plugin_set_disable( p_aout->output.p_sys->p_pcm_handle,
                                              PLUGIN_DISABLE_MMAP ) ) < 0 )
    {
        msg_Err( p_aout, "unable to disable mmap (%s)", snd_strerror(i_ret) );
        E_(CloseAudio)( p_this );
        free( p_aout->output.p_sys );
        return -1;
    }

    p_aout->output.p_sys->p_silent_buffer = malloc( DEFAULT_FRAME_SIZE * 4 );
    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    memset( &pi, 0, sizeof(pi) );
    memset( &pp, 0, sizeof(pp) );

    pi.channel = SND_PCM_CHANNEL_PLAYBACK;
    if( ( i_ret = snd_pcm_plugin_info( p_aout->output.p_sys->p_pcm_handle,
                                       &pi ) ) < 0 )
    {
        msg_Err( p_aout, "unable to get plugin info (%s)",
                         snd_strerror( i_ret ) );
        E_(CloseAudio)( p_this );
        free( p_aout->output.p_sys );
        return -1;
    }

    pp.mode       = SND_PCM_MODE_BLOCK;
    pp.channel    = SND_PCM_CHANNEL_PLAYBACK;
    pp.start_mode = SND_PCM_START_FULL;
    pp.stop_mode  = SND_PCM_STOP_STOP;

    pp.buf.block.frags_max   = 3;
    pp.buf.block.frags_min   = 1;

    pp.format.interleave     = 1;
    pp.format.rate           = p_aout->output.output.i_rate;

    i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );
    if ( i_nb_channels > 2 )
    {
        /* I don't know if QNX supports more than two channels. */
        i_nb_channels = 2;
        p_aout->output.output.i_channels = AOUT_CHAN_STEREO;
    }
    pp.format.voices         = i_nb_channels;

    p_aout->output.output.i_format = AOUT_FMT_S16_NE;
    p_aout->output.i_nb_samples = DEFAULT_FRAME_SIZE;
    pp.format.format = SND_PCM_SFMT_S16;
    i_bytes_per_sample = 2;

    pp.buf.block.frag_size = p_aout->output.i_nb_samples *
                            p_aout->output.output.i_channels *
                            i_bytes_per_sample;

    /* set parameters */
    if( ( i_ret = snd_pcm_plugin_params( p_aout->output.p_sys->p_pcm_handle,
                                         &pp ) ) < 0 )
    {
        msg_Err( p_aout, "unable to set parameters (%s)", snd_strerror(i_ret) );
        E_(CloseAudio)( p_this );
        free( p_aout->output.p_sys );
        return -1;
    }

    /* prepare channel */
    if( ( i_ret = snd_pcm_plugin_prepare( p_aout->output.p_sys->p_pcm_handle,
                                          SND_PCM_CHANNEL_PLAYBACK ) ) < 0 )
    {
        msg_Err( p_aout, "unable to prepare channel (%s)",
                         snd_strerror( i_ret ) );
        E_(CloseAudio)( p_this );
        free( p_aout->output.p_sys );
        return -1;
    }

    /* Create audio thread and wait for its readiness. */
    if( vlc_thread_create( p_aout, "aout", QNXaoutThread,
                           VLC_THREAD_PRIORITY_OUTPUT, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create QNX audio thread (%s)", strerror(errno) );
        E_(CloseAudio)( p_this );
        free( p_aout->output.p_sys );
        return -1;
    }

    return( 0 );
}

/*****************************************************************************
 * GetBufInfo: buffer status query
 *****************************************************************************
 * This function returns the number of used byte in the queue.
 * It also deals with errors : indeed if the device comes to run out
 * of data to play, it switches to the "underrun" status. It has to
 * be flushed and re-prepared
 *****************************************************************************/
static int GetBufInfo( aout_instance_t *p_aout )
{
    int i_ret;
    snd_pcm_channel_status_t status;

    /* get current pcm status */
    memset( &status, 0, sizeof(status) );
    if( ( i_ret = snd_pcm_plugin_status( p_aout->output.p_sys->p_pcm_handle,
                                         &status ) ) < 0 )
    {
        msg_Err( p_aout, "unable to get device status (%s)",
                         snd_strerror( i_ret ) );
        return( -1 );
    }

    /* check for underrun */
    switch( status.status )
    {
        case SND_PCM_STATUS_READY:
        case SND_PCM_STATUS_UNDERRUN:
            if( ( i_ret = snd_pcm_plugin_prepare( p_aout->output.p_sys->p_pcm_handle,
                                          SND_PCM_CHANNEL_PLAYBACK ) ) < 0 )
            {
                msg_Err( p_aout, "unable to prepare channel (%s)",
                                 snd_strerror( i_ret ) );
            }
            break;
    }

    return( status.count );
}

/*****************************************************************************
 * Play : plays a sample
 *****************************************************************************
 * Plays a sample using the snd_pcm_write function from the alsa API
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
}

/*****************************************************************************
 * CloseAudio: close the audio device
 *****************************************************************************/
void E_(CloseAudio) ( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    int i_ret;

    p_aout->b_die = 1;
    vlc_thread_join( p_aout );

    if( ( i_ret = snd_pcm_close( p_aout->output.p_sys->p_pcm_handle ) ) < 0 )
    {
        msg_Err( p_aout, "unable to close audio device (%s)",
                         snd_strerror( i_ret ) );
    }

    free( p_aout->output.p_sys->p_silent_buffer );
    free( p_aout->output.p_sys );
}


/*****************************************************************************
 * QNXaoutThread: asynchronous thread used to DMA the data to the device
 *****************************************************************************/
static int QNXaoutThread( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    while ( !p_aout->b_die )
    {
        aout_buffer_t * p_buffer;
        int i_tmp, i_size;
        byte_t * p_bytes;

        if ( p_aout->output.output.i_format != VLC_FOURCC('s','p','d','i') )
        {
            mtime_t next_date = 0;

            /* Get the presentation date of the next write() operation. It
             * is equal to the current date + duration of buffered samples.
             * Order is important here, since GetBufInfo is believed to take
             * more time than mdate(). */
            next_date = (mtime_t)GetBufInfo( p_aout ) * 1000000
                      / p_aout->output.output.i_bytes_per_frame
                      / p_aout->output.output.i_rate
                      * p_aout->output.output.i_frame_length;
            next_date += mdate();

            p_buffer = aout_OutputNextBuffer( p_aout, next_date, VLC_FALSE );
        }
        else
        {
            p_buffer = aout_OutputNextBuffer( p_aout, 0, VLC_TRUE );
        }

        if ( p_buffer != NULL )
        {
            p_bytes = p_buffer->p_buffer;
            i_size = p_buffer->i_nb_bytes;
        }
        else
        {
            i_size = DEFAULT_FRAME_SIZE / p_aout->output.output.i_frame_length
                      * p_aout->output.output.i_bytes_per_frame;
            p_bytes = p_aout->output.p_sys->p_silent_buffer;
            memset( p_bytes, 0, i_size );
        }

        i_tmp = snd_pcm_plugin_write( p_aout->output.p_sys->p_pcm_handle,
                                        (void *) p_bytes,
                                        (size_t) i_size );

        if( i_tmp < 0 )
        {
            msg_Err( p_aout, "write failed (%s)", strerror(errno) );
        }

        if ( p_buffer != NULL )
        {
            aout_BufferFree( p_buffer );
        }
    }

    return 0;
}

