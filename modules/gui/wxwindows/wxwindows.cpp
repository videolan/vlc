/*****************************************************************************
 * wxwindows.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: wxwindows.cpp,v 1.19 2003/07/18 11:39:39 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO

#include <wx/wxprec.h>
#include <wx/wx.h>

#include <vlc/intf.h>

#include "wxwindows.h"

/* Temporary hack */
#ifdef __DARWIN__
int wxEntry( int argc, char *argv[] , bool enterLoop = TRUE );
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static int  OpenDialogs  ( vlc_object_t * );

static void Run          ( intf_thread_t * );
static void Init         ( intf_thread_t * );

static void ShowDialog   ( intf_thread_t *, int, int );

/*****************************************************************************
 * Local classes declarations.
 *****************************************************************************/
class Instance: public wxApp
{
public:
    Instance();
    Instance( intf_thread_t *_p_intf );

    bool OnInit();

private:
    intf_thread_t *p_intf;
    wxLocale locale;                                /* locale we'll be using */
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( (char *) _("wxWindows interface module") );
    set_capability( "interface", 50 );
    set_callbacks( Open, Close );
    add_shortcut( "wxwindows" );
    add_shortcut( "wxwin" );
    add_shortcut( "wx" );
    set_program( "wxvlc" );

    add_submodule();
    set_description( _("wxWindows dialogs provider") );
    set_capability( "dialogs provider", 50 );
    set_callbacks( OpenDialogs, Close );

#if !defined(WIN32)
    linked_with_a_crap_library_which_uses_atexit();
#endif
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return VLC_ENOMEM;
    }

    p_intf->pf_run = Run;

    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    /* Initialize wxWindows thread */
    p_intf->p_sys->b_playing = 0;

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->i_playing = -1;
    p_intf->p_sys->b_slider_free = 1;
    p_intf->p_sys->i_slider_pos = p_intf->p_sys->i_slider_oldpos = 0;

    p_intf->p_sys->p_popup_menu = NULL;

    p_intf->pf_show_dialog = NULL;

    return VLC_SUCCESS;
}

static int OpenDialogs( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    int i_ret = Open( p_this );

    p_intf->pf_show_dialog = ShowDialog;

    return i_ret;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: wxWindows thread
 *****************************************************************************/
#if !defined(__BUILTIN__) && defined( WIN32 )
HINSTANCE hInstance = 0;
extern "C" BOOL WINAPI
DllMain (HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)
{
    hInstance = (HINSTANCE)hModule;
    return TRUE;
}
#endif

static void Run( intf_thread_t *p_intf )
{
    if( p_intf->pf_show_dialog )
    {
        /* The module is used in dialog provider mode */

        /* Create a new thread for wxWindows */
        if( vlc_thread_create( p_intf, "Skins Dialogs Thread",
                               Init, 0, VLC_TRUE ) )
        {
            msg_Err( p_intf, "cannot create Skins Dialogs Thread" );
            p_intf->pf_show_dialog = NULL;
        }
    }
    else
    {
        /* The module is used in interface mode */
        Init( p_intf );
    }
}

static void Init( intf_thread_t *p_intf )
{
#if !defined( WIN32 )
    static char  *p_args[] = { "" };
    int i_args = 1;
#endif

    /* Hack to pass the p_intf pointer to the new wxWindow Instance object */
    wxTheApp = new Instance( p_intf );

#if defined( WIN32 )
#if !defined(__BUILTIN__)
    wxEntry( hInstance/*GetModuleHandle(NULL)*/, NULL, NULL, SW_SHOW );
#else
    wxEntry( GetModuleHandle(NULL), NULL, NULL, SW_SHOW );
#endif
#else
    wxEntry( i_args, p_args );
#endif
}

/* following functions are local */

/*****************************************************************************
 * Constructors.
 *****************************************************************************/
Instance::Instance( )
{
}

Instance::Instance( intf_thread_t *_p_intf )
{
    /* Initialization */
    p_intf = _p_intf;
}

IMPLEMENT_APP_NO_MAIN(Instance)

/*****************************************************************************
 * Instance::OnInit: the parent interface execution starts here
 *****************************************************************************
 * This is the "main program" equivalent, the program execution will
 * start here.
 *****************************************************************************/
bool Instance::OnInit()
{
    /* Initialization of i18n stuff.
     * Usefull for things we don't have any control over, like wxWindows
     * provided facilities (eg. open file dialog) */
    locale.Init( wxLANGUAGE_DEFAULT );

    /* Make an instance of your derived frame. Passing NULL (the default value
     * of Frame's constructor is NULL) as the frame doesn't have a parent
     * since it is the first window */

    if( !p_intf->pf_show_dialog )
    {
        /* The module is used in interface mode */
        Interface *MainInterface = new Interface( p_intf );
        p_intf->p_sys->p_wxwindow = MainInterface;

        /* Show the interface */
        MainInterface->Show( TRUE );

        SetTopWindow( MainInterface );

        /* Start timer */
        new Timer( p_intf, MainInterface );
    }

    /* Creates the dialogs provider */
    p_intf->p_sys->p_wxwindow =
        new DialogsProvider( p_intf, p_intf->pf_show_dialog ?
                             NULL : p_intf->p_sys->p_wxwindow );

    p_intf->p_sys->pf_show_dialog = ShowDialog;

    /* OK, initialization is over */
    vlc_thread_ready( p_intf );

    /* Return TRUE to tell program to continue (FALSE would terminate) */
    return TRUE;
}

static void ShowDialog( intf_thread_t *p_intf, int i_dialog_event, int i_arg )
{
    wxCommandEvent event( wxEVT_DIALOG, i_dialog_event );
    event.SetInt( i_arg );
    p_intf->p_sys->p_wxwindow->AddPendingEvent( event );
}
