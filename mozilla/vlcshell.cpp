/*****************************************************************************
 * vlcshell.c: a VideoLAN Client plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: vlcshell.cpp,v 1.1 2002/09/17 08:18:24 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdio.h>
#include <string.h>

/* Mozilla stuff */
#include <plugin/npapi.h>

/* X11 stuff */
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

/* vlc stuff */
#include <vlc/vlc.h>
#include "config.h"

#include "vlcpeer.h"
#include "vlcplugin.h"

/******************************************************************************
 * Unix-only declarations
 ******************************************************************************/
#ifndef WIN32
static void Redraw( Widget w, XtPointer closure, XEvent *event );
#endif

/******************************************************************************
 * Windows-only declarations
 *****************************************************************************/
#ifdef WIN32
HINSTANCE g_hDllInstance = NULL;

BOOL WINAPI
DllMain( HINSTANCE  hinstDLL,                   // handle of DLL module
                    DWORD  fdwReason,       // reason for calling function
                    LPVOID  lpvReserved)
{
        switch (fdwReason) {
                case DLL_PROCESS_ATTACH:
                  g_hDllInstance = hinstDLL;
                  break;
                case DLL_THREAD_ATTACH:
                case DLL_PROCESS_DETACH:
                case DLL_THREAD_DETACH:
                break;
        }
        return TRUE;
}
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

    switch( variable )
    {
        case NPPVpluginNameString:
            *((char **)value) = PLUGIN_NAME;
            return NPERR_NO_ERROR;

        case NPPVpluginDescriptionString:
            *((char **)value) = PLUGIN_DESCRIPTION;
            return NPERR_NO_ERROR;
    }

    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    VlcPlugin* p_plugin = (VlcPlugin*) instance->pdata;

    switch( variable )
    {
        case NPPVpluginScriptableInstance:
            *(nsISupports**)value = p_plugin->getScriptable();
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

        default:
            return NPERR_GENERIC_ERROR;
    }

    return NPERR_NO_ERROR;
}

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
    int i_ret;
    int i;

    char *ppsz_foo[] =
    {
        "vlc"
        /*, "--plugin-path", "/home/sam/videolan/vlc_MAIN/plugins"*/
        , "--vout", "xvideo,x11,dummy"
        /*, "--aout", "none"*/
        , "--intf", "dummy"
        /*, "--noaudio"*/
        , "-q"
        /*, "-v"*/
    };

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

    p_plugin->fMode = mode;
    p_plugin->fWindow = NULL;
    p_plugin->window = 0;

    p_plugin->p_vlc = vlc_create_r();
    if( p_plugin->p_vlc == NULL )
    {
        delete p_plugin;
        p_plugin = NULL;
        return NPERR_GENERIC_ERROR;
    }

    i_ret = vlc_init_r( p_plugin->p_vlc, sizeof(ppsz_foo)/sizeof(char*), ppsz_foo );
    if( i_ret )
    {
        vlc_destroy_r( p_plugin->p_vlc );
        p_plugin->p_vlc = NULL;
        delete p_plugin;
        p_plugin = NULL;
        return NPERR_GENERIC_ERROR;
    }

    vlc_set_r( p_plugin->p_vlc, "vout", "xvideo,x11,dummy" );
    vlc_set_r( p_plugin->p_vlc, "intf", "dummy" );
    vlc_set_r( p_plugin->p_vlc, "audio", "0" );

    i_ret = vlc_run_r( p_plugin->p_vlc );
    if( i_ret )
    {
        vlc_destroy_r( p_plugin->p_vlc );
        p_plugin->p_vlc = NULL;
        delete p_plugin;
        p_plugin = NULL;
        return NPERR_GENERIC_ERROR;
    }

    p_plugin->b_stream = 0;
    p_plugin->psz_target = NULL;

    for( i = 0; i < argc ; i++ )
    {
        fprintf(stderr, "arg %i: '%s' = '%s'\n", i, argn[i], argv[i]);
        if( !strcmp(argn[i],"target") )
        {
            fprintf(stderr, "target specified: %s\n", argv[i]);
            p_plugin->psz_target = strdup( argv[i] );
        }
        else
        {
            /*vlc_set_r( p_plugin->psz_target, argn[i], argv[i] );*/
        }
    }

    return NPERR_NO_ERROR;
}

NPError NPP_Destroy( NPP instance, NPSavedData** save )
{
    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    VlcPlugin* p_plugin = (VlcPlugin*)instance->pdata;

    if( p_plugin != NULL )
    {
        if( p_plugin->p_vlc != NULL )
        {
            vlc_destroy_r( p_plugin->p_vlc );
            p_plugin->p_vlc = NULL;
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
    char psz_window[32];

    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    VlcPlugin* p_plugin = (VlcPlugin*)instance->pdata;

    /* Write the window ID for vlc */
    sprintf( psz_window, "%li", (long int)window->window );
    vlc_set_r( p_plugin->p_vlc, "x11-drawable", psz_window );
    vlc_set_r( p_plugin->p_vlc, "xvideo-drawable", psz_window );

    /*
     * PLUGIN DEVELOPERS:
     *  Before setting window to point to the
     *  new window, you may wish to compare the new window
     *  info to the previous window (if any) to note window
     *  size changes, etc.
     */

    Widget netscape_widget;

    p_plugin->window = (Window) window->window;
    p_plugin->x = window->x;
    p_plugin->y = window->y;
    p_plugin->width = window->width;
    p_plugin->height = window->height;
    p_plugin->display = ((NPSetWindowCallbackStruct *)window->ws_info)->display;

    netscape_widget = XtWindowToWidget(p_plugin->display, p_plugin->window);
    XtAddEventHandler(netscape_widget, ExposureMask, FALSE, (XtEventHandler)Redraw, p_plugin);
    Redraw(netscape_widget, (XtPointer)p_plugin, NULL);

    p_plugin->fWindow = window;

#if 1
    if( !p_plugin->b_stream )
    {
        p_plugin->b_stream = 1;
        if( p_plugin->psz_target )
        {
            vlc_add_target_r( p_plugin->p_vlc, p_plugin->psz_target,
                              PLAYLIST_APPEND, PLAYLIST_END );
            vlc_add_target_r( p_plugin->p_vlc, "vlc:loop",
                              PLAYLIST_APPEND, PLAYLIST_END );
        }
    }
#endif

    return NPERR_NO_ERROR;
}

NPError NPP_NewStream( NPP instance, NPMIMEType type, NPStream *stream,
                       NPBool seekable, uint16 *stype )
{
    if( instance == NULL )
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    VlcPlugin* p_plugin = (VlcPlugin*)instance->pdata;

    fprintf(stderr, "NPP_NewStream - FILE mode !!\n");

    /* We want a *filename* ! */
    *stype = NP_ASFILE;

#if 0
    if( p_plugin->b_stream == 0 )
    {
        p_plugin->psz_target = strdup( stream->url );
        p_plugin->b_stream = 1;
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

    fprintf(stderr, "NPP_WriteReady\n");

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
    fprintf(stderr, "NPP_Write %i\n", len);

    if (instance != NULL)
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

    VlcPlugin* p_plugin = (VlcPlugin*) instance->pdata;

    return NPERR_NO_ERROR;
}


void NPP_StreamAsFile( NPP instance, NPStream *stream, const char* fname )
{
    if( instance == NULL )
    {
        return;
    }

    VlcPlugin* p_plugin = (VlcPlugin*)instance->pdata;

    fprintf(stderr, "NPP_StreamAsFile\n");
    vlc_add_target_r( p_plugin->p_vlc, fname, PLAYLIST_APPEND, PLAYLIST_END );
    vlc_add_target_r( p_plugin->p_vlc, "vlc:loop", 
                      PLAYLIST_APPEND, PLAYLIST_END );
}

#if 0
void NPP_StreamAsFile( NPP instance, NPStream *stream, const char* fname )
{
    fprintf(stderr,"filename : %s\n", fname);
    ((VlcPlugin*) instance->pdata)->SetFileName(fname);

    fprintf(stderr,"SetFileNeme ok. \n");
}
#endif


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
    if(printInfo == NULL)
        return;

    if (instance != NULL) {
    /***** Insert NPP_Print code here *****\
        PluginInstance* p_plugin = (PluginInstance*) instance->pdata;
    \**************************************/

        if (printInfo->mode == NP_FULL) {
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
        else {  /* If not fullscreen, we must be embedded */
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
 * UNIX-only methods
 *****************************************************************************/
#ifndef WIN32
static void Redraw( Widget w, XtPointer closure, XEvent *event )
{
    VlcPlugin* p_plugin = (VlcPlugin*)closure;
    GC gc;
    XGCValues gcv;
    const char* text = "hello d00dZ, I'm in void Redraw()";

    XtVaGetValues(w, XtNbackground, &gcv.background,
                  XtNforeground, &gcv.foreground, 0);
    gc = XCreateGC(p_plugin->display, p_plugin->window,
                   GCForeground|GCBackground, &gcv);
    XDrawRectangle(p_plugin->display, p_plugin->window, gc,
                   0, 0, p_plugin->width-1, p_plugin->height-1);
    XDrawString(p_plugin->display, p_plugin->window, gc,
                p_plugin->width/2 - 100, p_plugin->height/2,
                text, strlen(text));
    return;
}
#endif

