/*****************************************************************************
 * vpar_synchro.c : frame dropping routines
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Jean-Marc Dressler <polu@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include <stdlib.h>                                                /* free() */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "intf_msg.h"

#include "input.h"
#include "decoder_fifo.h"
#include "video.h"
#include "video_output.h"

#include "vdec_idct.h"
#include "video_decoder.h"
#include "vdec_motion.h"

#include "vpar_blocks.h"
#include "vpar_headers.h"
#include "vpar_synchro.h"
#include "video_parser.h"

#define MAX_COUNT 3

/*
 * Local prototypes
 */

#ifdef SAM_SYNCHRO

/*****************************************************************************
 * vpar_SynchroUpdateStructures : Update the synchro structures
 *****************************************************************************/
void vpar_SynchroUpdateStructures( vpar_thread_t * p_vpar,
                                   int i_coding_type, boolean_t b_kept )
{
    int             i_can_display;
    mtime_t         i_pts;
    pes_packet_t *  p_pes = p_vpar->bit_stream.p_decoder_fifo->buffer[
                               p_vpar->bit_stream.p_decoder_fifo->i_start ];

    /* try to guess the current DTS and PTS */
    if( p_pes->b_has_pts )
    {
        i_pts = p_pes->i_pts;

        /* if the image is I type, then the presentation timestamp is
         * the PTS of the PES. Otherwise, we calculate it with the
         * theorical framerate value */
        if( i_coding_type == I_CODING_TYPE )
        {
            p_vpar->synchro.i_last_pts = p_pes->i_pts;
        }
        else
        {
            p_vpar->synchro.i_last_pts += p_vpar->synchro.i_theorical_delay;
        }

        p_pes->b_has_pts = 0;
    }
    else
    {
        p_vpar->synchro.i_last_pts += p_vpar->synchro.i_theorical_delay;
        i_pts = p_vpar->synchro.i_last_pts;
    }

    /* update structures */
    switch(i_coding_type)
    {
        case P_CODING_TYPE:

            p_vpar->synchro.i_P_seen += 1024;
            if( b_kept ) p_vpar->synchro.i_P_kept += 1024;
            break;

        case B_CODING_TYPE:
            p_vpar->synchro.i_B_seen += 1024;
            if( b_kept ) p_vpar->synchro.i_B_kept += 1024;
            break;

        case I_CODING_TYPE:

            /* update the last I PTS we have, we need it to
             * calculate the theorical framerate */
            if (i_pts != p_vpar->synchro.i_last_seen_I_pts)
            {
                if ( p_vpar->synchro.i_last_seen_I_pts )
                {
                    p_vpar->synchro.i_theorical_delay =
                      1024 * ( i_pts - p_vpar->synchro.i_last_seen_I_pts )
                          / ( 1024 + p_vpar->synchro.i_B_seen
                                + p_vpar->synchro.i_P_seen);
                }
                p_vpar->synchro.i_last_seen_I_pts = i_pts;
            }

            /* now we calculated all statistics, it's time to
             * decide what we have the time to display */
            i_can_display = 
                ( (i_pts - p_vpar->synchro.i_last_kept_I_pts) << 10 )
                                / p_vpar->synchro.i_delay;

            p_vpar->synchro.b_all_I = 0;
            p_vpar->synchro.b_all_B = 0;
            p_vpar->synchro.b_all_P = 0;
            p_vpar->synchro.displayable_p = 0;
            p_vpar->synchro.displayable_b = 0;

            if( ( p_vpar->synchro.b_all_I = ( i_can_display >= 1024 ) ) )
            {
                i_can_display -= 1024;

                if( !( p_vpar->synchro.b_all_P
                        = ( i_can_display > p_vpar->synchro.i_P_seen ) ) )
                {
                    p_vpar->synchro.displayable_p = i_can_display;
                }
                else
                {
                    i_can_display -= p_vpar->synchro.i_P_seen;

                    if( !( p_vpar->synchro.b_all_B
                            = ( i_can_display > p_vpar->synchro.i_B_seen ) ) )
                    {
                        p_vpar->synchro.displayable_b = i_can_display;
                    }
                }
            }

#if 0
            if( p_vpar->synchro.b_all_I )
                intf_ErrMsg( "  I: 1024/1024  " );

            if( p_vpar->synchro.b_all_P )
                intf_ErrMsg( "P: %i/%i  ", p_vpar->synchro.i_P_seen,
                                           p_vpar->synchro.i_P_seen );
            else if( p_vpar->synchro.displayable_p > 0 )
                intf_ErrMsg( "P: %i/%i  ", p_vpar->synchro.displayable_p,
                                             p_vpar->synchro.i_P_seen );
            else
                intf_ErrMsg( "                " );

            if( p_vpar->synchro.b_all_B )
                intf_ErrMsg( "B: %i/%i", p_vpar->synchro.i_B_seen,
                                         p_vpar->synchro.i_B_seen );
            else if( p_vpar->synchro.displayable_b > 0 )
                intf_ErrMsg( "B: %i/%i", p_vpar->synchro.displayable_b,
                                           p_vpar->synchro.i_B_seen );
            else
                intf_ErrMsg( "                " );

            intf_ErrMsg( "Decoding: " );
            /*intf_ErrMsg( "\n" );*/
#endif
            p_vpar->synchro.i_P_seen = 0;
            p_vpar->synchro.i_B_seen = 0;

            /* update some values */
            if( b_kept )
            {
                p_vpar->synchro.i_last_kept_I_pts = i_pts;
                p_vpar->synchro.i_P_kept = 0;
                p_vpar->synchro.i_B_kept = 0;
            }

            break;
    }
}

/*****************************************************************************
 * vpar_SynchroChoose : Decide whether we will decode a picture or not
 *****************************************************************************/
boolean_t vpar_SynchroChoose( vpar_thread_t * p_vpar, int i_coding_type,
                              int i_structure )
{
    mtime_t i_delay = p_vpar->synchro.i_last_pts - mdate();

    switch( i_coding_type )
    {
        case I_CODING_TYPE:

            if( p_vpar->synchro.i_type != VPAR_SYNCHRO_DEFAULT )
            {
                /* I, IP, IP+, IPB */
                if( p_vpar->synchro.i_type == VPAR_SYNCHRO_Iplus )
                {
                    p_vpar->synchro.b_dropped_last = 1;
                }
                return( 1 );
            }

            return( p_vpar->synchro.b_all_I );

        case P_CODING_TYPE:

            if( p_vpar->synchro.i_type == VPAR_SYNCHRO_I ) /* I */
            {
                return( 0 );
            }

            if( p_vpar->synchro.i_type == VPAR_SYNCHRO_Iplus ) /* I+ */
            {
                if( p_vpar->synchro.b_dropped_last )
                {
                    p_vpar->synchro.b_dropped_last = 0;
                    return( 1 );
                }
                else
                {
                    return( 0 );
                }
            }

            if( p_vpar->synchro.i_type >= VPAR_SYNCHRO_IP ) /* IP, IP+, IPB */
            {
                return( 1 );
            }

            if( p_vpar->synchro.b_all_P )
            {
                return( 1 );
            }

            if( p_vpar->synchro.displayable_p * i_delay
                < p_vpar->synchro.i_delay )
            {
                return( 0 );
            }

            p_vpar->synchro.displayable_p -= 1024;

            return( 1 );

        case B_CODING_TYPE:

            if( p_vpar->synchro.i_type != VPAR_SYNCHRO_DEFAULT )
            {
                if( p_vpar->synchro.i_type <= VPAR_SYNCHRO_IP ) /* I, IP */
                {
                    return( 0 );
                }
                else if( p_vpar->synchro.i_type == VPAR_SYNCHRO_IPB ) /* IPB */
                {
                    return( 1 );
                }

                if( p_vpar->synchro.b_dropped_last ) /* IP+ */
                {
                    p_vpar->synchro.b_dropped_last = 0;
                    return( 1 );
                }

                p_vpar->synchro.b_dropped_last = 1;
                return( 0 );
            }

            if( p_vpar->synchro.b_all_B )
            {
                return( 1 );
            }

            if( p_vpar->synchro.displayable_b <= 0 )
            {
                return( 0 );
            }

            if( i_delay < 0 )
            {
                p_vpar->synchro.displayable_b -= 512;
                return( 0 );
            }

            p_vpar->synchro.displayable_b -= 1024;
            return( 1 );
    }

    return( 0 );

}

/*****************************************************************************
 * vpar_SynchroTrash : Update timers when we trash a picture
 *****************************************************************************/
void vpar_SynchroTrash( vpar_thread_t * p_vpar, int i_coding_type,
                        int i_structure )
{
    vpar_SynchroUpdateStructures (p_vpar, i_coding_type, 0);

}

/*****************************************************************************
 * vpar_SynchroDecode : Update timers when we decide to decode a picture
 *****************************************************************************/
void vpar_SynchroDecode( vpar_thread_t * p_vpar, int i_coding_type,
                            int i_structure )
{
    vpar_SynchroUpdateStructures (p_vpar, i_coding_type, 1);

    p_vpar->synchro.i_date_fifo[p_vpar->synchro.i_stop] = mdate();

    FIFO_INCREMENT( i_stop );

}

/*****************************************************************************
 * vpar_SynchroEnd : Called when the image is totally decoded
 *****************************************************************************/
void vpar_SynchroEnd( vpar_thread_t * p_vpar )
{
    if( p_vpar->synchro.i_stop != p_vpar->synchro.i_start )
    {
        mtime_t i_delay;

        i_delay = ( mdate() -
            p_vpar->synchro.i_date_fifo[p_vpar->synchro.i_start] )
              / ( (p_vpar->synchro.i_stop - p_vpar->synchro.i_start) & 0x0f );

        p_vpar->synchro.i_delay =
            ( 7 * p_vpar->synchro.i_delay + i_delay ) >> 3;

#if 0
        intf_ErrMsg( "decode %lli (mean %lli, theorical %lli)\n",
                     i_delay, p_vpar->synchro.i_delay,
                     p_vpar->synchro.i_theorical_delay );
#endif
    }
    else
    {
        intf_ErrMsg( "vpar error: critical ! fifo full\n" );
    }

    FIFO_INCREMENT( i_start );
}

/*****************************************************************************
 * vpar_SynchroDate : When an image has been decoded, ask for its date
 *****************************************************************************/
mtime_t vpar_SynchroDate( vpar_thread_t * p_vpar )
{
#if 0

    mtime_t i_displaydate = p_vpar->synchro.i_last_pts;

    static mtime_t i_delta = 0;

    intf_ErrMsg( "displaying image with delay %lli and delta %lli\n",
        i_displaydate - mdate(),
        i_displaydate - i_delta );

    intf_ErrMsg ( "theorical fps: %f - actual fps: %f \n",
        1000000.0 / p_vpar->synchro.i_theorical_delay, 1000000.0 / p_vpar->synchro.i_delay );

    i_delta = i_displaydate;

    return i_displaydate;
#else

    return p_vpar->synchro.i_last_pts;

#endif
}

#endif

#ifdef MEUUH_SYNCHRO

/* synchro a deux balles backportee du decodeur de reference. NE MARCHE PAS
AVEC LES IMAGES MONOTRAMES */

boolean_t vpar_SynchroChoose( vpar_thread_t * p_vpar, int i_coding_type,
                              int i_structure )
{
    switch (i_coding_type)
    {
    case B_CODING_TYPE:
        if ((p_vpar->synchro.kludge_level <= p_vpar->synchro.kludge_nbp))
        {
            p_vpar->synchro.kludge_b++;
            return( 0 );
        }
        if (p_vpar->synchro.kludge_b %
             (p_vpar->synchro.kludge_nbb /
                (p_vpar->synchro.kludge_level - p_vpar->synchro.kludge_nbp)))
        {
            p_vpar->synchro.kludge_b++;
            return( 0 );
        }
        p_vpar->synchro.kludge_b++;
        return( 1 );

    case P_CODING_TYPE:
        if (p_vpar->synchro.kludge_p++ >= p_vpar->synchro.kludge_level)
        {
            return( 0 );
        }
        return( 1 );

    default:
        return( 1 );
    }
}

void vpar_SynchroTrash( vpar_thread_t * p_vpar, int i_coding_type,
                        int i_structure )
{
    if (DECODER_FIFO_START(p_vpar->fifo)->b_has_pts && i_coding_type == I_CODING_TYPE)
    {
        p_vpar->synchro.kludge_nbframes = 0;
        p_vpar->synchro.kludge_date = DECODER_FIFO_START(p_vpar->fifo)->i_pts;
    }
    else
        p_vpar->synchro.kludge_nbframes++;
    DECODER_FIFO_START(p_vpar->fifo)->b_has_pts = 0;
}

void vpar_SynchroDecode( vpar_thread_t * p_vpar, int i_coding_type,
                            int i_structure )
{
    if (DECODER_FIFO_START(p_vpar->fifo)->b_has_pts && i_coding_type == I_CODING_TYPE)
    {
        p_vpar->synchro.kludge_nbframes = 0;
        p_vpar->synchro.kludge_date = DECODER_FIFO_START(p_vpar->fifo)->i_pts;
        DECODER_FIFO_START(p_vpar->fifo)->b_has_pts = 0;
    }
    else
        p_vpar->synchro.kludge_nbframes++;
}

mtime_t vpar_SynchroDate( vpar_thread_t * p_vpar )
{
    return( p_vpar->synchro.kludge_date
            + p_vpar->synchro.kludge_nbframes * 1000000
                / (p_vpar->sequence.i_frame_rate ) * 1001 );
}

void vpar_SynchroEnd( vpar_thread_t * p_vpar )
{
}

void vpar_SynchroKludge( vpar_thread_t * p_vpar, mtime_t date )
{
    mtime_t     show_date;
    int         temp = p_vpar->synchro.kludge_level;

    p_vpar->synchro.kludge_nbp = p_vpar->synchro.kludge_p ? p_vpar->synchro.kludge_p : 5;
    p_vpar->synchro.kludge_nbb = p_vpar->synchro.kludge_b ? p_vpar->synchro.kludge_b : 6;
    show_date = date - mdate();
    p_vpar->synchro.kludge_p = 0;
    p_vpar->synchro.kludge_b = 0;

    if (show_date < (SYNC_DELAY - SYNC_TOLERATE) && show_date <= p_vpar->synchro.kludge_prevdate)
    {
        p_vpar->synchro.kludge_level--;
        if (p_vpar->synchro.kludge_level < 0)
            p_vpar->synchro.kludge_level = 0;
        else if (p_vpar->synchro.kludge_level >
                     p_vpar->synchro.kludge_nbp + p_vpar->synchro.kludge_nbb)
            p_vpar->synchro.kludge_level = p_vpar->synchro.kludge_nbp + p_vpar->synchro.kludge_nbb;
#ifdef DEBUG
        if (temp != p_vpar->synchro.kludge_level)
            intf_DbgMsg("vdec debug: Level changed from %d to %d (%Ld)\n",
                        temp, p_vpar->synchro.kludge_level, show_date );
#endif
    }
    else if (show_date > (SYNC_DELAY + SYNC_TOLERATE) && show_date >= p_vpar->synchro.kludge_prevdate)
    {
        p_vpar->synchro.kludge_level++;
        if (p_vpar->synchro.kludge_level > p_vpar->synchro.kludge_nbp + p_vpar->synchro.kludge_nbb)
            p_vpar->synchro.kludge_level = p_vpar->synchro.kludge_nbp + p_vpar->synchro.kludge_nbb;
#ifdef DEBUG
        if (temp != p_vpar->synchro.kludge_level)
            intf_DbgMsg("vdec debug: Level changed from %d to %d (%Ld)\n",
                        temp, p_vpar->synchro.kludge_level, show_date );
#endif
    }

    p_vpar->synchro.kludge_prevdate = show_date;
    if ((p_vpar->synchro.kludge_level - p_vpar->synchro.kludge_nbp) > p_vpar->synchro.kludge_nbb)
        p_vpar->synchro.kludge_level = p_vpar->synchro.kludge_nbb + p_vpar->synchro.kludge_nbp;
}

#endif


#ifdef POLUX_SYNCHRO

void vpar_SynchroSetCurrentDate( vpar_thread_t * p_vpar, int i_coding_type )
{
    pes_packet_t * p_pes =
        p_vpar->bit_stream.p_decoder_fifo->buffer[p_vpar->bit_stream.p_decoder_fifo->i_start];


    switch( i_coding_type )
    {
    case B_CODING_TYPE:
        if( p_pes->b_has_pts )
        {
            if( p_pes->i_pts < p_vpar->synchro.i_current_frame_date )
            {
                intf_ErrMsg( "vpar warning: pts_date < current_date\n" );
            }
            p_vpar->synchro.i_current_frame_date = p_pes->i_pts;
            p_pes->b_has_pts = 0;
        }
        else
        {
            p_vpar->synchro.i_current_frame_date += 1000000 / (p_vpar->sequence.i_frame_rate) * 1001;
        }
        break;

    default:

        if( p_vpar->synchro.i_backward_frame_date == 0 )
        {
            p_vpar->synchro.i_current_frame_date += 1000000 / (p_vpar->sequence.i_frame_rate) * 1001;
        }
        else
        {
            if( p_vpar->synchro.i_backward_frame_date < p_vpar->synchro.i_current_frame_date )
            {
                intf_ErrMsg( "vpar warning: backward_date < current_date (%Ld)\n",
                         p_vpar->synchro.i_backward_frame_date - p_vpar->synchro.i_current_frame_date );
            }
            p_vpar->synchro.i_current_frame_date = p_vpar->synchro.i_backward_frame_date;
            p_vpar->synchro.i_backward_frame_date = 0;
        }

        if( p_pes->b_has_pts )
        {
            p_vpar->synchro.i_backward_frame_date = p_pes->i_pts;
            p_pes->b_has_pts = 0;
        }
       break;
    }
}

boolean_t vpar_SynchroChoose( vpar_thread_t * p_vpar, int i_coding_type,
                              int i_structure )
{
    boolean_t b_result = 1;
    int i_synchro_level = p_vpar->p_vout->i_synchro_level;

    vpar_SynchroSetCurrentDate( p_vpar, i_coding_type );

    /*
     * The synchro level is updated by the video input (see SynchroLevelUpdate)
     * so we just use the synchro_level to decide which frame to trash
     */

    switch( i_coding_type )
    {
    case I_CODING_TYPE:

        p_vpar->synchro.r_p_average =
            (p_vpar->synchro.r_p_average*(SYNC_AVERAGE_COUNT-1)+p_vpar->synchro.i_p_count)/SYNC_AVERAGE_COUNT;
        p_vpar->synchro.r_b_average =
            (p_vpar->synchro.r_b_average*(SYNC_AVERAGE_COUNT-1)+p_vpar->synchro.i_b_count)/SYNC_AVERAGE_COUNT;

        p_vpar->synchro.i_p_nb = (int)(p_vpar->synchro.r_p_average+0.5);
        p_vpar->synchro.i_b_nb = (int)(p_vpar->synchro.r_b_average+0.5);

        p_vpar->synchro.i_p_count = p_vpar->synchro.i_b_count = 0;
        p_vpar->synchro.i_b_trasher = p_vpar->synchro.i_b_nb / 2;
        p_vpar->synchro.i_i_count++;
       break;

    case P_CODING_TYPE:
        p_vpar->synchro.i_p_count++;
        if( p_vpar->synchro.i_p_count > i_synchro_level )
        {
            b_result = 0;
        }
        break;

    case B_CODING_TYPE:
        p_vpar->synchro.i_b_count++;
        if( p_vpar->synchro.i_p_nb >= i_synchro_level )
        {
            /* We must trash all the B */
            b_result = 0;
        }
        else
        {
            /* We use the brensenham algorithm to decide which B to trash */
            p_vpar->synchro.i_b_trasher +=
                p_vpar->synchro.i_b_nb - (i_synchro_level-p_vpar->synchro.i_p_nb);
            if( p_vpar->synchro.i_b_trasher >= p_vpar->synchro.i_b_nb )
            {
                b_result = 0;
                p_vpar->synchro.i_b_trasher -= p_vpar->synchro.i_b_nb;
            }
        }
        break;
    }

    return( b_result );
}

void vpar_SynchroTrash( vpar_thread_t * p_vpar, int i_coding_type,
                        int i_structure )
{
    vpar_SynchroChoose( p_vpar, i_coding_type, i_structure );
}

void vpar_SynchroUpdateLevel()
{
    //vlc_mutex_lock( &level_lock );
    //vlc_mutex_unlock( &level_lock );
}

mtime_t vpar_SynchroDate( vpar_thread_t * p_vpar )
{
    return( p_vpar->synchro.i_current_frame_date );
}

/* functions with no use */

void vpar_SynchroEnd( vpar_thread_t * p_vpar )
{
}

void vpar_SynchroDecode( vpar_thread_t * p_vpar, int i_coding_type,
                            int i_structure )
{
}

#endif
