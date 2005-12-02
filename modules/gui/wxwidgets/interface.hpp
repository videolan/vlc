/*****************************************************************************
 * interface.hpp: Main interface headers
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

#ifndef _WXVLC_INTERFACE_H_
#define _WXVLC_INTERFACE_H_

#include "wxwidgets.hpp"
#include "input_manager.hpp"

#include <wx/dnd.h>
#include <wx/accel.h>
#include <wx/taskbar.h>
#include <wx/splitter.h>


namespace wxvlc
{
    class Timer;
    class Interface;

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

        wxBoxSizer       *main_sizer;
        wxSplitterWindow *splitter;

        wxPanel     *main_panel;
        wxBoxSizer  *panel_sizer;

        wxStatusBar *statusbar;

        InputManager *input_manager;

        wxControl  *volctrl;

    #ifdef wxHAS_TASK_BAR_ICON
        Systray     *p_systray;
    #endif

        wxWindow *video_window;

    private:
        void SetupHotkeys();
        void CreateOurMenuBar();
        void CreateOurToolBar();
        void CreateOurExtendedPanel();
        void Open( int i_access_method );

        void SetIntfMinSize();

        /* Event handlers (these functions should _not_ be virtual) */
        void OnExit( wxCommandEvent& event );
        void OnAbout( wxCommandEvent& event );

        void OnOpenFileSimple( wxCommandEvent& event );
        void OnOpenDir( wxCommandEvent& event );
        void OnOpenFile( wxCommandEvent& event );
        void OnOpenDisc( wxCommandEvent& event );
        void OnOpenNet( wxCommandEvent& event );
        void OnOpenSat( wxCommandEvent& event );

        void OnExtended( wxCommandEvent& event );
        void OnSmallPlaylist( wxCommandEvent& event );

        void OnBookmarks( wxCommandEvent& event );
        void OnShowDialog( wxCommandEvent& event );
        void OnPlayStream( wxCommandEvent& event );
        void OnStopStream( wxCommandEvent& event );
        void OnPrevStream( wxCommandEvent& event );
        void OnNextStream( wxCommandEvent& event );
        void OnSlowStream( wxCommandEvent& event );
        void OnFastStream( wxCommandEvent& event );

        void OnMenuOpen( wxMenuEvent& event );

    #if defined( __WXMSW__ ) || defined( __WXMAC__ )
        void OnContextMenu2(wxContextMenuEvent& event);
    #endif
        void OnContextMenu(wxMouseEvent& event);

        void OnControlEvent( wxCommandEvent& event );

        DECLARE_EVENT_TABLE();

        Timer *timer;
        intf_thread_t *p_intf;

        int i_old_playing_status;

        /* For auto-generated menus */
        wxMenu *p_settings_menu;
        wxMenu *p_audio_menu;
        wxMenu *p_video_menu;
        wxMenu *p_navig_menu;

        /* Extended panel */
        vlc_bool_t  b_extra;
        wxPanel     *extra_frame;

        /* Playlist panel */
        vlc_bool_t  b_playlist_manager;
        wxPanel     *playlist_manager;

        /* Utility dimensions */
        wxSize main_min_size;
        wxSize playlist_min_size;
        wxSize ext_min_size;
    };


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
};

void PopupMenu( intf_thread_t *, wxWindow *, const wxPoint& );
wxMenu *SettingsMenu( intf_thread_t *, wxWindow *, wxMenu * = NULL );
wxMenu *AudioMenu( intf_thread_t *, wxWindow *, wxMenu * = NULL );
wxMenu *VideoMenu( intf_thread_t *, wxWindow *, wxMenu * = NULL );
wxMenu *NavigMenu( intf_thread_t *, wxWindow *, wxMenu * = NULL );

#endif
