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
#include "vpar_synchro.h"
#include "video_parser.h"
#include "video_fifo.h"

/*****************************************************************************
 * vdec_MotionComponent : last stage of motion compensation
 *****************************************************************************/
static __inline__ void MotionComponent(
                    yuv_data_t * p_src,     /* source block */
                    yuv_data_t * p_dest,    /* dest block */
                    int i_width,            /* (explicit) width of block */
                    int i_height,           /* (explicit) height of block */
                    int i_stride,           /* number of coeffs to jump
                                             * between each predicted line */
                    int i_step,             /* number of coeffs to jump to
                                             * go to the next line of the
                                             * field */
                    int i_select,           /* half-pel vectors */
                    boolean_t b_average     /* (explicit) averaging of several
                                             * predictions */ )
{
    int i_x, i_y, i_x1, i_y1;
    unsigned int i_dummy;

    if( !b_average )
    {
        /* Please note that b_average will be expanded at compile time */

        switch( i_select )
        {
        case 0:
            /* !xh, !yh, !average */
            for( i_y = 0; i_y < i_height; i_y += 4 )
            {
                for( i_y1 = 0; i_y1 < 4; i_y1++ )
                {
                    for( i_x = 0; i_x < i_width; i_x += 8 )
                    {
                         for( i_x1 = 0; i_x1 < 8; i_x1++ )
                         {
                             p_dest[i_x+i_x1] = p_src[i_x+i_x1];
                         }
                    }
                    p_dest += i_stride;
                    p_src += i_stride;
                }
            }
            break;

        case 1:
            /* xh, !yh, !average */
            for( i_y = 0; i_y < i_height; i_y += 4 )
            {
                for( i_y1 = 0; i_y1 < 4; i_y1++ )
                {
                    for( i_x = 0; i_x < i_width; i_x += 8 )
                    {
                         for( i_x1 = 0; i_x1 < 8; i_x1++ )
                         {
                             p_dest[i_x+i_x1] = (unsigned int)(p_src[i_x+i_x1]
                                                      + p_src[i_x+i_x1 + 1] + 1)
                                                    >> 1;
                         }
                    }
                    p_dest += i_stride;
                    p_src += i_stride;
                }
            }
            break;

        case 2:
            /* !xh, yh, !average */
            for( i_y = 0; i_y < i_height; i_y += 4 )
            {
                for( i_y1 = 0; i_y1 < 4; i_y1++ )
                {
                    for( i_x = 0; i_x < i_width; i_x += 8 )
                    {
                         for( i_x1 = 0; i_x1 < 8; i_x1++ )
                         {
                             p_dest[i_x+i_x1] = (unsigned int)(p_src[i_x+i_x1] + 1
                                                + p_src[i_x+i_x1 + i_step])
                                              >> 1;  
                         }
                    }
                    p_dest += i_stride;
                    p_src += i_stride;
                }
            }
            break;

        case 3:
            /* xh, yh, !average (3) */
            for( i_y = 0; i_y < i_height; i_y += 4 )
            {
                for( i_y1 = 0; i_y1 < 4; i_y1++ )
                {
                    for( i_x = 0; i_x < i_width; i_x += 8 )
                    {
                         for( i_x1 = 0; i_x1 < 8; i_x1++ )
                         {
                             p_dest[i_x+i_x1]
                                = ((unsigned int)(
                                      p_src[i_x+i_x1]
                                    + p_src[i_x+i_x1 + 1] 
                                    + p_src[i_x+i_x1 + i_step]
                                    + p_src[i_x+i_x1 + i_step + 1]
                                    + 2) >> 2);
                         }
                    }
                    p_dest += i_stride;
                    p_src += i_stride;
                }
            }
            break;
        }

    }
    else
    {
        /* b_average */
        switch( i_select )
        {
        case 0:
            /* !xh, !yh, average */
            for( i_y = 0; i_y < i_height; i_y += 4 )
            {
                for( i_y1 = 0; i_y1 < 4; i_y1++ )
                {
                    for( i_x = 0; i_x < i_width; i_x += 8 )
                    {
                         for( i_x1 = 0; i_x1 < 8; i_x1++ )
                         {
                             i_dummy = p_dest[i_x + i_x1] + p_src[i_x + i_x1];
                             p_dest[i_x + i_x1] = (i_dummy + 1) >> 1;
                         }
                    }
                    p_dest += i_stride;
                    p_src += i_stride;
                }
            }
            break;

        case 1:
            /* xh, !yh, average */
            for( i_y = 0; i_y < i_height; i_y += 4 )
            {
                for( i_y1 = 0; i_y1 < 4; i_y1++ )
                {
                    for( i_x = 0; i_x < i_width; i_x += 8 )
                    {
                         for( i_x1 = 0; i_x1 < 8; i_x1++ )
                         {
                             i_dummy = p_dest[i_x+i_x1]
                                + ((unsigned int)(p_src[i_x+i_x1]
                                                  + p_src[i_x+i_x1 + 1] + 1) >> 1);
                             p_dest[i_x + i_x1] = (i_dummy + 1) >> 1;
                         }
                    }
                    p_dest += i_stride;
                    p_src += i_stride;
                }
            }
            break;

        case 2:
            /* !xh, yh, average */
            for( i_y = 0; i_y < i_height; i_y += 4 )
            {
                for( i_y1 = 0; i_y1 < 4; i_y1++ )
                {
                    for( i_x = 0; i_x < i_width; i_x += 8 )
                    {
                         for( i_x1 = 0; i_x1 < 8; i_x1++ )
                         {
                             i_dummy = p_dest[i_x+i_x1]
                                + ((unsigned int)(p_src[i_x+i_x1] + 1
                                         + p_src[i_x+i_x1 + i_step]) >> 1);
                             p_dest[i_x + i_x1] = (i_dummy + 1) >> 1;
                         }
                    }
                    p_dest += i_stride;
                    p_src += i_stride;
                }
            }
            break;

        case 3:
            /* xh, yh, average */
            for( i_y = 0; i_y < i_height; i_y += 4 )
            {
                for( i_y1 = 0; i_y1 < 4; i_y1++ )
                {
                    for( i_x = 0; i_x < i_width; i_x += 8 )
                    {
                         for( i_x1 = 0; i_x1 < 8; i_x1++ )
                         {
                             i_dummy = p_dest[i_x+i_x1]
                                + ((unsigned int)(
                                      p_src[i_x+i_x1]
                                    + p_src[i_x+i_x1 + 1]
                                    + p_src[i_x+i_x1 + i_step]
                                    + p_src[i_x+i_x1 + i_step + 1]
                                    + 2) >> 2);
                             p_dest[i_x + i_x1] = (i_dummy + 1) >> 1;
                         }
                    }
                    p_dest += i_stride;
                    p_src += i_stride;
                }
            }
            break;
        }
    }
}

/*****************************************************************************
 * Motion420 : motion compensation for a 4:2:0 macroblock
 *****************************************************************************/
static __inline__ void Motion420(
                    macroblock_t * p_mb,        /* destination macroblock */
                    picture_t * p_source,       /* source picture */
                    boolean_t b_source_field,   /* source field */
                    boolean_t b_dest_field,     /* destination field */
                    int i_mv_x, int i_mv_y,     /* motion vector coordinates,
                                                 * in half pels */
                    int i_l_stride,             /* number of coeffs to jump to
                                                 * go to the next predicted
                                                 * line */
                    int i_c_stride,
                    int i_height,               /* height of the block to
                                                 * predict, in luminance
                                                 * (explicit) */
                    int i_offset,               /* position of the first
                                                 * predicted line (explicit) */
                    boolean_t b_average         /* (explicit) averaging of
                                                 * several predictions */ )
{
    /* Temporary variables to avoid recalculating things twice */
    int     i_source_offset, i_dest_offset, i_c_height, i_c_select;

    /* Luminance */
    MotionComponent( /* source */
                     p_source->p_y
                       + (p_mb->i_l_x + (i_mv_x >> 1))
                       + (p_mb->i_motion_l_y + i_offset
                          + (i_mv_y >> 1)
                          + b_source_field)
                         * p_mb->p_picture->i_width,
                     /* destination */
                     p_mb->p_picture->p_y
                       + (p_mb->i_l_x)
                       + (p_mb->i_motion_l_y + b_dest_field)
                         * p_mb->p_picture->i_width,
                     /* prediction width and height */
                     16, i_height,
                     /* stride */
                     i_l_stride, p_mb->i_l_stride,
                     /* select */
                     ((i_mv_y & 1) << 1) | (i_mv_x & 1),
                     b_average );

    i_source_offset = (p_mb->i_c_x + ((i_mv_x/2) >> 1))
                        + ((p_mb->i_motion_c_y + (i_offset >> 1)
                           + ((i_mv_y/2) >> 1))
                           + b_source_field)
                          * p_mb->p_picture->i_chroma_width;
    i_dest_offset = (p_mb->i_c_x)
                      + (p_mb->i_motion_c_y + b_dest_field)
                        * p_mb->p_picture->i_chroma_width;
    i_c_height = i_height >> 1;
    i_c_select = (((i_mv_y/2) & 1) << 1) | ((i_mv_x/2) & 1);

    /* Chrominance Cr */
    MotionComponent( p_source->p_u
                       + i_source_offset,
                     p_mb->p_picture->p_u
                       + i_dest_offset,
                     8, i_c_height, i_c_stride, p_mb->i_c_stride,
                     i_c_select, b_average );

    /* Chrominance Cb */
    MotionComponent( p_source->p_v
                       + i_source_offset,
                     p_mb->p_picture->p_v
                       + i_dest_offset,
                     8, i_c_height, i_c_stride, p_mb->i_c_stride,
                     i_c_select, b_average );
}

/*****************************************************************************
 * Motion422 : motion compensation for a 4:2:2 macroblock
 *****************************************************************************/
static __inline__ void Motion422(
                    macroblock_t * p_mb,        /* destination macroblock */
                    picture_t * p_source,       /* source picture */
                    boolean_t b_source_field,   /* source field */
                    boolean_t b_dest_field,     /* destination field */
                    int i_mv_x, int i_mv_y,     /* motion vector coordinates,
                                                 * in half pels */
                    int i_l_stride,             /* number of coeffs to jump to
                                                 * go to the next predicted
                                                 * line */
                    int i_c_stride,
                    int i_height,               /* height of the block to
                                                 * predict, in luminance
                                                 * (explicit) */
                    int i_offset,               /* position of the first
                                                 * predicted line (explicit) */
                    boolean_t b_average         /* (explicit) averaging of
                                                 * several predictions */ )
{
    int     i_source_offset, i_dest_offset, i_c_select;

    /* Luminance */
    MotionComponent( /* source */
                     p_source->p_y
                       + (p_mb->i_l_x + (i_mv_x >> 1))
                       + (p_mb->i_motion_l_y + i_offset
                          + (i_mv_y >> 1)
                          + b_source_field)
                         * p_mb->p_picture->i_width,
                     /* destination */
                     p_mb->p_picture->p_y
                       + (p_mb->i_l_x)
                       + (p_mb->i_motion_l_y + b_dest_field)
                         * p_mb->p_picture->i_width,
                     /* prediction width and height */
                     16, i_height,
                     /* stride */
                     i_l_stride, p_mb->i_l_stride,
                     /* select */
                     ((i_mv_y & 1) << 1) | (i_mv_x & 1),
                     b_average );

    i_source_offset = (p_mb->i_c_x + ((i_mv_x/2) >> 1))
                        + ((p_mb->i_motion_c_y + (i_offset)
                           + ((i_mv_y) >> 1))
                           + b_source_field)
                          * p_mb->p_picture->i_chroma_width;
    i_dest_offset = (p_mb->i_c_x)
                      + (p_mb->i_motion_c_y + b_dest_field)
                        * p_mb->p_picture->i_chroma_width;
    i_c_select = ((i_mv_y & 1) << 1) | ((i_mv_x/2) & 1);

    /* Chrominance Cr */
    MotionComponent( p_source->p_u
                       + i_source_offset,
                     p_mb->p_picture->p_u
                       + i_dest_offset,
                     8, i_height, i_c_stride, p_mb->i_c_stride,
                     i_c_select, b_average );

    /* Chrominance Cb */
    MotionComponent( p_source->p_v
                       + i_source_offset,
                     p_mb->p_picture->p_u
                       + i_dest_offset,
                     8, i_height, i_c_stride, p_mb->i_c_stride,
                     i_c_select, b_average );
}

/*****************************************************************************
 * Motion444 : motion compensation for a 4:4:4 macroblock
 *****************************************************************************/
static __inline__ void Motion444(
                    macroblock_t * p_mb,        /* destination macroblock */
                    picture_t * p_source,       /* source picture */
                    boolean_t b_source_field,   /* source field */
                    boolean_t b_dest_field,     /* destination field */
                    int i_mv_x, int i_mv_y,     /* motion vector coordinates,
                                                 * in half pels */
                    int i_l_stride,             /* number of coeffs to jump to
                                                 * go to the next predicted
                                                 * line */
                    int i_c_stride,
                    int i_height,               /* height of the block to
                                                 * predict, in luminance
                                                 * (explicit) */
                    int i_offset,               /* position of the first
                                                 * predicted line (explicit) */
                    boolean_t b_average         /* (explicit) averaging of
                                                 * several predictions */ )
{
    int     i_source_offset, i_dest_offset, i_select;

    i_source_offset = (p_mb->i_l_x + (i_mv_x >> 1))
                        + (p_mb->i_motion_l_y + i_offset
                           + (i_mv_y >> 1)
                           + b_source_field)
                          * p_mb->p_picture->i_width;
    i_dest_offset = (p_mb->i_l_x)
                      + (p_mb->i_motion_l_y + b_dest_field)
                        * p_mb->p_picture->i_width;
    i_select = ((i_mv_y & 1) << 1) | (i_mv_x & 1);


    /* Luminance */
    MotionComponent( p_source->p_y
                       + i_source_offset,
                     p_mb->p_picture->p_y
                       + i_dest_offset,
                     16, i_height, i_l_stride, p_mb->i_l_stride,
                     i_select, b_average );

    /* Chrominance Cr */
    MotionComponent( p_source->p_u
                       + i_source_offset,
                     p_mb->p_picture->p_u
                       + i_dest_offset,
                     16, i_height, i_l_stride, p_mb->i_l_stride,
                     i_select, b_average );

    /* Chrominance Cb */
    MotionComponent( p_source->p_v
                       + i_source_offset,
                     p_mb->p_picture->p_v
                       + i_dest_offset,
                     16, i_height, i_l_stride, p_mb->i_l_stride,
                     i_select, b_average );
}

/*****************************************************************************
 * DualPrimeArithmetic : Dual Prime Additional arithmetic (7.6.3.6)
 *****************************************************************************/ 
static __inline__ void DualPrimeArithmetic( macroblock_t * p_mb,
                                            int ppi_dmv[2][2],
                                            int i_mv_x, int i_mv_y )
{
    if( p_mb->i_structure == FRAME_STRUCTURE )
    {
        if( p_mb->b_top_field_first )
        {
            /* vector for prediction of top field from bottom field */
            ppi_dmv[0][0] = ((i_mv_x + (i_mv_x > 0)) >> 1) + p_mb->pi_dm_vector[0];
            ppi_dmv[0][1] = ((i_mv_y + (i_mv_y > 0)) >> 1) + p_mb->pi_dm_vector[1] - 1;

            /* vector for prediction of bottom field from top field */
            ppi_dmv[1][0] = ((3*i_mv_x + (i_mv_x > 0)) >> 1) + p_mb->pi_dm_vector[0];
            ppi_dmv[1][1] = ((3*i_mv_y + (i_mv_y > 0)) >> 1) + p_mb->pi_dm_vector[1] + 1;
        }
        else
        {
            /* vector for prediction of top field from bottom field */
            ppi_dmv[0][0] = ((3*i_mv_x + (i_mv_x > 0)) >> 1) + p_mb->pi_dm_vector[0];
            ppi_dmv[0][1] = ((3*i_mv_y + (i_mv_y > 0)) >> 1) + p_mb->pi_dm_vector[1] - 1;

            /* vector for prediction of bottom field from top field */
            ppi_dmv[1][0] = ((i_mv_x + (i_mv_x > 0)) >> 1) + p_mb->pi_dm_vector[0];
            ppi_dmv[1][1] = ((i_mv_y + (i_mv_y > 0)) >> 1) + p_mb->pi_dm_vector[1] + 1;
        }
    }
    else
    {
        /* vector for prediction from field of opposite 'parity' */
        ppi_dmv[0][0] = ((i_mv_x + (i_mv_x > 0)) >> 1) + p_mb->pi_dm_vector[0];
        ppi_dmv[0][1] = ((i_mv_y + (i_mv_y > 0)) >> 1) + p_mb->pi_dm_vector[1];

        /* correct for vertical field shift */
        if( p_mb->i_structure == TOP_FIELD )
            ppi_dmv[0][1]--;
        else
            ppi_dmv[0][1]++;
    }
}


/*****************************************************************************
 * vdec_MotionDummy : motion compensation for an intra macroblock
 *****************************************************************************/
void vdec_MotionDummy( macroblock_t * p_mb )
{
    /* Nothing to do :) */
}

/*****************************************************************************
 * vdec_MotionFieldField : motion compensation for field motion type (field)
 *****************************************************************************/
#define FIELDFIELD( MOTION )                                            \
    picture_t *     p_pred;                                             \
                                                                        \
    if( p_mb->i_mb_type & MB_MOTION_FORWARD )                           \
    {                                                                   \
        if( p_mb->b_P_coding_type                                       \
             && (p_mb->i_current_structure == FRAME_STRUCTURE)          \
             && (p_mb->b_motion_field != p_mb->ppi_field_select[0][0]) )\
            p_pred = p_mb->p_picture;                                   \
        else                                                            \
            p_pred = p_mb->p_forward;                                   \
                                                                        \
        MOTION( p_mb, p_pred, p_mb->ppi_field_select[0][0],             \
                p_mb->b_motion_field,                                   \
                p_mb->pppi_motion_vectors[0][0][0],                     \
                p_mb->pppi_motion_vectors[0][0][1],                     \
                p_mb->i_l_stride, p_mb->i_c_stride, 16, 0, 0 );         \
                                                                        \
        if( p_mb->i_mb_type & MB_MOTION_BACKWARD )                      \
        {                                                               \
            MOTION( p_mb, p_mb->p_backward,                             \
                    p_mb->ppi_field_select[0][1],                       \
                    p_mb->b_motion_field,                               \
                    p_mb->pppi_motion_vectors[0][1][0],                 \
                    p_mb->pppi_motion_vectors[0][1][1],                 \
                    p_mb->i_l_stride, p_mb->i_c_stride, 16, 0, 1 );     \
    }                                                                   \
                                                                        \
    else /* MB_MOTION_BACKWARD */                                       \
    {                                                                   \
        MOTION( p_mb, p_mb->p_backward, p_mb->ppi_field_select[0][1],   \
                p_mb->b_motion_field,                                   \
                p_mb->pppi_motion_vectors[0][1][0],                     \
                p_mb->pppi_motion_vectors[0][1][1],                     \
                p_mb->i_l_stride, p_mb->i_c_stride, 16, 0, 0 );         \
    }                                                                   \
}

void vdec_MotionFieldField420( macroblock_t * p_mb )
{
    FIELDFIELD( Motion420 )
}

void vdec_MotionFieldField422( macroblock_t * p_mb )
{
    FIELDFIELD( Motion422 )
}

void vdec_MotionFieldField444( macroblock_t * p_mb )
{
    FIELDFIELD( Motion444 )
}

/*****************************************************************************
 * vdec_MotionField16x8XXX : motion compensation for 16x8 motion type (field)
 *****************************************************************************/
#define FIELD16X8( MOTION )                                             \
{                                                                       \
    picture_t *     p_pred;                                             \
                                                                        \
    if( p_mb->i_mb_type & MB_MOTION_FORWARD )                           \
    {                                                                   \
        if( p_mb->b_P_coding_type                                       \
             && (p_mb->i_current_structure == FRAME_STRUCTURE)          \
             && (p_mb->b_motion_field != p_mb->ppi_field_select[0][0]) )\
            p_pred = p_mb->p_picture;                                   \
        else                                                            \
            p_pred = p_mb->p_forward;                                   \
                                                                        \
        MOTION( p_mb, p_pred, p_mb->ppi_field_select[0][0],             \
                p_mb->b_motion_field,                                   \
                p_mb->pppi_motion_vectors[0][0][0],                     \
                p_mb->pppi_motion_vectors[0][0][1],                     \
                p_mb->i_l_stride, p_mb->i_c_stride, 8, 0, 0 );          \
                                                                        \
        if( p_mb->b_P_coding_type                                       \
             && (p_mb->i_current_structure == FRAME_STRUCTURE)          \
             && (p_mb->b_motion_field != p_mb->ppi_field_select[1][0]) )\
            p_pred = p_mb->p_picture;                                   \
        else                                                            \
            p_pred = p_mb->p_forward;                                   \
                                                                        \
        MOTION( p_mb, p_pred, p_mb->ppi_field_select[1][0],             \
                p_mb->b_motion_field,                                   \
                p_mb->pppi_motion_vectors[1][0][0],                     \
                p_mb->pppi_motion_vectors[1][0][1],                     \
                p_mb->i_l_stride, p_mb->i_c_stride, 8, 8, 0 );          \
                                                                        \
        if( p_mb->i_mb_type & MB_MOTION_BACKWARD )                      \
        {                                                               \
            MOTION( p_mb, p_mb->p_backward,                             \
                    p_mb->ppi_field_select[0][1],                       \
                    p_mb->b_motion_field,                               \
                    p_mb->pppi_motion_vectors[0][1][0],                 \
                    p_mb->pppi_motion_vectors[0][1][1],                 \
                    p_mb->i_l_stride, p_mb->i_c_stride, 8, 0, 1 );      \
                                                                        \
            MOTION( p_mb, p_mb->p_backward,                             \
                    p_mb->ppi_field_select[1][1],                       \
                    p_mb->b_motion_field,                               \
                    p_mb->pppi_motion_vectors[1][1][0],                 \
                    p_mb->pppi_motion_vectors[1][1][1],                 \
                    p_mb->i_l_stride, p_mb->i_c_stride, 8, 8, 1 );      \
        }                                                               \
    }                                                                   \
                                                                        \
    else /* MB_MOTION_BACKWARD */                                       \
    {                                                                   \
        MOTION( p_mb, p_mb->p_backward, p_mb->ppi_field_select[0][1],   \
                p_mb->b_motion_field,                                   \
                p_mb->pppi_motion_vectors[0][1][0],                     \
                p_mb->pppi_motion_vectors[0][1][1],                     \
                p_mb->i_l_stride, p_mb->i_c_stride, 8, 0, 0 );          \
                                                                        \
        MOTION( p_mb, p_mb->p_backward, p_mb->ppi_field_select[1][1],   \
                p_mb->b_motion_field,                                   \
                p_mb->pppi_motion_vectors[1][1][0],                     \
                p_mb->pppi_motion_vectors[1][1][1],                     \
                p_mb->i_l_stride, p_mb->i_c_stride, 8, 8, 0 );          \
    }                                                                   \
}

void vdec_MotionField16x8420( macroblock_t * p_mb )
{
    FIELD16X8( Motion420 )
}

void vdec_MotionField16x8422( macroblock_t * p_mb )
{
    FIELD16X8( Motion422 )
}

void vdec_MotionField16x8444( macroblock_t * p_mb )
{
    FIELD16X8( Motion444 )
}

/*****************************************************************************
 * vdec_MotionFieldDMV : motion compensation for dmv motion type (field)
 *****************************************************************************/
void vdec_MotionFieldDMV( macroblock_t * p_mb )
{
#if 0
    /* This is necessarily a MOTION_FORWARD only macroblock */
    motion_arg_t    args;
    picture_t *     p_pred;
    int             ppi_dmv[2][2];

    args.i_height = 16;
    args.b_average = 0;
    args.b_dest_field = p_mb->b_motion_field;
    args.i_offset = 0;

    if( p_mb->i_current_structure == FRAME_STRUCTURE )
        p_pred = p_mb->p_picture;
    else
        p_pred = p_mb->p_forward;

    DualPrimeArithmetic( p_mb, ppi_dmv, p_mb->pppi_motion_vectors[0][0][0],
                         p_mb->pppi_motion_vectors[0][0][1] );

    /* predict from field of same parity */
    args.p_source = p_mb->p_forward;
    args.b_source_field = p_mb->b_motion_field;
    args.i_mv_x = p_mb->pppi_motion_vectors[0][0][0];
    args.i_mv_y = p_mb->pppi_motion_vectors[0][0][1];
    p_mb->pf_chroma_motion( p_mb, &args );

    /* predict from field of opposite parity */
    args.b_average = 1;
    args.p_source = p_pred;
    args.b_source_field = !p_mb->b_motion_field;
    args.i_mv_x = ppi_dmv[0][0];
    args.i_mv_y = ppi_dmv[0][1];
    p_mb->pf_chroma_motion( p_mb, &args );
#endif
}

/*****************************************************************************
 * vdec_MotionFrameFrameXXX : motion compensation for frame motion type (frame)
 *****************************************************************************/
#define FRAMEFRAME( MOTION )                                            \
{                                                                       \
    if( p_mb->i_mb_type & MB_MOTION_FORWARD )                           \
    {                                                                   \
        MOTION( p_mb, p_mb->p_forward, 0, 0,                            \
                p_mb->pppi_motion_vectors[0][0][0],                     \
                p_mb->pppi_motion_vectors[0][0][1],                     \
                p_mb->i_l_stride, p_mb->i_c_stride, 16, 0, 0 );         \
                                                                        \
        if( p_mb->i_mb_type & MB_MOTION_BACKWARD )                      \
        {                                                               \
            MOTION( p_mb, p_mb->p_backward, 0, 0,                       \
                    p_mb->pppi_motion_vectors[0][1][0],                 \
                    p_mb->pppi_motion_vectors[0][1][1],                 \
                    p_mb->i_l_stride, p_mb->i_c_stride, 16, 0, 1 );     \
        }                                                               \
    }                                                                   \
                                                                        \
    else /* MB_MOTION_BACKWARD */                                       \
    {                                                                   \
        MOTION( p_mb, p_mb->p_backward, 0, 0,                           \
                p_mb->pppi_motion_vectors[0][1][0],                     \
                p_mb->pppi_motion_vectors[0][1][1],                     \
                p_mb->i_l_stride, p_mb->i_c_stride, 16, 0, 0 );         \
    }                                                                   \
} /* FRAMEFRAME */

void vdec_MotionFrameFrame420( macroblock_t * p_mb )
{
    FRAMEFRAME( Motion420 )
}

void vdec_MotionFrameFrame422( macroblock_t * p_mb )
{
    FRAMEFRAME( Motion422 )
}

void vdec_MotionFrameFrame444( macroblock_t * p_mb )
{
    FRAMEFRAME( Motion444 )
}

/*****************************************************************************
 * vdec_MotionFrameFieldXXX : motion compensation for field motion type (frame)
 *****************************************************************************/
#define FRAMEFIELD( MOTION )                                            \
{                                                                       \
    int i_l_stride = p_mb->i_l_stride << 1;                             \
    int i_c_stride = p_mb->i_c_stride << 1;                             \
                                                                        \
    if( p_mb->i_mb_type & MB_MOTION_FORWARD )                           \
    {                                                                   \
        MOTION( p_mb, p_mb->p_forward, p_mb->ppi_field_select[0][0], 0, \
                p_mb->pppi_motion_vectors[0][0][0],                     \
                p_mb->pppi_motion_vectors[0][0][1],                     \
                i_l_stride, i_c_stride, 8, 0, 0 );                      \
                                                                        \
        MOTION( p_mb, p_mb->p_forward, p_mb->ppi_field_select[1][0], 1, \
                p_mb->pppi_motion_vectors[1][0][0],                     \
                p_mb->pppi_motion_vectors[1][0][1],                     \
                i_l_stride, i_c_stride, 8, 0, 0 );                      \
                                                                        \
        if( p_mb->i_mb_type & MB_MOTION_BACKWARD )                      \
        {                                                               \
            MOTION( p_mb, p_mb->p_backward,                             \
                    p_mb->ppi_field_select[0][1], 0,                    \
                    p_mb->pppi_motion_vectors[0][1][0],                 \
                    p_mb->pppi_motion_vectors[0][1][1],                 \
                    i_l_stride, i_c_stride, 8, 0, 1 );                  \
                                                                        \
            MOTION( p_mb, p_mb->p_backward,                             \
                    p_mb->ppi_field_select[1][1], 1,                    \
                    p_mb->pppi_motion_vectors[1][1][0],                 \
                    p_mb->pppi_motion_vectors[1][1][1],                 \
                    i_l_stride, i_c_stride, 8, 0, 1 );                  \
        }                                                               \
    }                                                                   \
                                                                        \
    else /* MB_MOTION_BACKWARD only */                                  \
    {                                                                   \
        MOTION( p_mb, p_mb->p_backward, p_mb->ppi_field_select[0][1], 0,\
                p_mb->pppi_motion_vectors[0][1][0],                     \
                p_mb->pppi_motion_vectors[0][1][1],                     \
                i_l_stride, i_c_stride, 8, 0, 0 );                      \
                                                                        \
        MOTION( p_mb, p_mb->p_backward, p_mb->ppi_field_select[1][1], 1,\
                p_mb->pppi_motion_vectors[1][1][0],                     \
                p_mb->pppi_motion_vectors[1][1][1],                     \
                i_l_stride, i_c_stride, 8, 0, 0 );                      \
    }                                                                   \
} /* FRAMEFIELD */

void vdec_MotionFrameField420( macroblock_t * p_mb )
{
    FRAMEFIELD( Motion420 )
}

void vdec_MotionFrameField422( macroblock_t * p_mb )
{
    FRAMEFIELD( Motion422 )
}

void vdec_MotionFrameField444( macroblock_t * p_mb )
{
    FRAMEFIELD( Motion444 )
}

/*****************************************************************************
 * vdec_MotionFrameDMV : motion compensation for dmv motion type (frame)
 *****************************************************************************/
void vdec_MotionFrameDMV( macroblock_t * p_mb )
{
#if 0
    /* This is necessarily a MOTION_FORWARD only macroblock */
    motion_arg_t    args;
    int             ppi_dmv[2][2];

    args.i_l_x_step = p_mb->i_l_stride << 1;
    args.i_c_x_step = p_mb->i_c_stride << 1;
    args.i_height = 8;
    args.b_average = 0;
    args.b_dest_field = 0;
    args.i_offset = 0;
    args.p_source = p_mb->p_forward;

    DualPrimeArithmetic( p_mb, ppi_dmv, p_mb->pppi_motion_vectors[0][0][0],
                         p_mb->pppi_motion_vectors[0][0][1] );

    /* predict top field from top field */
    args.b_source_field = 0;
    args.i_mv_x = p_mb->pppi_motion_vectors[0][0][0];
    args.i_mv_y = p_mb->pppi_motion_vectors[0][0][1] >> 1;
    p_mb->pf_chroma_motion( p_mb, &args );

    /* predict and add to top field from bottom field */
    args.b_average = 1;
    args.b_source_field = 1;
    args.i_mv_x = ppi_dmv[0][0];
    args.i_mv_y = ppi_dmv[0][1];
    p_mb->pf_chroma_motion( p_mb, &args );

    /* predict bottom field from bottom field */
    args.b_average = 0;
    args.b_dest_field = 1;
    args.b_source_field = 0;
    args.i_mv_x = p_mb->pppi_motion_vectors[0][0][0];
    args.i_mv_y = p_mb->pppi_motion_vectors[0][0][1] >> 1;
    p_mb->pf_chroma_motion( p_mb, &args );

    /* predict and add to bottom field from top field */
    args.b_average = 1;
    args.b_source_field = 1;
    args.i_mv_x = ppi_dmv[1][0];
    args.i_mv_y = ppi_dmv[1][1];
    p_mb->pf_chroma_motion( p_mb, &args );
#endif
}

