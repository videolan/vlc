/*****************************************************************************
 * interface.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004, 2003 VideoLAN
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
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/vout.h>
#include <vlc/intf.h>

#include "wxwindows.h"

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
#include "bitmaps/speaker.xpm"

#define TOOLBAR_BMP_WIDTH 16
#define TOOLBAR_BMP_HEIGHT 16

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
    wxVolCtrl( intf_thread_t *_p_intf, wxWindow* parent, wxWindowID id,
               wxPoint = wxDefaultPosition, wxSize = wxSize( 20, -1 ) );
    virtual ~wxVolCtrl() {};

    void UpdateVolume();

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
    OpenDisc_Event,
    OpenNet_Event,
    OpenCapture_Event,
    OpenSat_Event,
    OpenOther_Event,
    EjectDisc_Event,

    StreamWizard_Event,
    Wizard_Event,

    Playlist_Event,
    Logs_Event,
    FileInfo_Event,

    Prefs_Event,
    Extended_Event,
    Bookmarks_Event,
    Skins_Event,

    SliderScroll_Event,
    StopStream_Event,
    PlayStream_Event,
    PrevStream_Event,
    NextStream_Event,
    SlowStream_Event,
    FastStream_Event,

    Adjust_Event,
    RestoreDefaults_Event,
    Hue_Event,
    Contrast_Event,
    Brightness_Event,
    Saturation_Event,
    Gamma_Event,

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

    EVT_MENU( Extended_Event, Interface::OnExtended)
    EVT_MENU( Bookmarks_Event, Interface::OnShowDialog)

    EVT_CHECKBOX( Adjust_Event, Interface::OnEnableAdjust)
    EVT_BUTTON( RestoreDefaults_Event, Interface::OnRestoreDefaults)
    EVT_TEXT( Ratio_Event, Interface::OnRatio)
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
    EVT_MENU(OpenCapture_Event, Interface::OnShowDialog)
    EVT_MENU(OpenSat_Event, Interface::OnShowDialog)
    EVT_MENU(StreamWizard_Event, Interface::OnShowDialog)
    EVT_MENU(Wizard_Event, Interface::OnShowDialog)
    EVT_MENU(StopStream_Event, Interface::OnStopStream)
    EVT_MENU(PlayStream_Event, Interface::OnPlayStream)
    EVT_MENU(PrevStream_Event, Interface::OnPrevStream)
    EVT_MENU(NextStream_Event, Interface::OnNextStream)
    EVT_MENU(SlowStream_Event, Interface::OnSlowStream)
    EVT_MENU(FastStream_Event, Interface::OnFastStream)

    /* Slider events */
    EVT_COMMAND_SCROLL(SliderScroll_Event, Interface::OnSliderUpdate)
    
    EVT_COMMAND_SCROLL(Hue_Event, Interface::OnAdjustUpdate)
    EVT_COMMAND_SCROLL(Contrast_Event, Interface::OnAdjustUpdate)
    EVT_COMMAND_SCROLL(Brightness_Event, Interface::OnAdjustUpdate)
    EVT_COMMAND_SCROLL(Saturation_Event, Interface::OnAdjustUpdate)
    EVT_COMMAND_SCROLL(Gamma_Event, Interface::OnAdjustUpdate)

    /* Custom events */
    EVT_COMMAND(0, wxEVT_INTF, Interface::OnControlEvent)
    EVT_COMMAND(1, wxEVT_INTF, Interface::OnControlEvent)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Interface::Interface( intf_thread_t *_p_intf ):
    wxFrame( NULL, -1, wxT("VLC media player"),
             wxDefaultPosition, wxSize(700,100), wxDEFAULT_FRAME_STYLE )
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

    /* Create the extra panel */
    CreateOurExtendedPanel();
    frame_sizer->Add( extra_frame, 0, wxEXPAND , 0 );
    frame_sizer->Hide( extra_frame );

    /* Creation of the status bar
     * Helptext for menu items and toolbar tools will automatically get
     * displayed here. */
    int i_status_width[3] = {-6, -2, -9};
    statusbar = CreateStatusBar( 3 );                            /* 2 fields */
    statusbar->SetStatusWidths( 3, i_status_width );
    statusbar->SetStatusText( wxString::Format(wxT("x%.2f"), 1.0), 1 );

    /* Video window */
    if( config_GetInt( p_intf, "wxwin-embed" ) )
    {
        VideoWindow( p_intf, this );
        frame_sizer->Add( p_intf->p_sys->p_video_sizer, 1, wxEXPAND , 0 );
    }

    /* Creation of the slider sub-window */
    CreateOurSlider();
    frame_sizer->Add( slider_frame, 0, wxEXPAND , 0 );
    frame_sizer->Hide( slider_frame );

    /* Make sure we've got the right background colour */
    SetBackgroundColour( slider_frame->GetBackgroundColour() );

    /* Layout everything */
    frame_sizer->Layout();
    frame_sizer->Fit(this);

#if wxUSE_DRAG_AND_DROP
    /* Associate drop targets with the main interface */
    SetDropTarget( new DragAndDrop( p_intf ) );
#endif

    SetupHotkeys();

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

void Interface::Update()
{
    /* Misc updates */
    ((wxVolCtrl *)volctrl)->UpdateVolume();
}

void Interface::OnControlEvent( wxCommandEvent& event )
{
    switch( event.GetId() )
    {
    case 0:
        frame_sizer->Layout();
        frame_sizer->Fit(this);
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
    /* Create the "File" menu */
    wxMenu *file_menu = new wxMenu;
    file_menu->Append( OpenFileSimple_Event,
                       wxU(_("Quick &Open File...\tCtrl-O")) );

    file_menu->AppendSeparator();
    file_menu->Append( OpenFile_Event, wxU(_("Open &File...\tCtrl-F")) );
    file_menu->Append( OpenDisc_Event, wxU(_("Open &Disc...\tCtrl-D")) );
    file_menu->Append( OpenNet_Event,
                       wxU(_("Open &Network Stream...\tCtrl-N")) );
    file_menu->Append( OpenCapture_Event,
                       wxU(_("Open &Capture Device...\tCtrl-C")) );

    file_menu->AppendSeparator();
    file_menu->Append( Wizard_Event, wxU(_("&Wizard...\tCtrl-W")) );
    file_menu->Append( StreamWizard_Event,
                       wxU(_("Old streaming Wizard...")) );
    file_menu->AppendSeparator();
    file_menu->Append( Exit_Event, wxU(_("E&xit\tCtrl-X")) );

    /* Create the "View" menu */
    wxMenu *view_menu = new wxMenu;
    view_menu->Append( Playlist_Event, wxU(_("&Playlist...\tCtrl-P")) );
    view_menu->Append( Logs_Event, wxU(_("&Messages...\tCtrl-M")) );
    view_menu->Append( FileInfo_Event,
                       wxU(_("Stream and Media &info...\tCtrl-I")) );

    /* Create the "Auto-generated" menus */
    p_settings_menu = SettingsMenu( p_intf, this );
    p_audio_menu = AudioMenu( p_intf, this );
    p_video_menu = VideoMenu( p_intf, this );
    p_navig_menu = NavigMenu( p_intf, this );

    /* Create the "Help" menu */
    wxMenu *help_menu = new wxMenu;
    help_menu->Append( About_Event, wxU(_("About VLC media player")) );

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
            4 /* approximate margin */;
#endif
    }
    frame_sizer->SetMinSize( i_size, -1 );

    /* Intercept all menu events in our custom event handler */
    PushEventHandler( new MenuEvtHandler( p_intf, this ) );

#if wxUSE_DRAG_AND_DROP
    /* Associate drop targets with the menubar */
    menubar->SetDropTarget( new DragAndDrop( p_intf ) );
#endif
}

class VLCVolCtrl : public wxControl
{
public:
    VLCVolCtrl( intf_thread_t *p_intf, wxWindow *p_parent, wxGauge ** );
    virtual ~VLCVolCtrl() {};

    virtual void OnPaint( wxPaintEvent &event );

  private:
    DECLARE_EVENT_TABLE()
    int i_y_offset;
};

BEGIN_EVENT_TABLE(VLCVolCtrl, wxControl)
   EVT_PAINT(VLCVolCtrl::OnPaint)
END_EVENT_TABLE()

#if defined(__WXGTK__)
#define VLCVOL_HEIGHT p_parent->GetSize().GetHeight()
#else
#define VLCVOL_HEIGHT TOOLBAR_BMP_HEIGHT
#endif
VLCVolCtrl::VLCVolCtrl( intf_thread_t *p_intf, wxWindow *p_parent,
                        wxGauge **pp_volctrl )
  :wxControl( p_parent, -1, wxDefaultPosition, wxSize(64, VLCVOL_HEIGHT ),
              wxBORDER_NONE ),
   i_y_offset((VLCVOL_HEIGHT - TOOLBAR_BMP_HEIGHT) / 2)
{
    *pp_volctrl = new wxVolCtrl( p_intf, this, -1, wxPoint( 18, i_y_offset ),
                                 wxSize( 44, TOOLBAR_BMP_HEIGHT ) );
}

void VLCVolCtrl::OnPaint( wxPaintEvent &evt )
{
    wxPaintDC dc( this );
    wxBitmap mPlayBitmap( speaker_xpm );
    dc.DrawBitmap( mPlayBitmap, 0, i_y_offset, TRUE );
}

void Interface::CreateOurToolBar()
{
#define HELP_OPEN N_("Open")
#define HELP_STOP N_("Stop")
#define HELP_PLAY N_("Play")
#define HELP_PAUSE N_("Pause")
#define HELP_PLO N_("Playlist")
#define HELP_PLP N_("Previous playlist item")
#define HELP_PLN N_("Next playlist item")
#define HELP_SLOW N_("Play slower")
#define HELP_FAST N_("Play faster")

    wxLogNull LogDummy; /* Hack to suppress annoying log message on the win32
                         * version because we don't include wx.rc */

    wxToolBar *toolbar =
        CreateToolBar( wxTB_HORIZONTAL | wxTB_FLAT | wxTB_DOCKABLE );

    toolbar->SetToolBitmapSize( wxSize(TOOLBAR_BMP_WIDTH,TOOLBAR_BMP_HEIGHT) );

    toolbar->AddTool( OpenFile_Event, wxT(""),
                      wxBitmap( eject_xpm ), wxU(_(HELP_OPEN)) );
    toolbar->AddSeparator();
    toolbar->AddTool( PlayStream_Event, wxT(""), wxBitmap( play_xpm ),
                      wxU(_(HELP_PLAY)) );
#if 0
    toolbar->AddTool( PlayStream_Event, wxT(""), wxBitmap( pause_xpm ),
                      wxU(_(HELP_PAUSE)) );
#endif
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

    wxControl *p_dummy_ctrl =
        new wxControl( toolbar, -1, wxDefaultPosition,
                       wxSize(35, 16 ), wxBORDER_NONE );

    toolbar->AddControl( p_dummy_ctrl );

    VLCVolCtrl *sound_control = new VLCVolCtrl( p_intf, toolbar, &volctrl );
    toolbar->AddControl( sound_control );

    toolbar->Realize();

#if wxUSE_DRAG_AND_DROP
    /* Associate drop targets with the toolbar */
    toolbar->SetDropTarget( new DragAndDrop( p_intf ) );
#endif
}

void Interface::CreateOurSlider()
{
    /* Create a new frame and sizer containing the slider */
    slider_frame = new wxPanel( this, -1, wxDefaultPosition, wxDefaultSize );
    slider_frame->SetAutoLayout( TRUE );
    wxBoxSizer *frame_sizer = new wxBoxSizer( wxHORIZONTAL );
    //frame_sizer->SetMinSize( -1, 50 );

    /* Create slider */
    slider = new wxSlider( slider_frame, SliderScroll_Event, 0, 0,
                           SLIDER_MAX_POS, wxDefaultPosition, wxDefaultSize );

    /* Add everything to the frame */
    frame_sizer->Add( slider, 1, wxEXPAND | wxALL, 5 );
    slider_frame->SetSizer( frame_sizer );
    frame_sizer->Layout();
    frame_sizer->SetSizeHints(slider_frame);

    /* Hide the slider by default */
    slider_frame->Hide();
}


void Interface::CreateOurExtendedPanel()
{
    char *psz_filters;

    extra_frame = new wxPanel( this, -1, wxDefaultPosition, wxDefaultSize );
    extra_frame->SetAutoLayout( TRUE );
    wxBoxSizer *extra_sizer = new wxBoxSizer( wxHORIZONTAL );

    /* Create static box to surround the adjust controls */
    wxStaticBox *adjust_box =
           new wxStaticBox( extra_frame, -1, wxU(_("Adjust Image")) );

    /* Create the size for the frame */
    wxStaticBoxSizer *adjust_sizer =
        new wxStaticBoxSizer( adjust_box, wxVERTICAL );
    adjust_sizer->SetMinSize( -1, 50 );
    
    /* Create flex grid */
    wxFlexGridSizer *adjust_gridsizer =
        new wxFlexGridSizer( 6, 2, 0, 0);
    adjust_gridsizer->AddGrowableCol(1);

    /* Create every controls */

    /* Create the adjust button */
    wxCheckBox * adjust_check = new wxCheckBox( extra_frame, Adjust_Event,
                                                 wxU(_("Enable")));

    /* Create the restore to defaults button */
    restoredefaults_button = 
        new wxButton( extra_frame, RestoreDefaults_Event,
        wxU(_("Restore Defaults")), wxDefaultPosition);

    wxStaticText *hue_text = new wxStaticText( extra_frame, -1,
                                       wxU(_("Hue")) );
    hue_slider = new wxSlider ( extra_frame, Hue_Event, 0, 0,
                                360, wxDefaultPosition, wxDefaultSize );


    wxStaticText *contrast_text = new wxStaticText( extra_frame, -1,
                                       wxU(_("Contrast")) );
    contrast_slider = new wxSlider ( extra_frame, Contrast_Event, 0, 0,
                                200, wxDefaultPosition, wxDefaultSize);

    wxStaticText *brightness_text = new wxStaticText( extra_frame, -1,
                                       wxU(_("Brightness")) );
    brightness_slider = new wxSlider ( extra_frame, Brightness_Event, 0, 0,
                           200, wxDefaultPosition, wxDefaultSize) ;

    wxStaticText *saturation_text = new wxStaticText( extra_frame, -1,
                                          wxU(_("Saturation")) );
    saturation_slider = new wxSlider ( extra_frame, Saturation_Event, 0, 0,
                           300, wxDefaultPosition, wxDefaultSize );

    wxStaticText *gamma_text = new wxStaticText( extra_frame, -1,
                                          wxU(_("Gamma")) );
    gamma_slider = new wxSlider ( extra_frame, Gamma_Event, 0, 0,
                           100, wxDefaultPosition, wxDefaultSize );

    adjust_gridsizer->Add(adjust_check, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(restoredefaults_button, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(hue_text, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(hue_slider, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(contrast_text, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(contrast_slider, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(brightness_text, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(brightness_slider, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(saturation_text, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(saturation_slider, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(gamma_text, 1, wxEXPAND, 0);
    adjust_gridsizer->Add(gamma_slider, 1, wxEXPAND, 0);
    
    adjust_sizer->Add(adjust_gridsizer,1,wxEXPAND, 0);
    
    extra_sizer->Add(adjust_sizer,1,wxBOTTOM,5);

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
                                          wxU(_("Aspect Ratio")) );

    ratio_combo = new wxComboBox( extra_frame, Ratio_Event, wxT(""),
                                  wxDefaultPosition, wxSize(120,-1),
                                  WXSIZEOF(ratio_array), ratio_array,
                                  0 );

    ratio_sizer->Add( ratio_text, 0, wxALL, 2 );
    ratio_sizer->Add( ratio_combo, 0, wxALL, 2 );
    ratio_sizer->Layout();

    video_sizer->Add( ratio_sizer  , 0 , wxALL , 0 );
    video_sizer->Layout();

#if 0
    wxBoxSizer *visual_sizer = new wxBoxSizer( wxHORIZONTAL );

    wxCheckBox *visual_checkbox = new wxCheckBox( extra_frame, Visual_Event,
                                            wxU(_("Visualizations")) );

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

    other_sizer->Add( audio_sizer , 0 , wxALL | wxEXPAND , 0 );
#endif
    other_sizer->Add( video_sizer, 0, wxALL | wxEXPAND , 0);
    other_sizer->Layout();

    extra_sizer->Add(other_sizer,0,wxBOTTOM,5);

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
        restoredefaults_button->Enable();
        saturation_slider->Enable();
        contrast_slider->Enable();
        brightness_slider->Enable();
        hue_slider->Enable();
        gamma_slider->Enable();
    }
    else
    {
        adjust_check->SetValue( 0 );
        restoredefaults_button->Disable();
        saturation_slider->Disable();
        contrast_slider->Disable();
        brightness_slider->Disable();
        hue_slider->Disable();
        gamma_slider->Disable();
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
    f_value = config_GetFloat( p_intf, "gamma" );
    if( f_value > 0 && f_value < 10 )
        gamma_slider->SetValue( (int)(10 * f_value) );

    extra_frame->Hide();
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
        case KEY_END: return WXK_HOME;
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

    wxAcceleratorEntry p_entries[i_hotkeys];

    /* Setup the hotkeys as accelerators */
    for( int i = 0; i < i_hotkeys; i++ )
    {
        p_entries[i].Set( ConvertHotkeyModifiers( p_hotkeys[i].i_key ),
                          ConvertHotkey( p_hotkeys[i].i_key ),
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
        msg_Dbg( p_intf, "accelerator table loaded" );
    }
}

/*****************************************************************************
 * Event Handlers.
 *****************************************************************************/

void Interface::OnMenuOpen(wxMenuEvent& event)
{
#if defined( __WXMSW__ )
#   define GetEventObject GetMenu
#endif

    if( event.GetEventObject() == p_settings_menu )
    {
        p_settings_menu = SettingsMenu( p_intf, this, p_settings_menu );

        /* Add static items */
        p_settings_menu->AppendCheckItem( Extended_Event,
            wxU(_("&Extended GUI") ) );
        if( b_extra ) p_settings_menu->Check( Extended_Event, TRUE );
        p_settings_menu->AppendCheckItem( Bookmarks_Event,
            wxU(_("&Bookmarks...") ) );
        p_settings_menu->Append( Prefs_Event, wxU(_("&Preferences...")) );
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
    msg.Printf( wxString(wxT("VLC media player " VERSION)) +
        wxU(_(" (wxWindows interface)\n\n")) +
        wxU(_("(c) 1996-2004 - the VideoLAN Team\n\n")) +
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
        case StreamWizard_Event:
            i_id = INTF_DIALOG_STREAMWIZARD;
            break;
        case Wizard_Event:
            i_id = INTF_DIALOG_WIZARD;
            break;
        case Bookmarks_Event:
            i_id = INTF_DIALOG_BOOKMARKS;
            break;
        default:
            i_id = INTF_DIALOG_FILE;
            break;
        }

        p_intf->p_sys->pf_show_dialog( p_intf, i_id, 1, 0 );
    }
}

void Interface::OnExtended(wxCommandEvent& event)
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
        restoredefaults_button->Enable();
        brightness_slider->Enable();
        saturation_slider->Enable();
        contrast_slider->Enable();
        hue_slider->Enable();
        gamma_slider->Enable();
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
        restoredefaults_button->Disable();
        brightness_slider->Disable();
        saturation_slider->Disable();
        contrast_slider->Disable();
        hue_slider->Disable();
        gamma_slider->Disable();
    }
    if(psz_filters) free(psz_filters);
    if(psz_new) free(psz_new);
}

void Interface::OnRestoreDefaults( wxCommandEvent &event)
{
    hue_slider->SetValue(0);
    saturation_slider->SetValue(100);
    brightness_slider->SetValue(100);
    contrast_slider->SetValue(100),
    gamma_slider->SetValue(10);

    wxScrollEvent *hscroll_event = new wxScrollEvent(0, Hue_Event, 0);
    OnAdjustUpdate(*hscroll_event);
    
    wxScrollEvent *sscroll_event = new wxScrollEvent(0, Saturation_Event, 100);
    OnAdjustUpdate(*sscroll_event);
    
    wxScrollEvent *bscroll_event = new wxScrollEvent(0, Brightness_Event, 100);
    OnAdjustUpdate(*bscroll_event);
    
    wxScrollEvent *cscroll_event = new wxScrollEvent(0, Contrast_Event, 100);
    OnAdjustUpdate(*cscroll_event);
    
    wxScrollEvent *gscroll_event = new wxScrollEvent(0, Gamma_Event, 10);
    OnAdjustUpdate(*gscroll_event);

}

void Interface::OnAdjustUpdate( wxScrollEvent &event)
{
    vout_thread_t *p_vout = (vout_thread_t *)vlc_object_find(p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE);
    if(p_vout == NULL)
        switch(event.GetId())
        {
            case Hue_Event: 
                config_PutInt( p_intf , "hue" , event.GetPosition() );
                break;
            
            case Saturation_Event: 
                config_PutFloat( p_intf , "saturation" , (float)event.GetPosition()/100 );
                break;
            
            case Brightness_Event: 
                config_PutFloat( p_intf , "brightness" , (float)event.GetPosition()/100 );
                break;
            
            case Contrast_Event: 
                config_PutFloat( p_intf , "contrast" , (float)event.GetPosition()/100 );
                break;
            
            case Gamma_Event: 
                config_PutFloat( p_intf , "gamma" , (float)event.GetPosition()/10 );
                break;
        }
    else
    {
        vlc_value_t val;
        switch(event.GetId())
        {
            case Hue_Event: 
                val.i_int = event.GetPosition();
                var_Set(p_vout, "hue", val);
                break;
            
            case Saturation_Event:
                val.f_float = (float)event.GetPosition()/100;
                var_Set(p_vout, "saturation", val);
                break;
            
            case Brightness_Event: 
                val.f_float = (float)event.GetPosition()/100;
                var_Set(p_vout, "brightness", val);
                break;
            
            case Contrast_Event: 
                val.f_float = (float)event.GetPosition()/100;
                var_Set(p_vout, "contrast", val);
                break;
            
            case Gamma_Event: 
                val.f_float = (float)event.GetPosition()/10;
                var_Set(p_vout, "gamma", val);
                break;
        }
        vlc_object_release(p_vout);
    }
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
            char psz_time[ MSTRTIME_MAX_SIZE ], psz_total[ MSTRTIME_MAX_SIZE ];
            mtime_t i_seconds;
            vlc_value_t val;

            i_seconds = var_GetTime( p_intf->p_sys->p_input, "length" ) / I64C(1000000 );
            secstotimestr( psz_total, i_seconds );

            i_seconds = var_GetTime( p_intf->p_sys->p_input, "time" ) / I64C(1000000 );
            secstotimestr( psz_time, i_seconds );

            statusbar->SetStatusText( wxU(psz_time)+ wxString(wxT(" / ")) + wxU(psz_total), 0 );
        }
    }
#endif

#undef WIN32
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

    /* FIXME --fenrir */
#if 0
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
#endif

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

    /* FIXME --fenrir */
#if 0
    var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
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
#endif
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
        GetToolBar()->InsertTool( 2, PlayStream_Event, wxU(_("Pause")),
                                  wxBitmap( pause_xpm ), wxNullBitmap,
                                  wxITEM_NORMAL, wxU(_(HELP_PAUSE)) );
    }
    else
    {
        GetToolBar()->InsertTool( 2, PlayStream_Event, wxU(_("Play")),
                                  wxBitmap( play_xpm ), wxNullBitmap,
                                  wxITEM_NORMAL, wxU(_(HELP_PLAY)) );
    }

    GetToolBar()->Realize();

    i_old_playing_status = i_playing_status;
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
        playlist_Add( p_playlist, (const char *)filenames[i].mb_str(),
                      (const char *)filenames[i].mb_str(),
                      PLAYLIST_APPEND | ((i | b_enqueue) ? 0 : PLAYLIST_GO),
                      PLAYLIST_END );

    vlc_object_release( p_playlist );

    return TRUE;
}
#endif

/*****************************************************************************
 * Definition of VolCtrl class.
 *****************************************************************************/
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
