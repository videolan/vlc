/*****************************************************************************
 * voutgl.m: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN
 * $Id: vout.m 8351 2004-08-02 13:06:38Z hartman $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
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

- (id)initWithFrame: (NSRect) frame vout: (vout_thread_t*) p_vout;

@end


struct vout_sys_t
{
    NSAutoreleasePool * o_pool;
    VLCWindow         * o_window;
    VLCGLView         * o_glview;
    vlc_bool_t          b_saved_frame;
    NSRect              s_frame;
    vlc_bool_t          b_got_frame;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Init   ( vout_thread_t * p_vout );
static void End    ( vout_thread_t * p_vout );
static int  Manage ( vout_thread_t * p_vout );
static int  Control( vout_thread_t *, int, va_list );
static void Swap   ( vout_thread_t * p_vout );

int E_(OpenVideoGL)  ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;

    if( !CGDisplayUsesOpenGLAcceleration( kCGDirectMainDisplay ) )
    {
        msg_Warn( p_vout, "no hardware acceleration" );
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

    /* Spawn window */
    p_vout->p_sys->b_got_frame = VLC_FALSE;
    p_vout->p_sys->o_window = [[VLCWindow alloc] initWithVout: p_vout
                                                 frame: nil];
    if( !p_vout->p_sys->o_window )
    {
        return VLC_EGENERIC;
    }

    /* Add OpenGL view */
#define o_glview p_vout->p_sys->o_glview
    o_glview = [[VLCGLView alloc] initWithFrame:
                [p_vout->p_sys->o_window frame] vout: p_vout];
    [p_vout->p_sys->o_window setContentView: o_glview];
    [o_glview autorelease];
#undef o_glview

    p_vout->pf_init   = Init;
    p_vout->pf_end    = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_control= Control;
    p_vout->pf_swap   = Swap;

    return VLC_SUCCESS;
}

void E_(CloseVideoGL) ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    /* Remove the GLView from the window, because we are not sure OS X
       will actually close the window right away. When it doesn't,
       VLCGLView's reshape is called while p_vout and p_vout->p_sys
       aren't valid anymore and crashes. */
    [p_vout->p_sys->o_window setContentView: NULL];

    /* Close the window */
    [p_vout->p_sys->o_window close];

    /* Clean up */
    [o_pool release];
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
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

        if( !p_vout->b_fullscreen )
        {
            /* Save window size and position */
            p_vout->p_sys->s_frame.size =
                [[p_vout->p_sys->o_window contentView] frame].size;
            p_vout->p_sys->s_frame.origin =
                [p_vout->p_sys->o_window frame].origin;
            p_vout->p_sys->b_saved_frame = VLC_TRUE;
        }
        [p_vout->p_sys->o_window close];

        p_vout->b_fullscreen = !p_vout->b_fullscreen;

        if( p_vout->p_sys->b_saved_frame )
        {
            p_vout->p_sys->o_window = [[VLCWindow alloc]
                initWithVout: p_vout frame: &p_vout->p_sys->s_frame];
        }
        else
        {
            p_vout->p_sys->o_window = [[VLCWindow alloc]
                initWithVout: p_vout frame: nil];
        }

#define o_glview p_vout->p_sys->o_glview
        o_glview = [[VLCGLView alloc] initWithFrame: [p_vout->p_sys->o_window frame] vout: p_vout];
        [p_vout->p_sys->o_window setContentView: o_glview];
        [o_glview autorelease];
        [[o_glview openGLContext] makeCurrentContext];
#undef o_glview
        [o_pool release];

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }
    [p_vout->p_sys->o_window manage];
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
            [p_vout->p_sys->o_window setOnTop: b_arg];
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
    if( [p_vout->p_sys->o_glview lockFocusIfCanDraw] )
    {
        glFlush();
        [p_vout->p_sys->o_glview unlockFocus];
    }
}

/*****************************************************************************
 * VLCGLView implementation
 *****************************************************************************/
@implementation VLCGLView

- (id) initWithFrame: (NSRect) frame vout: (vout_thread_t*) _p_vout
{
    p_vout = _p_vout;

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
        msg_Warn( p_vout, "Cannot create NSOpenGLPixelFormat" );
        return nil;
    }

    self = [super initWithFrame:frame pixelFormat: fmt];
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
    NSRect bounds = [self bounds];

    [[self openGLContext] makeCurrentContext];

    var_Get( p_vout, "macosx-stretch", &val );
    if( val.b_bool )
    {
        x = bounds.size.width;
        y = bounds.size.height;
    }
    else if( bounds.size.height * p_vout->render.i_aspect <
             bounds.size.width * VOUT_ASPECT_FACTOR )
    {
        x = bounds.size.height * p_vout->render.i_aspect / VOUT_ASPECT_FACTOR;
        y = bounds.size.height;
    }
    else
    {
        x = bounds.size.width;
        y = bounds.size.width * VOUT_ASPECT_FACTOR / p_vout->render.i_aspect;
    }
    glViewport( ( bounds.size.width - x ) / 2,
                ( bounds.size.height - y ) / 2, x, y );

    if( p_vout->p_sys->b_got_frame )
    {
        /* Ask the opengl module to redraw */
        vout_thread_t * p_parent;
        p_parent = (vout_thread_t *) p_vout->p_parent;
        if( p_parent && p_parent->pf_display )
        {
            p_parent->pf_display( p_parent, NULL );
        }
    }
    else
    {
        glClear( GL_COLOR_BUFFER_BIT );
    }
}

- (void) drawRect: (NSRect) rect
{
    [[self openGLContext] makeCurrentContext];
    glFlush();
}

@end


