/*****************************************************************************
 * dialogs.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: dialogs.cpp,v 1.4 2003/07/20 10:38:49 gbazin Exp $
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
#include <vlc/aout.h>

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO

#include <wx/wxprec.h>
#include <wx/wx.h>

#include <vlc/intf.h>
#include "stream_control.h"

#include "wxwindows.h"

/* include the icon graphic */
#include "../../../share/vlc32x32.xpm"

DEFINE_LOCAL_EVENT_TYPE( wxEVT_DIALOG );

BEGIN_EVENT_TABLE(DialogsProvider, wxFrame)
    /* Idle loop used to update some of the dialogs */
    EVT_IDLE(DialogsProvider::OnIdle)

    /* Custom wxDialog events */
    EVT_COMMAND(INTF_DIALOG_FILE, wxEVT_DIALOG, DialogsProvider::OnOpenFile)
    EVT_COMMAND(INTF_DIALOG_DISC, wxEVT_DIALOG, DialogsProvider::OnOpenDisc)
    EVT_COMMAND(INTF_DIALOG_NET, wxEVT_DIALOG, DialogsProvider::OnOpenNet)
    EVT_COMMAND(INTF_DIALOG_FILE_SIMPLE, wxEVT_DIALOG,
                DialogsProvider::OnOpenFileSimple)
    EVT_COMMAND(INTF_DIALOG_FILE_GENERIC, wxEVT_DIALOG,
                DialogsProvider::OnOpenFileGeneric)

    EVT_COMMAND(INTF_DIALOG_PLAYLIST, wxEVT_DIALOG,
                DialogsProvider::OnPlaylist)
    EVT_COMMAND(INTF_DIALOG_MESSAGES, wxEVT_DIALOG,
                DialogsProvider::OnMessages)
    EVT_COMMAND(INTF_DIALOG_PREFS, wxEVT_DIALOG,
                DialogsProvider::OnPreferences)
    EVT_COMMAND(INTF_DIALOG_FILEINFO, wxEVT_DIALOG,
                DialogsProvider::OnFileInfo)
    EVT_COMMAND(INTF_DIALOG_POPUPMENU, wxEVT_DIALOG,
                DialogsProvider::OnPopupMenu)
    EVT_COMMAND(INTF_DIALOG_EXIT, wxEVT_DIALOG,
                DialogsProvider::OnExitThread)
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
DialogsProvider::DialogsProvider( intf_thread_t *_p_intf, wxWindow *p_parent )
  :  wxFrame( p_parent, -1, wxT("") )
{
    /* Initializations */
    p_intf = _p_intf;
    p_open_dialog = NULL;
    p_file_dialog = NULL;
    p_playlist_dialog = NULL;
    p_messages_dialog = NULL;
    p_fileinfo_dialog = NULL;
    p_prefs_dialog = NULL;
    p_file_generic_dialog = NULL;

    /* Give our interface a nice little icon */
    p_intf->p_sys->p_icon = new wxIcon( vlc_xpm );

    /* Create the messages dialog so it can begin storing logs */
    p_messages_dialog = new Messages( p_intf, p_parent ? p_parent : this );

    /* Intercept all menu events in our custom event handler */
    PushEventHandler( new MenuEvtHandler( p_intf, NULL ) );
}

DialogsProvider::~DialogsProvider()
{
    /* Clean up */
    if( p_open_dialog )     delete p_open_dialog;
    if( p_prefs_dialog )    p_prefs_dialog->Destroy();
    if( p_file_dialog )     delete p_file_dialog;
    if( p_playlist_dialog ) delete p_playlist_dialog;
    if( p_messages_dialog ) delete p_messages_dialog;
    if( p_fileinfo_dialog ) delete p_fileinfo_dialog;
    if( p_file_generic_dialog ) delete p_file_generic_dialog;

    if( p_intf->p_sys->p_icon ) delete p_intf->p_sys->p_icon;
}

void DialogsProvider::OnIdle( wxIdleEvent& WXUNUSED(event) )
{
  /* Update the log window */
  if( p_messages_dialog )
    p_messages_dialog->UpdateLog();

  /* Update the playlist */
  if( p_playlist_dialog )
    p_playlist_dialog->UpdatePlaylist();

  /* Update the fileinfo windows */
  if( p_fileinfo_dialog )
    p_fileinfo_dialog->UpdateFileInfo();
}

void DialogsProvider::OnPlaylist( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the playlist window */
    if( !p_playlist_dialog )
        p_playlist_dialog = new Playlist( p_intf, this );

    if( p_playlist_dialog )
    {
        p_playlist_dialog->ShowPlaylist( !p_playlist_dialog->IsShown() );
    }
}

void DialogsProvider::OnMessages( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the log window */
    if( !p_messages_dialog )
        p_messages_dialog = new Messages( p_intf, this );

    if( p_messages_dialog )
    {
        p_messages_dialog->Show( !p_messages_dialog->IsShown() );
    }
}

void DialogsProvider::OnFileInfo( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the file info window */
    if( !p_fileinfo_dialog )
        p_fileinfo_dialog = new FileInfo( p_intf, this );

    if( p_fileinfo_dialog )
    {
        p_fileinfo_dialog->Show( !p_fileinfo_dialog->IsShown() );
    }
}

void DialogsProvider::OnPreferences( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the open dialog */
    if( !p_prefs_dialog )
        p_prefs_dialog = new PrefsDialog( p_intf, this );

    if( p_prefs_dialog )
    {
        p_prefs_dialog->Show( !p_prefs_dialog->IsShown() );
    }
}

void DialogsProvider::OnOpenFileGeneric( wxCommandEvent& event )
{
    intf_dialog_args_t *p_arg = (intf_dialog_args_t *)event.GetClientData();

    if( p_arg == NULL )
    {
        msg_Dbg( p_intf, "OnOpenFileGeneric() called with NULL arg" );
        return;
    }

    if( p_file_generic_dialog == NULL )
        p_file_generic_dialog = new wxFileDialog( this );

    if( p_file_generic_dialog )
    {
        p_file_generic_dialog->SetMessage( wxU(p_arg->psz_title) );
        p_file_generic_dialog->SetWildcard( wxU(p_arg->psz_extensions) );
        p_file_generic_dialog->SetStyle( (p_arg->b_save ? wxSAVE : wxOPEN) |
                                         (p_arg->b_multiple ? wxMULTIPLE:0) );
    }

    if( p_file_generic_dialog &&
        p_file_generic_dialog->ShowModal() == wxID_OK )
    {
        wxArrayString paths;

        p_file_generic_dialog->GetPaths( paths );

        p_arg->i_results = paths.GetCount();
        p_arg->psz_results = (char **)malloc( p_arg->i_results *
                                              sizeof(char *) );
        for( size_t i = 0; i < paths.GetCount(); i++ )
        {
            p_arg->psz_results[i] = strdup( paths[i].mb_str() );
        }
    }

    /* Callback */
    if( p_arg->pf_callback )
    {
        p_arg->pf_callback( p_arg );
    }

    /* Blocking or not ? */
    if( p_arg->b_blocking )
    {
        vlc_mutex_lock( &p_arg->lock );
        p_arg->b_ready = 1;
        vlc_cond_signal( &p_arg->wait );
        vlc_mutex_unlock( &p_arg->lock );
    }

    /* Clean-up */
    if( p_arg->b_blocking )
    {
        vlc_mutex_destroy( &p_arg->lock );
        vlc_cond_destroy( &p_arg->wait );
    }
    if( p_arg->psz_results )
    {
        for( int i = 0; i < p_arg->i_results; i++ )
        {
            free( p_arg->psz_results[i] );
        }
        free( p_arg->psz_results );
    }
    if( p_arg->psz_title ) free( p_arg->psz_title );
    if( p_arg->psz_extensions ) free( p_arg->psz_extensions );
    free( p_arg );
}

void DialogsProvider::OnOpenFileSimple( wxCommandEvent& event )
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    if( p_file_dialog == NULL )
        p_file_dialog = new wxFileDialog( this, wxU(_("Open file")),
            wxT(""), wxT(""), wxT("*"), wxOPEN | wxMULTIPLE );

    if( p_file_dialog && p_file_dialog->ShowModal() == wxID_OK )
    {
        wxArrayString paths;

        p_file_dialog->GetPaths( paths );

        for( size_t i = 0; i < paths.GetCount(); i++ )
            if( event.GetInt() )
                playlist_Add( p_playlist, (const char *)paths[i].mb_str(),
                              PLAYLIST_APPEND | (i ? 0 : PLAYLIST_GO),
                              PLAYLIST_END );
            else
                playlist_Add( p_playlist, (const char *)paths[i].mb_str(),
                              PLAYLIST_APPEND, PLAYLIST_END );
    }

    vlc_object_release( p_playlist );
}

void DialogsProvider::OnOpenFile( wxCommandEvent& event )
{
    Open( FILE_ACCESS, event.GetInt() );
}

void DialogsProvider::OnOpenDisc( wxCommandEvent& event )
{
    Open( DISC_ACCESS, event.GetInt() );
}

void DialogsProvider::OnOpenNet( wxCommandEvent& event )
{
    Open( NET_ACCESS, event.GetInt() );
}

void DialogsProvider::OnOpenSat( wxCommandEvent& event )
{
    Open( SAT_ACCESS, event.GetInt() );
}

void DialogsProvider::Open( int i_access_method, int i_arg )
{
    /* Show/hide the open dialog */
    if( !p_open_dialog )
        p_open_dialog = new OpenDialog( p_intf, this, i_access_method, i_arg );

    if( p_open_dialog )
    {
        p_open_dialog->Show( i_access_method, i_arg );
    }
}

void DialogsProvider::OnPopupMenu( wxCommandEvent& event )
{
    wxPoint mousepos = ScreenToClient( wxGetMousePosition() );
    ::PopupMenu( p_intf, this, mousepos );

}

void DialogsProvider::OnExitThread( wxCommandEvent& WXUNUSED(event) )
{
    delete this;
    wxTheApp->ExitMainLoop();
}
