/*****************************************************************************
 * croppadd.c: Crop/Padd image filter
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea @t videolan dot org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_vout.h>
#include "vlc_filter.h"

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Video scaling filter") );
    set_capability( "video filter2", 0 );
    set_callbacks( OpenFilter, CloseFilter );
vlc_module_end();

/*****************************************************************************
 * OpenFilter: probe the filter and return score
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        return VLC_EGENERIC;
    }

    p_filter->pf_video_filter = Filter;

    msg_Dbg( p_filter, "%ix%i + %ix%i -> %ix%i + %ix%i",
             p_filter->fmt_in.video.i_visible_width,
             p_filter->fmt_in.video.i_visible_height,
             p_filter->fmt_in.video.i_x_offset,
             p_filter->fmt_in.video.i_y_offset,
             p_filter->fmt_out.video.i_visible_width,
             p_filter->fmt_out.video.i_visible_height,
             p_filter->fmt_out.video.i_x_offset,
             p_filter->fmt_out.video.i_y_offset );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    int i_plane;
    int i_width, i_height, i_xcrop, i_ycrop,
        i_outwidth, i_outheight, i_xpadd, i_ypadd;

    const int p_padd_color[] = { 0x00, 0x80, 0x80, 0xff };

    if( !p_pic ) return NULL;

    /* Request output picture */
    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    /* p_pic and p_outpic have the same chroma/number of planes but that's
     * about it. */
    {
        plane_t *p_plane = p_pic->p+i_plane;
        plane_t *p_outplane = p_outpic->p+i_plane;
        uint8_t *p_in = p_plane->p_pixels;
        uint8_t *p_out = p_outplane->p_pixels;
        int i_pixel_pitch = p_plane->i_pixel_pitch;
        int i_padd_color = i_plane > 3 ? p_padd_color[0]
                                       : p_padd_color[i_plane];

        /* These assignments assume that the first plane always has
         * a width and height equal to the picture's */
        i_width =     ( p_filter->fmt_in.video.i_visible_width
                        * p_plane->i_visible_pitch )
                      / p_pic->p->i_visible_pitch;
        i_height =    ( p_filter->fmt_in.video.i_visible_height
                        * p_plane->i_visible_lines )
                      / p_pic->p->i_visible_lines;
        i_xcrop =     ( p_filter->fmt_in.video.i_x_offset
                        * p_plane->i_visible_pitch)
                      / p_pic->p->i_visible_pitch;
        i_ycrop =     ( p_filter->fmt_in.video.i_y_offset
                        * p_plane->i_visible_lines)
                      / p_pic->p->i_visible_lines;
        i_outwidth =  ( p_filter->fmt_out.video.i_visible_width
                        * p_outplane->i_visible_pitch )
                      / p_outpic->p->i_visible_pitch;
        i_outheight = ( p_filter->fmt_out.video.i_visible_height
                        * p_outplane->i_visible_lines )
                      / p_outpic->p->i_visible_lines;
        i_xpadd =     ( p_filter->fmt_out.video.i_x_offset
                        * p_outplane->i_visible_pitch )
                      / p_outpic->p->i_visible_pitch;
        i_ypadd =     ( p_filter->fmt_out.video.i_y_offset
                         * p_outplane->i_visible_lines )
                       / p_outpic->p->i_visible_lines;

        /* Crop the top */
        p_in += i_ycrop * p_plane->i_pitch;

        /* Padd on the top */
        p_filter->p_libvlc->pf_memset( p_out, i_padd_color,
                                       i_ypadd * p_outplane->i_pitch );
        p_out += i_ypadd * p_outplane->i_pitch;

        int i_line;
        for( i_line = 0; i_line < i_height; i_line++ )
        {
            uint8_t *p_in_next = p_in + p_plane->i_pitch;
            uint8_t *p_out_next = p_out + p_outplane->i_pitch;

            /* Crop on the left */
            p_in += i_xcrop * i_pixel_pitch;

            /* Padd on the left */
            p_filter->p_libvlc->pf_memset( p_out, i_padd_color,
                                           i_xpadd * i_pixel_pitch );
            p_out += i_xpadd * i_pixel_pitch;

            /* Copy the image and crop on the right */
            p_filter->p_libvlc->pf_memcpy( p_out, p_in,
                                           i_width * i_pixel_pitch );
            p_out += i_width * i_pixel_pitch;
            p_in += i_width * i_pixel_pitch;

            /* Padd on the right */
            p_filter->p_libvlc->pf_memset( p_out, i_padd_color,
                                    ( i_outwidth - i_width ) * i_pixel_pitch );

            /* Got to begining of the next line */
            p_in = p_in_next;
            p_out = p_out_next;
        }

        /* Padd on the bottom */
        p_filter->p_libvlc->pf_memset( p_out, i_padd_color,
                 ( i_outheight - i_ypadd - i_height ) * p_outplane->i_pitch );
    }

    p_outpic->date = p_pic->date;
    p_outpic->b_force = p_pic->b_force;
    p_outpic->i_nb_fields = p_pic->i_nb_fields;
    p_outpic->b_progressive = p_pic->b_progressive;
    p_outpic->b_top_field_first = p_pic->b_top_field_first;

    p_pic->pf_release( p_pic );

    return p_outpic;
}
