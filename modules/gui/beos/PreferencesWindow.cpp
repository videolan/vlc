/*****************************************************************************
 * PreferencesWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.cpp,v 1.1 2002/10/28 17:18:18 titer Exp $
 *
 * Authors: Eric Petit <titer@videolan.org>
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

/* BeOS headers */
#include <InterfaceKit.h>

/* VLC headers */
#include <vlc/vlc.h>
#include <vlc/intf.h>

/* BeOS module headers */
#include "VlcWrapper.h"
#include "MsgVals.h"
#include "PreferencesWindow.h"


/*****************************************************************************
 * Preferences::PreferencesWindow
 *****************************************************************************/
PreferencesWindow::PreferencesWindow( BRect frame, const char* name,
								intf_thread_t *p_interface )
	:	BWindow( frame, name, B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
				 B_NOT_ZOOMABLE | B_NOT_RESIZABLE )
{
	p_intf = p_interface;

    BRect rect = Bounds();
    p_preferences_view = new BView( rect, "preferences view",
                                    B_FOLLOW_ALL, B_WILL_DRAW );
    p_preferences_view->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    
    /* Create the "OK" button */
    rect.Set( 320, 160, 390, 185);
    BButton *p_button = new BButton( rect, NULL, "OK", new BMessage( PREFS_OK ) );
    p_preferences_view->AddChild( p_button );
    
    /* Create the "Cancel" button */
    rect.OffsetBy( -80, 0 );
    p_button = new BButton( rect, NULL, "Cancel", new BMessage( PREFS_CANCEL ) );
    p_preferences_view->AddChild( p_button );
    
    /* Create the box */
    rect.Set( 10, 10, 390, 150 );
    BBox *p_box = new BBox( rect, "preferences box", B_FOLLOW_ALL );
    
    /* Create the post-processing slider */
    rect.Set( 10, 10, 370, 50 );
    p_pp_slider = new BSlider( rect, "post-processing", "MPEG4 post-processing level",
                              new BMessage( SLIDER_UPDATE ),
                              0, 6, B_TRIANGLE_THUMB, B_FOLLOW_LEFT, B_WILL_DRAW );
    p_pp_slider->SetHashMarks(B_HASH_MARKS_BOTTOM);
    p_pp_slider->SetHashMarkCount( 7 );
    p_pp_slider->SetLimitLabels("None","Maximum");
    p_pp_slider->SetValue( config_GetInt( p_intf, "ffmpeg-pp-q" ) );
    p_box->AddChild( p_pp_slider );
    
    /* Create the luminence slider */
    rect.Set( 10, 65, 370, 90 );
    p_lum_slider = new BSlider( rect, "luminence", "Luminence",
                          new BMessage( SLIDER_UPDATE ),
                          0, 255, B_TRIANGLE_THUMB, B_FOLLOW_LEFT, B_WILL_DRAW );
    p_lum_slider->SetValue( config_GetInt( p_intf, "Y plan" ) );
    p_box->AddChild( p_lum_slider );
    
    rect.Set( 55, 110, 370, 120 );
    p_restart_string = new BStringView( rect, "restart", "",
                                        B_FOLLOW_ALL, B_WILL_DRAW);
    p_box->AddChild( p_restart_string );
    
    p_preferences_view-> AddChild( p_box );
    
    AddChild( p_preferences_view );

	// start window thread in hidden state
	Hide();
	Show();
}

/*****************************************************************************
 * PreferencesWindow::~PreferencesWindow
 *****************************************************************************/
PreferencesWindow::~PreferencesWindow()
{
}

/*****************************************************************************
 * PreferencesWindow::QuitRequested
 *****************************************************************************/
bool PreferencesWindow::QuitRequested()
{
    CancelChanges();
	Hide();
	return false;
}

/*****************************************************************************
 * PreferencesWindow::MessageReceived
 *****************************************************************************/
void PreferencesWindow::MessageReceived( BMessage * p_message )
{
	switch ( p_message->what )
	{
	    case SLIDER_UPDATE:
	    {
	        p_restart_string->SetText( "Changes will take effect when you restart playback" );
	        break;
	    }
	    case PREFS_CANCEL:
	    {
	        CancelChanges();
	        Hide();
	        break;
	    }
	    case PREFS_OK:
	    {
	        ApplyChanges();
	        Hide();
	    }
		default:
			BWindow::MessageReceived( p_message );
			break;
	}
}

/*****************************************************************************
 * PreferencesWindow::FrameResized
 *****************************************************************************/
void PreferencesWindow::FrameResized(float width, float height)
{
}

/*****************************************************************************
 * PreferencesWindow::ReallyQuit
 *****************************************************************************/
void PreferencesWindow::ReallyQuit()
{
    Hide();
    Quit();
}

/*****************************************************************************
 * PreferencesWindow::CancelChanges
 *****************************************************************************/
void PreferencesWindow::CancelChanges()
{
    p_pp_slider->SetValue( 0 );
    p_lum_slider->SetValue( 255 );
    p_restart_string->SetText( "" );
}

/*****************************************************************************
 * PreferencesWindow::ApplyChanges
 *****************************************************************************/
void PreferencesWindow::ApplyChanges()
{
    config_PutInt( p_intf, "ffmpeg-pp-q", p_pp_slider->Value() );
    config_PutInt( p_intf, "Y plan", p_lum_slider->Value() );
    if( p_lum_slider->Value() < 255 )
    {
        config_PutPsz( p_intf, "filter", "yuv" );
    }
    else
    {
        config_PutPsz( p_intf, "filter", NULL );
    }
    p_restart_string->SetText( "" );
}
