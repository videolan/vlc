/*****************************************************************************
 * wxwindows.h: private wxWindows interface description
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: wxwindows.h,v 1.48 2003/07/24 16:07:10 gbazin Exp $
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

#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/dnd.h>
#include <wx/treectrl.h>
#include <wx/gauge.h>

DECLARE_LOCAL_EVENT_TYPE( wxEVT_DIALOG, 0 );

enum
{
    FILE_ACCESS,
    DISC_ACCESS,
    NET_ACCESS,
    SAT_ACCESS,
    FILE_SIMPLE_ACCESS
};

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

#else // ENABLE_NLS && HAVE_GETTEXT && WIN32 && !HAVE_INCLUDED_GETTEXT
#if wxUSE_UNICODE
#   define wxU(ansi) wxString(ansi, *wxConvCurrent)
#else
#   define wxU(ansi) ansi
#endif

#endif

#if !defined(MODULE_NAME_IS_skins)
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
#endif /* !defined(MODULE_NAME_IS_skins) */

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

    wxBoxSizer  *frame_sizer;
    wxStatusBar *statusbar;

    wxSlider    *slider;
    wxWindow    *slider_frame;
    wxStaticBox *slider_box;

    wxGauge     *volctrl;

private:
    void CreateOurMenuBar();
    void CreateOurToolBar();
    void CreateOurSlider();
    void Open( int i_access_method );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnExit( wxCommandEvent& event );
    void OnAbout( wxCommandEvent& event );

    void OnShowDialog( wxCommandEvent& event );

    void OnPlayStream( wxCommandEvent& event );
    void OnStopStream( wxCommandEvent& event );
    void OnSliderUpdate( wxScrollEvent& event );
    void OnPrevStream( wxCommandEvent& event );
    void OnNextStream( wxCommandEvent& event );
    void OnSlowStream( wxCommandEvent& event );
    void OnFastStream( wxCommandEvent& event );

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
    wxMenu *p_audio_menu;
    vlc_bool_t b_audio_menu;
    wxMenu *p_video_menu;
    vlc_bool_t b_video_menu;
    wxMenu *p_navig_menu;
    vlc_bool_t b_navig_menu;
};

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
    wxFrame             *p_prefs_dialog;
    wxFileDialog        *p_file_generic_dialog;
};

/* Open Dialog */
class SoutDialog;
class SubsFileDialog;
class OpenDialog: public wxFrame
{
public:
    /* Constructor */
    OpenDialog( intf_thread_t *p_intf, wxWindow *p_parent,
                int i_access_method, int i_arg = 0 );
    virtual ~OpenDialog();

    int Show();
    int Show( int i_access_method, int i_arg = 0 );

    wxArrayString mrl;

private:
    wxPanel *FilePanel( wxWindow* parent );
    wxPanel *DiscPanel( wxWindow* parent );
    wxPanel *NetPanel( wxWindow* parent );
    wxPanel *SatPanel( wxWindow* parent );

    void UpdateMRL( int i_access_method );
    wxArrayString SeparateEntries( wxString );

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

    /* Event handlers for the net page */
    void OnNetPanelChange( wxCommandEvent& event );
    void OnNetTypeChange( wxCommandEvent& event );

    /* Event handlers for the stream output */
    void OnSubsFileEnable( wxCommandEvent& event );
    void OnSubsFileSettings( wxCommandEvent& WXUNUSED(event) );

    /* Event handlers for the stream output */
    void OnSoutEnable( wxCommandEvent& event );
    void OnSoutSettings( wxCommandEvent& WXUNUSED(event) );

    /* Event handlers for the demux dump */
    void OnDemuxDumpEnable( wxCommandEvent& event );
    void OnDemuxDumpBrowse( wxCommandEvent& event );
    void OnDemuxDumpChange( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxWindow *p_parent;
    int i_current_access_method;

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

    /* Controls for the net panel */
    wxRadioBox *net_type;
    int i_net_type;
    wxPanel *net_subpanels[4];
    wxRadioButton *net_radios[4];
    wxSpinCtrl *net_ports[4];
    wxTextCtrl *net_addrs[4];

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

    /* Controls for the demux dump */
    wxTextCtrl *demuxdump_textctrl;
    wxButton *demuxdump_button;
    wxCheckBox *demuxdump_checkbox;
    wxFileDialog *demuxdump_dialog;
};

/* Stream output Dialog */
class SoutDialog: public wxDialog
{
public:
    /* Constructor */
    SoutDialog( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~SoutDialog();

    wxString mrl;

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

    /* Event handlers for the net access output */
    void OnNetChange( wxCommandEvent& event );

    /* Event specific to the sap address */
    void OnSAPAddrChange( wxCommandEvent& event );
    
    /* Event handlers for the encapsulation panel */
    void OnEncapsulationChange( wxCommandEvent& event );

    /* Event handlers for the transcoding panel */
    void OnTranscodingEnable( wxCommandEvent& event );
    void OnTranscodingChange( wxCommandEvent& event );

    /* Event handlers for the misc panel */
    void OnSAPMiscChange( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxWindow *p_parent;

    wxComboBox *mrl_combo;

    /* Controls for the access outputs */
    wxPanel *access_subpanels[5];
    wxCheckBox *access_checkboxes[5];

    int i_access_type;

    wxComboBox *file_combo;
    wxSpinCtrl *net_ports[5];
    wxTextCtrl *net_addrs[5];

    /* Controls for the SAP announces */
    wxPanel *misc_subpanels[1];
    wxCheckBox *sap_checkbox;
    wxTextCtrl *sap_addr;
                            
    /* Controls for the encapsulation */
    wxRadioButton *encapsulation_radios[5];
    int i_encapsulation_type;

    /* Controls for transcoding */
    wxCheckBox *video_transc_checkbox;
    wxComboBox *video_codec_combo;
    wxComboBox *audio_codec_combo;
    wxCheckBox *audio_transc_checkbox;
    wxComboBox *video_bitrate_combo;
    wxComboBox *audio_bitrate_combo;
    wxComboBox *audio_channels_combo;
};

/* Subtitles File Dialog */
class SubsFileDialog: public wxDialog
{
public:
    /* Constructor */
    SubsFileDialog( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~SubsFileDialog();

    wxComboBox *file_combo;
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
    void UpdateLog();

private:
    /* Event handlers (these functions should _not_ be virtual) */
    void OnClose( wxCommandEvent& event );
    void OnVerbose( wxCommandEvent& event );
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

    bool b_need_update;
    vlc_mutex_t lock;

private:
    void DeleteItem( int item );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnAddFile( wxCommandEvent& event );
    void OnAddMRL( wxCommandEvent& event );
    void OnClose( wxCommandEvent& event );
    void OnOpen( wxCommandEvent& event );
    void OnSave( wxCommandEvent& event );
    void OnInvertSelection( wxCommandEvent& event );
    void OnDeleteSelection( wxCommandEvent& event );
    void OnSelectAll( wxCommandEvent& event );
    void OnActivateItem( wxListEvent& event );
    void OnKeyDown( wxListEvent& event );
    void Rebuild();

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxListView *listview;
    int i_update_counter;
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
    DragAndDrop( intf_thread_t *_p_intf );

    virtual bool OnDropFiles( wxCoord x, wxCoord y,
                              const wxArrayString& filenames );

private:
    intf_thread_t *p_intf;
};
#endif

/* Menus */
void PopupMenu( intf_thread_t *_p_intf, wxWindow *p_parent,
                const wxPoint& pos );
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
