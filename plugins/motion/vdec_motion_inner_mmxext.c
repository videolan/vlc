/*****************************************************************************
 * vdec_motion_inner_mmxext.c : motion compensation inner routines optimized
 *                              in MMX EXT
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vdec_motion_inner_mmxext.c,v 1.2 2001/06/07 15:27:44 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>, largerly inspired by the
 *          work done by the livid project <http://www.linuxvideo.org/>
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

#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "video.h"

#include "attributes.h"
#include "mmx.h"

/* OK, I know, this code has been taken from livid's mpeg2dec --Meuuh */

static mmx_t mask_one = {0x0101010101010101LL};

/*
 * Useful functions
 */

#define pavg_r2r(src,dest)      pavgb_r2r (src, dest);
#define pavg_m2r(src,dest)      pavgb_m2r (src, dest);

#define __MotionComponent_x_y_copy(width,height)                            \
void _M(MotionComponent_x_y_copy_##width##_##height)(yuv_data_t * p_src,    \
                                                 yuv_data_t * p_dest,       \
                                                 int i_stride)              \
{                                                                           \
    int i_y;                                                                \
                                                                            \
        pxor_r2r (mm0, mm0);                                                \
        pxor_r2r (mm1, mm1);                                                \
        pxor_r2r (mm2, mm2);                                                \
        pxor_r2r (mm3, mm3);                                                \
        pxor_r2r (mm4, mm4);                                                \
        pxor_r2r (mm5, mm5);                                                \
        pxor_r2r (mm6, mm6);                                                \
        pxor_r2r (mm7, mm7);                                                \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        movq_m2r( *p_src, mm0 );     /* load 8 ref bytes */                 \
        if( width == 16 )                                                   \
            movq_m2r( *(p_src + 8), mm1 );                                  \
        p_src += i_stride;                                                  \
                                                                            \
        movq_r2m( mm0, *p_dest );    /* store 8 bytes at curr */            \
        if( width == 16 )                                                   \
            movq_r2m( mm1, *(p_dest + 8) );                                 \
        p_dest += i_stride;                                                 \
    }                                                                       \
}

#define __MotionComponent_X_y_copy(width,height)                            \
void _M(MotionComponent_X_y_copy_##width##_##height)(yuv_data_t * p_src,    \
                                                 yuv_data_t * p_dest,       \
                                                 int i_stride)              \
{                                                                           \
    int i_y;                                                                \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        movq_m2r (*p_src, mm0);                                             \
        if( width == 16 )                                                   \
            movq_m2r (*(p_src + 8), mm1);                                   \
        pavg_m2r (*(p_src + 1), mm0);                                       \
        if( width == 16 )                                                   \
            pavg_m2r (*(p_src + 9), mm1);                                   \
        movq_r2m (mm0, *p_dest);                                            \
        p_src += i_stride;                                                  \
        if( width == 16 )                                                   \
            movq_r2m (mm1, *(p_dest + 8));                                  \
        p_dest += i_stride;                                                 \
    }                                                                       \
}

#define __MotionComponent_x_Y_copy(width,height)                            \
void _M(MotionComponent_x_Y_copy_##width##_##height)(yuv_data_t * p_src,    \
                                                 yuv_data_t * p_dest,       \
                                                 int i_stride)              \
{                                                                           \
    int i_y;                                                                \
    yuv_data_t * p_next_src = p_src + i_stride;                             \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        movq_m2r (*p_src, mm0);                                             \
        if( width == 16 )                                                   \
            movq_m2r (*(p_src + 8), mm1);                                   \
        pavg_m2r (*(p_next_src), mm0);                                      \
        if( width == 16 )                                                   \
            pavg_m2r (*(p_next_src + 8), mm1);                              \
        movq_r2m (mm0, *p_dest);                                            \
        p_src += i_stride;                                                  \
        p_next_src += i_stride;                                             \
        if( width == 16 )                                                   \
            movq_r2m (mm1, *(p_dest + 8));                                  \
        p_dest += i_stride;                                                 \
    }                                                                       \
}

#define __MotionComponent_X_Y_copy(width,height)                            \
void _M(MotionComponent_X_Y_copy_##width##_##height)(yuv_data_t * p_src,    \
                                                 yuv_data_t * p_dest,       \
                                                 int i_stride)              \
{                                                                           \
    int i_y;                                                                \
                                                                            \
    if( width == 16 )                                                       \
    {                                                                       \
        for( i_y = 0; i_y < height; i_y ++ )                                \
        {                                                                   \
            movq_m2r (*p_src, mm0);                                         \
            movq_m2r (*(p_src+i_stride+1), mm1);                            \
            movq_r2r (mm0, mm7);                                            \
            movq_m2r (*(p_src+1), mm2);                                     \
            pxor_r2r (mm1, mm7);                                            \
            movq_m2r (*(p_src + i_stride), mm3);                            \
            movq_r2r (mm2, mm6);                                            \
            pxor_r2r (mm3, mm6);                                            \
            pavg_r2r (mm1, mm0);                                            \
            pavg_r2r (mm3, mm2);                                            \
            por_r2r (mm6, mm7);                                             \
            movq_r2r (mm0, mm6);                                            \
            pxor_r2r (mm2, mm6);                                            \
            pand_r2r (mm6, mm7);                                            \
            pand_m2r (mask_one, mm7);                                       \
            pavg_r2r (mm2, mm0);                                            \
            psubusb_r2r (mm7, mm0);                                         \
            movq_r2m (mm0, *p_dest);                                        \
                                                                            \
            movq_m2r (*(p_src+8), mm0);                                     \
            movq_m2r (*(p_src+i_stride+9), mm1);                            \
            movq_r2r (mm0, mm7);                                            \
            movq_m2r (*(p_src+9), mm2);                                     \
            pxor_r2r (mm1, mm7);                                            \
            movq_m2r (*(p_src+i_stride+8), mm3);                            \
            movq_r2r (mm2, mm6);                                            \
            pxor_r2r (mm3, mm6);                                            \
            pavg_r2r (mm1, mm0);                                            \
            pavg_r2r (mm3, mm2);                                            \
            por_r2r (mm6, mm7);                                             \
            movq_r2r (mm0, mm6);                                            \
            pxor_r2r (mm2, mm6);                                            \
            pand_r2r (mm6, mm7);                                            \
            pand_m2r (mask_one, mm7);                                       \
            pavg_r2r (mm2, mm0);                                            \
            psubusb_r2r (mm7, mm0);                                         \
            p_src += i_stride;                                              \
            movq_r2m (mm0, *(p_dest+8));                                    \
            p_dest += i_stride;                                             \
        }                                                                   \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        movq_m2r (*p_src, mm0);                                             \
        movq_m2r (*(p_src+1), mm1);                                         \
        movq_r2r (mm0, mm7);                                                \
        pxor_r2r (mm1, mm7);                                                \
        pavg_r2r (mm1, mm0);                                                \
        p_src += i_stride;                                                  \
                                                                            \
        for( i_y = 0; i_y < height; i_y ++ )                                \
        {                                                                   \
            movq_m2r (*p_src, mm2);                                         \
            movq_r2r (mm0, mm5);                                            \
            movq_m2r (*(p_src+1), mm3);                                     \
            movq_r2r (mm2, mm6);                                            \
            pxor_r2r (mm3, mm6);                                            \
            pavg_r2r (mm3, mm2);                                            \
            por_r2r (mm6, mm7);                                             \
            pxor_r2r (mm2, mm5);                                            \
            pand_r2r (mm5, mm7);                                            \
            pavg_r2r (mm2, mm0);                                            \
            pand_m2r (mask_one, mm7);                                       \
            psubusb_r2r (mm7, mm0);                                         \
            p_src += i_stride;                                              \
            movq_r2m (mm0, *p_dest);                                        \
            p_dest += i_stride;                                             \
            movq_r2r (mm6, mm7);                                            \
            movq_r2r (mm2, mm0);                                            \
        }                                                                   \
    }                                                                       \
}

#define __MotionComponent_x_y_avg(width,height)                             \
void _M(MotionComponent_x_y_avg_##width##_##height)(yuv_data_t * p_src,     \
                                                yuv_data_t * p_dest,        \
                                                int i_stride)               \
{                                                                           \
    int i_y;                                                                \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        movq_m2r( *p_src, mm0 );                                            \
        if( width == 16 )                                                   \
            movq_m2r( *(p_src + 8), mm1 );                                  \
        pavg_m2r( *p_dest, mm0 );                                           \
        if( width == 16 )                                                   \
            pavg_m2r( *(p_dest + 8), mm1 );                                 \
        movq_r2m( mm0, *p_dest );                                           \
        p_src += i_stride;                                                  \
        if( width == 16 )                                                   \
            movq_r2m( mm1, *(p_dest + 8) );                                 \
        p_dest += i_stride;                                                 \
    }                                                                       \
}

#define __MotionComponent_X_y_avg(width,height)                             \
void _M(MotionComponent_X_y_avg_##width##_##height)(yuv_data_t * p_src,     \
                                                yuv_data_t * p_dest,        \
                                                int i_stride)               \
{                                                                           \
    int i_y;                                                                \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        movq_m2r (*p_src, mm0);                                             \
        if( width == 16 )                                                   \
            movq_m2r (*(p_src + 8), mm1);                                   \
        pavg_m2r (*(p_src + 1), mm0);                                       \
        if( width == 16 )                                                   \
            pavg_m2r (*(p_src + 9), mm1);                                   \
        pavg_m2r (*p_dest, mm0);                                            \
        if( width == 16 )                                                   \
            pavg_m2r (*(p_dest + 8), mm1);                                  \
        p_src += i_stride;                                                  \
        movq_r2m (mm0, *p_dest);                                            \
        if( width == 16 )                                                   \
            movq_r2m (mm1, *(p_dest + 8));                                  \
        p_dest += i_stride;                                                 \
    }                                                                       \
}

#define __MotionComponent_x_Y_avg(width,height)                             \
void _M(MotionComponent_x_Y_avg_##width##_##height)(yuv_data_t * p_src,     \
                                                yuv_data_t * p_dest,        \
                                                int i_stride)               \
{                                                                           \
    int i_y;                                                                \
    yuv_data_t * p_next_src = p_src + i_stride;                             \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        movq_m2r (*p_src, mm0);                                             \
        if( width == 16 )                                                   \
            movq_m2r (*(p_src + 8), mm1);                                   \
        pavg_m2r (*(p_next_src), mm0);                                      \
        if( width == 16 )                                                   \
            pavg_m2r (*(p_next_src + 8), mm1);                              \
        pavg_m2r (*p_dest, mm0);                                            \
        if( width == 16 )                                                   \
            pavg_m2r (*(p_dest + 8), mm1);                                  \
        p_src += i_stride;                                                  \
        p_next_src += i_stride;                                             \
        movq_r2m (mm0, *p_dest);                                            \
        if( width == 16 )                                                   \
            movq_r2m (mm1, *(p_dest + 8));                                  \
        p_dest += i_stride;                                                 \
    }                                                                       \
}

#define __MotionComponent_X_Y_avg(width,height)                             \
void _M(MotionComponent_X_Y_avg_##width##_##height)(yuv_data_t * p_src,     \
                                                yuv_data_t * p_dest,        \
                                                int i_stride)               \
{                                                                           \
    int i_y;                                                                \
                                                                            \
    if( width == 16 )                                                       \
    {                                                                       \
        for( i_y = 0; i_y < height; i_y ++ )                                \
        {                                                                   \
            movq_m2r (*p_src, mm0);                                         \
            movq_m2r (*(p_src+i_stride+1), mm1);                            \
            movq_r2r (mm0, mm7);                                            \
            movq_m2r (*(p_src+1), mm2);                                     \
            pxor_r2r (mm1, mm7);                                            \
            movq_m2r (*(p_src+i_stride), mm3);                              \
            movq_r2r (mm2, mm6);                                            \
            pxor_r2r (mm3, mm6);                                            \
            pavg_r2r (mm1, mm0);                                            \
            pavg_r2r (mm3, mm2);                                            \
            por_r2r (mm6, mm7);                                             \
            movq_r2r (mm0, mm6);                                            \
            pxor_r2r (mm2, mm6);                                            \
            pand_r2r (mm6, mm7);                                            \
            pand_m2r (mask_one, mm7);                                       \
            pavg_r2r (mm2, mm0);                                            \
            psubusb_r2r (mm7, mm0);                                         \
            movq_m2r (*p_dest, mm1);                                        \
            pavg_r2r (mm1, mm0);                                            \
            movq_r2m (mm0, *p_dest);                                        \
                                                                            \
            movq_m2r (*(p_src+8), mm0);                                     \
            movq_m2r (*(p_src+i_stride+9), mm1);                            \
            movq_r2r (mm0, mm7);                                            \
            movq_m2r (*(p_src+9), mm2);                                     \
            pxor_r2r (mm1, mm7);                                            \
            movq_m2r (*(p_src+i_stride+8), mm3);                            \
            movq_r2r (mm2, mm6);                                            \
            pxor_r2r (mm3, mm6);                                            \
            pavg_r2r (mm1, mm0);                                            \
            pavg_r2r (mm3, mm2);                                            \
            por_r2r (mm6, mm7);                                             \
            movq_r2r (mm0, mm6);                                            \
            pxor_r2r (mm2, mm6);                                            \
            pand_r2r (mm6, mm7);                                            \
            pand_m2r (mask_one, mm7);                                       \
            pavg_r2r (mm2, mm0);                                            \
            psubusb_r2r (mm7, mm0);                                         \
            movq_m2r (*(p_dest+8), mm1);                                    \
            pavg_r2r (mm1, mm0);                                            \
            p_src += i_stride;                                              \
            movq_r2m (mm0, *(p_dest+8));                                    \
            p_dest += i_stride;                                             \
        }                                                                   \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        for( i_y = 0; i_y < height; i_y ++ )                                \
        {                                                                   \
            movq_m2r (*p_src, mm0);                                         \
            movq_m2r (*(p_src+i_stride+1), mm1);                            \
            movq_r2r (mm0, mm7);                                            \
            movq_m2r (*(p_src+1), mm2);                                     \
            pxor_r2r (mm1, mm7);                                            \
            movq_m2r (*(p_src+i_stride), mm3);                              \
            movq_r2r (mm2, mm6);                                            \
            pxor_r2r (mm3, mm6);                                            \
            pavg_r2r (mm1, mm0);                                            \
            pavg_r2r (mm3, mm2);                                            \
            por_r2r (mm6, mm7);                                             \
            movq_r2r (mm0, mm6);                                            \
            pxor_r2r (mm2, mm6);                                            \
            pand_r2r (mm6, mm7);                                            \
            pand_m2r (mask_one, mm7);                                       \
            pavg_r2r (mm2, mm0);                                            \
            psubusb_r2r (mm7, mm0);                                         \
            movq_m2r (*p_dest, mm1);                                        \
            pavg_r2r (mm1, mm0);                                            \
            p_src += i_stride;                                              \
            movq_r2m (mm0, *p_dest);                                        \
            p_dest += i_stride;                                             \
        }                                                                   \
    }                                                                       \
}

#define __MotionComponents(width,height)                                    \
__MotionComponent_x_y_copy(width,height)                                    \
__MotionComponent_X_y_copy(width,height)                                    \
__MotionComponent_x_Y_copy(width,height)                                    \
__MotionComponent_X_Y_copy(width,height)                                    \
__MotionComponent_x_y_avg(width,height)                                     \
__MotionComponent_X_y_avg(width,height)                                     \
__MotionComponent_x_Y_avg(width,height)                                     \
__MotionComponent_X_Y_avg(width,height)

__MotionComponents (16,16)      /* 444, 422, 420 */
__MotionComponents (16,8)       /* 444, 422, 420 */
__MotionComponents (8,8)        /* 422, 420 */
__MotionComponents (8,4)        /* 420 */
#if 0
__MotionComponents (8,16)       /* 422 */
#endif
