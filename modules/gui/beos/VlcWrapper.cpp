/*****************************************************************************
 * VlcWrapper.cpp: BeOS plugin for vlc (derived from MacOS X port)
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: VlcWrapper.cpp,v 1.36 2003/07/23 01:13:47 gbazin Exp $
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
  #include <input_ext-plugins.h> // needed here when compiling without plugins
  #include <audio_output.h>
  #include <aout_internal.h>
}

#include "VlcWrapper.h"
#include "MsgVals.h"

const char * _AddEllipsis( char * string )
{
    char * temp;
    temp = (char*) calloc( strlen( string ) + 4, 1 );
    sprintf( temp, "%s%s", string, B_UTF8_ELLIPSIS );
    return temp;
}

/* constructor */
VlcWrapper::VlcWrapper( intf_thread_t *p_interface )
{
    p_intf = p_interface;
    p_input = NULL;
    p_playlist = (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
}

/* destructor */
VlcWrapper::~VlcWrapper()
{
    if( p_input )
        vlc_object_release( p_input );

    if( p_playlist )
        vlc_object_release( p_playlist );
}

/* UpdateInput: updates p_input */
void VlcWrapper::UpdateInput()
{
    if( !p_input )
        p_input = (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                     FIND_ANYWHERE );
        
    if( p_input )
        if( p_input->b_dead )
        {
            vlc_object_release( p_input );
            p_input = NULL;
        }
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
        return UNDEF_S;
    
    return p_input->stream.control.i_status;
}

int VlcWrapper::InputRate()
{
    if( !p_input )
        return DEFAULT_RATE;
    
    return p_input->stream.control.i_rate;
}

void VlcWrapper::InputSetRate( int rate )
{
    if( !p_input )
        return;

    input_SetRate( p_input, rate );
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
        menuItem = new BMenuItem( _("None"), message );
        if( !p_es )
            menuItem->SetMarked( true );
        list->AddItem( menuItem );
        
        for( i = 0; i < p_input->stream.i_es_number; i++ )
        {
            if( p_input->stream.pp_es[i]->i_cat == i_cat )
            {
                message = new BMessage( what );
                message->AddInt32( fieldName, i );
                if( !p_input->stream.pp_es[i]->psz_desc ||
                    !*p_input->stream.pp_es[i]->psz_desc )
                    trackName = _("<unknown>");
                else
                    trackName = strdup( p_input->stream.pp_es[i]->psz_desc );
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
                   (long long)(p_input->stream.p_selected_area->i_size
                       * f_position / SEEKSLIDER_RANGE ), 
                   INPUT_SEEK_SET );
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

void VlcWrapper::OpenFiles( BList* o_files, bool replace, int32 index )
{
	if ( o_files && o_files->CountItems() > 0)
	{
	    int size = PlaylistSize();
		bool wasEmpty = ( size < 1 );
		if ( index == -1 )
			index = PLAYLIST_END;
		int mode = index == PLAYLIST_END ? PLAYLIST_APPEND : PLAYLIST_INSERT;
	
	    /* delete current playlist */
	    if( replace )
	    {
	        for( int i = 0; i < size; i++ )
	        {
	            playlist_Delete( p_playlist, 0 );
	        }
	    }
	
	    /* insert files */
	    int32 count = o_files->CountItems();
	    for ( int32 i = count - 1; i >= 0; i-- )
	    {
	    	if ( BString* o_file = (BString *)o_files->RemoveItem( i ) )
	    	{
		        playlist_Add( p_playlist, o_file->String(),
		                      0, 0, mode, index );
		        if ( mode == PLAYLIST_INSERT )
		        	index++;
		        delete o_file;
	    	}
	    }
	    // TODO: implement a user setting
	    // if to start automatically
	    /* eventually restart playing */
	    if( replace || wasEmpty )
	    {
	        playlist_Stop( p_playlist );
	        playlist_Play( p_playlist );
	    }
	}
}
 
void VlcWrapper::OpenDisc(BString o_type, BString o_device, int i_title, int i_chapter)
{
    if( config_GetInt( p_intf, "beos-dvdmenus" ) )
        o_device.Prepend( "dvdplay:" );
    else
        o_device.Prepend( "dvdold:" );
    playlist_Add( p_playlist, o_device.String(), 0, 0,
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
	    vlc_mutex_lock( &p_playlist->object_lock );

		maxIndex = p_playlist->i_size;
		if ( maxIndex > 0 )
			currentIndex = p_playlist->i_index/* + 1 -> why?!?*/;
		else
			maxIndex = -1;

	    vlc_mutex_unlock( &p_playlist->object_lock );
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

/*************************
 * Playlist manipulation *
 *************************/

// PlaylistLock
bool
VlcWrapper::PlaylistLock() const
{
// TODO: search and destroy -> deadlock!
return true;
	if ( p_playlist )
	{
 		vlc_mutex_lock( &p_playlist->object_lock );
 		return true;
 	}
 	return false;
}

// PlaylistUnlock
void
VlcWrapper::PlaylistUnlock() const
{
// TODO: search and destroy -> deadlock!
return;
	vlc_mutex_unlock( &p_playlist->object_lock );
}

// PlaylistItemAt
void*
VlcWrapper::PlaylistItemAt( int index ) const
{
	playlist_item_t* item = NULL;
	if ( index >= 0 && index < p_playlist->i_size )
		item = p_playlist->pp_items[index];
	return (void*)item;
}

// PlaylistRemoveItem
void*
VlcWrapper::PlaylistRemoveItem( int index ) const
{
	playlist_item_t* copy = NULL;
	// check if item exists at the provided index
	if ( index >= 0 && index < p_playlist->i_size )
	{
		playlist_item_t* item = p_playlist->pp_items[index];
		if ( item )
		{
			// make a copy of the removed item
			copy = (playlist_item_t*)PlaylistCloneItem( (void*)item );
			// remove item from playlist (unfortunately, this frees it)
			playlist_Delete( p_playlist, index );
		}
	}
	return (void*)copy;
}

// PlaylistRemoveItem
void*
VlcWrapper::PlaylistRemoveItem( void* item ) const
{
	playlist_item_t* copy = NULL;
	for ( int32 i = 0; i < p_playlist->i_size; i++ )
	{
		if ( p_playlist->pp_items[i] == item )
		{
			copy = (playlist_item_t*)PlaylistRemoveItem( i );
			break;
		}
	}
	return (void*)copy;
}

// PlaylistAddItem
bool
VlcWrapper::PlaylistAddItem( void* item, int index ) const
{
	if ( item )
	{
		playlist_AddItem( p_playlist, (playlist_item_t*)item,
						  PLAYLIST_INSERT, index );
	}
	// TODO: once playlist is returning useful info, return that instead
	return true;
}

// PlaylistCloneItem
void*
VlcWrapper::PlaylistCloneItem( void* castToItem ) const
{
	playlist_item_t* copy = NULL;
	playlist_item_t* item = (playlist_item_t*)castToItem;
	if ( item )
	{
		copy = (playlist_item_t*)malloc( sizeof( playlist_item_t ) );
		if ( copy )
		{
			// make a copy of the item at index
			copy->psz_name = strdup( item->psz_name );
			copy->psz_uri  = strdup( item->psz_uri );
			copy->i_type = item->i_type;
			copy->i_status = item->i_status;
			copy->b_autodeletion = item->b_autodeletion;
		}
	}
	return (void*)copy;
}

// Careful! You need to know what you're doing here!
// The reason for having it, is to be able to deal with
// the rather lame list implementation of the playlist.
// It is meant to help manipulate the playlist with the above
// methods while keeping it valid.
//
// PlaylistSetPlaying
void
VlcWrapper::PlaylistSetPlaying( int index ) const
{
	if ( index < 0 )
		index = 0;
	if ( index >= p_playlist->i_size )
		index = p_playlist->i_size - 1;
	p_playlist->i_index = index;
}


/*********
 * audio *
 *********/

unsigned short VlcWrapper::GetVolume()
{
    unsigned short i_volume;
    aout_VolumeGet( p_intf, (audio_volume_t*)&i_volume );
    return i_volume;
}

void VlcWrapper::SetVolume( int value )
{
    if ( p_intf->p_sys->b_mute )
    {
        p_intf->p_sys->b_mute = 0;
    }
    aout_VolumeSet( p_intf, value );
}

void VlcWrapper::VolumeMute()
{
    aout_VolumeMute( p_intf, NULL );
    p_intf->p_sys->b_mute = 1;
}

void VlcWrapper::VolumeRestore()
{
    audio_volume_t dummy;
    aout_VolumeMute( p_intf, &dummy );
    p_intf->p_sys->b_mute = 0;
}

bool VlcWrapper::IsMuted()
{
    return p_intf->p_sys->b_mute;
}

/*******
 * DVD *
 *******/

bool VlcWrapper::IsUsingMenus()
{
    if( !p_input )
        return false;

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_index < 0 )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        return false;
    }
    
    char * psz_name = p_playlist->pp_items[p_playlist->i_index]->psz_name;
    if( !strncmp( psz_name, "dvdplay:", 8 ) )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        return true;
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    return false;
}

bool VlcWrapper::HasTitles()
{
    if( !p_input )
        return false;

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
 
void VlcWrapper::LoadSubFile( const char * psz_file )
{
    config_PutPsz( p_intf, "sub-file", strdup( psz_file ) );
}

void VlcWrapper::FilterChange()
{
    if( !p_input )
        return;
    
    vout_thread_t * p_vout;
    vlc_mutex_lock( &p_input->stream.stream_lock );

    // Warn the vout we are about to change the filter chain
    p_vout = (vout_thread_t*)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
    if( p_vout )
    {
        p_vout->b_filter_change = VLC_TRUE;
        vlc_object_release( p_vout );
    }

    // restart all video stream
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
