/*****************************************************************************
 * wxwindows.h: private wxWindows interface description
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: wxwindows.h,v 1.87 2004/01/25 03:29:02 hartman Exp $
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

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO

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
#include "vlc_keys.h"

DECLARE_LOCAL_EVENT_TYPE( wxEVT_DIALOG, 0 );

class OpenDialog;
class Playlist;
class Messages;
class FileInfo;

#define SLIDER_MAX_POS 10000

/* wxU is used to convert ansi/utf8 strings to unicode strings (wchar_t) */
#if defined( ENABLE_NLS ) && defined( HAVE_GETTEXT ) && \
    defined( WIN32 ) && !defined( HAVE_INCLUDED_GETTEXT )
#if wxUSE_UNICODE
#   define wxU(utf8) wxString(utf8, wxConvUTF8)
#else
#   define wxU(utf8) wxString(wxConvUTF8.cMB2WC(utf8), *wxConvCurrent)
#endif
#define ISUTF8 1

#else // ENABLE_NLS && HAVE_GETTEXT && WIN32 && !HAVE_INCLUDED_GETTEXT
#if wxUSE_UNICODE
#   define wxU(ansi) wxString(ansi, *wxConvCurrent)
#else
#   define wxU(ansi) ansi
#endif
#define ISUTF8 0

#endif

/* wxL2U (locale to unicode) is used to convert ansi strings to unicode
 * strings (wchar_t) */
#if wxUSE_UNICODE
#   define wxL2U(ansi) wxString(ansi, *wxConvCurrent)
#else
#   define wxL2U(ansi) ansi
#endif

#define WRAPCOUNT 80

#define OPEN_NORMAL 0
#define OPEN_STREAM 1

#define MODE_NONE 0
#define MODE_GROUP 1
#define MODE_AUTHOR 2
#define MODE_TITLE 3

wxArrayString SeparateEntries( wxString );

/*****************************************************************************
 * intf_sys_t: description and status of wxwindows interface
 *****************************************************************************/
struct intf_sys_t
{
    /* the wx parent window */
    wxWindow            *p_wxwindow;
    wxIcon              *p_icon;

    /* special actions */
    vlc_bool_t          b_playing;

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

    /* Send an event to show a dialog */
    void (*pf_show_dialog) ( intf_thread_t *p_intf, int i_dialog, int i_arg,
                             intf_dialog_args_t *p_arg );

    /* Popup menu */
    wxMenu              *p_popup_menu;

};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/*****************************************************************************
 * Classes declarations.
 *****************************************************************************/
class Interface;

/* Timer */
class Timer: public wxTimer
{
public:
    /* Constructor */
    Timer( intf_thread_t *p_intf, Interface *p_main_interface );
    virtual ~Timer();

    virtual void Notify();

private:
    intf_thread_t *p_intf;
    Interface *p_main_interface;
    int i_old_playing_status;
    int i_old_rate;
};

/* Main Interface */
class Interface: public wxFrame
{
public:
    /* Constructor */
    Interface( intf_thread_t *p_intf );
    virtual ~Interface();
    void TogglePlayButton( int i_playing_status );

//    wxFlexGridSizer *frame_sizer;
    wxBoxSizer  *frame_sizer;
    wxStatusBar *statusbar;

    wxSlider    *slider;
    wxWindow    *slider_frame;
    wxWindow    *extra_frame;
    wxStaticBox *slider_box;

    vlc_bool_t b_extra;

    wxStaticBox *adjust_box;
    wxSlider *brightness_slider;
    wxSlider *contrast_slider;
    wxSlider *saturation_slider;
    wxSlider *hue_slider;
    wxSlider *gamma_slider;

    wxStaticBox *other_box;
    wxComboBox *ratio_combo;

    wxGauge     *volctrl;

private:
    void UpdateAcceleratorTable();
    void CreateOurMenuBar();
    void CreateOurToolBar();
    void CreateOurExtraPanel();
    void CreateOurSlider();
    void Open( int i_access_method );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnExit( wxCommandEvent& event );
    void OnAbout( wxCommandEvent& event );

    void OnOpenFileSimple( wxCommandEvent& event );
    void OnOpenFile( wxCommandEvent& event );
    void OnOpenDisc( wxCommandEvent& event );
    void OnOpenNet( wxCommandEvent& event );
    void OnOpenSat( wxCommandEvent& event );
    void OnOpenV4L( wxCommandEvent& event );
    void OnStream( wxCommandEvent& event );
    void OnExtra( wxCommandEvent& event );
    void OnShowDialog( wxCommandEvent& event );
    void OnPlayStream( wxCommandEvent& event );
    void OnStopStream( wxCommandEvent& event );
    void OnSliderUpdate( wxScrollEvent& event );
    void OnPrevStream( wxCommandEvent& event );
    void OnNextStream( wxCommandEvent& event );
    void OnSlowStream( wxCommandEvent& event );
    void OnFastStream( wxCommandEvent& event );

    void OnEnableAdjust( wxCommandEvent& event );
    void OnHueUpdate( wxScrollEvent& event );
    void OnContrastUpdate( wxScrollEvent& event );
    void OnBrightnessUpdate( wxScrollEvent& event );
    void OnSaturationUpdate( wxScrollEvent& event );
    void OnGammaUpdate( wxScrollEvent& event );

    void OnRatio( wxCommandEvent& event );
    void OnEnableVisual( wxCommandEvent& event );

    void OnMenuOpen( wxMenuEvent& event );

#if defined( __WXMSW__ ) || defined( __WXMAC__ )
    void OnContextMenu2(wxContextMenuEvent& event);
#endif
    void OnContextMenu(wxMouseEvent& event);

    DECLARE_EVENT_TABLE();

    Timer *timer;
    intf_thread_t *p_intf;

private:
    int i_old_playing_status;

    /* For auto-generated menus */
    wxMenu *p_settings_menu;
    vlc_bool_t b_settings_menu;
    wxMenu *p_audio_menu;
    vlc_bool_t b_audio_menu;
    wxMenu *p_video_menu;
    vlc_bool_t b_video_menu;
    wxMenu *p_navig_menu;
    vlc_bool_t b_navig_menu;
};

class StreamDialog;

/* Dialogs Provider */
class DialogsProvider: public wxFrame
{
public:
    /* Constructor */
    DialogsProvider( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~DialogsProvider();

private:
    void Open( int i_access_method, int i_arg );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnExit( wxCommandEvent& event );
    void OnPlaylist( wxCommandEvent& event );
    void OnMessages( wxCommandEvent& event );
    void OnFileInfo( wxCommandEvent& event );
    void OnPreferences( wxCommandEvent& event );
    void OnStreamDialog( wxCommandEvent& event );

    void OnOpenFileGeneric( wxCommandEvent& event );
    void OnOpenFileSimple( wxCommandEvent& event );
    void OnOpenFile( wxCommandEvent& event );
    void OnOpenDisc( wxCommandEvent& event );
    void OnOpenNet( wxCommandEvent& event );
    void OnOpenSat( wxCommandEvent& event );

    void OnPopupMenu( wxCommandEvent& event );

    void OnIdle( wxIdleEvent& event );

    void OnExitThread( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;

public:
    /* Secondary windows */
    OpenDialog          *p_open_dialog;
    wxFileDialog        *p_file_dialog;
    Playlist            *p_playlist_dialog;
    Messages            *p_messages_dialog;
    FileInfo            *p_fileinfo_dialog;
    StreamDialog        *p_stream_dialog;
    wxFrame             *p_prefs_dialog;
    wxFileDialog        *p_file_generic_dialog;
};

/* Open Dialog */
class AutoBuiltPanel;
WX_DEFINE_ARRAY(AutoBuiltPanel *, ArrayOfAutoBuiltPanel);
class V4LDialog;
class SoutDialog;
class SubsFileDialog;
class OpenDialog: public wxFrame
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
    wxPanel *V4LPanel( wxWindow* parent );

    ArrayOfAutoBuiltPanel input_tab_array;

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );

    void OnPageChange( wxNotebookEvent& event );
    void OnMRLChange( wxCommandEvent& event );

    /* Event handlers for the file page */
    void OnFilePanelChange( wxCommandEvent& event );
    void OnFileBrowse( wxCommandEvent& event );

    /* Event handlers for the disc page */
    void OnDiscPanelChange( wxCommandEvent& event );
    void OnDiscTypeChange( wxCommandEvent& event );
    void OnDiscDeviceChange( wxCommandEvent& event );

    /* Event handlers for the net page */
    void OnNetPanelChange( wxCommandEvent& event );
    void OnNetTypeChange( wxCommandEvent& event );

    /* Event handlers for the v4l page */
    void OnV4LPanelChange( wxCommandEvent& event );
    void OnV4LTypeChange( wxCommandEvent& event );
    void OnV4LSettingsChange( wxCommandEvent& event );

    /* Event handlers for the stream output */
    void OnSubsFileEnable( wxCommandEvent& event );
    void OnSubsFileSettings( wxCommandEvent& WXUNUSED(event) );

    /* Event handlers for the stream output */
    void OnSoutEnable( wxCommandEvent& event );
    void OnSoutSettings( wxCommandEvent& WXUNUSED(event) );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxWindow *p_parent;
    int i_current_access_method;
    int i_disc_type_selection;

    int i_method; /* Normal or for the stream dialog ? */
    wxComboBox *mrl_combo;
    wxNotebook *notebook;

    /* Controls for the file panel */
    wxComboBox *file_combo;
    wxFileDialog *file_dialog;

    /* Controls for the disc panel */
    wxRadioBox *disc_type;
    wxTextCtrl *disc_device;
    wxSpinCtrl *disc_title;
    wxSpinCtrl *disc_chapter;

    /* The media equivalent name for a DVD names. For example,
       "Title", is "Track" for a CD-DA */
    wxStaticText *disc_title_label;
    wxStaticText *disc_chapter_label;
    
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
    wxCheckBox *net_ipv6;

    /* Controls for the v4l panel */
    wxRadioBox *video_type;
    wxTextCtrl *video_device;
    wxSpinCtrl *video_channel;
    wxButton *v4l_button;
    V4LDialog *v4l_dialog;
    wxArrayString v4l_mrl;

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
};

enum
{
    FILE_ACCESS = 0,
    DISC_ACCESS,
    NET_ACCESS,
#ifndef WIN32
    V4L_ACCESS,
#endif
    MAX_ACCESS,
    FILE_SIMPLE_ACCESS
};

/* V4L Dialog */
class V4LDialog: public wxDialog
{
public:
    /* Constructor */
    V4LDialog( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~V4LDialog();

    wxArrayString GetOptions();

private:
    void UpdateMRL();
    wxPanel *AudioPanel( wxWindow* parent );
    wxPanel *CommonPanel( wxWindow* parent );
    wxPanel *BitratePanel( wxWindow* parent );
    void    ParseMRL();

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );
    void OnMRLChange( wxCommandEvent& event );
    void OnAudioEnable( wxCommandEvent& event );
    void OnAudioChange( wxCommandEvent& event );
    void OnAudioChannel( wxCommandEvent& event );
    void OnSizeEnable( wxCommandEvent& event );
    void OnSize( wxCommandEvent& event );
    void OnNormEnable( wxCommandEvent& event );
    void OnNorm( wxCommandEvent& event );
    void OnFrequencyEnable( wxCommandEvent& event );
    void OnFrequency( wxCommandEvent& event );
    void OnBitrateEnable( wxCommandEvent& event );
    void OnBitrate( wxCommandEvent& event );
    void OnMaxBitrateEnable( wxCommandEvent& event );
    void OnMaxBitrate( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxWindow *p_parent;

    wxComboBox *mrl_combo;

    int i_access_type;

    /* Controls for the v4l advanced options */
    wxPanel *common_subpanel;
    wxPanel *common_panel;
    wxCheckBox *size_checkbox;
    wxComboBox *size_combo;
    wxCheckBox *norm_checkbox;
    wxComboBox *norm_combo;
    wxCheckBox *frequency_checkbox;
    wxSpinCtrl *frequency;

    wxPanel *audio_subpanel;
    wxPanel *audio_panel;
    wxCheckBox *audio_checkbox;
    wxTextCtrl *audio_device;
    wxSpinCtrl *audio_channel;

    wxPanel *bitrate_subpanel;
    wxPanel *bitrate_panel;
    wxCheckBox *bitrate_checkbox;
    wxSpinCtrl *bitrate;
    wxCheckBox *maxbitrate_checkbox;
    wxSpinCtrl *maxbitrate;

};

/* Stream output Dialog */
enum
{
    PLAY_ACCESS_OUT = 0,
    FILE_ACCESS_OUT,
    HTTP_ACCESS_OUT,
    MMSH_ACCESS_OUT,
    UDP_ACCESS_OUT,
    RTP_ACCESS_OUT,
    ACCESS_OUT_NUM
};

enum
{
    TS_ENCAPSULATION = 0,
    PS_ENCAPSULATION,
    MPEG1_ENCAPSULATION,
    OGG_ENCAPSULATION,
    RAW_ENCAPSULATION,
    ASF_ENCAPSULATION,
    AVI_ENCAPSULATION,
    MP4_ENCAPSULATION,
    MOV_ENCAPSULATION,
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
    wxSpinCtrl *delay_spinctrl;
    wxSpinCtrl *fps_spinctrl;

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



/* Preferences Dialog */
class PrefsTreeCtrl;
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
    void OnClose( wxCommandEvent& event );
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
class ItemInfoDialog;
class NewGroup;
class ExportPlaylist;
class Playlist: public wxFrame
{
public:
    /* Constructor */
    Playlist( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~Playlist();

    void UpdatePlaylist();
    void ShowPlaylist( bool show );
    void UpdateItem( int );

    bool b_need_update;
    vlc_mutex_t lock;

private:
    void DeleteItem( int item );
    void ShowInfos( int item );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnAddFile( wxCommandEvent& event );
    void OnAddMRL( wxCommandEvent& event );
    void OnClose( wxCommandEvent& event );
    void OnSearch( wxCommandEvent& event );
    void OnEnDis( wxCommandEvent& event );
    void OnInfos( wxCommandEvent& event );
    void OnSearchTextChange( wxCommandEvent& event );
    void OnOpen( wxCommandEvent& event );
    void OnSave( wxCommandEvent& event );

    void OnSort( wxCommandEvent& event );
    void OnColSelect( wxListEvent& event );

    void OnUp( wxCommandEvent& event);
    void OnDown( wxCommandEvent& event);

    void OnEnableSelection( wxCommandEvent& event );
    void OnDisableSelection( wxCommandEvent& event );
    void OnInvertSelection( wxCommandEvent& event );
    void OnDeleteSelection( wxCommandEvent& event );
    void OnSelectAll( wxCommandEvent& event );
    void OnRandom( wxCommandEvent& event );
    void OnRepeat( wxCommandEvent& event );
    void OnLoop ( wxCommandEvent& event );
    void OnActivateItem( wxListEvent& event );
    void OnKeyDown( wxListEvent& event );
    void OnNewGroup( wxCommandEvent& event );

    /* Popup functions */
    void OnPopup( wxListEvent& event );
    void OnPopupPlay( wxMenuEvent& event );
    void OnPopupDel( wxMenuEvent& event );
    void OnPopupEna( wxMenuEvent& event );
    void OnPopupInfo( wxMenuEvent& event );
    void Rebuild();

    wxTextCtrl *search_text;
    wxButton *search_button;
    DECLARE_EVENT_TABLE();

    wxMenu *popup_menu;

    ItemInfoDialog *iteminfo_dialog;

    intf_thread_t *p_intf;
    wxListView *listview;
    wxTreeCtrl *treeview;
    int i_update_counter;
    int i_sort_mode;

    int i_popup_item;

    int i_title_sorted;
    int i_author_sorted;
    int i_group_sorted;
};


class NewGroup: public wxDialog
{
public:
    /* Constructor */
    NewGroup( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~NewGroup();

private:

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxTextCtrl *groupname;

protected:
    friend class Playlist;
    friend class ItemInfoDialog;
    char *psz_name;
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
    void OnNewGroup( wxCommandEvent& event );

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
    wxTextCtrl *author_text;

    wxTreeCtrl *info_tree;
    wxTreeItemId info_root;

    wxCheckBox *enabled_checkbox;
    wxComboBox *group_combo;
    int ids_array[100];
};


/* File Info */
class FileInfo: public wxFrame
{
public:
    /* Constructor */
    FileInfo( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~FileInfo();
    void UpdateFileInfo();

private:
    void OnClose( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxTreeCtrl *fileinfo_tree;
    wxTreeItemId fileinfo_root;
    wxString fileinfo_root_label;

};


#if !defined(__WXX11__)
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

/* Menus */
void PopupMenu( intf_thread_t *_p_intf, wxWindow *p_parent,
                const wxPoint& pos );
wxMenu *SettingsMenu( intf_thread_t *_p_intf, wxWindow *p_parent );
wxMenu *AudioMenu( intf_thread_t *_p_intf, wxWindow *p_parent );
wxMenu *VideoMenu( intf_thread_t *_p_intf, wxWindow *p_parent );
wxMenu *NavigMenu( intf_thread_t *_p_intf, wxWindow *p_parent );

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

class Menu: public wxMenu
{
public:
    /* Constructor */
    Menu( intf_thread_t *p_intf, wxWindow *p_parent, int i_count,
          char **ppsz_names, int *pi_objects, int i_start_id );
    virtual ~Menu();

private:
    /* Event handlers (these functions should _not_ be virtual) */
    void OnClose( wxCommandEvent& event );
    void OnShowDialog( wxCommandEvent& event );
    void OnEntrySelected( wxCommandEvent& event );

    wxMenu *Menu::CreateDummyMenu();
    void   Menu::CreateMenuItem( wxMenu *, char *, vlc_object_t * );
    wxMenu *Menu::CreateChoicesMenu( char *, vlc_object_t * );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;

    int  i_item_id;
};

static inline int ConvertHotkeyModifiers( int i_hotkey )
{
    int i_accel_flags = 0;
    if( i_hotkey & KEY_MODIFIER_ALT ) i_accel_flags |= wxACCEL_ALT;
    if( i_hotkey & KEY_MODIFIER_CTRL ) i_accel_flags |= wxACCEL_CTRL;
    if( i_hotkey & KEY_MODIFIER_SHIFT ) i_accel_flags |= wxACCEL_SHIFT;
    return i_accel_flags;
}

static inline int ConvertHotkey( int i_hotkey )
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
    return 0;
}
