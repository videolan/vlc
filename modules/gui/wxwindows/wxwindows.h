/*****************************************************************************
 * wxwindows.h: private wxWindows interface description
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: wxwindows.h,v 1.3 2002/11/23 14:28:51 gbazin Exp $
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
#include <wx/dnd.h>

class Playlist;

#define SLIDER_MAX_POS 10000

/*****************************************************************************
 * intf_sys_t: description and status of Gtk+ interface
 *****************************************************************************/
struct intf_sys_t
{
    /* the wx parent window */
    wxWindow            *p_wxwindow;

    /* secondary windows */
    Playlist            *p_playlist_window;

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
    int                 i_part;                           /* current chapter */
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
};

/* Main Interface */
class Interface: public wxFrame
{
public:
    /* Constructor */
    Interface( intf_thread_t *p_intf );
    virtual ~Interface();

    wxBoxSizer  *frame_sizer;
    wxStatusBar *statusbar;

    wxSlider    *slider;
    wxWindow    *slider_frame;
    wxStaticBox *slider_box;

private:
    void CreateOurMenuBar();
    void CreateOurToolBar();
    void CreateOurSlider();

    /* Event handlers (these functions should _not_ be virtual) */
    void OnExit( wxCommandEvent& event );
    void OnAbout( wxCommandEvent& event );
    void OnPlaylist( wxCommandEvent& event );
    void OnOpenFile( wxCommandEvent& event );
    void OnPlayStream( wxCommandEvent& event );
    void OnStopStream( wxCommandEvent& event );
    void OnPauseStream( wxCommandEvent& event );
    void OnSliderUpdate( wxScrollEvent& event );
    void OnPrevStream( wxCommandEvent& event );
    void OnNextStream( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    Timer *timer;
    intf_thread_t *p_intf;
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

    void DeleteItem( int item );
    intf_thread_t *p_intf;
    Interface *p_main_interface;
    wxListView *listview;
    wxButton *ok_button;
};

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
