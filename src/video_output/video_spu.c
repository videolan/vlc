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

#include <stdio.h>

#include "config.h"
#include "common.h"
#include "video_spu.h"

#include "intf_msg.h"

typedef struct spu_s
{
    int i_id;
    byte_t *p_data;

    int x;
    int y;
    int width;
    int height;

} spu_t;

static int NewLine  ( spu_t *p_spu, int *i_id );
static int PutPixel ( spu_t *p_spu, int len, u8 color );

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
                     int i_x, int i_y, byte_t *p_pic,
                     int i_bytes_per_pixel, int i_bytes_per_line )
{
    int i_code = 0x00;
    int i_next = 0;
    int i_id = 0;
    boolean_t b_aligned = 1;
    byte_t *p_from[2];
    spu_t spu;

    p_from[1] = p_data + p_offset[1];
    p_from[0] = p_data + p_offset[0];

    spu.x = 0;
    spu.y = 0;
    spu.width = 720;
    spu.height = 576;
    spu.p_data = p_pic + i_x * i_bytes_per_pixel + i_y * i_bytes_per_line;

    while( p_from[0] < p_data + p_offset[1] + 2 )
    {
        GET_NIBBLE( i_code );

        if( i_code >= 0x04 )
        {
            found_code:
            if( PutPixel( &spu, i_code >> 2, i_code & 3 ) < 0 )
                return;

            if( spu.x >= spu.width )
            {
                /* byte-align the stream */
                b_aligned = 1;
                /* finish the line */
                NewLine( &spu, &i_id );
            }
            continue;
        }

        ADD_NIBBLE( i_code, (i_code << 4) );
        if( i_code >= 0x10 )       /* 1x .. 3x */
            goto found_code;

        ADD_NIBBLE( i_code, (i_code << 4) );
        if( i_code >= 0x40 )       /* 04x .. 0fx */
            goto found_code;

        ADD_NIBBLE( i_code, (i_code << 4) );
        if( i_code >= 0x100 )      /* 01xx .. 03xx */
            goto found_code;

        /* 00xx - should only happen for 00 00 */
        if( !b_aligned )
        {
            ADD_NIBBLE( i_code, (i_code << 4) );
        }

        if( i_code )
        {
            intf_DbgMsg( "video_spu: unknown code 0x%x "
                         "(dest %x side %x, x=%d, y=%d)\n",
                         i_code, p_from[i_id], i_id, spu.x, spu.y );
            if( NewLine( &spu, &i_id ) < 0 )
                return;
            continue;
        }

        /* aligned 00 00 */
        if( NewLine( &spu, &i_id ) < 0 )
            return;
    }
}

static int NewLine( spu_t *p_spu, int *i_id )
{
    int i_ret = PutPixel( p_spu, p_spu->width - p_spu->x, 0 );

    p_spu->x = 0;
    p_spu->y++;
    *i_id = 1 - *i_id;

    return i_ret;
}

static int PutPixel ( spu_t *p_spu, int i_len, u8 i_color )
{
    //static int p_palette[4] = { 0x0000, 0xfef8, 0x7777, 0xffff };
    static int p_palette[4] = { 0x0000, 0xffff, 0x5555, 0x0000 };

    if( (i_len + p_spu->x + p_spu->y * p_spu->width)
            > p_spu->height * p_spu->width )
    {
        intf_DbgMsg ( "video_spu: trying to draw beyond memory area! %d %d\n",
                      i_len, p_spu->height * p_spu->width
                             - ( i_len + p_spu->x + p_spu->y * p_spu->width) );
        p_spu->x += i_len;
        return -1;
    }
    else
    {

        if( i_color > 0x0f )
            intf_DbgMsg( "video_spu: invalid color\n" );

        if( i_color )
        {
            u8 *p_target
                = &p_spu->p_data[2 * ( p_spu->x + p_spu->y * p_spu->width ) ];

            memset( p_target, p_palette[i_color], 2 * i_len );
        }
        p_spu->x += i_len;
    }

    return 0;
}

