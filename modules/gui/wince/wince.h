/*****************************************************************************
 * wince.h: private WinCE interface descriptor
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Marodon Cedric <cedric_marodon@yahoo.fr>
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

#ifndef WINCE_RESOURCE

#define MENU_HEIGHT 26
#define SLIDER_HEIGHT 50

#define SLIDER_MAX_POS 10000

#define FILE_ACCESS 1
#define NET_ACCESS 2

#define OPEN_NORMAL 0
#define OPEN_STREAM 1

#include "vlc_keys.h"

#include <stdio.h>
#include <string>
#include <vector>
using namespace std; 

vector<string> SeparateEntries( LPWSTR entries );

class MenuItemExt;
class VideoWindow;

/*****************************************************************************
 * intf_sys_t: description and status of wxwindows interface
 *****************************************************************************/
struct intf_sys_t
{
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

    /* Dynamic Menu management */
    vector<MenuItemExt*> *p_audio_menu;
    vector<MenuItemExt*> *p_video_menu;
    vector<MenuItemExt*> *p_navig_menu;
    vector<MenuItemExt*> *p_settings_menu;

    VideoWindow          *p_video_window;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

class CBaseWindow
{
public:
   CBaseWindow(){ hInst = 0; }
   virtual ~CBaseWindow() {};

   HWND hWnd;                // The main window handle
   BOOL DlgFlag;             // True if object is a dialog window

   static LRESULT CALLBACK BaseWndProc( HWND hwnd, UINT msg,
                                        WPARAM wParam, LPARAM lParam );

protected:

   HINSTANCE       hInst;               // The current instance
   HWND            hwndCB;              // The command bar handle

   HINSTANCE       GetInstance () const { return hInst; }
   virtual LRESULT WndProc( HWND hwnd, UINT msg,
                            WPARAM wParam, LPARAM lParam,
                            PBOOL pbProcessed ){*pbProcessed = FALSE; return 0;}
};

class FileInfo;
class Messages;
class Playlist;
class Timer;
class OpenDialog;
class PrefsDialog;

CBaseWindow *CreateVideoWindow( intf_thread_t *, HINSTANCE, HWND );

/* Main Interface */
class Interface : public CBaseWindow
{
public:
    /* Constructor */
    Interface(){}
    ~Interface(){}

    BOOL InitInstance( HINSTANCE hInstance, intf_thread_t *_pIntf );

    void TogglePlayButton( int i_playing_status );

    HWND hwndMain;      // Handle to the main window.

    HWND hwndCB;        // Handle to the command bar (contains menu)
    HWND hwndTB;        // Handle to the toolbar.
    HWND hwndSlider;       // Handle to the Sliderbar.
    HWND hwndLabel;
    HWND hwndVol;          // Handle to the volume trackbar.
    HWND hwndSB;        // Handle to the status bar.
    HMENU hPopUpMenu;
    HMENU hMenu;
    FileInfo *fi; // pas besoin de la plupart de ses attributs
    Messages *hmsg;
    PrefsDialog *pref;
    Playlist *pl;
    Timer *ti;
    OpenDialog *open;
    CBaseWindow *video;
    HWND hwndVideo;

protected:

    virtual LRESULT WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                             PBOOL pbProcessed );

    HWND WINAPI CreateToolbar( HWND );
    HWND WINAPI CreateSliderbar( HWND );
    HWND WINAPI CreateStaticText( HWND );
    HWND WINAPI CreateVolTrackbar( HWND );
    HWND WINAPI CreateStatusbar( HWND );

    void OnOpenFileSimple( void );
    void OnPlayStream( void );
    void OnVideoOnTop( void );

    void OnSliderUpdate( int wp );
    void OnChange( int wp );
    void Change( int i_volume );
    void OnStopStream( void );
    void OnPrevStream( void );
    void OnNextStream( void );
    void OnSlowStream( void );
    void OnFastStream( void );

    intf_thread_t *pIntf;

    int i_old_playing_status;
};

/* File Info */
class FileInfo : public CBaseWindow
{
public:
    /* Constructor */
    FileInfo( intf_thread_t *_p_intf, HINSTANCE _hInst );
    virtual ~FileInfo(){};

protected:

    HWND hwnd_fileinfo;                 // handle to fileinfo window
    HWND hwndTV;                                // handle to tree-view control 
    intf_thread_t *p_intf;

    TCHAR szFileInfoClassName[100];     // Main window class name
    TCHAR szFileInfoTitle[100];         // Main window name

    virtual LRESULT WndProc( HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam, PBOOL pbProcessed );
    void UpdateFileInfo( HWND );
    BOOL CreateTreeView( HWND );
};

/* Messages */
class Messages : public CBaseWindow
{
public:
    /* Constructor */
    Messages( intf_thread_t *_p_intf, HINSTANCE _hInst );
    virtual ~Messages(){};

protected:

    intf_thread_t *p_intf;

    virtual LRESULT WndProc( HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam,
                             PBOOL pbProcessed );

    HWND hListView;
    void UpdateLog(void);

    vlc_bool_t b_verbose;
};

/* ItemInfo Dialog */
class ItemInfoDialog : public CBaseWindow
{
public:
    /* Constructor */
    ItemInfoDialog( intf_thread_t *, HINSTANCE, playlist_item_t * );
    virtual ~ItemInfoDialog(){};

protected:

    intf_thread_t *p_intf;
    HWND hwndCB;        // Handle to the command bar (but no menu)

    playlist_item_t *p_item;

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk();
    void UpdateInfo();

    virtual LRESULT WndProc( HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam,
                             PBOOL pbProcessed );

    /* Controls for the iteminfo dialog box */
    HWND uri_label;
    HWND uri_text;

    HWND name_label;
    HWND name_text;

    HWND checkbox_label;
    HWND enabled_checkbox;

    HWND info_tree;
};

/* Open Dialog */
class SubsFileDialog;
class OpenDialog : public CBaseWindow
{
public:
    /* Constructor */
    OpenDialog( intf_thread_t *_p_intf, HINSTANCE _hInst,
                int _i_access_method, int _i_arg, int _i_method );
    virtual ~OpenDialog(){};

    void UpdateMRL();
    void UpdateMRL( int i_access_method );

protected:

    intf_thread_t *p_intf;

    virtual LRESULT WndProc( HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam,
                             PBOOL pbProcessed );

    HWND mrl_box;
    HWND mrl_label;
    HWND mrl_combo;
    HWND label;

    HWND notebook;

    HWND file_combo;
    HWND browse_button;
    HWND subsfile_checkbox;
    HWND subsfile_label;
    HWND subsfile_button;
    SubsFileDialog *subsfile_dialog;

    HWND net_radios[4];
    HWND net_label[4];

    HWND net_port_label[4];
    HWND net_ports[4];
    HWND hUpdown[4];
    int i_net_ports[4];

    HWND net_addrs_label[4];
    HWND net_addrs[4];
        
    int i_current_access_method;
    int i_method; /* Normal or for the stream dialog ? */
    int i_open_arg;
    int i_net_type;
        
    void FilePanel( HWND hwnd );
    void NetPanel( HWND hwnd );

    void OnSubsFileEnable();
    void OnSubsFileSettings( HWND hwnd );

    void OnPageChange();

    void OnFilePanelChange();
    void OnFileBrowse();
    void OnNetPanelChange( int event );
    void OnNetTypeChange( int event );
    void DisableNETCtrl();

    void OnOk();

    vector<string> mrl;
    vector<string> subsfile_mrl;
};

/* Subtitles File Dialog */
class SubsFileDialog: public CBaseWindow
{
public:
    /* Constructor */
    SubsFileDialog( intf_thread_t *_p_intf, HINSTANCE _hInst );
    virtual ~SubsFileDialog(){};

    vector<string> subsfile_mrl;

protected:
    friend class OpenDialog;

    intf_thread_t *p_intf;

    HWND file_box;
    HWND file_combo;
    HWND browse_button;

    HWND enc_box;
    HWND enc_label;
    HWND encoding_combo;

    HWND misc_box;
    HWND delay_label;
    HWND delay_edit;
    HWND delay_spinctrl;
    HWND fps_label;
    HWND fps_edit;
    HWND fps_spinctrl;

    virtual LRESULT WndProc( HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam,
                             PBOOL pbProcessed );

    /* Event handlers (these functions should _not_ be virtual) */
    void OnFileBrowse();
};

/* Playlist */
class Playlist : public CBaseWindow
{
public:
    /* Constructor */
    Playlist( intf_thread_t *_p_intf, HINSTANCE _hInst );
    virtual ~Playlist(){};

protected:

    bool b_need_update;
    vlc_mutex_t lock;

    int i_title_sorted;
    int i_author_sorted;

    intf_thread_t *p_intf;
    HWND hwndCB;        // Handle to the command bar (contains menu)
    HWND hwndTB;        // Handle to the toolbar.
    HWND hListView;

    void UpdatePlaylist();
    void Rebuild();
    void UpdateItem( int );
    LRESULT ProcessCustomDraw( LPARAM lParam );
    void HandlePopupMenu( HWND hwnd, POINT point);

    void DeleteItem( int item );

    void OnOpen();
    void OnSave();
    void OnAddFile();
    void OnAddMRL();

    void OnDeleteSelection();
    void OnInvertSelection();
    void OnEnableSelection();
    void OnDisableSelection();
    void OnSelectAll();
    void OnActivateItem( int i_item );
    void ShowInfos( HWND hwnd, int i_item );

    void OnUp();
    void OnDown();

    void OnRandom();
    void OnLoop();
    void OnRepeat();

    void OnSort( UINT event );
    void OnColSelect( int iSubItem );

    void OnPopupPlay();
    void OnPopupDel();
    void OnPopupEna();
    void OnPopupInfo( HWND hwnd );

    virtual LRESULT WndProc( HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam,
                             PBOOL pbProcessed );
};

/* Timer */
class Timer
{
public:
    /* Constructor */
    Timer( intf_thread_t *p_intf, HWND hwnd, Interface *_p_main_interface);
    virtual ~Timer();
    void Notify( void ); 

private:
    intf_thread_t *p_intf;
    Interface *p_main_interface;
    //Interface *p_main_interface;
    int i_old_playing_status;
    int i_old_rate;
};

/* Menus */
void RefreshSettingsMenu( intf_thread_t *_p_intf, HMENU hMenu );
void RefreshAudioMenu( intf_thread_t *_p_intf, HMENU hMenu );
void RefreshVideoMenu( intf_thread_t *_p_intf, HMENU hMenu );
void RefreshNavigMenu( intf_thread_t *_p_intf, HMENU hMenu );
void RefreshMenu( intf_thread_t *, vector<MenuItemExt*> *, HMENU, int,
                  char **, int *, int );
int wce_GetMenuItemCount( HMENU );
void CreateMenuItem( intf_thread_t *, vector<MenuItemExt*> *, HMENU, char *,
                     vlc_object_t *, int * );
HMENU CreateChoicesMenu( intf_thread_t *, vector<MenuItemExt*> *, char *, 
                         vlc_object_t *, int * );
void OnMenuEvent( intf_thread_t *, int );

/*****************************************************************************
 * A small helper class which encapsulate wxMenuitem with some other useful
 * things.
 *****************************************************************************/
class MenuItemExt
{
public:
    /* Constructor */
    MenuItemExt( intf_thread_t *_p_intf, int _id, char *_psz_var,
                 int _i_object_id, vlc_value_t _val, int _i_val_type );

    virtual ~MenuItemExt();

    int id;
    intf_thread_t *p_intf;
    char *psz_var;
    int  i_val_type;
    int  i_object_id;
    vlc_value_t val;

private:

};


/* Preferences Dialog */
/* Preferences Dialog */
class PrefsTreeCtrl;
class PrefsDialog: public CBaseWindow
{
public:
    /* Constructor */
    PrefsDialog( intf_thread_t *_p_intf, HINSTANCE _hInst );
    virtual ~PrefsDialog(){};

protected:

    intf_thread_t *p_intf;

    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( void );
    /*void OnCancel( UINT event );
    void OnSave( UINT event );
    void OnResetAll( UINT event );
    void OnAdvanced( UINT event );*/

    HWND save_button;
    HWND reset_button;
    HWND advanced_checkbox;
    HWND advanced_label;

    PrefsTreeCtrl *prefs_tree;

    virtual LRESULT WndProc( HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam,
                             PBOOL pbProcessed );
};

/*****************************************************************************
 * A small helper function for utf8 <-> unicode conversions
 *****************************************************************************/
#ifdef UNICODE
    static wchar_t pwsz_mbtow[2048];
    static char psz_wtomb[2048];
    static inline wchar_t *_FROMMB( const char *psz_in )
    {
        mbstowcs( pwsz_mbtow, psz_in, 2048 );
        pwsz_mbtow[2048] = 0;
        return pwsz_mbtow;
    }
    static inline char *_TOMB( const wchar_t *pwsz_in )
    {
        wcstombs( psz_wtomb, pwsz_in, 2048 );
        psz_wtomb[2048] = 0;
        return psz_wtomb;
    }
#else
#   define _FROMMB(a)
#   define _TOMB(a)
#endif

#if defined( ENABLE_NLS ) && defined( ENABLE_UTF8 )
#   define ISUTF8 1
#else // ENABLE_NLS && ENABLE_UTF8
#   define ISUTF8 0
#endif

#endif //WINCE_RESOURCE

#define IDD_ABOUT                       101
#define IDI_ICON1                       102
#define IDB_BITMAP1                     103
#define IDB_BITMAP2                     111
#define IDR_MENUBAR1                    113
#define IDR_ACCELERATOR1                116
#define IDD_FILEINFO                    118
#define IDD_DUMMY                       118
#define IDD_MESSAGES                    119
#define IDR_MENUBAR                     120
#define IDR_MENUBAR2                    121
#define IDD_PLAYLIST                    122
#define IDB_BITMAP3                     123
#define IDD_ITEMINFO                    124
#define IDR_DUMMYMENU                   126
#define IDCLEAR                         1001
#define IDSAVEAS                        1002
#define IDC_TEXTCTRL                    1004
#define IDC_CUSTOM1                     1012
#define IDS_MAIN_MENUITEM1              40001
#define IDS_TITLE                       40002
#define IDS_CLASSNAME                   40003
#define IDS_CAP_QUICKFILEOPEN           40006
#define IDS_CAP_VIEW                    40009
#define IDS_CAP_SETTINGS                40012
#define IDS_CAP_AUDIO                   40015
#define IDS_CAP_VIDEO                   40018
#define IDS_CAP_HELP                    40021
#define IDS_CAP_Navigation              40024
#define IDS_CAP_FILE                    40025
#define ID_COLOR_OPTIONS                40026
#define IDS_DYNAMENU                    40027
#define ID_FILE                         40028
#define IDS_BLACK                       40028
#define IDS_LTGRAY                      40029
#define ID_VIEW                         40030
#define IDS_DKGRAY                      40030
#define IDS_WHITE                       40031
#define ID_SETTINGS                     40032
#define ID_AUDIO                        40034
#define ID_EMPTY                        40034
#define ID_VIDEO                        40036
#define ID_NAVIGATION                   40038
#define IDM_FILE                        40042
#define IDM_VIEW                        40044
#define IDM_SETTINGS                    40046
#define IDM_AUDIO                       40048
#define IDM_VIDEO                       40050
#define IDM_NAVIGATION                  40053
#define ID_FILE_QUICK_OPEN              40056
#define ID_FILE_OPENFILE                40057
#define ID_FILE_QUICKOPEN               40058
#define ID_FILE_OPENNETWORKSTREAM       40059
#define ID_FILE_OPENNET                 40060
#define ID_FILE_EXIT                    40061
#define ID_VIEW_PLAYLIST                40063
#define ID_VIEW_MESSAGES                40064
#define ID_VIEW_MEDIAINFO               40065
#define ID_VIEW_STREAMINFO              40066
#define IDS_CAP_NAV                     40067
#define ID_FILE_ABOUT                   40069
#define ID_SETTINGS_PREF                40071
#define ID_SETTINGS_EXTEND              40072
#define IDS_CAP_XXX                     40084
#define IDM_MANAGE                      40087
#define IDM_SORT                        40088
#define IDM_SEL                         40089
#define ID_SORT_AUTHOR                  40091
#define ID_SORT_RAUTHOR                 40092
#define ID_SORT_SHUFFLE                 40095
#define ID_SEL_INVERT                   40096
#define ID_SEL_DELETE                   40097
#define ID_SEL_SELECTALL                40098
#define ID_SEL_ENABLE                   40100
#define ID_SEL_DISABLE                  40101
#define ID_SORT_TITLE                   40102
#define ID_SORT_RTITLE                  40103
#define ID_MANAGE_SIMPLEADD             40104
#define ID_MANAGE_OPENPL                40105
#define ID_MANAGE_ADDMRL                40106
#define ID_MANAGE_SAVEPL                40107
#define ID_MENUITEM40108                40108
#define IDS_CAP_MENUITEM40109           40110
#define IDS_STOP                        57601
#define StopStream_Event                57601
#define IDS_PLAY                        57602
#define PlayStream_Event                57602
#define PrevStream_Event                57603
#define NextStream_Event                57604
#define SlowStream_Event                57605
#define FastStream_Event                57606

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        128
#define _APS_NEXT_COMMAND_VALUE         40111
#define _APS_NEXT_CONTROL_VALUE         1013
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
