/*****************************************************************************
 * bookmarks.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "wxwindows.h"

/* Callback prototype */
static int PlaylistChanged( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Class declaration.
 *****************************************************************************/
class BookmarksDialog: public wxFrame
{
public:
    /* Constructor */
    BookmarksDialog( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~BookmarksDialog();

    bool Show( bool );

private:

    void Update();

    /* Event handlers (these functions should _not_ be virtual) */
    void OnClose( wxCommandEvent& event );
    void OnAdd( wxCommandEvent& event );
    void OnDel( wxCommandEvent& event );
    void OnClear( wxCommandEvent& event );
    void OnActivateItem( wxListEvent& event );
    void OnUpdate( wxCommandEvent &event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxWindow *p_parent;

    wxListView *list_ctrl;
};

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    /* menu items */
    ButtonAdd_Event = wxID_HIGHEST + 1,
    ButtonDel_Event,
    ButtonClear_Event
};

DEFINE_LOCAL_EVENT_TYPE( wxEVT_BOOKMARKS );

BEGIN_EVENT_TABLE(BookmarksDialog, wxFrame)
    /* Hide the window when the user closes the window */
    EVT_CLOSE(BookmarksDialog::OnClose )
    EVT_BUTTON( ButtonAdd_Event, BookmarksDialog::OnAdd )
    EVT_BUTTON( ButtonDel_Event, BookmarksDialog::OnDel )
    EVT_BUTTON( ButtonClear_Event, BookmarksDialog::OnClear )

    EVT_LIST_ITEM_ACTIVATED( -1, BookmarksDialog::OnActivateItem )

    EVT_COMMAND( -1, wxEVT_BOOKMARKS, BookmarksDialog::OnUpdate )
END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
BookmarksDialog::BookmarksDialog( intf_thread_t *_p_intf, wxWindow *p_parent )
  : wxFrame( p_parent->GetParent() ? p_parent->GetParent() : p_parent,
             -1, wxU(_("Bookmarks")),
             !p_parent->GetParent() ? wxDefaultPosition :
               wxPoint( p_parent->GetParent()->GetRect().GetX(),
                        p_parent->GetParent()->GetRect().GetY() +
                        p_parent->GetParent()->GetRect().GetHeight() + 40 ),
             wxSize( 500, -1 ),
             wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT )
{
    /* Initializations */
    p_intf = _p_intf;
    SetIcon( *p_intf->p_sys->p_icon );

    wxPanel *main_panel = new wxPanel( this, -1 );
    wxBoxSizer *main_sizer = new wxBoxSizer( wxHORIZONTAL );

    wxBoxSizer *sizer = new wxBoxSizer( wxHORIZONTAL );

    wxPanel *panel = new wxPanel( main_panel, -1 );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    wxButton *button_add =
        new wxButton( panel, ButtonAdd_Event, wxU(_("Add")) );
    wxButton *button_del =
        new wxButton( panel, ButtonDel_Event, wxU(_("Remove")) );
    wxButton *button_clear =
        new wxButton( panel, ButtonClear_Event, wxU(_("Clear")) );
    panel_sizer->Add( button_add, 0, wxEXPAND );
    panel_sizer->Add( button_del, 0, wxEXPAND );
    panel_sizer->Add( button_clear, 0, wxEXPAND );
    panel->SetSizerAndFit( panel_sizer );

    list_ctrl = new wxListView( main_panel, -1,
                                wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxSUNKEN_BORDER |
                                wxLC_SINGLE_SEL );
    list_ctrl->InsertColumn( 0, wxU(_("Description")) );
    list_ctrl->SetColumnWidth( 0, 240 );
    list_ctrl->InsertColumn( 1, wxU(_("Size offset")) );
    list_ctrl->InsertColumn( 2, wxU(_("Time offset")) );

    sizer->Add( panel, 0, wxEXPAND | wxALL, 1 );
    sizer->Add( list_ctrl, 1, wxEXPAND | wxALL, 1 );
    main_panel->SetSizer( sizer );

    main_sizer->Add( main_panel, 1, wxEXPAND );
    SetSizer( main_sizer );

    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist )
    {
       /* Some global changes happened -> Rebuild all */
       var_AddCallback( p_playlist, "playlist-current",
                        PlaylistChanged, this );
       vlc_object_release( p_playlist );
    }
}

BookmarksDialog::~BookmarksDialog()
{
    playlist_t *p_playlist =
        (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                       FIND_ANYWHERE );
    if( p_playlist )
    {
       var_DelCallback( p_playlist, "playlist-current",
                        PlaylistChanged, this );
       vlc_object_release( p_playlist );
    }
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
wxWindow *BookmarksDialog( intf_thread_t *p_intf, wxWindow *p_parent )
{
    return new BookmarksDialog::BookmarksDialog( p_intf, p_parent );
}

void BookmarksDialog::Update()
{
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( !p_input ) return;

    seekpoint_t **pp_bookmarks;
    int i_bookmarks;

    list_ctrl->DeleteAllItems();
    if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks,
                       &i_bookmarks ) != VLC_SUCCESS )
    {
        vlc_object_release( p_input );
        return;
    }

    for( int i = 0; i < i_bookmarks; i++ )
    {
        list_ctrl->InsertItem( i, wxL2U( pp_bookmarks[i]->psz_name ) );
        list_ctrl->SetItem( i, 1, wxString::Format(wxT("%i"),
                            pp_bookmarks[i]->i_byte_offset ) );
        list_ctrl->SetItem( i, 2, wxString::Format(wxT("%i"),
                            pp_bookmarks[i]->i_time_offset/1000000 ) );
    }

    vlc_object_release( p_input );
}

bool BookmarksDialog::Show( bool show )
{
    Update();
    return wxFrame::Show( show );
}

void BookmarksDialog::OnClose( wxCommandEvent& event )
{
    Hide();
}

void BookmarksDialog::OnAdd( wxCommandEvent& event )
{
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( !p_input ) return;

    seekpoint_t bookmark;
    vlc_value_t pos;
    var_Get( p_input, "position", &pos );
    bookmark.psz_name = NULL;
    bookmark.i_byte_offset =
      (int64_t)((double)pos.f_float * p_input->stream.p_selected_area->i_size);
    var_Get( p_input, "time", &pos );
    bookmark.i_time_offset = pos.i_time;
    input_Control( p_input, INPUT_ADD_BOOKMARK, &bookmark );

    vlc_object_release( p_input );

    Update();
}

void BookmarksDialog::OnDel( wxCommandEvent& event )
{
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( !p_input ) return;

    int i_focused = list_ctrl->GetFocusedItem();
    if( i_focused >= 0 )
    {
        input_Control( p_input, INPUT_DEL_BOOKMARK, i_focused );
    }

    vlc_object_release( p_input );

    Update();
}

void BookmarksDialog::OnClear( wxCommandEvent& event )
{
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( !p_input ) return;

    input_Control( p_input, INPUT_CLEAR_BOOKMARKS );

    vlc_object_release( p_input );

    Update();
}

void BookmarksDialog::OnActivateItem( wxListEvent& event )
{
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( !p_input ) return;

    input_Control( p_input, INPUT_SET_BOOKMARK, event.GetIndex() );

    vlc_object_release( p_input );
}

void BookmarksDialog::OnUpdate( wxCommandEvent &event )
{
    Update();
}

/*****************************************************************************
 * PlaylistChanged: callback triggered by the intf-change playlist variable
 *  We don't rebuild the playlist directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                            vlc_value_t oval, vlc_value_t nval, void *param )
{
    BookmarksDialog::BookmarksDialog *p_dialog =
        (BookmarksDialog::BookmarksDialog *)param;

    wxCommandEvent bookmarks_event( wxEVT_BOOKMARKS, 0 );
    p_dialog->AddPendingEvent( bookmarks_event );

    return VLC_SUCCESS;
}
