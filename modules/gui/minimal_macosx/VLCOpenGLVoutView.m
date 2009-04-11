/*****************************************************************************
 * VLCOpenGLVoutView.m: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Damien Fouilleul <damienf at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont at videolan dot org>
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
#include "intf.h"
#include "voutgl.h"
#include "VLCOpenGLVoutView.h"
#include "VLCMinimalVoutWindow.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

/*****************************************************************************
 * cocoaglvoutviewInit
 *****************************************************************************/
int cocoaglvoutviewInit( vout_thread_t * p_vout )
{
    vlc_value_t value_drawable;
    id <VLCOpenGLVoutEmbedding> o_cocoaglview_container;

    msg_Dbg( p_vout, "Mac OS X Vout is opening" );

    var_Create( p_vout, "drawable-nsobject", VLC_VAR_DOINHERIT );
    var_Get( p_vout, "drawable-nsobject", &value_drawable );

    p_vout->p_sys->o_pool = [[NSAutoreleasePool alloc] init];

    o_cocoaglview_container = (id) value_drawable.p_address;
    if (!o_cocoaglview_container)
    {
        msg_Warn( p_vout, "No drawable!, spawing a window" );
    }

    p_vout->p_sys->b_embedded = false;


    /* Create the GL view */
    struct args { vout_thread_t * p_vout; id <VLCOpenGLVoutEmbedding> container; } args = { p_vout, o_cocoaglview_container };

    [VLCOpenGLVoutView performSelectorOnMainThread:@selector(autoinitOpenGLVoutViewIntVoutWithContainer:)
                        withObject:[NSData dataWithBytes: &args length: sizeof(struct args)] waitUntilDone:YES];

    [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * cocoaglvoutviewEnd
 *****************************************************************************/
void cocoaglvoutviewEnd( vout_thread_t * p_vout )
{
    id <VLCOpenGLVoutEmbedding> o_cocoaglview_container;

    msg_Dbg( p_vout, "Mac OS X Vout is closing" );
    var_Destroy( p_vout, "drawable-nsobject" );

    o_cocoaglview_container = [p_vout->p_sys->o_glview container];

    /* Make sure our view won't request the vout now */
    [p_vout->p_sys->o_glview detachFromVout];
    msg_Dbg( p_vout, "Mac OS X Vout is closing" );

    /* Let the view go, _without_blocking_ */
    [p_vout->p_sys->o_glview performSelectorOnMainThread:@selector(removeFromSuperview) withObject:NULL waitUntilDone:NO];

    if( [(id)o_cocoaglview_container respondsToSelector:@selector(removeVoutSubview:)] )
        [o_cocoaglview_container removeVoutSubview: p_vout->p_sys->o_glview];

    [p_vout->p_sys->o_glview release];

    [p_vout->p_sys->o_pool release];
 
}

/*****************************************************************************
 * cocoaglvoutviewManage
 *****************************************************************************/
int cocoaglvoutviewManage( vout_thread_t * p_vout )
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
            [[p_vout->p_sys->o_glview container] enterFullscreen];
        else
            [[p_vout->p_sys->o_glview container] leaveFullscreen];

        [o_pool release];

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    //[[p_vout->p_sys->o_glview container] manage];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * cocoaglvoutviewControl: control facility for the vout
 *****************************************************************************/
int cocoaglvoutviewControl( vout_thread_t *p_vout, int i_query, va_list args )
{
    bool b_arg;

    switch( i_query )
    {
        case VOUT_SET_STAY_ON_TOP:
            b_arg = (bool) va_arg( args, int );
            [[p_vout->p_sys->o_glview container] setOnTop: b_arg];
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * cocoaglvoutviewSwap
 *****************************************************************************/
void cocoaglvoutviewSwap( vout_thread_t * p_vout )
{
    p_vout->p_sys->b_got_frame = true;
    [[p_vout->p_sys->o_glview openGLContext] flushBuffer];
}

/*****************************************************************************
 * cocoaglvoutviewLock
 *****************************************************************************/
int cocoaglvoutviewLock( vout_thread_t * p_vout )
{
    if( kCGLNoError == CGLLockContext([[p_vout->p_sys->o_glview openGLContext] CGLContextObj]) )
    {
        [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
        return 0;
    }
    return 1;
}

/*****************************************************************************
 * cocoaglvoutviewUnlock
 *****************************************************************************/
void cocoaglvoutviewUnlock( vout_thread_t * p_vout )
{
    CGLUnlockContext([[p_vout->p_sys->o_glview openGLContext] CGLContextObj]);
}

/*****************************************************************************
 * VLCOpenGLVoutView implementation
 *****************************************************************************/
@implementation VLCOpenGLVoutView

/* Init a new gl view and register it to both the framework and the
 * vout_thread_t. Must be called from main thread. */
+ (void) autoinitOpenGLVoutViewIntVoutWithContainer: (NSData *) argsAsData
{
    NSAutoreleasePool   *pool = [[NSAutoreleasePool alloc] init];
    struct args { vout_thread_t * p_vout; id <VLCOpenGLVoutEmbedding> container; } *
        args = (struct args *)[argsAsData bytes];
    VLCOpenGLVoutView * oglview;

    if( !args->container )
    {
        args->container = [[VLCMinimalVoutWindow alloc] initWithContentRect: NSMakeRect( 0, 0, args->p_vout->i_window_width, args->p_vout->i_window_height )];
        [(VLCMinimalVoutWindow *)args->container makeKeyAndOrderFront: nil];
    }
    oglview = [[VLCOpenGLVoutView alloc] initWithVout: args->p_vout container: args->container];

    args->p_vout->p_sys->o_glview = oglview;
    [args->container addVoutSubview: oglview];

    [pool release];
}

- (void)dealloc
{
    [objectLock dealloc];
    [super dealloc];
}

- (void)removeFromSuperview
{
    [super removeFromSuperview];
}


- (id) initWithVout: (vout_thread_t *) vout container: (id <VLCOpenGLVoutEmbedding>) aContainer
{
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

    if( self = [super initWithFrame: NSMakeRect(0,0,10,10) pixelFormat: fmt] )
    {
        p_vout = vout;
        container = aContainer;
        objectLock = [[NSLock alloc] init];

        [fmt release];

        [[self openGLContext] makeCurrentContext];
        [[self openGLContext] update];

        /* Swap buffers only during the vertical retrace of the monitor.
        http://developer.apple.com/documentation/GraphicsImaging/
        Conceptual/OpenGL/chap5/chapter_5_section_44.html */
        GLint params[] = { 1 };
        CGLSetParameter( CGLGetCurrentContext(), kCGLCPSwapInterval,
                     params );
    }
    return self;
}

- (void) detachFromVout
{
    [objectLock lock];
    p_vout = NULL;
    [objectLock unlock];
}

- (id <VLCOpenGLVoutEmbedding>) container
{
    return container;
}

- (void) destroyVout
{
    [objectLock lock];
    if( p_vout )
    {
        vlc_object_detach( p_vout );
        vlc_object_release( p_vout );
        vlc_object_release( p_vout );
    }
    [objectLock unlock];
}

- (void) reshape
{
    int x, y;
    vlc_value_t val;

    [objectLock lock];
    if( !p_vout )
    {
        [objectLock unlock];
        return;
    }

    cocoaglvoutviewLock( p_vout );
    NSRect bounds = [self bounds];

    if( [[self container] stretchesVideo] )
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
        cocoaglvoutviewUnlock( p_vout );
        if( p_parent && p_parent->pf_display )
        {
            p_parent->pf_display( p_parent, NULL );
        }
    }
    else
    {
        glClear( GL_COLOR_BUFFER_BIT );
        cocoaglvoutviewUnlock( p_vout );
    }
    [objectLock unlock];
    [super reshape];
}

- (void) update
{
    if( kCGLNoError != CGLLockContext([[self openGLContext] CGLContextObj]) )
        return;
    [super update];
    CGLUnlockContext([[p_vout->p_sys->o_glview openGLContext] CGLContextObj]);
}

- (void) drawRect: (NSRect) rect
{
    if( kCGLNoError != CGLLockContext([[self openGLContext] CGLContextObj]) )
        return;
    [[self openGLContext] flushBuffer];
    [super drawRect:rect];
    CGLUnlockContext([[p_vout->p_sys->o_glview openGLContext] CGLContextObj]);
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}
@end

