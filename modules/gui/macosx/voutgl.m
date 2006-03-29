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
    vlc_bool_t          b_vout_size_update;
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

static int AspectCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

int E_(OpenVideoGL)  ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;

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

    p_vout->p_sys->o_pool = [[NSAutoreleasePool alloc] init];
    vlc_mutex_init( p_vout, &p_vout->p_sys->lock );

    /* Create the GL view */
    p_vout->p_sys->o_glview = [[VLCGLView alloc] initWithVout: p_vout];
    [p_vout->p_sys->o_glview autorelease];

    /* Spawn the window */

    if( !(p_vout->p_sys->o_vout_view = [VLCVoutView getVoutView: p_vout
                    subView: p_vout->p_sys->o_glview frame: nil]) )
    {
        return VLC_EGENERIC;
    }

    p_vout->p_sys->b_got_frame = VLC_FALSE;

    p_vout->pf_init   = Init;
    p_vout->pf_end    = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_control= Control;
    p_vout->pf_swap   = Swap;
    p_vout->pf_lock   = Lock;
    p_vout->pf_unlock = Unlock;

    return VLC_SUCCESS;
}

void E_(CloseVideoGL) ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    /* Close the window */
    [p_vout->p_sys->o_vout_view closeVout];

    /* Clean up */
    vlc_mutex_destroy( &p_vout->p_sys->lock );
    [o_pool release];
    free( p_vout->p_sys );
}

static int Init( vout_thread_t * p_vout )
{
    /* The variable is in fact changed on the parent vout */
    if( !var_Type( p_vout->p_parent, "aspect-ratio" ) )
    {
        var_Create( p_vout->p_parent, "aspect-ratio",
                                VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    }
    var_AddCallback( p_vout->p_parent, "aspect-ratio", AspectCallback, p_vout );
    [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
    return VLC_SUCCESS;
}

static void End( vout_thread_t * p_vout )
{
    var_DelCallback( p_vout->p_parent, "aspect-ratio", AspectCallback, p_vout );
    [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
}

static int Manage( vout_thread_t * p_vout )
{
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

    if( p_vout->p_sys->b_vout_size_update )
    {
        NSRect old_bounds = [p_vout->p_sys->o_glview bounds];
        [p_vout->p_sys->o_glview reshape];
        if( [p_vout->p_sys->o_glview bounds].size.height !=
            old_bounds.size.height ||
            [p_vout->p_sys->o_glview bounds].size.width !=
            old_bounds.size.width);
        {
             p_vout->p_sys->b_vout_size_update = VLC_FALSE;
        }
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

static int AspectCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    /* Only update the vout size if the aspect ratio has actually been changed*/

    if( strcmp( oldval.psz_string, newval.psz_string ) )
    {
        ((vout_thread_t *)p_data)->p_sys->b_vout_size_update = VLC_TRUE;
    }
    return VLC_SUCCESS;
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
        msg_Warn( p_vout, "Could not create OpenGL video output" );
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
    else if( bounds.size.height * p_vout->render.i_aspect *
             p_vout->fmt_in.i_sar_num <
             bounds.size.width * VOUT_ASPECT_FACTOR * p_vout->fmt_in.i_sar_den )
    {
        x = bounds.size.height * p_vout->render.i_aspect *
            p_vout->fmt_in.i_sar_num / ( VOUT_ASPECT_FACTOR *
            p_vout->fmt_in.i_sar_den );
        y = bounds.size.height;
    }
    else
    {
        x = bounds.size.width;
        y = bounds.size.width * p_vout->fmt_in.i_sar_den *
            VOUT_ASPECT_FACTOR / ( p_vout->fmt_in.i_sar_num *
            p_vout->render.i_aspect );
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


