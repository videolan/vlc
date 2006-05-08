/*****************************************************************************
 * infopanels.cpp : Information panels (general info, stats, ...)
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id$
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

#include <vlc_meta.h>

#ifndef wxRB_SINGLE
#   define wxRB_SINGLE 0
#endif

/*****************************************************************************
 * General info (URI, name, metadata)
 *****************************************************************************/
BEGIN_EVENT_TABLE( MetaDataPanel, wxPanel )
END_EVENT_TABLE()

MetaDataPanel::MetaDataPanel( intf_thread_t *_p_intf,
                              wxWindow* _p_parent,
                              bool _b_modifiable ):
    wxPanel( _p_parent, -1 )
{
    int flags= wxTE_PROCESS_ENTER;
    /* Initializations */
    p_intf = _p_intf;
    p_parent = _p_parent;
    b_modifiable = _b_modifiable;

    SetAutoLayout( TRUE );

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *sizer = new wxFlexGridSizer(2,8,20);
    sizer->AddGrowableCol(1);

    if( !b_modifiable )
        flags |= wxTE_READONLY;

    /* URI Textbox */
    wxStaticText *uri_static =
           new wxStaticText( this, -1, wxU(_("URI")) );
    sizer->Add( uri_static, 0 , wxALL , 0 );

    uri_text = new wxTextCtrl( this, -1,
            wxU(""), wxDefaultPosition, wxSize( 300, -1 ), flags );
    sizer->Add( uri_text, 1 ,  wxALL|wxEXPAND , 0 );

    /* Name Textbox */
    wxStaticText *name_static =
           new wxStaticText(  this, -1, wxU(_("Name")) );
    sizer->Add( name_static, 0 , wxALL , 0  );

    name_text = new wxTextCtrl( this, -1,
            wxU(""), wxDefaultPosition, wxSize( 300, -1 ), flags );
    sizer->Add( name_text, 1 , wxALL|wxEXPAND , 0 );
    sizer->Layout();

    /* Metadata */
    wxFlexGridSizer *meta_sizer = new wxFlexGridSizer(2,11,20);
    meta_sizer->AddGrowableCol(1);

#define ADD_META( string, widget ) {                                        \
        meta_sizer->Add( new wxStaticText( this, -1, wxU(_(string) ) ),1,   \
                         wxTOP|wxRIGHT|wxLEFT|wxEXPAND, 0 );                \
        widget = new wxStaticText( this, -1, wxU( "" ) );                   \
        meta_sizer->Add( widget, 1, wxTOP|wxRIGHT|wxLEFT|wxEXPAND, 0 ); }

    ADD_META( VLC_META_ARTIST, artist_text );
    ADD_META( VLC_META_GENRE, genre_text );
    ADD_META( VLC_META_COPYRIGHT, copyright_text );
    ADD_META( VLC_META_COLLECTION, collection_text );
    ADD_META( VLC_META_SEQ_NUM, seqnum_text );
    ADD_META( VLC_META_DESCRIPTION, description_text );
    ADD_META( VLC_META_RATING, rating_text );
    ADD_META( VLC_META_DATE, date_text );
    ADD_META( VLC_META_LANGUAGE, language_text );
    ADD_META( VLC_META_NOW_PLAYING, nowplaying_text );
    ADD_META( VLC_META_PUBLISHER, publisher_text );

    meta_sizer->Layout();

    panel_sizer->Add( sizer, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Add( meta_sizer, 0, wxEXPAND | wxALL, 5 );
    panel_sizer->Layout();
    SetSizerAndFit( panel_sizer );
}

MetaDataPanel::~MetaDataPanel()
{
}

void MetaDataPanel::Update( input_item_t *p_item )
{
    /* Rebuild the tree */
    Clear();

    uri_text->SetValue( wxU( p_item->psz_uri ) );
    name_text->SetValue( wxU( p_item->psz_name ) );

#define UPDATE_META( meta, widget ) {                                       \
    char *psz_meta = vlc_input_item_GetInfo( p_item, _(VLC_META_INFO_CAT),  \
                                            _(meta) );                      \
    if( psz_meta != NULL && *psz_meta)                                      \
    {                                                                       \
        widget->SetLabel( wxU( psz_meta ) );                                \
    }                                                                       \
    else { widget->SetLabel( wxU( "-" ) ); }                                \
    }

    UPDATE_META( VLC_META_ARTIST, artist_text );
    UPDATE_META( VLC_META_GENRE, genre_text );
    UPDATE_META( VLC_META_COPYRIGHT, copyright_text );
    UPDATE_META( VLC_META_COLLECTION, collection_text );
    UPDATE_META( VLC_META_SEQ_NUM, seqnum_text );
    UPDATE_META( VLC_META_DESCRIPTION, description_text );
    UPDATE_META( VLC_META_RATING, rating_text );
    UPDATE_META( VLC_META_DATE, date_text );
    UPDATE_META( VLC_META_LANGUAGE, language_text );
    UPDATE_META( VLC_META_NOW_PLAYING, nowplaying_text );
    UPDATE_META( VLC_META_PUBLISHER, publisher_text );

#undef UPDATE_META
}

char* MetaDataPanel::GetURI( )
{
    return  strdup( uri_text->GetLineText(0).mb_str() );
}

char* MetaDataPanel::GetName( )
{
    return  strdup( name_text->GetLineText(0).mb_str() );
}

void MetaDataPanel::Clear()
{
}

void MetaDataPanel::OnOk( )
{
}

void MetaDataPanel::OnCancel( )
{
}


/*****************************************************************************
 * General info panel
 *****************************************************************************/
BEGIN_EVENT_TABLE( AdvancedInfoPanel, wxPanel )
END_EVENT_TABLE()

AdvancedInfoPanel::AdvancedInfoPanel( intf_thread_t *_p_intf,
                                      wxWindow* _p_parent ):
    wxPanel( _p_parent, -1 )
{
    /* Initializations */
    p_intf = _p_intf;
    p_parent = _p_parent;

    SetAutoLayout( TRUE );

    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *sizer = new wxFlexGridSizer(2,8,20);
    sizer->AddGrowableCol(1);

    /* Treeview */
    info_tree = new wxTreeCtrl( this, -1, wxDefaultPosition,
                                wxSize(220,200),
                                wxSUNKEN_BORDER |wxTR_HAS_BUTTONS |
                                wxTR_HIDE_ROOT );
    info_root = info_tree->AddRoot( wxU( "" ) );

    panel_sizer->Add( info_tree, 1, wxEXPAND | wxALL, 5 );
    panel_sizer->Layout();
    SetSizerAndFit( panel_sizer );
}

AdvancedInfoPanel::~AdvancedInfoPanel()
{
}

void AdvancedInfoPanel::Update( input_item_t *p_item )
{
    /* Rebuild the tree */
    Clear();

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

void AdvancedInfoPanel::Clear()
{
    info_tree->DeleteChildren( info_root );
}

void AdvancedInfoPanel::OnOk( )
{
}

void AdvancedInfoPanel::OnCancel( )
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

    panel_sizer = new wxBoxSizer( wxVERTICAL );

    sizer = new wxFlexGridSizer( 2,2,20 );

    /* Input */
    wxStaticBox *input_box = new wxStaticBox( this, -1,
                                              wxU( _("Input") ) );
    input_box->SetAutoLayout( TRUE );
    input_bsizer = new wxStaticBoxSizer( input_box, wxVERTICAL );
    input_sizer = new wxFlexGridSizer( 4, 3, 2, 20 );

#define INPUT_ADD(txt,widget,dflt) \
    { input_sizer->Add ( new wxStaticText( this, -1, wxU(_( txt ) ) ),   \
                         0, wxALIGN_LEFT|wxLEFT, 5 );           \
      input_sizer->Add( 0, 0, wxEXPAND );                                \
      widget = new wxStaticText( this, -1, wxU( dflt ), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );                \
      input_sizer->Add( widget, 0, wxALIGN_RIGHT|wxRIGHT, 5  ); \
    }

    INPUT_ADD( "Read at media", read_bytes_text, "0" );
    INPUT_ADD( "Input bitrate", input_bitrate_text, "           0" );

    INPUT_ADD( "Demuxed", demux_bytes_text ,"0");
    INPUT_ADD( "Stream bitrate", demux_bitrate_text, "0" );

    input_sizer->Layout();
    input_bsizer->Add( input_sizer, 0, wxALL | wxGROW, 5 );
    input_bsizer->Layout();
    sizer->Add( input_bsizer, 0, wxALL|wxGROW, 5 );

   /* Vout */
    wxStaticBox *video_box = new wxStaticBox( this, -1, wxU( _("Video" ) ) );
    video_box->SetAutoLayout( TRUE );
    video_bsizer = new wxStaticBoxSizer( video_box, wxVERTICAL );
    video_sizer = new wxFlexGridSizer( 3, 3, 2, 20 );

#define VIDEO_ADD(txt,widget,dflt) \
    { video_sizer->Add ( new wxStaticText( this, -1, wxU(_( txt ) ) ),  \
                         0, wxALIGN_LEFT|wxLEFT , 5  );        \
      video_sizer->Add( 0, 0, wxEXPAND );                                \
      widget = new wxStaticText( this, -1, wxU( dflt ), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );               \
      video_sizer->Add( widget, 0, wxALIGN_RIGHT|wxRIGHT, 5 ); \
    }
    VIDEO_ADD( "Decoded blocks", video_decoded_text, "0" );
    VIDEO_ADD( "Displayed frames", displayed_text, "           0" );
    VIDEO_ADD( "Lost frames", lost_frames_text, "0" );


    video_sizer->Layout();
    video_bsizer->Add( video_sizer, 0, wxALL | wxGROW, 5 );
    video_bsizer->Layout();
    sizer->Add( video_bsizer , 0, wxALL| wxGROW, 5 );

    /* Sout */
    wxStaticBox *sout_box = new wxStaticBox( this, -1,
                                              wxU( _("Streaming" ) ) );
    sout_box->SetAutoLayout( TRUE );
    sout_bsizer = new wxStaticBoxSizer( sout_box, wxVERTICAL );
    sout_sizer = new wxFlexGridSizer( 3, 3, 2, 20 );

#define SOUT_ADD(txt,widget,dflt) \
    { sout_sizer->Add ( new wxStaticText( this, -1, wxU(_( txt ) ) ),  \
                         0, wxALIGN_LEFT|wxLEFT, 5  );        \
      sout_sizer->Add( 0, 0, wxEXPAND );                                \
      widget = new wxStaticText( this, -1, wxU( dflt ), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );              \
      sout_sizer->Add( widget, 0,wxALIGN_RIGHT|wxRIGHT, 5 ); \
    }
    SOUT_ADD( "Sent packets", sout_sent_packets_text, "          0" );
    SOUT_ADD( "Sent bytes", sout_sent_bytes_text, "0" );
    SOUT_ADD( "Send rate", sout_send_bitrate_text, "0" );
    sout_sizer->Layout();
    sout_bsizer->Add( sout_sizer, 0, wxALL | wxGROW, 5 );
    sout_bsizer->Layout();
    sizer->Add( sout_bsizer , 0, wxALL| wxGROW, 5 );

   /* Aout */
    wxStaticBox *audio_box = new wxStaticBox( this, -1,
                                              wxU( _("Audio" ) ) );
    audio_box->SetAutoLayout( TRUE );
    audio_bsizer = new wxStaticBoxSizer( audio_box, wxVERTICAL );
    audio_sizer = new wxFlexGridSizer( 3, 3, 2, 20 );

#define AUDIO_ADD(txt,widget,dflt) \
    { audio_sizer->Add ( new wxStaticText( this, -1, wxU(_( txt ) ) ),   \
                         0, wxALIGN_LEFT|wxLEFT, 5  );          \
      audio_sizer->Add( 0, 0, wxEXPAND );                                \
      widget = new wxStaticText( this, -1, wxU( dflt ), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );                \
      audio_sizer->Add( widget, 0, wxALIGN_RIGHT|wxRIGHT, 5 );  \
    }
    AUDIO_ADD( "Decoded blocks", audio_decoded_text, "         0" );
    AUDIO_ADD( "Played buffers", played_abuffers_text, "0" );
    AUDIO_ADD( "Lost buffers", lost_abuffers_text, "0" );
    audio_sizer->Layout();
    audio_bsizer->Add( audio_sizer, 0, wxALL | wxGROW, 5 );
    audio_bsizer->Layout();
    sizer->Add( audio_bsizer , 0, wxALL| wxGROW, 5 );

    sizer->Layout();
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
    UPDATE( read_bytes_text, "%8.0f kB",(float)(p_item->p_stats->i_read_bytes)/1000 );
    UPDATE( input_bitrate_text, "%6.0f kb/s", (float)(p_item->p_stats->f_input_bitrate)*8000 );
    UPDATE( demux_bytes_text, "%8.0f kB", (float)(p_item->p_stats->i_demux_read_bytes)/1000 );
    UPDATE( demux_bitrate_text, "%6.0f kb/s",  (float)(p_item->p_stats->f_demux_bitrate)*8000 );

    /* Video */
    UPDATE( video_decoded_text, "%5i", p_item->p_stats->i_decoded_video );
    UPDATE( displayed_text, "%5i", p_item->p_stats->i_displayed_pictures );
    UPDATE( lost_frames_text, "%5i", p_item->p_stats->i_lost_pictures );

    /* Sout */
    UPDATE( sout_sent_packets_text, "%5i", p_item->p_stats->i_sent_packets );
    UPDATE( sout_sent_bytes_text, "%8.0f kB",
            (float)(p_item->p_stats->i_sent_bytes)/1000 );
    UPDATE( sout_send_bitrate_text, "%6.0f kb/s",
            (float)(p_item->p_stats->f_send_bitrate*8)*1000 );

    /* Audio*/
    UPDATE( audio_decoded_text, "%5i", p_item->p_stats->i_decoded_audio );
    UPDATE( played_abuffers_text, "%5i", p_item->p_stats->i_played_abuffers );
    UPDATE( lost_abuffers_text, "%5i", p_item->p_stats->i_lost_abuffers );

    vlc_mutex_unlock( &p_item->p_stats->lock );

    input_sizer->Layout();
    video_sizer->Layout();
    sout_sizer->Layout();
    audio_sizer->Layout();

    sizer->Layout();
    panel_sizer->Layout();
    SetSizerAndFit( panel_sizer );
}

void InputStatsInfoPanel::Clear()
{}

void InputStatsInfoPanel::OnOk( )
{}

void InputStatsInfoPanel::OnCancel( )
{}

