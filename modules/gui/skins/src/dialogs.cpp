/*****************************************************************************
 * dialogs.cpp: Handles all the different dialog boxes we provide.
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dialogs.cpp,v 1.1 2003/06/03 22:20:00 gbazin Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

//--- VLC -------------------------------------------------------------------
#include <vlc/vlc.h>
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "event.h"
#include "banks.h"
#include "theme.h"
#include "../os_theme.h"
#include "themeloader.h"
#include "window.h"
#include "vlcproc.h"
#include "skin_common.h"
#include "dialogs.h"

#ifdef BASIC_SKINS

// Constructor
Dialogs::Dialogs( intf_thread_t *_p_intf ){}
// Destructor
Dialogs::~Dialogs(){}

void Dialogs::ShowOpen( bool b_play ){}
void Dialogs::ShowMessages(){}
void Dialogs::ShowPrefs(){}
void Dialogs::ShowFileInfo(){}

#else // BASIC_SKINS

#include "../../wxwindows/wxwindows.h"
#include "share/vlc32x32.xpm"       // include the graphic icon

#define ShowOpen_Event     0
#define ShowMessages_Event 1
#define ShowPrefs_Event    2
#define ShowFileInfo_Event 3
#define ExitThread_Event   99

//---------------------------------------------------------------------------
// Local classes declarations.
//---------------------------------------------------------------------------

DEFINE_EVENT_TYPE(wxEVT_DIALOG)

class Instance: public wxApp
{
public:
    Instance();
#ifdef GTK2_SKINS
    Instance( intf_thread_t *_p_intf, CallBackObjects *callback );
#else
    Instance( intf_thread_t *_p_intf );
#endif

    bool OnInit();
    int  OnExit();

private:
    intf_thread_t *p_intf;

#ifdef GTK2_SKINS
    CallBackObjects *callbackobj;
#endif

    DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE(Instance, wxApp)
    EVT_COMMAND(ShowOpen_Event, wxEVT_DIALOG, Dialogs::OnShowOpen)
    EVT_COMMAND(ShowMessages_Event, wxEVT_DIALOG, Dialogs::OnShowMessages)
    EVT_COMMAND(ShowPrefs_Event, wxEVT_DIALOG, Dialogs::OnShowPrefs)
    EVT_COMMAND(ShowFileInfo_Event, wxEVT_DIALOG, Dialogs::OnShowFileInfo)
    EVT_COMMAND(ExitThread_Event, wxEVT_DIALOG, Dialogs::OnExitThread)
END_EVENT_TABLE()

//---------------------------------------------------------------------------
// Implementation of Instance class
//---------------------------------------------------------------------------
Instance::Instance( )
{
}

#ifdef GTK2_SKINS
Instance::Instance( intf_thread_t *_p_intf, CallBackObjects *callback )
{
    // Initialization
    p_intf = _p_intf;
    callbackobj = callback;
}
#else
Instance::Instance( intf_thread_t *_p_intf )
{
    // Initialization
    p_intf = _p_intf;
}
#endif

IMPLEMENT_APP_NO_MAIN(Instance)

bool Instance::OnInit()
{
    p_intf->p_sys->p_icon = new wxIcon( vlc_xpm );

#ifdef GTK2_SKINS
    // Set event callback. Yes, it's a big hack ;)
    gdk_event_handler_set( GTK2Proc, (gpointer)callbackobj, NULL );
#endif

    // Create all the dialog boxes
    p_intf->p_sys->p_dialogs->OpenDlg =
        new OpenDialog( p_intf, NULL, FILE_ACCESS );
    p_intf->p_sys->p_dialogs->MessagesDlg = new Messages( p_intf, NULL );
    p_intf->p_sys->p_dialogs->PrefsDlg = new PrefsDialog( p_intf, NULL );
    p_intf->p_sys->p_dialogs->FileInfoDlg = new FileInfo( p_intf, NULL );

#ifdef GTK2_SKINS
    // Add timer
    g_timeout_add( 200, (GSourceFunc)RefreshTimer, (gpointer)p_intf );
#endif

    // OK, initialization is over, now the other thread can go on working...
    vlc_thread_ready( p_intf );

    return TRUE;
}

int Instance::OnExit()
{
    // Delete evertything
    delete p_intf->p_sys->p_dialogs->FileInfoDlg;
    delete p_intf->p_sys->p_dialogs->PrefsDlg;
    delete p_intf->p_sys->p_dialogs->MessagesDlg;
    delete p_intf->p_sys->p_dialogs->OpenDlg;
    delete p_intf->p_sys->p_icon;

    return 0;
}

//---------------------------------------------------------------------------
#if !defined(__BUILTIN__) && defined( WIN32 )
HINSTANCE hInstance = 0;
extern "C" BOOL WINAPI
DllMain (HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)
{
    hInstance = (HINSTANCE)hModule;
    return TRUE;
}
#endif

//---------------------------------------------------------------------------
// Thread callback
// We create all wxWindows dialogs in a separate thread because we don't want
// any interaction with our own message loop
//---------------------------------------------------------------------------
void SkinsDialogsThread( intf_thread_t *p_intf )
{
#if !defined( WIN32 )
    static char  *p_args[] = { "" };
#endif

    /* Hack to pass the p_intf pointer to the new wxWindow Instance object */
    wxTheApp = new Instance( p_intf );

#if defined( WIN32 )
#if !defined(__BUILTIN__)
    wxEntry( hInstance/*GetModuleHandle(NULL)*/, NULL, NULL, SW_SHOW, TRUE );
#else
    wxEntry( GetModuleHandle( NULL ), NULL, NULL, SW_SHOW, TRUE );
#endif
#else
    wxEntry( 1, p_args );
#endif

    return;
}

//---------------------------------------------------------------------------
// Implementation of Dialogs class
//---------------------------------------------------------------------------
Dialogs::Dialogs( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
    p_intf->p_sys->p_dialogs = this;

    // Create a new thread for wxWindows
    if( vlc_thread_create( p_intf, "Skins Dialogs Thread", SkinsDialogsThread,
                           0, VLC_TRUE ) )
    {
        OpenDlg = NULL;
        msg_Err( p_intf, "cannot create SkinsDialogsThread" );
    }
}

Dialogs::~Dialogs()
{
    wxCommandEvent event( wxEVT_DIALOG, ExitThread_Event );
    event.SetClientData( this );

    wxTheApp->AddPendingEvent( event );

    vlc_thread_join( p_intf ); //FIXME, use own vlc_object
}

void Dialogs::ShowOpen( bool b_play )
{
    wxCommandEvent event( wxEVT_DIALOG, ShowOpen_Event );
    event.SetClientData( this );
    event.SetInt( b_play );

    wxTheApp->AddPendingEvent( event );
}

void Dialogs::ShowMessages()
{
    wxCommandEvent event( wxEVT_DIALOG, ShowMessages_Event );
    event.SetClientData( this );

    wxTheApp->AddPendingEvent( event );
}

void Dialogs::ShowPrefs()
{
    wxCommandEvent event( wxEVT_DIALOG, ShowPrefs_Event );
    event.SetClientData( this );

    wxTheApp->AddPendingEvent( event );
}

void Dialogs::ShowFileInfo()
{
    wxCommandEvent event( wxEVT_DIALOG, ShowFileInfo_Event );
    event.SetClientData( this );

    wxTheApp->AddPendingEvent( event );
}

void Dialogs::OnShowOpen( wxCommandEvent& event )
{
    Dialogs *p_dialogs = (Dialogs *)event.GetClientData();
    bool b_play = event.GetInt() ? TRUE : FALSE;

    //p_dialogs->OpenDlg->Show( !p_dialogs->OpenDlg->IsShown() );
 
    if( p_dialogs->OpenDlg->ShowModal() != wxID_OK )
    {
        return;
    }

    // Check if playlist is available
    playlist_t *p_playlist = p_dialogs->p_intf->p_sys->p_playlist;
    if( p_playlist == NULL )
    {
        return;
    }

    if( b_play )
    {
        // Append and play
        for( size_t i = 0; i < p_dialogs->OpenDlg->mrl.GetCount(); i++ )
        {
            playlist_Add( p_playlist,
                (const char *)p_dialogs->OpenDlg->mrl[i].mb_str(),
                PLAYLIST_APPEND | (i ? 0 : PLAYLIST_GO), PLAYLIST_END );
        }
        p_dialogs->p_intf->p_sys->p_theme->EvtBank->Get( "play" )->SendEvent();
    }
    else
    {
        // Append only
        for( size_t i = 0; i < p_dialogs->OpenDlg->mrl.GetCount(); i++ )
        {
            playlist_Add( p_playlist,
                (const char *)p_dialogs->OpenDlg->mrl[i].mb_str(),
                PLAYLIST_APPEND, PLAYLIST_END );
        }
    }

    // Refresh interface !
    p_dialogs->p_intf->p_sys->p_theme->EvtBank->Get( "playlist_refresh" )
        ->PostSynchroMessage();

    return;
}

void Dialogs::OnShowMessages( wxCommandEvent& event )
{
    Dialogs *p_dialogs = (Dialogs *)event.GetClientData();
    p_dialogs->MessagesDlg->Show( !p_dialogs->MessagesDlg->IsShown() );
}

void Dialogs::OnShowPrefs( wxCommandEvent& event )
{
    Dialogs *p_dialogs = (Dialogs *)event.GetClientData();
    p_dialogs->PrefsDlg->Show( !p_dialogs->PrefsDlg->IsShown() );
}

void Dialogs::OnShowFileInfo( wxCommandEvent& event )
{
    Dialogs *p_dialogs = (Dialogs *)event.GetClientData();
    p_dialogs->FileInfoDlg->Show( !p_dialogs->FileInfoDlg->IsShown() );
}

void Dialogs::OnExitThread( wxCommandEvent& event )
{
    wxTheApp->ExitMainLoop();
}
#endif // BASIC_SKINS
