/*****************************************************************************
 * aout_s16.c: 16 bit signed audio output functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Cyril Deguet <asmax@via.ecp.fr>
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
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <videolan/vlc.h>

#include "audio_output.h"
#include "aout_common.h"

/*****************************************************************************
 * Functions
 *****************************************************************************/

void aout_S16Thread( aout_thread_t * p_aout )
{
    int  i_fifo;
    long l_buffer, l_buffer_limit, l_bytes;

    /* As the s32_buffer was created with calloc(), we don't have to set this
     * memory to zero and we can immediately jump into the thread's loop */
    while ( ! p_aout->b_die )
    {
        vlc_mutex_lock( &p_aout->fifos_lock );

        for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
        {
            if( p_aout->fifo[i_fifo].b_die )
            {
                aout_FreeFifo( &p_aout->fifo[i_fifo] );
            }
            else
            {
                aout_FillBuffer( p_aout, &p_aout->fifo[i_fifo] );
            }
        }

        vlc_mutex_unlock( &p_aout->fifos_lock );

        l_buffer_limit = p_aout->l_units <<  p_aout->b_stereo;

        for ( l_buffer = 0; l_buffer < l_buffer_limit; l_buffer++ )
        {
            ((s16 *)p_aout->buffer)[l_buffer] =
                     (s16)( ( p_aout->s32_buffer[l_buffer] / AOUT_MAX_FIFOS )
                            * p_aout->i_volume / 256 ) ;
            p_aout->s32_buffer[l_buffer] = 0;
        }

        l_bytes = p_aout->pf_getbufinfo( p_aout, l_buffer_limit );

        p_aout->date = mdate() + ((((mtime_t)((l_bytes + 4 * p_aout->i_latency) / 
                (sizeof(s16) << p_aout->b_stereo))) * 1000000) / 
                ((mtime_t)p_aout->l_rate)) + p_main->i_desync;
                
        p_aout->pf_play( p_aout, (byte_t *)p_aout->buffer,
                         l_buffer_limit * sizeof(s16) );

        if ( l_bytes > (l_buffer_limit * sizeof(s16)) )
        {
            msleep( p_aout->l_msleep );
        }
    }

    vlc_mutex_lock( &p_aout->fifos_lock );

    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        aout_FreeFifo( &p_aout->fifo[i_fifo] );
    }

    vlc_mutex_unlock( &p_aout->fifos_lock );
}

