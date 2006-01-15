/*****************************************************************************
 * fileinfo.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

#include "dialogs/fileinfo.hpp"
#include "dialogs/infopanels.hpp"

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

static int ItemChanged( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );


/* IDs for the controls and the menu commands */
enum
{
    Close_Event
};

BEGIN_EVENT_TABLE(FileInfo, wxFrame)
    /* Button events */
    EVT_BUTTON(wxID_OK, FileInfo::OnButtonClose)

    /* Hide the window when the user closes the window */
    EVT_CLOSE(FileInfo::OnClose)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
FileInfo::FileInfo( intf_thread_t *_p_intf, wxWindow *p_parent ):
    wxFrame( p_parent, -1, wxU(_("Stream and media info")), wxDefaultPosition,
             wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    p_intf = _p_intf;
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                                 VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );

    /* Initializations */
    SetIcon( *p_intf->p_sys->p_icon );
    SetAutoLayout( TRUE );

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    wxNotebook *notebook = new wxNotebook( this, -1 );
#if (!wxCHECK_VERSION(2,5,2))
        wxNotebookSizer *notebook_sizer = new wxNotebookSizer( notebook );
#endif
    item_info = new ItemInfoPanel( p_intf, notebook, false );
    stats_info = new InputStatsInfoPanel( p_intf, notebook );

    notebook->AddPage( item_info, wxU(_("General") ), true );
    notebook->AddPage( stats_info, wxU(_("Statistics") ), false );

#if (!wxCHECK_VERSION(2,5,2))
    panel_sizer->Add( notebook_sizer, 1, wxEXPAND | wxALL, 5 );
#else
    panel_sizer->Add( notebook, 1, wxEXPAND | wxALL, 5 );
#endif

    panel_sizer->Layout();
    SetSizerAndFit( panel_sizer );


    if( p_playlist )
    {
        var_AddCallback( p_playlist, "item-change", ItemChanged, this );
        vlc_object_release( p_playlist );
    }

    last_update = 0L;
    b_need_update = VLC_TRUE;
    Update();
}

void FileInfo::Update()
{

    if( mdate() - last_update < 400000L ) return;
    last_update = mdate();

    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                                 VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( !p_playlist ) return;

    input_thread_t *p_input =
        (input_thread_t *)vlc_object_find( p_playlist, VLC_OBJECT_INPUT,
                                           FIND_CHILD );

    if( !p_input || p_input->b_dead || !p_input->input.p_item->psz_name )
    {
        item_info->Clear();
        stats_info->Clear();
        if ( p_input )
        {
            vlc_object_release(p_input);
        }
        vlc_object_release( p_playlist );
        return;
    }

    vlc_mutex_lock( &p_input->input.p_item->lock );
    if( b_need_update == VLC_TRUE )
    {
        item_info->Update( p_input->input.p_item );
    }
    stats_info->Update( p_input->input.p_item );
    vlc_mutex_unlock( &p_input->input.p_item->lock );

    vlc_object_release(p_input);
    vlc_object_release( p_playlist );
    b_need_update = VLC_FALSE;
    return;
}

FileInfo::~FileInfo()
{
}

void FileInfo::OnButtonClose( wxCommandEvent& event )
{
    wxCloseEvent cevent;
    OnClose(cevent);
}

void FileInfo::OnClose( wxCloseEvent& WXUNUSED(event) )
{
    Hide();
}

static int ItemChanged( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    FileInfo *p_fileinfo = (FileInfo *)param;
    p_fileinfo->b_need_update = VLC_TRUE;
    return VLC_SUCCESS;
}
