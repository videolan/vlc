/*****************************************************************************
 * esd.c : EsounD module
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <unistd.h>                                      /* write(), close() */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

#include <sys/socket.h>

#include <sys/time.h>
#include <time.h>

#include <esd.h>

/*****************************************************************************
 * aout_sys_t: esd audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some esd specific variables.
 *****************************************************************************/
struct aout_sys_t
{
    esd_format_t esd_format;
    int          i_fd;

    mtime_t      latency;       /* unused, but we might do something with it */
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Play         ( aout_instance_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("EsounD audio output") );
    set_shortname( "EsounD" );
    set_capability( "audio output", 50 );
    add_string( "esdserver", "", NULL, N_("Esound server"), NULL, false );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AOUT );
    set_callbacks( Open, Close );
    add_shortcut( "esound" );
vlc_module_end();

/*****************************************************************************
 * Open: open an esd socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;
    char * psz_server;
    int i_nb_channels;
    int i_newfd;

    /* Allocate structure */
    p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_aout->output.p_sys = p_sys;

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    /* Initialize some variables */
    p_sys->esd_format = ESD_BITS16 | ESD_STREAM | ESD_PLAY;
    p_sys->esd_format &= ~ESD_MASK_CHAN;

    p_aout->output.output.i_format = AOUT_FMT_S16_NE;
    i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );
    if ( i_nb_channels > 2 )
    {
        /* EsounD doesn't support more than two channels. */
        i_nb_channels = 2;
        p_aout->output.output.i_physical_channels =
            AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }

    switch( i_nb_channels )
    {
    case 1:
        p_sys->esd_format |= ESD_MONO;
        break;
    case 2:
        p_sys->esd_format |= ESD_STEREO;
        break;
    }

    /* Force the rate, otherwise the sound is very noisy */
    p_aout->output.output.i_rate = ESD_DEFAULT_RATE;
    p_aout->output.i_nb_samples = ESD_BUF_SIZE * 2;

    /* Open a socket for playing a stream
     * and try to open /dev/dsp if there's no EsounD */
    psz_server = config_GetPsz( p_aout, "esdserver" );
    if( psz_server && *psz_server )
    {
        p_sys->i_fd = esd_play_stream_fallback( p_sys->esd_format,
                                                p_aout->output.output.i_rate,
                                                psz_server, "vlc" );
    }
    else
    {
        p_sys->i_fd = esd_play_stream_fallback( p_sys->esd_format,
                                                p_aout->output.output.i_rate,
                                                NULL, "vlc" );
    }

    if( p_sys->i_fd < 0 )
    {
        msg_Err( p_aout, "cannot open esound socket (format 0x%08x at %d Hz)",
                         p_sys->esd_format, p_aout->output.output.i_rate );
        free( psz_server );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( psz_server && *psz_server )
    {
        struct timeval start, stop;
        esd_server_info_t * p_info;

        gettimeofday( &start, NULL );
        p_info = esd_get_server_info( p_sys->i_fd );
        gettimeofday( &stop, NULL );

        p_sys->latency = (mtime_t)( stop.tv_sec - start.tv_sec )
                           * (mtime_t)1000000;
        p_sys->latency += stop.tv_usec - start.tv_usec;
    }
    else
    {
        p_sys->latency = 0;
    }

    /* ESD latency is calculated for 44100 Hz. We don't have any way to get the
     * number of buffered samples, so I assume ESD_BUF_SIZE/2 */
    p_sys->latency +=
        (mtime_t)( esd_get_latency( i_newfd = esd_open_sound(NULL) )
                    + ESD_BUF_SIZE / 2
                      * p_aout->output.output.i_bytes_per_frame
                      * p_aout->output.output.i_rate
                      / ESD_DEFAULT_RATE )
      * (mtime_t)1000000
      / p_aout->output.output.i_bytes_per_frame
      / p_aout->output.output.i_rate;

    free( psz_server );
    close( i_newfd );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    aout_buffer_t * p_buffer;
    int i_tmp;

    p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );

    if ( p_buffer != NULL )
    {
        unsigned int pos;
        unsigned char *data = p_buffer->p_buffer;

        for( pos = 0; pos + ESD_BUF_SIZE <= p_buffer->i_nb_bytes;
             pos += ESD_BUF_SIZE )
        {
            i_tmp = write( p_sys->i_fd, data + pos, ESD_BUF_SIZE );
            if( i_tmp < 0 )
            {
                msg_Err( p_aout, "write failed (%m)" );
            }
        }
        aout_BufferFree( p_buffer );
    }
}

/*****************************************************************************
 * Close: close the Esound socket
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    close( p_sys->i_fd );
    free( p_sys );
}

#if 0
/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the socket
 *****************************************************************************/
static void Play( aout_thread_t *p_aout, uint8_t *buffer, int i_size )
{
    int i_amount;

    int m1 = p_aout->output.p_sys->esd_format & ESD_STEREO ? 1 : 2;
    int m2 = p_aout->output.p_sys->esd_format & ESD_BITS16 ? 64 : 128;

    i_amount = (m1 * 44100 * (ESD_BUF_SIZE + m1 * m2)) / p_aout->i_rate;

    write( p_aout->output.p_sys->i_fd, buffer, i_size );
}
#endif

