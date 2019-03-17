/*****************************************************************************
 * magnify.c : Magnify/Zoom interactive effect
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <math.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_image.h>
#include <vlc_filter.h>
#include <vlc_mouse.h>
#include <vlc_picture.h>
#include "filter_picture.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Magnify/Zoom interactive video filter") )
    set_shortname( N_( "Magnify" ))
    set_capability( "video filter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    set_callbacks( Create, Destroy )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_t *Filter( filter_t *, picture_t * );
static int Mouse( filter_t *, vlc_mouse_t *, const vlc_mouse_t *, const vlc_mouse_t * );

/* */
static void DrawZoomStatus( uint8_t *, int i_pitch, int i_width, int i_height,
                            int i_offset_x, int i_offset_y, bool b_visible );
static void DrawRectangle( uint8_t *, int i_pitch, int i_width, int i_height,
                           int x, int y, int i_w, int i_h );

/* */
typedef struct
{
    image_handler_t *p_image;

    vlc_tick_t i_hide_timeout;

    int i_zoom; /* zoom level in percent */
    int i_x, i_y; /* top left corner coordinates in original image */

    bool b_visible; /* is "interface" visible ? */

    vlc_tick_t i_last_activity;
} filter_sys_t;

#define VIS_ZOOM 4
#define ZOOM_FACTOR 8

/*****************************************************************************
 * Create:
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* */
    switch( p_filter->fmt_in.i_codec )
    {
    CASE_PLANAR_YUV
    case VLC_CODEC_GREY:
        break;
    default:
        msg_Err( p_filter, "Unsupported chroma %4.4s", (char *)&p_filter->fmt_in.i_codec );
        return VLC_EGENERIC;
    }
    if( !es_format_IsSimilar( &p_filter->fmt_in, &p_filter->fmt_out ) )
    {
        msg_Err( p_filter, "Input and output format does not match" );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = p_sys = malloc( sizeof( *p_sys ) );
    if( !p_filter->p_sys )
        return VLC_ENOMEM;

    p_sys->p_image = image_HandlerCreate( p_filter );
    if( !p_sys->p_image )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->i_x = 0;
    p_sys->i_y = 0;
    p_sys->i_zoom = 2*ZOOM_FACTOR;
    p_sys->b_visible = true;
    p_sys->i_last_activity = vlc_tick_now();
    p_sys->i_hide_timeout = VLC_TICK_FROM_MS( var_InheritInteger( p_filter, "mouse-hide-timeout" ) );

    /* */
    p_filter->pf_video_filter = Filter;
    p_filter->pf_video_mouse = Mouse;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy:
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    image_HandlerDelete( p_sys->p_image );

    free( p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    picture_t *p_outpic;

    int v_w, v_h;
    picture_t *p_converted;
    plane_t *p_oyp;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    /* */
    const bool b_visible = p_sys->b_visible;
    const int o_x = p_sys->i_x;
    const int o_y = p_sys->i_y;
    const int o_zoom = p_sys->i_zoom;

    /* background magnified image */
    if( o_zoom != ZOOM_FACTOR )
    {
        video_format_t fmt_in;
        video_format_t fmt_out;
        plane_t orig_planes[PICTURE_PLANE_MAX];
        memcpy(orig_planes, p_pic->p, sizeof orig_planes);

        for( int i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            const int o_yp = o_y * p_outpic->p[i_plane].i_visible_lines / p_outpic->p[Y_PLANE].i_visible_lines;
            const int o_xp = o_x * p_outpic->p[i_plane].i_visible_pitch / p_outpic->p[Y_PLANE].i_visible_pitch;

            p_pic->p[i_plane].p_pixels += o_yp * p_pic->p[i_plane].i_pitch + o_xp;
        }

        /* */
        fmt_in = p_filter->fmt_in.video;
        fmt_in.i_width  = fmt_in.i_visible_width  = (fmt_in.i_visible_width  * ZOOM_FACTOR / o_zoom) & ~1;
        fmt_in.i_height = fmt_in.i_visible_height = (fmt_in.i_visible_height * ZOOM_FACTOR / o_zoom) & ~1;

        /* */
        fmt_out = p_filter->fmt_out.video;
        p_converted = image_Convert( p_sys->p_image, p_pic, &fmt_in, &fmt_out );
        memcpy(p_pic->p, orig_planes, sizeof orig_planes);

        picture_CopyPixels( p_outpic, p_converted );

        picture_Release( p_converted );
    }
    else
    {
        picture_CopyPixels( p_outpic, p_pic );
    }

    /* zoom area selector */
    p_oyp = &p_outpic->p[Y_PLANE];
    if( b_visible )
    {
        video_format_t fmt_out;

        /* image visualization */
        fmt_out = p_filter->fmt_out.video;
        fmt_out.i_width  = fmt_out.i_visible_width  = (fmt_out.i_visible_width /VIS_ZOOM) & ~1;
        fmt_out.i_height = fmt_out.i_visible_height = (fmt_out.i_visible_height/VIS_ZOOM) & ~1;
        p_converted = image_Convert( p_sys->p_image, p_pic,
                                     &p_pic->format, &fmt_out );

        /* It will put only what can be copied at the top left */
        picture_CopyPixels( p_outpic, p_converted );

        picture_Release( p_converted );

        /* white rectangle on visualization */
        v_w = __MIN( fmt_out.i_visible_width  * ZOOM_FACTOR / o_zoom, fmt_out.i_visible_width - 1 );
        v_h = __MIN( fmt_out.i_visible_height * ZOOM_FACTOR / o_zoom, fmt_out.i_visible_height - 1 );

        DrawRectangle( p_oyp->p_pixels, p_oyp->i_pitch,
                       p_oyp->i_visible_pitch, p_oyp->i_visible_lines,
                       o_x/VIS_ZOOM, o_y/VIS_ZOOM,
                       v_w, v_h );

        /* */
        v_h = fmt_out.i_visible_height + 1;
    }
    else
    {
        v_h = 1;
    }

    /* print a small hide/show toggle text control */

    if( b_visible || p_sys->i_last_activity + p_sys->i_hide_timeout > vlc_tick_now() )
        DrawZoomStatus( p_oyp->p_pixels, p_oyp->i_pitch, p_oyp->i_visible_pitch, p_oyp->i_lines,
                        1, v_h, b_visible );

    /* zoom gauge */
    if( b_visible )
    {
        memset( p_oyp->p_pixels + (v_h+9)*p_oyp->i_pitch, 0xff, 41 );
        for( int y = v_h + 10; y < v_h + 90; y++ )
        {
            int i_width = v_h + 90 - y;
            i_width = i_width * i_width / 160;
            if( (80 - y + v_h)*ZOOM_FACTOR/10 < o_zoom )
            {
                memset( p_oyp->p_pixels + y*p_oyp->i_pitch, 0xff, i_width );
            }
            else
            {
                p_oyp->p_pixels[y*p_oyp->i_pitch] = 0xff;
                p_oyp->p_pixels[y*p_oyp->i_pitch + i_width - 1] = 0xff;
            }
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

static void DrawZoomStatus( uint8_t *pb_dst, int i_pitch, int i_width, int i_height,
                            int i_offset_x, int i_offset_y, bool b_visible )
{
    static const char *p_hide =
        "X   X XXXXX XXXX  XXXXX   XXXXX  XXX   XXX  XX XXL"
        "X   X   X   X   X X          X  X   X X   X X X XL"
        "XXXXX   X   X   X XXXX      X   X   X X   X X   XL"
        "X   X   X   X   X X        X    X   X X   X X   XL"
        "X   X XXXXX XXXX  XXXXX   XXXXX  XXX   XXX  X   XL";
    static const char *p_show =
        " XXXX X   X  XXX  X   X   XXXXX  XXX   XXX  XX XXL"
        "X     X   X X   X X   X      X  X   X X   X X X XL"
        " XXX  XXXXX X   X X X X     X   X   X X   X X   XL"
        "    X X   X X   X X X X    X    X   X X   X X   XL"
        "XXXX  X   X  XXX   X X    XXXXX  XXX   XXX  X   XL";
    const char *p_draw = b_visible ? p_hide : p_show;

    for( int i = 0, x = i_offset_x, y = i_offset_y; p_draw[i] != '\0'; i++ )
    {
        if( p_draw[i] == 'X' )
        {
            if( x < i_width && y < i_height )
                pb_dst[y*i_pitch + x] = 0xff;
            x++;
        }
        else if( p_draw[i] == ' ' )
        {
            x++;
        }
        else if( p_draw[i] == 'L' )
        {
            x = i_offset_x;
            y++;
        }
    }
}
static void DrawRectangle( uint8_t *pb_dst, int i_pitch, int i_width, int i_height,
                           int x, int y, int i_w, int i_h )
{
    if( x + i_w > i_width || y + i_h > i_height )
        return;

    /* top line */
    memset( &pb_dst[y * i_pitch + x], 0xff, i_w );

    /* left and right */
    for( int dy = 1; dy < i_h-1; dy++ )
    {
        pb_dst[(y+dy) * i_pitch + x +     0] = 0xff;
        pb_dst[(y+dy) * i_pitch + x + i_w-1] = 0xff;
    }

    /* bottom line */
    memset( &pb_dst[(y+i_h-1) * i_pitch + x], 0xff, i_w );
}

static int Mouse( filter_t *p_filter, vlc_mouse_t *p_mouse, const vlc_mouse_t *p_old, const vlc_mouse_t *p_new )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    const video_format_t *p_fmt = &p_filter->fmt_in.video;

    /* */
    const bool b_click = vlc_mouse_HasPressed( p_old, p_new, MOUSE_BUTTON_LEFT );
    const bool b_pressed = vlc_mouse_IsLeftPressed( p_new );

    bool b_grab = false;

    /* Find the mouse position */
    if( p_sys->b_visible )
    {
        const int i_visu_width  = p_fmt->i_visible_width  / VIS_ZOOM;
        const int i_visu_height = p_fmt->i_visible_height / VIS_ZOOM;

        if( p_new->i_x >= 0 && p_new->i_x < i_visu_width &&
            p_new->i_y >= 0 && p_new->i_y < i_visu_height )
        {
            /* Visualization */
            if( b_pressed )
            {
                const int v_w = p_fmt->i_visible_width  * ZOOM_FACTOR / p_sys->i_zoom;
                const int v_h = p_fmt->i_visible_height * ZOOM_FACTOR / p_sys->i_zoom;

                p_sys->i_x = VLC_CLIP( p_new->i_x * VIS_ZOOM - v_w/2, 0,
                                           (int)p_fmt->i_visible_width  - v_w - 1);
                p_sys->i_y = VLC_CLIP( p_new->i_y * VIS_ZOOM - v_h/2, 0,
                                           (int)p_fmt->i_visible_height - v_h - 1);

                b_grab = true;
            }
        }
        else if( p_new->i_x >= 0 && p_new->i_x < 80 &&
                 p_new->i_y >= i_visu_height &&
                 p_new->i_y <  i_visu_height + 9 )
        {
            /* Hide text */
            if( b_click )
            {
                p_sys->b_visible = false;
                b_grab = true;
            }
        }
        else if( p_new->i_x >= 0 &&
                 p_new->i_x <= ( i_visu_height + 90 - p_new->i_y ) *
                               ( i_visu_height + 90 - p_new->i_y ) / 160 &&
                 p_new->i_y >= i_visu_height + 9 &&
                 p_new->i_y <= i_visu_height + 90 )
        {
            /* Zoom gauge */
            if( b_pressed )
            {
                p_sys->i_zoom = __MAX( ZOOM_FACTOR,
                                       (80 + i_visu_height - p_new->i_y + 2) *
                                           ZOOM_FACTOR / 10 );

                const int v_w = p_fmt->i_visible_width  * ZOOM_FACTOR / p_sys->i_zoom;
                const int v_h = p_fmt->i_visible_height * ZOOM_FACTOR / p_sys->i_zoom;
                p_sys->i_x = VLC_CLIP( p_sys->i_x, 0, (int)p_fmt->i_visible_width  - v_w - 1 );
                p_sys->i_y = VLC_CLIP( p_sys->i_y, 0, (int)p_fmt->i_visible_height - v_h - 1 );

                b_grab = true;
            }
        }
    }
    else
    {
        if( p_new->i_x >= 0 && p_new->i_x <  80 &&
            p_new->i_y >= 0 && p_new->i_y <= 10 )
        {
            /* Show text */
            if( b_click )
            {
                p_sys->b_visible = true;
                b_grab = true;
            }
        }
    }

    if( vlc_mouse_HasMoved( p_old, p_new ) )
        p_sys->i_last_activity = vlc_tick_now();

    if( b_grab )
        return VLC_EGENERIC;

    /* */
    *p_mouse = *p_new;
    p_mouse->i_x = p_sys->i_x + p_new->i_x * ZOOM_FACTOR / p_sys->i_zoom;
    p_mouse->i_y = p_sys->i_y + p_new->i_y * ZOOM_FACTOR / p_sys->i_zoom;
    return VLC_SUCCESS;
}

