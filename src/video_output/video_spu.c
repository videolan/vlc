/*****************************************************************************
 * video_spu.c : DVD subpicture units functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_spu.c,v 1.21 2001/05/08 20:38:25 sam Exp $
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
#include "defs.h"

#include <stdio.h>
#include <string.h>                                    /* memcpy(), memset() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "video.h"
#include "video_output.h"
#include "video_spu.h"

/* FIXME: fake palette - the real one has to be sought in the .IFO */
static int p_palette[4] = { 0x0000, 0xffff, 0x5555, 0x8888 };

/*****************************************************************************
 * vout_RenderRGBSPU: draw an SPU on a picture
 *****************************************************************************
 * This is a fast implementation of the subpicture drawing code. The data
 * has been preprocessed once in spu_decoder.c, so we don't need to parse the
 * RLE buffer again and again. Most sanity checks are done in spu_decoder.c
 * so that this routine can be as fast as possible.
 *****************************************************************************/
void   vout_RenderRGBSPU( picture_t *p_pic, const subpicture_t *p_spu,
                          vout_buffer_t *p_buffer,
                          int i_bytes_per_pixel, int i_bytes_per_line )
{
    int  i_len, i_color;
    u16 *p_source = (u16 *)p_spu->p_data;

    int i_xscale = ( p_buffer->i_pic_width << 6 ) / p_pic->i_width;
    int i_yscale = ( p_buffer->i_pic_height << 6 ) / p_pic->i_height;

    int i_width  = p_spu->i_width  * i_xscale;
    int i_height = p_spu->i_height * i_yscale;

    int i_x, i_y, i_ytmp, i_yreal, i_ynext;

    u8 *p_dest = p_buffer->p_data + ( i_width >> 6 ) * i_bytes_per_pixel
                  /* Add the picture coordinates and the SPU coordinates */
                  + ( p_buffer->i_pic_x + ((p_spu->i_x * i_xscale) >> 6))
                       * i_bytes_per_pixel
                  + ( p_buffer->i_pic_y + ((p_spu->i_y * i_yscale) >> 6))
                       * i_bytes_per_line;

    /* Draw until we reach the bottom of the subtitle */
    i_y = 0;

    while( i_y < i_height )
    {
        i_ytmp = i_y >> 6;
        i_y += i_yscale;

        /* Check whether we need to draw one line or more than one */
        if( i_ytmp + 1 >= ( i_y >> 6 ) )
        {
            /* Just one line : we precalculate i_y >> 6 */
            i_yreal = i_bytes_per_line * i_ytmp;

            /* Draw until we reach the end of the line */
	    i_x = i_width;

            while( i_x )
            {
                /* Get the RLE part */
                i_color = *p_source & 0x3;

                /* Draw the line */
                if( i_color )
                {
                    i_len = i_xscale * ( *p_source++ >> 2 );

                    memset( p_dest - i_bytes_per_pixel * ( i_x >> 6 )
                                   + i_yreal,
                            p_palette[ i_color ],
                            i_bytes_per_pixel * ( ( i_len >> 6 ) + 1 ) );

                    i_x -= i_len;
                    continue;
                }

                i_x -= i_xscale * ( *p_source++ >> 2 );
            }
        }
        else
        {
            i_yreal = i_bytes_per_line * i_ytmp;
            i_ynext = i_bytes_per_line * i_y >> 6;

            /* Draw until we reach the end of the line */
            i_x = i_width;

            while( i_x )
            {
                /* Get the RLE part */
                i_color = *p_source & 0x3;

                /* Draw as many lines as needed */
                if( i_color )
                {
                    i_len = i_xscale * ( *p_source++ >> 2 );

                    for( i_ytmp = i_yreal ;
                         i_ytmp < i_ynext ;
                         i_ytmp += i_bytes_per_line )
                    {
                        memset( p_dest - i_bytes_per_pixel * ( i_x >> 6 )
                                       + i_ytmp,
                                p_palette[ i_color ],
                                i_bytes_per_pixel * ( ( i_len >> 6 ) + 1 ) );
                    }

                    i_x -= i_len;
                    continue;
                }

                i_x -= i_xscale * ( *p_source++ >> 2 );
            }
        }
    }
}

/*****************************************************************************
 * vout_RenderYUVSPU: draw an SPU on an YUV overlay
 *****************************************************************************
 * This is a fast implementation of the subpicture drawing code. The data
 * has been preprocessed once in spu_decoder.c, so we don't need to parse the
 * RLE buffer again and again. Most sanity checks are done in spu_decoder.c
 * so that this routine can be as fast as possible.
 *****************************************************************************/
void vout_RenderYUVSPU( picture_t *p_pic, const subpicture_t *p_spu )
{
    int  i_len, i_color;
    u16 *p_source = (u16 *)p_spu->p_data;

    int i_x, i_y;

    u8 *p_dest = p_pic->p_y + p_spu->i_x + p_spu->i_width
                        + p_pic->i_width * ( p_spu->i_y + p_spu->i_height );

    /* Draw until we reach the bottom of the subtitle */
    i_y = p_spu->i_height * p_pic->i_width;

    while( i_y )
    {
        /* Draw until we reach the end of the line */
        i_x = p_spu->i_width;

        while( i_x )
        {
            /* Draw the line if needed */
            i_color = *p_source & 0x3;

            if( i_color )
            {
                i_len = *p_source++ >> 2;
                memset( p_dest - i_x - i_y, p_palette[ i_color ], i_len );
                i_x -= i_len;
                continue;
            }

            i_x -= *p_source++ >> 2;
        }

        i_y -= p_pic->i_width;
    }
}

