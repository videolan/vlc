/*****************************************************************************
 * vdec_idct.c : IDCT functions
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
#include "video_parser.h"

/*
 * Local prototypes
 */

/* Our current implementation is a fast DCT, we might move to a fast DFT or
 * an MMX DCT in the future. */

/*****************************************************************************
 * vdec_DummyIDCT : dummy function that does nothing
 *****************************************************************************/
void vdec_DummyIDCT( elem_t * p_block, int i_idontcare )
{
}

/*****************************************************************************
 * vdec_SparseIDCT : IDCT function for sparse matrices
 *****************************************************************************/
void vdec_SparseIDCT( elem_t * p_block, int i_sparse_pos )
{
    /* Copy from mpeg_play */
}

/*****************************************************************************
 * vdec_IDCT : IDCT function for normal matrices
 *****************************************************************************/
void vdec_IDCT( elem_t * p_block, int i_idontcare )
{

}
