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
#include "vpar_motion.h"

/* mv_format */
#define MV_FIELD 0
#define MV_FRAME 1

/*
 * Local prototypes
 */

/****************************************************************************
 * vpar_MotionCode : Parse the next motion code
 ****************************************************************************/
static __inline__ int vpar_MotionCode( vpar_thread_t * p_vpar )
{
    int i_code;
   static mv_tab_t p_mv_tab0[8] = 
        { {-1,0}, {3,3}, {2,2}, {2,2}, {1,1}, {1,1}, {1,1}, {1,1} };
    /* Table B-10, motion_code, codes 0000011 ... 000011x */
    static mv_tab_t p_mv_tab1[8] =
        { {-1,0}, {-1,0}, {-1,0}, {7,6}, {6,6}, {5,6}, {4,5}, {4,5} };
    /* Table B-10, motion_code, codes 0000001100 ... 000001011x */
    static mv_tab_t p_mv_tab2[12] = {
        {16,9}, {15,9}, {14,9}, {13,9},
        {12,9}, {11,9}, {10,8}, {10,8},
        {9,8},  {9,8},  {8,8},  {8,8} };
    
   if( GetBits(&p_vpar->bit_stream, 1) )
   {
       return 0;
   }

   if( (i_code = ShowBits(&p_vpar->bit_stream, 9)) >= 64 )
   {
       i_code >>= 6;
       DumpBits( &p_vpar->bit_stream, p_mv_tab0[0].i_len );
       return( GetBits(&p_vpar->bit_stream, 1) ? -p_mv_tab0[i_code].i_val : p_mv_tab0[i_code].i_val );
   }

   if( i_code >= 24 )
   {
       i_code >>= 3;
       DumpBits( &p_vpar->bit_stream, p_mv_tab1[0].i_len );
       return( GetBits(&p_vpar->bit_stream, 1) ? -p_mv_tab1[i_code].i_val : p_mv_tab1[i_code].i_val );
   }

   if( (i_code -= 12) < 0 )
   {
       p_vpar->picture.b_error = 1;
       intf_DbgMsg( "vpar debug: Invalid motion_vector code\n" );
       return 0;
   }

   DumpBits( &p_vpar->bit_stream, p_mv_tab2[0].i_len );
   return( GetBits(&p_vpar->bit_stream, 1) ? -p_mv_tab2[i_code].i_val : p_mv_tab2[i_code].i_val );            
}

/****************************************************************************
 * vpar_DecodeMotionVector : decode a motion_vector
 ****************************************************************************/
static __inline__ void vpar_DecodeMotionVector( int * pi_prediction, int i_r_size,
        int i_motion_code, int i_motion_residual, int i_full_pel )
{
    int i_limit, i_vector;

    /* ISO/IEC 13818-1 section 7.6.3.1 */
    i_limit = 16 << i_r_size;
    i_vector = *pi_prediction >> i_full_pel;

    if( i_motion_code < 0 )
    {
        i_vector += ((i_motion_code-1) << i_r_size) + i_motion_residual + 1;
        if( i_vector >= i_limit )
            i_vector -= i_limit << 1;
    }
    else if( i_motion_code > 0 )
    {
        i_vector -= ((-i_motion_code-1) << i_r_size) + i_motion_residual + 1;
        if( i_vector < i_limit )
            i_vector += i_limit << 1;
    }

    *pi_prediction = i_vector << i_full_pel;
}

/****************************************************************************
 * vpar_MotionVector : Parse the next motion_vector field
 ****************************************************************************/
void vpar_MotionVector( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_r,
        int i_s, int i_full_pel )
{
        
    int i_motion_code, i_motion_residual;
    int i_r_size;

    i_r_size = p_vpar->picture.ppi_f_code[i_s][0]-1;
    i_motion_code = vpar_MotionCode( p_vpar );
    i_motion_residual = (i_r_size != 0 && i_motion_code != 0) ?
                        GetBits( &p_vpar->bit_stream, i_r_size) : 0;
    vpar_DecodeMotionVector( &p_vpar->slice.pppi_pmv[i_r][i_s][0], i_r_size,
                             i_motion_code, i_motion_residual, i_full_pel );
    p_mb->pppi_motion_vectors[i_r][i_s][0] = p_vpar->slice.pppi_pmv[i_r][i_s][0];

    if( p_vpar->mb.b_dmv )
    {
        if( GetBits(&p_vpar->bit_stream, 1) )
        {
            p_mb->pi_dm_vector[0] = GetBits( &p_vpar->bit_stream, 1 ) ? -1 : 1;
        }
        else
        {
            p_mb->pi_dm_vector[0] = 0;
        }
    }
    
    i_r_size = p_vpar->picture.ppi_f_code[i_s][1]-1;
    i_motion_code = vpar_MotionCode( p_vpar );
    i_motion_residual = (i_r_size != 0 && i_motion_code != 0) ?
                        GetBits( &p_vpar->bit_stream, i_r_size) : 0;
    vpar_DecodeMotionVector( &p_vpar->slice.pppi_pmv[i_r][i_s][1], i_r_size,
                             i_motion_code, i_motion_residual, i_full_pel );
    p_mb->pppi_motion_vectors[i_r][i_s][1] = p_vpar->slice.pppi_pmv[i_r][i_s][1];

    if( p_vpar->mb.b_dmv )
    {
        if( GetBits(&p_vpar->bit_stream, 1) )
        {
            p_mb->pi_dm_vector[1] = GetBits( &p_vpar->bit_stream, 1 ) ? -1 : 1;
        }
        else
        {
            p_mb->pi_dm_vector[1] = 0;
        }
    }
}

/*****************************************************************************
 * vpar_MPEG1MotionVector : Parse the next MPEG-1 motion vectors
 *****************************************************************************/
void vpar_MPEG1MotionVector( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_s )
{
    vpar_MotionVector( p_vpar, p_mb, 0, i_s, p_vpar->picture.pb_full_pel_vector[i_s] );
}

/*****************************************************************************
 * vpar_MPEG2MotionVector : Parse the next MPEG-2 motion_vectors field
 *****************************************************************************/
void vpar_MPEG2MotionVector( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_s )
{
    if( p_vpar->mb.i_mv_count == 1 )
    {
        if( p_vpar->mb.i_mv_format == MV_FIELD && !p_vpar->mb.b_dmv )
        {
            p_mb->ppi_field_select[0][i_s] = p_mb->ppi_field_select[1][i_s]
                                            = GetBits( &p_vpar->bit_stream, 1 );
        }
        vpar_MotionVector( p_vpar, p_mb, 0, i_s, 0 );
    }
    else
    {
        p_mb->ppi_field_select[0][i_s] = GetBits( &p_vpar->bit_stream, 1 );
        vpar_MotionVector( p_vpar, p_mb, 0, i_s, 0 );
        p_mb->ppi_field_select[1][i_s] = GetBits( &p_vpar->bit_stream, 1 );
        vpar_MotionVector( p_vpar, p_mb, 1, i_s, 0 );
    }
}
