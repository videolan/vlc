/*****************************************************************************
 * video_spu.c : DVD subpicture units functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Henri Fallon <henri@via.ecp.fr>
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

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "video.h"
#include "video_output.h"
#include "video_spu.h"

#include "intf_msg.h"

/* FIXME: fake palette - the real one has to be sought in the .IFO */
static int p_palette[4] = { 0x0000, 0xffff, 0x5555, 0x8888 };

static __inline__ u8 GetNibble( u8 *p_source, int *pi_index )
{
    if( *pi_index & 0x1 )
    {
        return( p_source[(*pi_index)++ >> 1] & 0xf );
    }
    else
    {
        return( p_source[(*pi_index)++ >> 1] >> 4 );
    }
}

/*****************************************************************************
 * vout_RenderSPU: draw an SPU on a picture
 *****************************************************************************
 * 
 *****************************************************************************/
void vout_RenderSPU( vout_buffer_t *p_buffer, subpicture_t *p_spu,
                     int i_bytes_per_pixel, int i_bytes_per_line )
{
    int i_code = 0x00;
    int i_id = 0;
    int i_color;

    /* SPU size */
    int i_width = p_spu->i_width;
    int i_height = p_spu->i_height;

    /* Drawing coordinates inside the SPU */
    int i_x = 0, i_y = 0;

    /* FIXME: we need a way to get this information from the stream */
    #define TARGET_WIDTH     720
    #define TARGET_HEIGHT    576
    int i_xscale = ( p_buffer->i_pic_width << 6 ) / TARGET_WIDTH;
    int i_yscale = ( p_buffer->i_pic_height << 6 ) / TARGET_HEIGHT;

    u8 *p_source = p_spu->p_data;
    u8 *p_dest;
    int pi_index[2];

    pi_index[0] = ( p_spu->type.spu.i_offset[0] - 2 ) << 1;
    pi_index[1] = ( p_spu->type.spu.i_offset[1] - 2 ) << 1;

    p_dest = p_buffer->p_data
                /* add the picture coordinates and the SPU coordinates */
                + ( p_buffer->i_pic_x + ((p_spu->i_x * i_xscale) >> 6))
                     * i_bytes_per_pixel
                + ( p_buffer->i_pic_y + ((p_spu->i_y * i_yscale) >> 6))
                     * i_bytes_per_line;

    while( pi_index[0] >> 1 < p_spu->type.spu.i_offset[1] )
    {
        i_code = GetNibble( p_source, pi_index + i_id );

        if( i_code >= 0x04 )
        {
            found_code:

            if( ((i_code >> 2) + i_x + i_y * i_width) > i_height * i_width )
            {
                intf_DbgMsg ( "video_spu: invalid draw request ! %d %d",
                              i_code >> 2, i_height * i_width
                               - ( (i_code >> 2) + i_x + i_y * i_width ) );
                return;
            }
            else
            {
                if( (i_color = i_code & 0x3) )
                {
                    u8 *p_target = p_dest
                        + i_bytes_per_pixel * ((i_x * i_xscale) >> 6)
                        + i_bytes_per_line * ((i_y * i_yscale) >> 6);

                    memset( p_target, p_palette[i_color],
                            ((((i_code >> 2) * i_xscale) >> 6) + 1)
                            * i_bytes_per_pixel );
                }
                i_x += i_code >> 2;
            }

            if( i_x >= i_width )
            {
                /* byte-align the stream */
                if( pi_index[i_id] & 0x1 )
                {
                    pi_index[i_id]++;
                }

                i_id = ~i_id & 0x1;

                i_y++;
                i_x = 0;

                if( i_width <= i_y )
                {
                    return;
                }
            }
            continue;
        }

        i_code = ( i_code << 4 ) + GetNibble( p_source, pi_index + i_id );

        if( i_code >= 0x10 )   /* 00 11 xx cc */
        {
            goto found_code;   /* 00 01 xx cc */
        }

        i_code = ( i_code << 4 ) + GetNibble( p_source, pi_index + i_id );
        if( i_code >= 0x040 )  /* 00 00 11 xx xx cc */
        {
            goto found_code;   /* 00 00 01 xx xx cc */
        }

        i_code = ( i_code << 4 ) + GetNibble( p_source, pi_index + i_id );
        if( i_code >= 0x0100 ) /* 00 00 00 11 xx xx xx cc */
        {
            goto found_code;   /* 00 00 00 01 xx xx xx cc */
        }

        if( i_code & ~0x0003 )
        {
            /* we have a boo boo ! */
            intf_ErrMsg( "video_spu: unknown code 0x%x "
                         "(dest %x side %x, x=%d, y=%d)",
                         i_code, p_source, i_id, i_x, i_y );
            return;
        }
        else
        {
            /* if the 14 first bits are 0, then it's a new line */
            if( pi_index[i_id] & 0x1 )
            {
                pi_index[i_id]++;
            }

            i_id = ~i_id & 0x1;

            i_y++;
            i_x = 0;

            if( i_width <= i_y )
            {
                return;
            }
        }
    }
}

