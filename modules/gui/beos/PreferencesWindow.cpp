/*****************************************************************************
 * PreferencesWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: PreferencesWindow.cpp,v 1.2 2002/11/23 15:00:54 titer Exp $
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
				 B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_NOT_CLOSABLE )
{
	p_intf = p_interface;
    BRect rect;

    /* "background" view */
    p_prefs_view = new BView( Bounds(), NULL, B_FOLLOW_ALL, B_WILL_DRAW );
    p_prefs_view->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    AddChild( p_prefs_view );

    /* add the tabs */
    rect = Bounds();
    rect.top += 10;
    rect.bottom -= 65;
    p_tabview = new BTabView( rect, "preferences view" );
    p_tabview->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    
    p_ffmpeg_view = new BView( p_tabview->Bounds(), NULL, B_FOLLOW_ALL, B_WILL_DRAW );
    p_ffmpeg_view->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    p_adjust_view = new BView( p_tabview->Bounds(), NULL, B_FOLLOW_ALL, B_WILL_DRAW );
    p_adjust_view->SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR ) );
    
    p_ffmpeg_tab = new BTab();
    p_tabview->AddTab( p_ffmpeg_view, p_ffmpeg_tab );
    p_ffmpeg_tab->SetLabel( "Ffmpeg" );
    
    p_adjust_tab = new BTab();
    p_tabview->AddTab( p_adjust_view, p_adjust_tab );
    p_adjust_tab->SetLabel( "Adjust" );
    
    /* fills the tabs */
    /* ffmpeg tab */
    rect = p_ffmpeg_view->Bounds();
    rect.InsetBy( 10, 10 );
    rect.bottom = rect.top + 30;
    p_pp_slider = new BSlider( rect, "post-processing", "MPEG4 post-processing level",
                               new BMessage( SLIDER_UPDATE ),
                               0, 6, B_TRIANGLE_THUMB,
                               B_FOLLOW_LEFT, B_WILL_DRAW ); 
    p_pp_slider->SetHashMarks(B_HASH_MARKS_BOTTOM);
    p_pp_slider->SetHashMarkCount( 7 );
    p_pp_slider->SetLimitLabels( "None", "Maximum" );
    p_ffmpeg_view->AddChild( p_pp_slider );
    
    
    /* adjust tab */
    rect = p_adjust_view->Bounds();
    rect.InsetBy( 10, 10 );
    rect.bottom = rect.top + 30;
    p_brightness_slider = new BSlider( rect, "brightness", "Brightness",
                                       new BMessage( SLIDER_UPDATE ),
                                       0, 200, B_TRIANGLE_THUMB,
                                       B_FOLLOW_LEFT, B_WILL_DRAW );
    p_brightness_slider->SetValue( 100 * config_GetFloat( p_intf, "Brightness" ) );
    rect.OffsetBy( 0, 40 );
    p_contrast_slider = new BSlider( rect, "contrast", "Contrast",
                                     new BMessage( SLIDER_UPDATE ),
                                     0, 200, B_TRIANGLE_THUMB,
                                     B_FOLLOW_LEFT, B_WILL_DRAW );
    p_contrast_slider->SetValue( 100 * config_GetFloat( p_intf, "Contrast" ) );
    rect.OffsetBy( 0, 40 );
    p_hue_slider = new BSlider( rect, "hue", "Hue",
                                new BMessage( SLIDER_UPDATE ),
                                0, 360, B_TRIANGLE_THUMB,
                                B_FOLLOW_LEFT, B_WILL_DRAW );
    p_hue_slider->SetValue( config_GetInt( p_intf, "Hue" ) );
    rect.OffsetBy( 0, 40 );
    p_saturation_slider = new BSlider( rect, "saturation", "Saturation",
                                       new BMessage( SLIDER_UPDATE ),
                                       0, 200, B_TRIANGLE_THUMB,
                                       B_FOLLOW_LEFT, B_WILL_DRAW );
    p_saturation_slider->SetValue( 100 * config_GetFloat( p_intf, "Saturation" ) );
    p_adjust_view->AddChild( p_brightness_slider );
    p_adjust_view->AddChild( p_contrast_slider );
    p_adjust_view->AddChild( p_hue_slider );
    p_adjust_view->AddChild( p_saturation_slider );
    
    p_prefs_view->AddChild( p_tabview );

    /* restart message */
    rect = p_prefs_view->Bounds();
    rect.bottom -= 43;
    rect.top = rect.bottom - 10;
    p_restart_string = new BStringView( rect, NULL, "" );
    rgb_color redColor = {255, 0, 0, 255};
    p_restart_string->SetHighColor(redColor);
    p_restart_string->SetAlignment( B_ALIGN_CENTER );
    p_prefs_view->AddChild( p_restart_string );

    /* buttons */
    BButton *p_button;
    rect = Bounds();
    rect.InsetBy( 10, 10 );
    rect.top = rect.bottom - 25;
    rect.left = rect.right - 80;
    p_button = new BButton( rect, NULL, "OK", new BMessage( PREFS_OK ) );
    p_prefs_view->AddChild( p_button );
    
    rect.OffsetBy( -90, 0 );
    p_button = new BButton( rect, NULL, "Cancel", new BMessage( PREFS_CANCEL ) );
    p_prefs_view->AddChild( p_button );
    
    rect.OffsetBy( -90, 0 );
    p_button = new BButton( rect, NULL, "Defaults", new BMessage( PREFS_DEFAULTS ) );
    p_prefs_view->AddChild( p_button );
    
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
 * PreferencesWindow::MessageReceived
 *****************************************************************************/
void PreferencesWindow::MessageReceived( BMessage * p_message )
{
	switch ( p_message->what )
	{
	    case SLIDER_UPDATE:
	    {
	        ApplyChanges();
	        break;
	    }
	    case PREFS_DEFAULTS:
	    {
	        SetDefaults();
	        break;
	    }
	    case PREFS_OK:
	    {
	        config_PutInt( p_intf, "ffmpeg-pp-q", p_pp_slider->Value() );
            config_PutPsz( p_intf, "filter", "adjust" );
            ApplyChanges();
            Hide();
	    }
		default:
			BWindow::MessageReceived( p_message );
			break;
	}
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
 * PreferencesWindow::SetDefaults
 *****************************************************************************/
void PreferencesWindow::SetDefaults()
{
    p_pp_slider->SetValue( 0 );
    p_brightness_slider->SetValue( 100 );
    p_contrast_slider->SetValue( 100 );
    p_hue_slider->SetValue( 0 );
    p_saturation_slider->SetValue( 100 );

    p_restart_string->SetText( config_GetInt( p_intf, "ffmpeg-pp-q" ) ?
        "Changes will take effect after you restart playback" : "" );
    
    config_PutPsz( p_intf, "filter", NULL );
    config_PutInt( p_intf, "ffmpeg-pp-q", 0 );
    
    ApplyChanges();
}

/*****************************************************************************
 * PreferencesWindow::ApplyChanges
 *****************************************************************************/
void PreferencesWindow::ApplyChanges()
{
    bool b_restart_needed = false;

    if( ( !config_GetPsz( p_intf, "filter" ) ||
          strncmp( config_GetPsz( p_intf, "filter" ), "adjust", 6 ) ) &&
        ( p_brightness_slider->Value() != 100 ||
          p_contrast_slider->Value() != 100 ||
          p_hue_slider->Value() ||
          p_saturation_slider->Value() != 100 ) )
    {
        b_restart_needed = true;
    }

    if( p_pp_slider->Value() != config_GetInt( p_intf, "ffmpeg-pp-q" ) )
    {
        b_restart_needed = true;
    }
    
    config_PutFloat( p_intf, "Brightness",
                     (float)p_brightness_slider->Value() / 100 );
    config_PutFloat( p_intf, "Contrast",
                     (float)p_contrast_slider->Value() / 100 );
    config_PutInt( p_intf, "Hue", p_hue_slider->Value() );
    config_PutFloat( p_intf, "Saturation",
                     (float)p_saturation_slider->Value() / 100 );
    
    p_restart_string->LockLooper();
    p_restart_string->SetText( b_restart_needed ?
        "Changes will take effect after you restart playback" : "" );
    p_restart_string->UnlockLooper();
}
