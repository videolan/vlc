/*****************************************************************************
 * voutgl.m: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id: vout.m 8351 2004-08-02 13:06:38Z hartman $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <vlc_keys.h>

#include "intf.h"
#include "vout.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include <AGL/agl.h>

/*****************************************************************************
 * VLCView interface
 *****************************************************************************/
@interface VLCGLView : NSOpenGLView
{
    vout_thread_t * p_vout;
}

- (id) initWithVout: (vout_thread_t *) p_vout;
@end

struct vout_sys_t
{
    NSAutoreleasePool * o_pool;
    VLCGLView         * o_glview;
    VLCVoutView       * o_vout_view;
    vlc_bool_t          b_saved_frame;
    NSRect              s_frame;
    vlc_bool_t          b_got_frame;
    vlc_mutex_t         lock;
    /* Mozilla plugin-related variables */
    vlc_bool_t          b_embedded;
    AGLContext          agl_ctx;
    AGLDrawable         agl_drawable;
    int                 i_offx, i_offy;
    int                 i_width, i_height;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Init   ( vout_thread_t * p_vout );
static void End    ( vout_thread_t * p_vout );
static int  Manage ( vout_thread_t * p_vout );
static int  Control( vout_thread_t *, int, va_list );
static void Swap   ( vout_thread_t * p_vout );
static int  Lock   ( vout_thread_t * p_vout );
static void Unlock ( vout_thread_t * p_vout );

static int  aglInit   ( vout_thread_t * p_vout );
static void aglEnd    ( vout_thread_t * p_vout );
static int  aglManage ( vout_thread_t * p_vout );
static void aglSwap   ( vout_thread_t * p_vout );

static int DrawableRedraw( vlc_object_t *p_this, const char *psz_name,
    vlc_value_t oval, vlc_value_t nval, void *param);

int E_(OpenVideoGL)  ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;
    vlc_value_t value_drawable;

    if( !CGDisplayUsesOpenGLAcceleration( kCGDirectMainDisplay ) )
    {
        msg_Warn( p_vout, "no OpenGL hardware acceleration found. "
                          "Video display will be slow" );
        return( 1 );
    }
    msg_Dbg( p_vout, "display is Quartz Extreme accelerated" );

    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    vlc_mutex_init( p_vout, &p_vout->p_sys->lock );

    var_Get( p_vout->p_vlc, "drawable", &value_drawable );
    if( value_drawable.i_int != 0 )
    {
        static const GLint ATTRIBUTES[] = { 
            AGL_WINDOW,
            AGL_RGBA,
            AGL_NO_RECOVERY,
            AGL_ACCELERATED,
            AGL_DOUBLEBUFFER,
            AGL_RED_SIZE,   8,
            AGL_GREEN_SIZE, 8,
            AGL_BLUE_SIZE,  8,
            AGL_ALPHA_SIZE, 8,
            AGL_DEPTH_SIZE, 24,
            AGL_NONE };

        AGLDevice screen;
        AGLPixelFormat pixFormat;

        p_vout->p_sys->b_embedded = VLC_TRUE;

        screen = GetGWorldDevice((CGrafPtr)value_drawable.i_int);
        if( NULL == screen )
        {
            msg_Err( p_vout, "can't find screen device for drawable" );
            return VLC_EGENERIC;
        }
        
        pixFormat = aglChoosePixelFormat(&screen, 1, ATTRIBUTES);
        if( NULL == pixFormat )
        {
            msg_Err( p_vout, "no screen renderer available for required attributes." );
            return VLC_EGENERIC;
        }
        
        p_vout->p_sys->agl_ctx = aglCreateContext(pixFormat, NULL);
        aglDestroyPixelFormat(pixFormat);
        if( NULL == p_vout->p_sys->agl_ctx )
        {
            msg_Err( p_vout, "cannot create AGL context." );
            return VLC_EGENERIC;
        }
        else {
            // tell opengl to sync buffer swap with vertical retrace
            GLint param = 1;
            aglSetInteger(p_vout->p_sys->agl_ctx, AGL_SWAP_INTERVAL, &param);
            aglEnable(p_vout->p_sys->agl_ctx, AGL_SWAP_INTERVAL);
        }

        p_vout->pf_init             = aglInit;
        p_vout->pf_end              = aglEnd;
        p_vout->pf_manage           = aglManage;
        p_vout->pf_control          = NULL;
        p_vout->pf_swap             = aglSwap;
        p_vout->pf_lock             = Lock;
        p_vout->pf_unlock           = Unlock;
    }
    else
    {
        p_vout->p_sys->b_embedded = VLC_FALSE;

        p_vout->p_sys->o_pool = [[NSAutoreleasePool alloc] init];

        /* Create the GL view */
        p_vout->p_sys->o_glview = [[VLCGLView alloc] initWithVout: p_vout];
        [p_vout->p_sys->o_glview autorelease];

        /* Spawn the window */

        if( !(p_vout->p_sys->o_vout_view = [VLCVoutView getVoutView: p_vout
                        subView: p_vout->p_sys->o_glview frame: nil]) )
        {
            return VLC_EGENERIC;
        }
        p_vout->pf_init   = Init;
        p_vout->pf_end    = End;
        p_vout->pf_manage = Manage;
        p_vout->pf_control= Control;
        p_vout->pf_swap   = Swap;
        p_vout->pf_lock   = Lock;
        p_vout->pf_unlock = Unlock;
    }
    p_vout->p_sys->b_got_frame = VLC_FALSE;

    return VLC_SUCCESS;
}

void E_(CloseVideoGL) ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;
    if( p_vout->p_sys->b_embedded )
    {
        var_DelCallback(p_vout->p_vlc, "drawableredraw", DrawableRedraw, p_vout);
        aglDestroyContext(p_vout->p_sys->agl_ctx);
    }
    else
    {
        NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

        /* Close the window */
        [p_vout->p_sys->o_vout_view closeVout];

        [o_pool release];
    }
    /* Clean up */
    vlc_mutex_destroy( &p_vout->p_sys->lock );
    free( p_vout->p_sys );
}

static int Init( vout_thread_t * p_vout )
{
    [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
    return VLC_SUCCESS;
}

static void End( vout_thread_t * p_vout )
{
    [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
}

static int Manage( vout_thread_t * p_vout )
{
    if( p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        [p_vout->p_sys->o_glview reshape];
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;
    }
    if( p_vout->i_changes & VOUT_CROP_CHANGE )
    {
        [p_vout->p_sys->o_glview reshape];
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
    }

    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

        if( !p_vout->b_fullscreen )
        {
            /* Save window size and position */
            p_vout->p_sys->s_frame.size =
                [p_vout->p_sys->o_vout_view frame].size;
            p_vout->p_sys->s_frame.origin =
                [[p_vout->p_sys->o_vout_view getWindow ]frame].origin;
            p_vout->p_sys->b_saved_frame = VLC_TRUE;
        }
        [p_vout->p_sys->o_vout_view closeVout];

        p_vout->b_fullscreen = !p_vout->b_fullscreen;

#define o_glview p_vout->p_sys->o_glview
        o_glview = [[VLCGLView alloc] initWithVout: p_vout];
        [o_glview autorelease];

        if( p_vout->p_sys->b_saved_frame )
        {
            p_vout->p_sys->o_vout_view = [VLCVoutView getVoutView: p_vout
                        subView: o_glview
                        frame: &p_vout->p_sys->s_frame];
        }
        else
        {
            p_vout->p_sys->o_vout_view = [VLCVoutView getVoutView: p_vout
                        subView: o_glview frame: nil];

        }

        [[o_glview openGLContext] makeCurrentContext];
#undef o_glview

        [o_pool release];

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    [p_vout->p_sys->o_vout_view manage];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    vlc_bool_t b_arg;

    switch( i_query )
    {
        case VOUT_SET_STAY_ON_TOP:
            b_arg = va_arg( args, vlc_bool_t );
            [p_vout->p_sys->o_vout_view setOnTop: b_arg];
            return VLC_SUCCESS;

        case VOUT_CLOSE:
        case VOUT_REPARENT:
        default:
            return vout_vaControlDefault( p_vout, i_query, args );
    }
}

static void Swap( vout_thread_t * p_vout )
{
    p_vout->p_sys->b_got_frame = VLC_TRUE;
    [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
    glFlush();
}

static int Lock( vout_thread_t * p_vout )
{
    vlc_mutex_lock( &p_vout->p_sys->lock );
    return 0;
}

static void Unlock( vout_thread_t * p_vout )
{
    vlc_mutex_unlock( &p_vout->p_sys->lock );
}

/*****************************************************************************
 * VLCGLView implementation
 *****************************************************************************/
@implementation VLCGLView

- (id) initWithVout: (vout_thread_t *) vout
{
    p_vout = vout;

    NSOpenGLPixelFormatAttribute attribs[] =
    {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFANoRecovery,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAWindow,
        0
    };

    NSOpenGLPixelFormat * fmt = [[NSOpenGLPixelFormat alloc]
        initWithAttributes: attribs];

    if( !fmt )
    {
        msg_Warn( p_vout, "could not create OpenGL video output" );
        return nil;
    }

    self = [super initWithFrame: NSMakeRect(0,0,10,10) pixelFormat: fmt];
    [fmt release];

    [[self openGLContext] makeCurrentContext];
    [[self openGLContext] update];

    /* Swap buffers only during the vertical retrace of the monitor.
       http://developer.apple.com/documentation/GraphicsImaging/
       Conceptual/OpenGL/chap5/chapter_5_section_44.html */
    long params[] = { 1 };
    CGLSetParameter( CGLGetCurrentContext(), kCGLCPSwapInterval,
                     params );
    return self;
}

- (void) reshape
{
    int x, y;
    vlc_value_t val;

    Lock( p_vout );
    NSRect bounds = [self bounds];

    [[self openGLContext] makeCurrentContext];

    var_Get( p_vout, "macosx-stretch", &val );
    if( val.b_bool )
    {
        x = bounds.size.width;
        y = bounds.size.height;
    }
    else if( bounds.size.height * p_vout->fmt_in.i_visible_width *
             p_vout->fmt_in.i_sar_num <
             bounds.size.width * p_vout->fmt_in.i_visible_height *
             p_vout->fmt_in.i_sar_den )
    {
        x = ( bounds.size.height * p_vout->fmt_in.i_visible_width *
              p_vout->fmt_in.i_sar_num ) /
            ( p_vout->fmt_in.i_visible_height * p_vout->fmt_in.i_sar_den);

        y = bounds.size.height;
    }
    else
    {
        x = bounds.size.width;
        y = ( bounds.size.width * p_vout->fmt_in.i_visible_height *
              p_vout->fmt_in.i_sar_den) /
            ( p_vout->fmt_in.i_visible_width * p_vout->fmt_in.i_sar_num  );
    }

    glViewport( ( bounds.size.width - x ) / 2,
                ( bounds.size.height - y ) / 2, x, y );

    if( p_vout->p_sys->b_got_frame )
    {
        /* Ask the opengl module to redraw */
        vout_thread_t * p_parent;
        p_parent = (vout_thread_t *) p_vout->p_parent;
        Unlock( p_vout );
        if( p_parent && p_parent->pf_display )
        {
            p_parent->pf_display( p_parent, NULL );
        }
    }
    else
    {
        glClear( GL_COLOR_BUFFER_BIT );
        Unlock( p_vout );
    }
    [super reshape];
}

- (void) update
{
    Lock( p_vout );
    [super update];
    Unlock( p_vout );
}

- (void) drawRect: (NSRect) rect
{
    Lock( p_vout );
    [[self openGLContext] makeCurrentContext];
    glFlush();
    [super drawRect:rect];
    Unlock( p_vout );
}

@end

/*****************************************************************************
 * embedded AGL context implementation
 *****************************************************************************/

static void UpdateEmbeddedGeometry( vout_thread_t *p_vout );
static void aglReshape( vout_thread_t * p_vout );

static int aglInit( vout_thread_t * p_vout )
{
    UpdateEmbeddedGeometry(p_vout);
    var_AddCallback(p_vout->p_vlc, "drawableredraw", DrawableRedraw, p_vout);

    aglSetCurrentContext(p_vout->p_sys->agl_ctx);
    return VLC_SUCCESS;
}

static void aglEnd( vout_thread_t * p_vout )
{
    aglSetCurrentContext(NULL);
}

static void aglReshape( vout_thread_t * p_vout )
{
    int x, y;
    vlc_value_t val;
    int i_offx   = p_vout->p_sys->i_offx;
    int i_offy   = p_vout->p_sys->i_offy;
    int i_height = p_vout->p_sys->i_height;
    int i_width  = p_vout->p_sys->i_width;

    Lock( p_vout );

    aglSetCurrentContext(p_vout->p_sys->agl_ctx);

    var_Get( p_vout, "macosx-stretch", &val );
    if( val.b_bool )
    {
        x = i_width;
        y = i_height;
    }
    else if( i_height * p_vout->fmt_in.i_visible_width *
             p_vout->fmt_in.i_sar_num <
             i_width * p_vout->fmt_in.i_visible_height *
             p_vout->fmt_in.i_sar_den )
    {
        x = ( i_height * p_vout->fmt_in.i_visible_width *
              p_vout->fmt_in.i_sar_num ) /
            ( p_vout->fmt_in.i_visible_height * p_vout->fmt_in.i_sar_den);

        y = i_height;
    }
    else
    {
        x = i_width;
        y = ( i_width * p_vout->fmt_in.i_visible_height *
              p_vout->fmt_in.i_sar_den) /
            ( p_vout->fmt_in.i_visible_width * p_vout->fmt_in.i_sar_num  );
    }

    glViewport( i_offx+( i_width - x ) / 2,
                i_offy+( i_height - y ) / 2, x, y );

    if( p_vout->p_sys->b_got_frame )
    {
        /* Ask the opengl module to redraw */
        vout_thread_t * p_parent;
        p_parent = (vout_thread_t *) p_vout->p_parent;
        Unlock( p_vout );
        if( p_parent && p_parent->pf_display )
        {
            p_parent->pf_display( p_parent, NULL );
        }
    }
    else
    {
        glClear( GL_COLOR_BUFFER_BIT );
        Unlock( p_vout );
    }
}

static int aglManage( vout_thread_t * p_vout )
{
    if( p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        aglReshape(p_vout);
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;
    }
    if( p_vout->i_changes & VOUT_CROP_CHANGE )
    {
        aglReshape(p_vout);
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
    }
    return VLC_SUCCESS;
}

static void aglSwap( vout_thread_t * p_vout )
{
    p_vout->p_sys->b_got_frame = VLC_TRUE;
    aglSwapBuffers(p_vout->p_sys->agl_ctx);
}

static void UpdateEmbeddedGeometry( vout_thread_t *p_vout )
{
    vlc_value_t val;
    vlc_value_t valt, vall, valb, valr, valx, valy, valw, valh,
                valportx, valporty;

    Rect winBounds;    
    Rect clientBounds;
    
    GLint rect[4];

    var_Get( p_vout->p_vlc, "drawable", &val );
    var_Get( p_vout->p_vlc, "drawablet", &valt );
    var_Get( p_vout->p_vlc, "drawablel", &vall );
    var_Get( p_vout->p_vlc, "drawableb", &valb );
    var_Get( p_vout->p_vlc, "drawabler", &valr );
    var_Get( p_vout->p_vlc, "drawablex", &valx );
    var_Get( p_vout->p_vlc, "drawabley", &valy );
    var_Get( p_vout->p_vlc, "drawablew", &valw );
    var_Get( p_vout->p_vlc, "drawableh", &valh );
    var_Get( p_vout->p_vlc, "drawableportx", &valportx );
    var_Get( p_vout->p_vlc, "drawableporty", &valporty );

    // mozilla plugin provides coordinates based on port bounds
    // however AGL coordinates are based on window structure region
    // and are vertically flipped

    GetWindowBounds(GetWindowFromPort((CGrafPtr)val.i_int),
        kWindowStructureRgn, &winBounds);
    GetWindowBounds(GetWindowFromPort((CGrafPtr)val.i_int),
        kWindowContentRgn, &clientBounds);

    /* update video clipping bounds in drawable */
    rect[0] = (clientBounds.left-winBounds.left)
            + vall.i_int;                       // from window left edge
    rect[1] = (winBounds.bottom-winBounds.top)
            - (clientBounds.top-winBounds.top)
            - valb.i_int;                       // from window bottom edge
    rect[2] = valr.i_int-vall.i_int;            // width
    rect[3] = valb.i_int-valt.i_int;            // height
    aglSetInteger(p_vout->p_sys->agl_ctx, AGL_BUFFER_RECT, rect);
    aglEnable(p_vout->p_sys->agl_ctx, AGL_BUFFER_RECT);

    /* update video internal bounds in drawable */
    p_vout->p_sys->i_offx   = -vall.i_int - valportx.i_int;
    p_vout->p_sys->i_offy   = valb.i_int + valporty.i_int - valh.i_int; 
    p_vout->p_sys->i_width  = valw.i_int;
    p_vout->p_sys->i_height = valh.i_int;

    if( p_vout->p_sys->agl_drawable == (AGLDrawable)val.i_int )
    {
        aglUpdateContext(p_vout->p_sys->agl_ctx);
    }
    else
    {
        p_vout->p_sys->agl_drawable = (AGLDrawable)val.i_int;
        aglSetDrawable(p_vout->p_sys->agl_ctx, p_vout->p_sys->agl_drawable);
    }
    aglReshape( p_vout );
}

/* If we're embedded, the application is expected to indicate a
 * window change (move/resize/etc) via the "drawableredraw" value.
 */

static int DrawableRedraw( vlc_object_t *p_this, const char *psz_name,
    vlc_value_t oval, vlc_value_t nval, void *param)
{
    vout_thread_t *p_vout = (vout_thread_t *)param;

    UpdateEmbeddedGeometry( p_vout );

    return VLC_SUCCESS;
}

