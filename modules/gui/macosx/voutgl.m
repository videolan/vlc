/*****************************************************************************
 * voutgl.m: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2004, 2007-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Damien Fouilleul <damienf at videolan dot org>
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
#include <string.h>

#include <vlc_common.h>
#include <vlc_keys.h>

#include "intf.h"
#include "vout.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include <AGL/agl.h>

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

    /* Mozilla plugin-related variables (not 64bit compatible) */
    bool                b_embedded;
#ifndef __x86_64__
    AGLContext          agl_ctx;
    AGLDrawable         agl_drawable;
    int                 i_offx, i_offy;
    int                 i_width, i_height;
    WindowRef           theWindow;
    WindowGroupRef      winGroup;
    bool                b_clipped_out;
    Rect                clipBounds, viewBounds;
#endif
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

#ifndef __x86_64__
static int  aglInit   ( vout_thread_t * p_vout );
static void aglEnd    ( vout_thread_t * p_vout );
static int  aglManage ( vout_thread_t * p_vout );
static int  aglControl( vout_thread_t *, int, va_list );
static void aglSwap   ( vout_thread_t * p_vout );
static int  aglLock   ( vout_thread_t * p_vout );
static void aglUnlock ( vout_thread_t * p_vout );
#endif

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

#ifndef __x86_64__
    int i_drawable_agl;
    i_drawable_agl = var_GetInteger( p_vout->p_libvlc, "drawable-agl" );
    if( i_drawable_agl > 0 )
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

        AGLPixelFormat pixFormat;

        p_vout->p_sys->b_embedded = true;

        pixFormat = aglChoosePixelFormat(NULL, 0, ATTRIBUTES);
        if( NULL == pixFormat )
        {
            msg_Err( p_vout, "no screen renderer available for required attributes." );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }
 
        p_vout->p_sys->agl_ctx = aglCreateContext(pixFormat, NULL);
        aglDestroyPixelFormat(pixFormat);
        if( NULL == p_vout->p_sys->agl_ctx )
        {
            msg_Err( p_vout, "cannot create AGL context." );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }
        else
        {
            // tell opengl not to sync buffer swap with vertical retrace (too inefficient)
            GLint param = 0;
            aglSetInteger(p_vout->p_sys->agl_ctx, AGL_SWAP_INTERVAL, &param);
            aglEnable(p_vout->p_sys->agl_ctx, AGL_SWAP_INTERVAL);
        }

        p_vout->pf_init             = aglInit;
        p_vout->pf_end              = aglEnd;
        p_vout->pf_manage           = aglManage;
        p_vout->pf_control          = aglControl;
        p_vout->pf_swap             = aglSwap;
        p_vout->pf_lock             = aglLock;
        p_vout->pf_unlock           = aglUnlock;
    }
    else
    {
#endif
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
#ifndef __x86_64__
    }
#endif
    p_vout->p_sys->b_got_frame = false;

    return VLC_SUCCESS;
}

void CloseVideoGL ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;


#ifndef __x86_64__
    if( p_vout->p_sys->b_embedded )
    {
        /* If the fullscreen window is still open, close it */
        if( p_vout->b_fullscreen )
        {
            p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
            aglManage( p_vout );
            var_SetBool( p_vout->p_parent, "fullscreen", false );
        }
        if( p_vout->p_sys->agl_ctx )
        {
            aglEnd( p_vout );
            aglDestroyContext(p_vout->p_sys->agl_ctx);
        }
    }
    else
#endif
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

@end

/*****************************************************************************
 * embedded AGL context implementation
 *****************************************************************************/

#ifndef __x86_64__

static void aglSetViewport( vout_thread_t *p_vout, Rect viewBounds, Rect clipBounds );
static void aglReshape( vout_thread_t * p_vout );
static OSStatus WindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);

static int aglInit( vout_thread_t * p_vout )
{
    Rect viewBounds;
    Rect clipBounds;
 
    p_vout->p_sys->agl_drawable = (AGLDrawable)
            var_GetInteger( p_vout->p_libvlc, "drawable-agl" );
    aglSetDrawable(p_vout->p_sys->agl_ctx, p_vout->p_sys->agl_drawable);

    viewBounds.top = var_GetInteger( p_vout->p_libvlc, "drawable-view-top" );
    viewBounds.left = var_GetInteger( p_vout->p_libvlc, "drawable-view-left" );
    viewBounds.bottom = var_GetInteger( p_vout->p_libvlc, "drawable-view-bottom" );
    viewBounds.right = var_GetInteger( p_vout->p_libvlc, "drawable-view-right" );
    clipBounds.top = var_GetInteger( p_vout->p_libvlc, "drawable-clip-top" );
    clipBounds.left = var_GetInteger( p_vout->p_libvlc, "drawable-clip-left" );
    clipBounds.bottom = var_GetInteger( p_vout->p_libvlc, "drawable-clip-bottom" );
    clipBounds.right = var_GetInteger( p_vout->p_libvlc, "drawable-clip-right" );

    p_vout->p_sys->b_clipped_out = (clipBounds.top == clipBounds.bottom)
                                 || (clipBounds.left == clipBounds.right);
    if( ! p_vout->p_sys->b_clipped_out )
    {
        aglLock(p_vout);
        aglSetViewport(p_vout, viewBounds, clipBounds);
        aglReshape(p_vout);
        aglUnlock(p_vout);
    }
    p_vout->p_sys->clipBounds = clipBounds;
    p_vout->p_sys->viewBounds = viewBounds;

    return VLC_SUCCESS;
}

static void aglEnd( vout_thread_t * p_vout )
{
    aglSetCurrentContext(NULL);
    if( p_vout->p_sys->theWindow )
    {
        DisposeWindow( p_vout->p_sys->theWindow );
        p_vout->p_sys->theWindow = NULL;
    }
}

static void aglReshape( vout_thread_t * p_vout )
{
    unsigned int x, y;
    unsigned int i_height = p_vout->p_sys->i_height;
    unsigned int i_width  = p_vout->p_sys->i_width;

    vout_PlacePicture(p_vout, i_width, i_height, &x, &y, &i_width, &i_height);

    glViewport( p_vout->p_sys->i_offx + x, p_vout->p_sys->i_offy + y, i_width, i_height );

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

/* private event class */
enum
{
    kEventClassVLCPlugin = 'vlcp',
};
/* private event kinds */
enum
{
    kEventVLCPluginShowFullscreen = 32768,
    kEventVLCPluginHideFullscreen,
};

static void sendEventToMainThread(EventTargetRef target, UInt32 class, UInt32 kind)
{
    EventRef myEvent;
    if( noErr == CreateEvent(NULL, class, kind, 0, kEventAttributeNone, &myEvent) )
    {
        if( noErr == SetEventParameter(myEvent, kEventParamPostTarget, typeEventTargetRef, sizeof(EventTargetRef), &target) )
        {
            PostEventToQueue(GetMainEventQueue(), myEvent, kEventPriorityStandard);
        }
        ReleaseEvent(myEvent);
    }
}

static int aglManage( vout_thread_t * p_vout )
{
    if( p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        aglLock( p_vout );
        aglReshape(p_vout);
        aglUnlock( p_vout );
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;
    }
    if( p_vout->i_changes & VOUT_CROP_CHANGE )
    {
        aglLock( p_vout );
        aglReshape(p_vout);
        aglUnlock( p_vout );
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
    }
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        aglSetDrawable(p_vout->p_sys->agl_ctx, NULL);
        aglLock( p_vout );
        if( p_vout->b_fullscreen )
        {
            /* Close the fullscreen window and resume normal drawing */
            Rect viewBounds;
            Rect clipBounds;

            p_vout->p_sys->agl_drawable = (AGLDrawable)
                    var_GetInteger( p_vout->p_libvlc, "drawable-agl" );

            aglSetDrawable(p_vout->p_sys->agl_ctx, p_vout->p_sys->agl_drawable);

            viewBounds.top = var_GetInteger( p_vout->p_libvlc, "drawable-view-top" );
            viewBounds.left = var_GetInteger( p_vout->p_libvlc, "drawable-view-left" );
            viewBounds.bottom = var_GetInteger( p_vout->p_libvlc, "drawable-view-bottom" );
            viewBounds.right = var_GetInteger( p_vout->p_libvlc, "drawable-view-right" );
            clipBounds.top = var_GetInteger( p_vout->p_libvlc, "drawable-clip-top" );
            clipBounds.left = var_GetInteger( p_vout->p_libvlc, "drawable-clip-left" );
            clipBounds.bottom = var_GetInteger( p_vout->p_libvlc, "drawable-clip-bottom" );
            clipBounds.right = var_GetInteger( p_vout->p_libvlc, "drawable-clip-right" );

            aglSetCurrentContext(p_vout->p_sys->agl_ctx);
            aglSetViewport(p_vout, viewBounds, clipBounds);

            if( p_vout->p_sys->theWindow )
            {
                /* Most Carbon APIs are not thread-safe, therefore delagate some GUI visibilty
                 * update to the main thread */
                sendEventToMainThread(GetWindowEventTarget(p_vout->p_sys->theWindow),
                                      kEventClassVLCPlugin, kEventVLCPluginHideFullscreen);
            }
        }
        else
        {
            /* Go into fullscreen */
            Rect deviceRect;
 
            GDHandle deviceHdl = GetMainDevice();
            deviceRect = (*deviceHdl)->gdRect;
 
            if( !p_vout->p_sys->theWindow )
            {
                /* Create a window */
                WindowAttributes windowAttrs;

                windowAttrs = kWindowStandardDocumentAttributes
                            | kWindowStandardHandlerAttribute
                            | kWindowLiveResizeAttribute
                            | kWindowNoShadowAttribute;
 
                windowAttrs &= (~kWindowResizableAttribute);

                CreateNewWindow(kDocumentWindowClass, windowAttrs, &deviceRect, &p_vout->p_sys->theWindow);
                if( !p_vout->p_sys->winGroup )
                {
                    CreateWindowGroup(0, &p_vout->p_sys->winGroup);
                    SetWindowGroup(p_vout->p_sys->theWindow, p_vout->p_sys->winGroup);
                    SetWindowGroupParent( p_vout->p_sys->winGroup, GetWindowGroupOfClass(kDocumentWindowClass) ) ;
                }
 
                // Window title
                CFStringRef titleKey    = CFSTR("Fullscreen VLC media plugin");
                CFStringRef windowTitle = CFCopyLocalizedString(titleKey, NULL);
                SetWindowTitleWithCFString(p_vout->p_sys->theWindow, windowTitle);
                CFRelease(titleKey);
                CFRelease(windowTitle);
 
                //Install event handler
                static const EventTypeSpec win_events[] = {
                    { kEventClassMouse, kEventMouseDown },
                    { kEventClassMouse, kEventMouseMoved },
                    { kEventClassMouse, kEventMouseUp },
                    { kEventClassWindow, kEventWindowClosed },
                    { kEventClassWindow, kEventWindowBoundsChanged },
                    { kEventClassCommand, kEventCommandProcess },
                    { kEventClassVLCPlugin, kEventVLCPluginShowFullscreen },
                    { kEventClassVLCPlugin, kEventVLCPluginHideFullscreen },
                };
                InstallWindowEventHandler (p_vout->p_sys->theWindow, NewEventHandlerUPP (WindowEventHandler), GetEventTypeCount(win_events), win_events, p_vout, NULL);
            }
            else
            {
                /* just in case device resolution changed */
                SetWindowBounds(p_vout->p_sys->theWindow, kWindowContentRgn, &deviceRect);
            }
            glClear( GL_COLOR_BUFFER_BIT );
            p_vout->p_sys->agl_drawable = (AGLDrawable)GetWindowPort(p_vout->p_sys->theWindow);
            aglSetDrawable(p_vout->p_sys->agl_ctx, p_vout->p_sys->agl_drawable);
            aglSetCurrentContext(p_vout->p_sys->agl_ctx);
            aglSetViewport(p_vout, deviceRect, deviceRect);
            //aglSetFullScreen(p_vout->p_sys->agl_ctx, device_width, device_height, 0, 0);

            if( p_vout->p_sys->theWindow )
            {
                /* Most Carbon APIs are not thread-safe, therefore delagate some GUI visibilty
                 * update to the main thread */
                sendEventToMainThread(GetWindowEventTarget(p_vout->p_sys->theWindow),
                                      kEventClassVLCPlugin, kEventVLCPluginShowFullscreen);
            }
        }
        aglReshape(p_vout);
        aglUnlock( p_vout );
        p_vout->b_fullscreen = !p_vout->b_fullscreen;
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }
    return VLC_SUCCESS;
}

static int aglControl( vout_thread_t *p_vout, int i_query, va_list args )
{
    switch( i_query )
    {
        case VOUT_SET_VIEWPORT:
        {
            Rect viewBounds, clipBounds;
            viewBounds.top = va_arg( args, int);
            viewBounds.left = va_arg( args, int);
            viewBounds.bottom = va_arg( args, int);
            viewBounds.right = va_arg( args, int);
            clipBounds.top = va_arg( args, int);
            clipBounds.left = va_arg( args, int);
            clipBounds.bottom = va_arg( args, int);
            clipBounds.right = va_arg( args, int);
 
            if( !p_vout->b_fullscreen )
            {
                /*
                ** check that the clip rect is not empty, as this is used
                ** by Firefox to prevent a plugin from displaying during
                ** a scrolling event. In this case we just prevent buffers
                ** from being swapped and ignore clipping as this is less
                ** disruptive than a GL geometry change
                */

                p_vout->p_sys->b_clipped_out = (clipBounds.top == clipBounds.bottom)
                                             || (clipBounds.left == clipBounds.right);
                if( ! p_vout->p_sys->b_clipped_out )
                {
                    /* ignore consecutive viewport update with identical parameters */
                    if( memcmp(&clipBounds, &(p_vout->p_sys->clipBounds), sizeof(clipBounds) )
                     && memcmp(&viewBounds, &(p_vout->p_sys->viewBounds), sizeof(viewBounds)) )
                    {
                        aglLock( p_vout );
                        aglSetViewport(p_vout, viewBounds, clipBounds);
                        aglReshape( p_vout );
                        aglUnlock( p_vout );
                        p_vout->p_sys->clipBounds = clipBounds;
                        p_vout->p_sys->viewBounds = viewBounds;
                    }
                }
            }
            return VLC_SUCCESS;
        }

        case VOUT_REDRAW_RECT:
        {
            vout_thread_t * p_parent;
            Rect areaBounds;

            areaBounds.top = va_arg( args, int);
            areaBounds.left = va_arg( args, int);
            areaBounds.bottom = va_arg( args, int);
            areaBounds.right = va_arg( args, int);

            /* Ask the opengl module to redraw */
            p_parent = (vout_thread_t *) p_vout->p_parent;
            if( p_parent && p_parent->pf_display )
            {
                p_parent->pf_display( p_parent, NULL );
            }
            return VLC_SUCCESS;
        }

        default:
            return VLC_EGENERIC;
    }
}

static void aglSwap( vout_thread_t * p_vout )
{
    if( ! p_vout->p_sys->b_clipped_out )
    {
        p_vout->p_sys->b_got_frame = true;
        aglSwapBuffers(p_vout->p_sys->agl_ctx);
    }
    else
    {
        /* drop frame */
        glFlush();
    }
}

/* Enter this function with the p_vout locked */
static void aglSetViewport( vout_thread_t *p_vout, Rect viewBounds, Rect clipBounds )
{
    // mozilla plugin provides coordinates based on port bounds
    // however AGL coordinates are based on window structure region
    // and are vertically flipped
    GLint rect[4];
    CGrafPtr port = (CGrafPtr)p_vout->p_sys->agl_drawable;
    Rect winBounds, clientBounds;

    GetWindowBounds(GetWindowFromPort(port),
        kWindowStructureRgn, &winBounds);
    GetWindowBounds(GetWindowFromPort(port),
        kWindowContentRgn, &clientBounds);

    /* update video clipping bounds in drawable */
    rect[0] = (clientBounds.left-winBounds.left)
            + clipBounds.left;                  // from window left edge
    rect[1] = (winBounds.bottom-winBounds.top)
            - (clientBounds.top-winBounds.top)
            - clipBounds.bottom;                // from window bottom edge
    rect[2] = clipBounds.right-clipBounds.left; // width
    rect[3] = clipBounds.bottom-clipBounds.top; // height

    aglSetInteger(p_vout->p_sys->agl_ctx, AGL_BUFFER_RECT, rect);
    aglEnable(p_vout->p_sys->agl_ctx, AGL_BUFFER_RECT);

    /* update video internal bounds in drawable */
    p_vout->p_sys->i_width  = viewBounds.right-viewBounds.left;
    p_vout->p_sys->i_height = viewBounds.bottom-viewBounds.top;
    p_vout->p_sys->i_offx   = -clipBounds.left - viewBounds.left;
    p_vout->p_sys->i_offy   = clipBounds.bottom + viewBounds.top
                            - p_vout->p_sys->i_height;

    aglUpdateContext(p_vout->p_sys->agl_ctx);
}

//default window event handler
static pascal OSStatus WindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
    OSStatus result = noErr;
    UInt32 class = GetEventClass (event);
    UInt32 kind = GetEventKind (event);
    vout_thread_t *p_vout = (vout_thread_t *)userData;

    result = CallNextEventHandler(nextHandler, event);
    if(class == kEventClassCommand)
    {
        HICommand theHICommand;
        GetEventParameter( event, kEventParamDirectObject, typeHICommand, NULL, sizeof( HICommand ), NULL, &theHICommand );
 
        switch ( theHICommand.commandID )
        {
            default:
                result = eventNotHandledErr;
        }
    }
    else if(class == kEventClassWindow)
    {
        WindowRef     window;
        Rect          rectPort = {0,0,0,0};
 
        GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &window);

        if(window)
        {
            GetPortBounds(GetWindowPort(window), &rectPort);
        }

        switch (kind)
        {
            case kEventWindowClosed:
            case kEventWindowZoomed:
            case kEventWindowBoundsChanged:
                break;
 
            default:
                result = eventNotHandledErr;
        }
    }
    else if(class == kEventClassMouse)
    {
        switch (kind)
        {
            case kEventMouseDown:
            {
                UInt16     button;
 
                GetEventParameter(event, kEventParamMouseButton, typeMouseButton, NULL, sizeof(button), NULL, &button);
                switch (button)
                {
                    case kEventMouseButtonPrimary:
                    {
                        vlc_value_t val;

                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int |= 1;
                        var_Set( p_vout, "mouse-button-down", val );
                        break;
                    }
                    case kEventMouseButtonSecondary:
                    {
                        vlc_value_t val;

                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int |= 2;
                        var_Set( p_vout, "mouse-button-down", val );
                        break;
                    }
                    case kEventMouseButtonTertiary:
                    {
                        vlc_value_t val;

                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int |= 4;
                        var_Set( p_vout, "mouse-button-down", val );
                        break;
                    }
                    default:
                        result = eventNotHandledErr;
                }
                break;
            }

            case kEventMouseUp:
            {
                UInt16     button;
 
                GetEventParameter(event, kEventParamMouseButton, typeMouseButton, NULL, sizeof(button), NULL, &button);
                switch (button)
                {
                    case kEventMouseButtonPrimary:
                    {
                        UInt32 clickCount = 0;
                        GetEventParameter(event, kEventParamClickCount, typeUInt32, NULL, sizeof(clickCount), NULL, &clickCount);
                        if( clickCount > 1 )
                        {
                            vlc_value_t val;

                            val.b_bool = false;
                            var_Set((vout_thread_t *) p_vout->p_parent, "fullscreen", val);
                        }
                        else
                        {
                            vlc_value_t val;

                            var_SetBool( p_vout, "mouse-clicked", true );

                            var_Get( p_vout, "mouse-button-down", &val );
                            val.i_int &= ~1;
                            var_Set( p_vout, "mouse-button-down", val );
                        }
                        break;
                    }
                    case kEventMouseButtonSecondary:
                    {
                        vlc_value_t val;

                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~2;
                        var_Set( p_vout, "mouse-button-down", val );
                        break;
                    }
                    case kEventMouseButtonTertiary:
                    {
                        vlc_value_t val;

                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~2;
                        var_Set( p_vout, "mouse-button-down", val );
                        break;
                    }
                    default:
                        result = eventNotHandledErr;
                }
                break;
            }

            case kEventMouseMoved:
            {
                Point ml;
                vlc_value_t val;

                unsigned int i_x, i_y;
                unsigned int i_height = p_vout->p_sys->i_height;
                unsigned int i_width  = p_vout->p_sys->i_width;

                vout_PlacePicture(p_vout, i_width, i_height, &i_x, &i_y, &i_width, &i_height);

                GetEventParameter(event, kEventParamWindowMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &ml);
 
                val.i_int = ( ((int)ml.h) - i_x ) *
                            p_vout->render.i_width / i_width;
                var_Set( p_vout, "mouse-x", val );

                val.i_int = ( ((int)ml.v) - i_y ) *
                            p_vout->render.i_height / i_height;

                var_Set( p_vout, "mouse-y", val );

                val.b_bool = true;
                var_Set( p_vout, "mouse-moved", val );

                break;
            }
 
            default:
                result = eventNotHandledErr;
        }
    }
    else if(class == kEventClassTextInput)
    {
        switch (kind)
        {
            case kEventTextInputUnicodeForKeyEvent:
            {
                break;
            }
            default:
                result = eventNotHandledErr;
        }
    }
    else if(class == kEventClassVLCPlugin)
    {
        switch (kind)
        {
            case kEventVLCPluginShowFullscreen:
                ShowWindow (p_vout->p_sys->theWindow);
                SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
                //CGDisplayHideCursor(kCGDirectMainDisplay);
                break;
            case kEventVLCPluginHideFullscreen:
                HideWindow (p_vout->p_sys->theWindow);
                SetSystemUIMode( kUIModeNormal, 0);
                CGDisplayShowCursor(kCGDirectMainDisplay);
                break;
            default:
                result = eventNotHandledErr;
                break;
        }
    }
    return result;
}

static int aglLock( vout_thread_t * p_vout )
{
	/* get the underlying CGL context */
    CGLContextObj cglContext;
    if( aglGetCGLContext(p_vout->p_sys->agl_ctx, (void**)&cglContext) )
    {
        if( kCGLNoError == CGLLockContext( cglContext ) )
        {
            aglSetCurrentContext(p_vout->p_sys->agl_ctx);
            return 0;
        }
    }
    return 1;
}

static void aglUnlock( vout_thread_t * p_vout )
{
	/* get the underlying CGL context */
    CGLContextObj cglContext;
    if( aglGetCGLContext(p_vout->p_sys->agl_ctx, (void**)&cglContext) )
    {
        CGLUnlockContext( cglContext );
    }
}

#endif
