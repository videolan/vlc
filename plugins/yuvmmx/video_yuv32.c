/*****************************************************************************
 * video_yuv32.c: MMX YUV transformation functions for 32 bpp
 * Provides functions to perform the YUV conversion. The functions provided here
 * are a complete and portable C implementation, and may be replaced in certain
 * case by optimized functions.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <math.h>                                            /* exp(), pow() */
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"
#include "video.h"
#include "video_output.h"
#include "video_yuv.h"
#include "video_yuv_macros.h"
#include "video_yuv_asm.h"

#include "intf_msg.h"

/*****************************************************************************
 * ConvertY4Gray32: grayscale YUV 4:x:x to RGB 4 Bpp
 *****************************************************************************/
void ConvertY4Gray32( YUV_ARGS_32BPP )
{
    intf_ErrMsg( "yuvmmx error: unhandled function, grayscale, bpp = 32\n" );
}

/*****************************************************************************
 * ConvertYUV420RGB32: color YUV 4:2:0 to RGB 4 Bpp
 *****************************************************************************/
void ConvertYUV420RGB32( YUV_ARGS_32BPP )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width;                              /* chroma width */
    u32 *       p_pic_start;       /* beginning of the current line for copy */
    u32 *       p_buffer_start;                   /* conversion buffer start */
    u32 *       p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */

    /*
     * Initialize some values  - i_pic_line_width will store the line skip
     */
    i_pic_line_width -= i_pic_width;
    i_chroma_width =    i_width / 2;
    p_buffer_start =    p_vout->yuv.p_buffer;
    p_offset_start =    p_vout->yuv.p_offset;
    SetOffset( i_width, i_height, i_pic_width, i_pic_height,
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start, 0 );

    /*
     * Perform conversion
     */
    i_scale_count = ( i_vertical_scaling == 1 ) ? i_pic_height : i_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;

        for ( i_x = i_width / 8; i_x--; )
        {
            __asm__( ".align 8"
                     MMX_INIT_32
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
        }

        SCALE_WIDTH;
        SCALE_HEIGHT( 420, 4 );
    }
}

/*****************************************************************************
 * ConvertYUV422RGB32: color YUV 4:2:2 to RGB 4 Bpp
 *****************************************************************************/
void ConvertYUV422RGB32( YUV_ARGS_32BPP )
{
    intf_ErrMsg( "yuv error: unhandled function, chroma = 422, bpp = 32\n" );
}

/*****************************************************************************
 * ConvertYUV444RGB32: color YUV 4:4:4 to RGB 4 Bpp
 *****************************************************************************/
void ConvertYUV444RGB32( YUV_ARGS_32BPP )
{
    intf_ErrMsg( "yuv error: unhandled function, chroma = 444, bpp = 32\n" );
}

