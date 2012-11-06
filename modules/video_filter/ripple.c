/*****************************************************************************
 * ripple.c : Ripple video effect plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Antoine Cellerier <dionoea -at- videolan -dot- org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>                                            /* sin(), cos() */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Ripple video filter") )
    set_shortname( N_( "Ripple" ))
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_shortcut( "ripple" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * filter_sys_t: Distort video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Distort specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    double  f_angle;
    mtime_t last_date;
};

/*****************************************************************************
 * Create: allocates Distort video thread output method
 *****************************************************************************
 * This function allocates and initializes a Distort vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_filter->pf_video_filter = Filter;

    p_filter->p_sys->f_angle = 0.0;
    p_filter->p_sys->last_date = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Distort video thread output method
 *****************************************************************************
 * Terminate an output method created by DistortCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    free( p_filter->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Distort image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    int i_index;
    double f_angle;
    mtime_t new_date = mdate();

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    p_filter->p_sys->f_angle -= (p_filter->p_sys->last_date - new_date) / 100000.0;
    p_filter->p_sys->last_date = new_date;
    f_angle = p_filter->p_sys->f_angle;

    for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
    {
        int i_line, i_first_line, i_num_lines, i_offset, i_pixel_pitch,
            i_visible_pixels;
        uint8_t black_pixel;
        uint8_t *p_in, *p_out;

        black_pixel = ( p_pic->i_planes > 1 && i_index == Y_PLANE ) ? 0x00
                                                                    : 0x80;

        i_num_lines = p_pic->p[i_index].i_visible_lines;
        i_pixel_pitch = p_pic->p[i_index].i_pixel_pitch;
        switch( p_filter->fmt_in.video.i_chroma )
        {
            CASE_PACKED_YUV_422
                // Quick hack to fix u/v inversion occuring with 2 byte pixel pitch
                i_pixel_pitch *= 2;
                break;
        }

        i_visible_pixels = p_pic->p[i_index].i_visible_pitch/i_pixel_pitch;

        i_first_line = i_num_lines * 4 / 5;

        p_in = p_pic->p[i_index].p_pixels;
        p_out = p_outpic->p[i_index].p_pixels;

        for( i_line = 0 ; i_line < i_first_line ; i_line++ )
        {
            memcpy( p_out, p_in, p_pic->p[i_index].i_visible_pitch );
            p_in += p_pic->p[i_index].i_pitch;
            p_out += p_outpic->p[i_index].i_pitch;
        }

        /* Ok, we do 3 times the sin() calculation for each line. So what ? */
        for( i_line = i_first_line ; i_line < i_num_lines ; i_line++ )
        {
            /* Calculate today's offset, don't go above 1/20th of the screen */
            i_offset = (int)( (double)(i_visible_pixels)
                         * sin( f_angle + 2.0 * (double)i_line
                                              / (double)( 1 + i_line
                                                            - i_first_line) )
                         * (double)(i_line - i_first_line)
                         / (double)i_num_lines
                         / 8.0 )*i_pixel_pitch;

            if( i_offset )
            {
                if( i_offset < 0 )
                {
                    memcpy( p_out, p_in - i_offset,
                                p_pic->p[i_index].i_visible_pitch + i_offset );
                    p_in -= p_pic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch;
                    memset( p_out + i_offset, black_pixel, -i_offset );
                }
                else
                {
                    memcpy( p_out + i_offset, p_in,
                                p_pic->p[i_index].i_visible_pitch - i_offset );
                    memset( p_out, black_pixel, i_offset );
                    p_in -= p_pic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch;
                }
            }
            else
            {
                memcpy( p_out, p_in, p_pic->p[i_index].i_visible_pitch );
                p_in -= p_pic->p[i_index].i_pitch;
                p_out += p_outpic->p[i_index].i_pitch;
            }

        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}
