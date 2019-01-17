/*****************************************************************************
 * algo_basic.c : Basic algorithms for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2000-2011 VLC authors and VideoLAN
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Damien Lucas <nitrox@videolan.org>  (Bob, Blend)
 *         Laurent Aimar <fenrir@videolan.org> (Bob, Blend)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdint.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_filter.h>

#include "merge.h"
#include "deinterlace.h" /* definition of p_sys, needed for Merge() */

#include "algo_basic.h"

/*****************************************************************************
 * RenderDiscard: only keep TOP or BOTTOM field, discard the other.
 *****************************************************************************/

int RenderDiscard( filter_t *p_filter, picture_t *p_outpic, picture_t *p_pic )
{
    VLC_UNUSED(p_filter);
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        for( ; p_out < p_out_end ; )
        {
            memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

            p_out += p_outpic->p[i_plane].i_pitch;
            p_in += 2 * p_pic->p[i_plane].i_pitch;
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RenderBob: renders a BOB picture - simple copy
 *****************************************************************************/

int RenderBob( filter_t *p_filter, picture_t *p_outpic, picture_t *p_pic,
               int order, int i_field )
{
    VLC_UNUSED(p_filter);
    VLC_UNUSED(order);
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;
        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        /* For BOTTOM field we need to add the first line */
        if( i_field == 1 )
        {
            memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
        }

        p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

        for( ; p_out < p_out_end ; )
        {
            memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

            p_out += p_outpic->p[i_plane].i_pitch;

            memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

            p_in += 2 * p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
        }

        memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

        /* For TOP field we need to add the last line */
        if( i_field == 0 )
        {
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
            memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RenderLinear: BOB with linear interpolation
 *****************************************************************************/

int RenderLinear( filter_t *p_filter,
                  picture_t *p_outpic, picture_t *p_pic, int order, int i_field )
{
    VLC_UNUSED(order);
    int i_plane;

    filter_sys_t *p_sys = p_filter->p_sys;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;
        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        /* For BOTTOM field we need to add the first line */
        if( i_field == 1 )
        {
            memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
        }

        p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

        for( ; p_out < p_out_end ; )
        {
            memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

            p_out += p_outpic->p[i_plane].i_pitch;

            Merge( p_out, p_in, p_in + 2 * p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_in += 2 * p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
        }

        memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

        /* For TOP field we need to add the last line */
        if( i_field == 0 )
        {
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
            memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
        }
    }
    EndMerge();
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RenderMean: Half-resolution blender
 *****************************************************************************/

int RenderMean( filter_t *p_filter, picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;

    filter_sys_t *p_sys = p_filter->p_sys;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        /* All lines: mean value */
        for( ; p_out < p_out_end ; )
        {
            Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_out += p_outpic->p[i_plane].i_pitch;
            p_in += 2 * p_pic->p[i_plane].i_pitch;
        }
    }
    EndMerge();
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RenderBlend: Full-resolution blender
 *****************************************************************************/

int RenderBlend( filter_t *p_filter, picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;

    filter_sys_t *p_sys = p_filter->p_sys;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        /* First line: simple copy */
        memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
        p_out += p_outpic->p[i_plane].i_pitch;

        /* Remaining lines: mean value */
        for( ; p_out < p_out_end ; )
        {
            Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_out += p_outpic->p[i_plane].i_pitch;
            p_in  += p_pic->p[i_plane].i_pitch;
        }
    }
    EndMerge();
    return VLC_SUCCESS;
}
