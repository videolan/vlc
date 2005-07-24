/*****************************************************************************
 * wxwindows.h: private wxWindows interface description
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
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

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO

#include <list>

#include <wx/wxprec.h>
#include <wx/wx.h>

#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/dnd.h>
#include <wx/treectrl.h>
#include <wx/gauge.h>
#include <wx/accel.h>
#include <wx/checkbox.h>
#include <wx/wizard.h>
#include <wx/taskbar.h>
#include "vlc_keys.h"

#if (!wxCHECK_VERSION(2,5,0))
typedef long wxTreeItemIdValue;
#endif

DECLARE_LOCAL_EVENT_TYPE( wxEVT_DIALOG, 0 );
DECLARE_LOCAL_EVENT_TYPE( wxEVT_INTF, 1 );

#define SLIDER_MAX_POS 10000

/* wxU is used to convert ansi/utf8 strings to unicode strings (wchar_t) */
#if defined( ENABLE_NLS ) && defined( ENABLE_UTF8 )
#if wxUSE_UNICODE
#   define wxU(utf8) wxString(utf8, wxConvUTF8)
#else
#   define wxU(utf8) wxString(wxConvUTF8.cMB2WC(utf8), *wxConvCurrent)
#endif
#define ISUTF8 1

#else // ENABLE_NLS && ENABLE_UTF8
#if wxUSE_UNICODE
#   define wxU(ansi) wxString(ansi, wxConvLocal)
#else
#   define wxU(ansi) (ansi)
#endif
#define ISUTF8 0

#endif

/* wxL2U (locale to unicode) is used to convert ansi strings to unicode
 * strings (wchar_t) */
#define wxL2U(ansi) wxU(ansi)

#define WRAPCOUNT 80

#define OPEN_NORMAL 0
#define OPEN_STREAM 1

#define MODE_NONE 0
#define MODE_GROUP 1
#define MODE_AUTHOR 2
#define MODE_TITLE 3

enum{
  ID_CONTROLS_TIMER,
  ID_SLIDER_TIMER,
};

class DialogsProvider;
class PrefsTreeCtrl;
class AutoBuiltPanel;
class VideoWindow;
class WindowSettings;

/*****************************************************************************
 * intf_sys_t: description and status of wxwindows interface
 *****************************************************************************/
struct intf_sys_t
{
    /* the wx parent window */
    wxWindow            *p_wxwindow;
    wxIcon              *p_icon;

    /* window settings */
    WindowSettings      *p_window_settings;

    /* special actions */
    vlc_bool_t          b_playing;
    vlc_bool_t          b_intf_show;                /* interface to be shown */

    /* The input thread */
    input_thread_t *    p_input;

    /* The slider */
    int                 i_slider_pos;                     /* slider position */
    int                 i_slider_oldpos;                /* previous position */
    vlc_bool_t          b_slider_free;                      /* slider status */

    /* The messages window */
    msg_subscription_t* p_sub;                  /* message bank subscription */

    /* Playlist management */
    int                 i_playing;                 /* playlist selected item */
    unsigned            i_playlist_usage;

    /* Send an event to show a dialog */
    void (*pf_show_dialog) ( intf_thread_t *p_intf, int i_dialog, int i_arg,
                             intf_dialog_args_t *p_arg );

    /* Popup menu */
    wxMenu              *p_popup_menu;

    /* Hotkeys */
    int                 i_first_hotkey_event;
    int                 i_hotkeys;

    /* Embedded vout */
    VideoWindow         *p_video_window;
    wxBoxSizer          *p_video_sizer;
    vlc_bool_t          b_video_autosize;

    /* Aout */
    aout_instance_t     *p_aout;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
wxArrayString SeparateEntries( wxString );
wxWindow *CreateVideoWindow( intf_thread_t *p_intf, wxWindow *p_parent );
void UpdateVideoWindow( intf_thread_t *p_intf, wxWindow *p_window );
wxWindow *BookmarksDialog( intf_thread_t *p_intf, wxWindow *p_parent );
wxWindow *CreateDialogsProvider( intf_thread_t *p_intf, wxWindow *p_parent );

namespace wxvlc
{
class Interface;
class OpenDialog;
class SoutDialog;
class SubsFileDialog;
class Playlist;
class Messages;
class FileInfo;
class StreamDialog;
class WizardDialog;
class ItemInfoDialog;
class NewGroup;
class ExportPlaylist;

/*****************************************************************************
 * Classes declarations.
 *****************************************************************************/
/* Timer */
class Timer: public wxTimer
{
public:
    /* Constructor */
    Timer( intf_thread_t *p_intf, Interface *p_main_interface );
    virtual ~Timer();

    virtual void Notify();

private:
    //use wxWindow::IsShown instead
    //vlc_bool_t b_slider_shown;
    //vlc_bool_t b_disc_shown;
    intf_thread_t *p_intf;
    Interface *p_main_interface;
    vlc_bool_t b_init;
    int i_old_playing_status;
    int i_old_rate;
};


/* Extended panel */
class ExtraPanel: public wxPanel
{
public:
    /* Constructor */
    ExtraPanel( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~ExtraPanel();

    wxStaticBox *adjust_box;
    wxButton *restoredefaults_button;
    wxSlider *brightness_slider;
    wxSlider *contrast_slider;
    wxSlider *saturation_slider;
    wxSlider *hue_slider;
    wxSlider *gamma_slider;

    wxStaticBox *other_box;
    wxComboBox *ratio_combo;

    char *psz_bands;
    float f_preamp;
    vlc_bool_t b_update;

private:

    wxPanel *VideoPanel( wxWindow * );
    wxPanel *EqzPanel( wxWindow * );
    wxPanel *AudioPanel( wxWindow * );

    wxNotebook *notebook;

    wxCheckBox *eq_chkbox;

    wxCheckBox *eq_2p_chkbox;

    wxButton *eq_restoredefaults_button;

    wxSlider *smooth_slider;
    wxStaticText *smooth_text;

    wxSlider *preamp_slider;
    wxStaticText * preamp_text;

    int i_smooth;
    wxWindow *p_parent;

    wxSlider *band_sliders[10];
    wxStaticText *band_texts[10];

    int i_values[10];

    void CheckAout();

    /* Event handlers (these functions should _not_ be virtual) */

    void OnEnableAdjust( wxCommandEvent& );
    void OnEnableEqualizer( wxCommandEvent& );
    void OnRestoreDefaults( wxCommandEvent& );
    void OnChangeEqualizer( wxScrollEvent& );
    void OnAdjustUpdate( wxScrollEvent& );
    void OnRatio( wxCommandEvent& );
    void OnFiltersInfo( wxCommandEvent& );
    void OnSelectFilter( wxCommandEvent& );

    void OnEqSmooth( wxScrollEvent& );
    void OnPreamp( wxScrollEvent& );
    void OnEq2Pass( wxCommandEvent& );
    void OnEqRestore( wxCommandEvent& );

    void OnHeadphone( wxCommandEvent& );
    void OnNormvol( wxCommandEvent& );
    void OnNormvolSlider( wxScrollEvent& );

    void OnIdle( wxIdleEvent& );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    vlc_bool_t b_my_update;
};

#if 0
/* Extended Window  */
class ExtraWindow: public wxFrame
{
public:
    /* Constructor */
    ExtraWindow( intf_thread_t *p_intf, wxWindow *p_parent, wxPanel *panel );
    virtual ~ExtraWindow();

private:

    wxPanel *panel;

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
};
#endif

/* Systray integration */
#ifdef wxHAS_TASK_BAR_ICON
class Systray: public wxTaskBarIcon
{
public:
    Systray( Interface* p_main_interface, intf_thread_t *p_intf );
    virtual ~Systray() {};
    wxMenu* CreatePopupMenu();
    void UpdateTooltip( const wxChar* tooltip );

private:
    void OnMenuIconize( wxCommandEvent& event );
    void OnLeftClick( wxTaskBarIconEvent& event );
    void OnPlayStream ( wxCommandEvent& event );
    void OnStopStream ( wxCommandEvent& event );
    void OnPrevStream ( wxCommandEvent& event );
    void OnNextStream ( wxCommandEvent& event );
    void OnExit(  wxCommandEvent& event );
    Interface* p_main_interface;
    intf_thread_t *p_intf;
    DECLARE_EVENT_TABLE()
};
#endif

/* Main Interface */
class Interface: public wxFrame
{
public:
    /* Constructor */
    Interface( intf_thread_t *p_intf, long style = wxDEFAULT_FRAME_STYLE );
    virtual ~Interface();
    void Init();
    void TogglePlayButton( int i_playing_status );
    void Update();
    void PlayStream();
    void StopStream();
    void PrevStream();
    void NextStream();

    wxBoxSizer  *frame_sizer;
    wxStatusBar *statusbar;

    void HideSlider(bool layout = true);
    void ShowSlider(bool show = true, bool layout = true);

    wxSlider    *slider;
    wxWindow    *slider_frame;
    wxBoxSizer  *slider_sizer;
    wxPanel     *extra_frame;

    void HideDiscFrame(bool layout = true);
    void ShowDiscFrame(bool show = true, bool layout = true);

    wxPanel         *disc_frame;
    wxBoxSizer      *disc_sizer;
    wxBitmapButton  *disc_menu_button;
    wxBitmapButton  *disc_prev_button;
    wxBitmapButton  *disc_next_button;

    wxFrame    *extra_window;

    vlc_bool_t b_extra;
    vlc_bool_t b_undock;

    wxControl  *volctrl;

#ifdef wxHAS_TASK_BAR_ICON
    Systray     *p_systray;
#endif

    wxTimer m_controls_timer;
    wxTimer m_slider_timer;

private:
    void SetupHotkeys();
    void CreateOurMenuBar();
    void CreateOurToolBar();
    void CreateOurExtendedPanel();
    void CreateOurSlider();
    void Open( int i_access_method );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnControlsTimer(wxTimerEvent& WXUNUSED(event));
    void OnSliderTimer(wxTimerEvent& WXUNUSED(event));

    void OnExit( wxCommandEvent& event );
    void OnAbout( wxCommandEvent& event );

    void OnOpenFileSimple( wxCommandEvent& event );
    void OnOpenDir( wxCommandEvent& event );
    void OnOpenFile( wxCommandEvent& event );
    void OnOpenDisc( wxCommandEvent& event );
    void OnOpenNet( wxCommandEvent& event );
    void OnOpenSat( wxCommandEvent& event );

    void OnExtended( wxCommandEvent& event );
    //void OnUndock( wxCommandEvent& event );

    void OnBookmarks( wxCommandEvent& event );
    void OnShowDialog( wxCommandEvent& event );
    void OnPlayStream( wxCommandEvent& event );
    void OnStopStream( wxCommandEvent& event );
    void OnSliderUpdate( wxScrollEvent& event );
    void OnPrevStream( wxCommandEvent& event );
    void OnNextStream( wxCommandEvent& event );
    void OnSlowStream( wxCommandEvent& event );
    void OnFastStream( wxCommandEvent& event );

    void OnDiscMenu( wxCommandEvent& event );
    void OnDiscPrev( wxCommandEvent& event );
    void OnDiscNext( wxCommandEvent& event );

    void OnMenuOpen( wxMenuEvent& event );

#if defined( __WXMSW__ ) || defined( __WXMAC__ )
    void OnContextMenu2(wxContextMenuEvent& event);
#endif
    void OnContextMenu(wxMouseEvent& event);

    void OnControlEvent( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    Timer *timer;
    intf_thread_t *p_intf;

    wxWindow *video_window;

    int i_old_playing_status;

    /* For auto-generated menus */
    wxMenu *p_settings_menu;
    wxMenu *p_audio_menu;
    wxMenu *p_video_menu;
    wxMenu *p_navig_menu;
};

/* Open Dialog */
WX_DEFINE_ARRAY(AutoBuiltPanel *, ArrayOfAutoBuiltPanel);
class OpenDialog: public wxDialog
{
public:
    /* Constructor */
    OpenDialog( intf_thread_t *p_intf, wxWindow *p_parent,
                int i_access_method, int i_arg = 0  );

    /* Extended Contructor */
    OpenDialog( intf_thread_t *p_intf, wxWindow *p_parent,
                int i_access_method, int i_arg = 0 , int _i_method = 0 );
    virtual ~OpenDialog();

    int Show();
    int Show( int i_access_method, int i_arg = 0 );

    void UpdateMRL();
    void UpdateMRL( int i_access_method );

    wxArrayString mrl;

private:
    wxPanel *FilePanel( wxWindow* parent );
    wxPanel *DiscPanel( wxWindow* parent );
    wxPanel *NetPanel( wxWindow* parent );

    ArrayOfAutoBuiltPanel input_tab_array;

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );
    void OnClose( wxCloseEvent& event );

    void OnPageChange( wxNotebookEvent& event );
    void OnMRLChange( wxCommandEvent& event );

    /* Event handlers for the file page */
    void OnFilePanelChange( wxCommandEvent& event );
    void OnFileBrowse( wxCommandEvent& event );

    /* Event handlers for the disc page */
    void OnDiscPanelChangeSpin( wxSpinEvent& event );
    void OnDiscPanelChange( wxCommandEvent& event );
    void OnDiscTypeChange( wxCommandEvent& event );
#ifdef HAVE_LIBCDIO
    void OnDiscProbe( wxCommandEvent& event );
#endif
    void OnDiscDeviceChange( wxCommandEvent& event );

    /* Event handlers for the net page */
    void OnNetPanelChangeSpin( wxSpinEvent& event );
    void OnNetPanelChange( wxCommandEvent& event );
    void OnNetTypeChange( wxCommandEvent& event );

    /* Event handlers for the stream output */
    void OnSubsFileEnable( wxCommandEvent& event );
    void OnSubsFileSettings( wxCommandEvent& WXUNUSED(event) );

    /* Event handlers for the stream output */
    void OnSoutEnable( wxCommandEvent& event );
    void OnSoutSettings( wxCommandEvent& WXUNUSED(event) );

    /* Event handlers for the caching option */
    void OnCachingEnable( wxCommandEvent& event );
    void OnCachingChange( wxCommandEvent& event );
    void OnCachingChangeSpin( wxSpinEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxWindow *p_parent;
    int i_current_access_method;
    int i_disc_type_selection;

    int i_method; /* Normal or for the stream dialog ? */
    int i_open_arg;

    wxComboBox *mrl_combo;
    wxNotebook *notebook;

    /* Controls for the file panel */
    wxComboBox *file_combo;
    wxFileDialog *file_dialog;

    /* Controls for the disc panel */
    wxRadioBox *disc_type;
    wxCheckBox *disc_probe;
    wxTextCtrl *disc_device;
    wxSpinCtrl *disc_title; int i_disc_title;
    wxSpinCtrl *disc_chapter; int i_disc_chapter;
    wxSpinCtrl *disc_sub; int i_disc_sub;
    wxSpinCtrl *disc_audio; int i_disc_audio;

    /* The media equivalent name for a DVD names. For example,
     * "Title", is "Track" for a CD-DA */
    wxStaticText *disc_title_label;
    wxStaticText *disc_chapter_label;
    wxStaticText *disc_sub_label;
    wxStaticText *disc_audio_label;

    /* Indicates if the disc device control was modified */
    bool b_disc_device_changed;

    /* Controls for the net panel */
    wxRadioBox *net_type;
    int i_net_type;
    wxPanel *net_subpanels[4];
    wxRadioButton *net_radios[4];
    wxSpinCtrl *net_ports[4];
    int        i_net_ports[4];
    wxTextCtrl *net_addrs[4];
    wxCheckBox *net_timeshift;
    wxCheckBox *net_ipv6;

    /* Controls for the subtitles file */
    wxButton *subsfile_button;
    wxCheckBox *subsfile_checkbox;
    SubsFileDialog *subsfile_dialog;
    wxArrayString subsfile_mrl;

    /* Controls for the stream output */
    wxButton *sout_button;
    wxCheckBox *sout_checkbox;
    SoutDialog *sout_dialog;
    wxArrayString sout_mrl;

    /* Controls for the caching options */
    wxCheckBox *caching_checkbox;
    wxSpinCtrl *caching_value;
    int i_caching;
};

enum
{
    FILE_ACCESS = 0,
    DISC_ACCESS,
    NET_ACCESS,

    /* Auto-built panels */
    CAPTURE_ACCESS
};
#define MAX_ACCESS CAPTURE_ACCESS

/* Stream output Dialog */
enum
{
    PLAY_ACCESS_OUT = 0,
    FILE_ACCESS_OUT,
    HTTP_ACCESS_OUT,
    MMSH_ACCESS_OUT,
    UDP_ACCESS_OUT,
    ACCESS_OUT_NUM
};

enum
{
    TS_ENCAPSULATION = 0,
    PS_ENCAPSULATION,
    MPEG1_ENCAPSULATION,
    OGG_ENCAPSULATION,
    ASF_ENCAPSULATION,
    MP4_ENCAPSULATION,
    MOV_ENCAPSULATION,
    WAV_ENCAPSULATION,
    RAW_ENCAPSULATION,
    AVI_ENCAPSULATION,
    ENCAPS_NUM
};

enum
{
    ANN_MISC_SOUT = 0,
    MISC_SOUT_NUM
};

class SoutDialog: public wxDialog
{
public:
    /* Constructor */
    SoutDialog( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~SoutDialog();

    wxArrayString GetOptions();

private:
    void UpdateMRL();
    wxPanel *AccessPanel( wxWindow* parent );
    wxPanel *MiscPanel( wxWindow* parent );
    wxPanel *EncapsulationPanel( wxWindow* parent );
    wxPanel *TranscodingPanel( wxWindow* parent );
    void    ParseMRL();

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );
    void OnMRLChange( wxCommandEvent& event );
    void OnAccessTypeChange( wxCommandEvent& event );

    /* Event handlers for the file access output */
    void OnFileChange( wxCommandEvent& event );
    void OnFileBrowse( wxCommandEvent& event );
    void OnFileDump( wxCommandEvent& event );

    /* Event handlers for the net access output */
    void OnNetChange( wxCommandEvent& event );

    /* Event specific to the announce address */
    void OnAnnounceGroupChange( wxCommandEvent& event );
    void OnAnnounceAddrChange( wxCommandEvent& event );

    /* Event handlers for the encapsulation panel */
    void OnEncapsulationChange( wxCommandEvent& event );

    /* Event handlers for the transcoding panel */
    void OnTranscodingEnable( wxCommandEvent& event );
    void OnTranscodingChange( wxCommandEvent& event );

    /* Event handlers for the misc panel */
    void OnSAPMiscChange( wxCommandEvent& event );
    void OnSLPMiscChange( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxWindow *p_parent;

    wxComboBox *mrl_combo;

    /* Controls for the access outputs */
    wxPanel *access_panel;
    wxPanel *access_subpanels[ACCESS_OUT_NUM];
    wxCheckBox *access_checkboxes[ACCESS_OUT_NUM];

    int i_access_type;

    wxComboBox *file_combo;
    wxCheckBox *dump_checkbox;
    wxSpinCtrl *net_ports[ACCESS_OUT_NUM];
    wxTextCtrl *net_addrs[ACCESS_OUT_NUM];

    /* Controls for the SAP announces */
    wxPanel *misc_panel;
    wxPanel *misc_subpanels[MISC_SOUT_NUM];
    wxCheckBox *sap_checkbox;
    wxCheckBox *slp_checkbox;
    wxTextCtrl *announce_group;
    wxTextCtrl *announce_addr;

    /* Controls for the encapsulation */
    wxPanel *encapsulation_panel;
    wxRadioButton *encapsulation_radios[ENCAPS_NUM];
    int i_encapsulation_type;

    /* Controls for transcoding */
    wxPanel *transcoding_panel;
    wxCheckBox *video_transc_checkbox;
    wxComboBox *video_codec_combo;
    wxComboBox *audio_codec_combo;
    wxCheckBox *audio_transc_checkbox;
    wxComboBox *video_bitrate_combo;
    wxComboBox *audio_bitrate_combo;
    wxComboBox *audio_channels_combo;
    wxComboBox *video_scale_combo;
    wxComboBox *subtitles_codec_combo;
    wxCheckBox *subtitles_transc_checkbox;
    wxCheckBox *subtitles_overlay_checkbox;

    /* Misc controls */
    wxCheckBox *sout_all_checkbox;
};

/* Subtitles File Dialog */
class SubsFileDialog: public wxDialog
{
public:
    /* Constructor */
    SubsFileDialog( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~SubsFileDialog();

    wxComboBox *file_combo;
    wxComboBox *encoding_combo;
    wxComboBox *size_combo;
    wxComboBox *align_combo;
    wxSpinCtrl *fps_spinctrl;
    wxSpinCtrl *delay_spinctrl;

private:
    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );
    void OnFileBrowse( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxWindow *p_parent;
};

/* Stream */
class StreamDialog: public wxFrame
{
public:
    /* Constructor */
    StreamDialog( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~StreamDialog();

private:
    void OnClose( wxCommandEvent& event );
    void OnOpen( wxCommandEvent& event );
    void OnSout( wxCommandEvent& event );
    void OnStart( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;

    wxStaticText *step2_label;
    wxStaticText *step3_label;
    wxButton *sout_button;
    wxButton *start_button;
    wxArrayString mrl;
    wxArrayString sout_mrl;
    OpenDialog *p_open_dialog;
    SoutDialog *p_sout_dialog;
};

/* Wizard */
class WizardDialog : public wxWizard
{
public:
    /* Constructor */
    WizardDialog( intf_thread_t *, wxWindow *p_parent, char *, int, int );
    virtual ~WizardDialog();
    void SetTranscode( char const *vcodec, int vb, char const *acodec, int ab);
    void SetMrl( const char *mrl );
    void SetTTL( int i_ttl );
    void SetPartial( int, int );
    void SetStream( char const *method, char const *address );
    void SetTranscodeOut( char const *address );
    void SetAction( int i_action );
    int  GetAction();
    void SetSAP( bool b_enabled, const char *psz_name );
    void SetMux( char const *mux );
    void Run();
    int i_action;
    char *method;

protected:
    int vb,ab;
    int i_from, i_to, i_ttl;
    char *vcodec , *acodec , *address , *mrl , *mux ;
    char *psz_sap_name;
    bool b_sap;
    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
};


/* Preferences Dialog */
class PrefsDialog: public wxFrame
{
public:
    /* Constructor */
    PrefsDialog( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~PrefsDialog();

private:
    wxPanel *PrefsPanel( wxWindow* parent );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );
    void OnSave( wxCommandEvent& event );
    void OnResetAll( wxCommandEvent& event );
    void OnAdvanced( wxCommandEvent& event );
    void OnClose( wxCloseEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;

    PrefsTreeCtrl *prefs_tree;
};

/* Messages */
class Messages: public wxFrame
{
public:
    /* Constructor */
    Messages( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~Messages();
    bool Show( bool show = TRUE );
    void UpdateLog();

private:
    /* Event handlers (these functions should _not_ be virtual) */
    void OnButtonClose( wxCommandEvent& event );
    void OnClose( wxCloseEvent& WXUNUSED(event) );
    void OnClear( wxCommandEvent& event );
    void OnSaveLog( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxTextCtrl *textctrl;
    wxTextAttr *info_attr;
    wxTextAttr *err_attr;
    wxTextAttr *warn_attr;
    wxTextAttr *dbg_attr;

    wxFileDialog *save_log_dialog;

    vlc_bool_t b_verbose;
};

/* Playlist */
class Playlist: public wxFrame
{
public:
    /* Constructor */
    Playlist( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~Playlist();

    void UpdatePlaylist();
    void ShowPlaylist( bool show );
    void UpdateItem( int );
    void AppendItem( wxCommandEvent& );

    bool b_need_update;

private:
    void RemoveItem( int );
    void DeleteTreeItem( wxTreeItemId );
    void DeleteItem( int item );
    void DeleteNode( playlist_item_t *node );

    void RecursiveDeleteSelection( wxTreeItemId );

    /* Event handlers (these functions should _not_ be virtual) */

    /* Menu Handlers */
    void OnAddFile( wxCommandEvent& event );
    void OnAddDir( wxCommandEvent& event );
    void OnAddMRL( wxCommandEvent& event );
    void OnMenuClose( wxCommandEvent& event );
    void OnClose( wxCloseEvent& WXUNUSED(event) );

    void OnDeleteSelection( wxCommandEvent& event );

    void OnOpen( wxCommandEvent& event );
    void OnSave( wxCommandEvent& event );

    /* Search (user) */
    void OnSearch( wxCommandEvent& event );
    /*void OnSearchTextChange( wxCommandEvent& event );*/
    wxTextCtrl *search_text;
    wxButton *search_button;
    wxTreeItemId search_current;

    void OnEnDis( wxCommandEvent& event );

    /* Sort */
    int i_sort_mode;
    void OnSort( wxCommandEvent& event );
    int i_title_sorted;
    int i_group_sorted;
    int i_duration_sorted;

    /* Dynamic menus */
    void OnMenuEvent( wxCommandEvent& event );
    void OnMenuOpen( wxMenuEvent& event );
    wxMenu *p_view_menu;
    wxMenu *p_sd_menu;
    wxMenu *ViewMenu();
    wxMenu *SDMenu();

    void OnUp( wxCommandEvent& event);
    void OnDown( wxCommandEvent& event);

    void OnRandom( wxCommandEvent& event );
    void OnRepeat( wxCommandEvent& event );
    void OnLoop ( wxCommandEvent& event );

    void OnActivateItem( wxTreeEvent& event );
    void OnKeyDown( wxTreeEvent& event );
    void OnNewGroup( wxCommandEvent& event );

    /* Popup  */
    wxMenu *item_popup;
    wxMenu *node_popup;
    wxTreeItemId i_wx_popup_item;
    int i_popup_item;
    int i_popup_parent;
    void OnPopup( wxContextMenuEvent& event );
    void OnPopupPlay( wxCommandEvent& event );
    void OnPopupPreparse( wxCommandEvent& event );
    void OnPopupSort( wxCommandEvent& event );
    void OnPopupDel( wxCommandEvent& event );
    void OnPopupEna( wxCommandEvent& event );
    void OnPopupInfo( wxCommandEvent& event );
    void Rebuild( vlc_bool_t );

    void Preparse();

    /* Update */
    void UpdateNode( playlist_item_t*, wxTreeItemId );
    void UpdateNodeChildren( playlist_item_t*, wxTreeItemId );
    void CreateNode( playlist_item_t*, wxTreeItemId );
    void UpdateTreeItem( wxTreeItemId );

    /* Search (internal) */
    int CountItems( wxTreeItemId);
    wxTreeItemId FindItem( wxTreeItemId, int );
    wxTreeItemId FindItemByName( wxTreeItemId, wxString,
                                 wxTreeItemId, vlc_bool_t *);

    wxTreeItemId saved_tree_item;
    int i_saved_id;

    playlist_t *p_playlist;


    /* Custom events */
    void OnPlaylistEvent( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();


    /* Global widgets */
    wxStatusBar *statusbar;
    ItemInfoDialog *iteminfo_dialog;

    int i_update_counter;

    intf_thread_t *p_intf;
    wxTreeCtrl *treectrl;
    int i_current_view;
    vlc_bool_t b_changed_view;
    char **pp_sds;


};

/* ItemInfo Dialog */
class ItemInfoDialog: public wxDialog
{
public:
    /* Constructor */
    ItemInfoDialog( intf_thread_t *p_intf, playlist_item_t *_p_item,
                    wxWindow *p_parent );
    virtual ~ItemInfoDialog();

    wxArrayString GetOptions();

private:
    wxPanel *InfoPanel( wxWindow* parent );
    wxPanel *GroupPanel( wxWindow* parent );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );

    void UpdateInfo();

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    playlist_item_t *p_item;
    wxWindow *p_parent;

    /* Controls for the iteminfo dialog box */
    wxPanel *info_subpanel;
    wxPanel *info_panel;

    wxPanel *group_subpanel;
    wxPanel *group_panel;

    wxTextCtrl *uri_text;
    wxTextCtrl *name_text;

    wxTreeCtrl *info_tree;
    wxTreeItemId info_root;

};


/* File Info */
class FileInfo: public wxFrame
{
public:
    /* Constructor */
    FileInfo( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~FileInfo();
    void UpdateFileInfo();

    vlc_bool_t b_need_update;

private:
    void OnButtonClose( wxCommandEvent& event );
    void OnClose( wxCloseEvent& WXUNUSED(event) );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxTreeCtrl *fileinfo_tree;
    wxTreeItemId fileinfo_root;
    wxString fileinfo_root_label;

};

/* Update VLC */
class UpdateVLC: public wxFrame
{
public:
    /* Constructor */
    UpdateVLC( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~UpdateVLC();

private:
    void OnButtonClose( wxCommandEvent& event );
    void OnClose( wxCloseEvent& WXUNUSED(event) );
    void GetData();
    void OnCheckForUpdate( wxCommandEvent& event );
    void OnMirrorChoice( wxCommandEvent& event );
    void UpdateUpdatesTree();
    void UpdateMirrorsChoice();
    void OnUpdatesTreeActivate( wxTreeEvent& event );
    void DownloadFile( wxString url, wxString dst );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxTreeCtrl *updates_tree;
    wxTreeItemId updates_root;

    wxChoice *mirrors_choice;

    wxString release_type; /* could be "stable", "test", "nightly" ... */

    struct update_file_t
    {
        wxString type;
        wxString md5;
        wxString size;
        wxString url;
        wxString description;
    };

    struct update_version_t
    {
        wxString type;
        wxString major;
        wxString minor;
        wxString revision;
        wxString extra;
        std::list<update_file_t> m_files;
    };

    std::list<update_version_t> m_versions;

    struct update_mirror_t
    {
        wxString name;
        wxString location;
        wxString type;
        wxString base_url;
    };

    std::list<update_mirror_t> m_mirrors;
};

#if wxUSE_DRAG_AND_DROP
/* Drag and Drop class */
class DragAndDrop: public wxFileDropTarget
{
public:
    DragAndDrop( intf_thread_t *_p_intf, vlc_bool_t b_enqueue = VLC_FALSE );

    virtual bool OnDropFiles( wxCoord x, wxCoord y,
                              const wxArrayString& filenames );

private:
    intf_thread_t *p_intf;
    vlc_bool_t b_enqueue;
};
#endif
} // end of wxvlc namespace

/* */
class WindowSettings
{
public:
    WindowSettings( intf_thread_t *_p_intf );
    virtual ~WindowSettings();
    enum
    {
        ID_SCREEN = -1,
        ID_MAIN,
        ID_PLAYLIST,
        ID_MESSAGES,
        ID_FILE_INFO,
        ID_BOOKMARKS,
        ID_VIDEO,

        ID_MAX,
    };

    void SetSettings( int id, bool _b_shown,
                      wxPoint p = wxDefaultPosition, wxSize s = wxDefaultSize );
    bool GetSettings( int id, bool& _b_shown, wxPoint& p, wxSize& s );

    void SetScreen( int i_screen_w, int i_screen_h );

private:
    intf_thread_t *p_intf;

    int     i_screen_w;
    int     i_screen_h;
    bool    b_valid[ID_MAX];
    bool    b_shown[ID_MAX];
    wxPoint position[ID_MAX];
    wxSize  size[ID_MAX];
};

/* Menus */
void PopupMenu( intf_thread_t *, wxWindow *, const wxPoint& );
wxMenu *SettingsMenu( intf_thread_t *, wxWindow *, wxMenu * = NULL );
wxMenu *AudioMenu( intf_thread_t *, wxWindow *, wxMenu * = NULL );
wxMenu *VideoMenu( intf_thread_t *, wxWindow *, wxMenu * = NULL );
wxMenu *NavigMenu( intf_thread_t *, wxWindow *, wxMenu * = NULL );

namespace wxvlc
{
class MenuEvtHandler : public wxEvtHandler
{
public:
    MenuEvtHandler( intf_thread_t *p_intf, Interface *p_main_interface );
    virtual ~MenuEvtHandler();

    void OnMenuEvent( wxCommandEvent& event );
    void OnShowDialog( wxCommandEvent& event );

private:

    DECLARE_EVENT_TABLE()

    intf_thread_t *p_intf;
    Interface *p_main_interface;
};

} // end of wxvlc namespace


/*
 * wxWindows keeps dead locking because the timer tries to lock the playlist
 * when it's already locked somewhere else in the very wxWindows interface
 * module. Unless someone implements a "vlc_mutex_trylock", we need that.
 */
inline void LockPlaylist( intf_sys_t *p_sys, playlist_t *p_pl )
{
    if( p_sys->i_playlist_usage++ == 0)
        vlc_mutex_lock( &p_pl->object_lock );
}

inline void UnlockPlaylist( intf_sys_t *p_sys, playlist_t *p_pl )
{
    if( --p_sys->i_playlist_usage == 0)
        vlc_mutex_unlock( &p_pl->object_lock );
}

using namespace wxvlc;

