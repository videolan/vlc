/*****************************************************************************
 * transforms_yuv.h: C specific YUV transformation macros
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: transforms_yuv.h,v 1.3 2001/10/11 13:19:27 massiot Exp $
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * CONVERT_4YUV_PIXELS: dither 4 pixels in 8 bpp
 *****************************************************************************
 * These macros dither 4 pixels in 8 bpp
 *****************************************************************************/
#define CONVERT_4YUV_PIXELS( CHROMA )                                         \
    *p_pic++ = p_lookup[                                                      \
        (((*p_y++ + dither10[i_real_y]) >> 4) << 7)                           \
      + ((*p_u + dither20[i_real_y]) >> 5) * 9                                \
      + ((*p_v + dither20[i_real_y]) >> 5) ];                                 \
    *p_pic++ = p_lookup[                                                      \
        (((*p_y++ + dither11[i_real_y]) >> 4) << 7)                           \
      + ((*p_u++ + dither21[i_real_y]) >> 5) * 9                              \
      + ((*p_v++ + dither21[i_real_y]) >> 5) ];                               \
    *p_pic++ = p_lookup[                                                      \
        (((*p_y++ + dither12[i_real_y]) >> 4) << 7)                           \
      + ((*p_u + dither22[i_real_y]) >> 5) * 9                                \
      + ((*p_v + dither22[i_real_y]) >> 5) ];                                 \
    *p_pic++ = p_lookup[                                                      \
        (((*p_y++ + dither13[i_real_y]) >> 4) << 7)                           \
      + ((*p_u++ + dither23[i_real_y]) >> 5) * 9                              \
      + ((*p_v++ + dither23[i_real_y]) >> 5) ];                               \

/*****************************************************************************
 * CONVERT_4YUV_PIXELS_SCALE: dither and scale 4 pixels in 8 bpp
 *****************************************************************************
 * These macros dither 4 pixels in 8 bpp, with horizontal scaling
 *****************************************************************************/
#define CONVERT_4YUV_PIXELS_SCALE( CHROMA )                                   \
    *p_pic++ = p_lookup[                                                      \
        ( ((*p_y + dither10[i_real_y]) >> 4) << 7)                            \
        + ((*p_u + dither20[i_real_y]) >> 5) * 9                              \
        + ((*p_v + dither20[i_real_y]) >> 5) ];                               \
    p_y += *p_offset++;                                                       \
    p_u += *p_offset;                                                         \
    p_v += *p_offset++;                                                       \
    *p_pic++ = p_lookup[                                                      \
        ( ((*p_y + dither11[i_real_y]) >> 4) << 7)                            \
        + ((*p_u + dither21[i_real_y]) >> 5) * 9                              \
        + ((*p_v + dither21[i_real_y]) >> 5) ];                               \
    p_y += *p_offset++;                                                       \
    p_u += *p_offset;                                                         \
    p_v += *p_offset++;                                                       \
    *p_pic++ = p_lookup[                                                      \
        ( ((*p_y + dither12[i_real_y]) >> 4) << 7)                            \
        + ((*p_u + dither22[i_real_y]) >> 5) * 9                              \
        + ((*p_v + dither22[i_real_y]) >> 5) ];                               \
    p_y += *p_offset++;                                                       \
    p_u += *p_offset;                                                         \
    p_v += *p_offset++;                                                       \
    *p_pic++ = p_lookup[                                                      \
        ( ((*p_y + dither13[i_real_y]) >> 4) << 7)                            \
        + ((*p_u + dither23[i_real_y]) >> 5) * 9                              \
        + ((*p_v + dither23[i_real_y]) >> 5) ];                               \
    p_y += *p_offset++;                                                       \
    p_u += *p_offset;                                                         \
    p_v += *p_offset++;                                                       \

/*****************************************************************************
 * SCALE_WIDTH_DITHER: scale a line horizontally for dithered 8 bpp
 *****************************************************************************
 * This macro scales a line using an offset array.
 *****************************************************************************/
#define SCALE_WIDTH_DITHER( CHROMA )                                          \
    if( b_horizontal_scaling )                                                \
    {                                                                         \
        /* Horizontal scaling - we can't use a buffer due to dithering */     \
        p_offset = p_offset_start;                                            \
        for( i_x = i_pic_width / 16; i_x--; )                                 \
        {                                                                     \
            CONVERT_4YUV_PIXELS_SCALE( CHROMA )                               \
            CONVERT_4YUV_PIXELS_SCALE( CHROMA )                               \
            CONVERT_4YUV_PIXELS_SCALE( CHROMA )                               \
            CONVERT_4YUV_PIXELS_SCALE( CHROMA )                               \
        }                                                                     \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        for( i_x = i_width / 16; i_x--;  )                                    \
        {                                                                     \
            CONVERT_4YUV_PIXELS( CHROMA )                                     \
            CONVERT_4YUV_PIXELS( CHROMA )                                     \
            CONVERT_4YUV_PIXELS( CHROMA )                                     \
            CONVERT_4YUV_PIXELS( CHROMA )                                     \
        }                                                                     \
    }                                                                         \
    /* Increment of picture pointer to end of line is still needed */         \
    p_pic += i_pic_line_width;                                                \
                                                                              \
    /* Increment the Y coordinate in the matrix, modulo 4 */                  \
    i_real_y = (i_real_y + 1) & 0x3;                                          \

/*****************************************************************************
 * SCALE_HEIGHT_DITHER: handle vertical scaling for dithered 8 bpp
 *****************************************************************************
 * This macro handles vertical scaling for a picture. CHROMA may be 420,
 * 422 or 444 for RGB conversion, or 400 for gray conversion.
 *****************************************************************************/
#define SCALE_HEIGHT_DITHER( CHROMA )                                         \
                                                                              \
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
                                                                              \
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
            p_y -= i_width;                                                   \
            p_u -= i_chroma_width;                                            \
            p_v -= i_chroma_width;                                            \
            SCALE_WIDTH_DITHER( CHROMA );                                     \
        }                                                                     \
        i_scale_count += i_pic_height;                                        \
        break;                                                                \
    }                                                                         \

