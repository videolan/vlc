/*****************************************************************************
 * vlcplugin.c: a VideoLAN Client plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: vlcplugin.c,v 1.1 2002/07/04 18:11:57 sam Exp $
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

#include "vlcplugin.h"

/*******************************************************************************
 * Unix-only declarations
 ******************************************************************************/
static void Redraw( Widget w, XtPointer closure, XEvent *event );

/*******************************************************************************
 * UNIX-only API calls
 ******************************************************************************/
char* NPP_GetMIMEDescription( void )
{
    return( PLUGIN_MIMETYPES );
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    NPError err = NPERR_NO_ERROR;
    if (variable == NPPVpluginNameString)
        *((char **)value) = PLUGIN_NAME;
    else if (variable == NPPVpluginDescriptionString)
        *((char **)value) = PLUGIN_DESCRIPTION;
    else
        err = NPERR_GENERIC_ERROR;

    return err;
}

/*******************************************************************************
 * General Plug-in Calls
 ******************************************************************************/
NPError NPP_Initialize( void )
{
    fprintf(stderr, "NPP_Initialize\n");
    return NPERR_NO_ERROR;
}

jref NPP_GetJavaClass( void )
{
    return NULL;		/* Java disabled */
}

void NPP_Shutdown( void )
{
    /* Java disabled */
}

NPError NPP_New( NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                 char* argn[], char* argv[], NPSavedData* saved )
{
    NPError result = NPERR_NO_ERROR;
    PluginInstance* This;
    int i_ret;
    int i;

    char *ppsz_foo[] =
    {
        "vlc",
        //"--plugin-path", "/home/sam/videolan/vlc_MAIN/plugins",
        "--vout", "xvideo,x11,dummy",
        "--intf", "dummy",
        "--noaudio",
        //"-v"
    };

    fprintf(stderr, "NPP_New\n");

    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;
        
    instance->pdata = NPN_MemAlloc(sizeof(PluginInstance));
    
    This = (PluginInstance*) instance->pdata;

    if (This == NULL)
        return NPERR_OUT_OF_MEMORY_ERROR;

    {
        /* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
        This->fMode = mode;
        This->fWindow = NULL;

        This->window = 0;
    }

    This->p_vlc = vlc_create();
    if( This->p_vlc == NULL )
    {
        return NPERR_GENERIC_ERROR;
    }

    i_ret = vlc_init( This->p_vlc, sizeof(ppsz_foo)/sizeof(char*), ppsz_foo );
    if( i_ret )
    {
        vlc_destroy( This->p_vlc );
        This->p_vlc = NULL;
        return NPERR_GENERIC_ERROR;
    }

    i_ret = vlc_run( This->p_vlc );
    if( i_ret )
    {
        vlc_end( This->p_vlc );
        vlc_destroy( This->p_vlc );
        This->p_vlc = NULL;
        return NPERR_GENERIC_ERROR;
    }

    This->b_stream = 0;
    This->psz_target = NULL;

    for( i = 0; i < argc ; i++ )
    {
        fprintf(stderr, "arg %i: '%s' = '%s'\n", i, argn[i], argv[i]);
        if(!strcmp(argn[i],"target"))
        {
            fprintf(stderr, "target specified: %s\n", argv[i]);
            This->psz_target = strdup( argv[i] );
        }
        else
        {
            //__config_PutPsz( This->psz_target, argn[i], argv[i] );
        }
    }

    return result;
}

NPError NPP_Destroy( NPP instance, NPSavedData** save )
{
    PluginInstance* This;

    fprintf(stderr, "NPP_Destroy\n");

    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    This = (PluginInstance*) instance->pdata;

    if( This->p_vlc != NULL )
    {
        vlc_stop( This->p_vlc );
        vlc_end( This->p_vlc );
        vlc_destroy( This->p_vlc );
        This->p_vlc = NULL;
    }

    if( This->psz_target )
    {
        free( This->psz_target );
        This->psz_target = NULL;
    }

    if (This != NULL) {
        NPN_MemFree(instance->pdata);
        instance->pdata = NULL;
    }

    return NPERR_NO_ERROR;
}

NPError NPP_SetWindow( NPP instance, NPWindow* window )
{
    NPError result = NPERR_NO_ERROR;
    PluginInstance* This;

    fprintf(stderr, "NPP_SetWindow\n");

    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    This = (PluginInstance*) instance->pdata;

    __config_PutInt( This->p_vlc, "x11-drawable", window->window );
    __config_PutInt( This->p_vlc, "xvideo-drawable", window->window );
    /*
     * PLUGIN DEVELOPERS:
     *	Before setting window to point to the
     *	new window, you may wish to compare the new window
     *	info to the previous window (if any) to note window
     *	size changes, etc.
     */
    
    {
        Widget netscape_widget;

        This->window = (Window) window->window;
        This->x = window->x;
        This->y = window->y;
        This->width = window->width;
        This->height = window->height;
        This->display = ((NPSetWindowCallbackStruct *)window->ws_info)->display;

        netscape_widget = XtWindowToWidget(This->display, This->window);
        XtAddEventHandler(netscape_widget, ExposureMask, FALSE, (XtEventHandler)Redraw, This);
        Redraw(netscape_widget, (XtPointer)This, NULL);
    }

    This->fWindow = window;

#if 1
    if( !This->b_stream )
    {
        This->b_stream = 1;
        if( This->psz_target )
        {
            vlc_add_target( This->p_vlc, This->psz_target, PLAYLIST_APPEND, PLAYLIST_END );
                    /* We loop, dude */
            vlc_add_target( This->p_vlc, "vlc:loop", PLAYLIST_APPEND, PLAYLIST_END );
        }
    }
#endif

    return result;
}


NPError NPP_NewStream( NPP instance, NPMIMEType type, NPStream *stream, 
                       NPBool seekable, uint16 *stype )
{
    PluginInstance* This;

    fprintf(stderr, "NPP_NewStream - FILE mode !!\n");

    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    This = (PluginInstance*) instance->pdata;

    /* We want a *filename* ! */
    *stype = NP_ASFILE;

#if 0
    if( This->b_stream == 0 )
    {
        This->psz_target = strdup( stream->url );
        This->b_stream = 1;
    }
#endif

    return NPERR_NO_ERROR;
}


/* PLUGIN DEVELOPERS:
 *	These next 2 functions are directly relevant in a plug-in which
 *	handles the data in a streaming manner. If you want zero bytes
 *	because no buffer space is YET available, return 0. As long as
 *	the stream has not been written to the plugin, Navigator will
 *	continue trying to send bytes.  If the plugin doesn't want them,
 *	just return some large number from NPP_WriteReady(), and
 *	ignore them in NPP_Write().  For a NP_ASFILE stream, they are
 *	still called but can safely be ignored using this strategy.
 */

int32 STREAMBUFSIZE = 0X0FFFFFFF; /* If we are reading from a file in NPAsFile
                   * mode so we can take any size stream in our
                   * write call (since we ignore it) */

#define SARASS_SIZE (1024*1024)

int32 NPP_WriteReady( NPP instance, NPStream *stream )
{
    PluginInstance* This;

    fprintf(stderr, "NPP_WriteReady\n");

    if (instance != NULL)
    {
        This = (PluginInstance*) instance->pdata;
        /* Muahahahahahahaha */
        return STREAMBUFSIZE;
        //return SARASS_SIZE;
    }

    /* Number of bytes ready to accept in NPP_Write() */
    return STREAMBUFSIZE;
    //return 0;
}


int32 NPP_Write( NPP instance, NPStream *stream, int32 offset,
                 int32 len, void *buffer )
{
    fprintf(stderr, "NPP_Write %i\n", len);

    if (instance != NULL)
    {
        //PluginInstance* This = (PluginInstance*) instance->pdata;
    }

    return len;		/* The number of bytes accepted */
}


NPError NPP_DestroyStream( NPP instance, NPStream *stream, NPError reason )
{
    PluginInstance* This;
    fprintf(stderr, "NPP_DestroyStream\n");

    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;
    This = (PluginInstance*) instance->pdata;

    return NPERR_NO_ERROR;
}


void NPP_StreamAsFile( NPP instance, NPStream *stream, const char* fname )
{
    PluginInstance* This;
    fprintf(stderr, "NPP_StreamAsFile\n");
    if (instance != NULL)
    {
        This = (PluginInstance*) instance->pdata;
        vlc_add_target( This->p_vlc, fname, PLAYLIST_APPEND, PLAYLIST_END );
                /* We loop, dude */
        vlc_add_target( This->p_vlc, "vlc:loop", PLAYLIST_APPEND, PLAYLIST_END );
    }
}


void NPP_Print( NPP instance, NPPrint* printInfo )
{
    fprintf(stderr, "NPP_Print\n");

    if(printInfo == NULL)
        return;

    if (instance != NULL) {
        PluginInstance* This = (PluginInstance*) instance->pdata;
    
        if (printInfo->mode == NP_FULL) {
            /*
             * PLUGIN DEVELOPERS:
             *	If your plugin would like to take over
             *	printing completely when it is in full-screen mode,
             *	set printInfo->pluginPrinted to TRUE and print your
             *	plugin as you see fit.  If your plugin wants Netscape
             *	to handle printing in this case, set
             *	printInfo->pluginPrinted to FALSE (the default) and
             *	do nothing.  If you do want to handle printing
             *	yourself, printOne is true if the print button
             *	(as opposed to the print menu) was clicked.
             *	On the Macintosh, platformPrint is a THPrint; on
             *	Windows, platformPrint is a structure
             *	(defined in npapi.h) containing the printer name, port,
             *	etc.
             */

            void* platformPrint =
                printInfo->print.fullPrint.platformPrint;
            NPBool printOne =
                printInfo->print.fullPrint.printOne;
            
            /* Do the default*/
            printInfo->print.fullPrint.pluginPrinted = FALSE;
        }
        else {	/* If not fullscreen, we must be embedded */
            /*
             * PLUGIN DEVELOPERS:
             *	If your plugin is embedded, or is full-screen
             *	but you returned false in pluginPrinted above, NPP_Print
             *	will be called with mode == NP_EMBED.  The NPWindow
             *	in the printInfo gives the location and dimensions of
             *	the embedded plugin on the printed page.  On the
             *	Macintosh, platformPrint is the printer port; on
             *	Windows, platformPrint is the handle to the printing
             *	device context.
             */

            NPWindow* printWindow =
                &(printInfo->print.embedPrint.window);
            void* platformPrint =
                printInfo->print.embedPrint.platformPrint;
        }
    }
}

/*******************************************************************************
// NPP_URLNotify:
// Notifies the instance of the completion of a URL request. 
// 
// NPP_URLNotify is called when Netscape completes a NPN_GetURLNotify or
// NPN_PostURLNotify request, to inform the plug-in that the request,
// identified by url, has completed for the reason specified by reason. The most
// common reason code is NPRES_DONE, indicating simply that the request
// completed normally. Other possible reason codes are NPRES_USER_BREAK,
// indicating that the request was halted due to a user action (for example,
// clicking the "Stop" button), and NPRES_NETWORK_ERR, indicating that the
// request could not be completed (for example, because the URL could not be
// found). The complete list of reason codes is found in npapi.h. 
// 
// The parameter notifyData is the same plug-in-private value passed as an
// argument to the corresponding NPN_GetURLNotify or NPN_PostURLNotify
// call, and can be used by your plug-in to uniquely identify the request. 
 ******************************************************************************/
void NPP_URLNotify( NPP instance, const char* url, NPReason reason,
                    void* notifyData )
{
}

/*******************************************************************************
 * UNIX-only methods
 ******************************************************************************/
static void Redraw( Widget w, XtPointer closure, XEvent *event )
{
    PluginInstance* This = (PluginInstance*)closure;
    GC gc;
    XGCValues gcv;
    const char* text = "hello d00dZ, I'm in void Redraw()";

    XtVaGetValues(w, XtNbackground, &gcv.background,
                  XtNforeground, &gcv.foreground, 0);
    gc = XCreateGC(This->display, This->window, 
                   GCForeground|GCBackground, &gcv);
    XDrawRectangle(This->display, This->window, gc, 
                   0, 0, This->width-1, This->height-1);
    XDrawString(This->display, This->window, gc, 
                This->width/2 - 100, This->height/2,
                text, strlen(text));
    return;
}

