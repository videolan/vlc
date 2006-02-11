/*****************************************************************************
 * bookmarks.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "dialogs/bookmarks.hpp"

#include "wizard.hpp"

/* Callback prototype */
static int PlaylistChanged( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    /* menu items */
    ButtonAdd_Event = wxID_HIGHEST + 1,
    ButtonDel_Event,
    ButtonClear_Event,
    ButtonExtract_Event,
    ButtonEdit_Event
};

DEFINE_LOCAL_EVENT_TYPE( wxEVT_BOOKMARKS );

BEGIN_EVENT_TABLE(BookmarksDialog, wxFrame)
    /* Hide the window when the user closes the window */
    EVT_CLOSE(BookmarksDialog::OnClose )
    EVT_BUTTON( wxID_ADD, BookmarksDialog::OnAdd )
    EVT_BUTTON( wxID_DELETE, BookmarksDialog::OnDel )
    EVT_BUTTON( wxID_CLEAR, BookmarksDialog::OnClear )
    EVT_BUTTON( ButtonExtract_Event, BookmarksDialog::OnExtract )
    EVT_BUTTON( ButtonEdit_Event, BookmarksDialog::OnEdit )

    EVT_LIST_ITEM_ACTIVATED( -1, BookmarksDialog::OnActivateItem )

    EVT_COMMAND( -1, wxEVT_BOOKMARKS, BookmarksDialog::OnUpdate )
END_EVENT_TABLE()

BEGIN_EVENT_TABLE( BookmarkEditDialog, wxDialog)
    EVT_BUTTON( wxID_OK, BookmarkEditDialog::OnOK)
END_EVENT_TABLE()

/****************************************************************************
 * BookmarkEditDialog
 ***************************************************************************/
BookmarkEditDialog::BookmarkEditDialog( intf_thread_t *_p_intf,
           wxWindow *_p_parent, seekpoint_t *_p_seekpoint ):wxDialog(
            _p_parent, -1, wxU(_("Edit bookmark")), wxDefaultPosition,
            wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    p_seekpoint = _p_seekpoint;
    SetIcon( *p_intf->p_sys->p_icon );

    /* Create a panel to put everything in*/
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer * sizer = new wxFlexGridSizer( 2 , 3 , 1 );
    name_text = new wxTextCtrl( this, -1, wxU( p_seekpoint->psz_name ?
                                               p_seekpoint->psz_name : "" ),
                                wxDefaultPosition, wxSize( 100, 20) );
    time_text = new wxTextCtrl( this, -1, wxString::Format(wxT("%d"),
                                (int)(p_seekpoint->i_time_offset / 1000000) ),
                                wxDefaultPosition, wxSize( 100, 20) );
    bytes_text = new wxTextCtrl( this, -1, wxString::Format(wxT("%d"),
                                (int)p_seekpoint->i_byte_offset ),
                                wxDefaultPosition, wxSize( 100, 20) );

    sizer->Add( new wxStaticText( this, -1, wxU(_("Name") ) ), 0, wxLEFT, 5 );
    sizer->Add( name_text, 0, wxEXPAND|wxRIGHT , 5 );
    sizer->Add( new wxStaticText( this, -1, wxU(_("Time") ) ), 0, wxLEFT, 5 );
    sizer->Add( time_text , 0, wxEXPAND|wxRIGHT , 5);
    sizer->Add( new wxStaticText( this, -1, wxU(_("Bytes") ) ), 0, wxLEFT, 5 );
    sizer->Add( bytes_text, 0, wxEXPAND|wxRIGHT, 5);

    wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxButton *ok_button = new wxButton( this, wxID_OK );
    ok_button->SetDefault();
    button_sizer->Add( ok_button );
    button_sizer->Add( new wxButton( this, wxID_CANCEL ) );

    panel_sizer->Add( sizer, 0, wxEXPAND | wxTOP|wxBOTTOM, 5 );
    panel_sizer->Add( button_sizer, 0, wxEXPAND | wxBOTTOM, 5 );
    panel_sizer->Layout();
    SetSizerAndFit( panel_sizer );
}

BookmarkEditDialog::~BookmarkEditDialog()
{
}
void BookmarkEditDialog::OnOK( wxCommandEvent &event )
{
    if( p_seekpoint->psz_name ) free( p_seekpoint->psz_name );
    p_seekpoint->psz_name = strdup( name_text->GetValue().mb_str() );
    p_seekpoint->i_byte_offset = atoi( bytes_text->GetValue().mb_str() );
    p_seekpoint->i_time_offset =  1000000 *
                                  atoll( time_text->GetValue().mb_str() ) ;
    EndModal( wxID_OK );
}

void BookmarkEditDialog::OnCancel( wxCommandEvent &event )
{
    EndModal( wxID_CANCEL );
}

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
        new wxButton( panel, wxID_ADD );
    wxButton *button_del =
        new wxButton( panel, wxID_DELETE );
    wxButton *button_clear =
        new wxButton( panel, wxID_CLEAR );
    wxButton *button_edit =
        new wxButton( panel, ButtonEdit_Event, wxU(_("Edit")) );
    wxButton *button_extract =
        new wxButton( panel, ButtonExtract_Event, wxU(_("Extract")) );

#define ADD_TEXT "Adds a bookmark at the current position in the stream"
#define REMOVE_TEXT "Removes the selected bookmarks"
#define CLEAR_TEXT "Removes all the bookmarks for that stream"
#define EDIT_TEXT "Edit the properties of a bookmark"
#define EXTRACT_TEXT "If you select two or more bookmarks, this will " \
               "launch the streaming/transcoding wizard to allow you to " \
              "stream or save the part of the stream between these bookmarks"
    button_add->SetToolTip(  wxU(_( ADD_TEXT ) ) );
    button_del->SetToolTip(  wxU(_( REMOVE_TEXT ) ) );
    button_clear->SetToolTip(  wxU(_( CLEAR_TEXT ) ) );
    button_edit->SetToolTip(  wxU(_( EDIT_TEXT ) ) );
    button_extract->SetToolTip( wxU(_( EXTRACT_TEXT ) ) );

    panel_sizer->Add( button_add, 0, wxEXPAND );
    panel_sizer->Add( button_del, 0, wxEXPAND );
    panel_sizer->Add( button_clear, 0, wxEXPAND );

    panel_sizer->Add( button_edit, 0, wxEXPAND );
    panel_sizer->Add( 0, 0, 1 );
    panel_sizer->Add( button_extract, 0, wxEXPAND );

    panel->SetSizerAndFit( panel_sizer );

    list_ctrl = new wxListView( main_panel, -1,
                                wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxSUNKEN_BORDER );
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
//wxFrame *BookmarksDialog( intf_thread_t *p_intf, wxWindow *p_parent )
//{
//    return new class BookmarksDialog( p_intf, p_parent );
//}

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
        /* FIXME: see if we can use the 64 bits integer format string */
        list_ctrl->SetItem( i, 1, wxString::Format(wxT("%d"),
                            (int)(pp_bookmarks[i]->i_byte_offset) ) );
        list_ctrl->SetItem( i, 2, wxString::Format(wxT("%d"),
                            (int)(pp_bookmarks[i]->i_time_offset / 1000000) ) );
    }

    vlc_object_release( p_input );
}

bool BookmarksDialog::Show( bool show )
{
    Update();
    return wxFrame::Show( show );
}

void BookmarksDialog::OnClose( wxCloseEvent& event )
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
    bookmark.psz_name = NULL;
    bookmark.i_byte_offset = 0;
    bookmark.i_time_offset = 0;

    var_Get( p_input, "position", &pos );
    bookmark.psz_name = NULL;
    input_Control( p_input, INPUT_GET_BYTE_POSITION, &bookmark.i_byte_offset );
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

void BookmarksDialog::OnExtract( wxCommandEvent& event )
{
    long i_first = list_ctrl->GetNextItem( -1, wxLIST_NEXT_ALL,
                                          wxLIST_STATE_SELECTED );
    long i_second = list_ctrl->GetNextItem( i_first, wxLIST_NEXT_ALL,
                                          wxLIST_STATE_SELECTED );

    if( i_first == -1 || i_second == -1 )
    {
        wxMessageBox( wxU(_("You must select two bookmarks") ),
                      wxU(_("Invalid selection") ), wxICON_WARNING | wxOK,
                      this );
        return;
    }
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( !p_input )
    {
        wxMessageBox( wxU(_("The stream must be playing or paused for "
                            "bookmarks to work" ) ), wxU(_("No input found")),
                            wxICON_WARNING | wxOK,
                            this );
        return;
    }

    seekpoint_t **pp_bookmarks;
    int i_bookmarks ;

    if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks,
                       &i_bookmarks ) != VLC_SUCCESS )
    {
        vlc_object_release( p_input );
        return;
    }

    if( i_first < i_bookmarks && i_second <= i_bookmarks )
    {
        WizardDialog *p_wizard_dialog = new WizardDialog( p_intf, this,
                               p_input->input.p_item->psz_uri,
                               pp_bookmarks[i_first]->i_time_offset/1000000,
                               pp_bookmarks[i_second]->i_time_offset/1000000 );
        vlc_object_release( p_input );
        if( p_wizard_dialog )
        {
            p_wizard_dialog->Run();
            delete p_wizard_dialog;
        }
    }
    else
    {
        vlc_object_release( p_input );
    }
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

void BookmarksDialog::OnEdit( wxCommandEvent& event )
{
    input_thread_t *p_old_input;
    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );
    if( !p_input ) return;


    seekpoint_t **pp_bookmarks;
    int i_bookmarks;

    if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks,
                       &i_bookmarks ) != VLC_SUCCESS )
    {
        vlc_object_release( p_input );
        return;
    }
    p_old_input = p_input;
    vlc_object_release( p_input );

    long i_first = list_ctrl->GetNextItem( -1, wxLIST_NEXT_ALL,
                                               wxLIST_STATE_SELECTED );

    if( i_first > -1 && i_first <= i_bookmarks )
    {
        BookmarkEditDialog *p_bmk_edit;
        p_bmk_edit = new BookmarkEditDialog( p_intf, this,
                                             pp_bookmarks[i_first]);

        if( p_bmk_edit->ShowModal() == wxID_OK )
        {
            p_input =(input_thread_t *)vlc_object_find( p_intf,
                            VLC_OBJECT_INPUT, FIND_ANYWHERE );
           if( !p_input )
           {
                wxMessageBox( wxU( _("No input found. The stream must be "
                                  "playing or paused for bookmarks to work.") ),
                              wxU( _("No input") ), wxICON_WARNING | wxOK,
                              this );
                return;
           }
           if( p_old_input != p_input )
           {
                wxMessageBox( wxU( _("Input has changed, unable to save "
                                  "bookmark. Use \"pause\" while editing "
                                  "bookmarks to keep the same input.") ),
                              wxU( _("Input has changed ") ),
                              wxICON_WARNING | wxOK, this );
                vlc_object_release( p_input );
                return;

           }
           if( input_Control( p_input, INPUT_CHANGE_BOOKMARK,
                              p_bmk_edit->p_seekpoint, i_first ) !=
               VLC_SUCCESS )
           {
               vlc_object_release( p_input );
               return;
           }
           Update();
           vlc_object_release( p_input );
        }
    }
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
    class BookmarksDialog *p_dialog = (class BookmarksDialog *)param;

    wxCommandEvent bookmarks_event( wxEVT_BOOKMARKS, 0 );
    p_dialog->AddPendingEvent( bookmarks_event );

    return VLC_SUCCESS;
}
