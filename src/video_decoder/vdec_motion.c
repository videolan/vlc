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
#include "vpar_synchro.h"
#include "video_parser.h"

/*
 * Local prototypes
 */

typedef struct motion_arg_s
{
    picture_t *     p_source;
    boolean_t       b_source_field;
    boolean_t       b_dest_field;
    int             i_height;
    int             i_x_step;
    int             i_mv_x, i_mv_y;
    boolean_t       b_average;
} motion_arg_t;

/*
typedef void         (*f_motion_c_t)( coeff_t *, pel_lookup_table_t *,
                                      int, coeff_t *, int, int,
                                      int, int, int, int, int );
*/

/*****************************************************************************
 * vdec_MotionComponent : last stage of motion compensation
 *****************************************************************************/
static __inline__ vdec_MotionComponent()
{

}

/*****************************************************************************
 * vdec_DummyRecon : motion compensation for an intra macroblock
 *****************************************************************************/
void vdec_DummyRecon( macroblock_t * p_mb )
{
    /* Nothing to do :) */
}

/*****************************************************************************
 * vdec_FieldFieldRecon : motion compensation for field motion type (field)
 *****************************************************************************/
void vdec_FieldFieldRecon( macroblock_t * p_mb )
{
#if 0
    motion_arg_t motion_arg;

    motion_arg
    
    if( p_mb->i_mb_type & MB_MOTION_FORWARD )
    {
        int i_current_field;
        picture_t * p_pred_frame;
        
        i_current_field = ( p_mb->i_structure == BOTTOM_FIELD );
        if( p_mb->b_P_coding_type && (p_mb->i_current_structure == FRAME_STRUCTURE)
            && (i_current_field != p_mb->ppi_field_select[0][0]) )
        {
            p_pred_frame = p_mb->p_forward;
        }
        else
        {
            p_pred_frame = p_mb->p_backward;
        }
        p_mb->pf_chroma_motion( p_mb, p_pred_frame, 0 /* average */ );
    }
#endif
}

/*****************************************************************************
 * vdec_Field16x8Recon : motion compensation for 16x8 motion type (field)
 *****************************************************************************/
void vdec_Field16x8Recon( macroblock_t * p_mb )
{

}

/*****************************************************************************
 * vdec_FieldDMVRecon : motion compensation for dmv motion type (field)
 *****************************************************************************/
void vdec_FieldDMVRecon( macroblock_t * p_mb )
{
    /* This is necessarily a MOTION_FORWARD only macroblock */
    
}

/*****************************************************************************
 * vdec_FrameFrameRecon : motion compensation for frame motion type (frame)
 *****************************************************************************/
void vdec_FrameFrameRecon( macroblock_t * p_mb )
{

}

/*****************************************************************************
 * vdec_FrameFieldRecon : motion compensation for field motion type (frame)
 *****************************************************************************/
void vdec_FrameFieldRecon( macroblock_t * p_mb )
{

}

/*****************************************************************************
 * vdec_FrameDMVRecon : motion compensation for dmv motion type (frame)
 *****************************************************************************/
void vdec_FrameDMVRecon( macroblock_t * p_mb )
{
    /* This is necessarily a MOTION_FORWARD only macroblock */
    
}

/*****************************************************************************
 * vdec_MotionField : motion compensation for skipped macroblocks (field)
 *****************************************************************************/
void vdec_MotionField( macroblock_t * p_mb )
{

}

/*****************************************************************************
 * vdec_MotionFrame : motion compensation for skipped macroblocks (frame)
 *****************************************************************************/
void vdec_MotionFrame( macroblock_t * p_mb )
{

}

/*****************************************************************************
 * vdec_Motion420 : motion compensation for a 4:2:0 macroblock
 *****************************************************************************/
void vdec_Motion420( macroblock_t * p_mb )
{
}

/*****************************************************************************
 * vdec_Motion422 : motion compensation for a 4:2:2 macroblock
 *****************************************************************************/
void vdec_Motion422( macroblock_t * p_mb )
{
}

/*****************************************************************************
 * vdec_Motion444 : motion compensation for a 4:4:4 macroblock
 *****************************************************************************/
void vdec_Motion444( macroblock_t * p_mb )
{
}

#if 0

/*****************************************************************************
 * vdec_MotionMacroblock420 : motion compensation for a 4:2:0 macroblock
 *****************************************************************************/
void vdec_MotionMacroblock420( macroblock_t * p_mb )
{
    /* Luminance */
    /*
    MotionBlock( p_undec_p->p_forward->p_u, p_undec_p->p_forward->p_lookup_lum,
                 p_undec_p->p_picture->i_width, p_u, i_mb_x, i_mb_y,
                 p_undec_p->p_picture->i_width,
                 p_undec_p->ppp_motion_vectors[0][0][0],
                 p_undec_p->ppp_motion_vectors[0][0][1] );
    */
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

#endif
