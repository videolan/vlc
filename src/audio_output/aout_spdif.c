/*****************************************************************************
 * aout_spdif: ac3 passthrough output
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: aout_spdif.c,v 1.17 2001/10/13 15:34:21 stef Exp $
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
#include "defs.h"

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>                                              /* memset() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"                             /* mtime_t, mdate(), msleep() */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "audio_output.h"
#include "aout_common.h"

#define FRAME_TIME      32000

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
  mtime_t     m_old = 0;


  intf_WarnMsg( 3, "aout info: starting spdif output loop" );

  while( !p_aout->b_die )
  {
    for( i_fifo = 0 ; i_fifo < AOUT_MAX_FIFOS ; i_fifo++ )
    {
      /* the loop read each fifo so that we can change the stream
       * on the fly but mulitplexing is not handled yet so
       * the sound will be broken is more than one fifo has data */ 
      /* TODO: write the muliplexer :) */
      if( p_aout->fifo[i_fifo].i_type == AOUT_ADEC_SPDIF_FIFO )
      {
        vlc_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
        if( p_aout->fifo[i_fifo].b_die )
        {
          vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );

          vlc_mutex_lock( &p_aout->fifos_lock );
          aout_FreeFifo( &p_aout->fifo[i_fifo] );
          vlc_mutex_unlock( &p_aout->fifos_lock );
        }
        else if( !AOUT_FIFO_ISEMPTY( p_aout->fifo[i_fifo] ) )
        {
          /* Copy data from fifo to buffer to release the lock earlier */
          memcpy( p_aout->buffer,
                  (byte_t *)p_aout->fifo[i_fifo].buffer
                          + p_aout->fifo[i_fifo].l_start_frame
                          * SPDIF_FRAME_SIZE,
                  SPDIF_FRAME_SIZE );

          m_play = p_aout->fifo[i_fifo].date[p_aout->fifo[i_fifo].
                       l_start_frame];

          p_aout->fifo[i_fifo].l_start_frame =
              (p_aout->fifo[i_fifo].l_start_frame + 1 )
              & AOUT_FIFO_SIZE;

          /* Compute the theorical duration of an ac3 frame */
          m_frame_time = 1000000 * AC3_FRAME_SIZE
                                 / p_aout->fifo[i_fifo].l_rate;

          vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );

          /* play spdif frame to the external decoder 
           * the kernel driver will sleep until the
           * dsp buffer is empty enough to accept the data */
          if( m_play > ( mdate() - m_frame_time ) )
          {
            /* check continuity */
            if( (m_play - m_old) != m_frame_time )
            {
              intf_WarnMsg( 6, "aout warning: long frame ? (%lld)",
                               m_play - m_old );
              mwait( m_play );
            }
            else
            {
              mwait( m_play - 3* m_frame_time );
            }
            m_old = m_play;

            p_aout->pf_play( p_aout,
                             (byte_t *)p_aout->buffer,
                             SPDIF_FRAME_SIZE );
          }
          else
          {
            intf_WarnMsg( 6, "aout info: late spdif frame" );
          }
        }
        else
        {
          vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );
          msleep( m_frame_time );
          intf_WarnMsg( 6, "aout info: empty spdif fifo" );
        }
      }
    }
  }

  intf_WarnMsg( 3, "aout info: exiting spdif loop" );
  vlc_mutex_lock( &p_aout->fifos_lock );

  for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
  {
    aout_FreeFifo( &p_aout->fifo[i_fifo] );
  }

  vlc_mutex_unlock( &p_aout->fifos_lock );

  return;
}

