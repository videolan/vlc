/*****************************************************************************
 * vdec_motion_inner_mmx.c : motion compensation inner routines optimized in
 *                           MMX
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vdec_motion_inner_mmx.c,v 1.6 2001/01/05 14:46:37 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */

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

#include "vdec_idct.h"
#include "video_decoder.h"
#include "vdec_motion.h"

#include "vpar_blocks.h"
#include "vpar_headers.h"
#include "vpar_synchro.h"
#include "video_parser.h"
#include "video_fifo.h"

#include "mmx.h"

/* OK, I know, this code has been taken from livid's mpeg2dec --Meuuh */

/* Some rounding constants */
mmx_t round1 = {0x0001000100010001LL};
mmx_t round4 = {0x0002000200020002LL};

/*
 * Useful functions
 */

static __inline__ void MMXZeroReg()
{
   /* load 0 into mm0 */
   pxor_r2r(mm0,mm0);
}

static __inline__ void MMXAverage2( u8 *dst, u8 *src1, u8 *src2 )
{
   //
   // *dst = clip_to_u8((*src1 + *src2 + 1)/2);
   //

   //mmx_zero_reg();

   movq_m2r(*src1,mm1);        // load 8 src1 bytes
   movq_r2r(mm1,mm2);          // copy 8 src1 bytes

   movq_m2r(*src2,mm3);        // load 8 src2 bytes
   movq_r2r(mm3,mm4);          // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm1);     // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm2);     // unpack high src1 bytes

   punpcklbw_r2r(mm0,mm3);     // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm4);     // unpack high src2 bytes

   paddw_r2r(mm3,mm1);         // add lows to mm1
   paddw_m2r(round1,mm1);
   psraw_i2r(1,mm1);           // /2

   paddw_r2r(mm4,mm2);         // add highs to mm2
   paddw_m2r(round1,mm2);
   psraw_i2r(1,mm2);           // /2

   packuswb_r2r(mm2,mm1);      // pack (w/ saturation)
   movq_r2m(mm1,*dst);         // store result in dst
}

static __inline__ void MMXInterpAverage2( u8 *dst, u8 *src1, u8 *src2 )
{
   //
   // *dst = clip_to_u8((*dst + (*src1 + *src2 + 1)/2 + 1)/2);
   //

   //mmx_zero_reg();

   movq_m2r(*dst,mm1);            // load 8 dst bytes
   movq_r2r(mm1,mm2);             // copy 8 dst bytes

   movq_m2r(*src1,mm3);           // load 8 src1 bytes
   movq_r2r(mm3,mm4);             // copy 8 src1 bytes

   movq_m2r(*src2,mm5);           // load 8 src2 bytes
   movq_r2r(mm5,mm6);             // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm1);        // unpack low dst bytes
   punpckhbw_r2r(mm0,mm2);        // unpack high dst bytes

   punpcklbw_r2r(mm0,mm3);        // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm4);        // unpack high src1 bytes

   punpcklbw_r2r(mm0,mm5);        // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm6);        // unpack high src2 bytes

   paddw_r2r(mm5,mm3);            // add lows
   paddw_m2r(round1,mm3);
   psraw_i2r(1,mm3);              // /2

   paddw_r2r(mm6,mm4);            // add highs
   paddw_m2r(round1,mm4);
   psraw_i2r(1,mm4);              // /2

   paddw_r2r(mm3,mm1);            // add lows
   paddw_m2r(round1,mm1);
   psraw_i2r(1,mm1);              // /2

   paddw_r2r(mm4,mm2);            // add highs
   paddw_m2r(round1,mm2);
   psraw_i2r(1,mm2);              // /2

   packuswb_r2r(mm2,mm1);         // pack (w/ saturation)
   movq_r2m(mm1,*dst);            // store result in dst
}

static __inline__ void MMXAverage4( u8 *dst, u8 *src1, u8 *src2, u8 *src3,
                                    u8 *src4 )
{
   //
   // *dst = clip_to_u8((*src1 + *src2 + *src3 + *src4 + 2)/4);
   //

   //mmx_zero_reg();

   movq_m2r(*src1,mm1);                // load 8 src1 bytes
   movq_r2r(mm1,mm2);                  // copy 8 src1 bytes

   punpcklbw_r2r(mm0,mm1);             // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm2);             // unpack high src1 bytes

   movq_m2r(*src2,mm3);                // load 8 src2 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src2 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   // now have partials in mm1 and mm2

   movq_m2r(*src3,mm3);                // load 8 src3 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src3 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src3 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src3 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   movq_m2r(*src4,mm5);                // load 8 src4 bytes
   movq_r2r(mm5,mm6);                  // copy 8 src4 bytes

   punpcklbw_r2r(mm0,mm5);             // unpack low src4 bytes
   punpckhbw_r2r(mm0,mm6);             // unpack high src4 bytes

   paddw_r2r(mm5,mm1);                 // add lows
   paddw_r2r(mm6,mm2);                 // add highs

   // now have subtotal in mm1 and mm2

   paddw_m2r(round4,mm1);
   psraw_i2r(2,mm1);                   // /4
   paddw_m2r(round4,mm2);
   psraw_i2r(2,mm2);                   // /4

   packuswb_r2r(mm2,mm1);              // pack (w/ saturation)
   movq_r2m(mm1,*dst);                 // store result in dst
}

static __inline__ void MMXInterpAverage4( u8 *dst, u8 *src1, u8 *src2,
                                          u8 *src3, u8 *src4 )
{
   //
   // *dst = clip_to_u8((*dst + (*src1 + *src2 + *src3 + *src4 + 2)/4 + 1)/2);
   //

   //mmx_zero_reg();

   movq_m2r(*src1,mm1);                // load 8 src1 bytes
   movq_r2r(mm1,mm2);                  // copy 8 src1 bytes

   punpcklbw_r2r(mm0,mm1);             // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm2);             // unpack high src1 bytes

   movq_m2r(*src2,mm3);                // load 8 src2 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src2 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   // now have partials in mm1 and mm2

   movq_m2r(*src3,mm3);                // load 8 src3 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src3 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src3 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src3 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   movq_m2r(*src4,mm5);                // load 8 src4 bytes
   movq_r2r(mm5,mm6);                  // copy 8 src4 bytes

   punpcklbw_r2r(mm0,mm5);             // unpack low src4 bytes
   punpckhbw_r2r(mm0,mm6);             // unpack high src4 bytes

   paddw_r2r(mm5,mm1);                 // add lows
   paddw_r2r(mm6,mm2);                 // add highs

   paddw_m2r(round4,mm1);
   psraw_i2r(2,mm1);                   // /4
   paddw_m2r(round4,mm2);
   psraw_i2r(2,mm2);                   // /4

   // now have subtotal/4 in mm1 and mm2

   movq_m2r(*dst,mm3);                 // load 8 dst bytes
   movq_r2r(mm3,mm4);                  // copy 8 dst bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low dst bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high dst bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   paddw_m2r(round1,mm1);
   psraw_i2r(1,mm1);                   // /2
   paddw_m2r(round1,mm2);
   psraw_i2r(1,mm2);                   // /2

   // now have end value in mm1 and mm2

   packuswb_r2r(mm2,mm1);              // pack (w/ saturation)
   movq_r2m(mm1,*dst);                 // store result in dst
}


/*
 * Actual Motion compensation
 */

#define __MotionComponent_x_y_copy(width,height)                            \
void MotionComponent_x_y_copy_##width##_##height(yuv_data_t * p_src,        \
                                                 yuv_data_t * p_dest,       \
                                                 int i_stride)              \
{                                                                           \
    int i_y;                                                                \
                                                                            \
    MMXZeroReg();                                                           \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        movq_m2r( *p_src, mm1 );     /* load 8 ref bytes */                 \
        movq_r2m( mm1, *p_dest );    /* store 8 bytes at curr */            \
                                                                            \
        if( width == 16 )                                                   \
        {                                                                   \
            movq_m2r( *(p_src + 8), mm1 );      /* load 8 ref bytes */      \
            movq_r2m( mm1, *(p_dest + 8) );     /* store 8 bytes at curr */ \
        }                                                                   \
                                                                            \
        p_dest += i_stride;                                                 \
        p_src += i_stride;                                                  \
    }                                                                       \
}

#define __MotionComponent_X_y_copy(width,height)                            \
void MotionComponent_X_y_copy_##width##_##height(yuv_data_t * p_src,        \
                                                 yuv_data_t * p_dest,       \
                                                 int i_stride)              \
{                                                                           \
    int i_y;                                                                \
                                                                            \
    MMXZeroReg();                                                           \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        MMXAverage2( p_dest, p_src, p_src + 1 );                            \
                                                                            \
        if( width == 16 )                                                   \
        {                                                                   \
            MMXAverage2( p_dest + 8, p_src + 8, p_src + 9 );                \
        }                                                                   \
                                                                            \
        p_dest += i_stride;                                                 \
        p_src += i_stride;                                                  \
    }                                                                       \
}

#define __MotionComponent_x_Y_copy(width,height)                            \
void MotionComponent_x_Y_copy_##width##_##height(yuv_data_t * p_src,        \
                                                 yuv_data_t * p_dest,       \
                                                 int i_stride)              \
{                                                                           \
    int i_y;                                                                \
    yuv_data_t * p_next_src = p_src + i_stride;                             \
                                                                            \
    MMXZeroReg();                                                           \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        MMXAverage2( p_dest, p_src, p_next_src );                           \
                                                                            \
        if( width == 16 )                                                   \
        {                                                                   \
            MMXAverage2( p_dest + 8, p_src + 8, p_next_src + 8 );           \
        }                                                                   \
                                                                            \
        p_dest += i_stride;                                                 \
        p_src += i_stride;                                                  \
        p_next_src += i_stride;                                             \
    }                                                                       \
}

#define __MotionComponent_X_Y_copy(width,height)                            \
void MotionComponent_X_Y_copy_##width##_##height(yuv_data_t * p_src,        \
                                                 yuv_data_t * p_dest,       \
                                                 int i_stride)              \
{                                                                           \
    int i_y;                                                                \
    yuv_data_t * p_next_src = p_src + i_stride;                             \
                                                                            \
    MMXZeroReg();                                                           \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        MMXAverage4( p_dest, p_src, p_src + 1, p_next_src, p_next_src + 1 );\
                                                                            \
        if( width == 16 )                                                   \
        {                                                                   \
            MMXAverage4( p_dest + 8, p_src + 8, p_src + 9,                  \
                         p_next_src + 8, p_next_src + 9 );                  \
        }                                                                   \
                                                                            \
        p_dest += i_stride;                                                 \
        p_src += i_stride;                                                  \
        p_next_src += i_stride;                                             \
    }                                                                       \
}

#define __MotionComponent_x_y_avg(width,height)                             \
void MotionComponent_x_y_avg_##width##_##height(yuv_data_t * p_src,         \
                                                yuv_data_t * p_dest,        \
                                                int i_stride)               \
{                                                                           \
    int i_y;                                                                \
                                                                            \
    MMXZeroReg();                                                           \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        MMXAverage2( p_dest, p_dest, p_src );                               \
                                                                            \
        if( width == 16 )                                                   \
        {                                                                   \
            MMXAverage2( p_dest + 8, p_dest + 8, p_src + 8 );               \
        }                                                                   \
                                                                            \
        p_dest += i_stride;                                                 \
        p_src += i_stride;                                                  \
    }                                                                       \
}

#define __MotionComponent_X_y_avg(width,height)                             \
void MotionComponent_X_y_avg_##width##_##height(yuv_data_t * p_src,         \
                                                yuv_data_t * p_dest,        \
                                                int i_stride)               \
{                                                                           \
    int i_y;                                                                \
                                                                            \
    MMXZeroReg();                                                           \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        MMXInterpAverage2( p_dest, p_src, p_src + 1 );                      \
                                                                            \
        if( width == 16 )                                                   \
        {                                                                   \
            MMXInterpAverage2( p_dest + 8, p_dest + 8, p_src + 9 );         \
        }                                                                   \
                                                                            \
        p_dest += i_stride;                                                 \
        p_src += i_stride;                                                  \
    }                                                                       \
}

#define __MotionComponent_x_Y_avg(width,height)                             \
void MotionComponent_x_Y_avg_##width##_##height(yuv_data_t * p_src,         \
                                                yuv_data_t * p_dest,        \
                                                int i_stride)               \
{                                                                           \
    int i_x, i_y;                                                           \
    unsigned int i_dummy;                                                   \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        for( i_x = 0; i_x < width; i_x++ )                                  \
        {                                                                   \
            i_dummy =                                                       \
                p_dest[i_x] + ((unsigned int)(p_src[i_x]                    \
                                              + p_src[i_x + i_stride]       \
                                              + 1) >> 1);                   \
            p_dest[i_x] = (i_dummy + 1) >> 1;                               \
        }                                                                   \
        p_dest += i_stride;                                                 \
        p_src += i_stride;                                                  \
    }                                                                       \
}

#define __MotionComponent_X_Y_avg(width,height)                             \
void MotionComponent_X_Y_avg_##width##_##height(yuv_data_t * p_src,         \
                                                yuv_data_t * p_dest,        \
                                                int i_stride)               \
{                                                                           \
    int i_y;                                                                \
    yuv_data_t * p_next_src = p_src + i_stride;                             \
                                                                            \
    MMXZeroReg();                                                           \
                                                                            \
    for( i_y = 0; i_y < height; i_y ++ )                                    \
    {                                                                       \
        MMXInterpAverage4( p_dest, p_src, p_src + 1, p_next_src,            \
                           p_next_src + 1 );                                \
                                                                            \
        if( width == 16 )                                                   \
        {                                                                   \
            MMXInterpAverage4( p_dest + 8, p_src + 8, p_src + 9,            \
                               p_next_src + 8, p_next_src + 9 );            \
        }                                                                   \
                                                                            \
        p_dest += i_stride;                                                 \
        p_src += i_stride;                                                  \
        p_next_src += i_stride;                                             \
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
