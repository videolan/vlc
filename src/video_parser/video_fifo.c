/*****************************************************************************
 * video_fifo.c : video FIFO management
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_fifo.c,v 1.25 2001/01/05 18:46:45 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "video.h"
#include "video_output.h"

#include "../video_decoder/vdec_idct.h"
#include "../video_decoder/video_decoder.h"
#include "../video_decoder/vdec_motion.h"

#include "../video_decoder/vpar_blocks.h"
#include "../video_decoder/vpar_headers.h"
#include "../video_decoder/vpar_synchro.h"
#include "../video_decoder/video_parser.h"
#include "../video_decoder/video_fifo.h"

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
