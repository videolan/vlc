/*****************************************************************************
 * intf_beos.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: intf_beos.cpp,v 1.13 2001/03/05 01:29:25 sam Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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

#define MODULE_NAME beos
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdio.h>
#include <stdlib.h>                                      /* malloc(), free() */

#include <kernel/OS.h>
#include <storage/Path.h>
#include <Alert.h>
#include <View.h>
#include <CheckBox.h>
#include <Button.h>
#include <Slider.h>
#include <StatusBar.h>
#include <Application.h>
#include <Message.h>
#include <NodeInfo.h>
#include <Locker.h>
#include <DirectWindow.h>

#include <malloc.h>
#include <string.h>

extern "C"
{
#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "modules.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_plst.h"
#include "intf_msg.h"
#include "audio_output.h"
#include "MsgVals.h"


#include "main.h"
}

#include "InterfaceWindow.h"
#include "Bitmaps.h"
#include "TransportButton.h"

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    InterfaceWindow * p_window;
    char              i_key;
} intf_sys_t;

/*****************************************************************************
 * InterfaceWindow
 *****************************************************************************/
 
InterfaceWindow::InterfaceWindow( BRect frame, const char *name , intf_thread_t  *p_interface )
    : BWindow(frame, name, B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
    	B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_WILL_ACCEPT_FIRST_CLICK |B_ASYNCHRONOUS_CONTROLS)
{
    p_intf = p_interface;
	BRect ButtonRect;
	float xStart = 2.0;
	float yStart = 10.0;

    SetName( "interface" );
    SetTitle(VOUT_TITLE " (BeOS interface)");
    
    BView* p_view;

	/* Add the view */
    p_view = new BView( Bounds(), "", B_FOLLOW_ALL, B_WILL_DRAW );
	p_view->SetViewColor(216,216,216);
    
   	/* Buttons */
   	/* Slow play */
	ButtonRect.SetLeftTop(BPoint(xStart, yStart));
	ButtonRect.SetRightBottom(ButtonRect.LeftTop() + kSkipButtonSize);
	xStart += kRewindBitmapWidth;
	TransportButton* p_slow = new TransportButton(ButtonRect, B_EMPTY_STRING,
											kSkipBackBitmapBits,
											kPressedSkipBackBitmapBits,
											kDisabledSkipBackBitmapBits,
											new BMessage(SLOWER_PLAY));
   	p_view->AddChild( p_slow );

	/* Play Pause */
	ButtonRect.SetLeftTop(BPoint(xStart, yStart));
	ButtonRect.SetRightBottom(ButtonRect.LeftTop() + kPlayButtonSize);
	xStart += kPlayPauseBitmapWidth + 1.0;
   	PlayPauseButton* p_play = new PlayPauseButton(ButtonRect, B_EMPTY_STRING,
											kPlayButtonBitmapBits,
											kPressedPlayButtonBitmapBits,
											kDisabledPlayButtonBitmapBits,
											kPlayingPlayButtonBitmapBits,
											kPressedPlayingPlayButtonBitmapBits,
											kPausedPlayButtonBitmapBits,
											kPressedPausedPlayButtonBitmapBits,
										   	new BMessage(START_PLAYBACK));
   
   	p_view->AddChild( p_play );
   	p_play->SetPlaying();

	/* Fast Foward */
	ButtonRect.SetLeftTop(BPoint(xStart, yStart));
	ButtonRect.SetRightBottom(ButtonRect.LeftTop() + kSkipButtonSize);
	xStart += kRewindBitmapWidth;
	TransportButton* p_fast = new TransportButton(ButtonRect, B_EMPTY_STRING,
											kSkipForwardBitmapBits,
											kPressedSkipForwardBitmapBits,
											kDisabledSkipForwardBitmapBits,
											new BMessage(FASTER_PLAY));
   	p_view->AddChild( p_fast );

	/* Stop */
	ButtonRect.SetLeftTop(BPoint(xStart, yStart));
	ButtonRect.SetRightBottom(ButtonRect.LeftTop() + kStopButtonSize);
	xStart += kStopBitmapWidth;
	TransportButton* p_stop = new TransportButton(ButtonRect, B_EMPTY_STRING,
											kStopButtonBitmapBits,
											kPressedStopButtonBitmapBits,
											kDisabledStopButtonBitmapBits,
											new BMessage(STOP_PLAYBACK));
   	p_view->AddChild( p_stop );
   	
   	/* Seek Status */	
	p_seek = new SeekSlider(BRect(5,35,355,65), this, 0, 100,
						B_TRIANGLE_THUMB);
	p_seek->SetValue(0);
	p_seek->UseFillColor(TRUE);
    p_view->AddChild( p_seek );

   	/* Volume Slider */	
	p_vol = new BSlider(BRect(xStart,2,300,20), "vol", "Volume", 
						 new BMessage(VOLUME_CHG), 0, VOLUME_MAX);
	p_vol->SetValue(VOLUME_DEFAULT);
    p_view->AddChild( p_vol );
    
    /* Volume Mute */
	p_mute = new BCheckBox(BRect(300,10,355,25), "mute", "Mute",
						 new BMessage(VOLUME_MUTE));
    p_view->AddChild( p_mute );
							 
	/* Set size and Show */
    AddChild( p_view );
	ResizeTo(360,70);
    Show();
}

InterfaceWindow::~InterfaceWindow()
{
}

/*****************************************************************************
 * InterfaceWindow::MessageReceived
 *****************************************************************************/
void InterfaceWindow::MessageReceived( BMessage * p_message )
{
	int vol_val = p_vol->Value();	// remember the current volume
	static int playback_status;		// remember playback state
	
	Activate();
    switch( p_message->what )
    {
    case OPEN_DVD:
    	break;
    case STOP_PLAYBACK:
    	// this currently stops playback not nicely
		if (p_intf->p_input != NULL )
		{
			// silence the sound, otherwise very horrible
			if (p_main->p_aout != NULL)
			{
				p_main->p_aout->vol = 0;
			}
			snooze(400000);
			input_SetStatus(p_intf->p_input, INPUT_STATUS_END);
		}
    	break;
	case START_PLAYBACK:
		// starts playing in normal mode
//		if (p_intf->p_input != NULL )
//		{			
//			if (p_main->p_aout != NULL)
//			{
//				p_main->p_aout->vol = vol_val;
//			}
//			snooze(400000);
//			input_SetStatus(p_intf->p_input, INPUT_STATUS_PLAY);
//			playback_status = PLAYING;
//		} 
//		break;
	case PAUSE_PLAYBACK:
		// pause the playback
		if (p_intf->p_input != NULL )
		{
			// mute the volume if currently playing
			if (playback_status == PLAYING)
			{
				if (p_main->p_aout != NULL)
				{
					p_main->p_aout->vol = 0;
				}
				playback_status = PAUSED;
			}
			else
			// restore the volume
			{
				if (p_main->p_aout != NULL)
				{
					p_main->p_aout->vol = vol_val;
				}
				playback_status = PLAYING;
			}
			snooze(400000);
			input_SetStatus(p_intf->p_input, INPUT_STATUS_PAUSE);
		}
		break;
	case FASTER_PLAY:
		// cycle the fast playback modes
		if (p_intf->p_input != NULL )
		{
			if (p_main->p_aout != NULL)
			{
				p_main->p_aout->vol = 0;
			}
			snooze(400000);
			input_SetStatus(p_intf->p_input, INPUT_STATUS_FASTER);
		}
		break;
	case SLOWER_PLAY:
		// cycle the slow playback modes
		if (p_intf->p_input != NULL )
		{
			if (p_main->p_aout != NULL)
			{
				p_main->p_aout->vol = 0;
			}
			snooze(400000);
			input_SetStatus(p_intf->p_input, INPUT_STATUS_SLOWER);
		}
		break;
	case SEEK_PLAYBACK:
		// handled by semaphores;
/*	    if( p_intf->p_input != NULL )
	    {
	    	float new_position;
		    if (p_message->FindFloat("be:value", &new_position) == B_OK)
		    {
		    	printf("%e\n", new_position);
	        	input_Seek( p_intf->p_input, new_position * 100 );
		    }
	    } */
		break;
	case VOLUME_CHG:
		// adjust the volume
        if (p_main->p_aout != NULL) 
        {
			p_main->p_aout->vol = vol_val;
		}
		break;
	case VOLUME_MUTE:
		// mute
        if (p_main->p_aout != NULL) 
   	    {
			if (p_mute->Value() == B_CONTROL_OFF)
			{
				p_main->p_aout->vol = vol_val;
			}	
			else
			{
				p_main->p_aout->vol = 0;
			}
		}
		break;
	case SELECT_CHANNEL:
		break;
    case B_SIMPLE_DATA:
        {
            entry_ref ref;
            if( p_message->FindRef( "refs", &ref ) == B_OK )
            {
                BPath path( &ref );
                char * psz_name = strdup(path.Path());
                intf_WarnMsg( 1, "intf: dropped text/uri-list data `%s'",
                              psz_name );
                intf_PlstAdd( p_main->p_playlist, PLAYLIST_END, psz_name );
            }

        }
        break;
    default:
        BWindow::MessageReceived( p_message );
        break;
    }
}

/*****************************************************************************
 * InterfaceWindow::QuitRequested
 *****************************************************************************/

bool InterfaceWindow::QuitRequested()
{
    p_intf->b_die = 1;

    return( false );
}

/*****************************************************************************
 * SeekSlider
 *****************************************************************************/
SeekSlider::SeekSlider(BRect frame,
				InterfaceWindow *owner,
				int32 minValue,
				int32 maxValue,
				thumb_style thumbType = B_TRIANGLE_THUMB)
			:BSlider(frame, B_EMPTY_STRING, B_EMPTY_STRING,
					NULL, minValue, maxValue, thumbType)
{
	fOwner = owner;
	fMouseDown = false;
}

SeekSlider::~SeekSlider()
{
}

/*****************************************************************************
 * SeekSlider::MouseDown
 *****************************************************************************/
void SeekSlider::MouseDown(BPoint where)
{
	BSlider::MouseDown(where);
	fOwner->fScrubSem = create_sem(1, "Vlc::fScrubSem");
	fMouseDown = true;					
}

/*****************************************************************************
 * SeekSlider::MouseUp
 *****************************************************************************/
void SeekSlider::MouseMoved(BPoint where, uint32 code, const BMessage *message)
{
	BSlider::MouseMoved(where, code, message);
	if (!fMouseDown)
		return;
	release_sem(fOwner->fScrubSem);
}

/*****************************************************************************
 * SeekSlider::MouseUp
 *****************************************************************************/
void SeekSlider::MouseUp(BPoint where)
{
	BSlider::MouseUp(where);
	delete_sem(fOwner->fScrubSem);
	fOwner->fScrubSem = B_ERROR;
	fMouseDown = false;					
}
	

extern "C"
{

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  intf_Probe     ( probedata_t *p_data );
static int  intf_Open      ( intf_thread_t *p_intf );
static void intf_Close     ( intf_thread_t *p_intf );
static void intf_Run       ( intf_thread_t *p_intf );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( intf_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = intf_Probe;
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Probe: probe the interface and return a score
 *****************************************************************************
 * This function tries to initialize Gnome and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int intf_Probe( probedata_t *p_data )
{
    if( TestMethod( INTF_METHOD_VAR, "beos" ) )
    {
        return( 999 );
    }

    return( 100 );
}

/*****************************************************************************
 * intf_Open: initialize interface
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t*) malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM));
        return( 1 );
    }
    p_intf->p_sys->i_key = -1;
    
    /* Create the interface window */
    p_intf->p_sys->p_window =
        new InterfaceWindow( BRect( 50, 50, 400, 100 ),
                             VOUT_TITLE " (BeOS interface)", p_intf );
    if( p_intf->p_sys->p_window == 0 )
    {
        free( p_intf->p_sys );
        intf_ErrMsg( "error: cannot allocate memory for InterfaceWindow" );
        return( 1 );
    }
    
    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy dummy interface
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Destroy the interface window */
    p_intf->p_sys->p_window->Lock();
    p_intf->p_sys->p_window->Quit();    

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * intf_Run: event loop
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
	
	float progress;
	bool seekNeeded = false;
	
    while( !p_intf->b_die )
    {

        /* Manage core vlc functions through the callback */
        p_intf->pf_manage( p_intf );

	    /* Manage the slider */
	    if( p_intf->p_input != NULL && p_intf->p_sys->p_window != NULL)
    	{
            if (acquire_sem(p_intf->p_sys->p_window->fScrubSem) == B_OK)
			{
				seekNeeded = true;
			}		

			if (seekNeeded)
            {
            	uint32 seekTo = (p_intf->p_sys->p_window->p_seek->Value() * 
            			p_intf->p_input->stream.p_selected_area->i_size) / 100;
				input_Seek( p_intf->p_input, seekTo );
				seekNeeded = false;            	
            }
			else if (p_intf->p_sys->p_window->Lock())
            {
	            progress = (100. * p_intf->p_input->stream.p_selected_area->i_tell) /
            				p_intf->p_input->stream.p_selected_area->i_size;
	            p_intf->p_sys->p_window->p_seek->SetValue(progress);
	            p_intf->p_sys->p_window->Unlock();
	        }
         }

        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }
}

} /* extern "C" */
