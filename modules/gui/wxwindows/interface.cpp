/*****************************************************************************
 * interface.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: interface.cpp,v 1.1 2002/11/18 13:02:16 gbazin Exp $
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

/* Let wxWindows take care of the i18n stuff */
#undef _

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield()
#undef CreateDialog()
#endif

#include <wx/wxprec.h>
#include <wx/wx.h>

#include "wxwindows.h"
#include "wx/artprov.h"

/* include the toolbar graphics */
#include "bitmaps/file.xpm"
#include "bitmaps/disc.xpm"
#include "bitmaps/net.xpm"
#include "bitmaps/play.xpm"
#include "bitmaps/pause.xpm"
#include "bitmaps/stop.xpm"
#include "bitmaps/previous.xpm"
#include "bitmaps/next.xpm"
#include "bitmaps/playlist.xpm"
/*****************************************************************************
 * Event Table.
 *****************************************************************************/

const int ID_TOOLBAR = 500;

/* IDs for the controls and the menu commands */
enum
{
    /* menu items */
    Exit_Event = 1,
    OpenFile_Event,
    OpenDisc_Event,
    OpenNet_Event,
    OpenSat_Event,
    EjectDisc_Event,

    Logs_Event,

    Audio_Event,
    Subtitles_Event,
    Prefs_Event,

    SliderScroll_Event,
    StopStream_Event,
    PlayStream_Event,
    PauseStream_Event,
    PrevStream_Event,
    NextStream_Event

    /* it is important for the id corresponding to the "About" command to have
     * this standard value as otherwise it won't be handled properly under Mac
     * (where it is special and put into the "Apple" menu) */
    About_Event = wxID_ABOUT,
};

BEGIN_EVENT_TABLE(Interface, wxFrame)
    /* Menu events */
    EVT_MENU(Exit_Event, Interface::OnExit)
    EVT_MENU(About_Event, Interface::OnAbout)
    EVT_MENU(OpenFile_Event, Interface::OnOpenFile)
    /* Toolbar events */
    EVT_MENU(OpenFile_Event, Interface::OnOpenFile)
    EVT_MENU(StopStream_Event, Interface::OnStopStream)
    EVT_MENU(PlayStream_Event, Interface::OnPlayStream)
    EVT_MENU(PauseStream_Event, Interface::OnPauseStream)
    EVT_MENU(PrevStream_Event, Interface::OnPrevStream)
    EVT_MENU(NextStream_Event, Interface::OnNextStream)
    /* Slider events */
    EVT_COMMAND_SCROLL(SliderScroll_Event, Interface::OnSliderUpdate)
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Interface::Interface( intf_thread_t *_p_intf ):
    wxFrame( NULL, -1, "title", wxDefaultPosition, wxDefaultSize,
             wxDEFAULT_FRAME_STYLE )
{

    /* Initializations */
    p_intf = _p_intf;

    /* Give our interface a nice icon */
    //SetIcon( wxICON(vlcicon) );

    /* Create our "File" menu */
#define HELP_FILE  N_("Open a file")
#define HELP_DISC  N_("Open a DVD or (S)VCD")
#define HELP_NET   N_("Open a network stream")
#define HELP_SAT   N_("Open a satellite stream")
#define HELP_EJECT N_("Eject the DVD/CD")
#define HELP_EXIT  N_("Exit this program")
    wxMenu *file_menu = new wxMenu;
    file_menu->Append( OpenFile_Event, _("&Open File..."), HELP_FILE );
    file_menu->Append( OpenDisc_Event, _("Open &Disc..."), HELP_DISC );
    file_menu->Append( OpenNet_Event, _("&Network Stream..."), HELP_NET );
#if 0
    file_menu->Append( OpenSat_Event, _("&Satellite Stream..."), HELP_NET );
#endif
    file_menu->AppendSeparator();
    file_menu->Append( EjectDisc_Event, _("&Eject Disc"), HELP_EJECT );
    file_menu->AppendSeparator();
    file_menu->Append( Exit_Event, _("E&xit"), HELP_EXIT );

    /* Create our "View" menu */
#define HELP_LOGS  N_("Show the program logs")
    wxMenu *view_menu = new wxMenu;
    view_menu->Append( Logs_Event, _("&Logs..."), HELP_LOGS );

    /* Create our "Settings" menu */
#define HELP_AUDIO N_("Change the current audio track")
#define HELP_SUBS  N_("Change the current subtitles stream")
#define HELP_PREFS N_("Go to the preferences menu")
    wxMenu *settings_menu = new wxMenu;
    settings_menu->Append( Audio_Event, _("&Audio"), HELP_AUDIO );
    settings_menu->Append( Subtitles_Event, _("&Subtitles"), HELP_SUBS );
    settings_menu->AppendSeparator();
    settings_menu->Append( Prefs_Event, _("&Preferences..."), HELP_PREFS );

    /* Create our "Help" menu */
#define HELP_ABOUT N_("About this program")
    wxMenu *help_menu = new wxMenu;
    help_menu->Append( About_Event, _("&About..."), HELP_ABOUT );

    /* Append the freshly created menus to the menu bar... */
    wxMenuBar *menubar = new wxMenuBar( wxMB_DOCKABLE );
    menubar->Append( file_menu, _("&File") );
    menubar->Append( view_menu, _("&View") );
    menubar->Append( settings_menu, _("&Settings") );
    menubar->Append( help_menu, _("&Help") );

    /* Attach the menu bar to the frame */
    SetMenuBar( menubar );

    /* Create toolbar */
#define HELP_STOP N_("Stop current playlist item")
#define HELP_PLAY N_("Play current playlist item")
#define HELP_PAUSE N_("Pause current playlist item")
#define HELP_PLO N_("Open playlist")
#define HELP_PLP N_("Previous playlist item")
#define HELP_PLN N_("Next playlist item")
    wxBitmap *p_bmp_file     = new wxBitmap( file_xpm );
    wxBitmap *p_bmp_disc     = new wxBitmap( disc_xpm );
    wxBitmap *p_bmp_net      = new wxBitmap( net_xpm );
    wxBitmap *p_bmp_play     = new wxBitmap( play_xpm );
    wxBitmap *p_bmp_stop     = new wxBitmap( stop_xpm );
    wxBitmap *p_bmp_pause    = new wxBitmap( pause_xpm );
    wxBitmap *p_bmp_prev     = new wxBitmap( previous_xpm );
    wxBitmap *p_bmp_next     = new wxBitmap( next_xpm );
    wxBitmap *p_bmp_playlist = new wxBitmap( playlist_xpm );

    wxToolBar *toolbar = CreateToolBar(
        wxTB_HORIZONTAL | wxTB_TEXT | wxTB_FLAT | wxTB_DOCKABLE, ID_TOOLBAR );

    toolbar->AddTool( OpenFile_Event, _("File"), *p_bmp_file, HELP_FILE );
    toolbar->AddTool( OpenFile_Event, _("Disc"), *p_bmp_disc, HELP_DISC );
    toolbar->AddTool( OpenFile_Event, _("Net"), *p_bmp_net, HELP_NET );
#if 0
    toolbar->AddTool( OpenFile_Event, _("Sat"), *p_bmp_net, HELP_SAT );
#endif
    toolbar->AddSeparator();
    toolbar->AddTool( StopStream_Event, _("Stop"), *p_bmp_stop, HELP_STOP );
    toolbar->AddTool( PlayStream_Event, _("Play"), *p_bmp_play, HELP_PLAY );
    toolbar->AddTool( PauseStream_Event, _("Pause"), *p_bmp_pause, HELP_PAUSE);
    toolbar->AddSeparator();
    toolbar->AddTool( wxID_OPEN, _("Playlist"), *p_bmp_playlist, HELP_PLO );
    toolbar->AddTool( PrevStream_Event, _("Prev"), *p_bmp_prev, HELP_PLP );
    toolbar->AddTool( NextStream_Event, _("Next"), *p_bmp_next, HELP_PLN );

    toolbar->Realize();

    /* Create slider */
    wxBoxSizer *slider_sizer = new wxBoxSizer( wxVERTICAL );
    slider = new wxSlider( this, SliderScroll_Event, 0, 0, 100,
                           wxDefaultPosition, wxSize( 450, 50 ),
                           wxSL_HORIZONTAL | wxSL_AUTOTICKS | wxSL_TOP );
    slider_sizer->Add( slider, 0, wxGROW | wxALL | wxALIGN_CENTER, 5 );

    /* use the sizer for layout */
    slider->Hide();
    slider_sizer->Layout();
    SetSizerAndFit( slider_sizer );

    /* Give the frame an optional statusbar. The '1' just means one field.
     * A gripsizer will automatically get put on into the corner, if that
     * is the normal OS behaviour for frames on that platform. Helptext
     * for menu items and toolbar tools will automatically get displayed
     * here. */
    statusbar = CreateStatusBar(2);
    int i_status_width[2] = {-1,-2};
    statusbar->SetStatusWidths( 2, i_status_width );

    SetTitle( COPYRIGHT_MESSAGE );
    SetAutoLayout( TRUE );
    Layout();
}

Interface::~Interface()
{
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void Interface::OnExit( wxCommandEvent& WXUNUSED(event) )
{
    /* TRUE is to force the frame to close. */
    Close(TRUE);
}

void Interface::OnAbout( wxCommandEvent& WXUNUSED(event) )
{
    wxString msg;
    msg.Printf( _("This is the about dialog of the VideoLAN Client.\n") );

    wxMessageBox( msg, "About VideoLAN Client",
                  wxOK | wxICON_INFORMATION, this );
}

void Interface::OnOpenFile( wxCommandEvent& WXUNUSED(event) )
{
    wxFileDialog dialog
                 (
                    this,
                    _T("Open file dialog"),
                    _T(""),
                    _T(""),
                    _T("*.*")
                 );

    if (dialog.ShowModal() == wxID_OK)
    {
        wxString info;
        info.Printf(_T("Full file name: %s\n")
                    _T("Path: %s\n")
                    _T("Name: %s"),
                    dialog.GetPath().c_str(),
                    dialog.GetDirectory().c_str(),
                    dialog.GetFilename().c_str());
        wxMessageDialog dialog2(this, info, _T("Selected file"));
        dialog2.ShowModal();
    }
}

void Interface::OnPlayStream( wxCommandEvent& WXUNUSED(event) )
{
    wxCommandEvent dummy;
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        OnOpenFile( dummy );
        return;
    }

    /* If the playlist is empty, open a file requester instead */
    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Play( p_playlist );
        vlc_object_release( p_playlist );
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        OnOpenFile( dummy );
    }
}

void Interface::OnStopStream( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t * p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );
}

void Interface::OnPauseStream( wxCommandEvent& WXUNUSED(event) )
{
    if( p_intf->p_sys->p_input == NULL )
    {
        return;
    }

    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PAUSE );
}

void Interface::OnSliderUpdate( wxScrollEvent& event )
{
    p_intf->p_sys->i_slider_pos = event.GetPosition();
}

void Interface::OnPrevStream( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t * p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );
}

void Interface::OnNextStream( wxCommandEvent& WXUNUSED(event) )
{
    playlist_t * p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );
}
