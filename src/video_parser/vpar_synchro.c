/*****************************************************************************
 * vpar_motion.c : motion vectors parsing
 * (c)1999 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/uio.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "intf_msg.h"
#include "debug.h"                    /* ?? temporaire, requis par netlist.h */

#include "input.h"
#include "input_netlist.h"
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
#include "video_fifo.h"

#define MAX_COUNT 3

/*
 * Local prototypes
 */

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
    mtime_t i_displaydate;
    decoder_fifo_t * decoder_fifo = p_vpar->bit_stream.p_decoder_fifo;

    /* interpolate the current _decode_ PTS */
    i_current_pts = decoder_fifo->buffer[decoder_fifo->i_start]->i_pts;
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
    i_displaydate = decoder_fifo->buffer[decoder_fifo->i_start]->i_pts;
    if( !i_displaydate || i_coding_type != I_CODING_TYPE )
    {
        if (!p_vpar->synchro.i_images_since_pts )
            p_vpar->synchro.i_images_since_pts = 10;

        i_displaydate = p_vpar->synchro.i_last_display_pts
                       + 1000000.0 / (p_vpar->synchro.theorical_fps);
        //fprintf (stderr, "  ");
    }
    //else fprintf (stderr, "R ");
    //if (dropped) fprintf (stderr, "  "); else fprintf (stderr, "* ");
    //fprintf (stderr, "%i ", i_coding_type);
    //fprintf (stderr, "pts %lli delta %lli\n", i_displaydate, i_displaydate - p_vpar->synchro.i_last_display_pts);

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
    
                p_vpar->synchro.i_last_nondropped_i_pts = i_current_pts;
                p_vpar->synchro.nondropped_p_count = 0;
                p_vpar->synchro.nondropped_b_count = 0;
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
    mtime_t i_delay;
    int keep;

    i_delay = p_vpar->synchro.i_last_decode_pts - mdate();

    //fprintf( stderr, "delay is %lli - ", i_delay);
    
#if 1
    /*if ( i_coding_type == B_CODING_TYPE )
        return (0);*/

    //return( i_coding_type == I_CODING_TYPE || i_coding_type == P_CODING_TYPE );
    if( i_delay > 120000 )
    {	    
        keep = 1;
    }
    else if( i_delay > 100000 )
    {
        keep = ( i_coding_type == I_CODING_TYPE
                    || i_coding_type == P_CODING_TYPE );
    }
    else if( i_delay > 50000 )
    {	    
        keep = ( i_coding_type == I_CODING_TYPE );
    }
    else
    {	    
        keep = 0;
    }
#endif

    //if (!keep) fprintf( stderr, "trashing a type %i with delay %lli\n", i_coding_type, i_delay);
//    else fprintf( stderr, "chooser :               ok - displaying a %i \n", i_coding_type);

    return (keep);

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
        / (p_vpar->synchro.i_fifo_stop - p_vpar->synchro.i_fifo_start & 0x0f);

    p_vpar->synchro.i_mean_decode_time =
        ( 7 * p_vpar->synchro.i_mean_decode_time + i_decode_time ) / 8;

    //fprintf (stderr, "decoding time is %lli\n", p_vpar->synchro.i_mean_decode_time);

    p_vpar->synchro.i_fifo_start = (p_vpar->synchro.i_fifo_start + 1) & 0xf;

}

/*****************************************************************************
 * vpar_SynchroDate : When an image has been decoded, ask for its date
 *****************************************************************************/
mtime_t vpar_SynchroDate( vpar_thread_t * p_vpar )
{
    mtime_t i_displaydate = p_vpar->synchro.i_last_display_pts;
    static mtime_t i_delta = 0;
    
    //fprintf(stderr, "displaying type %i with delay %lli and delta %lli\n", p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_start].i_image_type, i_displaydate - mdate(), i_displaydate - i_delta);
    //fprintf (stderr, "theorical fps: %f - actual fps: %f \n", p_vpar->synchro.theorical_fps, p_vpar->synchro.actual_fps);

    i_delta = i_displaydate;
    return i_displaydate;
}

