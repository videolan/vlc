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

#if 0

/*****************************************************************************
 * cocoaglvoutviewInit
 *****************************************************************************/
int cocoaglvoutviewInit( vout_window_t *p_wnd, const vout_window_cfg_t *cfg)
{
    vlc_value_t value_drawable;
    id <VLCOpenGLVoutEmbedding> o_cocoaglview_container;

    msg_Dbg( p_wnd, "Mac OS X Vout is opening" );

    var_Create( p_wnd, "drawable-nsobject", VLC_VAR_DOINHERIT );
    var_Get( p_wnd, "drawable-nsobject", &value_drawable );

    p_wnd->sys->o_pool = [[NSAutoreleasePool alloc] init];

    /* This will be released in cocoaglvoutviewEnd(), on
     * main thread, after we are done using it. */
    o_cocoaglview_container = [(id) value_drawable.p_address retain];
    if (!o_cocoaglview_container)
    {
        msg_Warn( p_wnd, "No drawable!, spawing a window" );
    }

    //p_vout->p_sys->b_embedded = false;


    /* Create the GL view */
    struct args { vout_window_t *p_wnd; const vout_window_cfg_t *cfg; id <VLCOpenGLVoutEmbedding> container; } args = { p_wnd, cfg, o_cocoaglview_container };

    [VLCOpenGLVoutView performSelectorOnMainThread:@selector(autoinitOpenGLVoutViewIntVoutWithContainer:)
                        withObject:[NSData dataWithBytes: &args length: sizeof(struct args)] waitUntilDone:YES];

    return VLC_SUCCESS;
}

/*****************************************************************************
 * cocoaglvoutviewEnd
 *****************************************************************************/
void cocoaglvoutviewEnd( vout_window_t * p_wnd )
{
    id <VLCOpenGLVoutEmbedding> o_cocoaglview_container;

    if (!p_wnd->handle.nsobject)
        return;

    msg_Dbg( p_wnd, "Mac OS X Vout is closing" );
    var_Destroy( p_wnd, "drawable-nsobject" );

    o_cocoaglview_container = [(VLCOpenGLVoutView  *)p_wnd->handle.nsobject container];

    /* Make sure our view won't request the vout now */
    [(VLCOpenGLVoutView  *)p_wnd->handle.nsobject detachFromVoutWindow];
    msg_Dbg( p_wnd, "Mac OS X Vout is closing" );

    if( [(id)o_cocoaglview_container respondsToSelector:@selector(removeVoutSubview:)] )
        [(id)o_cocoaglview_container performSelectorOnMainThread:@selector(removeVoutSubview:) withObject:p_wnd->handle.nsobject waitUntilDone:NO];

    /* Let the view go and release it, _without_blocking_ */
    [(id)p_wnd->handle.nsobject performSelectorOnMainThread:@selector(removeFromSuperviewAndRelease) withObject:nil waitUntilDone:NO];
    p_wnd->handle.nsobject = nil;

    /* Release the container now that we don't use it */
    [(id)o_cocoaglview_container performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];

    [p_wnd->sys->o_pool release];
    p_wnd->sys->o_pool = nil;
 
}

/*****************************************************************************
 * VLCOpenGLVoutView implementation
 *****************************************************************************/
@implementation VLCOpenGLVoutView

/* Init a new gl view and register it to both the framework and the
 * vout_thread_t. Must be called from main thread. */
+ (void) autoinitOpenGLVoutViewIntVoutWithContainer: (NSData *) argsAsData
{
    struct args { vout_window_t *p_wnd; vout_window_cfg_t *cfg; id <VLCOpenGLVoutEmbedding> container; } *
        args = (struct args *)[argsAsData bytes];
    VLCOpenGLVoutView * oglview;

    if( !args->container )
    {
        args->container = [[VLCMinimalVoutWindow alloc] initWithContentRect: NSMakeRect( 0, 0, args->cfg->width, args->cfg->height )];
        [(VLCMinimalVoutWindow *)args->container makeKeyAndOrderFront: nil];
    }
    oglview = [[VLCOpenGLVoutView alloc] initWithVoutWindow: args->p_wnd container: args->container];

    args->p_wnd->handle.nsobject = oglview;
    [args->container addVoutSubview: oglview];
}

- (void)dealloc
{
    [objectLock dealloc];
    [super dealloc];
}

- (void)removeFromSuperviewAndRelease
{
    [self removeFromSuperview];
    [self release];
}

- (id) initWithVoutWindow: (vout_window_t *) wnd container: (id <VLCOpenGLVoutEmbedding>) aContainer
{
    if( self = [super initWithFrame: NSMakeRect(0,0,10,10)] )
    {
        p_wnd = wnd;
        container = aContainer;
        objectLock = [[NSLock alloc] init];

    }
    return self;
}

- (void) detachFromVoutWindow
{
    [objectLock lock];
    p_wnd = NULL;
    [objectLock unlock];
}

- (id <VLCOpenGLVoutEmbedding>) container
{
    return container;
}

- (void) destroyVoutWindow
{
    [objectLock lock];
    if( p_wnd )
    {
        vlc_object_release( p_wnd );
        vlc_object_release( p_wnd );
    }
    [objectLock unlock];
}


- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}
@end

#endif
