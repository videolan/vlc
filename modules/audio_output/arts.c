/*****************************************************************************
 * arts.c : aRts module
 *****************************************************************************
 * Copyright (C) 2001-2002 VideoLAN
 * $Id: arts.c,v 1.16 2002/11/15 17:17:29 gbazin Exp $
 *
 * Authors: Emmanuel Blindauer <manu@agat.net>
 *          Samuel Hocevar <sam@zoy.org>
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

#include <artsc.h>

/*****************************************************************************
 * aout_sys_t: arts audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some arts specific variables.
 *****************************************************************************/
struct aout_sys_t
{
    arts_stream_t stream;

    mtime_t       latency;
    int           i_size;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Play         ( aout_instance_t * );
static int  aRtsThread   ( aout_instance_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
   set_description( _("aRts audio module") );
   set_capability( "audio output", 50 );
   set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open an aRts socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;
    int i_err;
    int i_nb_channels;

    /* Allocate structure */
    p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return -1;
    }
    p_aout->output.p_sys = p_sys;

    i_err = arts_init();
    
    if (i_err < 0)
    {
        msg_Err( p_aout, "arts_init failed (%s)", arts_error_text(i_err) );
        free( p_sys );
        return -1;
    }

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    p_aout->output.output.i_format = AOUT_FMT_S16_NE;
    i_nb_channels = aout_FormatNbChannels( &p_aout->output.output );
    if ( i_nb_channels > 2 )
    {
        /* aRts doesn't support more than two channels. */
        i_nb_channels = 2;
        p_aout->output.output.i_physical_channels =
            AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }

    /* Open a socket for playing a stream, set format to 16 bits */
    p_sys->stream = arts_play_stream( p_aout->output.output.i_rate, 16,
                                      i_nb_channels, "vlc" );
    if( p_sys->stream == NULL )
    {
        msg_Err( p_aout, "cannot open aRts socket" );
        free( p_sys );
        return -1;
    }

    /* Try not to bufferize more than 200 ms */
    arts_stream_set( p_sys->stream, ARTS_P_BUFFER_TIME, 50 );

    /* Estimate latency with a half full buffer */
    p_sys->latency = (mtime_t)1000
       * (mtime_t)arts_stream_get( p_sys->stream, ARTS_P_SERVER_LATENCY );
    p_sys->i_size = arts_stream_get( p_sys->stream, ARTS_P_PACKET_SIZE );

    msg_Dbg( p_aout, "aRts initialized, latency %i000, %i packets of size %i",
                     arts_stream_get( p_sys->stream, ARTS_P_SERVER_LATENCY ),
                     arts_stream_get( p_sys->stream, ARTS_P_PACKET_COUNT ),
                     arts_stream_get( p_sys->stream, ARTS_P_PACKET_SIZE ) );

    p_aout->output.i_nb_samples = p_sys->i_size / sizeof(u16) / i_nb_channels;

    /* Create aRts thread and wait for its readiness. */
    if( vlc_thread_create( p_aout, "aout", aRtsThread,
                           VLC_THREAD_PRIORITY_OUTPUT, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create aRts thread (%s)", strerror(errno) );
        arts_close_stream( p_sys->stream );
        arts_free();
        free( p_sys );
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
    ;
}

/*****************************************************************************
 * Close: close the aRts socket
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    p_aout->b_die = 1;
    vlc_thread_join( p_aout );

    arts_close_stream( p_sys->stream );
    arts_free();
    free( p_sys );
}

/*****************************************************************************
 * aRtsThread: asynchronous thread used to DMA the data to the device
 *****************************************************************************/
static int aRtsThread( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
mtime_t calldate = mdate();

    while ( !p_aout->b_die )
    {
        aout_buffer_t * p_buffer;
        int i_tmp, i_size;
        byte_t * p_bytes;

fprintf(stderr, "can write %i\n", arts_stream_get( p_sys->stream, ARTS_P_BUFFER_SPACE ) );
while( arts_stream_get( p_sys->stream, ARTS_P_BUFFER_SPACE ) < 16384*3/2 )
{
fprintf(stderr, "sleep\n");
    msleep( 10000 );
}
fprintf(stderr, "after sleep: can write %i\n", arts_stream_get( p_sys->stream, ARTS_P_BUFFER_SPACE ) );

        /* Get the presentation date of the next write() operation. It
         * is equal to the current date + latency */
        p_buffer = aout_OutputNextBuffer( p_aout, mdate() + p_sys->latency / 4,
                                                  VLC_TRUE );

        if ( p_buffer != NULL )
        {
fprintf(stderr, "buffer duration "I64Fd", bytes %i\n", p_buffer->end_date - p_buffer->start_date, p_buffer->i_nb_bytes);
            p_bytes = p_buffer->p_buffer;
            i_size = p_buffer->i_nb_bytes;
        }
        else
        {
            i_size = p_sys->i_size;
            p_bytes = malloc( i_size );
            memset( p_bytes, 0, i_size );
        }

fprintf(stderr, "WRITING %i bytes\n", i_size);
        i_tmp = arts_write( p_sys->stream, p_bytes, i_size );
fprintf(stderr, "mdate: "I64Fd"\n", mdate() - calldate);
calldate = mdate();
fprintf(stderr, "can write %i\n", arts_stream_get( p_sys->stream, ARTS_P_BUFFER_SPACE ) );

        if( i_tmp < 0 )
        {
            msg_Err( p_aout, "write failed (%s)", arts_error_text(i_tmp) );
        }

        if ( p_buffer != NULL )
        {
            aout_BufferFree( p_buffer );
        }
        else
        {
            free( p_bytes );
        }
    }

    return 0;
}

