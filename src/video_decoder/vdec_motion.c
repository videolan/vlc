/*****************************************************************************
 * vdec_motion.c : motion compensation routines
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

typedef void         (*f_motion_c_t)( coeff_t *, pel_lookup_table_t *,
                                      int, coeff_t *, int, int,
                                      int, int, int, int, int );

/*****************************************************************************
 * vdec_DummyRecon : motion compensation for an intra macroblock
 *****************************************************************************/
void vdec_DummyRecon( macroblock_t * p_mb )
{
}

/*****************************************************************************
 * vdec_ForwardRecon : motion compensation for a forward predicted macroblock
 *****************************************************************************/
void vdec_ForwardRecon( macroblock_t * p_mb )
{

}

/*****************************************************************************
 * vdec_BackwardRecon : motion compensation for a backward predicted macroblock
 *****************************************************************************/
void vdec_BackwardRecon( macroblock_t * p_mb )
{

}

/*****************************************************************************
 * vdec_BidirectionalRecon : motion compensation for a bidirectionally
 *                           predicted macroblock
 *****************************************************************************/
void vdec_BidirectionalRecon( macroblock_t * p_mb )
{

}

/*****************************************************************************
 * vdec_MotionMacroblock420 : motion compensation for a 4:2:0 macroblock
 *****************************************************************************/
void vdec_MotionMacroblock420( macroblock_t * p_mb )
{
    /* Luminance */
    MotionBlock( p_undec_p->p_forward->p_u, p_undec_p->p_forward->p_lookup_lum,
                 p_undec_p->p_picture->i_width, p_u, i_mb_x, i_mb_y,
                 p_undec_p->p_picture->i_width,
                 p_undec_p->ppp_motion_vectors[0][0][0],
                 p_undec_p->ppp_motion_vectors[0][0][1] );

}

/*****************************************************************************
 * MotionBlock : motion compensation for one 8x8 block
 *****************************************************************************/
void __inline__ MotionBlock( coeff_t * p_src, pel_lookup_table_t * p_lookup,
                             int i_width_line,
                             coeff_t * p_dest, int i_dest_x, i_dest_y,
                             int i_stride_line,
                             i_mv1_x, i_mv1_y, i_mv2_x, i_mv2_y )
{
    static f_motion_c_t     ComponentMode[4]
                                = { &ComponentNN, &ComponentNH, &ComponentHN,
                                    &ComponentHH };

    int i_mode;

    i_mode = (i_mv_x & 1) | ((i_mv_y & 1) << 1);

    ComponentMode[i_mode]( p_src, p_lookup, i_width_line,
                           p_dest, i_dest_x, i_dest_y,
                           i_stride_line, i_mv1_x >> 1, i_mv1_y >> 1,
                           i_mv2_x >> 1, i_mv2_y >> 1 );
}

/*****************************************************************************
 * ComponentNN : motion compensation without half pel
 *****************************************************************************/
void ComponentNN( coeff_t * p_src, pel_lookup_table_t * p_lookup,
                  int i_width_line,
                  coeff_t * p_dest, int i_dest_x, i_dest_y,
                  int i_stride_line, i_mv1_x, i_mv1_y, i_mv2_x, i_mv2_y )
{
    int             i_vpos;
    register int    i_hpos, i_src_loc;
    
    i_src_loc = (i_dest_y + i_mv1_y)*i_width_line + i_dest_x + i_mv1_x;
    
    for( i_vpos = 0; i_vpos < 4; i_vpos++ )
    {
        for( i_hpos = 0; i_hpos < 8; i_hpos++ )
        {
            p_dest[i_hpos] += p_src[p_lookup->pi_pel[i_src_loc + i_hpos]];
        }
        
        p_dest += 8;
        i_src_loc += i_stride_line;
    }

    i_src_loc = (i_dest_y + i_mv2_y)*i_width_line + i_dest_x + i_mv2_x;

    for( i_vpos = 4; i_vpos < 8; i_vpos++ )
    {
        for( i_hpos = 0; i_hpos < 8; i_hpos++ )
        {
            p_dest[i_hpos] += p_src[p_lookup->pi_pel[i_src_loc + i_hpos]];
        }
        
        p_dest += 8;
        i_src_loc += i_stride_line;
    }
}
