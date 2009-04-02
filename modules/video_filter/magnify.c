/*****************************************************************************
 * magnify.c : Magnify/Zoom interactive effect
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>

#include <math.h>
#include <assert.h>

#include "filter_common.h"
#include "filter_picture.h"

#include "vlc_image.h"

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
static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static int  MouseEvent( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

static void DrawZoomStatus( uint8_t *, int i_pitch, int i_width, int i_height,
                            int i_offset_x, int i_offset_y, bool b_visible );
static void DrawRectangle( uint8_t *, int i_pitch, int i_width, int i_height,
                           int x, int y, int i_w, int i_h );

/*****************************************************************************
 * vout_sys_t: Magnify video output method descriptor
 *****************************************************************************/
struct vout_sys_t
{
    vout_thread_t *p_vout;

    image_handler_t *p_image;

    int64_t i_hide_timeout;

    vlc_mutex_t lock;
    int i_zoom; /* zoom level in percent */
    int i_x, i_y; /* top left corner coordinates in original image */

    bool b_visible; /* is "interface" visible ? */

    int64_t i_last_activity;
};

#define VIS_ZOOM 4
#define ZOOM_FACTOR 8

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * Create: allocates Magnify video thread output method
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    switch( p_vout->fmt_in.i_chroma )
    {
        CASE_PLANAR_YUV
        case VLC_FOURCC('G','R','E','Y'):
            break;
        default:
            msg_Err( p_vout, "Unsupported chroma" );
            return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    p_vout->p_sys->p_image = image_HandlerCreate( p_vout );

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize Magnify video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    video_format_t fmt;

    I_OUTPUTPICTURES = 0;

    memset( &fmt, 0, sizeof(video_format_t) );

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_vout->fmt_out = p_vout->fmt_in;
    fmt = p_vout->fmt_out;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video output" );

    p_vout->p_sys->p_vout = vout_Create( p_vout, &fmt );

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "cannot open vout, aborting" );
        return VLC_EGENERIC;
    }

    vlc_mutex_init( &p_vout->p_sys->lock );
    p_vout->p_sys->i_x = 0;
    p_vout->p_sys->i_y = 0;
    p_vout->p_sys->i_zoom = 2*ZOOM_FACTOR;
    p_vout->p_sys->b_visible = true;
    p_vout->p_sys->i_last_activity = mdate();
    p_vout->p_sys->i_hide_timeout = 1000 * var_GetInteger( p_vout, "mouse-hide-timeout" );

    vout_filter_AllocateDirectBuffers( p_vout, VOUT_MAX_PICTURES );

    vout_filter_AddChild( p_vout, p_vout->p_sys->p_vout, MouseEvent );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Magnify video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    vout_filter_DelChild( p_vout, p_sys->p_vout, MouseEvent );
    vout_CloseAndRelease( p_sys->p_vout );

    vout_filter_ReleaseDirectBuffers( p_vout );

    vlc_mutex_destroy( &p_vout->p_sys->lock );
}

/*****************************************************************************
 * Destroy: destroy Magnify video thread output method
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    image_HandlerDelete( p_vout->p_sys->p_image );


    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    picture_t *p_outpic;

    int v_w, v_h;
    picture_t *p_converted;
    plane_t *p_oyp;
    int i_plane;

    /* This is a new frame. Get a structure from the video_output. */
    while( ( p_outpic = vout_CreatePicture( p_sys->p_vout, 0, 0, 0 ) )
              == NULL )
    {
        if( !vlc_object_alive (p_vout) || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    p_outpic->date = p_pic->date;

    vlc_mutex_lock( &p_sys->lock );
    const bool b_visible = p_sys->b_visible;
    const int o_x = p_sys->i_x;
    const int o_y = p_sys->i_y;
    const int o_zoom = p_sys->i_zoom;
    const int64_t i_last_activity = p_sys->i_last_activity;
    vlc_mutex_unlock( &p_sys->lock );

    /* background magnified image */
    if( o_zoom != ZOOM_FACTOR )
    {
        video_format_t fmt_in;
        video_format_t fmt_out;
        picture_t crop;

        crop = *p_pic;
        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            const int o_yp = o_y * p_outpic->p[i_plane].i_lines / p_outpic->p[Y_PLANE].i_lines;
            const int o_xp = o_x * p_outpic->p[i_plane].i_pitch / p_outpic->p[Y_PLANE].i_pitch;

            crop.p[i_plane].p_pixels += o_yp * p_outpic->p[i_plane].i_pitch + o_xp;
        }

        /* */
        fmt_in = p_vout->fmt_out;
        fmt_in.i_width  = (fmt_in.i_width  * ZOOM_FACTOR / o_zoom) & ~1;
        fmt_in.i_height = (fmt_in.i_height * ZOOM_FACTOR / o_zoom) & ~1;

        /* */
        fmt_out = p_vout->fmt_out;

        p_converted = image_Convert( p_sys->p_image, &crop, &fmt_in, &fmt_out );

        picture_CopyPixels( p_outpic, p_converted );

        picture_Release( p_converted );
    }
    else
    {
        picture_CopyPixels( p_outpic, p_pic );
    }

    /* */
    p_oyp = &p_outpic->p[Y_PLANE];
    if( b_visible )
    {
        video_format_t fmt_out;

        /* image visualization */
        fmt_out = p_vout->fmt_out;
        fmt_out.i_width  = (p_vout->render.i_width/VIS_ZOOM ) & ~1;
        fmt_out.i_height = (p_vout->render.i_height/VIS_ZOOM) & ~1;
        p_converted = image_Convert( p_sys->p_image, p_pic,
                                     &p_pic->format, &fmt_out );
        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            int y;
            for( y = 0; y < p_converted->p[i_plane].i_visible_lines; y++)
            {
                vlc_memcpy(
                    &p_outpic->p[i_plane].p_pixels[y*p_outpic->p[i_plane].i_pitch],
                    p_converted->p[i_plane].p_pixels+y*p_converted->p[i_plane].i_pitch,
                    p_converted->p[i_plane].i_visible_pitch );
            }
        }
        picture_Release( p_converted );

        /* white rectangle on visualization */
        v_w = __MIN( fmt_out.i_width  * ZOOM_FACTOR / o_zoom, fmt_out.i_width - 1 );
        v_h = __MIN( fmt_out.i_height * ZOOM_FACTOR / o_zoom, fmt_out.i_height - 1 );

        DrawRectangle( p_oyp->p_pixels, p_oyp->i_pitch,
                       p_oyp->i_pitch, p_oyp->i_lines,
                       o_x/VIS_ZOOM, o_y/VIS_ZOOM,
                       v_w, v_h );

        /* */
        v_h = fmt_out.i_height + 1;
    }
    else
    {
        v_h = 1;
    }

    /* print a small "VLC ZOOM" */
    if( b_visible || i_last_activity + p_sys->i_hide_timeout > mdate() )
        DrawZoomStatus( p_oyp->p_pixels, p_oyp->i_pitch, p_oyp->i_pitch, p_oyp->i_lines,
                        1, v_h, b_visible );

    if( b_visible )
    {
        int y;

        /* zoom gauge */
        vlc_memset( p_oyp->p_pixels + (v_h+9)*p_oyp->i_pitch, 0xff, 41 );
        for( y = v_h + 10; y < v_h + 90; y++ )
        {
            int width = v_h + 90 - y;
            width = (width*width)/160;
            if( (80 - y + v_h)*ZOOM_FACTOR/10 < o_zoom )
            {
                vlc_memset( p_oyp->p_pixels + y*p_oyp->i_pitch, 0xff, width );
            }
            else
            {
                p_oyp->p_pixels[y*p_oyp->i_pitch] = 0xff;
                p_oyp->p_pixels[y*p_oyp->i_pitch + width - 1] = 0xff;
            }
        }
    }

    vout_DisplayPicture( p_sys->p_vout, p_outpic );
}

static void DrawZoomStatus( uint8_t *pb_dst, int i_pitch, int i_width, int i_height,
                            int i_offset_x, int i_offset_y, bool b_visible )
{
    static const char *p_hide =
        "X   X X      XXXX   XXXXX  XXX   XXX  XX XX   X   X XXXXX XXXX  XXXXXL"
        "X   X X     X          X  X   X X   X X X X   X   X   X   X   X X    L"
        " X X  X     X         X   X   X X   X X   X   XXXXX   X   X   X XXXX L"
        " X X  X     X        X    X   X X   X X   X   X   X   X   X   X X    L"
        "  X   XXXXX  XXXX   XXXXX  XXX   XXX  X   X   X   X XXXXX XXXX  XXXXXL";
    static const char *p_show = 
        "X   X X      XXXX   XXXXX  XXX   XXX  XX XX    XXXX X   X  XXX  X   XL"
        "X   X X     X          X  X   X X   X X X X   X     X   X X   X X   XL"
        " X X  X     X         X   X   X X   X X   X    XXX  XXXXX X   X X X XL"
        " X X  X     X        X    X   X X   X X   X       X X   X X   X X X XL"
        "  X   XXXXX  XXXX   XXXXX  XXX   XXX  X   X   XXXX  X   X  XXX   X X L";
    const char *p_draw = b_visible ? p_hide : p_show;
    int i, y, x;

    for( i = 0, x = i_offset_x, y = i_offset_y; p_draw[i] != '\0'; i++ )
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
    int dy;

    if( x + i_w > i_width || y + i_h > i_height )
        return;

    /* top line */
    vlc_memset( &pb_dst[y * i_pitch + x], 0xff, i_w );

    /* left and right */
    for( dy = 1; dy < i_h-1; dy++ )
    {
        pb_dst[(y+dy) * i_pitch + x +     0] = 0xff;
        pb_dst[(y+dy) * i_pitch + x + i_w-1] = 0xff;
    }

    /* bottom line */
    vlc_memset( &pb_dst[(y+i_h-1) * i_pitch + x], 0xff, i_w );
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = p_data;
    vlc_value_t vald,valx,valy;

    assert( p_this == VLC_OBJECT(p_vout->p_sys->p_vout) );

#define MOUSE_DOWN    1
#define MOUSE_CLICKED 2
#define MOUSE_MOVE_X  4
#define MOUSE_MOVE_Y  8
#define MOUSE_MOVE    12
    uint8_t mouse= 0;

    if( psz_var[6] == 'x' ) mouse |= MOUSE_MOVE_X;
    if( psz_var[6] == 'y' ) mouse |= MOUSE_MOVE_Y;
    if( psz_var[6] == 'c' ) mouse |= MOUSE_CLICKED;

    var_Get( p_vout->p_sys->p_vout, "mouse-button-down", &vald );
    if( vald.i_int & 0x1 ) mouse |= MOUSE_DOWN;
    var_Get( p_vout->p_sys->p_vout, "mouse-y", &valy );
    var_Get( p_vout->p_sys->p_vout, "mouse-x", &valx );

    vlc_mutex_lock( &p_vout->p_sys->lock );

    const int v_h = p_vout->output.i_height*ZOOM_FACTOR/p_vout->p_sys->i_zoom;
    const int v_w = p_vout->output.i_width*ZOOM_FACTOR/p_vout->p_sys->i_zoom;

    if( ( mouse&MOUSE_MOVE && mouse&MOUSE_DOWN)
        || mouse&MOUSE_CLICKED )
    {
    /* (mouse moved and mouse button is down) or (mouse clicked) */
        if( p_vout->p_sys->b_visible )
        {
            if(    0 <= valy.i_int
                && valy.i_int < (int)p_vout->output.i_height/VIS_ZOOM
                && 0 <= valx.i_int
                && valx.i_int < (int)p_vout->output.i_width/VIS_ZOOM )
            {
            /* mouse is over visualisation */
                p_vout->p_sys->i_x = __MIN( __MAX( valx.i_int*VIS_ZOOM - v_w/2, 0 ),
                                            p_vout->output.i_width - v_w - 1);
                p_vout->p_sys->i_y = __MIN( __MAX( valy.i_int * VIS_ZOOM - v_h/2,
                                        0 ), p_vout->output.i_height - v_h - 1);
            }
            else if( valx.i_int >= 0 && valx.i_int < 80
                && valy.i_int >= (int)p_vout->output.i_height/VIS_ZOOM
                && valy.i_int < (int)p_vout->output.i_height/VIS_ZOOM + 9
                && mouse&MOUSE_CLICKED )
            {
            /* mouse is over the "VLC ZOOM HIDE" text */
                p_vout->p_sys->b_visible = false;
            }
            else if(    (int)p_vout->output.i_height/VIS_ZOOM + 9 <= valy.i_int
                     && valy.i_int <= (int)p_vout->output.i_height/VIS_ZOOM + 90
                     && 0 <= valx.i_int
                     && valx.i_int <=
                     (( (int)p_vout->output.i_height/VIS_ZOOM + 90 -  valy.i_int)
               *( (int)p_vout->output.i_height/VIS_ZOOM + 90 -  valy.i_int))/160 )
            {
            /* mouse is over zoom gauge */
                p_vout->p_sys->i_zoom = __MAX( ZOOM_FACTOR,
                                ( 80 + (int)p_vout->output.i_height/VIS_ZOOM
                                   - valy.i_int + 2) * ZOOM_FACTOR/10 );
            }
            else if( mouse&MOUSE_MOVE_X && !(mouse&MOUSE_CLICKED) )
            {
                p_vout->p_sys->i_x -= (newval.i_int - oldval.i_int)
                                      *ZOOM_FACTOR/p_vout->p_sys->i_zoom;
            }
            else if( mouse&MOUSE_MOVE_Y && !(mouse&MOUSE_CLICKED) )
            {
                p_vout->p_sys->i_y -= (newval.i_int - oldval.i_int)
                                      *ZOOM_FACTOR/p_vout->p_sys->i_zoom;
            }
        }
        else
        {
            if( valx.i_int >= 0 && valx.i_int < 80 && valy.i_int >= 0
                && valy.i_int <= 10 && mouse&MOUSE_CLICKED )
            {
            /* mouse is over the "VLC ZOOM SHOW" text */
                p_vout->p_sys->b_visible = true;
            }
            else if( mouse&MOUSE_MOVE_X && !(mouse&MOUSE_CLICKED) )
            {
                p_vout->p_sys->i_x -= (newval.i_int - oldval.i_int)
                                      *ZOOM_FACTOR/p_vout->p_sys->i_zoom;
            }
            else if( mouse&MOUSE_MOVE_Y && !(mouse&MOUSE_CLICKED) )
            {
                p_vout->p_sys->i_y -= (newval.i_int - oldval.i_int)
                                      *ZOOM_FACTOR/p_vout->p_sys->i_zoom;
            }
        }
    }

    p_vout->p_sys->i_x =
         __MAX( 0, __MIN( p_vout->p_sys->i_x, (int)p_vout->output.i_width
         - (int)p_vout->output.i_width*ZOOM_FACTOR/p_vout->p_sys->i_zoom - 1 ));
    p_vout->p_sys->i_y =
         __MAX( 0, __MIN( p_vout->p_sys->i_y, (int)p_vout->output.i_height
        - (int)p_vout->output.i_height*ZOOM_FACTOR/p_vout->p_sys->i_zoom - 1 ));

    p_vout->p_sys->i_last_activity = mdate();
    vlc_mutex_unlock( &p_vout->p_sys->lock );

    /* FIXME forward event when not grabbed */

    return VLC_SUCCESS;
}
