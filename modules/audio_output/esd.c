/*****************************************************************************
 * esd.c : EsounD module
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: esd.c,v 1.4 2002/08/14 00:23:59 massiot Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include "aout_internal.h"

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
    vlc_bool_t   b_initialized;

    mtime_t      latency;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static int  SetFormat    ( aout_instance_t * );
static void Play         ( aout_instance_t *, aout_buffer_t * );
static int  ESDThread    ( aout_instance_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("EsounD audio module") );
    set_capability( "audio output", 50 );
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

    /* Allocate structure */
    p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return -1;
    }

    p_aout->output.p_sys = p_sys;

    /* Create ESD thread and wait for its readiness. */
    p_sys->b_initialized = VLC_FALSE;
    if( vlc_thread_create( p_aout, "aout", ESDThread, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create ESD thread (%s)", strerror(errno) );
        free( p_sys );
        return -1;
    }

    p_aout->output.pf_setformat = SetFormat;
    p_aout->output.pf_play = Play;

    return( 0 );
}

/*****************************************************************************
 * SetFormat: set the output format
 *****************************************************************************/
static int SetFormat( aout_instance_t *p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    p_sys->b_initialized = VLC_FALSE;

    /* Initialize some variables */
    p_sys->esd_format = ESD_BITS16 | ESD_STREAM | ESD_PLAY;
    p_sys->esd_format &= ~ESD_MASK_CHAN;

    switch( p_aout->output.output.i_channels )
    {
    case 1:
        p_sys->esd_format |= ESD_MONO;
        break;
    case 2:
        p_sys->esd_format |= ESD_STEREO;
        break;
    default:
        return -1;
    }

    /* open a socket for playing a stream
     * and try to open /dev/dsp if there's no EsounD */
    p_sys->i_fd = esd_play_stream_fallback( p_sys->esd_format,
                              p_aout->output.output.i_rate, NULL, "vlc" );
    if( p_sys->i_fd < 0 )
    {
        msg_Err( p_aout, "cannot open esound socket (format 0x%08x at %ld Hz)",
                         p_sys->esd_format, p_aout->output.output.i_rate );
        return -1;
    }

    p_aout->output.output.i_format = AOUT_FMT_S16_NE;
    p_aout->output.i_nb_samples = ESD_BUF_SIZE * 2;

    /* ESD latency is calculated for 44100 Hz. We don't have any way to get the
     * number of buffered samples, so I assume ESD_BUF_SIZE/2 */
    p_sys->latency =
        (mtime_t)( esd_get_latency( esd_open_sound(NULL) ) + ESD_BUF_SIZE / 2
                    * p_aout->output.output.i_rate / ESD_DEFAULT_RATE
                    * aout_FormatTo( &p_aout->output.output, 1 ) )
      * (mtime_t)1000000
      / (mtime_t)aout_FormatToByterate( &p_aout->output.output );

    p_sys->b_initialized = VLC_TRUE;

    return 0;
}

/*****************************************************************************
 * Play: queue a buffer for playing by ESDThread
 *****************************************************************************/
static void Play( aout_instance_t *p_aout, aout_buffer_t * p_buffer )
{
    aout_FifoPush( p_aout, &p_aout->output.fifo, p_buffer );
}

/*****************************************************************************
 * Close: close the Esound socket
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    p_aout->b_die = 1;
    vlc_thread_join( p_aout );

    close( p_sys->i_fd );
    free( p_sys );
}

/*****************************************************************************
 * ESDThread: asynchronous thread used to DMA the data to the device
 *****************************************************************************/
static int ESDThread( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    while ( !p_aout->b_die )
    {
        aout_buffer_t * p_buffer;
        int i_tmp, i_size;
        byte_t * p_bytes;

        if( !p_sys->b_initialized )
        {
            msleep( THREAD_SLEEP );
            continue;
        }

        /* Get the presentation date of the next write() operation. It
         * is equal to the current date + buffered samples + esd latency */
        p_buffer = aout_OutputNextBuffer( p_aout, mdate() + p_sys->latency );

        if ( p_buffer != NULL )
        {
            p_bytes = p_buffer->p_buffer;
            i_size = p_buffer->i_nb_bytes;
        }
        else
        {
            i_size = aout_FormatToByterate( &p_aout->output.output )
                      * ESD_BUF_SIZE * 2
                      / p_aout->output.output.i_rate;
            p_bytes = alloca( i_size );
            memset( p_bytes, 0, i_size );
        }

        i_tmp = write( p_sys->i_fd, p_bytes, i_size );

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

#if 0
/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the socket
 *****************************************************************************/
static void Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    int i_amount;

    int m1 = p_aout->output.p_sys->esd_format & ESD_STEREO ? 1 : 2;
    int m2 = p_aout->output.p_sys->esd_format & ESD_BITS16 ? 64 : 128;

    i_amount = (m1 * 44100 * (ESD_BUF_SIZE + m1 * m2)) / p_aout->i_rate;

    write( p_aout->output.p_sys->i_fd, buffer, i_size );
}
#endif

