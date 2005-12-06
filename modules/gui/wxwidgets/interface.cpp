/*****************************************************************************
 * interface.cpp : wxWidgets plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include "interface.hpp"
#include "playlist_manager.hpp"
#include "extrapanel.hpp"
#include "timer.hpp"
#include "video.hpp"
#include <vlc_keys.h>

#include "charset.h"

#include <vlc/aout.h>
#include "charset.h"

#include <wx/splitter.h>

/* include the toolbar graphics */
#include "bitmaps/play.xpm"
#include "bitmaps/pause.xpm"
#include "bitmaps/stop.xpm"
#include "bitmaps/prev.xpm"
#include "bitmaps/next.xpm"
#include "bitmaps/eject.xpm"
#include "bitmaps/slow.xpm"
#include "bitmaps/fast.xpm"
#include "bitmaps/playlist.xpm"
#include "bitmaps/playlist_small.xpm"
#include "bitmaps/speaker.xpm"
#include "bitmaps/speaker_mute.xpm"

#define TOOLBAR_BMP_WIDTH 16
#define TOOLBAR_BMP_HEIGHT 16

/* include the icon graphic */
#include "../../../share/vlc32x32.xpm"
/* include a small icon graphic for the systray icon */
#ifdef wxHAS_TASK_BAR_ICON
#include "../../../share/vlc16x16.xpm"
#endif

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

class wxVolCtrl;
class VLCVolCtrl : public wxControl
{
public:
    VLCVolCtrl( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~VLCVolCtrl() {};

    virtual void OnPaint( wxPaintEvent &event );
    void OnChange( wxMouseEvent& event );
    void UpdateVolume();

  private:
    DECLARE_EVENT_TABLE()

    wxVolCtrl *gauge;
    int i_y_offset;
    vlc_bool_t b_mute;
    intf_thread_t *p_intf;
};

BEGIN_EVENT_TABLE(VLCVolCtrl, wxControl)
    EVT_PAINT(VLCVolCtrl::OnPaint)

    /* Mouse events */
    EVT_LEFT_UP(VLCVolCtrl::OnChange)
END_EVENT_TABLE()

class Splitter : public wxSplitterWindow
{
public:
    Splitter( wxWindow *p_parent, intf_thread_t *_p_intf )
      : wxSplitterWindow( p_parent, -1, wxDefaultPosition, wxSize(0,0),
#if defined( __WXMSW__ )
                          wxCLIP_CHILDREN ),
#else
                          wxCLIP_CHILDREN | wxSP_3DSASH ),
#endif
        p_intf(_p_intf), i_sash_position(150), i_width(-1),
        b_video(0), i_delay(0)
    {
        SetSashSize( 0 );
    }

    virtual ~Splitter() {};

    virtual bool Split( wxWindow* window1, wxWindow* window2 )
    {
        SetSashSize( 0 );
        wxSize size = wxSize( i_width, i_sash_position );
        if( window2->GetSizer() ) window2->GetSizer()->SetMinSize( size );

        return wxSplitterWindow::SplitHorizontally( window1, window2,
                                                    -i_sash_position );
    }

    virtual bool Unsplit( wxWindow* window )
    {
        SetSashSize( 0 );
        return wxSplitterWindow::Unsplit( window );
    }

private:
    DECLARE_EVENT_TABLE()

    void OnSize( wxSizeEvent &event )
    {
        /* If we display video, then resize the video window */
        if( GetWindow2() &&
            p_intf->p_sys->p_video_window && p_intf->p_sys->p_video_sizer &&
            p_intf->p_sys->p_video_sizer->GetMinSize() != wxSize(0,0) )
        {
            if( !b_video ) i_delay = mdate() + 1000000;
            b_video = VLC_TRUE;

            SetSashSize( -1 );

#if defined( __WXMSW__ )
            SetSashPosition( event.GetSize().GetHeight() - i_sash_position );
#else
            SetSashPosition( event.GetSize().GetHeight() -
                             i_sash_position - GetSashSize() );
#endif
        }
        else if( GetWindow2() && GetWindow1() && GetWindow1()->GetSizer() )
        {
            wxSize size = GetWindow1()->GetSizer()->GetMinSize();

            if( b_video ) i_delay = mdate() + 1000000;
            b_video = VLC_FALSE;

            if( event.GetSize().GetHeight() - size.GetHeight() )
            {
                SetSashSize( 0 );

                SetSashPosition( size.GetHeight() ? size.GetHeight() : 1 );

                if( i_delay < mdate() )
                {
                    i_sash_position = event.GetSize().GetHeight() -
                        size.GetHeight();
                    i_width = event.GetSize().GetWidth();

                    size = wxSize( i_width, i_sash_position );
                    if( GetWindow2()->GetSizer() )
                        GetWindow2()->GetSizer()->SetMinSize( size );
                }
            }
        }

        event.Skip();
    }

    void OnSashPosChanged( wxSplitterEvent &event )
    {
        if( !GetSize().GetHeight() ){ event.Skip(); return; }

        if( i_delay < mdate() )
        {
            i_sash_position = GetSize().GetHeight() - event.GetSashPosition();

            wxSize size = wxSize( i_width, i_sash_position );
            if( GetWindow2()->GetSizer() )
                GetWindow2()->GetSizer()->SetMinSize( size );
        }
        event.Skip();
    }

    intf_thread_t *p_intf;
    int i_sash_position;
    int i_width;
    vlc_bool_t b_video;
    mtime_t i_delay;
};

BEGIN_EVENT_TABLE(Splitter, wxSplitterWindow)
    EVT_SIZE( Splitter::OnSize )
    EVT_SPLITTER_SASH_POS_CHANGED(-1, Splitter::OnSashPosChanged)
END_EVENT_TABLE()

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

DEFINE_LOCAL_EVENT_TYPE( wxEVT_INTF );

/* IDs for the controls and the menu commands */
enum
{
    /* menu items */
    MenuDummy_Event = wxID_HIGHEST + 1000,
    Exit_Event = wxID_HIGHEST,
    OpenFileSimple_Event,
    OpenAdv_Event,
    OpenFile_Event,
    OpenDir_Event,
    OpenDisc_Event,
    OpenNet_Event,
    OpenCapture_Event,
    OpenSat_Event,
    OpenOther_Event,
    EjectDisc_Event,

    Wizard_Event,

    Playlist_Event,
    PlaylistSmall_Event,
    Logs_Event,
    FileInfo_Event,

    Prefs_Event,
    Extended_Event,
    Bookmarks_Event,
    Skins_Event,

    StopStream_Event,
    PlayStream_Event,
    PrevStream_Event,
    NextStream_Event,
    SlowStream_Event,
    FastStream_Event,

    /* it is important for the id corresponding to the "About" command to have
     * this standard value as otherwise it won't be handled properly under Mac
     * (where it is special and put into the "Apple" menu) */
    About_Event = wxID_ABOUT,
    UpdateVLC_Event,
    VLM_Event,

    Iconize_Event
};

BEGIN_EVENT_TABLE(Interface, wxFrame)
    /* Menu events */
    EVT_MENU(Exit_Event, Interface::OnExit)
    EVT_MENU(About_Event, Interface::OnAbout)
    EVT_MENU(UpdateVLC_Event, Interface::OnShowDialog)
    EVT_MENU(VLM_Event, Interface::OnShowDialog)

    EVT_MENU(Playlist_Event, Interface::OnShowDialog)
    EVT_MENU(PlaylistSmall_Event, Interface::OnSmallPlaylist)
    EVT_MENU(Logs_Event, Interface::OnShowDialog)
    EVT_MENU(FileInfo_Event, Interface::OnShowDialog)
    EVT_MENU(Prefs_Event, Interface::OnShowDialog)

    EVT_MENU_OPEN(Interface::OnMenuOpen)

    EVT_MENU( Extended_Event, Interface::OnExtended )

    EVT_MENU( Bookmarks_Event, Interface::OnShowDialog)

#if defined( __WXMSW__ ) || defined( __WXMAC__ )
    EVT_CONTEXT_MENU(Interface::OnContextMenu2)
#endif
    EVT_RIGHT_UP(Interface::OnContextMenu)

    /* Toolbar events */
    EVT_MENU(OpenFileSimple_Event, Interface::OnShowDialog)
    EVT_MENU(OpenAdv_Event, Interface::OnShowDialog)
    EVT_MENU(OpenFile_Event, Interface::OnShowDialog)
    EVT_MENU(OpenDir_Event, Interface::OnShowDialog)
    EVT_MENU(OpenDisc_Event, Interface::OnShowDialog)
    EVT_MENU(OpenNet_Event, Interface::OnShowDialog)
    EVT_MENU(OpenCapture_Event, Interface::OnShowDialog)
    EVT_MENU(OpenSat_Event, Interface::OnShowDialog)
    EVT_MENU(Wizard_Event, Interface::OnShowDialog)
    EVT_MENU(StopStream_Event, Interface::OnStopStream)
    EVT_MENU(PlayStream_Event, Interface::OnPlayStream)
    EVT_MENU(PrevStream_Event, Interface::OnPrevStream)
    EVT_MENU(NextStream_Event, Interface::OnNextStream)
    EVT_MENU(SlowStream_Event, Interface::OnSlowStream)
    EVT_MENU(FastStream_Event, Interface::OnFastStream)

    /* Custom events */
    EVT_COMMAND(0, wxEVT_INTF, Interface::OnControlEvent)
    EVT_COMMAND(1, wxEVT_INTF, Interface::OnControlEvent)
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Interface::Interface( intf_thread_t *_p_intf, long style ):
    wxFrame( NULL, -1, wxT("VLC media player"),
             wxDefaultPosition, wxSize(700,100), style )
{
    /* Initializations */
    p_intf = _p_intf;
    b_extra = VLC_FALSE;
    extra_frame = 0;
    playlist_manager = 0;

    /* Give our interface a nice little icon */
    SetIcon( wxIcon( vlc_xpm ) );

    /* Create a splitter window that will fill in the interface window.
     * We need a splitter bar in order to make the embedded playlist
     * resizable. */
    splitter = new Splitter( this, p_intf );
    main_sizer = new wxBoxSizer( wxVERTICAL );
    main_sizer->Add( splitter, 1, wxEXPAND );
    SetSizer( main_sizer );

    /* Create a main panel that will fill in the interface window */
    main_panel = new wxPanel( splitter, -1, wxPoint(0,0), wxSize(0,0),
                              wxCLIP_CHILDREN );
    main_panel->SetFocus();

#if defined(__WXGTK20__) && wxCHECK_VERSION(2,5,6)
    /* As ugly as your butt! Please remove when wxWidgets 2.6 fixed their
     * Accelerators bug. */
    main_panel->m_imData = 0;
    m_imData = 0;
#endif

    /* Create a sizer for the main frame */
    panel_sizer = new wxBoxSizer( wxVERTICAL );
    main_panel->SetSizer( panel_sizer );

    /* Put this in the splitter */
    splitter->Initialize( main_panel );

#ifdef wxHAS_TASK_BAR_ICON
    /* Systray integration */
    p_systray = NULL;
    if( config_GetInt( p_intf, "wx-systray" ) )
    {
        p_systray = new Systray( this, p_intf );
    }
#endif

    /* Creation of the menu bar */
    CreateOurMenuBar();

    /* Creation of the tool bar */
    CreateOurToolBar();

    /* Creation of the status bar
     * Helptext for menu items and toolbar tools will automatically get
     * displayed here. */
    int i_status_width[3] = {100, 40, -1};
    statusbar = CreateStatusBar( 3 );                            /* 2 fields */
    statusbar->SetStatusWidths( 3, i_status_width );
    statusbar->SetStatusText( wxString::Format(wxT("x%.2f"), 1.0), 1 );

    /* Get minimum window size to prevent user from glitching it */
    splitter->SetSizeHints( -1, 0 );
    panel_sizer->Layout(); panel_sizer->Fit( main_panel );
    main_sizer->Layout(); main_sizer->Fit( this );
    main_min_size = GetSize();
    splitter->SetSizeHints( -1, -1 );

    /* Video window */
    video_window = 0;
    if( config_GetInt( p_intf, "wx-embed" ) )
    {
        video_window = CreateVideoWindow( p_intf, main_panel );
        panel_sizer->Add( p_intf->p_sys->p_video_sizer, 1, wxEXPAND, 0 );
    }

    /* Creation of the input manager panel */
    input_manager = new InputManager( p_intf, this, main_panel );
    panel_sizer->Add( input_manager, 0, wxEXPAND , 0 );

    /* Layout everything */
    splitter->SetSizeHints( -1, 0 );
    panel_sizer->Layout(); panel_sizer->Fit( main_panel );
    main_sizer->Layout(); main_sizer->Fit( this );
    splitter->SetSizeHints( -1, -1 );

#if wxUSE_DRAG_AND_DROP
    /* Associate drop targets with the main interface */
    SetDropTarget( new DragAndDrop( p_intf ) );
#endif

    SetupHotkeys();

    /* Start timer */
    timer = new Timer( p_intf, this );

    /* Restore previous position / settings */
    WindowSettings *ws = p_intf->p_sys->p_window_settings;
    wxPoint p;
    wxSize s;
    bool b_shown;

    ws->SetScreen( wxSystemSettings::GetMetric( wxSYS_SCREEN_X ),
                   wxSystemSettings::GetMetric( wxSYS_SCREEN_Y ) );

    if( ws->GetSettings( WindowSettings::ID_MAIN, b_shown, p, s ) ) Move( p );

    /* Show extended GUI if requested */
    wxCommandEvent dummy;
    if( config_GetInt( p_intf, "wx-extended" ) ) OnExtended( dummy );

    SetIntfMinSize();
}

Interface::~Interface()
{
    WindowSettings *ws = p_intf->p_sys->p_window_settings;

    if( !IsIconized() )
    {
        ws->SetSettings( WindowSettings::ID_MAIN, true,
                         GetPosition(), GetSize() );
    }

    PopEventHandler(true);

    if( video_window ) delete video_window;

#ifdef wxHAS_TASK_BAR_ICON
    if( p_systray ) delete p_systray;
#endif

    if( p_intf->p_sys->p_wxwindow ) delete p_intf->p_sys->p_wxwindow;

    /* Clean up */
    delete timer;
}

void Interface::Init()
{
    /* Misc init */
    SetupHotkeys();
}

void Interface::Update()
{
    /* Misc updates */
    ((VLCVolCtrl *)volctrl)->UpdateVolume();

    if( playlist_manager ) playlist_manager->Update();
}

void Interface::OnControlEvent( wxCommandEvent& event )
{
    switch( event.GetId() )
    {
    case 0:
        main_sizer->Layout();
        main_sizer->Fit( this );
        break;

    case 1:
        long i_style = GetWindowStyle();
        if( event.GetInt() ) i_style |= wxSTAY_ON_TOP;
        else i_style &= ~wxSTAY_ON_TOP;
        SetWindowStyle( i_style );
        break;
    }
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void Interface::CreateOurMenuBar()
{
    int minimal = config_GetInt( p_intf, "wx-minimal" );

    /* Create the "File" menu */
    wxMenu *file_menu = new wxMenu;

    if (!minimal)
    {
    file_menu->Append( OpenFileSimple_Event,
                       wxU(_("Quick &Open File...\tCtrl-O")) );

    file_menu->AppendSeparator();
    file_menu->Append( OpenFile_Event, wxU(_("Open &File...\tCtrl-F")) );
    file_menu->Append( OpenDir_Event, wxU(_("Open Dir&ectory...\tCtrl-E")) );
    file_menu->Append( OpenDisc_Event, wxU(_("Open &Disc...\tCtrl-D")) );
    file_menu->Append( OpenNet_Event,
                       wxU(_("Open &Network Stream...\tCtrl-N")) );
    file_menu->Append( OpenCapture_Event,
                       wxU(_("Open C&apture Device...\tCtrl-A")) );

    file_menu->AppendSeparator();
    file_menu->Append( Wizard_Event, wxU(_("&Wizard...\tCtrl-W")) );
    file_menu->AppendSeparator();
    }
    file_menu->Append( Exit_Event, wxU(_("E&xit\tCtrl-X")) );

    /* Create the "View" menu */
    wxMenu *view_menu = new wxMenu;
    if (!minimal)
    {
    view_menu->Append( Playlist_Event, wxU(_("&Playlist...\tCtrl-P")) );
    }
    view_menu->Append( Logs_Event, wxU(_("&Messages...\tCtrl-M")) );
    view_menu->Append( FileInfo_Event,
                       wxU(_("Stream and Media &info...\tCtrl-I")) );
    view_menu->Append( VLM_Event,
                       wxU(_("VLM Control...\tCtrl-V")) );

    /* Create the "Auto-generated" menus */
    p_settings_menu = SettingsMenu( p_intf, this );
    p_audio_menu = AudioMenu( p_intf, this );
    p_video_menu = VideoMenu( p_intf, this );
    p_navig_menu = NavigMenu( p_intf, this );

    /* Create the "Help" menu */
    wxMenu *help_menu = new wxMenu;
    help_menu->Append( About_Event, wxU(_("About VLC media player")) );
    help_menu->AppendSeparator();
    help_menu->Append( UpdateVLC_Event, wxU(_("Check for updates ...")) );

    /* Append the freshly created menus to the menu bar... */
    wxMenuBar *menubar = new wxMenuBar();
    menubar->Append( file_menu, wxU(_("&File")) );
    menubar->Append( view_menu, wxU(_("&View")) );
    menubar->Append( p_settings_menu, wxU(_("&Settings")) );
    menubar->Append( p_audio_menu, wxU(_("&Audio")) );
    menubar->Append( p_video_menu, wxU(_("&Video")) );
    menubar->Append( p_navig_menu, wxU(_("&Navigation")) );
    menubar->Append( help_menu, wxU(_("&Help")) );

    /* Attach the menu bar to the frame */
    SetMenuBar( menubar );

    /* Find out size of menu bar */
    int i_size = 0;
    for( unsigned int i = 0; i < menubar->GetMenuCount(); i++ )
    {
        int i_width, i_height;
        menubar->GetTextExtent( menubar->GetLabelTop(i), &i_width, &i_height );
        i_size += i_width +
#if defined(__WXGTK__)
            22 /* approximate margin */;
#else
#if (wxMAJOR_VERSION <= 2) && (wxMINOR_VERSION <= 5) && (wxRELEASE_NUMBER < 3)
            4 /* approximate margin */;
#else
            18 /* approximate margin */;
#endif
#endif
    }

/* Patch by zcot for menu wrapping */
#if defined(WIN32)
    /* Find out size of msw menu bar */
    i_size = 0;
    SIZE sizing;
    HDC hdc = GetDC( NULL );
    for( unsigned int i = 0; i < menubar->GetMenuCount(); i++ )
    {
        GetTextExtentPoint32( hdc, menubar->GetLabelTop(i).c_str(),
                                menubar->GetLabelTop(i).Length(), &sizing );

        // [ SM_CXDLGFRAME + pixels + textextent + pixels + SM_CXDLGFRAME ]
        i_size += sizing.cx + 2 + GetSystemMetrics( SM_CXDLGFRAME ) * 2;
    }
    ReleaseDC( NULL, hdc );
    i_size += GetSystemMetrics( SM_CXSIZEFRAME ) * 2 + 4;
#endif
/* End patch by zcot */

    panel_sizer->SetMinSize( i_size, -1 );

    /* Intercept all menu events in our custom event handler */
    PushEventHandler( new MenuEvtHandler( p_intf, this ) );

#if wxUSE_DRAG_AND_DROP
    /* Associate drop targets with the menubar */
    menubar->SetDropTarget( new DragAndDrop( p_intf ) );
#endif
}

void Interface::CreateOurToolBar()
{
#define HELP_OPEN N_("Open")
#define HELP_STOP N_("Stop")
#define HELP_PLAY N_("Play")
#define HELP_PAUSE N_("Pause")
#define HELP_PLO N_("Playlist")
#define HELP_SPLO N_("Small playlist")
#define HELP_PLP N_("Previous playlist item")
#define HELP_PLN N_("Next playlist item")
#define HELP_SLOW N_("Play slower")
#define HELP_FAST N_("Play faster")

    int minimal = config_GetInt( p_intf, "wx-minimal" );

    wxLogNull LogDummy; /* Hack to suppress annoying log message on the win32
                         * version because we don't include wx.rc */

    wxToolBar *toolbar =
        CreateToolBar( wxTB_HORIZONTAL | wxTB_FLAT );

    toolbar->SetToolBitmapSize( wxSize(TOOLBAR_BMP_WIDTH,TOOLBAR_BMP_HEIGHT) );

    if (!minimal)
    {
    toolbar->AddTool( OpenFile_Event, wxT(""),
                      wxBitmap( eject_xpm ), wxU(_(HELP_OPEN)) );
    toolbar->AddSeparator();
    }

    wxToolBarToolBase *p_tool = toolbar->AddTool( PlayStream_Event, wxT(""),
                      wxBitmap( play_xpm ), wxU(_(HELP_PLAY)), wxITEM_CHECK );
    p_tool->SetClientData( p_tool );

    if (!minimal)
    {
    toolbar->AddTool( StopStream_Event, wxT(""), wxBitmap( stop_xpm ),
                      wxU(_(HELP_STOP)) );
    toolbar->AddSeparator();

    toolbar->AddTool( PrevStream_Event, wxT(""),
                      wxBitmap( prev_xpm ), wxU(_(HELP_PLP)) );
    toolbar->AddTool( SlowStream_Event, wxT(""),
                      wxBitmap( slow_xpm ), wxU(_(HELP_SLOW)) );
    toolbar->AddTool( FastStream_Event, wxT(""),
                      wxBitmap( fast_xpm ), wxU(_(HELP_FAST)) );
    toolbar->AddTool( NextStream_Event, wxT(""), wxBitmap( next_xpm ),
                      wxU(_(HELP_PLN)) );
    toolbar->AddSeparator();
    toolbar->AddTool( Playlist_Event, wxT(""), wxBitmap( playlist_xpm ),
                      wxU(_(HELP_PLO)) );
    toolbar->AddTool( PlaylistSmall_Event, wxT(""),
                      wxBitmap( playlist_small_xpm ), wxU(_(HELP_SPLO)) );
    }

    wxControl *p_dummy_ctrl =
        new wxControl( toolbar, -1, wxDefaultPosition,
                       wxSize(16, 16 ), wxBORDER_NONE );

    toolbar->AddControl( p_dummy_ctrl );

    volctrl = new VLCVolCtrl( p_intf, toolbar );
    toolbar->AddControl( volctrl );

    toolbar->Realize();

#if wxUSE_DRAG_AND_DROP
    /* Associate drop targets with the toolbar */
    toolbar->SetDropTarget( new DragAndDrop( p_intf ) );
#endif
}

static int ConvertHotkeyModifiers( int i_hotkey )
{
    int i_accel_flags = 0;
    if( i_hotkey & KEY_MODIFIER_ALT ) i_accel_flags |= wxACCEL_ALT;
    if( i_hotkey & KEY_MODIFIER_CTRL ) i_accel_flags |= wxACCEL_CTRL;
    if( i_hotkey & KEY_MODIFIER_SHIFT ) i_accel_flags |= wxACCEL_SHIFT;
    if( !i_accel_flags ) i_accel_flags = wxACCEL_NORMAL;
    return i_accel_flags;
}

static int ConvertHotkey( int i_hotkey )
{
    int i_key = i_hotkey & ~KEY_MODIFIER;
    if( i_key & KEY_ASCII ) return i_key & KEY_ASCII;
    else if( i_key & KEY_SPECIAL )
    {
        switch ( i_key )
        {
        case KEY_LEFT: return WXK_LEFT;
        case KEY_RIGHT: return WXK_RIGHT;
        case KEY_UP: return WXK_UP;
        case KEY_DOWN: return WXK_DOWN;
        case KEY_SPACE: return WXK_SPACE;
        case KEY_ENTER: return WXK_RETURN;
        case KEY_F1: return WXK_F1;
        case KEY_F2: return WXK_F2;
        case KEY_F3: return WXK_F3;
        case KEY_F4: return WXK_F4;
        case KEY_F5: return WXK_F5;
        case KEY_F6: return WXK_F6;
        case KEY_F7: return WXK_F7;
        case KEY_F8: return WXK_F8;
        case KEY_F9: return WXK_F9;
        case KEY_F10: return WXK_F10;
        case KEY_F11: return WXK_F11;
        case KEY_F12: return WXK_F12;
        case KEY_HOME: return WXK_HOME;
        case KEY_END: return WXK_END;
        case KEY_INSERT: return WXK_INSERT;
        case KEY_DELETE: return WXK_DELETE;
        case KEY_MENU: return WXK_MENU;
        case KEY_ESC: return WXK_ESCAPE;
        case KEY_PAGEUP: return WXK_PRIOR;
        case KEY_PAGEDOWN: return WXK_NEXT;
        case KEY_TAB: return WXK_TAB;
        case KEY_BACKSPACE: return WXK_BACK;
        }
    }
    return WXK_F24;
}

void Interface::SetupHotkeys()
{
    struct vlc_t::hotkey *p_hotkeys = p_intf->p_vlc->p_hotkeys;
    int i_hotkeys;

    /* Count number of hoteys */
    for( i_hotkeys = 0; p_hotkeys[i_hotkeys].psz_action != NULL; i_hotkeys++ );

    p_intf->p_sys->i_first_hotkey_event = wxID_HIGHEST + 7000;
    p_intf->p_sys->i_hotkeys = i_hotkeys;

    wxAcceleratorEntry *p_entries = new wxAcceleratorEntry[i_hotkeys];

    /* Setup the hotkeys as accelerators */
    for( int i = 0; i < i_hotkeys; i++ )
    {
        int i_mod = ConvertHotkeyModifiers( p_hotkeys[i].i_key );
        int i_key = ConvertHotkey( p_hotkeys[i].i_key );

#ifdef WIN32
        if( !(p_hotkeys[i].i_key & KEY_SPECIAL) && i_mod )
            i_key = toupper(i_key);
#endif

        p_entries[i].Set( i_mod, i_key,
                          p_intf->p_sys->i_first_hotkey_event + i );
    }

    wxAcceleratorTable accel( i_hotkeys, p_entries );

    if( !accel.Ok() )
    {
        msg_Err( p_intf, "invalid accelerator table" );
    }
    else
    {
        SetAcceleratorTable( accel );
    }

    delete [] p_entries;
}

void Interface::SetIntfMinSize()
{
    wxSize ms = main_min_size;

    if( extra_frame && extra_frame->IsShown() )
    {
        ms.SetHeight( ms.GetHeight() + ext_min_size.GetHeight() );
        if( ext_min_size.GetWidth() > ms.GetWidth() )
            ms.SetWidth( ext_min_size.GetWidth() );
    }

    SetSizeHints( ms.GetWidth(), ms.GetHeight() );
}

/*****************************************************************************
 * Event Handlers.
 *****************************************************************************/
void Interface::OnMenuOpen( wxMenuEvent& event )
{
#if defined( __WXMSW__ )
#   define GetEventObject GetMenu
#endif

    if( event.GetEventObject() == p_settings_menu )
    {
        p_settings_menu = SettingsMenu( p_intf, this, p_settings_menu );

        /* Add static items */
        p_settings_menu->AppendCheckItem( Extended_Event,
            wxU(_("Extended &GUI\tCtrl-G") ) );
        if( b_extra ) p_settings_menu->Check( Extended_Event, TRUE );
        p_settings_menu->Append( Bookmarks_Event,
                                 wxU(_("&Bookmarks...\tCtrl-B") ) );
        p_settings_menu->Append( Prefs_Event,
                                 wxU(_("Preference&s...\tCtrl-S")) );
    }

    else if( event.GetEventObject() == p_audio_menu )
    {
        p_audio_menu = AudioMenu( p_intf, this, p_audio_menu );
    }

    else if( event.GetEventObject() == p_video_menu )
    {
        p_video_menu = VideoMenu( p_intf, this, p_video_menu );
    }

    else if( event.GetEventObject() == p_navig_menu )
    {
        p_navig_menu = NavigMenu( p_intf, this, p_navig_menu );
    }

#if defined( __WXMSW__ )
#   undef GetEventObject
#endif
}

#if defined( __WXMSW__ ) || defined( __WXMAC__ )
void Interface::OnContextMenu2(wxContextMenuEvent& event)
{
    /* Only show the context menu for the main interface */
    if( GetId() != event.GetId() )
    {
        event.Skip();
        return;
    }

    if( p_intf->p_sys->pf_show_dialog )
        p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_POPUPMENU, 1, 0 );
}
#endif
void Interface::OnContextMenu(wxMouseEvent& event)
{
    if( p_intf->p_sys->pf_show_dialog )
        p_intf->p_sys->pf_show_dialog( p_intf, INTF_DIALOG_POPUPMENU, 1, 0 );
}

void Interface::OnExit( wxCommandEvent& WXUNUSED(event) )
{
    /* TRUE is to force the frame to close. */
    Close(TRUE);
}

void Interface::OnAbout( wxCommandEvent& WXUNUSED(event) )
{
    wxString msg;
    msg.Printf( wxString(wxT("VLC media player " PACKAGE_VERSION)) +
        wxU(_(" (wxWidgets interface)\n\n")) +
        wxU(_("(c) 1996-2005 - the VideoLAN Team\n\n")) +
       wxU(_("Compiled by "))+ wxU(VLC_CompileBy())+ wxU("@") +
       wxU(VLC_CompileHost())+ wxT(".")+ wxU(VLC_CompileDomain())+ wxT(".\n") +
       wxU(_("Compiler: "))+ wxU(VLC_Compiler())+wxT( ".\n") +
       wxU(_("Based on SVN revision: "))+wxU(VLC_Changeset())+wxT(".\n\n") +
#ifdef __WXMSW__
        wxU( vlc_wraptext(LICENSE_MSG,WRAPCOUNT,VLC_TRUE) ) + wxT("\n\n") +
#else
        wxU( LICENSE_MSG ) + wxT("\n\n") +
#endif
        wxU(_("The VideoLAN team <videolan@videolan.org>\n"
              "http://www.videolan.org/\n\n")) );

    wxMessageBox( msg, wxString::Format(wxU(_("About %s")),
                  wxT("VLC media player")), wxOK | wxICON_INFORMATION, this );
}

void Interface::OnShowDialog( wxCommandEvent& event )
{
    if( p_intf->p_sys->pf_show_dialog )
    {
        int i_id;

        switch( event.GetId() )
        {
        case OpenFileSimple_Event:
            i_id = INTF_DIALOG_FILE_SIMPLE;
            break;
        case OpenAdv_Event:
            i_id = INTF_DIALOG_FILE;
            break;
        case OpenFile_Event:
            i_id = INTF_DIALOG_FILE;
            break;
        case OpenDir_Event:
            i_id = INTF_DIALOG_DIRECTORY;
            break;
        case OpenDisc_Event:
            i_id = INTF_DIALOG_DISC;
            break;
        case OpenNet_Event:
            i_id = INTF_DIALOG_NET;
            break;
        case OpenCapture_Event:
            i_id = INTF_DIALOG_CAPTURE;
            break;
        case OpenSat_Event:
            i_id = INTF_DIALOG_SAT;
            break;
        case Playlist_Event:
            i_id = INTF_DIALOG_PLAYLIST;
            break;
        case Logs_Event:
            i_id = INTF_DIALOG_MESSAGES;
            break;
        case FileInfo_Event:
            i_id = INTF_DIALOG_FILEINFO;
            break;
        case Prefs_Event:
            i_id = INTF_DIALOG_PREFS;
            break;
        case Wizard_Event:
            i_id = INTF_DIALOG_WIZARD;
            break;
        case Bookmarks_Event:
            i_id = INTF_DIALOG_BOOKMARKS;
            break;
        case UpdateVLC_Event:
            i_id = INTF_DIALOG_UPDATEVLC;
            break;
        case VLM_Event:
            i_id = INTF_DIALOG_VLM;
            break;
        default:
            i_id = INTF_DIALOG_FILE;
            break;
        }

        p_intf->p_sys->pf_show_dialog( p_intf, i_id, 1, 0 );
    }
}

void Interface::OnExtended( wxCommandEvent& WXUNUSED(event) )
{
    UpdateVideoWindow( p_intf, video_window );

    if( !extra_frame )
    {
        /* Create the extra panel */
        extra_frame = new ExtraPanel( p_intf, main_panel );
        panel_sizer->Add( extra_frame, 0, wxEXPAND , 0 );
        ext_min_size = extra_frame->GetBestSize();
    }

    b_extra = !b_extra;
    panel_sizer->Show( extra_frame, b_extra );

    SetIntfMinSize();
    main_sizer->Layout();
    main_sizer->Fit( this );
}

void Interface::OnSmallPlaylist( wxCommandEvent& WXUNUSED(event) )
{
    UpdateVideoWindow( p_intf, video_window );

    if( !playlist_manager )
    {
        /* Create the extra panel */
        playlist_manager = new PlaylistManager( p_intf, splitter );
    }

    if( !splitter->IsSplit() ) splitter->Split( main_panel, playlist_manager );
    else splitter->Unsplit( playlist_manager );

    SetIntfMinSize();
    main_sizer->Layout();
    main_sizer->Fit( this );
}

void Interface::OnPlayStream( wxCommandEvent& WXUNUSED(event) )
{
    PlayStream();
}

void Interface::PlayStream()
{
    wxCommandEvent dummy;
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist == NULL ) return;

    if( p_playlist->i_size && p_playlist->i_enabled )
    {
        vlc_value_t state;

        input_thread_t *p_input = (input_thread_t *)vlc_object_find( p_intf,
                                                       VLC_OBJECT_INPUT,
                                                       FIND_ANYWHERE );
        if( p_input == NULL )
        {
            /* No stream was playing, start one */
            playlist_Play( p_playlist );
            vlc_object_release( p_playlist );
            input_manager->Update();
            return;
        }

        var_Get( p_input, "state", &state );
        if( state.i_int != PAUSE_S )
        {
            /* A stream is being played, pause it */
            state.i_int = PAUSE_S;
        }
        else
        {
            /* Stream is paused, resume it */
            state.i_int = PLAYING_S;
        }
        var_Set( p_input, "state", state );

        vlc_object_release( p_input );
        vlc_object_release( p_playlist );
        input_manager->Update();
    }
    else
    {
        /* If the playlist is empty, open a file requester instead */
        vlc_object_release( p_playlist );
        OnShowDialog( dummy );
    }
}

void Interface::OnStopStream( wxCommandEvent& WXUNUSED(event) )
{
    StopStream();
}
void Interface::StopStream()
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
    input_manager->Update();
}

void Interface::OnPrevStream( wxCommandEvent& WXUNUSED(event) )
{
    PrevStream();
}

void Interface::PrevStream()
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
    NextStream();
}

void Interface::NextStream()
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
        vlc_value_t val; val.b_bool = VLC_TRUE;

        var_Set( p_input, "rate-slower", val );
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
        vlc_value_t val; val.b_bool = VLC_TRUE;

        var_Set( p_input, "rate-faster", val );
        vlc_object_release( p_input );
    }
}

void Interface::TogglePlayButton( int i_playing_status )
{
    wxToolBarToolBase *p_tool = (wxToolBarToolBase *)
        GetToolBar()->GetToolClientData( PlayStream_Event );
    if( !p_tool ) return;

    if( i_playing_status == PLAYING_S )
    {
        p_tool->SetNormalBitmap( wxBitmap( pause_xpm ) );
        p_tool->SetLabel( wxU(_("Pause")) );
        p_tool->SetShortHelp( wxU(_(HELP_PAUSE)) );
    }
    else
    {
        p_tool->SetNormalBitmap( wxBitmap( play_xpm ) );
        p_tool->SetLabel( wxU(_("Play")) );
        p_tool->SetShortHelp( wxU(_(HELP_PLAY)) );
    }

    GetToolBar()->Realize();

#if defined( __WXMSW__ )
    /* Needed to work around a bug in wxToolBar::Realize() */
    GetToolBar()->SetSize( GetSize().GetWidth(),
                           GetToolBar()->GetSize().GetHeight() );
    GetToolBar()->Update();
#endif

    GetToolBar()->ToggleTool( PlayStream_Event, true );
    GetToolBar()->ToggleTool( PlayStream_Event, false );
}

#if wxUSE_DRAG_AND_DROP
/*****************************************************************************
 * Definition of DragAndDrop class.
 *****************************************************************************/
DragAndDrop::DragAndDrop( intf_thread_t *_p_intf, vlc_bool_t _b_enqueue )
{
    p_intf = _p_intf;
    b_enqueue = _b_enqueue;
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
    {
        char *psz_utf8 = wxFromLocale( filenames[i] );

        playlist_Add( p_playlist, psz_utf8, psz_utf8,
                      PLAYLIST_APPEND | ((i | b_enqueue) ? 0 : PLAYLIST_GO),
                      PLAYLIST_END );
        wxLocaleFree( psz_utf8 );
    }

    vlc_object_release( p_playlist );

    return TRUE;
}
#endif

/*****************************************************************************
 * Definition of VolCtrl class.
 *****************************************************************************/
class wxVolCtrl: public wxGauge
{
public:
    /* Constructor */
    wxVolCtrl( intf_thread_t *_p_intf, wxWindow* parent, wxWindowID id,
               wxPoint = wxDefaultPosition, wxSize = wxSize( 20, -1 ) );
    virtual ~wxVolCtrl() {};

    void UpdateVolume();
    int GetVolume();

    void OnChange( wxMouseEvent& event );

private:
    intf_thread_t *p_intf;

    DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE(wxVolCtrl, wxWindow)
    /* Mouse events */
    EVT_LEFT_DOWN(wxVolCtrl::OnChange)
    EVT_MOTION(wxVolCtrl::OnChange)
END_EVENT_TABLE()

wxVolCtrl::wxVolCtrl( intf_thread_t *_p_intf, wxWindow* parent, wxWindowID id,
                      wxPoint point, wxSize size )
  : wxGauge( parent, id, 200, point, size, wxGA_HORIZONTAL | wxGA_SMOOTH )
{
    p_intf = _p_intf;
    UpdateVolume();
}

void wxVolCtrl::OnChange( wxMouseEvent& event )
{
    if( !event.LeftDown() && !event.LeftIsDown() ) return;

    int i_volume = event.GetX() * 200 / GetClientSize().GetWidth();
    aout_VolumeSet( p_intf, i_volume * AOUT_VOLUME_MAX / 200 / 2 );
    UpdateVolume();
}

void wxVolCtrl::UpdateVolume()
{
    audio_volume_t i_volume;
    aout_VolumeGet( p_intf, &i_volume );

    int i_gauge_volume = i_volume * 200 * 2 / AOUT_VOLUME_MAX;
    if( i_gauge_volume == GetValue() ) return;

    SetValue( i_gauge_volume );
    SetToolTip( wxString::Format((wxString)wxU(_("Volume")) + wxT(" %d"),
                i_gauge_volume / 2 ) );
}

#if defined(__WXGTK__)
#define VLCVOL_HEIGHT p_parent->GetSize().GetHeight()
#else
#define VLCVOL_HEIGHT TOOLBAR_BMP_HEIGHT
#endif
VLCVolCtrl::VLCVolCtrl( intf_thread_t *_p_intf, wxWindow *p_parent )
  :wxControl( p_parent, -1, wxDefaultPosition, wxSize(64, VLCVOL_HEIGHT ),
              wxBORDER_NONE ),
   i_y_offset((VLCVOL_HEIGHT - TOOLBAR_BMP_HEIGHT) / 2),
   b_mute(0), p_intf(_p_intf)
{
    gauge = new wxVolCtrl( p_intf, this, -1, wxPoint( 18, i_y_offset ),
                           wxSize( 44, TOOLBAR_BMP_HEIGHT ) );
}

void VLCVolCtrl::OnPaint( wxPaintEvent &evt )
{
    wxPaintDC dc( this );
    wxBitmap mPlayBitmap( b_mute ? speaker_mute_xpm : speaker_xpm );
    dc.DrawBitmap( mPlayBitmap, 0, i_y_offset, TRUE );
}

void VLCVolCtrl::OnChange( wxMouseEvent& event )
{
    if( event.GetX() < TOOLBAR_BMP_WIDTH )
    {
        int i_volume;
        aout_VolumeMute( p_intf, (audio_volume_t *)&i_volume );

        b_mute = !b_mute;
        Refresh();
    }
}

void VLCVolCtrl::UpdateVolume()
{
    gauge->UpdateVolume();

    int i_volume = gauge->GetValue();
    if( !!i_volume == !b_mute ) return;
    b_mute = !b_mute;
    Refresh();
}

/*****************************************************************************
 * Systray class.
 *****************************************************************************/

#ifdef wxHAS_TASK_BAR_ICON

BEGIN_EVENT_TABLE(Systray, wxTaskBarIcon)
    /* Mouse events */
#ifdef WIN32
    EVT_TASKBAR_LEFT_DCLICK(Systray::OnLeftClick)
#else
    EVT_TASKBAR_LEFT_DOWN(Systray::OnLeftClick)
#endif
    /* Menu events */
    EVT_MENU(Iconize_Event, Systray::OnMenuIconize)
    EVT_MENU(Exit_Event, Systray::OnExit)
    EVT_MENU(PlayStream_Event, Systray::OnPlayStream)
    EVT_MENU(NextStream_Event, Systray::OnNextStream)
    EVT_MENU(PrevStream_Event, Systray::OnPrevStream)
    EVT_MENU(StopStream_Event, Systray::OnStopStream)
END_EVENT_TABLE()

Systray::Systray( Interface *_p_main_interface, intf_thread_t *_p_intf )
{
    p_main_interface = _p_main_interface;
    p_intf = _p_intf;

    SetIcon( wxIcon( vlc16x16_xpm ), wxT("VLC media player") );
    if( !IsOk() || !IsIconInstalled() )
    {
        msg_Warn(p_intf, "cannot set systray icon, weird things may happen");
    }
}

/* Event handlers */
void Systray::OnMenuIconize( wxCommandEvent& event )
{
    p_main_interface->Show( ! p_main_interface->IsShown() );
    if ( p_main_interface->IsShown() ) p_main_interface->Raise();
}

void Systray::OnLeftClick( wxTaskBarIconEvent& event )
{
    wxCommandEvent cevent;
    OnMenuIconize(cevent);
}

void Systray::OnExit( wxCommandEvent& event )
{
    p_main_interface->Close(TRUE);
}

void Systray::OnPrevStream( wxCommandEvent& event )
{
    p_main_interface->PrevStream();
}

void Systray::OnNextStream( wxCommandEvent& event )
{
    p_main_interface->NextStream();
}

void Systray::OnPlayStream( wxCommandEvent& event )
{
    p_main_interface->PlayStream();
}

void Systray::OnStopStream( wxCommandEvent& event )
{
    p_main_interface->StopStream();
}

/* Systray popup menu */
wxMenu* Systray::CreatePopupMenu()
{
    int minimal = config_GetInt( p_intf, "wx-minimal" );

    wxMenu* systray_menu = new wxMenu;
    systray_menu->Append( Exit_Event, wxU(_("Quit VLC")) );
    systray_menu->AppendSeparator();
    systray_menu->Append( PlayStream_Event, wxU(_("Play/Pause")) );

    if (!minimal)
    {
    systray_menu->Append( PrevStream_Event, wxU(_("Previous")) );
    systray_menu->Append( NextStream_Event, wxU(_("Next")) );
    systray_menu->Append( StopStream_Event, wxU(_("Stop")) );
    }
    systray_menu->AppendSeparator();
    systray_menu->Append( Iconize_Event, wxU(_("Show/Hide interface")) );
    return systray_menu;
}

void Systray::UpdateTooltip( const wxChar* tooltip )
{
    SetIcon( wxIcon( vlc16x16_xpm ), tooltip );
}
#endif
