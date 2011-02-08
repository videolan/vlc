/*****************************************************************************
 * voutgl.m: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2004, 2007-2009, 2011 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Damien Fouilleul <damienf at videolan dot org>
 *          Felix Paul Kuehne <fkuehne at videolan dot org>
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
#include <stdlib.h>                                                /* free() */
#include <string.h>

#include <vlc_common.h>
#include <vlc_keys.h>

#include "intf.h"
#include "vout.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

/*****************************************************************************
 * VLCGLView interface
 *****************************************************************************/
@interface VLCGLView : NSOpenGLView <VLCVoutViewResetting>
{
    vout_thread_t * p_vout;
}

+ (void)resetVout: (NSValue *) voutValue;
- (id) initWithVout: (vout_thread_t *) p_vout;
@end

struct vout_sys_t
{
    VLCGLView         * o_glview;
    VLCVoutView       * o_vout_view;
    bool                b_saved_frame;
    NSRect              s_frame;
    bool                b_got_frame;

    bool                b_embedded;
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

int OpenVideoGL  ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;

    if( !CGDisplayUsesOpenGLAcceleration( kCGDirectMainDisplay ) )
    {
        msg_Warn( p_vout, "no OpenGL hardware acceleration found. "
                          "Video display might be slow" );
    }
    msg_Dbg( p_vout, "display is Quartz Extreme accelerated" );

    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    p_vout->p_sys->b_embedded = false;

    [VLCGLView performSelectorOnMainThread:@selector(initVout:) withObject:[NSValue valueWithPointer:p_vout] waitUntilDone:YES];

    [o_pool release];

    /* Check to see if initVout: was successfull */
    if( !p_vout->p_sys->o_vout_view )
    {
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    p_vout->pf_init   = Init;
    p_vout->pf_end    = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_control= Control;
    p_vout->pf_swap   = Swap;
    p_vout->pf_lock   = Lock;
    p_vout->pf_unlock = Unlock;
    p_vout->p_sys->b_got_frame = false;

    return VLC_SUCCESS;
}

void CloseVideoGL ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;

    if(VLCIntf && vlc_object_alive (VLCIntf))
    {
        NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

        /* Close the window */
        [p_vout->p_sys->o_vout_view performSelectorOnMainThread:@selector(closeVout) withObject:NULL waitUntilDone:YES];

        [o_pool release];
    }
    /* Clean up */
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

        p_vout->b_fullscreen = !p_vout->b_fullscreen;

        if( p_vout->b_fullscreen )
            [p_vout->p_sys->o_vout_view enterFullscreen];
        else
            [p_vout->p_sys->o_vout_view leaveFullscreen];

        [o_pool release];

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    if( p_vout->p_sys->o_vout_view )
        [p_vout->p_sys->o_vout_view manage];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    bool b_arg;

    switch( i_query )
    {
        case VOUT_SET_STAY_ON_TOP:
            b_arg = (bool) va_arg( args, int );
            [p_vout->p_sys->o_vout_view setOnTop: b_arg];
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}

static void Swap( vout_thread_t * p_vout )
{
    p_vout->p_sys->b_got_frame = true;
    [[p_vout->p_sys->o_glview openGLContext] flushBuffer];
}

static int Lock( vout_thread_t * p_vout )
{
    if( kCGLNoError == CGLLockContext([[p_vout->p_sys->o_glview openGLContext] CGLContextObj]) )
    {
        [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
        return 0;
    }
    return 1;
}

static void Unlock( vout_thread_t * p_vout )
{
    CGLUnlockContext([[p_vout->p_sys->o_glview openGLContext] CGLContextObj]);
}

/*****************************************************************************
 * VLCGLView implementation
 *****************************************************************************/
@implementation VLCGLView
+ (void)initVout:(NSValue *)arg
{
    vout_thread_t * p_vout = [arg pointerValue];

    /* Create the GL view */
    p_vout->p_sys->o_glview = [[VLCGLView alloc] initWithVout: p_vout];
    [p_vout->p_sys->o_glview autorelease];

    /* Spawn the window */
    id old_vout = p_vout->p_sys->o_vout_view;
    p_vout->p_sys->o_vout_view = [[VLCVoutView voutView: p_vout
                                                subView: p_vout->p_sys->o_glview frame: nil] retain];
    [old_vout release];
}

/* This function will reset the o_vout_view. It's useful to go fullscreen. */
+ (void)resetVout:(NSValue *) voutValue
{
    vout_thread_t * p_vout = [voutValue pointerValue];
    if( p_vout->b_fullscreen )
    {
        /* Save window size and position */
        p_vout->p_sys->s_frame.size =
            [p_vout->p_sys->o_vout_view frame].size;
        p_vout->p_sys->s_frame.origin =
            [[p_vout->p_sys->o_vout_view voutWindow]frame].origin;
        p_vout->p_sys->b_saved_frame = true;
    }

    [p_vout->p_sys->o_vout_view closeVout];

#define o_glview p_vout->p_sys->o_glview
    o_glview = [[VLCGLView alloc] initWithVout: p_vout];
    [o_glview autorelease];

    if( p_vout->p_sys->b_saved_frame )
    {
        id old_vout = p_vout->p_sys->o_vout_view;
        p_vout->p_sys->o_vout_view = [[VLCVoutView voutView: p_vout
                                                    subView: o_glview
                                                      frame: &p_vout->p_sys->s_frame] retain];
        [old_vout release];
    }
    else
    {
        id old_vout = p_vout->p_sys->o_vout_view;
        p_vout->p_sys->o_vout_view = [[VLCVoutView voutView: p_vout
                                                    subView: o_glview frame: nil] retain];
        [old_vout release];
    }
#undef o_glview
}

- (id) initWithVout: (vout_thread_t *) vout
{
    /* Must be called from main thread:
     * "The NSView class is generally thread-safe, with a few exceptions. You
     * should create, destroy, resize, move, and perform other operations on NSView
     * objects only from the main thread of an application. Drawing from secondary
     * threads is thread-safe as long as you bracket drawing calls with calls to
     * lockFocusIfCanDraw and unlockFocus." Cocoa Thread Safety */

    p_vout = vout;

    NSOpenGLPixelFormatAttribute attribs[] =
    {
        NSOpenGLPFADoubleBuffer,
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
    GLint params[] = { 1 };
    CGLSetParameter( CGLGetCurrentContext(), kCGLCPSwapInterval, params );
    return self;
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (void) reshape
{
    int x, y;

    Lock( p_vout );
    NSRect bounds = [self bounds];

    if( var_GetBool( p_vout, "macosx-stretch" ) )
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

    [super reshape];

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

- (void) update
{
    Lock( p_vout );
    [super update];
    Unlock( p_vout );
}

- (void) drawRect: (NSRect) rect
{
    Lock( p_vout );
    [[p_vout->p_sys->o_glview openGLContext] flushBuffer];
    [super drawRect:rect];
    Unlock( p_vout );
}

- (void) renewGState
{
    NSWindow *window = [self window];

	if ([window respondsToSelector:@selector(disableScreenUpdatesUntilFlush)])
	{
		[window disableScreenUpdatesUntilFlush];
	}

    [super renewGState];
}

@end