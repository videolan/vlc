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
#include "video_parser.h"

#include "video_fifo.h"
#include "video_decoder.h"

/*
 * Local prototypes
 */

/*****************************************************************************
 * vpar_MPEG1MotionVector : Parse the next MPEG-1 motion vector
 *****************************************************************************/
void vpar_MPEG1MotionVector( vpar_thread_t * p_vpar, int i_mv )
{

}

/*****************************************************************************
 * vpar_MPEG2MotionVector : Parse the next MPEG-2 motion vector
 *****************************************************************************/
void vpar_MPEG2MotionVector( vpar_thread_t * p_vpar, int i_mv )
{

}
