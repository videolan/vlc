/*****************************************************************************
 * vdec_motion.c : motion compensation routines
 * (c)1999 VideoLAN
 *****************************************************************************/

/* ?? passer en terminate/destroy avec les signaux supplémentaires */

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

#include "undec_picture.h"
#include "video_fifo.h"
#include "video_decoder.h"

/*
 * Local prototypes
 */

/*****************************************************************************
 * vdec_MotionCompensation : motion compensation
 *****************************************************************************/
void vdec_MotionFrame( vdec_thread_t * p_vdec,
                       undec_picture_t * p_undec_p, int i_mb,
                       f_motion_mb_t pf_mb_motion )
{
    static int      p_chroma_nb_blocks[4] = {1, 2, 4};
    static int      p_chroma_nb_elems[4] = {0, 64, 128, 256};

    int i_mb_x, i_mb_y;   /* Position of our macroblock in the final picture */
    elem_t *    p_y, p_u, p_v;             /* Pointers to our picture's data */

#define P_mb_info p_undec_p->p_mb_info[i_mb]
    
    i_mb_x = (i_mb << 5) % p_undec_p->p_picture->i_width;
    i_mb_y = (i_mb << 5) / p_undec_p->p_picture->i_width;
    p_y = &p_undec_p->p_picture->p_y[256*i_mb];
    p_u = &p_undec_p->p_picture->p_u[p_chroma_nb_elems[p_undec_p->p_picture->i_chroma_type]*i_mb];
    p_v = &p_undec_p->p_picture->p_v[p_chroma_nb_elems[p_undec_p->p_picture->i_chroma_type]*i_mb];
    
    if( (p_undec_p->i_coding_type == P_CODING_TYPE) ||
        (P_mb_info->i_mb_type & MB_MOTION_FORWARD) )
    {
        if( (P_mb_info->i_motion_type == MOTION_FRAME) ||
            !(P_mb_info->i_mb_type & MB_INTRA) )
        {
            MotionBlock( p_undec_p->p_forward->p_u,
                         p_undec_p->p_forward->p_lookup_lum,
                         p_undec_p->p_picture->i_width,
                         p_u, i_mb_x, i_mb_y,
                         p_undec_p->p_picture->i_width,
                         p_undec_p->ppp_motion_vectors[0][0][0],
                         p_undec_p->ppp_motion_vectors[0][0][1] );
        }
    }
}

/*****************************************************************************
 * MotionMacroblock : motion compensation for a macroblock
 *****************************************************************************/
void vdec_MotionMacroblock420( coeff_t * p_src, pel_lookup_table_t * p_lookup,
                               int i_width_line,
                               coeff_t * p_dest, int i_dest_x, i_dest_y,
                               int i_stride_line,
                               i_mv1_x, i_mv1_y, i_mv2_x, i_mv2_y )
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
    static (void *)     ComponentMode( coeff_t * p_src,
                             pel_lookup_table_t * p_lookup,
                             coeff_t * p_dest, int i_dest_x, i_dest_y,
                             int i_stride_line, i_mv_x, i_mv_y )[4]
                                = { ComponentNN, ComponentNH, ComponentHN,
                                    ComponentHH };

    int i_mode;

    i_mode = (i_mv_x & 1) | ((i_mv_y & 1) << 1);

    ComponentMode[i_mode]( p_src, p_lookup, i_width_line,
                           p_dest, i_dest_x, i_dest_y,
                           i_stride_line, i_mv_x >> 1, i_mv_y >> 1 );
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
    
    i_src_loc = (i_dest_y + i_mv_y)*i_width_line + i_dest_x + i_mv_x;
    
    for( i_vpos = 0; i_vpos < 8; i_vpos++ )
    {
        for( i_hpos = 0; i_hpos < 8; i_hpos++ )
        {
            p_dest[i_hpos] += p_src[p_lookup->pi_pel[i_src_loc + i_hpos]];
        }
        
        p_dest += 8;
        i_src_loc += i_stride_line;
    }
}
