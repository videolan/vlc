/*****************************************************************************
 * vdec_motion_common.c : common motion compensation routines common
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vdec_motion_common.c,v 1.11 2001/12/09 17:01:36 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Marc Dressler <polux@via.ecp.fr>
 *          Michel Lespinasse <walken@via.ecp.fr>
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

/* MODULE_NAME defined in Makefile together with -DBUILTIN */
#ifdef BUILTIN
#   include "modules_inner.h"
#else
#   define _M( foo ) foo
#endif

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "video.h"

#include "vdec_ext-plugins.h"

#include "modules.h"
#include "modules_export.h"

extern int _M( motion_Probe )( probedata_t *p_data );

static void vdec_MotionFieldField420  ( macroblock_t * p_mb );
static void vdec_MotionFieldField422  ( macroblock_t * p_mb );
static void vdec_MotionFieldField444  ( macroblock_t * p_mb );
static void vdec_MotionField16x8420   ( macroblock_t * p_mb );
static void vdec_MotionField16x8422   ( macroblock_t * p_mb );
static void vdec_MotionField16x8444   ( macroblock_t * p_mb );
static void vdec_MotionFieldDMV420    ( macroblock_t * p_mb );
static void vdec_MotionFieldDMV422    ( macroblock_t * p_mb );
static void vdec_MotionFieldDMV444    ( macroblock_t * p_mb );
static void vdec_MotionFrameFrame420  ( macroblock_t * p_mb );
static void vdec_MotionFrameFrame422  ( macroblock_t * p_mb );
static void vdec_MotionFrameFrame444  ( macroblock_t * p_mb );
static void vdec_MotionFrameField420  ( macroblock_t * p_mb );
static void vdec_MotionFrameField422  ( macroblock_t * p_mb );
static void vdec_MotionFrameField444  ( macroblock_t * p_mb );
static void vdec_MotionFrameDMV420    ( macroblock_t * p_mb );
static void vdec_MotionFrameDMV422    ( macroblock_t * p_mb );
static void vdec_MotionFrameDMV444    ( macroblock_t * p_mb );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( motion_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = _M( motion_Probe );

#define list p_function_list->functions.motion
#define motion_functions( yuv ) \
    list.pf_field_field_##yuv = vdec_MotionFieldField##yuv; \
    list.pf_field_16x8_##yuv  = vdec_MotionField16x8##yuv;  \
    list.pf_field_dmv_##yuv   = vdec_MotionFieldDMV##yuv;   \
    list.pf_frame_field_##yuv = vdec_MotionFrameField##yuv; \
    list.pf_frame_frame_##yuv = vdec_MotionFrameFrame##yuv; \
    list.pf_frame_dmv_##yuv   = vdec_MotionFrameDMV##yuv;
    motion_functions( 420 )
    motion_functions( 422 )
    motion_functions( 444 )
#undef motion_functions
#undef list

    return;
}

#define __MotionComponents(width,height)                \
void _M( MotionComponent_x_y_copy_##width##_##height )();    \
void _M( MotionComponent_X_y_copy_##width##_##height )();    \
void _M( MotionComponent_x_Y_copy_##width##_##height )();    \
void _M( MotionComponent_X_Y_copy_##width##_##height )();    \
void _M( MotionComponent_x_y_avg_##width##_##height )();     \
void _M( MotionComponent_X_y_avg_##width##_##height )();     \
void _M( MotionComponent_x_Y_avg_##width##_##height )();     \
void _M( MotionComponent_X_Y_avg_##width##_##height )();

__MotionComponents (16,16)       /* 444, 422, 420 */
__MotionComponents (16,8)        /* 444, 422, 420 */
__MotionComponents (8,8)         /* 422, 420 */
__MotionComponents (8,4)         /* 420 */
#if 0
__MotionComponents (8,16)        /* 422 */
#endif

#define ___callTheRightOne(width,height)                                     \
    if ((i_width == width) && (i_height == height))                          \
    {                                                                        \
        if (!b_average)                                                      \
        {                                                                    \
            switch (i_select)                                                \
            {                                                                \
            case 0:                                                          \
                _M( MotionComponent_x_y_copy_##width##_##height )(p_src,     \
                                                          p_dest, i_stride); \
                break;                                                       \
            case 1:                                                          \
                _M( MotionComponent_X_y_copy_##width##_##height )(p_src,     \
                                                          p_dest, i_stride); \
                break;                                                       \
            case 2:                                                          \
                _M( MotionComponent_x_Y_copy_##width##_##height )(p_src,     \
                                                          p_dest, i_stride); \
                break;                                                       \
            case 3:                                                          \
                _M( MotionComponent_X_Y_copy_##width##_##height )(p_src,     \
                                                          p_dest, i_stride); \
                break;                                                       \
            }                                                                \
        }                                                                    \
        else                                                                 \
        {                                                                    \
            switch (i_select)                                                \
            {                                                                \
            case 0:                                                          \
                _M( MotionComponent_x_y_avg_##width##_##height )(p_src,      \
                                                          p_dest, i_stride); \
                break;                                                       \
            case 1:                                                          \
                _M( MotionComponent_X_y_avg_##width##_##height )(p_src,      \
                                                          p_dest, i_stride); \
                break;                                                       \
            case 2:                                                          \
                _M( MotionComponent_x_Y_avg_##width##_##height )(p_src,      \
                                                          p_dest, i_stride); \
                break;                                                       \
            case 3:                                                          \
                _M( MotionComponent_X_Y_avg_##width##_##height )(p_src,      \
                                                          p_dest, i_stride); \
                break;                                                       \
            }                                                                \
        }                                                                    \
    }

/*****************************************************************************
 * vdec_MotionComponent : last stage of motion compensation
 *****************************************************************************/
static __inline__ void MotionComponent(
                    pixel_data_t * p_src,   /* source block */
                    pixel_data_t * p_dest,  /* dest block */
                    int i_width,            /* (explicit) width of block */
                    int i_height,           /* (explicit) height of block */
                    int i_stride,           /* number of coeffs to jump
                                             * between each predicted line */
                    int i_select,           /* half-pel vectors */
                    boolean_t b_average     /* (explicit) averaging of several
                                             * predictions */ )
{
    ___callTheRightOne (16,16)
    ___callTheRightOne (16,8)
    ___callTheRightOne (8,8)
    ___callTheRightOne (8,4)
#if 0
    ___callTheRightOne (8,16)
#endif
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

    i_source_offset = (p_mb->i_l_x + (i_mv_x >> 1))
                       + (p_mb->i_motion_l_y + i_offset
                         + b_source_field)
                       * p_mb->p_picture->i_width
                       + (i_mv_y >> 1) * i_l_stride;

    if( i_source_offset >= p_source->i_size )
    {
        intf_WarnMsg( 2, "Bad motion vector (lum)" );
        return;
    }

    /* Luminance */
    MotionComponent( /* source */
                     p_source->planes[ Y_PLANE ].p_data
                       + i_source_offset,
                     /* destination */
                     p_mb->p_picture->planes[ Y_PLANE ].p_data
                       + (p_mb->i_l_x)
                       + (p_mb->i_motion_l_y + b_dest_field + i_offset)
                         * p_mb->p_picture->i_width,
                     /* prediction width and height */
                     16, i_height,
                     /* stride */
                     i_l_stride,
                     /* select */
                     ((i_mv_y & 1) << 1) | (i_mv_x & 1),
                     b_average );

    i_source_offset = (p_mb->i_c_x + ((i_mv_x/2) >> 1))
                        + (p_mb->i_motion_c_y + (i_offset >> 1)
                           + b_source_field)
                          * p_mb->p_picture->i_chroma_width
                        + ((i_mv_y/2) >> 1) * i_c_stride;

    if( i_source_offset >= p_source->i_chroma_size )
    {
        intf_WarnMsg( 2, "Bad motion vector (chroma)" );
        return;
    }

    i_dest_offset = (p_mb->i_c_x)
                      + (p_mb->i_motion_c_y + b_dest_field
                          + (i_offset >> 1))
                        * p_mb->p_picture->i_chroma_width;
    i_c_height = i_height >> 1;
    i_c_select = (((i_mv_y/2) & 1) << 1) | ((i_mv_x/2) & 1);

    /* Chrominance Cr */
    MotionComponent( p_source->planes[ U_PLANE ].p_data
                       + i_source_offset,
                     p_mb->p_picture->planes[ U_PLANE ].p_data
                       + i_dest_offset,
                     8, i_c_height, i_c_stride,
                     i_c_select, b_average );

    /* Chrominance Cb */
    MotionComponent( p_source->planes[ V_PLANE ].p_data
                       + i_source_offset,
                     p_mb->p_picture->planes[ V_PLANE ].p_data
                       + i_dest_offset,
                     8, i_c_height, i_c_stride,
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
#if 0
    int     i_source_offset, i_dest_offset, i_c_select;

    /* Luminance */
    MotionComponent( /* source */
                     p_source->planes[ Y_PLANE ].p_data
                       + (p_mb->i_l_x + (i_mv_x >> 1))
                       + (p_mb->i_motion_l_y + i_offset
                          + b_source_field)
                       * p_mb->p_picture->i_width
                       + (i_mv_y >> 1) * p_mb->i_l_stride,
                     /* destination */
                     p_mb->p_picture->planes[ Y_PLANE ].p_data
                       + (p_mb->i_l_x)
                       + (p_mb->i_motion_l_y + b_dest_field)
                         * p_mb->p_picture->i_width,
                     /* prediction width and height */
                     16, i_height,
                     /* stride */
                     i_l_stride,
                     /* select */
                     ((i_mv_y & 1) << 1) | (i_mv_x & 1),
                     b_average );

    i_source_offset = (p_mb->i_c_x + ((i_mv_x/2) >> 1))
                        + (p_mb->i_motion_c_y + i_offset
                           + b_source_field)
                        * p_mb->p_picture->i_chroma_width
                        + (i_mv_y) >> 1) * p_mb->i_c_stride;
    i_dest_offset = (p_mb->i_c_x)
                      + (p_mb->i_motion_c_y + b_dest_field)
                        * p_mb->p_picture->i_chroma_width;
    i_c_select = ((i_mv_y & 1) << 1) | ((i_mv_x/2) & 1);

    /* Chrominance Cr */
    MotionComponent( p_source->planes[ U_PLANE ].p_data
                       + i_source_offset,
                     p_mb->p_picture->planes[ U_PLANE ].p_data
                       + i_dest_offset,
                     8, i_height, i_c_stride,
                     i_c_select, b_average );

    /* Chrominance Cb */
    MotionComponent( p_source->planes[ V_PLANE ].p_data
                       + i_source_offset,
                     p_mb->p_picture->planes[ U_PLANE ].p_data
                       + i_dest_offset,
                     8, i_height, i_c_stride,
                     i_c_select, b_average );
#endif
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
#if 0
    int     i_source_offset, i_dest_offset, i_select;

    i_source_offset = (p_mb->i_l_x + (i_mv_x >> 1))
                        + (p_mb->i_motion_l_y + i_offset
                           + b_source_field)
                        * p_mb->p_picture->i_width
                        + (i_mv_y >> 1) * p_mb->i_l_stride;
    i_dest_offset = (p_mb->i_l_x)
                      + (p_mb->i_motion_l_y + b_dest_field)
                        * p_mb->p_picture->i_width;
    i_select = ((i_mv_y & 1) << 1) | (i_mv_x & 1);


    /* Luminance */
    MotionComponent( p_source->planes[ Y_PLANE ].p_data
                       + i_source_offset,
                     p_mb->p_picture->planes[ Y_PLANE ].p_data
                       + i_dest_offset,
                     16, i_height, i_l_stride,
                     i_select, b_average );

    /* Chrominance Cr */
    MotionComponent( p_source->planes[ U_PLANE ].p_data
                       + i_source_offset,
                     p_mb->p_picture->planes[ U_PLANE ].p_data
                       + i_dest_offset,
                     16, i_height, i_l_stride,
                     i_select, b_average );

    /* Chrominance Cb */
    MotionComponent( p_source->planes[ V_PLANE ].p_data
                       + i_source_offset,
                     p_mb->p_picture->planes[ V_PLANE ].p_data
                       + i_dest_offset,
                     16, i_height, i_l_stride,
                     i_select, b_average );
#endif
}

/*****************************************************************************
 * vdec_MotionFieldField : motion compensation for field motion type (field)
 *****************************************************************************/
#define FIELDFIELD( MOTION )                                            \
{                                                                       \
    picture_t *     p_pred;                                             \
                                                                        \
    if( p_mb->i_mb_type & MB_MOTION_FORWARD )                           \
    {                                                                   \
        if( p_mb->b_P_second                                            \
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
        }                                                               \
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

static void vdec_MotionFieldField420( macroblock_t * p_mb )
{
    FIELDFIELD( Motion420 )
}

static void vdec_MotionFieldField422( macroblock_t * p_mb )
{
    //FIELDFIELD( Motion422 )
}

static void vdec_MotionFieldField444( macroblock_t * p_mb )
{
    //FIELDFIELD( Motion444 )
}

/*****************************************************************************
 * vdec_MotionField16x8XXX: motion compensation for 16x8 motion type (field)
 *****************************************************************************/
#define FIELD16X8( MOTION )                                             \
{                                                                       \
    picture_t *     p_pred;                                             \
                                                                        \
    if( p_mb->i_mb_type & MB_MOTION_FORWARD )                           \
    {                                                                   \
        if( p_mb->b_P_second                                            \
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
        if( p_mb->b_P_second                                            \
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

static void vdec_MotionField16x8420( macroblock_t * p_mb )
{
    FIELD16X8( Motion420 )
}

static void vdec_MotionField16x8422( macroblock_t * p_mb )
{
    //FIELD16X8( Motion422 )
}

static void vdec_MotionField16x8444( macroblock_t * p_mb )
{
    //FIELD16X8( Motion444 )
}

/*****************************************************************************
 * vdec_MotionFieldDMVXXX : motion compensation for dmv motion type (field)
 *****************************************************************************/
#define FIELDDMV( MOTION )                                              \
{                                                                       \
    /* This is necessarily a MOTION_FORWARD only macroblock, in a P     \
     * picture. */                                                      \
    picture_t *     p_pred;                                             \
                                                                        \
    /* predict from field of same parity */                             \
    MOTION( p_mb, p_mb->p_forward,                                      \
            p_mb->b_motion_field, p_mb->b_motion_field,                 \
            p_mb->pppi_motion_vectors[0][0][0],                         \
            p_mb->pppi_motion_vectors[0][0][1],                         \
            p_mb->i_l_stride, p_mb->i_c_stride, 16, 0, 0 );             \
                                                                        \
    if( p_mb->b_P_second )                                              \
        p_pred = p_mb->p_picture;                                       \
    else                                                                \
        p_pred = p_mb->p_forward;                                       \
                                                                        \
    /* predict from field of opposite parity */                         \
    MOTION( p_mb, p_pred, !p_mb->b_motion_field, p_mb->b_motion_field,  \
            p_mb->ppi_dmv[0][0], p_mb->ppi_dmv[0][1],                   \
            p_mb->i_l_stride, p_mb->i_c_stride, 16, 0, 1 );             \
} /* FIELDDMV */

static void vdec_MotionFieldDMV420( macroblock_t * p_mb )
{
    FIELDDMV( Motion420 )
}

static void vdec_MotionFieldDMV422( macroblock_t * p_mb )
{
    //FIELDDMV( Motion422 )
}

static void vdec_MotionFieldDMV444( macroblock_t * p_mb )
{
    //FIELDDMV( Motion444 )
}

/*****************************************************************************
 * vdec_MotionFrameFrameXXX?? : motion compensation for frame motion type (frame)
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

static void vdec_MotionFrameFrame420( macroblock_t * p_mb )
{
    FRAMEFRAME( Motion420 )
}

static void vdec_MotionFrameFrame422( macroblock_t * p_mb )
{
    //FRAMEFRAME( Motion422 )
}

static void vdec_MotionFrameFrame444( macroblock_t * p_mb )
{
    //FRAMEFRAME( Motion444 )
}

/*****************************************************************************
 * vdec_MotionFrameFieldXXX?? : motion compensation for field motion type (frame)
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
                p_mb->pppi_motion_vectors[0][0][1] >> 1,                \
                i_l_stride, i_c_stride, 8, 0, 0 );                      \
                                                                        \
        MOTION( p_mb, p_mb->p_forward, p_mb->ppi_field_select[1][0], 1, \
                p_mb->pppi_motion_vectors[1][0][0],                     \
                p_mb->pppi_motion_vectors[1][0][1] >> 1,                \
                i_l_stride, i_c_stride, 8, 0, 0 );                      \
                                                                        \
        if( p_mb->i_mb_type & MB_MOTION_BACKWARD )                      \
        {                                                               \
            MOTION( p_mb, p_mb->p_backward,                             \
                    p_mb->ppi_field_select[0][1], 0,                    \
                    p_mb->pppi_motion_vectors[0][1][0],                 \
                    p_mb->pppi_motion_vectors[0][1][1] >> 1,            \
                    i_l_stride, i_c_stride, 8, 0, 1 );                  \
                                                                        \
            MOTION( p_mb, p_mb->p_backward,                             \
                    p_mb->ppi_field_select[1][1], 1,                    \
                    p_mb->pppi_motion_vectors[1][1][0],                 \
                    p_mb->pppi_motion_vectors[1][1][1] >> 1,            \
                    i_l_stride, i_c_stride, 8, 0, 1 );                  \
        }                                                               \
    }                                                                   \
                                                                        \
    else /* MB_MOTION_BACKWARD only */                                  \
    {                                                                   \
        MOTION( p_mb, p_mb->p_backward, p_mb->ppi_field_select[0][1], 0,\
                p_mb->pppi_motion_vectors[0][1][0],                     \
                p_mb->pppi_motion_vectors[0][1][1] >> 1,                \
                i_l_stride, i_c_stride, 8, 0, 0 );                      \
                                                                        \
        MOTION( p_mb, p_mb->p_backward, p_mb->ppi_field_select[1][1], 1,\
                p_mb->pppi_motion_vectors[1][1][0],                     \
                p_mb->pppi_motion_vectors[1][1][1] >> 1,                \
                i_l_stride, i_c_stride, 8, 0, 0 );                      \
    }                                                                   \
} /* FRAMEFIELD */

static void vdec_MotionFrameField420( macroblock_t * p_mb )
{
    FRAMEFIELD( Motion420 )
}

static void vdec_MotionFrameField422( macroblock_t * p_mb )
{
    //FRAMEFIELD( Motion422 )
}

static void vdec_MotionFrameField444( macroblock_t * p_mb )
{
    //FRAMEFIELD( Motion444 )
}

/*****************************************************************************
 * vdec_MotionFrameDMVXXX?? : motion compensation for dmv motion type (frame)
 *****************************************************************************/
#define FRAMEDMV( MOTION )                                              \
{                                                                       \
    /* This is necessarily a MOTION_FORWARD only macroblock, in a P     \
     * picture. */                                                      \
                                                                        \
    /* predict top field from top field */                              \
    MOTION( p_mb, p_mb->p_forward, 0, 0,                                \
            p_mb->pppi_motion_vectors[0][0][0],                         \
            p_mb->pppi_motion_vectors[0][0][1],                         \
            /* XXX?? XXX?? >> 1 ? */                                    \
            p_mb->i_l_stride << 1, p_mb->i_c_stride << 1, 8, 0, 0 );    \
                                                                        \
    /* predict and add to top field from bottom field */                \
    MOTION( p_mb, p_mb->p_forward, 1, 0,                                \
            p_mb->ppi_dmv[0][0], p_mb->ppi_dmv[0][1],                   \
            p_mb->i_l_stride << 1, p_mb->i_c_stride << 1, 8, 0, 1 );    \
                                                                        \
    /* predict bottom field from bottom field */                        \
    MOTION( p_mb, p_mb->p_forward, 1, 1,                                \
            p_mb->pppi_motion_vectors[0][0][0],                         \
            p_mb->pppi_motion_vectors[0][0][1],                         \
            /* XXX?? XXX?? >> 1 ? */                                    \
            p_mb->i_l_stride << 1, p_mb->i_c_stride << 1, 8, 0, 0 );    \
                                                                        \
    /* predict and add to bottom field from top field */                \
    MOTION( p_mb, p_mb->p_forward, 1, 0,                                \
            p_mb->ppi_dmv[1][0], p_mb->ppi_dmv[1][1],                   \
            p_mb->i_l_stride << 1, p_mb->i_c_stride << 1, 8, 0, 1 );    \
} /* FRAMEDMV */

static void vdec_MotionFrameDMV420( macroblock_t * p_mb )
{
    FRAMEDMV( Motion420 )
}

static void vdec_MotionFrameDMV422( macroblock_t * p_mb )
{
    //FRAMEDMV( Motion422 )
}

static void vdec_MotionFrameDMV444( macroblock_t * p_mb )
{
    //FRAMEDMV( Motion444 )
}

