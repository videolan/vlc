/*****************************************************************************
 * wxwidgets.cpp : wxWidgets plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id$
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
#include <vlc/intf.h>

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif

#include "wxwidgets.h"

/* Temporary hack */
#if defined(WIN32) && defined(_WX_INIT_H_)
#if (wxMAJOR_VERSION <= 2) && (wxMINOR_VERSION <= 5) && (wxRELEASE_NUMBER < 3)
/* Hack to detect wxWidgets 2.5 which has a different wxEntry() prototype */
extern int wxEntry( HINSTANCE hInstance, HINSTANCE hPrevInstance = NULL,
                    char *pCmdLine = NULL, int nCmdShow = SW_NORMAL );
#endif
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static int  OpenDialogs  ( vlc_object_t * );

static void Run          ( intf_thread_t * );
static void Init         ( intf_thread_t * );

static void ShowDialog   ( intf_thread_t *, int, int, intf_dialog_args_t * );

#if (wxCHECK_VERSION(2,5,0))
void *wxClassInfo_sm_classTable_BUGGY = 0;
#endif

/*****************************************************************************
 * Local classes declarations.
 *****************************************************************************/
class Instance: public wxApp
{
public:
    Instance();
    Instance( intf_thread_t *_p_intf );

    bool OnInit();
    int  OnExit();

private:
    intf_thread_t *p_intf;
    wxLocale locale;                                /* locale we'll be using */
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define EMBED_TEXT N_("Embed video in interface")
#define EMBED_LONGTEXT N_("Embed the video inside the interface instead " \
    "of having it in a separate window.")
#define BOOKMARKS_TEXT N_("Show bookmarks dialog")
#define BOOKMARKS_LONGTEXT N_("Show bookmarks dialog when the interface " \
    "starts.")
#define EXTENDED_TEXT N_("Show extended GUI")
#define EXTENDED_LONGTEXT N_("Show extended GUI")
#define TASKBAR_TEXT N_("Show taskbar entry")
#define TASKBAR_LONGTEXT N_("Show taskbar entry")
#define MINIMAL_TEXT N_("Minimal interface")
#define MINIMAL_LONGTEXT N_("Use minimal interface, no toolbar, few menus")
#define SIZE_TO_VIDEO_TEXT N_("Size to video")
#define SIZE_TO_VIDEO_LONGTEXT N_("Resize VLC to match the video resolution")
#define SYSTRAY_TEXT N_("Show systray icon")
#define SYSTRAY_LONGTEXT N_("Show systray icon")

vlc_module_begin();
#ifdef WIN32
    int i_score = 150;
#else
    int i_score = getenv( "DISPLAY" ) == NULL ? 15 : 150;
#endif
    set_shortname( (char*) "wxWidgets" );
    set_description( (char *) _("wxWidgets interface module") );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_GENERAL );
    set_capability( "interface", i_score );
    set_callbacks( Open, Close );
    add_shortcut( "wxwindows" );
    add_shortcut( "wxwin" );
    add_shortcut( "wx" );
    add_shortcut( "wxwidgets" );
    set_program( "wxvlc" );

    add_bool( "wx-embed", 1, NULL,
              EMBED_TEXT, EMBED_LONGTEXT, VLC_FALSE );
    add_deprecated( "wxwin-enbed", VLC_FALSE); /*Deprecated since 0.8.4*/
    add_bool( "wx-bookmarks", 0, NULL,
              BOOKMARKS_TEXT, BOOKMARKS_LONGTEXT, VLC_FALSE );
    add_deprecated( "wxwin-bookmarks", VLC_FALSE); /*Deprecated since 0.8.4*/
    add_bool( "wx-taskbar", 1, NULL,
              TASKBAR_TEXT, TASKBAR_LONGTEXT, VLC_FALSE );
    add_deprecated( "wxwin-taskbar", VLC_FALSE); /*Deprecated since 0.8.4*/
    add_bool( "wx-extended", 0, NULL,
              EXTENDED_TEXT, EXTENDED_LONGTEXT, VLC_FALSE );
    add_bool( "wx-minimal", 0, NULL,
              MINIMAL_TEXT, MINIMAL_LONGTEXT, VLC_TRUE );
    add_deprecated( "wxwin-minimal", VLC_FALSE); /*Deprecated since 0.8.4*/
    add_bool( "wx-autosize", 1, NULL,
              SIZE_TO_VIDEO_TEXT, SIZE_TO_VIDEO_LONGTEXT, VLC_TRUE );
    add_deprecated( "wxwin-autosize", VLC_FALSE); /*Deprecated since 0.8.4*/
#ifdef wxHAS_TASK_BAR_ICON
    add_bool( "wx-systray", 0, NULL,
              SYSTRAY_TEXT, SYSTRAY_LONGTEXT, VLC_FALSE );
    add_deprecated( "wxwin-systray", VLC_FALSE); /*Deprecated since 0.8.4*/
#endif
    add_string( "wx-config-last", NULL, NULL,
                "last config", "last config", VLC_TRUE );
        change_autosave();
    add_deprecated( "wxwin-config-last", VLC_FALSE); /*Deprecated since 0.8.4*/

    add_submodule();
    set_description( _("wxWidgets dialogs provider") );
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
    memset( p_intf->p_sys, 0, sizeof( intf_sys_t ) );

    p_intf->pf_run = Run;

    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    /* Initialize wxWidgets thread */
    p_intf->p_sys->b_playing = 0;

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->i_playing = -1;
    p_intf->p_sys->b_slider_free = 1;
    p_intf->p_sys->i_slider_pos = p_intf->p_sys->i_slider_oldpos = 0;

    p_intf->p_sys->p_popup_menu = NULL;
    p_intf->p_sys->p_video_window = NULL;

    p_intf->pf_show_dialog = NULL;

    /* We support play on start */
    p_intf->b_play = VLC_TRUE;

    p_intf->p_sys->b_video_autosize =
        config_GetInt( p_intf, "wx-autosize" );

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

    vlc_mutex_lock( &p_intf->object_lock );
    p_intf->b_dead = VLC_TRUE;
    vlc_mutex_unlock( &p_intf->object_lock );

    if( p_intf->pf_show_dialog )
    {
        /* We must destroy the dialogs thread */
        wxCommandEvent event( wxEVT_DIALOG, INTF_DIALOG_EXIT );
        p_intf->p_sys->p_wxwindow->AddPendingEvent( event );
        vlc_thread_join( p_intf );
    }

    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

    /* */
    delete p_intf->p_sys->p_window_settings;

#if (wxCHECK_VERSION(2,5,0))
    wxClassInfo::sm_classTable = (wxHashTable*)wxClassInfo_sm_classTable_BUGGY;
#endif

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: wxWidgets thread
 *****************************************************************************/

//when is this called?
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

        /* Create a new thread for wxWidgets */
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

    /* Hack to pass the p_intf pointer to the new wxWidgets Instance object */
#ifdef wxTheApp
    wxApp::SetInstance( new Instance( p_intf ) );
#else
    wxTheApp = new Instance( p_intf );
#endif

#if defined( WIN32 )
#if !defined(__BUILTIN__)

    //because no one knows when DllMain is called
    if (hInstance == NULL)
      hInstance = GetModuleHandle(NULL);

    wxEntry( hInstance/*GetModuleHandle(NULL)*/, NULL, NULL, SW_SHOW );
#else
    wxEntry( GetModuleHandle(NULL), NULL, NULL, SW_SHOW );
#endif
#else
    wxEntry( i_args, p_args );
#endif
    setlocale( LC_NUMERIC, "C" );
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
     * Usefull for things we don't have any control over, like wxWidgets
     * provided facilities (eg. open file dialog) */
    locale.Init( wxLANGUAGE_DEFAULT, wxLOCALE_LOAD_DEFAULT );
    setlocale( LC_NUMERIC, "C" );

    /* Load saved window settings */
    p_intf->p_sys->p_window_settings = new WindowSettings( p_intf );

    /* Make an instance of your derived frame. Passing NULL (the default value
     * of Frame's constructor is NULL) as the frame doesn't have a parent
     * since it is the first window */

    if( !p_intf->pf_show_dialog )
    {
        /* The module is used in interface mode */
        long style = wxDEFAULT_FRAME_STYLE;
        if ( ! config_GetInt( p_intf, "wx-taskbar" ) )
        {
            style = wxDEFAULT_FRAME_STYLE|wxFRAME_NO_TASKBAR;
        }

        Interface *MainInterface = new Interface( p_intf, style );
        p_intf->p_sys->p_wxwindow = MainInterface;

        /* Show the interface */
        MainInterface->Show( TRUE );
        SetTopWindow( MainInterface );
        MainInterface->Raise();
    }

    /* Creates the dialogs provider */
    p_intf->p_sys->p_wxwindow =
        CreateDialogsProvider( p_intf, p_intf->pf_show_dialog ?
                               NULL : p_intf->p_sys->p_wxwindow );

    p_intf->p_sys->pf_show_dialog = ShowDialog;

    /* OK, initialization is over */
    vlc_thread_ready( p_intf );

    /* Check if we need to start playing */
    if( !p_intf->pf_show_dialog && p_intf->b_play )
    {
        playlist_t *p_playlist =
            (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
        if( p_playlist )
        {
            playlist_LockControl( p_playlist, PLAYLIST_AUTOPLAY );
            vlc_object_release( p_playlist );
        }
    }

    /* Return TRUE to tell program to continue (FALSE would terminate) */
    return TRUE;
}

/*****************************************************************************
 * Instance::OnExit: called when the interface execution stops
 *****************************************************************************/
int Instance::OnExit()
{
    if( p_intf->pf_show_dialog )
    {
         /* We need to manually clean up the dialogs class */
         if( p_intf->p_sys->p_wxwindow ) delete p_intf->p_sys->p_wxwindow;
    }

#if (wxCHECK_VERSION(2,5,0))
    wxClassInfo_sm_classTable_BUGGY = wxClassInfo::sm_classTable;
    wxClassInfo::sm_classTable = 0;
#endif

    return 0;
}

static void ShowDialog( intf_thread_t *p_intf, int i_dialog_event, int i_arg,
                        intf_dialog_args_t *p_arg )
{
    wxCommandEvent event( wxEVT_DIALOG, i_dialog_event );
    event.SetInt( i_arg );
    event.SetClientData( p_arg );

#ifdef WIN32
    SendMessage( (HWND)p_intf->p_sys->p_wxwindow->GetHandle(),
                 WM_CANCELMODE, 0, 0 );
#endif
    if( i_dialog_event == INTF_DIALOG_POPUPMENU && i_arg == 0 ) return;

    /* Hack to prevent popup events to be enqueued when
     * one is already active */
    if( i_dialog_event != INTF_DIALOG_POPUPMENU ||
        !p_intf->p_sys->p_popup_menu )
    {
        p_intf->p_sys->p_wxwindow->AddPendingEvent( event );
    }
}

/*****************************************************************************
 * WindowSettings utility class
 *****************************************************************************/
WindowSettings::WindowSettings( intf_thread_t *_p_intf )
{
    char *psz_org = NULL;
    char *psz;
    int i;

    /* */
    p_intf = _p_intf;

    /* */
    for( i = 0; i < ID_MAX; i++ )
    {
        b_valid[i] = false;
        b_shown[i] = false;
        position[i] = wxDefaultPosition;
        size[i] = wxDefaultSize;
    }
    b_shown[ID_MAIN] = true;

    if( p_intf->pf_show_dialog ) return;

    /* Parse the configuration */
    psz_org = psz = config_GetPsz( p_intf, "wx-config-last" );
    if( !psz || *psz == '\0' ) return;

    msg_Dbg( p_intf, "Using last windows config '%s'", psz );

    i_screen_w = 0;
    i_screen_h = 0;
    while( psz && *psz )
    {
        int id, v[4];

        psz = strchr( psz, '(' );

        if( !psz )
            break;
        psz++;

        id = strtol( psz, &psz, 0 );
        if( *psz != ',' ) /* broken cfg */
            goto invalid;
        psz++;

        for( i = 0; i < 4; i++ )
        {
            v[i] = strtol( psz, &psz, 0 );

            if( i < 3 )
            {
                if( *psz != ',' )
                    goto invalid;
                psz++;
            }
            else
            {
                if( *psz != ')' )
                    goto invalid;
            }
        }
        if( id == ID_SCREEN )
        {
            i_screen_w = v[2];
            i_screen_h = v[3];
        }
        else if( id >= 0 && id < ID_MAX )
        {
            b_valid[id] = true;
            b_shown[id] = true;
            position[id] = wxPoint( v[0], v[1] );
            size[id] = wxSize( v[2], v[3] );

            msg_Dbg( p_intf, "id=%d p=(%d,%d) s=(%d,%d)",
                     id, position[id].x, position[id].y,
                         size[id].x, size[id].y );
        }

        psz = strchr( psz, ')' );
        if( psz ) psz++;
    }

    if( i_screen_w <= 0 || i_screen_h <= 0 )
        goto invalid;

    for( i = 0; i < ID_MAX; i++ )
    {
        if( !b_valid[i] )
            continue;
        if( position[i].x < 0 || position[i].y < 0 )
            goto invalid;
        if( size[i].x <= 0 || size[i].y <= 0 )
            goto invalid;
    }

    if( psz_org ) free( psz_org );
    return;

invalid:
    msg_Dbg( p_intf, "last windows config is invalid (ignored)" );
    for( i = 0; i < ID_MAX; i++ )
    {
        b_valid[i] = false;
        b_shown[i] = false;
        position[i] = wxDefaultPosition;
        size[i] = wxDefaultSize;
    }
    if( psz_org ) free( psz_org );
}


WindowSettings::~WindowSettings( )
{
    wxString sCfg;

    if( p_intf->pf_show_dialog ) return;

    sCfg = wxString::Format( wxT("(%d,0,0,%d,%d)"), ID_SCREEN,
                             wxSystemSettings::GetMetric( wxSYS_SCREEN_X ),
                             wxSystemSettings::GetMetric( wxSYS_SCREEN_Y ) );
    for( int i = 0; i < ID_MAX; i++ )
    {
        if( !b_valid[i] || !b_shown[i] )
            continue;

        sCfg += wxString::Format( wxT("(%d,%d,%d,%d,%d)"),
                                  i, position[i].x, position[i].y,
                                     size[i].x, size[i].y );
    }

    config_PutPsz( p_intf, "wx-config-last", sCfg.mb_str() );
}

void WindowSettings::SetScreen( int i_screen_w, int i_screen_h )
{
    int i;

    for( i = 0; i < ID_MAX; i++ )
    {
        if( !b_valid[i] )
            continue;
        if( position[i].x >= i_screen_w || position[i].y >= i_screen_h )
            goto invalid;
    }
    return;

invalid:
    for( i = 0; i < ID_MAX; i++ )
    {
        b_valid[i] = false;
        b_shown[i] = false;
        position[i] = wxDefaultPosition;
        size[i] = wxDefaultSize;
    }
}

void WindowSettings::SetSettings( int id, bool _b_shown, wxPoint p, wxSize s )
{
    if( id < 0 || id >= ID_MAX )
        return;

    b_valid[id] = true;
    b_shown[id] = _b_shown;

    position[id] = p;
    size[id] = s;
}

bool WindowSettings::GetSettings( int id, bool& _b_shown, wxPoint& p, wxSize& s)
{
    if( id < 0 || id >= ID_MAX )
        return false;

    if( !b_valid[id] )
        return false;

    _b_shown = b_shown[id];
    p = position[id];
    s = size[id];

    return true;
}
