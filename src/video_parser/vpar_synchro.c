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
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

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
	
    tab->mean = ( tab->mean + 3 * count ) / 4;
    tab->deviation = ( tab->deviation + 3 * abs (tab->mean - count) ) / 4;

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
            optimal_deviation = vpar_SynchroUpdateTab(
                            &p_vpar->synchro.tab_p[0],
                            p_vpar->synchro.current_p_count);
            predict = p_vpar->synchro.tab_p[0].mean;

            candidate_deviation = vpar_SynchroUpdateTab(
                            &p_vpar->synchro.tab_p[1 + (p_vpar->synchro.modulo & 0x1)],
                            p_vpar->synchro.current_p_count);
            if (candidate_deviation < optimal_deviation)
	    {
                optimal_deviation = candidate_deviation;
                predict = p_vpar->synchro.tab_p[1 + (p_vpar->synchro.modulo & 0x1)].mean;
            }

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
            optimal_deviation = vpar_SynchroUpdateTab(
                            &p_vpar->synchro.tab_b[0],
                            p_vpar->synchro.current_b_count);
            predict = p_vpar->synchro.tab_b[0].mean;

            candidate_deviation = vpar_SynchroUpdateTab(
                            &p_vpar->synchro.tab_b[1 + (p_vpar->synchro.modulo & 0x1)],
                            p_vpar->synchro.current_b_count);
            if (candidate_deviation < optimal_deviation)
	    {
                optimal_deviation = candidate_deviation;
                predict = p_vpar->synchro.tab_b[1 + (p_vpar->synchro.modulo & 0x1)].mean;
            }

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
//    return( 1 );
    return( i_coding_type == I_CODING_TYPE || i_coding_type == P_CODING_TYPE );
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
    vpar_SynchroUpdateStructures (p_vpar, i_coding_type);

}

/*****************************************************************************
 * vpar_SynchroDecode : Update timers when we decide to decode a picture
 *****************************************************************************/
void vpar_SynchroDecode( vpar_thread_t * p_vpar, int i_coding_type,
                            int i_structure )
{
    vpar_SynchroUpdateStructures (p_vpar, i_coding_type);

}

/*****************************************************************************
 * vpar_SynchroEnd : Called when the image is totally decoded
 *****************************************************************************/
mtime_t vpar_SynchroEnd( vpar_thread_t * p_vpar )
{
    
    return mdate() + 700000;
}

