/*****************************************************************************
 * infopanels.cpp : Information panels (general info, stats, ...)
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id: iteminfo.cpp 13905 2006-01-12 23:10:04Z dionoea $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include "dialogs/infopanels.hpp"
#include <wx/combobox.h>
#include <wx/statline.h>

#ifndef wxRB_SINGLE
#   define wxRB_SINGLE 0
#endif

/*****************************************************************************
 * General info panel
 *****************************************************************************/
BEGIN_EVENT_TABLE( ItemInfoPanel, wxPanel )
END_EVENT_TABLE()

ItemInfoPanel::ItemInfoPanel( intf_thread_t *_p_intf,
                              wxWindow* _p_parent,
                              bool _b_modifiable ):
    wxPanel( _p_parent, -1 )
{
    /* Initializations */
    p_intf = _p_intf;
    p_parent = _p_parent;
    b_modifiable = _b_modifiable;

    SetAutoLayout( TRUE );

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *sizer = new wxFlexGridSizer(2,3,20);
    /* URI Textbox */
    wxStaticText *uri_static =
           new wxStaticText( this, -1, wxU(_("URI")) );
    sizer->Add( uri_static, 0 , wxALL , 5 );

    uri_text = new wxTextCtrl( this, -1,
            wxU(""), wxDefaultPosition, wxSize( 300, -1 ),
            wxTE_PROCESS_ENTER );
    sizer->Add( uri_text, 1 ,  wxALL , 5 );

    /* Name Textbox */
    wxStaticText *name_static =
           new wxStaticText(  this, -1, wxU(_("Name")) );
    sizer->Add( name_static, 0 , wxALL , 5  );

    name_text = new wxTextCtrl( this, -1,
            wxU(""), wxDefaultPosition, wxSize( 300, -1 ),
            wxTE_PROCESS_ENTER );
    sizer->Add( name_text, 1 , wxALL , 5 );

    /* Treeview */
    info_tree = new wxTreeCtrl( this, -1, wxDefaultPosition,
                                wxSize(220,200),
                                wxSUNKEN_BORDER |wxTR_HAS_BUTTONS |
                                wxTR_HIDE_ROOT );
    info_root = info_tree->AddRoot( wxU( "" ) );

    sizer->Layout();
    panel_sizer->Add( sizer, 0, wxEXPAND, 5 );
    panel_sizer->Add( info_tree, 0, wxEXPAND, 5 );
    panel_sizer->Layout();
    SetSizerAndFit( panel_sizer );
}

ItemInfoPanel::~ItemInfoPanel()
{
}

void ItemInfoPanel::Update( input_item_t *p_item )
{
    /* Rebuild the tree */
    Clear();

    uri_text->SetValue( wxU( p_item->psz_uri ) );
    name_text->SetValue( wxU( p_item->psz_name ) );

    for( int i = 0; i< p_item->i_categories ; i++)
    {
        wxTreeItemId cat = info_tree->AppendItem( info_root,
                            wxU( p_item->pp_categories[i]->psz_name) );

        for( int j = 0 ; j < p_item->pp_categories[i]->i_infos ; j++ )
        {
           info_tree->AppendItem( cat , (wxString)
               wxU(p_item->pp_categories[i]->pp_infos[j]->psz_name) +
               wxT(": ") +
               wxU(p_item->pp_categories[i]->pp_infos[j]->psz_value) );
        }

        info_tree->Expand( cat );
    }
}

void ItemInfoPanel::Clear()
{
    info_tree->DeleteChildren( info_root );
}

void ItemInfoPanel::OnOk( )
{
}

void ItemInfoPanel::OnCancel( )
{
}

/*****************************************************************************
 * Statistics info panel
 *****************************************************************************/
BEGIN_EVENT_TABLE( InputStatsInfoPanel, wxPanel )
END_EVENT_TABLE()

InputStatsInfoPanel::InputStatsInfoPanel( intf_thread_t *_p_intf,
                                          wxWindow* _p_parent ):
    wxPanel( _p_parent, -1 )
{
    /* Initializations */
    p_intf = _p_intf;
    p_parent = _p_parent;

    SetAutoLayout( TRUE );

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *sizer = new wxFlexGridSizer( 2,2,20 );

    /* Input */
    wxStaticBox *input_box = new wxStaticBox( this, -1,
                                              wxU( _("Input") ) );
    wxStaticBoxSizer *input_bsizer = new wxStaticBoxSizer( input_box,
                                                          wxVERTICAL );
    wxFlexGridSizer *input_sizer = new wxFlexGridSizer( 2,2, 20 );

#define INPUT_ADD(txt,widget,dflt) \
    { input_sizer->Add ( new wxStaticText( this, -1, wxU(_( txt ) ) ) ); \
      widget = new wxStaticText( this, -1, wxU( dflt ) );                \
      input_sizer->Add( widget );                                        \
    }

    INPUT_ADD( "Read at media", read_bytes_text, "0" );
    INPUT_ADD( "Input bitrate", input_bitrate_text, "0" );

    INPUT_ADD( "Demuxed", demux_bytes_text ,"0");
    INPUT_ADD( "Stream bitrate", demux_bitrate_text, "0" );

    input_sizer->Layout();
    input_bsizer->Add( input_sizer, 0, wxALL | wxGROW, 5 );
    input_bsizer->Layout();
    sizer->Add( input_bsizer, 0, wxALL|wxGROW, 5 );

   /* Vout */
    wxStaticBox *video_box = new wxStaticBox( this, -1,
                                              wxU( _("Video" ) ) );
    wxStaticBoxSizer *video_bsizer = new wxStaticBoxSizer( video_box,
                                                           wxVERTICAL );
    wxFlexGridSizer *video_sizer = new wxFlexGridSizer( 2,3, 20 );

#define VIDEO_ADD(txt,widget,dflt) \
    { video_sizer->Add ( new wxStaticText( this, -1, wxU(_( txt ) ) ) ); \
      widget = new wxStaticText( this, -1, wxU( dflt ) );                \
      video_sizer->Add( widget );                                        \
    }
    VIDEO_ADD( "Decoded blocks", video_decoded_text, "0" );
    VIDEO_ADD( "Displayed frames", displayed_text, "0" );
    VIDEO_ADD( "Lost frames", lost_frames_text, "0" );


    video_sizer->Layout();
    video_bsizer->Add( video_sizer, 0, wxALL | wxGROW, 5 );
    video_bsizer->Layout();
    sizer->Add( video_bsizer , 0, wxALL| wxGROW, 5 );

    panel_sizer->Add( sizer, 0, wxEXPAND, 5 );
    panel_sizer->Layout();
    SetSizerAndFit( panel_sizer );

}

InputStatsInfoPanel::~InputStatsInfoPanel()
{
}

void InputStatsInfoPanel::Update( input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->p_stats->lock );

    /* Input */
#define UPDATE( widget,format, calc... )   \
{                                       \
    wxString formatted;                 \
    formatted.Printf(  wxString( wxT(format) ), ## calc ); \
    widget->SetLabel( formatted );                      \
}
    UPDATE( read_bytes_text, "%.0f kB",(float)(p_item->p_stats->i_read_bytes)/1000 );
    UPDATE( input_bitrate_text, "%.0f kB/s", (float)(p_item->p_stats->f_input_bitrate)*1000 );
    UPDATE( demux_bytes_text, "%.0f kB", (float)(p_item->p_stats->i_demux_read_bytes)/1000 );
    UPDATE( demux_bitrate_text, "%.0f kB/s",  (float)(p_item->p_stats->f_demux_bitrate)*1000 );

    /* Video */
    UPDATE( video_decoded_text, "%i", p_item->p_stats->i_decoded_video );
    UPDATE( displayed_text, "%i", p_item->p_stats->i_displayed_pictures );
    UPDATE( lost_frames_text, "%i", p_item->p_stats->i_lost_pictures );

    vlc_mutex_unlock( &p_item->p_stats->lock );
}

void InputStatsInfoPanel::Clear()
{}

void InputStatsInfoPanel::OnOk( )
{}

void InputStatsInfoPanel::OnCancel( )
{}

