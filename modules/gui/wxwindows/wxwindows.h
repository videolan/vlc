/*****************************************************************************
 * wxwindows.h: private wxWindows interface description
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: wxwindows.h,v 1.17 2003/04/17 14:00:44 anil Exp $
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

class Playlist;
class Messages;
class FileInfo;

#define SLIDER_MAX_POS 10000

/*****************************************************************************
 * intf_sys_t: description and status of wxwindows interface
 *****************************************************************************/
struct intf_sys_t
{
    /* the wx parent window */
    wxWindow            *p_wxwindow;
    wxIcon              *p_icon;

    /* secondary windows */
    Playlist            *p_playlist_window;
    Messages            *p_messages_window;
    FileInfo            *p_fileinfo_window;

    /* special actions */
    vlc_bool_t          b_playing;
    vlc_bool_t          b_popup_changed;                   /* display menu ? */
    vlc_bool_t          b_window_changed;        /* window display toggled ? */
    vlc_bool_t          b_playlist_changed;    /* playlist display toggled ? */
    vlc_bool_t          b_slider_free;                      /* slider status */

    /* menus handlers */
    vlc_bool_t          b_program_update;   /* do we need to update programs
                                                                        menu */
    vlc_bool_t          b_title_update;  /* do we need to update title menus */
    vlc_bool_t          b_chapter_update;            /* do we need to update
                                                               chapter menus */
    vlc_bool_t          b_audio_update;  /* do we need to update audio menus */
    vlc_bool_t          b_spu_update;      /* do we need to update spu menus */

    /* windows and widgets */

    /* The input thread */
    input_thread_t *    p_input;

    /* The slider */
    int                 i_slider_pos;                     /* slider position */
    int                 i_slider_oldpos;                /* previous position */

    /* The messages window */
    msg_subscription_t* p_sub;                  /* message bank subscription */

    /* Playlist management */
    int                 i_playing;                 /* playlist selected item */

    /* The window labels for DVD mode */
    unsigned int        i_part;                           /* current chapter */
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

    wxMenu      *p_popup_menu;

    wxArrayString mrl_history;

private:
    void CreateOurMenuBar();
    void CreateOurToolBar();
    void CreateOurSlider();
    void Open( int i_access_method );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnExit( wxCommandEvent& event );
    void OnAbout( wxCommandEvent& event );
    void OnMessages( wxCommandEvent& event );
    void OnPlaylist( wxCommandEvent& event );
    void OnLogs( wxCommandEvent& event );
    void OnFileInfo( wxCommandEvent& event );
    void OnPreferences( wxCommandEvent& event );

    void OnOpenFile( wxCommandEvent& event );
    void OnOpenDisc( wxCommandEvent& event );
    void OnOpenNet( wxCommandEvent& event );
    void OnOpenSat( wxCommandEvent& event );

    void OnPlayStream( wxCommandEvent& event );
    void OnStopStream( wxCommandEvent& event );
    void OnSliderUpdate( wxScrollEvent& event );
    void OnPrevStream( wxCommandEvent& event );
    void OnNextStream( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    Timer *timer;
    intf_thread_t *p_intf;

    wxFrame *p_prefs_dialog;

    int i_old_playing_status;
};

/* Open Dialog */
class OpenDialog: public wxDialog
{
public:
    /* Constructor */
    OpenDialog( intf_thread_t *p_intf, Interface *p_main_interface,
                int i_access_method );
    virtual ~OpenDialog();
    void Rebuild();
    void Manage();

    wxString mrl;

private:
    wxPanel *FilePanel( wxWindow* parent );
    wxPanel *DiscPanel( wxWindow* parent );
    wxPanel *NetPanel( wxWindow* parent );
    wxPanel *SatPanel( wxWindow* parent );

    void UpdateMRL( int i_access_method );

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
    void OnSoutEnable( wxCommandEvent& event );
    void OnSoutSettings( wxCommandEvent& WXUNUSED(event) );

    /* Event handlers for the demux dump */
    void OnDemuxDumpEnable( wxCommandEvent& event );
    void OnDemuxDumpBrowse( wxCommandEvent& event );
    void OnDemuxDumpChange( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    Interface *p_main_interface;
    int i_current_access_method;

    wxComboBox *mrl_combo;

    /* Controls for the file panel */
    wxComboBox *file_combo;

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

    /* Controls for the stream output */
    wxButton *sout_button;
    wxCheckBox *sout_checkbox;

    /* Controls for the demux dump */
    wxTextCtrl *demuxdump_textctrl;
    wxButton *demuxdump_button;
    wxCheckBox *demuxdump_checkbox;
};

enum
{
    FILE_ACCESS = 0,
    DISC_ACCESS,
    NET_ACCESS,
    SAT_ACCESS
};

/* Stream output Dialog */
class SoutDialog: public wxDialog
{
public:
    /* Constructor */
    SoutDialog( intf_thread_t *p_intf, Interface *p_main_interface );
    virtual ~SoutDialog();

    wxString mrl;

private:
    void UpdateMRL();
    wxPanel *AccessPanel( wxWindow* parent );
    wxPanel *EncapsulationPanel( wxWindow* parent );
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

    /* Event handlers for the encapsulation panel */
    void OnEncapsulationChange( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    Interface *p_main_interface;

    wxComboBox *mrl_combo;
    wxPanel *access_panel;
    wxPanel *encapsulation_panel;

    /* Controls for the access outputs */
    wxPanel *access_subpanels[4];
    wxRadioButton *access_radios[4];

    int i_access_type;

    wxComboBox *file_combo;
    wxSpinCtrl *net_port;
    wxTextCtrl *net_addr;

    /* Controls for the encapsulation */
    wxRadioButton *encapsulation_radios[4];
    int i_encapsulation_type;

};

/* Preferences Dialog */
class PrefsTreeCtrl;
class PrefsDialog: public wxFrame
{
public:
    /* Constructor */
    PrefsDialog( intf_thread_t *p_intf, Interface *p_main_interface );
    virtual ~PrefsDialog();

private:
    wxPanel *PrefsPanel( wxWindow* parent );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );
    void OnSave( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    Interface *p_main_interface;

    PrefsTreeCtrl *prefs_tree;
};

/* Messages */
class Messages: public wxFrame
{
public:
    /* Constructor */
    Messages( intf_thread_t *p_intf, Interface *_p_main_interface );
    virtual ~Messages();
    void UpdateLog();

private:
    /* Event handlers (these functions should _not_ be virtual) */
    void OnClose( wxCommandEvent& event );
    void OnVerbose( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    Interface *p_main_interface;
    wxTextCtrl *textctrl;
    wxTextAttr *info_attr;
    wxTextAttr *err_attr;
    wxTextAttr *warn_attr;
    wxTextAttr *dbg_attr;

    vlc_bool_t b_verbose;
};

/* Playlist */
class Playlist: public wxFrame
{
public:
    /* Constructor */
    Playlist( intf_thread_t *p_intf, Interface *p_main_interface );
    virtual ~Playlist();
    void Rebuild();
    void Manage();

private:
    void DeleteItem( int item );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnAddUrl( wxCommandEvent& event );
    void OnAddDirectory( wxCommandEvent& event );
    void OnClose( wxCommandEvent& event );
    void OnInvertSelection( wxCommandEvent& event );
    void OnDeleteSelection( wxCommandEvent& event );
    void OnSelectAll( wxCommandEvent& event );
    void OnActivateItem( wxListEvent& event );
    void OnKeyDown( wxListEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    Interface *p_main_interface;
    wxListView *listview;
    wxButton *ok_button;
};

/* File Info */
class FileInfo: public wxFrame
{
public:
    /* Constructor */
    FileInfo( intf_thread_t *p_intf, Interface *p_main_interface );
    virtual ~FileInfo();
    void UpdateFileInfo();

private:
    void OnClose( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();
   
    intf_thread_t *p_intf;
    wxTreeCtrl *fileinfo_tree;
    wxTreeItemId fileinfo_root;

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

/* Popup contextual menu */
class PopupMenu: public wxMenu
{
public:
    /* Constructor */
    PopupMenu( intf_thread_t *p_intf, Interface *p_main_interface );
    virtual ~PopupMenu();

private:
    /* Event handlers (these functions should _not_ be virtual) */
    void OnClose( wxCommandEvent& event );
    void OnEntrySelected( wxCommandEvent& event );

    wxMenu *PopupMenu::CreateDummyMenu();
    void   PopupMenu::CreateMenuEntry( char *, vlc_object_t * );
    wxMenu *PopupMenu::CreateSubMenu( char *, vlc_object_t * );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    Interface *p_main_interface;

    int  i_item_id;
};

class PopupEvtHandler : public wxEvtHandler
{
public:
    PopupEvtHandler( intf_thread_t *p_intf, Interface *p_main_interface );
    virtual ~PopupEvtHandler();

    void PopupEvtHandler::OnMenuEvent( wxCommandEvent& event );

private:

    DECLARE_EVENT_TABLE()

    intf_thread_t *p_intf;
    Interface *p_main_interface;
};

class wxMenuItemExt: public wxMenuItem
{
public:
    /* Constructor */
    wxMenuItemExt( wxMenu* parentMenu, int id,
                   const wxString& text,
                   const wxString& helpString,
                   wxItemKind kind,
                   char *_psz_var, int _i_object_id, vlc_value_t _val ):
        wxMenuItem( parentMenu, id, text, helpString, kind )
    {
        /* Initializations */
        psz_var = _psz_var;
        i_object_id = _i_object_id;
        val = _val;
    };

    virtual ~wxMenuItemExt() { if( psz_var ) free( psz_var ); };

    char *psz_var;
    int  i_object_id;
    vlc_value_t val;

private:

};
