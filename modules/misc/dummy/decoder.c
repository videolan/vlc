/*****************************************************************************
 * dec_dummy.c: dummy decoder plugin for vlc.
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: decoder.c,v 1.3 2002/10/27 16:58:13 gbazin Exp $
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
#include <vlc/vlc.h>
#include <vlc/decoder.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h> /* write(), close() */
#endif

#include <sys/types.h> /* open() */
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h> /* sprintf() */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Run ( decoder_fifo_t * );

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Always returns 0 because we are the dummy decoder!
 *****************************************************************************/
int E_(OpenDecoder) ( vlc_object_t *p_this )
{
    ((decoder_fifo_t*)p_this)->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Run: this function is called just after the thread is created
 *****************************************************************************/
static int Run ( decoder_fifo_t *p_fifo )
{
    u8           p_buffer[1024];

    bit_stream_t bit_stream;
    mtime_t      last_date = mdate();
    size_t       i_bytes = 0;

    char         psz_file[100];
    int          i_fd;

    sprintf( psz_file, "stream.%i", p_fifo->i_object_id );
    i_fd = open( psz_file, O_WRONLY | O_CREAT | O_TRUNC, 00644 );

    if( i_fd == -1 )
    {
        msg_Err( p_fifo, "cannot create `%s'", psz_file );
        p_fifo->b_error = 1;
        DecoderError( p_fifo );
        return -1;
    }

    msg_Dbg( p_fifo, "dumping stream to file `%s'", psz_file );

    if( InitBitstream( &bit_stream, p_fifo, NULL, NULL ) != VLC_SUCCESS )
    {
        msg_Err( p_fifo, "cannot initialize bitstream" );
        p_fifo->b_error = 1;
        DecoderError( p_fifo );
        close( i_fd );
        return -1;
    }

    while( !p_fifo->b_die && !p_fifo->b_error )
    {
        GetChunk( &bit_stream, p_buffer, 1024 );
        write( i_fd, p_buffer, 1024 );

        i_bytes += 1024;

        if( mdate() < last_date + 2000000 )
        {
            continue;
        }

        msg_Dbg( p_fifo, "dumped %i bytes", i_bytes );

        i_bytes = 0;
        last_date = mdate();
    }

    if( i_bytes )
    {
        msg_Dbg( p_fifo, "dumped %i bytes", i_bytes );
    }

    close( i_fd );
    CloseBitstream( &bit_stream );

    if( p_fifo->b_error )
    {
        DecoderError( p_fifo );
        return -1;
    }

    return 0;
}
