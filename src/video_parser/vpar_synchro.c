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
#include "video_fifo.h"
#include "vpar_synchro.h"
#include "video_parser.h"

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
                                   int i_coding_type )
{
    float candidate_deviation;
    float optimal_deviation;
    float predict;

    switch(i_coding_type)
    {
        case P_CODING_TYPE:
            p_vpar->synchro.current_p_count++;
            break;
        case B_CODING_TYPE:
            p_vpar->synchro.current_b_count++;
            break;
        case I_CODING_TYPE:

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
    static int meuh = 1;
    static int truc = 0;
//    return( 1 );
    if( i_coding_type == 1 )
        meuh = 1;
    if( i_coding_type == 2 )
        meuh++;
    truc++;
    if( truc == 3 )
    {
       // while(1);
    }
    return( i_coding_type == I_CODING_TYPE || (i_coding_type == P_CODING_TYPE) && (meuh == 2));
    intf_DbgMsg("vpar debug: synchro image %i - modulo is %i\n", i_coding_type, p_vpar->synchro.modulo);
    intf_DbgMsg("vpar debug: synchro predict P %e - predict B %e\n", p_vpar->synchro.p_count_predict, p_vpar->synchro.b_count_predict);

    return(0);
    return( i_coding_type == I_CODING_TYPE );
}

/*****************************************************************************
 * vpar_SynchroTrash : Update timers when we trash a picture
 *****************************************************************************/
void vpar_SynchroTrash( vpar_thread_t * p_vpar, int i_coding_type,
                        int i_structure )
{
    //fprintf ( stderr, "trashing type %i\n", p_vpar->picture.i_coding_type );
    
    vpar_SynchroUpdateStructures (p_vpar, i_coding_type);

}

/*****************************************************************************
 * vpar_SynchroDecode : Update timers when we decide to decode a picture
 *****************************************************************************/
void vpar_SynchroDecode( vpar_thread_t * p_vpar, int i_coding_type,
                            int i_structure )
{
    //fprintf ( stderr, "decoding type %i\n", p_vpar->picture.i_coding_type );

    vpar_SynchroUpdateStructures (p_vpar, i_coding_type);

    p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_stop].decode_date = mdate();
    p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_stop].i_image_type
        = i_coding_type;
    p_vpar->synchro.i_fifo_stop = (p_vpar->synchro.i_fifo_stop + 1) & 0xf;

    fprintf ( stderr, "%i images in synchro fifo\n", ( p_vpar->synchro.i_fifo_stop - p_vpar->synchro.i_fifo_start ) & 0xf );
}

/*****************************************************************************
 * vpar_SynchroEnd : Called when the image is totally decoded
 *****************************************************************************/
void vpar_SynchroEnd( vpar_thread_t * p_vpar )
{
    mtime_t * p_decode_time;

    fprintf ( stderr, "type %i decoding time was %lli\n",
        p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_start].i_image_type,
	( mdate()
           - p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_start].decode_date )
           / (( p_vpar->synchro.i_fifo_stop - p_vpar->synchro.i_fifo_start ) & 0xf ));

    p_vpar->synchro.decode_time =
        ( (p_vpar->synchro.decode_time * 3) + (mdate()
            - p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_start].decode_date)
            / (( p_vpar->synchro.i_fifo_stop - p_vpar->synchro.i_fifo_start)
          & 0xf) ) >> 2;

    p_vpar->synchro.i_fifo_start = (p_vpar->synchro.i_fifo_start + 1) & 0xf;
}

/*****************************************************************************
 * vpar_SynchroDate : When an image has been decoded, ask for its date
 *****************************************************************************/
mtime_t vpar_SynchroDate( vpar_thread_t * p_vpar )
{
    decoder_fifo_t * fifo;
    mtime_t displaydate;

    fifo = p_vpar->bit_stream.p_decoder_fifo;
    displaydate = fifo->buffer[fifo->i_start]->i_pts;

    if (displaydate) fprintf(stderr, "displaying type %i at %lli, (time %lli, delta %lli)\n", p_vpar->synchro.fifo[p_vpar->synchro.i_fifo_start].i_image_type, displaydate, mdate() + 1000000, displaydate - mdate() - 1000000);
    return displaydate;
}

