/*****************************************************************************
 * interface.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: interface.cpp,v 1.33 2003/05/24 17:52:49 gbazin Exp $
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
#include "stream_control.h"

#include "wxwindows.h"

/* include the toolbar graphics */
#include "bitmaps/file.xpm"
#include "bitmaps/disc.xpm"
#include "bitmaps/net.xpm"
#if 0
#include "bitmaps/sat.xpm"
#endif
#include "bitmaps/play.xpm"
#include "bitmaps/pause.xpm"
#include "bitmaps/stop.xpm"
#include "bitmaps/previous.xpm"
#include "bitmaps/next.xpm"
#include "bitmaps/playlist.xpm"
#include "bitmaps/fast.xpm"
#include "bitmaps/slow.xpm"

#define TOOLBAR_BMP_WIDTH 36
#define TOOLBAR_BMP_HEIGHT 36

/* include the icon graphic */
#include "share/vlc32x32.xpm"

/*****************************************************************************
 * Local class declarations.
 *****************************************************************************/
class wxMenuExt: public wxMenu
{
public:
    /* Constructor */
    wxMenuExt( wxMenu* parentMenu, int id, const wxString& text,
                   const wxString& helpString, wxItemKind kind,
                   char *_psz_var, int _i_object_id, vlc_value_t _val,
                   int _i_val_type );

    virtual ~wxMenuExt() {};

    char *psz_var;
    int  i_val_type;
    int  i_object_id;
    vlc_value_t val;

private:

};

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    /* menu items */
    Exit_Event = wxID_HIGHEST,
    OpenFile_Event,
    OpenDisc_Event,
    OpenNet_Event,
    OpenSat_Event,
    EjectDisc_Event,

    Playlist_Event,
    Logs_Event,
    FileInfo_Event,

    Prefs_Event,

    SliderScroll_Event,
    StopStream_Event,
    PlayStream_Event,
    PrevStream_Event,
    NextStream_Event,
    SlowStream_Event,
    FastStream_Event,

    /* it is important for the id corresponding to the "About" command to have
     * this standard value as otherwise it won't be handled properly under Mac
     * (where it is special and put into the "Apple" menu) */
    About_Event = wxID_ABOUT
};

BEGIN_EVENT_TABLE(Interface, wxFrame)
    /* Menu events */
    EVT_MENU(Exit_Event, Interface::OnExit)
    EVT_MENU(About_Event, Interface::OnAbout)
    EVT_MENU(Playlist_Event, Interface::OnPlaylist)
    EVT_MENU(Logs_Event, Interface::OnLogs)
    EVT_MENU(FileInfo_Event, Interface::OnFileInfo)
    EVT_MENU(Prefs_Event, Interface::OnPreferences)

    EVT_MENU_OPEN(Interface::OnMenuOpen)

#if defined( __WXMSW__ ) || defined( __WXMAC__ )
    EVT_CONTEXT_MENU(Interface::OnContextMenu)
#else
    EVT_RIGHT_UP(Interface::OnContextMenu)
#endif

    /* Toolbar events */
    EVT_MENU(OpenFile_Event, Interface::OnOpenFile)
    EVT_MENU(OpenDisc_Event, Interface::OnOpenDisc)
    EVT_MENU(OpenNet_Event, Interface::OnOpenNet)
    EVT_MENU(OpenSat_Event, Interface::OnOpenSat)
    EVT_MENU(StopStream_Event, Interface::OnStopStream)
    EVT_MENU(PlayStream_Event, Interface::OnPlayStream)
    EVT_MENU(PrevStream_Event, Interface::OnPrevStream)
    EVT_MENU(NextStream_Event, Interface::OnNextStream)
    EVT_MENU(SlowStream_Event, Interface::OnSlowStream)
    EVT_MENU(FastStream_Event, Interface::OnFastStream)

    /* Slider events */
    EVT_COMMAND_SCROLL(SliderScroll_Event, Interface::OnSliderUpdate)
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Interface::Interface( intf_thread_t *_p_intf ):
    wxFrame( NULL, -1, wxT(VOUT_TITLE),
             wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    p_prefs_dialog = NULL;
    i_old_playing_status = PAUSE_S;
    p_open_dialog = NULL;

    /* Give our interface a nice little icon */
    p_intf->p_sys->p_icon = new wxIcon( vlc_xpm );
    SetIcon( *p_intf->p_sys->p_icon );

    /* Create a sizer for the main frame */
    frame_sizer = new wxBoxSizer( wxHORIZONTAL );
    SetSizer( frame_sizer );

    /* Creation of the menu bar */
    CreateOurMenuBar();

    /* Creation of the tool bar */
    CreateOurToolBar();

    /* Creation of the slider sub-window */
    CreateOurSlider();
    frame_sizer->Add( slider_frame, 1, wxGROW, 0 );
    frame_sizer->Hide( slider_frame );

    /* Creation of the status bar
     * Helptext for menu items and toolbar tools will automatically get
     * displayed here. */
    int i_status_width[3] = {-6, -2, -9};
    statusbar = CreateStatusBar( 3 );                            /* 2 fields */
    statusbar->SetStatusWidths( 3, i_status_width );
    statusbar->SetStatusText( wxString::Format(wxT("x%.2f"), 1.0), 1 );


    /* Make sure we've got the right background colour */
    SetBackgroundColour( slider_frame->GetBackgroundColour() );

    /* Layout everything */
    SetAutoLayout( TRUE );
    frame_sizer->Layout();
    frame_sizer->Fit(this);

#if !defined(__WXX11__)
    /* Associate drop targets with the main interface */
    SetDropTarget( new DragAndDrop( p_intf ) );
#endif
}

Interface::~Interface()
{
    /* Clean up */
    if( p_open_dialog ) delete p_open_dialog;
    if( p_prefs_dialog ) p_prefs_dialog->Destroy();
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void Interface::CreateOurMenuBar()
{
#define HELP_FILE  N_("Open a file")
#define HELP_DISC  N_("Open a DVD or (S)VCD")
#define HELP_NET   N_("Open a network stream")
#define HELP_SAT   N_("Open a satellite stream")
#define HELP_EJECT N_("Eject the DVD/CD")
#define HELP_EXIT  N_("Exit this program")

#define HELP_PLAYLIST   N_("Open the playlist")
#define HELP_LOGS       N_("Show the program logs")
#define HELP_FILEINFO       N_("Show information about the file being played")

#define HELP_PREFS N_("Go to the preferences menu")

#define HELP_ABOUT N_("About this program")

    /* Create the "File" menu */
    wxMenu *file_menu = new wxMenu;
    file_menu->Append( OpenFile_Event, wxU(_("&Open File...")),
                       wxU(_(HELP_FILE)) );
    file_menu->Append( OpenDisc_Event, wxU(_("Open &Disc...")),
                       wxU(_(HELP_DISC)) );
    file_menu->Append( OpenNet_Event, wxU(_("&Network Stream...")),
                       wxU(_(HELP_NET)) );
#if 0
    file_menu->Append( OpenSat_Event, wxU(_("&Satellite Stream...")),
                       wxU(_(HELP_NET)) );
#endif
#if 0
    file_menu->AppendSeparator();
    file_menu->Append( EjectDisc_Event, wxU(_("&Eject Disc")),
                       wxU(_(HELP_EJECT)) );
#endif
    file_menu->AppendSeparator();
    file_menu->Append( Exit_Event, wxU(_("E&xit")), wxU(_(HELP_EXIT)) );

    /* Create the "View" menu */
    wxMenu *view_menu = new wxMenu;
    view_menu->Append( Playlist_Event, wxU(_("&Playlist...")),
                       wxU(_(HELP_PLAYLIST)) );
    view_menu->Append( Logs_Event, wxU(_("&Logs...")), wxU(_(HELP_LOGS)) );
    view_menu->Append( FileInfo_Event, wxU(_("&File info...")),
                       wxU(_(HELP_FILEINFO)) );

    /* Create the "Settings" menu */
    wxMenu *settings_menu = new wxMenu;
    settings_menu->Append( Prefs_Event, wxU(_("&Preferences...")),
                           wxU(_(HELP_PREFS)) );

    /* Create the "Audio" menu */
    p_audio_menu = new wxMenu;
    b_audio_menu = 1;

    /* Create the "Video" menu */
    p_video_menu = new wxMenu;
    b_video_menu = 1;

    /* Create the "Navigation" menu */
    p_navig_menu = new wxMenu;
    b_navig_menu = 1;

    /* Create the "Help" menu */
    wxMenu *help_menu = new wxMenu;
    help_menu->Append( About_Event, wxU(_("&About...")), wxU(_(HELP_ABOUT)) );

    /* Append the freshly created menus to the menu bar... */
    wxMenuBar *menubar = new wxMenuBar( wxMB_DOCKABLE );
    menubar->Append( file_menu, wxU(_("&File")) );
    menubar->Append( view_menu, wxU(_("&View")) );
    menubar->Append( settings_menu, wxU(_("&Settings")) );
    menubar->Append( p_audio_menu, wxU(_("&Audio")) );
    menubar->Append( p_video_menu, wxU(_("&Video")) );
    menubar->Append( p_navig_menu, wxU(_("&Navigation")) );
    menubar->Append( help_menu, wxU(_("&Help")) );

    /* Attach the menu bar to the frame */
    SetMenuBar( menubar );

    /* Intercept all menu events in our custom event handler */
    PushEventHandler( new MenuEvtHandler( p_intf, this ) );

#if !defined(__WXX11__)
    /* Associate drop targets with the menubar */
    menubar->SetDropTarget( new DragAndDrop( p_intf ) );
#endif
}

void Interface::CreateOurToolBar()
{
#define HELP_STOP N_("Stop current playlist item")
#define HELP_PLAY N_("Play current playlist item")
#define HELP_PAUSE N_("Pause current playlist item")
#define HELP_PLO N_("Open playlist")
#define HELP_PLP N_("Previous playlist item")
#define HELP_PLN N_("Next playlist item")
#define HELP_SLOW N_("Play slower")
#define HELP_FAST N_("Play faster")

    wxLogNull LogDummy; /* Hack to suppress annoying log message on the win32
                         * version because we don't include wx.rc */

    wxToolBar *toolbar = CreateToolBar(
        wxTB_HORIZONTAL | wxTB_FLAT | wxTB_DOCKABLE );

    toolbar->SetToolBitmapSize( wxSize(TOOLBAR_BMP_WIDTH,TOOLBAR_BMP_HEIGHT) );

    toolbar->AddTool( OpenFile_Event, wxU(_("File")), wxBitmap( file_xpm ),
                      wxU(_(HELP_FILE)) );
    toolbar->AddTool( OpenDisc_Event, wxU(_("Disc")), wxBitmap( disc_xpm ),
                      wxU(_(HELP_DISC)) );
    toolbar->AddTool( OpenNet_Event, wxU(_("Net")), wxBitmap( net_xpm ),
                      wxU(_(HELP_NET)) );
#if 0
    toolbar->AddTool( OpenSat_Event, wxU(_("Sat")), wxBitmap( sat_xpm ),
                      wxU(_(HELP_SAT)) );
#endif
    toolbar->AddSeparator();
    toolbar->AddTool( StopStream_Event, wxU(_("Stop")), wxBitmap( stop_xpm ),
                      wxU(_(HELP_STOP)) );
    toolbar->AddTool( PlayStream_Event, wxU(_("Play")), wxBitmap( play_xpm ),
                      wxU(_(HELP_PLAY)) );
    toolbar->AddSeparator();
    toolbar->AddTool( Playlist_Event, wxU(_("Playlist")),
                      wxBitmap( playlist_xpm ), wxU(_(HELP_PLO)) );
    toolbar->AddTool( PrevStream_Event, wxU(_("Prev")),
                      wxBitmap( previous_xpm ), wxU(_(HELP_PLP)) );
    toolbar->AddTool( NextStream_Event, wxU(_("Next")), wxBitmap( next_xpm ),
                      wxU(_(HELP_PLN)) );
    toolbar->AddTool( SlowStream_Event, wxU(_("Slower")), wxBitmap( slow_xpm ),
                      wxU(_(HELP_SLOW)) );
    toolbar->AddTool( FastStream_Event, wxU(_("Faster")), wxBitmap( fast_xpm ),
                      wxU(_(HELP_FAST)) );

    toolbar->Realize();

    /* Place the toolbar in a sizer, so we can calculate the width of the
     * toolbar and set this as the minimum for the main frame size. */
    wxBoxSizer *toolbar_sizer = new wxBoxSizer( wxHORIZONTAL );
    toolbar_sizer->Add( toolbar, 0, 0, 0 );
    toolbar_sizer->Layout();

#ifndef WIN32
    frame_sizer->SetMinSize( toolbar_sizer->GetMinSize().GetWidth(), -1 );
#else
    frame_sizer->SetMinSize( toolbar->GetToolSize().GetWidth() *
                             toolbar->GetToolsCount(), -1 );
#endif

#if !defined(__WXX11__)
    /* Associate drop targets with the toolbar */
    toolbar->SetDropTarget( new DragAndDrop( p_intf ) );
#endif
}

void Interface::CreateOurSlider()
{
    /* Create a new frame containing the slider */
    slider_frame = new wxPanel( this, -1, wxDefaultPosition, wxDefaultSize );
    slider_frame->SetAutoLayout( TRUE );

    /* Create static box to surround the slider */
    slider_box = new wxStaticBox( slider_frame, -1, wxT("") );

    /* Create sizer for slider frame */
    wxStaticBoxSizer *slider_sizer =
        new wxStaticBoxSizer( slider_box, wxHORIZONTAL );
    slider_frame->SetSizer( slider_sizer );
    slider_sizer->SetMinSize( -1, 50 );

    /* Create slider */
    slider = new wxSlider( slider_frame, SliderScroll_Event, 0, 0,
                           SLIDER_MAX_POS, wxDefaultPosition, wxDefaultSize );
    slider_sizer->Add( slider, 1, wxGROW | wxALL, 5 );
    slider_sizer->Layout();
    slider_sizer->SetSizeHints(slider_frame);

    /* Hide the slider by default */
    slider_frame->Hide();
}

void Interface::Open( int i_access_method )
{
    /* Show/hide the open dialog */
    if( p_open_dialog == NULL )
        p_open_dialog = new OpenDialog( p_intf, this, i_access_method );

    if( p_open_dialog &&
        p_open_dialog->ShowModal( i_access_method ) == wxID_OK )
    {
        /* Update the playlist */
        playlist_t *p_playlist =
            (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
        if( p_playlist == NULL )
        {
            return;
        }

        for( size_t i = 0; i < p_open_dialog->mrl.GetCount(); i++ )
        {
            playlist_Add( p_playlist,
                (const char *)p_open_dialog->mrl[i].mb_str(),
                PLAYLIST_APPEND | (i ? 0 : PLAYLIST_GO), PLAYLIST_END );
        }

        TogglePlayButton( PLAYING_S );

        vlc_object_release( p_playlist );
    }
}

/*****************************************************************************
 * Event Handlers.
 *****************************************************************************/
/* Work-around helper for buggy wxGTK */
void RecursiveDestroy( wxMenu *menu )
{
    wxMenuItemList::Node *node = menu->GetMenuItems().GetFirst();
    for( ; node; )
    {
        wxMenuItem *item = node->GetData();
        node = node->GetNext();

        /* Delete the submenus */
        wxMenu *submenu = item->GetSubMenu();
        if( submenu )
        {
            RecursiveDestroy( submenu );
        }
        menu->Delete( item );
    }
}

void Interface::OnMenuOpen(wxMenuEvent& event)
{
#if !defined( __WXMSW__ )
    if( event.GetEventObject() == p_audio_menu )
    {
        if( b_audio_menu )
        {
            p_audio_menu = AudioMenu( p_intf, this );

            /* Work-around for buggy wxGTK */
            wxMenu *menu = GetMenuBar()->GetMenu( 3 );
            RecursiveDestroy( menu );
            /* End work-around */

            menu =
                GetMenuBar()->Replace( 3, p_audio_menu, wxU(_("&Audio")) );
            if( menu ) delete menu;

            b_audio_menu = 0;
        }
        else b_audio_menu = 1;
    }
    else if( event.GetEventObject() == p_video_menu )
    {
        if( b_video_menu )
        {
            p_video_menu = VideoMenu( p_intf, this );

            /* Work-around for buggy wxGTK */
            wxMenu *menu = GetMenuBar()->GetMenu( 4 );
            RecursiveDestroy( menu );
            /* End work-around */

            menu =
                GetMenuBar()->Replace( 4, p_video_menu, wxU(_("&Video")) );
            if( menu ) delete menu;

            b_video_menu = 0;
        }
        else b_video_menu = 1;
    }
    else if( event.GetEventObject() == p_navig_menu )
    {
        if( b_navig_menu )
        {
            p_navig_menu = NavigMenu( p_intf, this );

            /* Work-around for buggy wxGTK */
            wxMenu *menu = GetMenuBar()->GetMenu( 5 );
            RecursiveDestroy( menu );
            /* End work-around */

            menu =
                GetMenuBar()->Replace( 5, p_navig_menu, wxU(_("&Navigation")));
            if( menu ) delete menu;

            b_navig_menu = 0;
        }
        else b_navig_menu = 1;
    }

#else
    p_audio_menu = AudioMenu( p_intf, this );
    wxMenu *menu = GetMenuBar()->Replace( 3, p_audio_menu, wxU(_("&Audio")) );
    if( menu ) delete menu;

    p_video_menu = VideoMenu( p_intf, this );
    menu = GetMenuBar()->Replace( 4, p_video_menu, wxU(_("&Video")) );
    if( menu ) delete menu;

    p_navig_menu = NavigMenu( p_intf, this );
    menu = GetMenuBar()->Replace( 5, p_navig_menu, wxU(_("&Navigation")) );
    if( menu ) delete menu;

#endif
}

#if defined( __WXMSW__ ) || defined( __WXMAC__ )
void Interface::OnContextMenu(wxContextMenuEvent& event)
{
    ::PopupMenu( p_intf, this, ScreenToClient(event.GetPosition()) );
}
#else
void Interface::OnContextMenu(wxMouseEvent& event)
{
    ::PopupMenu( p_intf, this, event.GetPosition() );
}
#endif

void Interface::OnExit( wxCommandEvent& WXUNUSED(event) )
{
    /* TRUE is to force the frame to close. */
    Close(TRUE);
}

void Interface::OnAbout( wxCommandEvent& WXUNUSED(event) )
{
    wxString msg;
    msg.Printf( wxString(wxT(VOUT_TITLE)) +
        wxU(_(" (wxWindows interface)\n\n")) +
        wxU(_("(C) 1996-2003 - the VideoLAN Team\n\n")) +
        wxU(_("The VideoLAN team <videolan@videolan.org>\n"
              "http://www.videolan.org/\n\n")) +
        wxU(_("This is the VideoLAN Client, a DVD, MPEG and DivX player."
              "\nIt can play MPEG and MPEG2 files from a file or from a "
              "network source.")) );

    wxMessageBox( msg, wxString::Format(wxU(_("About %s")), wxT(VOUT_TITLE)),
                  wxOK | wxICON_INFORMATION, this );
}

void Interface::OnPlaylist( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the playlist window */
    Playlist *p_playlist_window = p_intf->p_sys->p_playlist_window;
    if( p_playlist_window )
    {
        p_playlist_window->ShowPlaylist( ! p_playlist_window->IsShown() );
    }
}

void Interface::OnLogs( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the log window */
    wxFrame *p_messages_window = p_intf->p_sys->p_messages_window;
    if( p_messages_window )
    {
        p_messages_window->Show( ! p_messages_window->IsShown() );
    }
}

void Interface::OnFileInfo( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the file info window */
    wxFrame *p_fileinfo_window = p_intf->p_sys->p_fileinfo_window;
    if( p_fileinfo_window )
    {
        p_fileinfo_window->Show( ! p_fileinfo_window->IsShown() );
    }
}

void Interface::OnPreferences( wxCommandEvent& WXUNUSED(event) )
{
    /* Show/hide the open dialog */
    if( p_prefs_dialog == NULL )
    {
        p_prefs_dialog = new PrefsDialog( p_intf, this );
    }

    if( p_prefs_dialog )
    {
        p_prefs_dialog->Show( true );
    }
}

void Interface::OnOpenFile( wxCommandEvent& WXUNUSED(event) )
{
    Open( FILE_ACCESS );
}

void Interface::OnOpenDisc( wxCommandEvent& WXUNUSED(event) )
{
    Open( DISC_ACCESS );
}

void Interface::OnOpenNet( wxCommandEvent& WXUNUSED(event) )
{
    Open( NET_ACCESS );
}

void Interface::OnOpenSat( wxCommandEvent& WXUNUSED(event) )
{
    Open( SAT_ACCESS );
}

void Interface::OnPlayStream( wxCommandEvent& WXUNUSED(event) )
{
    wxCommandEvent dummy;
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        /* If the playlist is empty, open a file requester instead */
        OnOpenFile( dummy );
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );

        input_thread_t *p_input = (input_thread_t *)vlc_object_find( p_intf,
                                                       VLC_OBJECT_INPUT,
                                                       FIND_ANYWHERE );
        if( p_input == NULL )
        {
            /* No stream was playing, start one */
            playlist_Play( p_playlist );
            TogglePlayButton( PLAYING_S );
            vlc_object_release( p_playlist );
            return;
        }

        if( p_input->stream.control.i_status != PAUSE_S )
        {
            /* A stream is being played, pause it */
            input_SetStatus( p_input, INPUT_STATUS_PAUSE );
            TogglePlayButton( PAUSE_S );
            vlc_object_release( p_playlist );
            vlc_object_release( p_input );
            return;
        }

        /* Stream is paused, resume it */
        playlist_Play( p_playlist );
        TogglePlayButton( PLAYING_S );
        vlc_object_release( p_input );
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
    TogglePlayButton( PAUSE_S );
    vlc_object_release( p_playlist );
}

void Interface::OnSliderUpdate( wxScrollEvent& event )
{
    vlc_mutex_lock( &p_intf->change_lock );

#ifdef WIN32
    if( event.GetEventType() == wxEVT_SCROLL_THUMBRELEASE
        || event.GetEventType() == wxEVT_SCROLL_ENDSCROLL )
    {
#endif
        if( p_intf->p_sys->i_slider_pos != event.GetPosition()
            && p_intf->p_sys->p_input )
        {
            p_intf->p_sys->i_slider_pos = event.GetPosition();
            input_Seek( p_intf->p_sys->p_input, p_intf->p_sys->i_slider_pos *
                        100 / SLIDER_MAX_POS,
                        INPUT_SEEK_PERCENT | INPUT_SEEK_SET );
        }

#ifdef WIN32
        p_intf->p_sys->b_slider_free = VLC_TRUE;
    }
    else
    {
        p_intf->p_sys->b_slider_free = VLC_FALSE;

        if( p_intf->p_sys->p_input )
        {
            /* Update stream date */
#define p_area p_intf->p_sys->p_input->stream.p_selected_area
            char psz_time[ OFFSETTOTIME_MAX_SIZE ];

            slider_box->SetLabel(
                wxU(input_OffsetToTime( p_intf->p_sys->p_input,
                                        psz_time,
                                        p_area->i_size * event.GetPosition()
                                        / SLIDER_MAX_POS )) );
#undef p_area
        }
    }
#endif

    vlc_mutex_unlock( &p_intf->change_lock );
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

void Interface::OnSlowStream( wxCommandEvent& WXUNUSED(event) )
{
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( p_input )
    {
        input_SetStatus( p_input, INPUT_STATUS_SLOWER );
        vlc_object_release( p_input );
    }
}

void Interface::OnFastStream( wxCommandEvent& WXUNUSED(event) )
{
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( p_input )
    {
        input_SetStatus( p_input, INPUT_STATUS_FASTER );
        vlc_object_release( p_input );
    }
}

void Interface::TogglePlayButton( int i_playing_status )
{
    if( i_playing_status == i_old_playing_status )
        return;

    GetToolBar()->DeleteTool( PlayStream_Event );

    if( i_playing_status == PLAYING_S )
    {
        GetToolBar()->InsertTool( 5, PlayStream_Event, wxU(_("Pause")),
                                  wxBitmap( pause_xpm ) );
    }
    else
    {
        GetToolBar()->InsertTool( 5, PlayStream_Event, wxU(_("Play")),
                                  wxBitmap( play_xpm ) );
    }

    GetToolBar()->Realize();

    i_old_playing_status = i_playing_status;
}

#if !defined(__WXX11__)
/*****************************************************************************
 * Definition of DragAndDrop class.
 *****************************************************************************/
DragAndDrop::DragAndDrop( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
}

bool DragAndDrop::OnDropFiles( wxCoord, wxCoord,
                               const wxArrayString& filenames )
{
    /* Add dropped files to the playlist */

    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return FALSE;
    }

    for( size_t i = 0; i < filenames.GetCount(); i++ )
        playlist_Add( p_playlist, (const char *)filenames[i].mb_str(),
                      PLAYLIST_APPEND | (i ? 0 : PLAYLIST_GO), PLAYLIST_END );

    vlc_object_release( p_playlist );

    return TRUE;
}
#endif
