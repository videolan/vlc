/*****************************************************************************
 * aout_spdif.c: AC3 passthrough output
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: aout_spdif.c,v 1.29 2002/06/01 12:32:01 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
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
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>                                              /* memset() */

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "audio_output.h"
#include "aout_spdif.h"

/****************************************************************************
 * iec958_build_burst: builds an iec958/spdif frame
 ****************************************************************************/
void iec958_build_burst( u8 * p_buf, aout_fifo_t * p_fifo )
{
    const u8 p_sync[6] = { 0x72, 0xF8, 0x1F, 0x4E, 0x01, 0x00 };
    int      i_length  = p_fifo->i_frame_size;
#ifndef HAVE_SWAB
    /* Skip the first byte if i_length is odd */
    u16 * p_in  = (u16 *)( p_fifo->buffer + ( i_length & 0x1 ) );
    u16 * p_out = (u16 *)p_buf;
#endif

    /* Add the spdif headers */
    memcpy( p_buf, p_sync, 6 );
    p_buf[6] = ( i_length * 8 ) & 0xFF;
    p_buf[7] = ( ( i_length * 8 ) >> 8 ) & 0xFF;

#ifdef HAVE_SWAB
    swab( p_fifo->buffer + p_fifo->i_start_frame * i_length,
          p_buf + 8, i_length );
#else
    /* i_length should be even */
    i_length &= ~0x1;

    while( i_length )
    {
        *p_out = ( (*p_in & 0x00ff) << 16 ) | ( (*p_in & 0xff00) >> 16 );
        p_in++;
        p_out++;
        i_length -= 2;
    }
#endif

    /* Add zeroes to complete the spdif frame,
     * they will be ignored by the decoder */
    memset( p_buf + 8 + i_length, 0, SPDIF_FRAME_SIZE - 8 - i_length );
}


/*****************************************************************************
 * aout_SpdifThread: audio output thread that sends raw spdif data
 *                   to an external decoder
 *****************************************************************************
 * This output thread is quite specific as it can only handle one fifo now.
 *
 * Note: spdif can demux up to 8 ac3 streams, and can even take
 * care of time stamps (cf ac3 spec) but I'm not sure all decoders
 * implement it.
 *****************************************************************************/
void aout_SpdifThread( aout_thread_t * p_aout )
{
    int         i_fifo;
    mtime_t     m_frame_time = 0;
    mtime_t     m_play;
    mtime_t     m_old;

    while( !p_aout->b_die )
    {
        i_fifo = 0;
        /* Find spdif fifo */
        while( ( p_aout->fifo[i_fifo].i_format != AOUT_FIFO_SPDIF ) &&
               ( i_fifo < AOUT_MAX_FIFOS ) && !p_aout->b_die )
        {
            i_fifo++;
        }

        m_old = 0;

        while( !p_aout->b_die && 
               !p_aout->fifo[i_fifo].b_die )
        {
            vlc_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
            if( !AOUT_FIFO_ISEMPTY( p_aout->fifo[i_fifo] ) )
            {
                /* Copy data from fifo to buffer to release the
                 * lock earlier */
                iec958_build_burst( p_aout->buffer,
                                    &p_aout->fifo[i_fifo] );

                m_play = p_aout->fifo[i_fifo].date[p_aout->fifo[i_fifo].
                             i_start_frame];

                p_aout->fifo[i_fifo].i_start_frame =
                    (p_aout->fifo[i_fifo].i_start_frame + 1 )
                    & AOUT_FIFO_SIZE;

                /* Compute the theorical duration of an ac3 frame */
                m_frame_time = 1000000 * AC3_FRAME_SIZE
                                       / p_aout->fifo[i_fifo].i_rate;

                vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );

                /* Play spdif frame to the external decoder 
                 * the kernel driver will sleep until the
                 * dsp buffer is empty enough to accept the data */
                if( m_play > ( mdate() - m_frame_time ) )
                {
                    /* check continuity */
                    if( (m_play - m_old) != m_frame_time )
                    {
                        mwait( m_play - m_frame_time );
                    }
                    else
                    {
                        mwait( m_play - 2 * m_frame_time );
                    }
                    m_old = m_play;
                    
                    p_aout->pf_getbufinfo( p_aout, 0 );

                    p_aout->pf_play( p_aout, (byte_t *)p_aout->buffer,
                                     SPDIF_FRAME_SIZE );
                }
            }
            else
            {
                msg_Warn( p_aout, "empty spdif fifo" );
                while( AOUT_FIFO_ISEMPTY( p_aout->fifo[i_fifo] ) &&
                       !p_aout->b_die && 
                       !p_aout->fifo[i_fifo].b_die )

                {
                    vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );
                    msleep( m_frame_time );
                    vlc_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
                }
                vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );
            }
        }

        vlc_mutex_lock( &p_aout->fifos_lock );
        aout_FreeFifo( &p_aout->fifo[i_fifo] );
        vlc_mutex_unlock( &p_aout->fifos_lock );
    }

    return;
}

