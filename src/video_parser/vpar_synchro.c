/*****************************************************************************
 * vpar_motion.c : motion vectors parsing
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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
 * vpar_SynchroUpdateTab : Update a mean table in the synchro structure
 *****************************************************************************/
float vpar_SynchroUpdateTab( video_synchro_tab_t * tab, int count )
{

    tab->mean = ( tab->mean + MAX_COUNT * count ) / ( MAX_COUNT + 1 );
    tab->deviation = ( tab->deviation + MAX_COUNT * abs (tab->mean - count) )
                        / ( MAX_COUNT + 1 );

    return tab->deviation;
}

/*****************************************************************************
 * vpar_SynchroUpdateStructures : Update the synchro structures
 *****************************************************************************/
void vpar_SynchroUpdateStructures( vpar_thread_t * p_vpar,
                                   int i_coding_type, int dropped )
{
    float candidate_deviation;
    float optimal_deviation;
    float predict;
    mtime_t i_current_pts;
    mtime_t i_delay;
    mtime_t i_displaydate;
    decoder_fifo_t * decoder_fifo = p_vpar->bit_stream.p_decoder_fifo;

    /* interpolate the current _decode_ PTS */
    i_current_pts = decoder_fifo->buffer[decoder_fifo->i_start]->b_has_pts ?
                    decoder_fifo->buffer[decoder_fifo->i_start]->i_pts :
                    0;
    if( !i_current_pts )
    {
        i_current_pts = p_vpar->synchro.i_last_decode_pts
                       + 1000000.0 / (1 + p_vpar->synchro.actual_fps);
    }
    p_vpar->synchro.i_last_decode_pts = i_current_pts;

    /* see if the current image has a pts - if not, set to 0 */
    p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_stop].i_pts
            = i_current_pts;

    /* update display time */
    i_displaydate = decoder_fifo->buffer[decoder_fifo->i_start]->b_has_pts ?
                    decoder_fifo->buffer[decoder_fifo->i_start]->i_pts :
                    0;
    if( !i_displaydate || i_coding_type != I_CODING_TYPE )
    {
        if (!p_vpar->synchro.i_images_since_pts )
            p_vpar->synchro.i_images_since_pts = 10;

        i_displaydate = p_vpar->synchro.i_last_display_pts
                       + 1000000.0 / (p_vpar->synchro.theorical_fps);
    }

    decoder_fifo->buffer[decoder_fifo->i_start]->b_has_pts = 0;

    p_vpar->synchro.i_images_since_pts--;
    p_vpar->synchro.i_last_display_pts = i_displaydate;



    /* update structures */
    switch(i_coding_type)
    {
        case P_CODING_TYPE:

            p_vpar->synchro.current_p_count++;
            if( !dropped ) p_vpar->synchro.nondropped_p_count++;
            break;

        case B_CODING_TYPE:
            p_vpar->synchro.current_b_count++;
            if( !dropped ) p_vpar->synchro.nondropped_b_count++;
            break;

        case I_CODING_TYPE:

            /* update information about images we can decode */
            if (i_current_pts != p_vpar->synchro.i_last_i_pts)
            {
                if ( p_vpar->synchro.i_last_i_pts && i_current_pts != p_vpar->synchro.i_last_i_pts)
                {
                    p_vpar->synchro.theorical_fps = (p_vpar->synchro.theorical_fps + 1000000.0 * (1 + p_vpar->synchro.current_b_count + p_vpar->synchro.current_p_count) / (i_current_pts - p_vpar->synchro.i_last_i_pts)) / 2;
                }
                p_vpar->synchro.i_last_i_pts = i_current_pts;
            }

            if( !dropped )
            {
                if ( p_vpar->synchro.i_last_nondropped_i_pts && i_current_pts != p_vpar->synchro.i_last_nondropped_i_pts)
                {
                    p_vpar->synchro.actual_fps = (p_vpar->synchro.actual_fps + 1000000.0 * (1 + p_vpar->synchro.nondropped_b_count + p_vpar->synchro.nondropped_p_count) / (i_current_pts - p_vpar->synchro.i_last_nondropped_i_pts)) / 2;
                }

            }


            /* update all the structures for P images */

            /* period == 1 */
            optimal_deviation = vpar_SynchroUpdateTab(
                            &p_vpar->synchro.tab_p[0],
                            p_vpar->synchro.current_p_count);
            predict = p_vpar->synchro.tab_p[0].mean;

            /* period == 2 */
            candidate_deviation = vpar_SynchroUpdateTab(
                            &p_vpar->synchro.tab_p[1 + (p_vpar->synchro.modulo & 0x1)],
                            p_vpar->synchro.current_p_count);
            if (candidate_deviation < optimal_deviation)
            {
                optimal_deviation = candidate_deviation;
                predict = p_vpar->synchro.tab_p[1 + (p_vpar->synchro.modulo & 0x1)].mean;
            }

            /* period == 3 */
            candidate_deviation = vpar_SynchroUpdateTab(
                            &p_vpar->synchro.tab_p[3 + (p_vpar->synchro.modulo % 3)],
                            p_vpar->synchro.current_p_count);
            if (candidate_deviation < optimal_deviation)
            {
                optimal_deviation = candidate_deviation;
                predict = p_vpar->synchro.tab_p[1 + (p_vpar->synchro.modulo % 3)].mean;
            }

            p_vpar->synchro.p_count_predict = predict;
            p_vpar->synchro.current_p_count = 0;


            /* update all the structures for B images */

            /* period == 1 */
            optimal_deviation = vpar_SynchroUpdateTab(
                            &p_vpar->synchro.tab_b[0],
                            p_vpar->synchro.current_b_count);
            predict = p_vpar->synchro.tab_b[0].mean;

            /* period == 2 */
            candidate_deviation = vpar_SynchroUpdateTab(
                            &p_vpar->synchro.tab_b[1 + (p_vpar->synchro.modulo & 0x1)],
                            p_vpar->synchro.current_b_count);
            if (candidate_deviation < optimal_deviation)
            {
                optimal_deviation = candidate_deviation;
                predict = p_vpar->synchro.tab_b[1 + (p_vpar->synchro.modulo & 0x1)].mean;
            }

            /* period == 3 */
            candidate_deviation = vpar_SynchroUpdateTab(
                            &p_vpar->synchro.tab_b[3 + (p_vpar->synchro.modulo % 3)],
                            p_vpar->synchro.current_b_count);
            if (candidate_deviation < optimal_deviation)
            {
                optimal_deviation = candidate_deviation;
                predict = p_vpar->synchro.tab_b[1 + (p_vpar->synchro.modulo % 3)].mean;
            }

            p_vpar->synchro.b_count_predict = predict;
            p_vpar->synchro.current_b_count = 0;

            /* now we calculated all statistics, it's time to
             * decide what we have the time to display
             */
            i_delay = i_current_pts - p_vpar->synchro.i_last_nondropped_i_pts;

            p_vpar->synchro.can_display_i
                = ( p_vpar->synchro.i_mean_decode_time < i_delay );

            p_vpar->synchro.can_display_p
                    = ( p_vpar->synchro.i_mean_decode_time
                    * (1 + p_vpar->synchro.p_count_predict) < i_delay );

            if( !p_vpar->synchro.can_display_p )
            {
                p_vpar->synchro.displayable_p
                    = -1 + i_delay / p_vpar->synchro.i_mean_decode_time;
                if( p_vpar->synchro.displayable_p < 0 )
                    p_vpar->synchro.displayable_p = 0;
            }
            else
                p_vpar->synchro.displayable_p = 0;

            if( p_vpar->synchro.can_display_p
                && !(p_vpar->synchro.can_display_b
                    = ( p_vpar->synchro.i_mean_decode_time
                    * (1 + p_vpar->synchro.b_count_predict
                        + p_vpar->synchro.p_count_predict)) < i_delay) )
            {
                p_vpar->synchro.displayable_b
                    = -2.0 + i_delay / p_vpar->synchro.i_mean_decode_time
                        - p_vpar->synchro.can_display_p;
            }
            else
                p_vpar->synchro.displayable_b = 0;

#if 0
            intf_DbgMsg( "I %i  P %i (%f)  B %i (%f)\n",
                p_vpar->synchro.can_display_i,
                p_vpar->synchro.can_display_p,
                p_vpar->synchro.displayable_p,
                p_vpar->synchro.can_display_b,
                p_vpar->synchro.displayable_b );
#endif

            /* update some values */
            if( !dropped )
            {
                p_vpar->synchro.i_last_nondropped_i_pts = i_current_pts;
                p_vpar->synchro.nondropped_p_count = 0;
                p_vpar->synchro.nondropped_b_count = 0;
            }

            break;

    }

    p_vpar->synchro.modulo++;

}

/*****************************************************************************
 * vpar_SynchroChoose : Decide whether we will decode a picture or not
 *****************************************************************************/
boolean_t vpar_SynchroChoose( vpar_thread_t * p_vpar, int i_coding_type,
                              int i_structure )
{
    mtime_t i_delay = p_vpar->synchro.i_last_decode_pts - mdate();

    switch( i_coding_type )
    {
        case I_CODING_TYPE:

            return( p_vpar->synchro.can_display_i );

        case P_CODING_TYPE:

            if( p_vpar->synchro.can_display_p )
                return( 1 );

            if( p_vpar->synchro.displayable_p * i_delay
                < p_vpar->synchro.i_mean_decode_time )
            {
                //intf_ErrMsg( "trashed a P\n" );
                return( 0 );
            }

            p_vpar->synchro.displayable_p--;
            return( 1 );

        case B_CODING_TYPE:

            if( p_vpar->synchro.can_display_b )
                return( 1 );

            /* modulo & 0x3 is here to add some randomness */
            if( i_delay < (1 + (p_vpar->synchro.modulo & 0x3))
                * p_vpar->synchro.i_mean_decode_time )
            {
                //intf_ErrMsg( "trashed a B\n" );
                return( 0 );
            }

            if( p_vpar->synchro.displayable_b <= 0 )
                return( 0 );

            p_vpar->synchro.displayable_b--;
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
    vpar_SynchroUpdateStructures (p_vpar, i_coding_type, 1);

}

/*****************************************************************************
 * vpar_SynchroDecode : Update timers when we decide to decode a picture
 *****************************************************************************/
void vpar_SynchroDecode( vpar_thread_t * p_vpar, int i_coding_type,
                            int i_structure )
{
    vpar_SynchroUpdateStructures (p_vpar, i_coding_type, 0);

    p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_stop].i_decode_date = mdate();
    p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_stop].i_image_type
        = i_coding_type;

    p_vpar->synchro.i_fifo_stop = (p_vpar->synchro.i_fifo_stop + 1) & 0xf;

}

/*****************************************************************************
 * vpar_SynchroEnd : Called when the image is totally decoded
 *****************************************************************************/
void vpar_SynchroEnd( vpar_thread_t * p_vpar )
{
    mtime_t i_decode_time;

    i_decode_time = (mdate() -
            p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_start].i_decode_date)
        / ( (p_vpar->synchro.i_fifo_stop - p_vpar->synchro.i_fifo_start) & 0x0f);

    p_vpar->synchro.i_mean_decode_time =
        ( 7 * p_vpar->synchro.i_mean_decode_time + i_decode_time ) / 8;

    /* intf_ErrMsg( "decoding time was %lli\n",
        p_vpar->synchro.i_mean_decode_time ); */

    p_vpar->synchro.i_fifo_start = (p_vpar->synchro.i_fifo_start + 1) & 0xf;

}

/*****************************************************************************
 * vpar_SynchroDate : When an image has been decoded, ask for its date
 *****************************************************************************/
mtime_t vpar_SynchroDate( vpar_thread_t * p_vpar )
{
    mtime_t i_displaydate = p_vpar->synchro.i_last_display_pts;

#if 0
    static mtime_t i_delta = 0;

    intf_ErrMsg( "displaying type %i with delay %lli and delta %lli\n",
        p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_start].i_image_type,
        i_displaydate - mdate(),
        i_displaydate - i_delta );

    intf_ErrMsg ( "theorical fps: %f - actual fps: %f \n",
        p_vpar->synchro.theorical_fps, p_vpar->synchro.actual_fps );

    i_delta = i_displaydate;
#endif

    return i_displaydate;
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
            + p_vpar->synchro.kludge_nbframes*1000000/(p_vpar->sequence.r_frame_rate ) );
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
            p_vpar->synchro.i_current_frame_date += 1000000/(p_vpar->sequence.r_frame_rate);
        }
        break;

    default:

        if( p_vpar->synchro.i_backward_frame_date == 0 )
        {
            p_vpar->synchro.i_current_frame_date += 1000000/(p_vpar->sequence.r_frame_rate);
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
