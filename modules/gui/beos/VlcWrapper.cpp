/*****************************************************************************
 * VlcWrapper.cpp: BeOS plugin for vlc (derived from MacOS X port)
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: VlcWrapper.cpp,v 1.23 2003/01/29 00:02:09 titer Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Tony Casltey <tony@castley.net>
 *          Stephan Aßmus <stippi@yellowbites.com>
 *          Eric Petit <titer@videolan.org>
 *
 * This program is free software{} you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation{} either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY{} without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program{} if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <AppKit.h>
#include <InterfaceKit.h>
#include <SupportKit.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>
extern "C"
{
  #include <audio_output.h>
  #include <aout_internal.h>
}

#include "VlcWrapper.h"
#include "MsgVals.h"

/* constructor */
VlcWrapper::VlcWrapper( intf_thread_t *p_interface )
{
    p_intf = p_interface;
    p_input = NULL;
    p_aout = NULL;
    p_playlist = (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
}

/* destructor */
VlcWrapper::~VlcWrapper()
{
    if( p_input )
    {
        vlc_object_release( p_input );
    }
    if( p_playlist )
    {
        vlc_object_release( p_playlist );
    }
    if( p_aout )
    {
        vlc_object_release( p_aout );
    }
}

/* UpdateInputAndAOut: updates p_input and p_aout, returns true if the
   interface needs to be updated */
bool VlcWrapper::UpdateInputAndAOut()
{
    if( p_input == NULL )
    {
        p_input = (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                     FIND_ANYWHERE );
    }
    if( p_aout == NULL )
    {
        p_aout = (aout_instance_t*)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                    FIND_ANYWHERE );
    }
        
    if( p_input != NULL )
    {
        if( p_input->b_dead )
        {
            vlc_object_release( p_input );
            p_input = NULL;
            
            if( p_aout )
            {
                vlc_object_release( p_aout );
                p_aout = NULL;
            }
        }
        return true;
    }
    return false;
}


/***************************
 * input infos and control *
 ***************************/

bool VlcWrapper::HasInput()
{
    return ( p_input != NULL );
}

int VlcWrapper::InputStatus()
{
    if( !p_input )
    {
        return UNDEF_S;
    }
    return p_input->stream.control.i_status;
}

int VlcWrapper::InputRate()
{
    if( !p_input )
    {
        return DEFAULT_RATE;
    }
    return p_input->stream.control.i_rate;
}

void VlcWrapper::InputSlower()
{
    if( p_input != NULL )
    {
        input_SetStatus( p_input, INPUT_STATUS_SLOWER );
    }
}

void VlcWrapper::InputFaster()
{
    if( p_input != NULL )
    {
        input_SetStatus( p_input, INPUT_STATUS_FASTER );
    }
}

BList * VlcWrapper::GetChannels( int i_cat )
{
    if( p_input )
    {
        unsigned int i;
        uint32 what;
        const char* fieldName;

        switch( i_cat )
        {
            case AUDIO_ES:
            {
                what = SELECT_CHANNEL;
                fieldName = "channel";
                break;
            }
            case SPU_ES:
            {
                what = SELECT_SUBTITLE;
                fieldName = "subtitle";
                break;
            }
            default:
            return NULL;
       }

        vlc_mutex_lock( &p_input->stream.stream_lock );
      
        /* find which track is currently playing */
        es_descriptor_t *p_es = NULL;
        for( i = 0; i < p_input->stream.i_selected_es_number; i++ )
        {
            if( p_input->stream.pp_selected_es[i]->i_cat == i_cat )
                p_es = p_input->stream.pp_selected_es[i];
        }
        
        /* build a list of all tracks */
        BList *list = new BList( p_input->stream.i_es_number );
        BMenuItem *menuItem;
        BMessage *message;
        char *trackName;
        
        /* "None" */
        message = new BMessage( what );
        message->AddInt32( fieldName, -1 );
        menuItem = new BMenuItem( "None", message );
        if( !p_es )
            menuItem->SetMarked( true );
        list->AddItem( menuItem );
        
        for( i = 0; i < p_input->stream.i_es_number; i++ )
        {
            if( p_input->stream.pp_es[i]->i_cat == i_cat )
            {
                message = new BMessage( what );
                message->AddInt32( fieldName, i );
                if( strlen( p_input->stream.pp_es[i]->psz_desc ) )
                    trackName = strdup( p_input->stream.pp_es[i]->psz_desc );
                else
                    trackName = "<unknown>";
                menuItem = new BMenuItem( trackName, message );
                if( p_input->stream.pp_es[i] == p_es )
                    menuItem->SetMarked( true );
                list->AddItem( menuItem );
            }
        }
        
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        return list;
    }
    return NULL;
}

void VlcWrapper::ToggleLanguage( int i_language )
{
    es_descriptor_t * p_es = NULL;
    es_descriptor_t * p_es_old = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    for( unsigned int i = 0; i < p_input->stream.i_selected_es_number ; i++ )
    {
        if( p_input->stream.pp_selected_es[i]->i_cat == AUDIO_ES )
        {
            p_es_old = p_input->stream.pp_selected_es[i];
            break;
        }
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    
    if( i_language != -1 )
    {
        p_es = p_input->stream.pp_es[i_language];
    }
    if( p_es == p_es_old )
    {
        return;
    }
    if( p_es_old )
    {
        input_ToggleES( p_input, p_es_old, VLC_FALSE );
    }
    if( p_es )
    {
        input_ToggleES( p_input, p_es, VLC_TRUE );
    }
}

void VlcWrapper::ToggleSubtitle( int i_subtitle )
{
    es_descriptor_t * p_es = NULL;
    es_descriptor_t * p_es_old = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    for( unsigned int i = 0; i < p_input->stream.i_selected_es_number ; i++ )
    {
        if( p_input->stream.pp_selected_es[i]->i_cat == SPU_ES )
        {
            p_es_old = p_input->stream.pp_selected_es[i];
            break;
        }
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    
    if( i_subtitle != -1 )
    {
        p_es = p_input->stream.pp_es[i_subtitle];
    }
    if( p_es == p_es_old )
    {
        return;
    }
    if( p_es_old )
    {
        input_ToggleES( p_input, p_es_old, VLC_FALSE );
    }
    if( p_es )
    {
        input_ToggleES( p_input, p_es, VLC_TRUE );
    }
}

const char * VlcWrapper::GetTimeAsString()
{
    static char psz_currenttime[ OFFSETTOTIME_MAX_SIZE ];
        
    if( p_input == NULL )
    {
        return ("-:--:--");
    }     
   
    input_OffsetToTime( p_input, 
                        psz_currenttime, 
                        p_input->stream.p_selected_area->i_tell );        

    return(psz_currenttime);
}

float VlcWrapper::GetTimeAsFloat()
{
    float f_time = 0.0;

    if( p_input != NULL )
    {
        f_time = (float)p_input->stream.p_selected_area->i_tell / 
                 (float)p_input->stream.p_selected_area->i_size;
    }    
    else
    {
        f_time = 0.0;
    }
    return( f_time );
}

void VlcWrapper::SetTimeAsFloat( float f_position )
{
    if( p_input != NULL )
    {
        input_Seek( p_input, 
                   (long long int)(p_input->stream.p_selected_area->i_size
                       * f_position / SEEKSLIDER_RANGE ), 
                   INPUT_SEEK_SET);
    }
}

bool VlcWrapper::IsPlaying()
{

	bool playing = false;
	if ( p_input )
	{
		switch ( p_input->stream.control.i_status )
		{
			case PLAYING_S:
			case FORWARD_S:
			case BACKWARD_S:
			case START_S:
				playing = true;
	            break;
			case PAUSE_S:
			case UNDEF_S:
			case NOT_STARTED_S:
			default:
				break;
		}
	}
	return playing;

}

/************
 * playlist *
 ************/

void VlcWrapper::OpenFiles( BList* o_files, bool replace )
{
    BString *o_file;
    int size = PlaylistSize();
	bool wasEmpty = ( size < 1 );

    /* delete current playlist */
    if( replace )
    {
        for( int i = 0; i < size; i++ )
        {
            playlist_Delete( p_playlist, 0 );
        }
    }

    /* append files */
    while( ( o_file = (BString *)o_files->LastItem() ) )
    {
        playlist_Add( p_playlist, o_file->String(),
                      PLAYLIST_APPEND, PLAYLIST_END );
        o_files->RemoveItem(o_files->CountItems() - 1);
    }
    
    /* eventually restart playing */
    if( replace || wasEmpty )
    {
        playlist_Stop( p_playlist );
        playlist_Play( p_playlist );
    }
}
 
void VlcWrapper::OpenDisc(BString o_type, BString o_device, int i_title, int i_chapter)
{
    if( p_intf->p_sys->b_dvdmenus )
        o_device.Prepend( "dvd:" );
    else
        o_device.Prepend( "dvdold:" );
    playlist_Add( p_playlist, o_device.String(),
                  PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
}

int VlcWrapper::PlaylistSize()
{
    vlc_mutex_lock( &p_playlist->object_lock );
    int i_size = p_playlist->i_size;
    vlc_mutex_unlock( &p_playlist->object_lock );
    return i_size;
}

char * VlcWrapper::PlaylistItemName( int i )
{
   return p_playlist->pp_items[i]->psz_name;
}

int VlcWrapper::PlaylistCurrent()
{
    return p_playlist->i_index;
}

bool VlcWrapper::PlaylistPlay()
{
    if( PlaylistSize() )
    {
        playlist_Play( p_playlist );
    }
    return( true );
}

void VlcWrapper::PlaylistPause()
{
    if( p_input )
    {
        input_SetStatus( p_input, INPUT_STATUS_PAUSE );
    }
}

void VlcWrapper::PlaylistStop()
{
    playlist_Stop( p_playlist );
}

void VlcWrapper::PlaylistNext()
{
    playlist_Next( p_playlist );
}

void VlcWrapper::PlaylistPrev()
{
    playlist_Prev( p_playlist );
}

void VlcWrapper::GetPlaylistInfo( int32& currentIndex, int32& maxIndex )
{
	currentIndex = -1;
	maxIndex = -1;
	if ( p_playlist )
	{
		maxIndex = p_playlist->i_size;
		if ( maxIndex > 0 )
			currentIndex = p_playlist->i_index + 1;
		else
			maxIndex = -1;
	}
}

void VlcWrapper::PlaylistJumpTo( int pos )
{
    playlist_Goto( p_playlist, pos );
}

void VlcWrapper::GetNavCapabilities( bool *canSkipPrev, bool *canSkipNext )
{
	if ( canSkipPrev && canSkipNext )
	{
		// init the parameters
		*canSkipPrev = false;
		*canSkipNext = false;
		// get playlist info
		int pos = PlaylistCurrent();
		int size = PlaylistSize();

		// see if we have got a stream going		
		if ( p_input )
		{
			vlc_mutex_lock( &p_input->stream.stream_lock );

			bool hasTitles = p_input->stream.i_area_nb > 1;
			int numChapters = p_input->stream.p_selected_area->i_part_nb;
			bool hasChapters = numChapters > 1;
			// first, look for chapters
			if ( hasChapters )
			{
				*canSkipPrev = p_input->stream.p_selected_area->i_part > 0;
				*canSkipNext = p_input->stream.p_selected_area->i_part <
									 p_input->stream.p_selected_area->i_part_nb - 1;
			}
			// if one of the skip capabilities is false,
			// make it depend on titles instead
			if ( !*canSkipPrev && hasTitles )
				*canSkipPrev = p_input->stream.p_selected_area->i_id > 1;
			if ( !*canSkipNext && hasTitles )
				*canSkipNext = p_input->stream.p_selected_area->i_id <
				                   p_input->stream.i_area_nb - 1;

			vlc_mutex_unlock( &p_input->stream.stream_lock );
		}
		// last but not least, make capabilities depend on playlist
		if ( !*canSkipPrev )
			*canSkipPrev = pos > 0;
		if ( !*canSkipNext )
			*canSkipNext = pos < size - 1;
	}
}

void VlcWrapper::NavigatePrev()
{
	bool hasSkiped = false;

	// see if we have got a stream going		
	if ( p_input )
	{
		// get information from stream (lock it while looking at it)
		vlc_mutex_lock( &p_input->stream.stream_lock );

		int currentTitle = p_input->stream.p_selected_area->i_id;
		int currentChapter = p_input->stream.p_selected_area->i_part;
		int numTitles = p_input->stream.i_area_nb;
		bool hasTitles = numTitles > 1;
		int numChapters = p_input->stream.p_selected_area->i_part_nb;
		bool hasChapters = numChapters > 1;

		vlc_mutex_unlock( &p_input->stream.stream_lock );

		// first, look for chapters
		if ( hasChapters )
		{
			// skip to the previous chapter
			currentChapter--;

			if ( currentChapter >= 0 )
			{
				ToggleChapter( currentChapter );
				hasSkiped = true;
			}
		}
		// if we couldn't skip chapters, try titles instead
		if ( !hasSkiped && hasTitles )
		{
			// skip to the previous title
			currentTitle--;
			// disallow area 0 since it is used for video_ts.vob
			if( currentTitle > 0 )
			{
				ToggleTitle(currentTitle);
				hasSkiped = true;
			}
		}

	}
	// last but not least, skip to previous file
	if ( !hasSkiped )
		PlaylistPrev();
}

void VlcWrapper::NavigateNext()
{
	bool hasSkiped = false;

	// see if we have got a stream going		
	if ( p_input )
	{
		// get information from stream (lock it while looking at it)
		vlc_mutex_lock( &p_input->stream.stream_lock );

		int currentTitle = p_input->stream.p_selected_area->i_id;
		int currentChapter = p_input->stream.p_selected_area->i_part;
		int numTitles = p_input->stream.i_area_nb;
		bool hasTitles = numTitles > 1;
		int numChapters = p_input->stream.p_selected_area->i_part_nb;
		bool hasChapters = numChapters > 1;

		vlc_mutex_unlock( &p_input->stream.stream_lock );

		// first, look for chapters
		if ( hasChapters )
		{
			// skip to the next chapter
			currentChapter++;
			if ( currentChapter < numChapters )
			{
				ToggleChapter( currentChapter );
				hasSkiped = true;
			}
		}
		// if we couldn't skip chapters, try titles instead
		if ( !hasSkiped && hasTitles )
		{
			// skip to the next title
			currentTitle++;
			// disallow area 0 since it is used for video_ts.vob
			if ( currentTitle < numTitles - 1 )
			{
				ToggleTitle(currentTitle);
				hasSkiped = true;
			}
		}

	}
	// last but not least, skip to next file
	if ( !hasSkiped )
		PlaylistNext();
}


/*********
 * audio *
 *********/

bool VlcWrapper::HasAudio()
{
    return( p_aout != NULL );
}

unsigned short VlcWrapper::GetVolume()
{
    if( p_aout != NULL )
    {
        unsigned short i_volume;
        aout_VolumeGet( p_aout, (audio_volume_t*)&i_volume );
        return i_volume;
    }
    return 0;
}

void VlcWrapper::SetVolume(int value)
{
    if( p_aout != NULL )
    {
		if ( p_intf->p_sys->b_mute )
		{
			p_intf->p_sys->b_mute = 0;
		}
        aout_VolumeSet( p_aout, value );
    }
}

void VlcWrapper::VolumeMute()
{
    if( p_aout != NULL )
   	{
   	    aout_VolumeGet( p_aout, &p_intf->p_sys->i_saved_volume );
	    aout_VolumeMute( p_aout, NULL );
	    p_intf->p_sys->b_mute = 1;
   	}
}

void VlcWrapper::VolumeRestore()
{
    if( p_aout != NULL )
   	{
        aout_VolumeSet( p_aout, p_intf->p_sys->i_saved_volume );
        p_intf->p_sys->b_mute = 0;
	}
}

bool VlcWrapper::IsMuted()
{
    return p_intf->p_sys->b_mute;
}

/*******
 * DVD *
 *******/

bool VlcWrapper::HasTitles()
{
    if( !p_input )
    {
        return false;
    }
    return ( p_input->stream.i_area_nb > 1 );
}

BList * VlcWrapper::GetTitles()
{
    if( p_input )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
      
        BList *list = new BList( p_input->stream.i_area_nb );
        BMenuItem *menuItem;
        BMessage *message;
        
        for( unsigned int i = 1; i < p_input->stream.i_area_nb; i++ )
        {
            message = new BMessage( TOGGLE_TITLE );
            message->AddInt32( "index", i );
            BString helper( "" );
            helper << i;
            menuItem = new BMenuItem( helper.String(), message );
            menuItem->SetMarked( p_input->stream.p_selected_area->i_id == i );
            list->AddItem( menuItem );
        }
        
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        return list;
    }
    return NULL;
}

void VlcWrapper::PrevTitle()
{
    int i_id;
    i_id = p_input->stream.p_selected_area->i_id - 1;
    if( i_id > 0 )
    {
        ToggleTitle(i_id);
    }
}

void VlcWrapper::NextTitle()
{
    unsigned int i_id;
    i_id = p_input->stream.p_selected_area->i_id + 1;
    if( i_id < p_input->stream.i_area_nb )
    {
        ToggleTitle(i_id);
    }
}

void VlcWrapper::ToggleTitle(int i_title)
{
    if( p_input != NULL )
    {
        input_ChangeArea( p_input,
                          p_input->stream.pp_areas[i_title] );

        vlc_mutex_lock( &p_input->stream.stream_lock );

        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
}

void VlcWrapper::TitleInfo( int32 &currentIndex, int32 &maxIndex )
{
	currentIndex = -1;
	maxIndex = -1;
	if ( p_input )
	{
		vlc_mutex_lock( &p_input->stream.stream_lock );

		maxIndex = p_input->stream.i_area_nb - 1;
		if ( maxIndex > 0)
			currentIndex = p_input->stream.p_selected_area->i_id;
		else
			maxIndex = -1;

		vlc_mutex_unlock( &p_input->stream.stream_lock );
	}
}

bool VlcWrapper::HasChapters()
{
    if( !p_input )
    {
        return false;
    }
    return ( p_input->stream.p_selected_area->i_part_nb > 1 );
}

BList * VlcWrapper::GetChapters()
{
    if( p_input )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
      
        BList *list = new BList( p_input->stream.p_selected_area->i_part_nb );
        BMenuItem *menuItem;
        BMessage *message;
        
        for( unsigned int i = 1;
             i < p_input->stream.p_selected_area->i_part_nb + 1; i++ )
        {
            message = new BMessage( TOGGLE_CHAPTER );
            message->AddInt32( "index", i );
            BString helper( "" );
            helper << i;
            menuItem = new BMenuItem( helper.String(), message );
            menuItem->SetMarked( p_input->stream.p_selected_area->i_part == i );
            list->AddItem( menuItem );
        }
        
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        return list;
    }
    return NULL;
}

void VlcWrapper::PrevChapter()
{
    int i_id;
    i_id = p_input->stream.p_selected_area->i_part - 1;
    if( i_id >= 0 )
    {
        ToggleChapter(i_id);
    }
}

void VlcWrapper::NextChapter()
{
    int i_id;
    i_id = p_input->stream.p_selected_area->i_part + 1;
    if( i_id >= 0 )
    {
        ToggleChapter(i_id);
    }
}

void VlcWrapper::ToggleChapter(int i_chapter)
{
    if( p_input != NULL )
    {
        p_input->stream.p_selected_area->i_part = i_chapter;
        input_ChangeArea( p_input,
                          p_input->stream.p_selected_area );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
}

void VlcWrapper::ChapterInfo( int32 &currentIndex, int32 &maxIndex )
{
	currentIndex = -1;
	maxIndex = -1;
	if ( p_input )
	{
		vlc_mutex_lock( &p_input->stream.stream_lock );

		maxIndex = p_input->stream.p_selected_area->i_part_nb - 1;
		if ( maxIndex > 0)
			currentIndex = p_input->stream.p_selected_area->i_part;
		else
			maxIndex = -1;

		vlc_mutex_unlock( &p_input->stream.stream_lock );
	}
}

/****************
 * Miscellanous *
 ****************/
 
void VlcWrapper::LoadSubFile( char * psz_file )
{
    config_PutPsz( p_intf, "sub-file", strdup( psz_file ) );
}

void VlcWrapper::FilterChange()
{
    if( !p_input )
        return;
    
    vout_thread_t * p_vout;
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Warn the vout we are about to change the filter chain */
    p_vout = (vout_thread_t*)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
    if( p_vout )
    {
        p_vout->b_filter_change = VLC_TRUE;
        vlc_object_release( p_vout );
    }

    /* restart all video stream */
    for( unsigned int i = 0; i < p_input->stream.i_es_number; i++ )
    {
        if( ( p_input->stream.pp_es[i]->i_cat == VIDEO_ES ) &&
            ( p_input->stream.pp_es[i]->p_decoder_fifo != NULL ) )
        {
            input_UnselectES( p_input, p_input->stream.pp_es[i] );
            input_SelectES( p_input, p_input->stream.pp_es[i] );
        }
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}
