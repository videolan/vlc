/*****************************************************************************
 * vlm_streampanel:cpp
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: playlist.cpp 12582 2005-09-17 14:15:32Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/OR MODIFy
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

#include "dialogs/vlm/vlm_streampanel.hpp"
#include "dialogs/vlm/vlm_stream.hpp"
#include "dialogs/vlm/vlm_slider_manager.hpp"

#include "dialogs/vlm/vlm_panel.hpp"

#include "bitmaps/play.xpm"
#include "bitmaps/pause.xpm"
#include "bitmaps/stop.xpm"
#include "bitmaps/trash.xpm"

enum
{
    BPlay_Event,
    BStop_Event,
    BSlider_Event,
    BEdit_Event,
    BTrash_Event,
};

BEGIN_EVENT_TABLE( VLMBroadcastStreamPanel, wxPanel )
    EVT_BUTTON( BPlay_Event, VLMBroadcastStreamPanel::OnPlay )
    EVT_BUTTON( BStop_Event, VLMBroadcastStreamPanel::OnStop )
    EVT_BUTTON( BEdit_Event, VLMBroadcastStreamPanel::OnEdit )
    EVT_BUTTON( BTrash_Event, VLMBroadcastStreamPanel::OnTrash )
    EVT_COMMAND_SCROLL( BSlider_Event, VLMBroadcastStreamPanel::OnSliderUpdate )
END_EVENT_TABLE()

/***********************************************************************
 * VLMStream
 ***********************************************************************/
VLMStreamPanel::VLMStreamPanel( intf_thread_t * _p_intf, wxWindow * _p_parent ):
         wxPanel( _p_parent, -1, wxDefaultPosition, wxDefaultSize )
{
    p_intf = _p_intf;
    p_slider = NULL;
}

VLMStreamPanel::~VLMStreamPanel()
{
}

/***********************************************************************
 * VLMBroadcastStream
 ***********************************************************************/
VLMBroadcastStreamPanel::VLMBroadcastStreamPanel( intf_thread_t* _p_intf,
       wxWindow *_p_parent , VLMBroadcastStream *_stream ):
       VLMStreamPanel( _p_intf, _p_parent ),
       p_stream( _stream )
{
    wxStaticBox *box = new wxStaticBox( this, -1,
                    wxU( p_stream->p_media->psz_name ) );

    wxStaticBoxSizer *box_sizer = new wxStaticBoxSizer( box, wxHORIZONTAL );

    play_button = new wxBitmapButton( this, BPlay_Event,
                                wxBitmap( play_xpm ) );
    play_button->SetToolTip( wxU(_("Play/Pause") ) );
    box_sizer->Add( play_button, 0, wxEXPAND | wxALL, 5 );

    wxBitmapButton *stop_button = new wxBitmapButton( this, BStop_Event,
                                          wxBitmap( stop_xpm ) );
    stop_button->SetToolTip( wxU(_("Stop") ) );
    box_sizer->Add( stop_button, 0, wxEXPAND | wxALL, 5 );

    p_slider = new wxSlider( this,  BSlider_Event, 0, 0,
                            SLIDER_MAX_POS, wxDefaultPosition, wxDefaultSize );
    p_slider->Disable();
    box_sizer->Add( p_slider,    1, wxEXPAND | wxALL, 5 );

    p_time = new wxStaticText( this, -1, wxU( "0:00:00 / 0:00:00") );
    box_sizer->Add( p_time, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5 );

    wxBitmapButton *edit_button = new wxBitmapButton( this, BEdit_Event,
                                    wxBitmap( trash_xpm ) );
    edit_button->SetToolTip( wxU( _("Edit") ) );
    box_sizer->Add( edit_button, 0, wxEXPAND | wxALL , 5 );

    wxBitmapButton *trash_button = new wxBitmapButton( this, BTrash_Event,
                                    wxBitmap( trash_xpm ) );
    trash_button->SetToolTip( wxU( _("Delete" ) ) );
    box_sizer->Add( trash_button, 0, wxEXPAND | wxALL , 5 );

    box_sizer->Layout();
    SetSizerAndFit( box_sizer );

    p_sm = new VLMSliderManager( p_intf, this );
}

VLMBroadcastStreamPanel::~VLMBroadcastStreamPanel()
{
}

void VLMBroadcastStreamPanel::TogglePlayButton( int state )
{
    if( state == PLAYING_S )
    {
        play_button->SetBitmapLabel( wxBitmap( pause_xpm ) );
    }
    if( state == PAUSE_S )
    {
        play_button->SetBitmapLabel( wxBitmap( play_xpm ) );
    }
}

void VLMBroadcastStreamPanel::Update()
{
    /* Update managed slider */
    p_sm->Update();
    p_time->SetLabel( p_sm->time_string );
}

void VLMBroadcastStreamPanel::OnPlay( wxCommandEvent &event )
{
    /* FIXME: Factorize input / VLM code here */
    /* Handle multiple instance */
    if( p_stream->p_media->i_instance > 0 &&
        p_stream->p_media->instance[0]->p_input )
    {
        vlc_value_t val;
        vlc_object_yield( p_stream->p_media->instance[0]->p_input );
        var_Get( p_stream->p_media->instance[0]->p_input, "state", &val );
        if( val.i_int != PAUSE_S )
        {
            /* Pause */
            val.i_int = PAUSE_S;
        }
        else
        {
            /* Resume */
            val.i_int = PLAYING_S;
        }
        var_Set( p_stream->p_media->instance[0]->p_input, "state", val );
        TogglePlayButton( val.i_int );
        vlc_object_release( p_stream->p_media->instance[0]->p_input );
    }
    else
    {
         p_stream->Play();
         TogglePlayButton( PLAYING_S );
    }
}

void VLMBroadcastStreamPanel::OnStop( wxCommandEvent &event )
{
    p_stream->Stop();
}

void VLMBroadcastStreamPanel::OnEdit( wxCommandEvent &event )
{
     VLMEditStreamFrame *p_frame =
           new  VLMEditStreamFrame( p_intf, this, p_stream->p_vlm, VLC_TRUE,
                                    p_stream );
     p_frame->Show();
}

void VLMBroadcastStreamPanel::OnTrash( wxCommandEvent &event )
{
    p_stream->Delete();
}

void VLMBroadcastStreamPanel::OnSliderUpdate( wxScrollEvent& event )
{
    p_sm->ProcessUpdate( event );
}

/***********************************************************************
 * VLMVODStream
 ***********************************************************************/
VLMVODStreamPanel::VLMVODStreamPanel( intf_thread_t* _p_intf,
       wxWindow *_p_parent , VLMVODStream *_stream ):
       VLMStreamPanel( _p_intf, _p_parent ),
       p_stream( _stream )
{
}

VLMVODStreamPanel::~VLMVODStreamPanel()
{
}
