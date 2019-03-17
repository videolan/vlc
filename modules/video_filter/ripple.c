/*****************************************************************************
 * ripple.c : Ripple video effect plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
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
#include <vlc_picture.h>
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
    set_capability( "video filter", 0 )
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
typedef struct
{
    double  f_angle;
    vlc_tick_t last_date;
} filter_sys_t;

/*****************************************************************************
 * Create: allocates Distort video thread output method
 *****************************************************************************
 * This function allocates and initializes a Distort vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    const vlc_chroma_description_t *p_chroma =
        vlc_fourcc_GetChromaDescription( p_filter->fmt_in.video.i_chroma );
    if( p_chroma == NULL || p_chroma->plane_count == 0 )
        return VLC_EGENERIC;

    /* Allocate structure */
    filter_sys_t *p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_filter->p_sys = p_sys;
    p_filter->pf_video_filter = Filter;

    p_sys->f_angle = 0.0;
    p_sys->last_date = 0;

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
    double f_angle;
    vlc_tick_t new_date = vlc_tick_now();

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    filter_sys_t *p_sys = p_filter->p_sys;

    p_sys->f_angle -= 10.0f * secf_from_vlc_tick(p_sys->last_date - new_date);
    p_sys->last_date = new_date;
    f_angle = p_sys->f_angle;

    for( int i_index = 0; i_index < p_pic->i_planes; i_index++ )
    {
        int i_first_line, i_visible_pitch, i_num_lines, i_offset, i_pixel_pitch,
            i_visible_pixels;
        uint16_t black_pixel;
        uint8_t *p_in, *p_out;

        black_pixel = ( p_pic->i_planes > 1 && i_index == Y_PLANE ) ? 0x00
                                                                    : 0x80;

        i_num_lines = p_pic->p[i_index].i_visible_lines;
        i_visible_pitch = p_pic->p[i_index].i_visible_pitch;
        i_pixel_pitch = p_pic->p[i_index].i_pixel_pitch;

        switch( p_filter->fmt_in.video.i_chroma )
        {
            CASE_PLANAR_YUV10
                black_pixel = ( p_pic->i_planes > 1 && i_index == Y_PLANE ) ? 0x00
                                                                            : 0x200;
                break;
            CASE_PACKED_YUV_422
                // Quick hack to fix u/v inversion occuring with 2 byte pixel pitch
                i_pixel_pitch *= 2;
                /* fallthrough */
            CASE_PLANAR_YUV
                black_pixel = ( p_pic->i_planes > 1 && i_index == Y_PLANE ) ? 0x00
                                                                            : 0x80;
                break;
            default:
                black_pixel = 0x00;
        }

        i_visible_pixels = i_visible_pitch/i_pixel_pitch;

        i_first_line = i_num_lines * 4 / 5;

        p_in = p_pic->p[i_index].p_pixels;
        p_out = p_outpic->p[i_index].p_pixels;

        for( int i_line = 0; i_line < i_first_line; i_line++ )
        {
            memcpy( p_out, p_in, i_visible_pitch );
            p_in += p_pic->p[i_index].i_pitch;
            p_out += p_outpic->p[i_index].i_pitch;
        }

        /* Ok, we do 3 times the sin() calculation for each line. So what ? */
        for( int i_line = i_first_line; i_line < i_num_lines; i_line++ )
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
                void *p_black_out;
                if( i_offset < 0 )
                {
                    memcpy( p_out, p_in - i_offset,
                                i_visible_pitch + i_offset );
                    p_black_out = &p_out[i_visible_pitch + i_offset];
                    i_offset = -i_offset;
                }
                else
                {
                    memcpy( p_out + i_offset, p_in,
                                i_visible_pitch - i_offset );
                    p_black_out = p_out;
                }
                if (black_pixel > 0xFF)
                {
                    uint16_t *p_out16 = p_black_out;
                    for (int x = 0; x < i_offset; x += 2)
                        *p_out16++ = black_pixel;
                }
                else
                    memset( p_black_out, black_pixel, i_offset );
            }
            else
            {
                memcpy( p_out, p_in, i_visible_pitch );
            }
            p_in -= p_pic->p[i_index].i_pitch;
            p_out += p_outpic->p[i_index].i_pitch;
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}
