/*******************************************************************************
 * video_fifo.c : video FIFO management
 * (c)1999 VideoLAN
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
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
#include "debug.h"                      /* ?? temporaire, requis par netlist.h */

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

/*****************************************************************************
 * vpar_InitFIFO : initialize the video FIFO
 *****************************************************************************/
void vpar_InitFIFO( vpar_thread_t * p_vpar )
{
#ifdef VDEC_SMP
    int                 i_dummy;
#endif

    p_vpar->vfifo.p_vpar = p_vpar;

#ifdef VDEC_SMP
    /* Initialize mutex and cond */
    vlc_mutex_init( &p_vpar->vfifo.lock );
    vlc_cond_init( &p_vpar->vfifo.wait );
    vlc_mutex_init( &p_vpar->vbuffer.lock );
    
    /* Initialize FIFO properties */
    p_vpar->vfifo.i_start = p_vpar->vfifo.i_end = 0;
    
    /* Initialize buffer properties */
    p_vpar->vbuffer.i_index = VFIFO_SIZE; /* all structures are available */
    for( i_dummy = 0; i_dummy < VFIFO_SIZE + 1; i_dummy++ )
    {
        p_vpar->vbuffer.pp_mb_free[i_dummy] = p_vpar->vbuffer.p_macroblocks
                                               + i_dummy;
    }
#endif
}
