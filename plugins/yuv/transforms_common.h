/*****************************************************************************
 * transforms_common.h: YUV transformation macros for truecolor
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: transforms_common.h,v 1.3 2001/10/11 13:19:27 massiot Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * CONVERT_YUV_PIXEL, CONVERT_Y_PIXEL: pixel conversion blocks
 *****************************************************************************
 * These conversion routines are used by YUV conversion functions.
 * conversion are made from p_y, p_u, p_v, which are modified, to p_buffer,
 * which is also modified.
 *****************************************************************************/
#define CONVERT_Y_PIXEL( BPP )                                                \
    /* Only Y sample is present */                                            \
    p_ybase = p_yuv + *p_y++;                                                 \
    *p_buffer++ = p_ybase[RED_OFFSET-((V_RED_COEF*128)>>SHIFT) + i_red] |     \
        p_ybase[GREEN_OFFSET-(((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT)       \
        + i_green ] | p_ybase[BLUE_OFFSET-((U_BLUE_COEF*128)>>SHIFT) + i_blue];

#define CONVERT_YUV_PIXEL( BPP )                                              \
    /* Y, U and V samples are present */                                      \
    i_uval =    *p_u++;                                                       \
    i_vval =    *p_v++;                                                       \
    i_red =     (V_RED_COEF * i_vval) >> SHIFT;                               \
    i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;     \
    i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;                              \
    CONVERT_Y_PIXEL( BPP )                                                    \

/*****************************************************************************
 * SCALE_WIDTH: scale a line horizontally
 *****************************************************************************
 * This macro scales a line using rendering buffer and offset array. It works
 * for 1, 2 and 4 Bpp.
 *****************************************************************************/
#define SCALE_WIDTH                                                           \
    if( b_horizontal_scaling )                                                \
    {                                                                         \
        /* Horizontal scaling, conversion has been done to buffer.            \
         * Rewind buffer and offset, then copy and scale line */              \
        p_buffer = p_buffer_start;                                            \
        p_offset = p_offset_start;                                            \
        for( i_x = i_pic_width / 16; i_x--; )                                 \
        {                                                                     \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
        }                                                                     \
        p_pic += i_pic_line_width;                                            \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        /* No scaling, conversion has been done directly in picture memory.   \
         * Increment of picture pointer to end of line is still needed */     \
        p_pic += i_pic_width + i_pic_line_width;                              \
    }                                                                         \

/*****************************************************************************
 * SCALE_HEIGHT: handle vertical scaling
 *****************************************************************************
 * This macro handle vertical scaling for a picture. CHROMA may be 420, 422 or
 * 444 for RGB conversion, or 400 for gray conversion. It works for 1, 2, 3
 * and 4 Bpp.
 *****************************************************************************/
#define SCALE_HEIGHT( CHROMA, BPP )                                           \
    /* If line is odd, rewind 4:2:0 U and V samples */                        \
    if( (CHROMA == 420) && !(i_y & 0x1) )                                     \
    {                                                                         \
        p_u -= i_chroma_width;                                                \
        p_v -= i_chroma_width;                                                \
    }                                                                         \
                                                                              \
    /*                                                                        \
     * Handle vertical scaling. The current line can be copied or next one    \
     * can be ignored.                                                        \
     */                                                                       \
    switch( i_vertical_scaling )                                              \
    {                                                                         \
    case -1:                             /* vertical scaling factor is < 1 */ \
        while( (i_scale_count -= i_pic_height) > 0 )                          \
        {                                                                     \
            /* Height reduction: skip next source line */                     \
            p_y += i_width;                                                   \
            i_y++;                                                            \
            if( CHROMA == 420 )                                               \
            {                                                                 \
                if( i_y & 0x1 )                                               \
                {                                                             \
                    p_u += i_chroma_width;                                    \
                    p_v += i_chroma_width;                                    \
                }                                                             \
            }                                                                 \
            else if( (CHROMA == 422) || (CHROMA == 444) )                     \
            {                                                                 \
                p_u += i_width;                                               \
                p_v += i_width;                                               \
            }                                                                 \
        }                                                                     \
        i_scale_count += i_height;                                            \
        break;                                                                \
    case 1:                              /* vertical scaling factor is > 1 */ \
        while( (i_scale_count -= i_height) > 0 )                              \
        {                                                                     \
            /* Height increment: copy previous picture line */                \
            for( i_x = i_pic_width / 16; i_x--; )                             \
            {                                                                 \
                *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );           \
                *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );           \
                if( BPP > 1 )                               /* 2, 3, 4 Bpp */ \
                {                                                             \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                }                                                             \
                if( BPP > 2 )                                  /* 3, 4 Bpp */ \
                {                                                             \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                }                                                             \
                if( BPP > 3 )                                     /* 4 Bpp */ \
                {                                                             \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                }                                                             \
            }                                                                 \
            p_pic +=        i_pic_line_width;                                 \
            p_pic_start +=  i_pic_line_width;                                 \
        }                                                                     \
        i_scale_count += i_pic_height;                                        \
        break;                                                                \
    }                                                                         \

