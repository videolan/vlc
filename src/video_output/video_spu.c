/*****************************************************************************
 * video_spu.h : DVD subpicture units functions
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
#include "video_spu.h"

#include "intf_msg.h"

typedef struct vout_spu_s
{
    int i_id;
    byte_t *p_data;

    int x;
    int y;
    int width;
    int height;

} vout_spu_t;

static int NewLine  ( vout_spu_t *p_vspu, int *i_id );

/* i = get_nibble(); */
#define GET_NIBBLE( i ) \
    if( b_aligned ) \
    { \
        i_next = *p_from[i_id]; \
        /*printf("%.1x", i_next >> 4);*/ \
        p_from[ i_id ]++; \
        b_aligned = 0; \
        i = i_next >> 4; \
    } \
    else \
    { \
        b_aligned = 1; \
        /*printf("%.1x", i_next & 0xf);*/ \
        i = i_next & 0xf; \
    }

/* i = j + get_nibble(); */
#define ADD_NIBBLE( i, j ) \
    if( b_aligned ) \
    { \
        i_next = *p_from[i_id]; \
        /*printf("%.1x", i_next >> 4);*/ \
        p_from[ i_id ]++; \
        b_aligned = 0; \
        i = (j) + (i_next >> 4); \
    } \
    else \
    { \
        b_aligned = 1; \
        /*printf("%.1x", i_next & 0xf);*/ \
        i = (j) + (i_next & 0xf); \
    }

/*****************************************************************************
 * vout_RenderSPU: draws an SPU on a picture
 *****************************************************************************
 * 
 *****************************************************************************/
void vout_RenderSPU( byte_t *p_data, int p_offset[2],
                     subpicture_t *p_subpic, byte_t *p_pic,
                     int i_bytes_per_pixel, int i_bytes_per_line )
{
    int i_code = 0x00;
    int i_next = 0;
    int i_id = 0;
    int i_color;

    /* fake palette - the real one has to be sought in the .IFO */
    static int p_palette[4] = { 0x0000, 0xffff, 0x5555, 0x0000 };

    boolean_t b_aligned = 1;
    byte_t *p_from[2];
    vout_spu_t vspu;

    p_from[1] = p_data + p_offset[1];
    p_from[0] = p_data + p_offset[0];

    vspu.x = 0;
    vspu.y = 0;
    vspu.width = 720;
    vspu.height = 576;
    vspu.p_data = p_pic + p_subpic->i_x * i_bytes_per_pixel + p_subpic->i_y * i_bytes_per_line;

    while( p_from[0] < p_data + p_offset[1] )
    {
        GET_NIBBLE( i_code );

        if( i_code >= 0x04 )
        {
            found_code:

            if( ((i_code >> 2) + vspu.x + vspu.y * vspu.width)
                    > vspu.height * vspu.width )
            {
                intf_DbgMsg ( "video_spu: invalid draw request ! %d %d\n",
                              i_code >> 2, vspu.height * vspu.width
                               - ( (i_code >> 2) + vspu.x
                                   + vspu.y * vspu.width ) );
                return;
            }
            else
            {
                if( (i_color = i_code & 0x3) )
                {
                    u8 *p_target = &vspu.p_data[ 2 * 
                                    ( vspu.x + vspu.y * vspu.width ) ];
                    memset( p_target, p_palette[i_color], 2 * (i_code >> 2) );
                }
                vspu.x += i_code >> 2;
            }

            if( vspu.x >= vspu.width )
            {
                /* byte-align the stream */
                b_aligned = 1;
                /* finish the line */
                NewLine( &vspu, &i_id );
            }
            continue;
        }

        ADD_NIBBLE( i_code, (i_code << 4) );
        if( i_code >= 0x10 )   /* 00 11 xx cc */
            goto found_code;   /* 00 01 xx cc */

        ADD_NIBBLE( i_code, (i_code << 4) );
        if( i_code >= 0x040 )  /* 00 00 11 xx xx cc */
            goto found_code;   /* 00 00 01 xx xx cc */

        ADD_NIBBLE( i_code, (i_code << 4) );
        if( i_code >= 0x0100 ) /* 00 00 00 11 xx xx xx cc */
            goto found_code;   /* 00 00 00 01 xx xx xx cc */

        /* if the 14 first bits are 0, then it's a newline */
        if( i_code <= 0x0003 )
        {
            if( NewLine( &vspu, &i_id ) < 0 )
                return;

            if( !b_aligned )
                b_aligned = 1;
        }
        else
        {
            /* we have a boo boo ! */
            intf_DbgMsg( "video_spu: unknown code 0x%x "
                         "(dest %x side %x, x=%d, y=%d)\n",
                         i_code, p_from[i_id], i_id, vspu.x, vspu.y );
            if( NewLine( &vspu, &i_id ) < 0 )
                return;
            continue;
        }
    }
}

static int NewLine( vout_spu_t *p_vspu, int *i_id )
{
    *i_id = 1 - *i_id;

    p_vspu->x = 0;
    p_vspu->y++;

    return( p_vspu->width - p_vspu->y );

}

