/*****************************************************************************
 * magnify.c : Magnify/Zoom interactive effect
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include <math.h>

#include "filter_common.h"
#include "vlc_image.h"
#include "vlc_input.h"
#include "vlc_playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static int  SendEvents   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  MouseEvent   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Magnify/Zoom interactive video filter") );
    set_shortname( N_( "Magnify" ));
    set_capability( "video filter", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: Magnify video output method descriptor
 *****************************************************************************/
struct vout_sys_t
{
    vout_thread_t *p_vout;

    image_handler_t *p_image;

    int i_zoom; /* zoom level in percent */
    int i_x, i_y; /* top left corner coordinates in original image */

    vlc_bool_t b_visible; /* is "interface" visible ? */
};

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

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

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
    int i_index;
    picture_t *p_pic;
    video_format_t fmt = {0};

    I_OUTPUTPICTURES = 0;

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

#define VIS_ZOOM 4
    p_vout->p_sys->i_x = 0;
    p_vout->p_sys->i_y = 0;
#define ZOOM_FACTOR 100
    p_vout->p_sys->i_zoom = 200;

    var_AddCallback( p_vout->p_sys->p_vout, "mouse-x", MouseEvent, p_vout );
    var_AddCallback( p_vout->p_sys->p_vout, "mouse-y", MouseEvent, p_vout );
    var_AddCallback( p_vout->p_sys->p_vout, "mouse-clicked",
                     MouseEvent, p_vout);

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );
    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );
    ADD_PARENT_CALLBACKS( SendEventsToChild );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Magnify video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }

    var_DelCallback( p_vout->p_sys->p_vout, "mouse-x", MouseEvent, p_vout);
    var_DelCallback( p_vout->p_sys->p_vout, "mouse-y", MouseEvent, p_vout);
    var_DelCallback( p_vout->p_sys->p_vout, "mouse-clicked", MouseEvent, p_vout);
}

/*****************************************************************************
 * Destroy: destroy Magnify video thread output method
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    if( p_vout->p_sys->p_vout )
    {
        DEL_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );
        vlc_object_detach( p_vout->p_sys->p_vout );
        vout_Destroy( p_vout->p_sys->p_vout );
    }

    image_HandlerDelete( p_vout->p_sys->p_image );

    DEL_PARENT_CALLBACKS( SendEventsToChild );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic;

    int o_x = p_vout->p_sys->i_x;
    int o_y = p_vout->p_sys->i_y;
    int o_zoom = p_vout->p_sys->i_zoom;
    int x,y,o_yp,o_xp;
    int v_w, v_h;
    video_format_t fmt_out = {0};
    picture_t *p_converted;
    plane_t *p_oyp=NULL;

    /* This is a new frame. Get a structure from the video_output. */
    while( ( p_outpic = vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
              == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    vout_DatePicture( p_vout->p_sys->p_vout, p_outpic, p_pic->date );

    p_oyp = &(p_outpic->p[Y_PLANE]);

    /* background magnified image */
    if( o_zoom != ZOOM_FACTOR )
    {
#define magnify( plane ) \
    o_yp = o_y*p_outpic->p[plane].i_lines/p_outpic->p[Y_PLANE].i_lines; \
    o_xp = o_x*p_outpic->p[plane].i_pitch/p_outpic->p[Y_PLANE].i_pitch; \
    for( y=0; y<p_outpic->p[plane].i_visible_lines; y++ ) \
    { \
        for( x=0; x<p_outpic->p[plane].i_visible_pitch; x++ ) \
        { \
            p_outpic->p[plane].p_pixels[y*p_outpic->p[plane].i_pitch+x] = \
                p_pic->p[plane].p_pixels[ \
                    ( o_yp + y*ZOOM_FACTOR/o_zoom )*p_outpic->p[plane].i_pitch \
                    + o_xp + x*ZOOM_FACTOR/o_zoom \
                ]; \
        } \
    }
    magnify( Y_PLANE );
    magnify( U_PLANE );
    magnify( V_PLANE );
#undef magnify
    }
    else
    {
#define copy( plane ) \
        memcpy( p_outpic->p[plane].p_pixels, p_pic->p[plane].p_pixels, \
            p_outpic->p[plane].i_lines * p_outpic->p[plane].i_pitch );
        copy( Y_PLANE );
        copy( U_PLANE );
        copy( V_PLANE );
#undef copy
    }

    if( p_vout->p_sys->b_visible )
    {
        /* image visualization */
        fmt_out = p_vout->fmt_out;
        fmt_out.i_width = p_vout->render.i_width/VIS_ZOOM;
        fmt_out.i_height = p_vout->render.i_height/VIS_ZOOM;
        p_converted = image_Convert( p_vout->p_sys->p_image, p_pic,
                                     &(p_pic->format), &fmt_out );
    #define copyimage( plane ) \
        for( y=0; y<p_converted->p[plane].i_visible_lines; y++) \
        { \
            memcpy( p_outpic->p[plane].p_pixels+y*p_outpic->p[plane].i_pitch, \
            p_converted->p[plane].p_pixels+y*p_converted->p[plane].i_pitch, \
            p_converted->p[plane].i_visible_pitch ); \
        }
        copyimage( Y_PLANE );
        copyimage( U_PLANE );
        copyimage( V_PLANE );
    #undef copyimage
        p_converted->pf_release( p_converted );

        /* white rectangle on visualization */
        v_w = p_oyp->i_pitch*ZOOM_FACTOR/(VIS_ZOOM*o_zoom);
        v_h = (o_y+p_oyp->i_lines*ZOOM_FACTOR/o_zoom)/VIS_ZOOM;
        /* top line */
        memset( p_oyp->p_pixels
                + o_y/VIS_ZOOM*p_oyp->i_pitch
                + o_x/VIS_ZOOM, 0xff, v_w+1 );

        for( y = o_y/VIS_ZOOM+1; y < v_h; y++ )
        {
            /* left line */
            p_oyp->p_pixels[
                y*p_oyp->i_pitch+o_x/VIS_ZOOM
            ] = 0xff;
            /* right line */
            p_oyp->p_pixels[
                y*p_oyp->i_pitch+o_x/VIS_ZOOM + v_w
            ] = 0xff;
        }
        /* bottom line */
        memset( p_oyp->p_pixels
                + v_h*p_oyp->i_pitch
                + o_x/VIS_ZOOM, 0xff, v_w+1 );

        /* */
        v_h = p_oyp->i_lines/VIS_ZOOM;
    }
    else
    {
        v_h = 1;
    }

    /* print a small "VLC ZOOM" ... gruikkkkkkkkk */
#define DRAW(a) {int c,l=1;L a;}
#define L ;l++,c=1
#define X ;draw(l,c);c+=1
#define o +1
#define draw(y,x) p_oyp->p_pixels[(v_h+y)*p_oyp->i_pitch+x] = 0xff;
if( p_vout->p_sys->b_visible )
DRAW(
X o o o X o X o o o o o o X X X X o o o X X X X X o o X X X o o o X X X o o X X o X X o o o X o o o X o X X X X X o X X X X o o X X X X X L
X o o o X o X o o o o o X o o o o o o o o o o X o o X o o o X o X o o o X o X o X o X o o o X o o o X o o o X o o o X o o o X o X o o o o L
o X o X o o X o o o o o X o o o o o o o o o X o o o X o o o X o X o o o X o X o o o X o o o X X X X X o o o X o o o X o o o X o X X X X o L
o X o X o o X o o o o o X o o o o o o o o X o o o o X o o o X o X o o o X o X o o o X o o o X o o o X o o o X o o o X o o o X o X o o o o L
o o X o o o X X X X X o o X X X X o o o X X X X X o o X X X o o o X X X o o X o o o X o o o X o o o X o X X X X X o X X X X o o X X X X X L
)
else
DRAW(
X o o o X o X o o o o o o X X X X o o o X X X X X o o X X X o o o X X X o o X X o X X o o o o X X X X o X o o o X o o X X X o o X o o o X L
X o o o X o X o o o o o X o o o o o o o o o o X o o X o o o X o X o o o X o X o X o X o o o X o o o o o X o o o X o X o o o X o X o o o X L
o X o X o o X o o o o o X o o o o o o o o o X o o o X o o o X o X o o o X o X o o o X o o o o X X X o o X X X X X o X o o o X o X o X o X L
o X o X o o X o o o o o X o o o o o o o o X o o o o X o o o X o X o o o X o X o o o X o o o o o o o X o X o o o X o X o o o X o X o X o X L
o o X o o o X X X X X o o X X X X o o o X X X X X o o X X X o o o X X X o o X o o o X o o o X X X X o o X o o o X o o X X X o o o X o X o L
)
#undef DRAW
#undef L
#undef X
#undef O
#undef draw

    if( p_vout->p_sys->b_visible )
    {
        /* zoom gauge */
        memset( p_oyp->p_pixels
                    + (v_h+9)*p_oyp->i_pitch,
                    0xff, 41 );
        for( y = v_h + 10; y < v_h + 90; y++ )
        {
            int width = v_h + 90 - y;
            width = (width*width)/160;
            if( (80 - y + v_h)*10 < o_zoom )
            {
                memset( p_oyp->p_pixels
                    + y*p_oyp->i_pitch,
                    0xff, width );
            }
            else
            {
                p_oyp->p_pixels[y*p_oyp->i_pitch] = 0xff;
                p_oyp->p_pixels[y*p_oyp->i_pitch + width - 1] = 0xff;
            }
        }
    }

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    var_Set( (vlc_object_t *)p_data, psz_var, newval );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendEventsToChild: forward events to the child/children vout
 *****************************************************************************/
static int SendEventsToChild( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    var_Set( p_vout->p_sys->p_vout, psz_var, newval );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_data;
    vlc_value_t vald,valx,valy;

#define MOUSE_DOWN    1
#define MOUSE_CLICKED 2
#define MOUSE_MOVE_X  4
#define MOUSE_MOVE_Y  8
#define MOUSE_MOVE    12
    uint8_t mouse= 0;

    int v_h = p_vout->output.i_height*ZOOM_FACTOR/p_vout->p_sys->i_zoom;
    int v_w = p_vout->output.i_width*ZOOM_FACTOR/p_vout->p_sys->i_zoom;

    if( psz_var[6] == 'x' ) mouse |= MOUSE_MOVE_X;
    if( psz_var[6] == 'y' ) mouse |= MOUSE_MOVE_Y;
    if( psz_var[6] == 'c' ) mouse |= MOUSE_CLICKED;

    var_Get( p_vout->p_sys->p_vout, "mouse-button-down", &vald );
    if( vald.i_int & 0x1 ) mouse |= MOUSE_DOWN;
    var_Get( p_vout->p_sys->p_vout, "mouse-y", &valy );
    var_Get( p_vout->p_sys->p_vout, "mouse-x", &valx );

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
                p_vout->p_sys->b_visible = VLC_FALSE;
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
                p_vout->p_sys->b_visible = VLC_TRUE;
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


    return VLC_SUCCESS;
}
