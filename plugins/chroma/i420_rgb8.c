/*****************************************************************************
 * i420_rgb8.c : YUV to bitmap RGB conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: i420_rgb8.c,v 1.2 2002/01/12 01:25:57 sam Exp $
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
#include "i420_rgb_c.h"

static void SetOffset( int, int, int, int, boolean_t *, int *, int * );

/*****************************************************************************
 * I420_RGB8: color YUV 4:2:0 to RGB 8 bpp
 *****************************************************************************/
void _M( I420_RGB8 )( vout_thread_t *p_vout, picture_t *p_source,
                                             picture_t *p_dest )
{
    /* We got this one from the old arguments */
    u16 *p_pic = (u16*)p_dest->p->p_pixels;
    u8 *p_y = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS, *p_v = p_source->V_PIXELS;

    /* FIXME: add margins here */
    int i_pic_line_width = p_dest->p->i_pitch / 2;

    boolean_t   b_hscale;             /* horizontal scaling type */
    int         i_vscale;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width;                              /* chroma width */
    u16 *       p_pic_start;       /* beginning of the current line for copy */
    u16 *       p_buffer_start;                   /* conversion buffer start */
    u16 *       p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */

    /*
     * Initialize some values  - i_pic_line_width will store the line skip
     */
    i_pic_line_width -= p_vout->output.i_width;
    i_chroma_width = p_vout->render.i_width / 2;
    p_buffer_start = (u16*)p_vout->chroma.p_sys->p_buffer;
    p_offset_start = p_vout->chroma.p_sys->p_offset;
    SetOffset( p_vout->render.i_width, p_vout->render.i_height,
               p_vout->output.i_width, p_vout->output.i_height,
               &b_hscale, &i_vscale, p_offset_start );

    /* FIXME */
#warning "Please ignore the following warnings, I already know about them"
    intf_ErrMsg( "vout error: I420_RGB8 unimplemented, "
                 "please harass sam@zoy.org" );
}

/* Following functions are local */

/*****************************************************************************
 * SetOffset: build offset array for conversion functions
 *****************************************************************************
 * This function will build an offset array used in later conversion functions.
 * It will also set horizontal and vertical scaling indicators. If b_double
 * is set, the p_offset structure has interleaved Y and U/V offsets.
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
        int i_dummy = 0;

        /* Prepare scaling array for horizontal extension */
        *pb_hscale = 1;
        i_scale_count = i_pic_width;
        for( i_x = i_width; i_x--; )
        {
            while( (i_scale_count -= i_width) > 0 )
            {
                *p_offset++ = 0;
                *p_offset++ = 0;
            }
            *p_offset++ = 1;
            *p_offset++ = i_dummy;
            i_dummy = 1 - i_dummy;
            i_scale_count += i_pic_width;
        }
    }
    else /* if( i_pic_width - i_width < 0 ) */
    {
        int i_remainder = 0;
        int i_jump;

        /* Prepare scaling array for horizontal reduction */
        *pb_hscale = 1;
        i_scale_count = i_width;
        for( i_x = i_pic_width; i_x--; )
        {
            i_jump = 1;
            while( (i_scale_count -= i_pic_width) > 0 )
            {
                i_jump += 1;
            }
            *p_offset++ = i_jump;
            *p_offset++ = ( i_jump += i_remainder ) >> 1;
            i_remainder = i_jump & 1;
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

