/*****************************************************************************
 * video_decoder.c : video decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_decoder.c,v 1.60 2001/10/11 16:12:43 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Michel Lespinasse <walken@zoy.org>
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#include <stdlib.h>                                                /* free() */
#include <string.h>                                    /* memcpy(), memset() */
#include <errno.h>                                                  /* errno */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "video.h"
#include "video_output.h"

#include "vdec_ext-plugins.h"
#include "video_decoder.h"
#include "vpar_pool.h"
#include "video_parser.h"

/*
 * Local prototypes
 */
static void     RunThread           ( vdec_thread_t *p_vdec );

/*****************************************************************************
 * vdec_CreateThread: create a video decoder thread
 *****************************************************************************
 * This function creates a new video decoder thread, and returns a pointer
 * to its description. On error, it returns NULL.
 *****************************************************************************/
vdec_thread_t * vdec_CreateThread( vdec_pool_t * p_pool )
{
    vdec_thread_t *     p_vdec;

    intf_DbgMsg("vdec debug: creating video decoder thread");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_vdec = (vdec_thread_t *)malloc( sizeof(vdec_thread_t) )) == NULL )
    {
        intf_ErrMsg("vdec error: not enough memory for vdec_CreateThread() to create the new thread");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_vdec->b_die = 0;

    /*
     * Initialize the parser properties
     */
    p_vdec->p_pool = p_pool;

    /* Spawn the video decoder thread */
    if ( vlc_thread_create(&p_vdec->thread_id, "video decoder",
         (vlc_thread_func_t)RunThread, (void *)p_vdec) )
    {
        intf_ErrMsg("vdec error: can't spawn video decoder thread");
        free( p_vdec );
        return( NULL );
    }

    intf_DbgMsg("vdec debug: video decoder thread (%p) created", p_vdec);
    return( p_vdec );
}

/*****************************************************************************
 * vdec_DestroyThread: destroy a video decoder thread
 *****************************************************************************/
void vdec_DestroyThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: requesting termination of video decoder thread %p", p_vdec);

    /* Ask thread to kill itself */
    p_vdec->b_die = 1;

    /* Make sure the decoder thread leaves the vpar_GetMacroblock() function */
    vlc_mutex_lock( &p_vdec->p_pool->lock );
    vlc_cond_broadcast( &p_vdec->p_pool->wait_undecoded );
    vlc_mutex_unlock( &p_vdec->p_pool->lock );

    /* Waiting for the decoder thread to exit */
    vlc_thread_join( p_vdec->thread_id );
}

/* following functions are local */

/*****************************************************************************
 * vdec_InitThread: initialize video decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization.
 *****************************************************************************/
void vdec_InitThread( vdec_thread_t * p_vdec )
{
    intf_DbgMsg("vdec debug: initializing video decoder thread %p", p_vdec);

#if !defined(SYS_BEOS)
#   if VDEC_NICE
    /* Re-nice ourself - otherwise we would steal CPU time from the video
     * output, which would make a poor display. */
#       if !defined(WIN32)
    if( nice(VDEC_NICE) == -1 )
#       else
    if( !SetThreadPriority( GetCurrentThread(),
                            THREAD_PRIORITY_BELOW_NORMAL ) )
#       endif
    {
        intf_WarnMsg( 2, "vpar warning : couldn't nice() (%s)",
                      strerror(errno) );
    }
#   endif
#endif

    p_vdec->p_idct_data = NULL;

    p_vdec->p_pool->pf_idct_init( &p_vdec->p_idct_data );

    /* Mark thread as running and return */
    intf_DbgMsg("vdec debug: InitThread(%p) succeeded", p_vdec);
}

/*****************************************************************************
 * vdec_EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
void vdec_EndThread( vdec_thread_t * p_vdec )
{
    intf_DbgMsg("vdec debug: EndThread(%p)", p_vdec);

    if( p_vdec->p_idct_data != NULL )
    {
        free( p_vdec->p_idct_data );
    }

    free( p_vdec );
}

/*****************************************************************************
 * MotionBlock: does one component of the motion compensation
 *****************************************************************************/
static __inline__ void MotionBlock( vdec_pool_t * p_pool,
                                    boolean_t b_average,
                                    int i_x_pred, int i_y_pred,
                                    yuv_data_t * pp_dest[3], int i_dest_offset,
                                    yuv_data_t * pp_src[3], int i_src_offset,
                                    int i_stride, int i_height,
                                    boolean_t b_second_half,
                                    int i_chroma_format )
{
    int             i_xy_half;
    yuv_data_t *    p_src1;
    yuv_data_t *    p_src2;

    i_xy_half = ((i_y_pred & 1) << 1) | (i_x_pred & 1);

    p_src1 = pp_src[0] + i_src_offset
                + (i_x_pred >> 1) + (i_y_pred >> 1) * i_stride
                + b_second_half * (i_stride << 3);

    p_pool->ppppf_motion[b_average][0][i_xy_half]
            ( pp_dest[0] + i_dest_offset + b_second_half * (i_stride << 3),
              p_src1, i_stride, i_height );

    if( i_chroma_format != CHROMA_NONE )
    {
        /* Expanded at compile-time. */
        if( i_chroma_format != CHROMA_444 )
        {
            i_x_pred /= 2;
            i_stride >>= 1;
            i_src_offset >>= 1;
            i_dest_offset >>= 1;
        }
        if( i_chroma_format == CHROMA_420 )
        {
            i_y_pred /= 2;
            i_height >>= 1;
        }

        i_xy_half = ((i_y_pred & 1) << 1) | (i_x_pred & 1);

        i_src_offset += b_second_half * (i_stride << 3);
        i_dest_offset += b_second_half * (i_stride << 3);

        p_src1 = pp_src[1] + i_src_offset
                    + (i_x_pred >> 1) + (i_y_pred >> 1) * i_stride;
        p_src2 = pp_src[2] + i_src_offset
                    + (i_x_pred >> 1) + (i_y_pred >> 1) * i_stride;

        p_pool->ppppf_motion[b_average][(i_chroma_format != CHROMA_444)]
                            [i_xy_half]
                ( pp_dest[1] + i_dest_offset, p_src1, i_stride, i_height );
        p_pool->ppppf_motion[b_average][(i_chroma_format != CHROMA_444)]
                            [i_xy_half]
                ( pp_dest[2] + i_dest_offset, p_src2, i_stride, i_height );
    }
}


/*****************************************************************************
 * DecodeMacroblock: decode a macroblock
 *****************************************************************************/
#define DECODE_INTRA_BLOCK( i_b, p_dest, I_CHROMA )                         \
    p_idct = &p_mb->p_idcts[i_b];                                           \
    p_idct->pf_idct( p_idct->pi_block, p_dest,                              \
                     i_b < 4 ? i_lum_dct_stride :                           \
                         I_CHROMA == CHROMA_420 ?                           \
                         p_vpar->picture.i_field_width >> 1 :               \
                         i_chrom_dct_stride,                                \
                     p_vdec->p_idct_data, p_idct->i_sparse_pos ); 

#define DECODE_NONINTRA_BLOCK( i_b, p_dest, I_CHROMA )                      \
    if( p_mb->i_coded_block_pattern & (1 << (11 - (i_b))) )                 \
    {                                                                       \
        DECODE_INTRA_BLOCK( i_b, p_dest, I_CHROMA );                        \
    }
    
#define DECLARE_DECODEMB( PSZ_NAME, I_CHROMA )                              \
void PSZ_NAME ( vdec_thread_t *p_vdec, macroblock_t * p_mb )                \
{                                                                           \
    int             i, i_lum_dct_offset, i_lum_dct_stride;                  \
    /* This is to keep the compiler happy with CHROMA_420 and CHROMA_NONE */\
    int             i_chrom_dct_offset __attribute__((unused));             \
    int             i_chrom_dct_stride __attribute__((unused));             \
    idct_inner_t *  p_idct;                                                 \
    vdec_pool_t *   p_pool = p_vdec->p_pool;                                \
    vpar_thread_t * p_vpar = p_pool->p_vpar;                                \
                                                                            \
    if( p_mb->i_mb_modes & DCT_TYPE_INTERLACED )                            \
    {                                                                       \
        i_lum_dct_offset = p_vpar->picture.i_field_width;                   \
        i_lum_dct_stride = p_vpar->picture.i_field_width * 2;             \
        if( I_CHROMA == CHROMA_422 )                                        \
        {                                                                   \
            i_chrom_dct_offset = p_vpar->picture.i_field_width >> 1;        \
            i_chrom_dct_stride = p_vpar->picture.i_field_width;             \
        }                                                                   \
        else if( I_CHROMA == CHROMA_444 )                                   \
        {                                                                   \
            i_chrom_dct_offset = p_vpar->picture.i_field_width;             \
            i_chrom_dct_stride = p_vpar->picture.i_field_width * 2;         \
        }                                                                   \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        i_lum_dct_offset = p_vpar->picture.i_field_width * 8;               \
        i_lum_dct_stride = p_vpar->picture.i_field_width;                   \
        if( I_CHROMA == CHROMA_422 )                                        \
        {                                                                   \
            i_chrom_dct_offset = p_vpar->picture.i_field_width * 4;         \
            i_chrom_dct_stride = p_vpar->picture.i_field_width >> 1;        \
        }                                                                   \
        else if( I_CHROMA == CHROMA_444 )                                   \
        {                                                                   \
            i_chrom_dct_offset = p_vpar->picture.i_field_width * 8;         \
            i_chrom_dct_stride = p_vpar->picture.i_field_width;             \
        }                                                                   \
    }                                                                       \
                                                                            \
    if( !(p_mb->i_mb_modes & MB_INTRA) )                                    \
    {                                                                       \
        /*                                                                  \
         * Motion Compensation (ISO/IEC 13818-2 section 7.6)                \
         */                                                                 \
        for( i = 0; i < p_mb->i_nb_motions; i++ )                           \
        {                                                                   \
            motion_inner_t *    p_motion = &p_mb->p_motions[i];             \
            MotionBlock( p_pool, p_motion->b_average,                       \
                         p_motion->i_x_pred, p_motion->i_y_pred,            \
                         p_mb->pp_dest, p_motion->i_dest_offset,            \
                         p_motion->pp_source, p_motion->i_src_offset,       \
                         p_motion->i_stride, p_motion->i_height,            \
                         p_motion->b_second_half, I_CHROMA );               \
        }                                                                   \
                                                                            \
        /*                                                                  \
         * Inverse DCT (ISO/IEC 13818-2 section Annex A) and                \
         * adding prediction and coefficient data (ISO/IEC                  \
         * 13818-2 section 7.6.8)                                           \
         */                                                                 \
        DECODE_NONINTRA_BLOCK( 0, p_mb->p_y_data, I_CHROMA );               \
        DECODE_NONINTRA_BLOCK( 1, p_mb->p_y_data + 8, I_CHROMA );           \
        DECODE_NONINTRA_BLOCK( 2, p_mb->p_y_data + i_lum_dct_offset,        \
                               I_CHROMA );                                  \
        DECODE_NONINTRA_BLOCK( 3, p_mb->p_y_data + i_lum_dct_offset + 8,    \
                               I_CHROMA );                                  \
        if( I_CHROMA != CHROMA_NONE )                                       \
        {                                                                   \
            DECODE_NONINTRA_BLOCK( 4, p_mb->p_u_data, I_CHROMA );           \
            DECODE_NONINTRA_BLOCK( 5, p_mb->p_v_data, I_CHROMA );           \
            if( I_CHROMA != CHROMA_420 )                                    \
            {                                                               \
                DECODE_NONINTRA_BLOCK( 6, p_mb->p_u_data                    \
                                           + i_chrom_dct_offset, I_CHROMA );\
                DECODE_NONINTRA_BLOCK( 7, p_mb->p_v_data                    \
                                           + i_chrom_dct_offset, I_CHROMA );\
                if( I_CHROMA == CHROMA_444 )                                \
                {                                                           \
                    DECODE_NONINTRA_BLOCK( 8, p_mb->p_u_data + 8,           \
                                           I_CHROMA );                      \
                    DECODE_NONINTRA_BLOCK( 9, p_mb->p_v_data + 8,           \
                                           I_CHROMA );                      \
                    DECODE_NONINTRA_BLOCK( 10, p_mb->p_u_data + 8           \
                                           + i_chrom_dct_offset, I_CHROMA );\
                    DECODE_NONINTRA_BLOCK( 11, p_mb->p_v_data + 8           \
                                           + i_chrom_dct_offset, I_CHROMA );\
                }                                                           \
            }                                                               \
        }                                                                   \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        /* Intra macroblock */                                              \
        DECODE_INTRA_BLOCK( 0, p_mb->p_y_data, I_CHROMA );                  \
        DECODE_INTRA_BLOCK( 1, p_mb->p_y_data + 8, I_CHROMA );              \
        DECODE_INTRA_BLOCK( 2, p_mb->p_y_data + i_lum_dct_offset,           \
                            I_CHROMA );                                     \
        DECODE_INTRA_BLOCK( 3, p_mb->p_y_data + i_lum_dct_offset + 8,       \
                            I_CHROMA );                                     \
        if( I_CHROMA != CHROMA_NONE )                                       \
        {                                                                   \
            DECODE_INTRA_BLOCK( 4, p_mb->p_u_data, I_CHROMA );              \
            DECODE_INTRA_BLOCK( 5, p_mb->p_v_data, I_CHROMA );              \
            if( I_CHROMA != CHROMA_420 )                                    \
            {                                                               \
                DECODE_INTRA_BLOCK( 6, p_mb->p_u_data                       \
                                        + i_chrom_dct_offset, I_CHROMA );   \
                DECODE_INTRA_BLOCK( 7, p_mb->p_v_data                       \
                                        + i_chrom_dct_offset, I_CHROMA );   \
                if( I_CHROMA == CHROMA_444 )                                \
                {                                                           \
                    DECODE_INTRA_BLOCK( 8, p_mb->p_u_data + 8, I_CHROMA );  \
                    DECODE_INTRA_BLOCK( 9, p_mb->p_v_data + 8, I_CHROMA );  \
                    DECODE_INTRA_BLOCK( 10, p_mb->p_u_data + 8              \
                                           + i_chrom_dct_offset, I_CHROMA );\
                    DECODE_INTRA_BLOCK( 11, p_mb->p_v_data + 8              \
                                           + i_chrom_dct_offset, I_CHROMA );\
                }                                                           \
            }                                                               \
        }                                                                   \
    }                                                                       \
}

DECLARE_DECODEMB( vdec_DecodeMacroblockBW, CHROMA_NONE );
DECLARE_DECODEMB( vdec_DecodeMacroblock420, CHROMA_420 );
DECLARE_DECODEMB( vdec_DecodeMacroblock422, CHROMA_422 );
DECLARE_DECODEMB( vdec_DecodeMacroblock444, CHROMA_444 );

#undef DECLARE_DECODEMB

/*****************************************************************************
 * RunThread: video decoder thread
 *****************************************************************************
 * Video decoder thread. This function does only return when the thread is
 * terminated.
 *****************************************************************************/
static void RunThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: running video decoder thread (%p) (pid == %i)",
                p_vdec, getpid());

    vdec_InitThread( p_vdec );

    /*
     * Main loop
     */
    while( !p_vdec->b_die )
    {
        macroblock_t *          p_mb;

        if( (p_mb = vpar_GetMacroblock( p_vdec->p_pool, &p_vdec->b_die )) != NULL )
        {
            p_vdec->p_pool->pf_vdec_decode( p_vdec, p_mb );

            /* Decoding is finished, release the macroblock and free
             * unneeded memory. */
            p_vdec->p_pool->pf_free_mb( p_vdec->p_pool, p_mb );
        }
    }

    /* End of thread */
    vdec_EndThread( p_vdec );
}

