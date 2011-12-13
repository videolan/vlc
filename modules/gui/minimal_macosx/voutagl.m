/*****************************************************************************
 * voutagl.m: MacOS X agl OpenGL provider (used by webbrowser.plugin)
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
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

#include "intf.h"
#include "voutgl.h"
#include "voutagl.h"

/*****************************************************************************
 * embedded AGL context implementation (not 64bit compatible)
 *****************************************************************************/

#ifndef __x86_64__
static void aglSetViewport( vout_thread_t *p_vout, Rect viewBounds, Rect clipBounds );
static void aglReshape( vout_thread_t * p_vout );
static OSStatus WindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);

int aglInit( vout_thread_t * p_vout )
{
    vlc_value_t val;

    Rect viewBounds;
    Rect clipBounds;

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
        msg_Err( p_vout, "No screen renderer available for required attributes." );
        return VLC_EGENERIC;
    }
 
    p_vout->p_sys->agl_ctx = aglCreateContext(pixFormat, NULL);
    aglDestroyPixelFormat(pixFormat);
    if( NULL == p_vout->p_sys->agl_ctx )
    {
        msg_Err( p_vout, "Cannot create AGL context." );
        return VLC_EGENERIC;
    }
    else
    {
        // tell opengl not to sync buffer swap with vertical retrace (too inefficient)
        GLint param = 0;
        aglSetInteger(p_vout->p_sys->agl_ctx, AGL_SWAP_INTERVAL, &param);
        aglEnable(p_vout->p_sys->agl_ctx, AGL_SWAP_INTERVAL);
    }

    var_Get( p_vout->p_libvlc, "drawable-agl", &val );
    p_vout->p_sys->agl_drawable = (AGLDrawable)val.i_int;
    aglSetDrawable(p_vout->p_sys->agl_ctx, p_vout->p_sys->agl_drawable);

    var_Get( p_vout->p_libvlc, "drawable-view-top", &val );
    viewBounds.top = val.i_int;
    var_Get( p_vout->p_libvlc, "drawable-view-left", &val );
    viewBounds.left = val.i_int;
    var_Get( p_vout->p_libvlc, "drawable-view-bottom", &val );
    viewBounds.bottom = val.i_int;
    var_Get( p_vout->p_libvlc, "drawable-view-right", &val );
    viewBounds.right = val.i_int;
    var_Get( p_vout->p_libvlc, "drawable-clip-top", &val );
    clipBounds.top = val.i_int;
    var_Get( p_vout->p_libvlc, "drawable-clip-left", &val );
    clipBounds.left = val.i_int;
    var_Get( p_vout->p_libvlc, "drawable-clip-bottom", &val );
    clipBounds.bottom = val.i_int;
    var_Get( p_vout->p_libvlc, "drawable-clip-right", &val );
    clipBounds.right = val.i_int;

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

void aglEnd( vout_thread_t * p_vout )
{
    aglSetCurrentContext(NULL);
    if( p_vout->p_sys->theWindow )
        DisposeWindow( p_vout->p_sys->theWindow );
    aglDestroyContext(p_vout->p_sys->agl_ctx);
}

void aglReshape( vout_thread_t * p_vout )
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

int aglManage( vout_thread_t * p_vout )
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
            vlc_value_t val;
            Rect viewBounds;
            Rect clipBounds;

            var_Get( p_vout->p_libvlc, "drawable-agl", &val );
            p_vout->p_sys->agl_drawable = (AGLDrawable)val.i_int;
            aglSetDrawable(p_vout->p_sys->agl_ctx, p_vout->p_sys->agl_drawable);

            var_Get( p_vout->p_libvlc, "drawable-view-top", &val );
            viewBounds.top = val.i_int;
            var_Get( p_vout->p_libvlc, "drawable-view-left", &val );
            viewBounds.left = val.i_int;
            var_Get( p_vout->p_libvlc, "drawable-view-bottom", &val );
            viewBounds.bottom = val.i_int;
            var_Get( p_vout->p_libvlc, "drawable-view-right", &val );
            viewBounds.right = val.i_int;
            var_Get( p_vout->p_libvlc, "drawable-clip-top", &val );
            clipBounds.top = val.i_int;
            var_Get( p_vout->p_libvlc, "drawable-clip-left", &val );
            clipBounds.left = val.i_int;
            var_Get( p_vout->p_libvlc, "drawable-clip-bottom", &val );
            clipBounds.bottom = val.i_int;
            var_Get( p_vout->p_libvlc, "drawable-clip-right", &val );
            clipBounds.right = val.i_int;

            aglSetCurrentContext(p_vout->p_sys->agl_ctx);
            aglSetViewport(p_vout, viewBounds, clipBounds);

            /* Most Carbon APIs are not thread-safe, therefore delagate some GUI visibilty update to the main thread */
            if( p_vout->p_sys->theWindow )
                sendEventToMainThread(GetWindowEventTarget(p_vout->p_sys->theWindow), kEventClassVLCPlugin, kEventVLCPluginHideFullscreen);
        }
        else
        {
            Rect deviceRect;
 
            GDHandle deviceHdl = GetMainDevice();
            deviceRect = (*deviceHdl)->gdRect;
 
            if( !p_vout->p_sys->theWindow )
            {
                /* Create a window */
                WindowAttributes    windowAttrs;

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

            /* Most Carbon APIs are not thread-safe, therefore delagate some GUI visibilty update to the main thread */
            sendEventToMainThread(GetWindowEventTarget(p_vout->p_sys->theWindow), kEventClassVLCPlugin, kEventVLCPluginShowFullscreen);
        }
        aglReshape(p_vout);
        aglUnlock( p_vout );
        p_vout->b_fullscreen = !p_vout->b_fullscreen;
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }
    return VLC_SUCCESS;
}

int aglControl( vout_thread_t *p_vout, int i_query, va_list args )
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

void aglSwap( vout_thread_t * p_vout )
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
                            int x, y;

                            var_GetCoords( p_vout, "mouse-moved", &x, &y );
                            var_SetCoords( p_vout, "mouse-clicked", x, y );

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
                int x, y;

                vout_PlacePicture(p_vout, i_width, i_height, &i_x, &i_y, &i_width, &i_height);

                GetEventParameter(event, kEventParamWindowMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &ml);
 
                x = (((int)ml.h) - i_x) * p_vout->render.i_width / i_width;
                y = (((int)ml.v) - i_y) * p_vout->render.i_height / i_height;
                var_SetCoords( p_vout, "mouse-moved", x, y );
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

int aglLock( vout_thread_t * p_vout )
{
#ifdef __ppc__
    /*
     * before 10.4, we set the AGL context as current and
     * then we retrieve and use the matching CGL context
     */
    aglSetCurrentContext(p_vout->p_sys->agl_ctx);
    return kCGLNoError != CGLLockContext( CGLGetCurrentContext() );
#else
    /* since 10.4, this is the safe way to get the underlying CGL context */
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
#endif
}

void aglUnlock( vout_thread_t * p_vout )
{
#ifdef __ppc__
    /*
     * before 10.4, we assume that the AGL context is current.
     * therefore, we use the current CGL context
     */
    CGLUnlockContext( CGLGetCurrentContext() );
#else
    /* since 10.4, this is the safe way to get the underlying CGL context */
    CGLContextObj cglContext;
    if( aglGetCGLContext(p_vout->p_sys->agl_ctx, (void**)&cglContext) )
    {
        CGLUnlockContext( cglContext );
    }
#endif
}

#endif
