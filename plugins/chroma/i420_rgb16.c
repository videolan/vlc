/*****************************************************************************
 * i420_rgb16.c : YUV to bitmap RGB conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: i420_rgb16.c,v 1.5 2002/03/17 17:00:38 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <math.h>                                            /* exp(), pow() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                                      /* malloc(), free() */

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "i420_rgb.h"
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
#   include "i420_rgb_c.h"
#elif defined (MODULE_NAME_IS_chroma_i420_rgb_mmx)
#   include "i420_rgb_mmx.h"
#endif

static void SetOffset( int, int, int, int, boolean_t *, int *, int * );

/*****************************************************************************
 * I420_RGB15: color YUV 4:2:0 to RGB 15 bpp
 *****************************************************************************
 * Horizontal alignment needed:
 *  - input: 8 pixels (8 Y bytes, 4 U/V bytes), margins not allowed
 *  - output: 1 pixel (2 bytes), margins allowed
 * Vertical alignment needed:
 *  - input: 2 lines (2 Y lines, 1 U/V line)
 *  - output: 1 line
 *****************************************************************************/
void _M( I420_RGB15 )( vout_thread_t *p_vout, picture_t *p_src,
                                              picture_t *p_dest )
{
    /* We got this one from the old arguments */
    u16 *p_pic = (u16*)p_dest->p->p_pixels;
    u8  *p_y   = p_src->Y_PIXELS;
    u8  *p_u   = p_src->U_PIXELS;
    u8  *p_v   = p_src->V_PIXELS;

    boolean_t   b_hscale;                         /* horizontal scaling type */
    int         i_vscale;                           /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_right_margin;
    int         i_rewind;
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width = p_vout->render.i_width / 2; /* chroma width */
    u16 *       p_pic_start;       /* beginning of the current line for copy */
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    u16 *       p_yuv = p_vout->chroma.p_sys->p_rgb16;
    u16 *       p_ybase;                     /* Y dependant conversion table */
#endif

    /* Conversion buffer pointer */
    u16 *       p_buffer_start = (u16*)p_vout->chroma.p_sys->p_buffer;
    u16 *       p_buffer;

    /* Offset array pointer */
    int *       p_offset_start = p_vout->chroma.p_sys->p_offset;
    int *       p_offset;

    if( p_dest->p->b_margin )
    {
        i_right_margin = (p_dest->p->i_pitch - p_dest->p->i_visible_bytes) / 2;
    }
    else
    {
        i_right_margin = 0;
    }

    if( p_vout->render.i_width & 7 )
    {
        i_rewind = 8 - ( p_vout->render.i_width & 7 );
    }
    else
    {
        i_rewind = 0;
    }

    /* Rule: when a picture of size (x1,y1) with aspect ratio r1 is rendered
     * on a picture of size (x2,y2) with aspect ratio r2, if x1 grows to x1'
     * then y1 grows to y1' = x1' * y2/x2 * r2/r1 */
    SetOffset( p_vout->render.i_width, p_vout->render.i_height,
               p_vout->output.i_width, p_vout->output.i_height,
               &b_hscale, &i_vscale, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = ( i_vscale == 1 ) ?
                    p_vout->output.i_height : p_vout->render.i_height;
    for( i_y = 0; i_y < p_vout->render.i_height; i_y++ )
    {
        p_pic_start = p_pic;
        p_buffer = b_hscale ? p_buffer_start : p_pic;

        for ( i_x = p_vout->render.i_width / 8; i_x--; )
        {
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
#elif defined (MODULE_NAME_IS_chroma_i420_rgb_mmx)
            __asm__( MMX_INIT_16
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            __asm__( ".align 8"
                     MMX_YUV_MUL
                     MMX_YUV_ADD
                     MMX_UNPACK_15
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            p_y += 8;
            p_u += 4;
            p_v += 4;
            p_buffer += 8;
#endif
        }

        /* Here we do some unaligned reads and duplicate conversions, but
         * at least we have all the pixels */
        if( i_rewind )
        {
            p_y -= i_rewind;
            p_u -= i_rewind >> 1;
            p_v -= i_rewind >> 1;
            p_buffer -= i_rewind;
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
#elif defined (MODULE_NAME_IS_chroma_i420_rgb_mmx)
            __asm__( MMX_INIT_16
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            __asm__( ".align 8"
                     MMX_YUV_MUL
                     MMX_YUV_ADD
                     MMX_UNPACK_15
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            p_y += 8;
            p_u += 4;
            p_v += 4;
            p_buffer += 8;
#endif
        }
        SCALE_WIDTH;
        SCALE_HEIGHT( 420, 2 );
    }
}

/*****************************************************************************
 * I420_RGB16: color YUV 4:2:0 to RGB 16 bpp
 *****************************************************************************
 * Horizontal alignment needed:
 *  - input: 8 pixels (8 Y bytes, 4 U/V bytes), margins not allowed
 *  - output: 1 pixel (2 bytes), margins allowed
 * Vertical alignment needed:
 *  - input: 2 lines (2 Y lines, 1 U/V line)
 *  - output: 1 line
 *****************************************************************************/
void _M( I420_RGB16 )( vout_thread_t *p_vout, picture_t *p_src,
                                              picture_t *p_dest )
{
    /* We got this one from the old arguments */
    u16 *p_pic = (u16*)p_dest->p->p_pixels;
    u8  *p_y   = p_src->Y_PIXELS;
    u8  *p_u   = p_src->U_PIXELS;
    u8  *p_v   = p_src->V_PIXELS;

    boolean_t   b_hscale;                         /* horizontal scaling type */
    int         i_vscale;                           /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_right_margin;
    int         i_rewind;
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width = p_vout->render.i_width / 2; /* chroma width */
    u16 *       p_pic_start;       /* beginning of the current line for copy */
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    u16 *       p_yuv = p_vout->chroma.p_sys->p_rgb16;
    u16 *       p_ybase;                     /* Y dependant conversion table */
#endif

    /* Conversion buffer pointer */
    u16 *       p_buffer_start = (u16*)p_vout->chroma.p_sys->p_buffer;
    u16 *       p_buffer;

    /* Offset array pointer */
    int *       p_offset_start = p_vout->chroma.p_sys->p_offset;
    int *       p_offset;

    if( p_dest->p->b_margin )
    {
        i_right_margin = (p_dest->p->i_pitch - p_dest->p->i_visible_bytes) / 2;
    }
    else
    {
        i_right_margin = 0;
    }

    if( p_vout->render.i_width & 7 )
    {
        i_rewind = 8 - ( p_vout->render.i_width & 7 );
    }
    else
    {
        i_rewind = 0;
    }

    /* Rule: when a picture of size (x1,y1) with aspect ratio r1 is rendered
     * on a picture of size (x2,y2) with aspect ratio r2, if x1 grows to x1'
     * then y1 grows to y1' = x1' * y2/x2 * r2/r1 */
    SetOffset( p_vout->render.i_width, p_vout->render.i_height,
               p_vout->output.i_width, p_vout->output.i_height,
               &b_hscale, &i_vscale, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = ( i_vscale == 1 ) ?
                    p_vout->output.i_height : p_vout->render.i_height;
    for( i_y = 0; i_y < p_vout->render.i_height; i_y++ )
    {
        p_pic_start = p_pic;
        p_buffer = b_hscale ? p_buffer_start : p_pic;

        for ( i_x = p_vout->render.i_width / 8; i_x--; )
        {
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
#elif defined (MODULE_NAME_IS_chroma_i420_rgb_mmx)
            __asm__( MMX_INIT_16
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            __asm__( ".align 8"
                     MMX_YUV_MUL
                     MMX_YUV_ADD
                     MMX_UNPACK_16
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            p_y += 8;
            p_u += 4;
            p_v += 4;
            p_buffer += 8;
#endif
        }

        /* Here we do some unaligned reads and duplicate conversions, but
         * at least we have all the pixels */
        if( i_rewind )
        {
            p_y -= i_rewind;
            p_u -= i_rewind >> 1;
            p_v -= i_rewind >> 1;
            p_buffer -= i_rewind;
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
#elif defined (MODULE_NAME_IS_chroma_i420_rgb_mmx)
            __asm__( MMX_INIT_16
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            __asm__( ".align 8"
                     MMX_YUV_MUL
                     MMX_YUV_ADD
                     MMX_UNPACK_16
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            p_y += 8;
            p_u += 4;
            p_v += 4;
            p_buffer += 8;
#endif
        }
        SCALE_WIDTH;
        SCALE_HEIGHT( 420, 2 );
    }
}

/*****************************************************************************
 * I420_RGB32: color YUV 4:2:0 to RGB 32 bpp
 *****************************************************************************
 * Horizontal alignment needed:
 *  - input: 8 pixels (8 Y bytes, 4 U/V bytes), margins not allowed
 *  - output: 1 pixel (2 bytes), margins allowed
 * Vertical alignment needed:
 *  - input: 2 lines (2 Y lines, 1 U/V line)
 *  - output: 1 line
 *****************************************************************************/
void _M( I420_RGB32 )( vout_thread_t *p_vout, picture_t *p_src,
                                              picture_t *p_dest )
{
    /* We got this one from the old arguments */
    u32 *p_pic = (u32*)p_dest->p->p_pixels;
    u8  *p_y   = p_src->Y_PIXELS;
    u8  *p_u   = p_src->U_PIXELS;
    u8  *p_v   = p_src->V_PIXELS;

    boolean_t   b_hscale;                         /* horizontal scaling type */
    int         i_vscale;                           /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_right_margin;
    int         i_rewind;
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width = p_vout->render.i_width / 2; /* chroma width */
    u32 *       p_pic_start;       /* beginning of the current line for copy */
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    u32 *       p_yuv = p_vout->chroma.p_sys->p_rgb32;
    u32 *       p_ybase;                     /* Y dependant conversion table */
#endif

    /* Conversion buffer pointer */
    u32 *       p_buffer_start = (u32*)p_vout->chroma.p_sys->p_buffer;
    u32 *       p_buffer;

    /* Offset array pointer */
    int *       p_offset_start = p_vout->chroma.p_sys->p_offset;
    int *       p_offset;

    if( p_dest->p->b_margin )
    {
        i_right_margin = (p_dest->p->i_pitch - p_dest->p->i_visible_bytes) / 2;
    }
    else
    {
        i_right_margin = 0;
    }

    if( p_vout->render.i_width & 7 )
    {
        i_rewind = 8 - ( p_vout->render.i_width & 7 );
    }
    else
    {
        i_rewind = 0;
    }

    /* Rule: when a picture of size (x1,y1) with aspect ratio r1 is rendered
     * on a picture of size (x2,y2) with aspect ratio r2, if x1 grows to x1'
     * then y1 grows to y1' = x1' * y2/x2 * r2/r1 */
    SetOffset( p_vout->render.i_width, p_vout->render.i_height,
               p_vout->output.i_width, p_vout->output.i_height,
               &b_hscale, &i_vscale, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = ( i_vscale == 1 ) ?
                    p_vout->output.i_height : p_vout->render.i_height;
    for( i_y = 0; i_y < p_vout->render.i_height; i_y++ )
    {
        p_pic_start = p_pic;
        p_buffer = b_hscale ? p_buffer_start : p_pic;

        for ( i_x = p_vout->render.i_width / 8; i_x--; )
        {
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
#elif defined (MODULE_NAME_IS_chroma_i420_rgb_mmx)
            __asm__( MMX_INIT_32
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            __asm__( ".align 8"
                     MMX_YUV_MUL
                     MMX_YUV_ADD
                     MMX_UNPACK_32
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            p_y += 8;
            p_u += 4;
            p_v += 4;
            p_buffer += 8;
#endif
        }

        /* Here we do some unaligned reads and duplicate conversions, but
         * at least we have all the pixels */
        if( i_rewind )
        {
            p_y -= i_rewind;
            p_u -= i_rewind >> 1;
            p_v -= i_rewind >> 1;
            p_buffer -= i_rewind;
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
#elif defined (MODULE_NAME_IS_chroma_i420_rgb_mmx)
            __asm__( MMX_INIT_32
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            __asm__( ".align 8"
                     MMX_YUV_MUL
                     MMX_YUV_ADD
                     MMX_UNPACK_32
                     : : "r" (p_y), "r" (p_u), "r" (p_v), "r" (p_buffer) );

            p_y += 8;
            p_u += 4;
            p_v += 4;
            p_buffer += 8;
#endif
        }
        SCALE_WIDTH;
        SCALE_HEIGHT( 420, 4 );
    }
}

/* Following functions are local */

/*****************************************************************************
 * SetOffset: build offset array for conversion functions
 *****************************************************************************
 * This function will build an offset array used in later conversion functions.
 * It will also set horizontal and vertical scaling indicators.
 *****************************************************************************/
static void SetOffset( int i_width, int i_height, int i_pic_width,
                       int i_pic_height, boolean_t *pb_hscale,
                       int *pi_vscale, int *p_offset )
{
    int i_x;                                    /* x position in destination */
    int i_scale_count;                                     /* modulo counter */

    /*
     * Prepare horizontal offset array
     */
    if( i_pic_width - i_width == 0 )
    {
        /* No horizontal scaling: YUV conversion is done directly to picture */
        *pb_hscale = 0;
    }
    else if( i_pic_width - i_width > 0 )
    {
        /* Prepare scaling array for horizontal extension */
        *pb_hscale = 1;
        i_scale_count = i_pic_width;
        for( i_x = i_width; i_x--; )
        {
            while( (i_scale_count -= i_width) > 0 )
            {
                *p_offset++ = 0;
            }
            *p_offset++ = 1;
            i_scale_count += i_pic_width;
        }
    }
    else /* if( i_pic_width - i_width < 0 ) */
    {
        /* Prepare scaling array for horizontal reduction */
        *pb_hscale = 1;
        i_scale_count = i_width;
        for( i_x = i_pic_width; i_x--; )
        {
            *p_offset = 1;
            while( (i_scale_count -= i_pic_width) > 0 )
            {
                *p_offset += 1;
            }
            p_offset++;
            i_scale_count += i_width;
        }
    }

    /*
     * Set vertical scaling indicator
     */
    if( i_pic_height - i_height == 0 )
    {
        *pi_vscale = 0;
    }
    else if( i_pic_height - i_height > 0 )
    {
        *pi_vscale = 1;
    }
    else /* if( i_pic_height - i_height < 0 ) */
    {
        *pi_vscale = -1;
    }
}

