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

/*
 * Local prototypes
 */

/*****************************************************************************
 * vpar_SynchroChoose : Decide whether we will decode a picture or not
 *****************************************************************************/
boolean_t vpar_SynchroChoose( vpar_thread_t * p_vpar, int i_coding_type,
                              int i_structure )
{
    return( i_coding_type == I_CODING_TYPE );
}

/*****************************************************************************
 * vpar_SynchroTrash : Update timers when we trash a picture
 *****************************************************************************/
void vpar_SynchroTrash( vpar_thread_t * p_vpar, int i_coding_type,
                        int i_structure )
{

}

/*****************************************************************************
 * vpar_SynchroDecode : Update timers when we decide to decode a picture
 *****************************************************************************/
mtime_t vpar_SynchroDecode( vpar_thread_t * p_vpar, int i_coding_type,
                            int i_structure )
{
    return mdate() + 500000;
}

/*****************************************************************************
 * vpar_SynchroEnd : Called when the image is totally decoded
 *****************************************************************************/
void vpar_SynchroEnd( vpar_thread_t * p_vpar )
{

}



