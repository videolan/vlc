/*****************************************************************************
 * interface.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001, 2003 VideoLAN
 * $Id: interface.cpp,v 1.81 2003/12/31 10:30:44 zorglub Exp $
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
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/vout.h>
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

#include <wx/listctrl.h>

#define TOOLBAR_BMP_WIDTH 36
#define TOOLBAR_BMP_HEIGHT 36

/* include the icon graphic */
#include "../../../share/vlc32x32.xpm"

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

class wxVolCtrl: public wxGauge
{
public:
    /* Constructor */
    wxVolCtrl( intf_thread_t *_p_intf, wxWindow* parent, wxWindowID id );
    virtual ~wxVolCtrl() {};

    void Change( int i_volume );

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

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    /* menu items */
    MenuDummy_Event = wxID_HIGHEST + 1000,
    Exit_Event = wxID_HIGHEST,
    OpenFileSimple_Event,
    OpenAdv_Event,
    OpenFile_Event,
    OpenDisc_Event,
    OpenNet_Event,
    OpenSat_Event,
    OpenOther_Event,
    EjectDisc_Event,

    Stream_Event,

    Playlist_Event,
    Logs_Event,
    FileInfo_Event,

    Prefs_Event,
    Extra_Event,
    Skins_Event,

    SliderScroll_Event,
    StopStream_Event,
    PlayStream_Event,
    PrevStream_Event,
    NextStream_Event,
    SlowStream_Event,
    FastStream_Event,

    Adjust_Event,
    Hue_Event,
    Contrast_Event,
    Brightness_Event,
    Saturation_Event,

    Ratio_Event,
    Visual_Event,

    /* it is important for the id corresponding to the "About" command to have
     * this standard value as otherwise it won't be handled properly under Mac
     * (where it is special and put into the "Apple" menu) */
    About_Event = wxID_ABOUT
};

BEGIN_EVENT_TABLE(Interface, wxFrame)
    /* Menu events */
    EVT_MENU(Exit_Event, Interface::OnExit)
    EVT_MENU(About_Event, Interface::OnAbout)

    EVT_MENU(Playlist_Event, Interface::OnShowDialog)
    EVT_MENU(Logs_Event, Interface::OnShowDialog)
    EVT_MENU(FileInfo_Event, Interface::OnShowDialog)
    EVT_MENU(Prefs_Event, Interface::OnShowDialog)

    EVT_MENU_OPEN(Interface::OnMenuOpen)

    EVT_MENU( Extra_Event, Interface::OnExtra)
    EVT_CHECKBOX( Adjust_Event, Interface::OnEnableAdjust)

    EVT_COMBOBOX( Ratio_Event, Interface::OnRatio)
    EVT_CHECKBOX( Visual_Event, Interface::OnEnableVisual)

#if defined( __WXMSW__ ) || defined( __WXMAC__ )
    EVT_CONTEXT_MENU(Interface::OnContextMenu2)
#endif
    EVT_RIGHT_UP(Interface::OnContextMenu)

    /* Toolbar events */
    EVT_MENU(OpenFileSimple_Event, Interface::OnShowDialog)
    EVT_MENU(OpenAdv_Event, Interface::OnShowDialog)
    EVT_MENU(OpenFile_Event, Interface::OnShowDialog)
    EVT_MENU(OpenDisc_Event, Interface::OnShowDialog)
    EVT_MENU(OpenNet_Event, Interface::OnShowDialog)
    EVT_MENU(OpenSat_Event, Interface::OnShowDialog)
    EVT_MENU(Stream_Event, Interface::OnStream)
    EVT_MENU(StopStream_Event, Interface::OnStopStream)
    EVT_MENU(PlayStream_Event, Interface::OnPlayStream)
    EVT_MENU(PrevStream_Event, Interface::OnPrevStream)
    EVT_MENU(NextStream_Event, Interface::OnNextStream)
    EVT_MENU(SlowStream_Event, Interface::OnSlowStream)
    EVT_MENU(FastStream_Event, Interface::OnFastStream)

    /* Slider events */
    EVT_COMMAND_SCROLL(SliderScroll_Event, Interface::OnSliderUpdate)

    EVT_COMMAND_SCROLL(Hue_Event, Interface::OnHueUpdate)
    EVT_COMMAND_SCROLL(Contrast_Event, Interface::OnContrastUpdate)
    EVT_COMMAND_SCROLL(Brightness_Event, Interface::OnBrightnessUpdate)
    EVT_COMMAND_SCROLL(Saturation_Event, Interface::OnSaturationUpdate)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Interface::Interface( intf_thread_t *_p_intf ):
    wxFrame( NULL, -1, wxT("VLC media player"),
             wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    i_old_playing_status = PAUSE_S;
    b_extra = VLC_FALSE;

    /* Give our interface a nice little icon */
    SetIcon( wxIcon( vlc_xpm ) );

    /* Create a sizer for the main frame */
    frame_sizer = new wxBoxSizer( wxVERTICAL );
    SetSizer( frame_sizer );

    /* Create a dummy widget that can get the keyboard focus */
    wxWindow *p_dummy = new wxWindow( this, 0, wxDefaultPosition,
                                      wxSize(0,0) );
    p_dummy->SetFocus();
    frame_sizer->Add( p_dummy, 0, wxEXPAND );

    /* Creation of the menu bar */
    CreateOurMenuBar();

    /* Creation of the tool bar */
    CreateOurToolBar();

    /* Creation of the slider sub-window */
    CreateOurSlider();
    frame_sizer->Add( slider_frame, 0, wxEXPAND , 0 );
    frame_sizer->Hide( slider_frame );

    /* Create the extra panel */
    CreateOurExtraPanel();
    frame_sizer->Add( extra_frame, 0, wxEXPAND , 0 );
    frame_sizer->Hide( extra_frame );

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
    frame_sizer->Layout();
    frame_sizer->Fit(this);

#if !defined(__WXX11__)
    /* Associate drop targets with the main interface */
    SetDropTarget( new DragAndDrop( p_intf ) );
#endif

    UpdateAcceleratorTable();

    /* Start timer */
    timer = new Timer( p_intf, this );
}

Interface::~Interface()
{
    if( p_intf->p_sys->p_wxwindow )
    {
        delete p_intf->p_sys->p_wxwindow;
    }

    /* Clean up */
    delete timer;
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
void Interface::CreateOurMenuBar()
{
#define HELP_SIMPLE N_("Quick file open")
#define HELP_ADV N_("Advanced open")
#define HELP_FILE  N_("Open a file")
#define HELP_DISC  N_("Open Disc Media")
#define HELP_NET   N_("Open a network stream")
#define HELP_SAT   N_("Open a satellite stream")
#define HELP_EJECT N_("Eject the DVD/CD")
#define HELP_EXIT  N_("Exit this program")

#define HELP_STREAMWIZARD N_("Open the streaming wizard")
#define HELP_OTHER N_("Open other types of inputs")

#define HELP_PLAYLIST   N_("Open the playlist")
#define HELP_LOGS       N_("Show the program logs")
#define HELP_FILEINFO       N_("Show information about the file being played")

#define HELP_PREFS N_("Go to the preferences menu")
#define EXTRA_PREFS N_("Shows the extended GUI")

#define HELP_ABOUT N_("About this program")

    /* Create the "File" menu */
    wxMenu *file_menu = new wxMenu;
    file_menu->Append( OpenFileSimple_Event, wxU(_("Quick &Open ...")),
                       wxU(_(HELP_SIMPLE)) );

    file_menu->AppendSeparator();
    file_menu->Append( OpenFile_Event, wxU(_("Open &File...")),
                      wxU(_(HELP_FILE)));
    file_menu->Append( OpenDisc_Event, wxU(_("Open &Disc...")),
                      wxU(_(HELP_DISC)));
    file_menu->Append( OpenNet_Event, wxU(_("Open &Network Stream...")),
                      wxU(_(HELP_NET)));

#if 0
    file_menu->Append( OpenSat_Event, wxU(_("Open &Satellite Stream...")),
                       wxU(_(HELP_NET)) );
#endif
    file_menu->AppendSeparator();
    file_menu->Append( Stream_Event, wxU(_("Streaming Wizard...")),
                       wxU(_(HELP_STREAMWIZARD)) );
    file_menu->AppendSeparator();
    file_menu->Append( Exit_Event, wxU(_("E&xit")), wxU(_(HELP_EXIT)) );

    /* Create the "View" menu */
    wxMenu *view_menu = new wxMenu;
    view_menu->Append( Playlist_Event, wxU(_("&Playlist...")),
                       wxU(_(HELP_PLAYLIST)) );
    view_menu->Append( Logs_Event, wxU(_("&Messages...")), wxU(_(HELP_LOGS)) );
    view_menu->Append( FileInfo_Event, wxU(_("&Stream and Media info...")),
                       wxU(_(HELP_FILEINFO)) );

    /* Create the "Settings" menu */
    p_settings_menu = new wxMenu;
    b_settings_menu = 1;

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
    menubar->Append( p_settings_menu, wxU(_("&Settings")) );
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

    wxToolBar *toolbar =
        CreateToolBar( wxTB_HORIZONTAL | wxTB_FLAT | wxTB_DOCKABLE );

    toolbar->SetToolBitmapSize( wxSize(TOOLBAR_BMP_WIDTH,TOOLBAR_BMP_HEIGHT) );

    toolbar->AddTool( OpenFileSimple_Event, wxU(_("Quick")),
                      wxBitmap( file_xpm ), wxU(_(HELP_SIMPLE)) );

    toolbar->AddSeparator();
    toolbar->AddTool( OpenFile_Event, wxU(_("File")), wxBitmap( file_xpm ),
                      wxU(_(HELP_FILE)) );
    toolbar->AddTool( OpenDisc_Event, wxU(_("Disc")), wxBitmap( disc_xpm ),
                      wxU(_(HELP_DISC)) );
    toolbar->AddTool( OpenNet_Event, wxU(_("Net")), wxBitmap( net_xpm ),
                      wxU(_(HELP_NET)) );
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
    toolbar_sizer->Add( toolbar, 1, 0, 0 );
    toolbar_sizer->Layout();

#ifndef WIN32
    frame_sizer->SetMinSize( toolbar_sizer->GetMinSize().GetWidth(), -1 );
#else /* That sucks but for some reason it works better */
    frame_sizer->SetMinSize( toolbar_sizer->GetMinSize().GetWidth()*2/3, -1 );
#endif

#if !defined(__WXX11__)
    /* Associate drop targets with the toolbar */
    toolbar->SetDropTarget( new DragAndDrop( p_intf ) );
#endif
}

void Interface::CreateOurSlider()
{
    /* Create a new frame and sizer containing the slider */
    slider_frame = new wxPanel( this, -1, wxDefaultPosition, wxDefaultSize );
    slider_frame->SetAutoLayout( TRUE );
    wxBoxSizer *frame_sizer =
        new wxBoxSizer( wxHORIZONTAL );

    /* Create static box to surround the slider */
    slider_box = new wxStaticBox( slider_frame, -1, wxT("") );

    /* Create sizer for slider frame */
    wxStaticBoxSizer *slider_sizer =
        new wxStaticBoxSizer( slider_box, wxHORIZONTAL );
    slider_sizer->SetMinSize( -1, 50 );

    /* Create slider */
    slider = new wxSlider( slider_frame, SliderScroll_Event, 0, 0,
                           SLIDER_MAX_POS, wxDefaultPosition, wxDefaultSize );
    slider_sizer->Add( slider, 1, wxEXPAND | wxALL, 5 );


    volctrl = new wxVolCtrl( p_intf, slider_frame, -1 );

    /* Add everything to the frame */
    frame_sizer->Add( slider_sizer, 1, wxEXPAND | wxBOTTOM, 5 );
    frame_sizer->Add( volctrl, 0, wxEXPAND | wxALL, 5 );
    slider_frame->SetSizer( frame_sizer );
    frame_sizer->Layout();
    frame_sizer->SetSizeHints(slider_frame);

    /* Hide the slider by default */
    slider_frame->Hide();
}


void Interface::CreateOurExtraPanel()
{
    char *psz_filters;

    extra_frame = new wxPanel( this, -1, wxDefaultPosition, wxDefaultSize );
    extra_frame->SetAutoLayout( TRUE );
    wxBoxSizer *extra_sizer = new wxBoxSizer( wxHORIZONTAL );

    /* Create static box to surround the adjust controls */
    wxStaticBox *adjust_box =
           new wxStaticBox( extra_frame, -1, wxU(_("Image adjust")) );

    /* Create the size for the frame */
    wxStaticBoxSizer *adjust_sizer =
        new wxStaticBoxSizer( adjust_box, wxVERTICAL );
    adjust_sizer->SetMinSize( -1, 50 );

    /* Create every controls */

    /* Create the adjust button */
    wxCheckBox * adjust_check = new wxCheckBox( extra_frame, Adjust_Event,
                                                 wxU(_("Enable")));


    wxBoxSizer *hue_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticText *hue_text = new wxStaticText( extra_frame, -1,
                                       wxU(_("Hue")) );
    hue_slider = new wxSlider ( extra_frame, Hue_Event, 0, 0,
                                360, wxDefaultPosition, wxDefaultSize );

    hue_sizer->Add(hue_text,1, 0 ,0);
    hue_sizer->Add(hue_slider,1, 0 ,0);
    hue_sizer->Layout();

    wxBoxSizer *contrast_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticText *contrast_text = new wxStaticText( extra_frame, -1,
                                       wxU(_("Contrast")) );
    contrast_slider = new wxSlider ( extra_frame, Contrast_Event, 0, 0,
                                200, wxDefaultPosition, wxDefaultSize);
    contrast_sizer->Add(contrast_text,1, 0 ,0);
    contrast_sizer->Add(contrast_slider,1, 0 ,0);
    contrast_sizer->Layout();

    wxBoxSizer *brightness_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticText *brightness_text = new wxStaticText( extra_frame, -1,
                                       wxU(_("Brightness")) );
    brightness_slider = new wxSlider ( extra_frame, Brightness_Event, 0, 0,
                           200, wxDefaultPosition, wxDefaultSize) ;
    brightness_sizer->Add(brightness_text,1,0,0);
    brightness_sizer->Add(brightness_slider,1,0,0);
    brightness_sizer->Layout();

    wxBoxSizer *saturation_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticText *saturation_text = new wxStaticText( extra_frame, -1,
                                          wxU(_("Saturation")) );
    saturation_slider = new wxSlider ( extra_frame, Saturation_Event, 0, 0,
                           300, wxDefaultPosition, wxDefaultSize );
    saturation_sizer->Add(saturation_text,1,0,0);
    saturation_sizer->Add(saturation_slider,1,0,0);
    saturation_sizer->Layout();

    adjust_sizer->Add(adjust_check, 1, wxEXPAND, 0);
    adjust_sizer->Add(hue_sizer, 1, wxEXPAND, 0);
    adjust_sizer->Add(contrast_sizer, 1, wxEXPAND, 0);
    adjust_sizer->Add(brightness_sizer, 1, wxEXPAND, 0);
    adjust_sizer->Add(saturation_sizer, 1, wxEXPAND, 0);

    extra_sizer->Add(adjust_sizer,1,wxBOTTOM,5);

#if 0
    /* Create sizer to surround the other controls */
    wxBoxSizer *other_sizer = new wxBoxSizer( wxVERTICAL );


    wxStaticBox *video_box =
            new wxStaticBox( extra_frame, -1, wxU(_("Video Options")) );
    /* Create the sizer for the frame */
    wxStaticBoxSizer *video_sizer =
       new wxStaticBoxSizer( video_box, wxVERTICAL );
    video_sizer->SetMinSize( -1, 50 );

    static const wxString ratio_array[] =
    {
        wxT("4:3"),
        wxT("16:9"),
    };

    wxBoxSizer *ratio_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticText *ratio_text = new wxStaticText( extra_frame, -1,
                                          wxU(_("Ratio")) );

    ratio_combo = new wxComboBox( extra_frame, Ratio_Event, wxT(""),
                                  wxDefaultPosition, wxSize(120,-1),
                                  WXSIZEOF(ratio_array), ratio_array,
                                  0 );

    ratio_sizer->Add( ratio_text, 0, wxALL, 2 );
    ratio_sizer->Add( ratio_combo, 0, wxALL, 2 );
    ratio_sizer->Layout();

    video_sizer->Add( ratio_sizer  , 0 , wxALL , 0 );
    video_sizer->Layout();

    wxBoxSizer *visual_sizer = new wxBoxSizer( wxHORIZONTAL );

    wxCheckBox *visual_checkbox = new wxCheckBox( extra_frame, Visual_Event,
                                            wxU(_("Visualisation")) );

    visual_sizer->Add( visual_checkbox, 0, wxEXPAND, 0);
    visual_sizer->Layout();

    wxStaticBox *audio_box =
              new wxStaticBox( extra_frame, -1, wxU(_("Audio Options")) );
    /* Create the sizer for the frame */
    wxStaticBoxSizer *audio_sizer =
        new wxStaticBoxSizer( audio_box, wxVERTICAL );
    audio_sizer->SetMinSize( -1, 50 );

    audio_sizer->Add( visual_sizer, 0, wxALL, 0);
    audio_sizer->Layout();

    other_sizer->Add( video_sizer, 0, wxALL | wxEXPAND , 0);
    other_sizer->Add( audio_sizer , 0 , wxALL | wxEXPAND , 0 );
    other_sizer->Layout();

    extra_sizer->Add(other_sizer,0,wxBOTTOM,5);
#endif

    extra_frame->SetSizer( extra_sizer );

    /* Layout the whole panel */
    extra_sizer->Layout();

    extra_sizer->SetSizeHints(extra_frame);

    /* Write down initial values */
#if 0
    psz_filters = config_GetPsz( p_intf, "audio-filter" );
    if( psz_filters && strstr( psz_filters, "visual" ) )
    {
        visual_checkbox->SetValue(1);
    }
    if( psz_filters ) free( psz_filters );
#endif
    psz_filters = config_GetPsz( p_intf, "filter" );
    if( psz_filters && strstr( psz_filters, "adjust" ) )
    {
        adjust_check->SetValue( 1 );
        saturation_slider->Enable();
        contrast_slider->Enable();
        brightness_slider->Enable();
        hue_slider->Enable();
    }
    else
    {
        adjust_check->SetValue( 0 );
        saturation_slider->Disable();
        contrast_slider->Disable();
        brightness_slider->Disable();
        hue_slider->Disable();
    }
    if( psz_filters ) free( psz_filters );

    int i_value = config_GetInt( p_intf, "hue" );
    if( i_value > 0 && i_value < 360 )
        hue_slider->SetValue( i_value );

    float f_value;
    f_value = config_GetFloat( p_intf, "saturation" );
    if( f_value > 0 && f_value < 5 )
        saturation_slider->SetValue( (int)(100 * f_value) );
    f_value = config_GetFloat( p_intf, "contrast" );
    if( f_value > 0 && f_value < 4 )
        contrast_slider->SetValue( (int)(100 * f_value) );
    f_value = config_GetFloat( p_intf, "brightness" );
    if( f_value > 0 && f_value < 2 )
        brightness_slider->SetValue( (int)(100 * f_value) );

    extra_frame->Hide();
}

void Interface::UpdateAcceleratorTable()
{
    /* Set some hotkeys */
    wxAcceleratorEntry entries[7];
    vlc_value_t val;
    int i = 0;

    var_Get( p_intf->p_vlc, "key-quit", &val );
    entries[i++].Set( ConvertHotkeyModifiers( val.i_int ),
                      ConvertHotkey( val.i_int ), Exit_Event );
    var_Get( p_intf->p_vlc, "key-stop", &val );
    entries[i++].Set( ConvertHotkeyModifiers( val.i_int ),
                      ConvertHotkey( val.i_int ), StopStream_Event );
    var_Get( p_intf->p_vlc, "key-play-pause", &val );
    entries[i++].Set( ConvertHotkeyModifiers( val.i_int ),
                      ConvertHotkey( val.i_int ), PlayStream_Event );
    var_Get( p_intf->p_vlc, "key-next", &val );
    entries[i++].Set( ConvertHotkeyModifiers( val.i_int ),
                      ConvertHotkey( val.i_int ), NextStream_Event );
    var_Get( p_intf->p_vlc, "key-prev", &val );
    entries[i++].Set( ConvertHotkeyModifiers( val.i_int ),
                      ConvertHotkey( val.i_int ), PrevStream_Event );
    var_Get( p_intf->p_vlc, "key-faster", &val );
    entries[i++].Set( ConvertHotkeyModifiers( val.i_int ),
                      ConvertHotkey( val.i_int ), FastStream_Event );
    var_Get( p_intf->p_vlc, "key-slower", &val );
    entries[i++].Set( ConvertHotkeyModifiers( val.i_int ),
                      ConvertHotkey( val.i_int ), SlowStream_Event );

    wxAcceleratorTable accel( 7, entries );

    if( !accel.Ok() )
        msg_Err( p_intf, "invalid accelerator table" );

    SetAcceleratorTable( accel );
    msg_Dbg( p_intf, "accelerator table loaded" );

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
    if( event.GetEventObject() == p_settings_menu )
    {
        if( b_settings_menu )
        {
            p_settings_menu = SettingsMenu( p_intf, this );

            /* Add static items */
            p_settings_menu->AppendCheckItem( Extra_Event,
                             wxU(_("&Extended GUI") ), wxU(_(EXTRA_PREFS)) );
            p_settings_menu->Append( Prefs_Event, wxU(_("&Preferences...")),
                                     wxU(_(HELP_PREFS)) );

            /* Work-around for buggy wxGTK */
            wxMenu *menu = GetMenuBar()->GetMenu( 2 );
            RecursiveDestroy( menu );
            /* End work-around */

            menu = GetMenuBar()->Replace( 2, p_settings_menu,
                                          wxU(_("&Settings")));
            if( menu ) delete menu;

            b_settings_menu = 0;
        }
        else b_settings_menu = 1;
    }
    else if( event.GetEventObject() == p_audio_menu )
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
    p_settings_menu = SettingsMenu( p_intf, this );
    /* Add static items */
    p_settings_menu->AppendCheckItem( Extra_Event, wxU(_("&Extended GUI") ),
                                      wxU(_(EXTRA_PREFS)) );
    p_settings_menu->Append( Prefs_Event, wxU(_("&Preferences...")),
                             wxU(_(HELP_PREFS)) );
    wxMenu *menu =
        GetMenuBar()->Replace( 2, p_settings_menu, wxU(_("&Settings")) );
    if( menu ) delete menu;

    p_audio_menu = AudioMenu( p_intf, this );
    menu = GetMenuBar()->Replace( 3, p_audio_menu, wxU(_("&Audio")) );
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
    msg.Printf( wxString(wxT("VLC media player " VERSION)) +
        wxU(_(" (wxWindows interface)\n\n")) +
        wxU(_("(c) 1996-2003 - the VideoLAN Team\n\n")) +
        wxU( vlc_wraptext(INTF_ABOUT_MSG,WRAPCOUNT,ISUTF8) ) + wxT("\n\n") +
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
        case OpenFile_Event:
            i_id = INTF_DIALOG_FILE;
            break;
        case OpenDisc_Event:
            i_id = INTF_DIALOG_DISC;
            break;
        case OpenNet_Event:
            i_id = INTF_DIALOG_NET;
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
        default:
            i_id = INTF_DIALOG_FILE;
            break;

        }

        p_intf->p_sys->pf_show_dialog( p_intf, i_id, 1, 0 );
    }
}


void Interface::OnStream( wxCommandEvent& event )
{
    StreamDialog *p_stream_dialog = new StreamDialog(p_intf,this);
    p_stream_dialog->Show();
}


void Interface::OnExtra(wxCommandEvent& event)
{
    if( b_extra == VLC_FALSE)
    {
        extra_frame->Show();
        frame_sizer->Show( extra_frame );
        b_extra = VLC_TRUE;
    }
    else
    {
        extra_frame->Hide();
        frame_sizer->Hide( extra_frame );
        b_extra = VLC_FALSE;
    }
    frame_sizer->Layout();
    frame_sizer->Fit(this);
}

void Interface::OnEnableAdjust(wxCommandEvent& event)
{
    char *psz_filters=config_GetPsz( p_intf, "filter");
    char *psz_new = NULL;
    if( event.IsChecked() )
    {
        if(psz_filters == NULL)
        {
            psz_new = strdup( "adjust" );
        }
        else
        {
            psz_new= (char *) malloc(strlen(psz_filters) + 8 );
            sprintf( psz_new, "%s:adjust", psz_filters);
        }
        config_PutPsz( p_intf, "filter", psz_new );
        vlc_value_t val;
        vout_thread_t *p_vout =
           (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                       FIND_ANYWHERE );
        if( p_vout != NULL )
        {
            val.psz_string = strdup( psz_new );
            var_Set( p_vout, "filter", val);
            vlc_object_release( p_vout );
        }
        if( val.psz_string ) free( val.psz_string );
        brightness_slider->Enable();
        saturation_slider->Enable();
        contrast_slider->Enable();
        hue_slider->Enable();
    }
    else
    {
        if( psz_filters != NULL )
        {

            char *psz_current;
            unsigned int i=0;
            for( i = 0; i< strlen(psz_filters ); i++)
            {
                if ( !strncasecmp( &psz_filters[i],"adjust",6 ))
                {
                    if(i > 0)
                        if( psz_filters[i-1] == ':' ) i--;
                    psz_current = strchr( &psz_filters[i+1] , ':' );
                    if( !psz_current )
                        psz_filters[i] = '\0';
                    else
                    {
                       memmove( &psz_filters[i] , psz_current,
                                &psz_filters[strlen(psz_filters)]-psz_current
                                +1);
                    }
                }
            }
            config_PutPsz( p_intf, "filter", psz_filters);
            vlc_value_t val;
            val.psz_string = strdup( psz_filters );
            vout_thread_t *p_vout =
               (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                       FIND_ANYWHERE );
            if( p_vout != NULL )
            {
                var_Set( p_vout, "filter", val);
                vlc_object_release( p_vout );
            }
            if( val.psz_string ) free( val.psz_string );
        }
        brightness_slider->Disable();
        saturation_slider->Disable();
        contrast_slider->Disable();
        hue_slider->Disable();
    }
    if(psz_filters) free(psz_filters);
    if(psz_new) free(psz_new);
}

void Interface::OnHueUpdate( wxScrollEvent& event)
{
   config_PutInt( p_intf , "hue" , event.GetPosition() );
}

void Interface::OnSaturationUpdate( wxScrollEvent& event)
{
   config_PutFloat( p_intf , "saturation" , (float)event.GetPosition()/100 );
}

void Interface::OnBrightnessUpdate( wxScrollEvent& event)
{
   config_PutFloat( p_intf , "brightness", (float)event.GetPosition()/100 );
}

void Interface::OnContrastUpdate(wxScrollEvent& event)
{
   config_PutFloat( p_intf , "contrast" , (float)event.GetPosition()/100 );

}

void Interface::OnRatio( wxCommandEvent& event )
{
   config_PutPsz( p_intf, "aspect-ratio", ratio_combo->GetValue().mb_str() );
}

void Interface::OnEnableVisual(wxCommandEvent& event)
{
    if( event.IsChecked() )
    {
        config_PutPsz( p_intf, "audio-filter", "visual" );
    }
    else
    {
        config_PutPsz( p_intf, "audio-filter", "" );
    }
}

void Interface::OnPlayStream( wxCommandEvent& WXUNUSED(event) )
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
            TogglePlayButton( PLAYING_S );
            vlc_object_release( p_playlist );
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

        TogglePlayButton( state.i_int );
        vlc_object_release( p_input );
        vlc_object_release( p_playlist );
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
            vlc_value_t pos;
            pos.f_float = (float)event.GetPosition() / (float)SLIDER_MAX_POS;

            var_Set( p_intf->p_sys->p_input, "position", pos );
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
            char psz_time[ MSTRTIME_MAX_SIZE ];

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

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->p_input != NULL )
    {
        vlc_mutex_lock( &p_playlist->p_input->stream.stream_lock );
        if( p_playlist->p_input->stream.p_selected_area->i_id > 1 )
        {
            vlc_value_t val; val.b_bool = VLC_TRUE;
            vlc_mutex_unlock( &p_playlist->p_input->stream.stream_lock );
            var_Set( p_playlist->p_input, "prev-title", val );
        } else
            vlc_mutex_unlock( &p_playlist->p_input->stream.stream_lock );
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

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

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->p_input != NULL )
    {
        vlc_mutex_lock( &p_playlist->p_input->stream.stream_lock );
        if( p_playlist->p_input->stream.i_area_nb > 1 &&
            p_playlist->p_input->stream.p_selected_area->i_id <
              p_playlist->p_input->stream.i_area_nb - 1 )
        {
            vlc_value_t val; val.b_bool = VLC_TRUE;
            vlc_mutex_unlock( &p_playlist->p_input->stream.stream_lock );
            var_Set( p_playlist->p_input, "next-title", val );
        } else
            vlc_mutex_unlock( &p_playlist->p_input->stream.stream_lock );
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

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
    if( i_playing_status == i_old_playing_status )
        return;

    GetToolBar()->DeleteTool( PlayStream_Event );

    if( i_playing_status == PLAYING_S )
    {
        GetToolBar()->InsertTool( 8, PlayStream_Event, wxU(_("Pause")),
                                  wxBitmap( pause_xpm ), wxNullBitmap,
                                  wxITEM_NORMAL, wxU(_(HELP_PAUSE)) );
    }
    else
    {
        GetToolBar()->InsertTool( 8, PlayStream_Event, wxU(_("Play")),
                                  wxBitmap( play_xpm ), wxNullBitmap,
                                  wxITEM_NORMAL, wxU(_(HELP_PLAY)) );
    }

    GetToolBar()->Realize();

    i_old_playing_status = i_playing_status;
}

#if !defined(__WXX11__)
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
        playlist_Add( p_playlist, (const char *)filenames[i].mb_str(), 0, 0,
                      PLAYLIST_APPEND | ((i | b_enqueue) ? 0 : PLAYLIST_GO),
                      PLAYLIST_END );

    vlc_object_release( p_playlist );

    return TRUE;
}
#endif

/*****************************************************************************
 * Definition of wxVolCtrl class.
 *****************************************************************************/
wxVolCtrl::wxVolCtrl( intf_thread_t *_p_intf, wxWindow* parent, wxWindowID id )
  : wxGauge( parent, id, 200, wxDefaultPosition, wxSize( 20, -1 ),
             wxGA_VERTICAL | wxGA_SMOOTH )
{
    p_intf = _p_intf;
}

void wxVolCtrl::OnChange( wxMouseEvent& event )
{
    if( !event.LeftDown() && !event.LeftIsDown() ) return;

    int i_volume = (GetClientSize().GetHeight() - event.GetY()) * 200 /
                    GetClientSize().GetHeight();
    Change( i_volume );
}

void wxVolCtrl::Change( int i_volume )
{
    aout_VolumeSet( p_intf, i_volume * AOUT_VOLUME_MAX / 200 / 2 );
    SetValue( i_volume );
    SetToolTip( wxString::Format((wxString)wxU(_("Volume")) + wxT(" %d"),
                i_volume ) );
}
