/*****************************************************************************
 * InterfaceWindow.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: InterfaceWindow.cpp,v 1.22 2002/07/23 13:16:51 tcastley Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tony@castley.net>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
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

/* System headers */
#include <kernel/OS.h>
#include <InterfaceKit.h>
#include <AppKit.h>
#include <StorageKit.h>
#include <SupportKit.h>
#include <malloc.h>
#include <scsi.h>
#include <scsiprobe_driver.h>
#include <fs_info.h>
#include <string.h>


/* VLC headers */
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/intf.h>

/* BeOS interface headers */
#include "MsgVals.h"
#include "MediaControlView.h"
#include "PlayListWindow.h"
#include "intf_vlc_wrapper.h"
#include "InterfaceWindow.h"


/*****************************************************************************
 * InterfaceWindow
 *****************************************************************************/

InterfaceWindow::InterfaceWindow( BRect frame, const char *name,
                                  intf_thread_t  *p_interface )
    : BWindow( frame, name, B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
               B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_WILL_ACCEPT_FIRST_CLICK
                | B_ASYNCHRONOUS_CONTROLS )
{
    file_panel = NULL;
    playlist_window = NULL;
    p_intf = p_interface;
    p_vlc_wrapper = Intf_VLCWrapper::getVLCWrapper(p_intf);
    BRect controlRect(0,0,0,0);
    
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    b_empty_playlist = (p_playlist->i_size < 0);
    b_mute = false;

    /* set the title bar */
    SetName( "interface" );
    SetTitle(VOUT_TITLE);

    /* set up the main menu */
    BMenuBar *menu_bar;
    menu_bar = new BMenuBar(controlRect, "main menu");
    AddChild( menu_bar );

    BMenu *mFile;
    BMenu *mAudio;
    CDMenu *cd_menu;
    BMenu *mNavigation;
    
    /* Add the file Menu */
    BMenuItem *mItem;
    menu_bar->AddItem( mFile = new BMenu( "File" ) );
    menu_bar->ResizeToPreferred();
    mFile->AddItem( mItem = new BMenuItem( "Open File" B_UTF8_ELLIPSIS,
                                           new BMessage(OPEN_FILE), 'O') );
    
    cd_menu = new CDMenu( "Open Disc" );
    mFile->AddItem( cd_menu );
    
    mFile->AddSeparatorItem();
    mFile->AddItem( mItem = new BMenuItem( "Play List" B_UTF8_ELLIPSIS,
                                           new BMessage(OPEN_PLAYLIST), 'P') );
    
    mFile->AddSeparatorItem();
    mFile->AddItem( mItem = new BMenuItem( "About" B_UTF8_ELLIPSIS,
                                       new BMessage(B_ABOUT_REQUESTED), 'A') );
    mItem->SetTarget( be_app );
    mFile->AddItem(mItem = new BMenuItem( "Quit",
                                        new BMessage(B_QUIT_REQUESTED), 'Q') );

    /* Add the Audio menu */
    menu_bar->AddItem ( mAudio = new BMenu( "Audio" ) );
    menu_bar->ResizeToPreferred();
    mAudio->AddItem( new LanguageMenu( "Language", AUDIO_ES, p_intf ) );
    mAudio->AddItem( new LanguageMenu( "Subtitles", SPU_ES, p_intf ) );

    /* Add the Navigation menu */
    menu_bar->AddItem( mNavigation = new BMenu( "Navigation" ) );
    menu_bar->ResizeToPreferred();
    mNavigation->AddItem( new BMenuItem( "Prev Title",
                                        new BMessage(PREV_TITLE)) );
    mNavigation->AddItem( new BMenuItem( "Next Title",
                                        new BMessage(NEXT_TITLE)) );
    mNavigation->AddItem( new BMenuItem( "Prev Chapter",
                                        new BMessage(PREV_CHAPTER)) );
    mNavigation->AddItem( new BMenuItem( "Next Chapter",
                                        new BMessage(NEXT_CHAPTER)) );
                                        
    ResizeTo(260,50 + menu_bar->Bounds().IntegerHeight()+1);
    controlRect = Bounds();
    controlRect.top += menu_bar->Bounds().IntegerHeight() + 1;

    p_mediaControl = new MediaControlView( controlRect );
    p_mediaControl->SetViewColor( ui_color(B_PANEL_BACKGROUND_COLOR) );
    b_empty_playlist = true;
    p_mediaControl->SetEnabled( !b_empty_playlist );

    /* Show */
    AddChild( p_mediaControl );
    Show();
    
}

InterfaceWindow::~InterfaceWindow()
{
    if (playlist_window) playlist_window->ReallyQuit();
}

/*****************************************************************************
 * InterfaceWindow::MessageReceived
 *****************************************************************************/
void InterfaceWindow::MessageReceived( BMessage * p_message )
{
    int vol_val = p_mediaControl->GetVolume();    // remember the current volume
    int playback_status;      // remember playback state
    int     i_index;
    BAlert *alert;

    Activate();
    playback_status = p_vlc_wrapper->inputGetStatus();

    switch( p_message->what )
    {
    case B_ABOUT_REQUESTED:
        alert = new BAlert(VOUT_TITLE, "BeOS " VOUT_TITLE "\n\n<www.videolan.org>", "Ok");
        alert->Go();
        break;

    case TOGGLE_ON_TOP:
        break;
        
    case OPEN_FILE:
        if( file_panel )
        {
            file_panel->Show();
            break;
        }
        file_panel = new BFilePanel();
        file_panel->SetTarget( this );
        file_panel->Show();
        b_empty_playlist = false;
        p_mediaControl->SetEnabled( !b_empty_playlist );
        break;

	case OPEN_PLAYLIST:
		{
		    BRect rect(20,20,320,420);
            playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

            playlist_window = PlayListWindow::getPlayList(rect,
                                "Playlist", p_playlist);
            playlist_window->Show();                    
        }
		break;
    case OPEN_DVD:
        {
            const char *psz_device;
            BString type("dvd");
            if( p_message->FindString("device", &psz_device) != B_ERROR )
            {
                BString device(psz_device);
                p_vlc_wrapper->openDisc(type, device, 0,0);
            }
        }
        break;

    case STOP_PLAYBACK:
        // this currently stops playback not nicely
        //p_vlc_wrapper->volume_mute();
        //snooze( 400000 );
        p_vlc_wrapper->playlistStop();
        p_mediaControl->SetStatus(NOT_STARTED_S,DEFAULT_RATE);
        break;

    case START_PLAYBACK:
        /*  starts playing in normal mode */

    case PAUSE_PLAYBACK:
        /* toggle between pause and play */
        if( p_intf->p_sys->p_input != NULL )
        {
            /* pause if currently playing */
            if ( playback_status == PLAYING_S )
            {
                //p_vlc_wrapper->volume_mute();
                //snooze( 400000 );
                p_vlc_wrapper->playlistPause();
            }
            else
            {
                //p_vlc_wrapper->volume_restore();
                p_vlc_wrapper->playlistPlay();
            }
        }
        else
        {
            /* Play a new file */
            p_vlc_wrapper->playlistPlay();
        }    
        break;

    case FASTER_PLAY:
        /* cycle the fast playback modes */
        //p_vlc_wrapper->volume_mute();
        //snooze( 400000 );
        p_vlc_wrapper->playFaster();
        break;

    case SLOWER_PLAY:
        /*  cycle the slow playback modes */
        //p_vlc_wrapper->volume_mute();
        //snooze( 400000 );
        p_vlc_wrapper->playSlower();
        break;

    case SEEK_PLAYBACK:
        /* handled by semaphores */
        break;

    case VOLUME_CHG:
        /* adjust the volume */
//        vlc_mutex_lock( &p_intf->p_sys->p_input->lock );
//        for( i_index = 0 ; i_index < p_aout_bank->i_count ; i_index++ )
//        {
//            if( p_aout_bank->pp_aout[i_index]->i_savedvolume )
//            {
//                p_aout_bank->pp_aout[i_index]->i_savedvolume = vol_val;
//            }
//            else
//            {
//                p_aout_bank->pp_aout[i_index]->i_volume = vol_val;
//            }
//        }
//        vlc_mutex_unlock( &p_aout_bank->lock );
        break;

    case VOLUME_MUTE:
        /* toggle muting */
        p_vlc_wrapper->toggleMute( );
        break;

    case SELECT_AUDIO:
        {
            int32 i = p_message->FindInt32( "audio" );
            p_vlc_wrapper->toggleLanguage( i );
        }
        break;

    case SELECT_SUBTITLE:
        {
            int32 i = p_message->FindInt32( "subtitle" );
            p_vlc_wrapper->toggleSubtitle( i );
        }
        break;
    case PREV_TITLE:
        {
            int             i_id;
            i_id = p_intf->p_sys->p_input->stream.p_selected_area->i_id - 1;

            /* Disallow area 0 since it is used for video_ts.vob */
            if( i_id > 0 )
            {
                p_vlc_wrapper->toggleTitle(i_id);
            }
            break;
        }
    case NEXT_TITLE:
        {
            int             i_id;

            i_id = p_intf->p_sys->p_input->stream.p_selected_area->i_id + 1;

            if( i_id < p_intf->p_sys->p_input->stream.i_area_nb )
            {
                p_vlc_wrapper->toggleTitle(i_id);
            }
        }
        break;
    case PREV_CHAPTER:
        {
            int             i_id;

            i_id = p_intf->p_sys->p_input->stream.p_selected_area->i_part - 1;

            if( i_id >= 0 )
            {
                p_vlc_wrapper->toggleChapter(i_id);
            }
        }
        break;
    case NEXT_CHAPTER:
        {
            int             i_id;

            i_id = p_intf->p_sys->p_input->stream.p_selected_area->i_part + 1;

            if( i_id >= 0 )
            {
                p_vlc_wrapper->toggleChapter(i_id);
            }
        }
        break;
    case B_REFS_RECEIVED:
    case B_SIMPLE_DATA:
        {
            entry_ref ref;
            BList* files = new BList();

            int i = 0;
            while( p_message->FindRef( "refs", i, &ref ) == B_OK )
            {
                BPath path( &ref );

                files->AddItem(new BString((char*)path.Path()) );
                i++;
            }
            p_vlc_wrapper->openFiles(files);
            delete files;
        }
        break;

    default:
        BWindow::MessageReceived( p_message );
        break;
    }

}

/*****************************************************************************
 * InterfaceWindow::updateInterface
 *****************************************************************************/
void InterfaceWindow::updateInterface()
{

	if ( p_intf->p_sys->p_input != NULL )
	{
        if ( acquire_sem(p_mediaControl->fScrubSem) == B_OK )
        {
            p_vlc_wrapper->setTimeAsFloat(p_mediaControl->GetSeekTo());
        }
        else if( Lock() )
        {
            p_mediaControl->SetStatus(p_intf->p_sys->p_input->stream.control.i_status, 
                                      p_intf->p_sys->p_input->stream.control.i_rate);
            p_mediaControl->SetProgress(p_intf->p_sys->p_input->stream.p_selected_area->i_tell,
                                        p_intf->p_sys->p_input->stream.p_selected_area->i_size);
            Unlock();
        }
    }
    
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if ( b_empty_playlist != (p_playlist->i_size < 1) )
    {
        if (Lock())
        {
            b_empty_playlist = !b_empty_playlist;
            p_mediaControl->SetEnabled( !b_empty_playlist );
            Unlock();
        }
    }
}

/*****************************************************************************
 * InterfaceWindow::QuitRequested
 *****************************************************************************/
bool InterfaceWindow::QuitRequested()
{
    p_intf->p_vlc->b_die = VLC_TRUE;

    return( true );
}

/*****************************************************************************
 * CDMenu::CDMenu
 *****************************************************************************/
CDMenu::CDMenu(const char *name)
      : BMenu(name)
{
}

/*****************************************************************************
 * CDMenu::~CDMenu
 *****************************************************************************/
CDMenu::~CDMenu()
{
}

/*****************************************************************************
 * CDMenu::AttachedToWindow
 *****************************************************************************/
void CDMenu::AttachedToWindow(void)
{
    while (RemoveItem((long int)0) != NULL);  // remove all items
    GetCD("/dev/disk");
    BMenu::AttachedToWindow();
}

/*****************************************************************************
 * CDMenu::GetCD
 *****************************************************************************/
int CDMenu::GetCD( const char *directory )
{
	BVolumeRoster *volRoster;
	BVolume       *vol;
	BDirectory    *dir;
	int           status;
	int           mounted;   
	char          name[B_FILE_NAME_LENGTH]; 
    fs_info       info;
	dev_t         dev;
	
	volRoster = new BVolumeRoster();
	vol = new BVolume();
	dir = new BDirectory();
	status = volRoster->GetNextVolume(vol);
	status = vol->GetRootDirectory(dir);
	while (status ==  B_NO_ERROR)
	{
	    mounted = vol->GetName(name);	
	    if ((mounted == B_OK) && /* Disk is currently Mounted */
	        (vol->IsReadOnly()) ) /* Disk is read-only */
	    {
	        dev = vol->Device();
            fs_stat_dev(dev, &info);
            
            device_geometry g;
            int i_dev;
            i_dev = open( info.device_name, O_RDONLY );
           
            if( i_dev >= 0 )
            {
                if( ioctl(i_dev, B_GET_GEOMETRY, &g, sizeof(g)) >= 0 )
                {
                    if( g.device_type == B_CD ) //ensure the drive is a CD-ROM
                    {
                        BMessage *msg;
                        msg = new BMessage( OPEN_DVD );
                        msg->AddString( "device", info.device_name );
                        BMenuItem *menu_item;
                        menu_item = new BMenuItem( name, msg );
                        AddItem( menu_item );
                    }
                    close(i_dev);
                }
            }
 	    }
 	    vol->Unset();
	    status = volRoster->GetNextVolume(vol);
	}
}

/*****************************************************************************
 * LanguageMenu::LanguageMenu
 *****************************************************************************/
LanguageMenu::LanguageMenu(const char *name, int menu_kind, 
                            intf_thread_t  *p_interface)
    :BMenu(name)
{
    kind = menu_kind;
    p_intf = p_interface;
}

/*****************************************************************************
 * LanguageMenu::~LanguageMenu
 *****************************************************************************/
LanguageMenu::~LanguageMenu()
{
}

/*****************************************************************************
 * LanguageMenu::AttachedToWindow
 *****************************************************************************/
void LanguageMenu::AttachedToWindow(void)
{
    while( RemoveItem((long int)0) != NULL )
    {
        ; // remove all items
    }

    SetRadioMode(true);
    GetChannels();
    BMenu::AttachedToWindow();
}

/*****************************************************************************
 * LanguageMenu::GetChannels
 *****************************************************************************/
int LanguageMenu::GetChannels()
{
    char  *psz_name;
    bool   b_active;
    BMessage *msg;
    int    i;
    es_descriptor_t *p_es  = NULL;

    /* Insert the null */
    if( kind == SPU_ES ) //audio
    {
        msg = new BMessage(SELECT_SUBTITLE);
        msg->AddInt32("subtitle", -1);
        BMenuItem *menu_item;
        menu_item = new BMenuItem("None", msg);
        AddItem(menu_item);
        menu_item->SetMarked(TRUE);

    }

    if( p_intf->p_sys->p_input == NULL )
    {
        return 1;
    }


    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
    for( i = 0; i < p_intf->p_sys->p_input->stream.i_selected_es_number; i++ )
    {
        if( kind == p_intf->p_sys->p_input->stream.pp_selected_es[i]->i_cat )
        {
            p_es = p_intf->p_sys->p_input->stream.pp_selected_es[i];
        }
    }

    for( i = 0; i < p_intf->p_sys->p_input->stream.i_es_number; i++ )
    {
        if( kind == p_intf->p_sys->p_input->stream.pp_es[i]->i_cat )
        {
            psz_name = p_intf->p_sys->p_input->stream.pp_es[i]->psz_desc;
            if( kind == AUDIO_ES ) //audio
            {
                msg = new BMessage(SELECT_AUDIO);
                msg->AddInt32("audio", i);
            }
            else
            {
                msg = new BMessage(SELECT_SUBTITLE);
                msg->AddInt32("subtitle", i);
            }
            BMenuItem *menu_item;
            menu_item = new BMenuItem(psz_name, msg);
            AddItem(menu_item);
            b_active = (p_es == p_intf->p_sys->p_input->stream.pp_es[i]);
            menu_item->SetMarked(b_active);
        }
    }
    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );

}



