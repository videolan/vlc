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

/* XXX: disable VLC here */
#define USE_LIBVLC 1

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* vlc stuff */
#ifdef USE_LIBVLC
#   include <vlc/vlc.h>
#endif

/* Mozilla stuff */
#ifdef HAVE_MOZILLA_CONFIG_H
#   include <mozilla-config.h>
#endif
#include <nsISupports.h>
#include <nsMemory.h>
#include <npapi.h>
#include <npruntime.h>

/* This is from mozilla java, do we really need it? */
#if 0
#include <jri.h>
#endif

#include "vlcplugin.h"
#include "vlcruntime.h"

#if USE_LIBVLC
#   define WINDOW_TEXT "(no picture)"
#else
#   define WINDOW_TEXT "(no libvlc)"
#endif

/* Enable/disable debugging printf's for X11 resizing */
#undef X11_RESIZE_DEBUG

/*****************************************************************************
 * Unix-only declarations
******************************************************************************/
#ifdef XP_UNIX
#   define VOUT_PLUGINS "xvideo,x11,dummy"
#   define AOUT_PLUGINS "esd,arts,alsa,oss,dummy"

static unsigned int i_previous_height = 100000;
static unsigned int i_previous_width = 100000;

static void Redraw( Widget w, XtPointer closure, XEvent *event );
static void Resize( Widget w, XtPointer closure, XEvent *event );
#endif

/*****************************************************************************
 * MacOS-only declarations
******************************************************************************/
#ifdef XP_MACOSX
#   define VOUT_PLUGINS "macosx,dummy"
#   define AOUT_PLUGINS "auhal,macosx,dummy"

#endif

/*****************************************************************************
 * Windows-only declarations
 *****************************************************************************/
#ifdef XP_WIN
#   define VOUT_PLUGINS "directx,wingdi,dummy"
#   define AOUT_PLUGINS "directx,waveout,dummy"

#if defined(XP_WIN) && !USE_LIBVLC
LRESULT CALLBACK Manage( HWND, UINT, WPARAM, LPARAM );
#endif
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

    static nsIID nsid = VLCINTF_IID;
    static char psz_desc[1000];

    switch( variable )
    {
        case NPPVpluginNameString:
            *((char **)value) = PLUGIN_NAME;
            return NPERR_NO_ERROR;

        case NPPVpluginDescriptionString:
#if USE_LIBVLC
            snprintf( psz_desc, 1000-1, PLUGIN_DESCRIPTION, VLC_Version() );
#else /* USE_LIBVLC */
            snprintf( psz_desc, 1000-1, PLUGIN_DESCRIPTION, "(disabled)" );
#endif /* USE_LIBVLC */
            psz_desc[1000-1] = 0;
            *((char **)value) = psz_desc;
            return NPERR_NO_ERROR;

        default:
            /* go on... */
            break;
    }

    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    VlcPlugin* p_plugin = (VlcPlugin*) instance->pdata;

    switch( variable )
    {
        case NPPVpluginScriptableInstance:
            *(nsISupports**)value = p_plugin->GetPeer();
            if( *(nsISupports**)value == NULL )
            {
                return NPERR_OUT_OF_MEMORY_ERROR;
            }
            break;

        case NPPVpluginScriptableIID:
            *(nsIID**)value = (nsIID*)NPN_MemAlloc( sizeof(nsIID) );
            if( *(nsIID**)value == NULL )
            {
                return NPERR_OUT_OF_MEMORY_ERROR;
            }
            **(nsIID**)value = nsid;
            break;

        case NPPVpluginScriptableNPObject:
            static VlcRuntimeClass<VlcRuntimeRootObject> *rootClass = new VlcRuntimeClass<VlcRuntimeRootObject>;
            *(NPObject**)value = NPN_CreateObject(instance, rootClass);
            if( *(NPObject**)value == NULL )
            {
                return NPERR_OUT_OF_MEMORY_ERROR;
            }
            break;

        default:
            return NPERR_GENERIC_ERROR;
    }

    return NPERR_NO_ERROR;
}

/******************************************************************************
 * Mac-only API calls
 *****************************************************************************/
#ifdef XP_MACOSX
int16 NPP_HandleEvent( NPP instance, void * event )
{
    if( instance == NULL )
    {
        return false;
    }

    VlcPlugin *p_plugin = (VlcPlugin*)instance->pdata;
    EventRecord *myEvent = (EventRecord*)event;

    switch( myEvent->what )
    {
        case nullEvent:
            break;
        case mouseDown:
        case mouseUp:
            return true;
        case keyUp:
        case keyDown:
        case autoKey:
            return true;
        case updateEvt:
        {
            NPWindow *npwindow = p_plugin->window;
            NP_Port *npport = (NP_Port *)(npwindow->window);

            SetPort( npport->port );
            //SetOrigin( npport->portx , npport->porty);

            /* draw the beautiful "No Picture" */

            ForeColor(blackColor);
            PenMode( patCopy );

	    Rect rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = npwindow->width;
            rect.bottom = npwindow->height;
            PaintRect( &rect );

            ForeColor(whiteColor);
            char *text = strdup( WINDOW_TEXT );
            MoveTo( (npwindow->width-80)/ 2  , npwindow->height / 2 );
            DrawText( text , 0 , strlen(text) );
            free(text);

            return true;
        }
        case activateEvt:
            return false;
        case NPEventType_GetFocusEvent:
        case NPEventType_LoseFocusEvent:
            return true;
        case NPEventType_AdjustCursorEvent:
            return false;
	case NPEventType_ScrollingBeginsEvent:
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
    int i;

#if USE_LIBVLC
    vlc_value_t value;
    int i_ret;

#endif /* USE_LIBVLC */

    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    VlcPlugin * p_plugin = new VlcPlugin( instance );

    if( p_plugin == NULL )
    {
        return NPERR_OUT_OF_MEMORY_ERROR;
    }

    instance->pdata = p_plugin;

#ifdef XP_WIN
    p_plugin->p_hwnd = NULL;
    p_plugin->pf_wndproc = NULL;
#endif /* XP_WIN */

#ifdef XP_UNIX
    p_plugin->window = 0;
    p_plugin->p_display = NULL;
#endif /* XP_UNIX */

    p_plugin->p_npwin = NULL;
    p_plugin->i_npmode = mode;
    p_plugin->i_width = 0;
    p_plugin->i_height = 0;

#if USE_LIBVLC
    p_plugin->i_vlc = VLC_Create();
    if( p_plugin->i_vlc < 0 )
    {
        p_plugin->i_vlc = 0;
        delete p_plugin;
        p_plugin = NULL;
        return NPERR_GENERIC_ERROR;
    }

    {
#ifdef XP_MACOSX
        char *ppsz_argv[] =
        {
            "vlc",
            "-vvvv",
            "--plugin-path",
            "/Library/Internet Plug-Ins/VLC Plugin.plugin/"
            "Contents/MacOS/modules"
        };

#elif defined(XP_WIN)
        char *ppsz_argv[] = { NULL, "-vv" };
        HKEY h_key;
        DWORD i_type, i_data = MAX_PATH + 1;
        char p_data[MAX_PATH + 1];
        if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, "Software\\VideoLAN\\VLC",
                          0, KEY_READ, &h_key ) == ERROR_SUCCESS )
        {
             if( RegQueryValueEx( h_key, "InstallDir", 0, &i_type,
                                  (LPBYTE)p_data, &i_data ) == ERROR_SUCCESS )
             {
                 if( i_type == REG_SZ )
                 {
                     strcat( p_data, "\\vlc" );
                     ppsz_argv[0] = p_data;
                 }
             }
             RegCloseKey( h_key );
        }

        if( !ppsz_argv[0] ) ppsz_argv[0] = "vlc";

#else /* XP_MACOSX */
        char *ppsz_argv[] =
        {
            "vlc"
            "-vvvv"
            /*, "--plugin-path", ""*/
        };

#endif /* XP_MACOSX */

        /* HACK: special case for loop, to have it set before playlist startup
         */
        for( i = 0; i < argc ; i++ )
        {
            if( !strcmp( argn[i], "loop" ) )
            {
                if( !strcmp( argv[i], "1" ) || !strcmp( argv[i], "yes" ) )
                {
                    value.b_bool = VLC_TRUE;
                    VLC_VariableSet( p_plugin->i_vlc, "conf::loop", value );
                }
            }
        }

        i_ret = VLC_Init( p_plugin->i_vlc, sizeof(ppsz_argv)/sizeof(char*),
                          ppsz_argv );

    }

    if( i_ret )
    {
        VLC_Destroy( p_plugin->i_vlc );
        p_plugin->i_vlc = 0;
        delete p_plugin;
        p_plugin = NULL;
        return NPERR_GENERIC_ERROR;
    }

    value.psz_string = "dummy";
    VLC_VariableSet( p_plugin->i_vlc, "conf::intf", value );
    value.psz_string = VOUT_PLUGINS;
    VLC_VariableSet( p_plugin->i_vlc, "conf::vout", value );
    value.psz_string = AOUT_PLUGINS;
    VLC_VariableSet( p_plugin->i_vlc, "conf::aout", value );

#else /* USE_LIBVLC */
    p_plugin->i_vlc = 1;

#endif /* USE_LIBVLC */

    p_plugin->b_stream = VLC_FALSE;
    p_plugin->b_autoplay = VLC_FALSE;
    p_plugin->psz_target = NULL;

    for( i = 0; i < argc ; i++ )
    {
        if( !strcmp( argn[i], "target" ) )
        {
            p_plugin->psz_target = argv[i];
        }
        else if( !strcmp( argn[i], "autoplay" ) )
        {
            if( !strcmp( argv[i], "1" ) || !strcmp( argv[i], "yes" ) )
            {
                p_plugin->b_autoplay = 1;
            }
        }
        else if( !strcmp( argn[i], "autostart" ) )
        {
            if( !strcmp( argv[i], "1" ) || !strcmp( argv[i], "true" ) )
            {
                p_plugin->b_autoplay = 1;
            }
        }
        else if( !strcmp( argn[i], "filename" ) )
        {
            p_plugin->psz_target = argv[i];
        }
        else if( !strcmp( argn[i], "src" ) )
        {
            p_plugin->psz_target = argv[i];
        }

#if USE_LIBVLC
        else if( !strcmp( argn[i], "fullscreen" ) )
        {
            if( !strcmp( argv[i], "1" ) || !strcmp( argv[i], "yes" ) )
            {
                value.b_bool = VLC_TRUE;
                VLC_VariableSet( p_plugin->i_vlc, "conf::fullscreen", value );
            }
        }
        else if( !strcmp( argn[i], "mute" ) )
        {
            if( !strcmp( argv[i], "1" ) || !strcmp( argv[i], "yes" ) )
            {
                VLC_VolumeMute( p_plugin->i_vlc );
            }
        }
#endif /* USE_LIBVLC */
    }

    if( p_plugin->psz_target )
    {
        p_plugin->psz_target = strdup( p_plugin->psz_target );
    }

    return NPERR_NO_ERROR;
}

#ifdef XP_WIN
/* This is really ugly but there is a deadlock when stopping a stream
 * (in VLC_CleanUp()) because the video output is a child of the drawable but
 * is in a different thread. */
static void HackStopVout( VlcPlugin* p_plugin )
{
    MSG msg;
    HWND hwnd;
    vlc_value_t value;

    VLC_VariableGet( p_plugin->i_vlc, "drawable", &value );

    hwnd = FindWindowEx( (HWND)value.i_int, 0, 0, 0 );
    if( !hwnd ) return;

    PostMessage( hwnd, WM_CLOSE, 0, 0 );

    do
    {
        while( PeekMessage( &msg, (HWND)value.i_int, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if( FindWindowEx( (HWND)value.i_int, 0, 0, 0 ) ) Sleep( 10 );
    }
    while( (hwnd = FindWindowEx( (HWND)value.i_int, 0, 0, 0 )) );
}
#endif /* XP_WIN */

NPError NPP_Destroy( NPP instance, NPSavedData** save )
{
    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    VlcPlugin* p_plugin = (VlcPlugin*)instance->pdata;

    if( p_plugin != NULL )
    {
        if( p_plugin->i_vlc )
        {
#if USE_LIBVLC
#   ifdef XP_WIN
            HackStopVout( p_plugin );
#   endif /* XP_WIN */
            VLC_CleanUp( p_plugin->i_vlc );
            VLC_Destroy( p_plugin->i_vlc );
#endif /* USE_LIBVLC */
            p_plugin->i_vlc = 0;
        }

        if( p_plugin->psz_target )
        {
            free( p_plugin->psz_target );
            p_plugin->psz_target = NULL;
        }

        delete p_plugin;
    }

    instance->pdata = NULL;

    return NPERR_NO_ERROR;
}

NPError NPP_SetWindow( NPP instance, NPWindow* window )
{
    vlc_value_t value;
#ifdef XP_MACOSX
    vlc_value_t valuex;
    vlc_value_t valuey;
    vlc_value_t valuew;
    vlc_value_t valueh;
    vlc_value_t valuet;
    vlc_value_t valuel;
    vlc_value_t valueb;
    vlc_value_t valuer;
    vlc_value_t valueportx;
    vlc_value_t valueporty;
    vlc_value_t valueredraw;
#endif /* XP_MACOSX */

    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    VlcPlugin* p_plugin = (VlcPlugin*)instance->pdata;

    /* Write the window ID for vlc */
#if USE_LIBVLC

#ifdef XP_MACOSX
    value.i_int = (int)(((NP_Port*) (window->window))->port);
    VLC_VariableSet( p_plugin->i_vlc, "drawable", value );

    valueportx.i_int = ((NP_Port*) (window->window))->portx;
    valueporty.i_int = ((NP_Port*) (window->window))->porty;
    VLC_VariableSet( p_plugin->i_vlc, "drawableportx", valueportx );
    VLC_VariableSet( p_plugin->i_vlc, "drawableporty", valueporty );

    valuex.i_int = window->x;
    valuey.i_int = window->y;
    valuew.i_int = window->width;
    valueh.i_int = window->height;
    valuet.i_int = window->clipRect.top;
    valuel.i_int = window->clipRect.left;
    valueb.i_int = window->clipRect.bottom;
    valuer.i_int = window->clipRect.right;

    VLC_VariableSet( p_plugin->i_vlc, "drawablet", valuet );
    VLC_VariableSet( p_plugin->i_vlc, "drawablel", valuel );
    VLC_VariableSet( p_plugin->i_vlc, "drawableb", valueb );
    VLC_VariableSet( p_plugin->i_vlc, "drawabler", valuer );
    VLC_VariableSet( p_plugin->i_vlc, "drawablex", valuex );
    VLC_VariableSet( p_plugin->i_vlc, "drawabley", valuey );
    VLC_VariableSet( p_plugin->i_vlc, "drawablew", valuew );
    VLC_VariableSet( p_plugin->i_vlc, "drawableh", valueh );

    p_plugin->window = window;

    valueredraw.i_int = 1;
    VLC_VariableSet( p_plugin->i_vlc, "drawableredraw", valueredraw );

#else /* XP_MACOSX */
    /* FIXME: this cast sucks */
    value.i_int = (int) (ptrdiff_t) (void *) window->window;
    VLC_VariableSet( p_plugin->i_vlc, "drawable", value );

#endif /* XP_MACOSX */

#endif /* USE_LIBVLC */

    /*
     * PLUGIN DEVELOPERS:
     *  Before setting window to point to the
     *  new window, you may wish to compare the new window
     *  info to the previous window (if any) to note window
     *  size changes, etc.
     */

#ifdef XP_WIN
    if( !window || !window->window )
    {
        /* Window was destroyed. Invalidate everything. */
        if( p_plugin->p_npwin )
        {
#if !USE_LIBVLC
            SetWindowLong( p_plugin->p_hwnd, GWL_WNDPROC,
                           (LONG)p_plugin->pf_wndproc );
#endif /* !USE_LIBVLC */
            p_plugin->pf_wndproc = NULL;
            p_plugin->p_hwnd = NULL;
        }

        p_plugin->p_npwin = window;
        return NPERR_NO_ERROR;
    }

    if( p_plugin->p_npwin )
    {
        if( p_plugin->p_hwnd == (HWND)window->window )
        {
            /* Same window, but something may have changed. First we
             * update the plugin structure, then we redraw the window */
            p_plugin->i_width = window->width;
            p_plugin->i_height = window->height;
            p_plugin->p_npwin = window;
#if !USE_LIBVLC
            InvalidateRect( p_plugin->p_hwnd, NULL, TRUE );
            UpdateWindow( p_plugin->p_hwnd );
#endif /* !USE_LIBVLC */
            return NPERR_NO_ERROR;
        }

        /* Window has changed. Destroy the one we have, and go
         * on as if it was a real initialization. */
#if !USE_LIBVLC
        SetWindowLong( p_plugin->p_hwnd, GWL_WNDPROC,
                       (LONG)p_plugin->pf_wndproc );
#endif /* !USE_LIBVLC */
        p_plugin->pf_wndproc = NULL;
        p_plugin->p_hwnd = NULL;
    }

#if !USE_LIBVLC
    p_plugin->pf_wndproc = (WNDPROC)SetWindowLong( (HWND)window->window,
                                                   GWL_WNDPROC, (LONG)Manage );
#endif /* !USE_LIBVLC */

    p_plugin->p_hwnd = (HWND)window->window;
    SetProp( p_plugin->p_hwnd, "w00t", (HANDLE)p_plugin );
    InvalidateRect( p_plugin->p_hwnd, NULL, TRUE );
    UpdateWindow( p_plugin->p_hwnd );
#endif /* XP_WIN */

    p_plugin->i_width = window->width;
    p_plugin->i_height = window->height;
    p_plugin->p_npwin = window;

#ifdef XP_UNIX
    p_plugin->window = (Window) window->window;
    p_plugin->p_display =
        ((NPSetWindowCallbackStruct *)window->ws_info)->display;

    XResizeWindow( p_plugin->p_display, p_plugin->window,
                   p_plugin->i_width, p_plugin->i_height );
    Widget w = XtWindowToWidget( p_plugin->p_display, p_plugin->window );

    XtAddEventHandler( w, ExposureMask, FALSE,
                       (XtEventHandler)Redraw, p_plugin );
    XtAddEventHandler( w, StructureNotifyMask, FALSE,
                       (XtEventHandler)Resize, p_plugin );
    Redraw( w, (XtPointer)p_plugin, NULL );
#endif /* XP_UNIX */

    if( !p_plugin->b_stream )
    {
        int i_mode = PLAYLIST_APPEND;

        if( p_plugin->b_autoplay )
        {
            i_mode |= PLAYLIST_GO;
        }

        if( p_plugin->psz_target )
        {
#if USE_LIBVLC
            VLC_AddTarget( p_plugin->i_vlc, p_plugin->psz_target,
                           0, 0, PLAYLIST_INSERT, 0 );
#endif
            p_plugin->b_stream = VLC_TRUE;
        }
    }

    return NPERR_NO_ERROR;
}

NPError NPP_NewStream( NPP instance, NPMIMEType type, NPStream *stream,
                       NPBool seekable, uint16 *stype )
{
    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

#if 0
    VlcPlugin* p_plugin = (VlcPlugin*)instance->pdata;
#endif

    /* fprintf(stderr, "NPP_NewStream - FILE mode !!\n"); */

    /* We want a *filename* ! */
    *stype = NP_ASFILE;

#if 0
    if( !p_plugin->b_stream )
    {
        p_plugin->psz_target = strdup( stream->url );
        p_plugin->b_stream = VLC_TRUE;
    }
#endif

    return NPERR_NO_ERROR;
}

int32 STREAMBUFSIZE = 0X0FFFFFFF; /* If we are reading from a file in NPAsFile
                   * mode so we can take any size stream in our
                   * write call (since we ignore it) */

#define SARASS_SIZE (1024*1024)

int32 NPP_WriteReady( NPP instance, NPStream *stream )
{
    VlcPlugin* p_plugin;

    /* fprintf(stderr, "NPP_WriteReady\n"); */

    if (instance != NULL)
    {
        p_plugin = (VlcPlugin*) instance->pdata;
        /* Muahahahahahahaha */
        return STREAMBUFSIZE;
        /*return SARASS_SIZE;*/
    }

    /* Number of bytes ready to accept in NPP_Write() */
    return STREAMBUFSIZE;
    /*return 0;*/
}


int32 NPP_Write( NPP instance, NPStream *stream, int32 offset,
                 int32 len, void *buffer )
{
    /* fprintf(stderr, "NPP_Write %i\n", (int)len); */

    if( instance != NULL )
    {
        /*VlcPlugin* p_plugin = (VlcPlugin*) instance->pdata;*/
    }

    return len;         /* The number of bytes accepted */
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

    /* fprintf(stderr, "NPP_StreamAsFile %s\n", fname); */

#if USE_LIBVLC
    VlcPlugin* p_plugin = (VlcPlugin*)instance->pdata;

    VLC_AddTarget( p_plugin->i_vlc, fname, 0, 0,
                   PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
#endif /* USE_LIBVLC */
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
#if defined(XP_WIN) && !USE_LIBVLC
LRESULT CALLBACK Manage( HWND p_hwnd, UINT i_msg, WPARAM wpar, LPARAM lpar )
{
    VlcPlugin* p_plugin = (VlcPlugin*) GetProp( p_hwnd, "w00t" );

    switch( i_msg )
    {
        case WM_PAINT:
        {
            PAINTSTRUCT paintstruct;
            HDC hdc;
            RECT rect;

            hdc = BeginPaint( p_hwnd, &paintstruct );

            GetClientRect( p_hwnd, &rect );
            FillRect( hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH) );
            TextOut( hdc, p_plugin->i_width / 2 - 40, p_plugin->i_height / 2,
                     WINDOW_TEXT, strlen(WINDOW_TEXT) );

            EndPaint( p_hwnd, &paintstruct );
            break;
        }
        default:
            p_plugin->pf_wndproc( p_hwnd, i_msg, wpar, lpar );
            break;
    }
    return 0;
}
#endif /* XP_WIN */

/******************************************************************************
 * UNIX-only methods
 *****************************************************************************/
#ifdef XP_UNIX
static void Redraw( Widget w, XtPointer closure, XEvent *event )
{
    VlcPlugin* p_plugin = (VlcPlugin*)closure;
    GC gc;
    XGCValues gcv;

    gcv.foreground = BlackPixel( p_plugin->p_display, 0 );
    gc = XCreateGC( p_plugin->p_display, p_plugin->window, GCForeground, &gcv );

    XFillRectangle( p_plugin->p_display, p_plugin->window, gc,
                    0, 0, p_plugin->i_width, p_plugin->i_height );

    gcv.foreground = WhitePixel( p_plugin->p_display, 0 );
    XChangeGC( p_plugin->p_display, gc, GCForeground, &gcv );

    XDrawString( p_plugin->p_display, p_plugin->window, gc,
                 p_plugin->i_width / 2 - 40, p_plugin->i_height / 2,
                 WINDOW_TEXT, strlen(WINDOW_TEXT) );

    XFreeGC( p_plugin->p_display, gc );
}

static void Resize ( Widget w, XtPointer closure, XEvent *event )
{
    VlcPlugin* p_plugin = (VlcPlugin*)closure;
    int i_ret;
    Window root_return, parent_return, * children_return;
    Window base_window;
    unsigned int i_nchildren;

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

    if( p_plugin->i_height == i_previous_height &&
        p_plugin->i_width == i_previous_width )
    {
        return;
    }
    i_previous_height = p_plugin->i_height;
    i_previous_width  = p_plugin->i_width;


    i_ret = XResizeWindow( p_plugin->p_display, p_plugin->window,
            p_plugin->i_width, p_plugin->i_height );

#ifdef X11_RESIZE_DEBUG
    fprintf( stderr,
             "vlcshell::Resize() XResizeWindow(owner) returned %d\n", i_ret );

    XGetWindowAttributes ( p_plugin->p_display, p_plugin->window, &attr );

    /* X is asynchronous, so the current size reported here is not
       necessarily the requested size as the Resize request may not
       yet have been handled by the plugin host */
    fprintf( stderr, "vlcshell::Resize() current (owner) size %d x %d\n",
             attr.width, attr.height );
#endif /* X11_RESIZE_DEBUG */

    XQueryTree( p_plugin->p_display, p_plugin->window,
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

        i_ret = XResizeWindow( p_plugin->p_display, base_window,
                p_plugin->i_width, p_plugin->i_height );

#ifdef X11_RESIZE_DEBUG
        fprintf( stderr,
                 "vlcshell::Resize() XResizeWindow(base) returned %d\n",
                 i_ret );

        XGetWindowAttributes( p_plugin->p_display, base_window, &attr );

        fprintf( stderr, "vlcshell::Resize() new size %d x %d\n",
                 attr.width, attr.height );
#endif /* X11_RESIZE_DEBUG */
    }
}

#endif /* XP_UNIX */

