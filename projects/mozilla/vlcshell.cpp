/*****************************************************************************
 * vlcshell.cpp: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Mozilla stuff */
#ifdef HAVE_MOZILLA_CONFIG_H
#   include <mozilla-config.h>
#endif

/* This is from mozilla java, do we really need it? */
#if 0
#include <jri.h>
#endif

#include "vlcplugin.h"

/* Enable/disable debugging printf's for X11 resizing */
#undef X11_RESIZE_DEBUG

#define WINDOW_TEXT "Video is loading..."

/*****************************************************************************
 * Unix-only declarations
******************************************************************************/
#ifdef XP_UNIX

static void Redraw( Widget w, XtPointer closure, XEvent *event );
static void ControlHandler( Widget w, XtPointer closure, XEvent *event );
static void Resize( Widget w, XtPointer closure, XEvent *event );

#endif

/*****************************************************************************
 * MacOS-only declarations
******************************************************************************/
#ifdef XP_MACOSX
#endif

/*****************************************************************************
 * Windows-only declarations
 *****************************************************************************/
#ifdef XP_WIN

static LRESULT CALLBACK Manage( HWND p_hwnd, UINT i_msg, WPARAM wpar, LPARAM lpar );

#endif

/******************************************************************************
 * UNIX-only API calls
 *****************************************************************************/
char * NPP_GetMIMEDescription( void )
{
    return PLUGIN_MIMETYPES;
}

NPError NPP_GetValue( NPP instance, NPPVariable variable, void *value )
{

    static char psz_desc[1000];

    /* plugin class variables */
    switch( variable )
    {
        case NPPVpluginNameString:
            *((char **)value) = PLUGIN_NAME;
            return NPERR_NO_ERROR;

        case NPPVpluginDescriptionString:
            snprintf( psz_desc, sizeof(psz_desc), PLUGIN_DESCRIPTION,
                      VLC_Version() );
            *((char **)value) = psz_desc;
            return NPERR_NO_ERROR;

        default:
            /* move on to instance variables ... */
            ;
    }

    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    /* plugin instance variables */

    VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(instance->pdata);
    if( NULL == p_plugin )
    {
        // plugin has not been initialized yet !
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    switch( variable )
    {
        case NPPVpluginScriptableNPObject:
        {
            /* retrieve plugin root class */
            NPClass *scriptClass = p_plugin->getScriptClass();
            if( scriptClass )
            {
                /* create an instance and return it */
                *(NPObject**)value = NPN_CreateObject(instance, scriptClass);
                return NPERR_NO_ERROR;
            }
            break;
        }

        default:
            ;
    }
    return NPERR_GENERIC_ERROR;
}

/*
 * there is some confusion in gecko headers regarding definition of this API
 * NPPVariable is wrongly defined as NPNVariable, which sounds incorrect.
 */

NPError NPP_SetValue( NPP instance, NPNVariable variable, void *value )
{
    return NPERR_GENERIC_ERROR;
}

/******************************************************************************
 * Mac-only API calls
 *****************************************************************************/
#ifdef XP_MACOSX
int16 NPP_HandleEvent( NPP instance, void * event )
{
    static UInt32 lastMouseUp = 0;

    if( instance == NULL )
    {
        return false;
    }

    VlcPlugin *p_plugin = (VlcPlugin*)instance->pdata;

    if( p_plugin == NULL )
    {
        return false;
    }

    EventRecord *myEvent = (EventRecord*)event;

    switch( myEvent->what )
    {
        case nullEvent:
            return true;
        case mouseDown:
        {
            if( (myEvent->when - lastMouseUp) < GetDblTime() )
            {
                /* double click */
                libvlc_instance_t *p_vlc = p_plugin->getVLC();

                if( p_vlc )
                {
                    if( libvlc_playlist_isplaying(p_vlc, NULL) )
                    {
                        libvlc_media_player_t *p_md =
                            libvlc_playlist_get_media_player(p_vlc, NULL);
                        if( p_md )
                        {
                            libvlc_toggle_fullscreen(p_md, NULL);
                            libvlc_media_player_release(p_md);
                        }
                    }
                }
            }
            return true;
        }
        case mouseUp:
            lastMouseUp = myEvent->when;
            return true;
        case keyUp:
        case keyDown:
        case autoKey:
            return true;
        case updateEvt:
        {
            const NPWindow& npwindow = p_plugin->getWindow();
            if( npwindow.window )
            {
                int hasVout = FALSE;
                libvlc_instance_t *p_vlc = p_plugin->getVLC();

                if( p_vlc )
                {
                    if( libvlc_playlist_isplaying(p_vlc, NULL) )
                    {
                        libvlc_media_player_t *p_md =
                            libvlc_playlist_get_media_player(p_vlc, NULL);
                        if( p_md )
                        {
                            hasVout = libvlc_media_player_has_vout(p_md,
                                                                     NULL);
                            if( hasVout )
                            {
                                libvlc_rectangle_t area;
                                area.left = 0;
                                area.top = 0;
                                area.right = npwindow.width;
                                area.bottom = npwindow.height;
                                libvlc_video_redraw_rectangle(p_md, &area,
                                                              NULL);
                            }
                            libvlc_media_player_release(p_md);
                        }
                    }
                }

                if( ! hasVout )
                {
                    /* draw the beautiful "No Picture" */

                    ForeColor(blackColor);
                    PenMode( patCopy );

                    /* seems that firefox forgets to set the following
                     * on occasion (reload) */
                    SetOrigin(((NP_Port *)npwindow.window)->portx,
                              ((NP_Port *)npwindow.window)->porty);

                    Rect rect;
                    rect.left = 0;
                    rect.top = 0;
                    rect.right = npwindow.width;
                    rect.bottom = npwindow.height;
                    PaintRect( &rect );

                    ForeColor(whiteColor);
                    MoveTo( (npwindow.width-80)/ 2  , npwindow.height / 2 );
                    DrawText( WINDOW_TEXT , 0 , strlen(WINDOW_TEXT) );
                }
            }
            return true;
        }
        case activateEvt:
            return false;
        case NPEventType_GetFocusEvent:
        case NPEventType_LoseFocusEvent:
            return true;
        case NPEventType_AdjustCursorEvent:
            return false;
        case NPEventType_MenuCommandEvent:
            return false;
        case NPEventType_ClippingChangedEvent:
            return false;
        case NPEventType_ScrollingBeginsEvent:
            return true;
        case NPEventType_ScrollingEndsEvent:
            return true;
        default:
            ;
    }
    return false;
}
#endif /* XP_MACOSX */

/******************************************************************************
 * General Plug-in Calls
 *****************************************************************************/
NPError NPP_Initialize( void )
{
    return NPERR_NO_ERROR;
}

jref NPP_GetJavaClass( void )
{
    return NULL;
}

void NPP_Shutdown( void )
{
    ;
}

NPError NPP_New( NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                 char* argn[], char* argv[], NPSavedData* saved )
{
    NPError status;

    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    VlcPlugin * p_plugin = new VlcPlugin( instance, mode );
    if( NULL == p_plugin )
    {
        return NPERR_OUT_OF_MEMORY_ERROR;
    }

    status = p_plugin->init(argc, argn, argv);
    if( NPERR_NO_ERROR == status )
    {
        instance->pdata = reinterpret_cast<void*>(p_plugin);
#if 0
        NPN_SetValue(instance, NPPVpluginWindowBool, (void *)false);
        NPN_SetValue(instance, NPPVpluginTransparentBool, (void *)false);
#endif
    }
    else
    {
        delete p_plugin;
    }
    return status;
}

NPError NPP_Destroy( NPP instance, NPSavedData** save )
{
    if( NULL == instance )
        return NPERR_INVALID_INSTANCE_ERROR;

    VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(instance->pdata);
    if( NULL == p_plugin )
        return NPERR_NO_ERROR;

    instance->pdata = NULL;

#if XP_WIN
    HWND win = (HWND)p_plugin->getWindow().window;
    WNDPROC winproc = p_plugin->getWindowProc();
    if( winproc )
    {
        /* reset WNDPROC */
        SetWindowLong( win, GWL_WNDPROC, (LONG)winproc );
    }
#endif

    delete p_plugin;

    return NPERR_NO_ERROR;
}

NPError NPP_SetWindow( NPP instance, NPWindow* window )
{
#ifdef XP_UNIX && !defined(__APPLE__)
    Window control;
    unsigned int i_control_height = 0, i_control_width = 0;
#endif

    if( ! instance )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    /* NPP_SetWindow may be called before NPP_New (Opera) */
    VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(instance->pdata);
    if( NULL == p_plugin )
    {
        /* we should probably show a splash screen here */
        return NPERR_NO_ERROR;
    }
#ifdef XP_UNIX && !defined(__APPLE__)
    control = p_plugin->getControlWindow();
#endif
    libvlc_instance_t *p_vlc = p_plugin->getVLC();

    /*
     * PLUGIN DEVELOPERS:
     *  Before setting window to point to the
     *  new window, you may wish to compare the new window
     *  info to the previous window (if any) to note window
     *  size changes, etc.
     */

    /* retrieve current window */
    NPWindow& curwin = p_plugin->getWindow();

#ifdef XP_MACOSX
    if( window && window->window )
    {
        /* check if plugin has a new parent window */
        CGrafPtr drawable = (((NP_Port*) (window->window))->port);
        if( !curwin.window || drawable != (((NP_Port*) (curwin.window))->port) )
        {
            /* set/change parent window */
            libvlc_video_set_parent(p_vlc, (libvlc_drawable_t)drawable, NULL);
        }

        /* as MacOS X video output is windowless, set viewport */
        libvlc_rectangle_t view, clip;

        /*
        ** browser sets port origin to top-left location of plugin
        ** relative to GrafPort window origin is set relative to document,
        ** which of little use for drawing
        */
        view.top     = ((NP_Port*) (window->window))->porty;
        view.left    = ((NP_Port*) (window->window))->portx;
        view.bottom  = window->height+view.top;
        view.right   = window->width+view.left;
        /* clipRect coordinates are also relative to GrafPort */
        clip.top     = window->clipRect.top;
        clip.left    = window->clipRect.left;
        clip.bottom  = window->clipRect.bottom;
        clip.right   = window->clipRect.right;

        libvlc_video_set_viewport(p_vlc, &view, &clip, NULL);

        /* remember new window */
        p_plugin->setWindow(*window);
    }
    else if( curwin.window ) {
        /* change/set parent */
        libvlc_video_set_parent(p_vlc, 0, NULL);
        curwin.window = NULL;
    }
#endif /* XP_MACOSX */

#ifdef XP_WIN
    if( window && window->window )
    {
        /* check if plugin has a new parent window */
        HWND drawable = (HWND) (window->window);
        if( !curwin.window || drawable != curwin.window )
        {
            /* reset previous window settings */
            HWND oldwin = (HWND)p_plugin->getWindow().window;
            WNDPROC oldproc = p_plugin->getWindowProc();
            if( oldproc )
            {
                /* reset WNDPROC */
                SetWindowLong( oldwin, GWL_WNDPROC, (LONG)oldproc );
            }
            /* attach our plugin object */
            SetWindowLongPtr((HWND)drawable, GWLP_USERDATA,
                             reinterpret_cast<LONG_PTR>(p_plugin));

            /* install our WNDPROC */
            p_plugin->setWindowProc( (WNDPROC)SetWindowLong( drawable,
                                             GWL_WNDPROC, (LONG)Manage ) );

            /* change window style to our liking */
            LONG style = GetWindowLong((HWND)drawable, GWL_STYLE);
            style |= WS_CLIPCHILDREN|WS_CLIPSIBLINGS;
            SetWindowLong((HWND)drawable, GWL_STYLE, style);

            /* change/set parent */
            libvlc_video_set_parent(p_vlc, (libvlc_drawable_t)drawable, NULL);

            /* remember new window */
            p_plugin->setWindow(*window);

            /* Redraw window */
            InvalidateRect( (HWND)drawable, NULL, TRUE );
            UpdateWindow( (HWND)drawable );
        }
    }
    else if ( curwin.window )
    {
        /* reset WNDPROC */
        HWND oldwin = (HWND)curwin.window;
        SetWindowLong( oldwin, GWL_WNDPROC, (LONG)(p_plugin->getWindowProc()) );
        p_plugin->setWindowProc(NULL);
        /* change/set parent */
        libvlc_video_set_parent(p_vlc, 0, NULL);
        curwin.window = NULL;
    }
#endif /* XP_WIN */

#ifdef XP_UNIX
    if( p_plugin->b_toolbar )
    {
        p_plugin->getToolbarSize( &i_control_width, &i_control_height );
    }
    else
    {
        i_control_height = i_control_width = 0;
    }

    if( window && window->window )
    {
        Window  parent   = (Window) window->window;
        if( !curwin.window || (parent != (Window)curwin.window) )
        {
            Display *p_display = ( (NPSetWindowCallbackStruct *)
                                   window->ws_info )->display;

            XResizeWindow( p_display, parent, window->width, window->height );

            int i_blackColor = BlackPixel(p_display, DefaultScreen(p_display));

            /* create windows */
            Window video = XCreateSimpleWindow( p_display, parent, 0, 0,
                           window->width, window->height - i_control_height,
                           0, i_blackColor, i_blackColor );
            Window controls = (Window) NULL;
            if( p_plugin->b_toolbar )
            {
                controls = XCreateSimpleWindow( p_display, parent,
                                0, window->height - i_control_height-1,
                                window->width, i_control_height-1,
                                0, i_blackColor, i_blackColor );
            }

            XMapWindow( p_display, parent );
            XMapWindow( p_display, video );
            if( controls ) { XMapWindow( p_display, controls ); }

            XFlush(p_display);

            /* bind events */
            Widget w = XtWindowToWidget( p_display, parent );

            XtAddEventHandler( w, ExposureMask, FALSE,
                               (XtEventHandler)Redraw, p_plugin );
            XtAddEventHandler( w, StructureNotifyMask, FALSE,
                               (XtEventHandler)Resize, p_plugin );
            XtAddEventHandler( w, ButtonReleaseMask, FALSE,
                               (XtEventHandler)ControlHandler, p_plugin );

            /* callback */
/*
            libvlc_media_player_t *p_md;

            libvlc_exception_t ex;
            libvlc_exception_init(& ex );
            p_md = libvlc_playlist_get_media_player( p_plugin->getVLC(), &ex );
            libvlc_exception_init( &ex );
            libvlc_event_attach( libvlc_media_player_event_manager( p_md, &ex ),
                                 libvlc_MediaPlayerPositionChanged, Redraw, NULL, &ex );
*/

            /* set/change parent window */
            libvlc_video_set_parent( p_vlc, (libvlc_drawable_t) video, NULL );

            /* remember window */
            p_plugin->setWindow( *window );
            p_plugin->setVideoWindow( video );
            if( controls ) { p_plugin->setControlWindow( controls ); }

            Redraw( w, (XtPointer)p_plugin, NULL );
        }
    }
    else if ( curwin.window )
    {
        /* change/set parent */
        libvlc_video_set_parent(p_vlc, 0, NULL);
        curwin.window = NULL;
    }
#endif /* XP_UNIX */

    if( !p_plugin->b_stream )
    {
        if( p_plugin->psz_target )
        {
            if( libvlc_playlist_add( p_vlc, p_plugin->psz_target,
                                     NULL, NULL ) != -1 )
            {
                if( p_plugin->b_autoplay )
                {
                    libvlc_playlist_play(p_vlc, 0, 0, NULL, NULL);
                }
            }
            p_plugin->b_stream = VLC_TRUE;
        }
    }
    return NPERR_NO_ERROR;
}

NPError NPP_NewStream( NPP instance, NPMIMEType type, NPStream *stream,
                       NPBool seekable, uint16 *stype )
{
    if( NULL == instance  )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(instance->pdata);
    if( NULL == p_plugin )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

   /*
   ** Firefox/Mozilla may decide to open a stream from the URL specified
   ** in the SRC parameter of the EMBED tag and pass it to us
   **
   ** since VLC will open the SRC URL as well, we're not interested in
   ** that stream. Otherwise, we'll take it and queue it up in the playlist
   */
    if( !p_plugin->psz_target || strcmp(stream->url, p_plugin->psz_target) )
    {
        /* TODO: use pipes !!!! */
        *stype = NP_ASFILEONLY;
        return NPERR_NO_ERROR;
    }
    return NPERR_GENERIC_ERROR;
}

int32 NPP_WriteReady( NPP instance, NPStream *stream )
{
    /* TODO */
    return 8*1024;
}


int32 NPP_Write( NPP instance, NPStream *stream, int32 offset,
                 int32 len, void *buffer )
{
    /* TODO */
    return len;
}


NPError NPP_DestroyStream( NPP instance, NPStream *stream, NPError reason )
{
    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    return NPERR_NO_ERROR;
}


void NPP_StreamAsFile( NPP instance, NPStream *stream, const char* fname )
{
    if( instance == NULL )
    {
        return;
    }

    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(instance->pdata);
    if( NULL == p_plugin )
    {
        return;
    }

    if( libvlc_playlist_add( p_plugin->getVLC(), fname, stream->url, NULL )
        != -1 )
    {
        if( p_plugin->b_autoplay )
        {
            libvlc_playlist_play( p_plugin->getVLC(), 0, 0, NULL, NULL);
        }
    }
}


void NPP_URLNotify( NPP instance, const char* url,
                    NPReason reason, void* notifyData )
{
    /***** Insert NPP_URLNotify code here *****\
    PluginInstance* p_plugin;
    if (instance != NULL)
        p_plugin = (PluginInstance*) instance->pdata;
    \*********************************************/
}


void NPP_Print( NPP instance, NPPrint* printInfo )
{
    if( printInfo == NULL )
    {
        return;
    }

    if( instance != NULL )
    {
        /***** Insert NPP_Print code here *****\
        PluginInstance* p_plugin = (PluginInstance*) instance->pdata;
        \**************************************/

        if( printInfo->mode == NP_FULL )
        {
            /*
             * PLUGIN DEVELOPERS:
             *  If your plugin would like to take over
             *  printing completely when it is in full-screen mode,
             *  set printInfo->pluginPrinted to TRUE and print your
             *  plugin as you see fit.  If your plugin wants Netscape
             *  to handle printing in this case, set
             *  printInfo->pluginPrinted to FALSE (the default) and
             *  do nothing.  If you do want to handle printing
             *  yourself, printOne is true if the print button
             *  (as opposed to the print menu) was clicked.
             *  On the Macintosh, platformPrint is a THPrint; on
             *  Windows, platformPrint is a structure
             *  (defined in npapi.h) containing the printer name, port,
             *  etc.
             */

            /***** Insert NPP_Print code here *****\
            void* platformPrint =
                printInfo->print.fullPrint.platformPrint;
            NPBool printOne =
                printInfo->print.fullPrint.printOne;
            \**************************************/

            /* Do the default*/
            printInfo->print.fullPrint.pluginPrinted = FALSE;
        }
        else
        {
            /* If not fullscreen, we must be embedded */
            /*
             * PLUGIN DEVELOPERS:
             *  If your plugin is embedded, or is full-screen
             *  but you returned false in pluginPrinted above, NPP_Print
             *  will be called with mode == NP_EMBED.  The NPWindow
             *  in the printInfo gives the location and dimensions of
             *  the embedded plugin on the printed page.  On the
             *  Macintosh, platformPrint is the printer port; on
             *  Windows, platformPrint is the handle to the printing
             *  device context.
             */

            /***** Insert NPP_Print code here *****\
            NPWindow* printWindow =
                &(printInfo->print.embedPrint.window);
            void* platformPrint =
                printInfo->print.embedPrint.platformPrint;
            \**************************************/
        }
    }
}

/******************************************************************************
 * Windows-only methods
 *****************************************************************************/
#if XP_WIN
static LRESULT CALLBACK Manage( HWND p_hwnd, UINT i_msg, WPARAM wpar, LPARAM lpar )
{
    VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(GetWindowLongPtr(p_hwnd, GWLP_USERDATA));

    switch( i_msg )
    {
        case WM_ERASEBKGND:
            return 1L;

        case WM_PAINT:
        {
            PAINTSTRUCT paintstruct;
            HDC hdc;
            RECT rect;

            hdc = BeginPaint( p_hwnd, &paintstruct );

            GetClientRect( p_hwnd, &rect );

            FillRect( hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH) );
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkColor(hdc, RGB(0, 0, 0));
            DrawText( hdc, WINDOW_TEXT, strlen(WINDOW_TEXT), &rect,
                      DT_CENTER|DT_VCENTER|DT_SINGLELINE);

            EndPaint( p_hwnd, &paintstruct );
            return 0L;
        }
        default:
            /* delegate to default handler */
            return CallWindowProc( p_plugin->getWindowProc(), p_hwnd,
                                   i_msg, wpar, lpar );
    }
}
#endif /* XP_WIN */

/******************************************************************************
 * UNIX-only methods
 *****************************************************************************/
#ifdef XP_UNIX
static void Redraw( Widget w, XtPointer closure, XEvent *event )
{
    VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(closure);
    Window control = p_plugin->getControlWindow();
    const NPWindow& window = p_plugin->getWindow();
    GC gc;
    XGCValues gcv;
    unsigned int i_control_height, i_control_width;

    if( p_plugin->b_toolbar )
    {
        p_plugin->showToolbar();
        p_plugin->getToolbarSize( &i_control_width, &i_control_height );
    }
    else
        i_control_height = i_control_width = 0;

    Window video = p_plugin->getVideoWindow();
    Display *p_display = ((NPSetWindowCallbackStruct *)window.ws_info)->display;

    gcv.foreground = BlackPixel( p_display, 0 );
    gc = XCreateGC( p_display, video, GCForeground, &gcv );

    XFillRectangle( p_display, video, gc,
                    0, 0, window.width, window.height - i_control_height);

    gcv.foreground = WhitePixel( p_display, 0 );
    XChangeGC( p_display, gc, GCForeground, &gcv );

    XDrawString( p_display, video, gc,
                 window.width / 2 - 40, (window.height - i_control_height) / 2,
                 WINDOW_TEXT, strlen(WINDOW_TEXT) );
    XFreeGC( p_display, gc );

    if( p_plugin->b_toolbar )
    {
        p_plugin->redrawToolbar();
        p_plugin->hideToolbar();
    }
}

static void ControlHandler( Widget w, XtPointer closure, XEvent *event )
{
    VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(closure);
    const NPWindow& window = p_plugin->getWindow();

    int i_height = window.height;
    int i_width = window.width;
    int i_xPos = event->xbutton.x;
    int i_yPos = event->xbutton.y;

    if( p_plugin && p_plugin->b_toolbar )
    {
        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        libvlc_media_player_t *p_md =
                libvlc_playlist_get_media_player(p_plugin->getVLC(), &ex);
        libvlc_exception_clear( &ex );

        /* jump in the movie */
        if( i_yPos <= (i_height-30) )
        {
            /* if a movie is loaded */
            if( p_md )
            {
                vlc_int64_t f_length;
                libvlc_exception_init( &ex );
                f_length = libvlc_media_player_get_length( p_md, &ex ) / 100;
                libvlc_exception_clear( &ex );

                f_length = (float)f_length *
                           ( ((float)i_xPos-4 ) / ( ((float)i_width-8)/100) );

                libvlc_exception_init( &ex );
                libvlc_media_player_set_time( p_md, f_length, &ex );
                libvlc_exception_clear( &ex );
            }
        }

        /* play/pause toggle */
        if( (i_yPos > (i_height-30)) && (i_xPos > 4) && (i_xPos <= 39) )
        {
            int i_playing;
            libvlc_exception_init( &ex );
            i_playing = libvlc_playlist_isplaying( p_plugin->getVLC(), &ex );
            libvlc_exception_clear( &ex );

            libvlc_exception_init( &ex );
            if( i_playing == 1 )
                libvlc_playlist_pause( p_plugin->getVLC(), &ex );
            else
                libvlc_playlist_play( p_plugin->getVLC(), -1, 0, NULL, &ex );
            libvlc_exception_clear( &ex );
        }

        /* stop */
        if( (i_yPos > (i_height-30)) && (i_xPos > 39) && (i_xPos < 67) )
        {
            libvlc_exception_init( &ex );
            libvlc_playlist_stop( p_plugin->getVLC(), &ex );
            libvlc_exception_clear( &ex );
        }

        /* fullscreen */
        if( (i_yPos > (i_height-30)) && (i_xPos >= 67) && (i_xPos < 94) )
        {
            int i_playing;
            libvlc_exception_init( &ex );
            i_playing = libvlc_playlist_isplaying( p_plugin->getVLC(), &ex );
            libvlc_exception_clear( &ex );

            if( (i_playing == 1) && p_md )
            {
                libvlc_exception_init( &ex );
                libvlc_set_fullscreen( p_md, 1, &ex );
                libvlc_exception_clear( &ex );
            }
        }

        /* mute toggle */
        if( (i_yPos > (i_height-30)) && (i_xPos >= 94) && (i_xPos < 109))
        {
            libvlc_exception_init( &ex );
            libvlc_audio_toggle_mute( p_plugin->getVLC(), &ex );
            libvlc_exception_clear( &ex );
        }

        if( p_md ) libvlc_media_player_release( p_md );
    }
    Redraw( w, closure, event );
}

static void Resize ( Widget w, XtPointer closure, XEvent *event )
{
    VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(closure);
    Window control = p_plugin->getControlWindow();
    const NPWindow& window = p_plugin->getWindow();
    Window  drawable   = p_plugin->getVideoWindow();
    Display *p_display = ((NPSetWindowCallbackStruct *)window.ws_info)->display;

    int i_ret;
    Window root_return, parent_return, * children_return;
    Window base_window;
    unsigned int i_nchildren;
    unsigned int i_control_height, i_control_width;

    if( p_plugin->b_toolbar )
    {
        p_plugin->getToolbarSize( &i_control_width, &i_control_height );
    }
    else
    {
        i_control_height = i_control_width = 0;
    }

#ifdef X11_RESIZE_DEBUG
    XWindowAttributes attr;

    if( event && event->type == ConfigureNotify )
    {
        fprintf( stderr, "vlcshell::Resize() ConfigureNotify %d x %d, "
                 "send_event ? %s\n", event->xconfigure.width,
                 event->xconfigure.height,
                 event->xconfigure.send_event ? "TRUE" : "FALSE" );
    }
#endif /* X11_RESIZE_DEBUG */

    if( ! p_plugin->setSize(window.width, (window.height - i_control_height)) )
    {
        /* size already set */
        return;
    }

    i_ret = XResizeWindow( p_display, drawable,
                           window.width, (window.height - i_control_height) );

#ifdef X11_RESIZE_DEBUG
    fprintf( stderr,
             "vlcshell::Resize() XResizeWindow(owner) returned %d\n", i_ret );

    XGetWindowAttributes ( p_display, drawable, &attr );

    /* X is asynchronous, so the current size reported here is not
       necessarily the requested size as the Resize request may not
       yet have been handled by the plugin host */
    fprintf( stderr, "vlcshell::Resize() current (owner) size %d x %d\n",
             attr.width, attr.height );
#endif /* X11_RESIZE_DEBUG */

    XQueryTree( p_display, drawable,
                &root_return, &parent_return, &children_return,
                &i_nchildren );

    if( i_nchildren > 0 )
    {
        /* XXX: Make assumptions related to the window parenting structure in
           vlc/modules/video_output/x11/xcommon.c */
        base_window = children_return[i_nchildren - 1];

#ifdef X11_RESIZE_DEBUG
        fprintf( stderr, "vlcshell::Resize() got %d children\n", i_nchildren );
        fprintf( stderr, "vlcshell::Resize() got base_window %p\n",
                 base_window );
#endif /* X11_RESIZE_DEBUG */

        i_ret = XResizeWindow( p_display, base_window,
                window.width, ( window.height - i_control_height ) );

#ifdef X11_RESIZE_DEBUG
        fprintf( stderr,
                 "vlcshell::Resize() XResizeWindow(base) returned %d\n",
                 i_ret );

        XGetWindowAttributes( p_display, base_window, &attr );

        fprintf( stderr, "vlcshell::Resize() new size %d x %d\n",
                 attr.width, attr.height );
#endif /* X11_RESIZE_DEBUG */
    }
}

#endif /* XP_UNIX */
