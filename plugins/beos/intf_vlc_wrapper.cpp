/*****************************************************************************
 * intf_vlc_wrapper.h: BeOS plugin for vlc (derived from MacOS X port )
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.cpp,v 1.1.2.4 2002/10/09 15:29:51 stippi Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Tony Casltey <tony@castley.net>
 *          Stephan Aßmus <stippi@yellowbites.com>
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
/* VLC headers */
#include <SupportKit.h>

extern "C"
{
#include <videolan/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_playlist.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"

}

#include "intf_vlc_wrapper.h"


bool Intf_VLCWrapper::manage()
{
   p_main->p_intf->pf_manage( p_main->p_intf );
   
   if ( p_main->p_intf->b_die )
   {
       // exit the lot
       return( 1 );
   }
   
   if ( p_input_bank->pp_input[0] != NULL )
   {
       vlc_mutex_lock( &p_input_bank->pp_input[0]->stream.stream_lock );
        if( !p_input_bank->pp_input[0]->b_die )
        {
            /* New input or stream map change */
            if( p_input_bank->pp_input[0]->stream.b_changed ||
                p_main->p_intf->p_sys->i_part !=
                p_input_bank->pp_input[0]->stream.p_selected_area->i_part )
            {
                setupMenus();
                p_main->p_intf->p_sys->b_disabled_menus = 0;
            }
        }
        vlc_mutex_unlock( &p_input_bank->pp_input[0]->stream.stream_lock );
    }
    else if ( !p_main->p_intf->p_sys->b_disabled_menus )
    {
        setupMenus();
        p_main->p_intf->p_sys->b_disabled_menus = 1;
    }
    return( 0 );
}

void Intf_VLCWrapper::quit()
{
    p_main->p_intf->b_die = 1;
}
    
/* playlist control */
bool Intf_VLCWrapper::playlistPlay()
{
    if( p_input_bank->pp_input[0] != NULL )
    {
		input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_PLAY );

		playlistLock();
		p_main->p_playlist->b_stopped = 0;
		playlistUnlock();

		p_main->p_intf->p_sys->b_mute = 0;
    }
    else
    {
        playlistLock();

        if( p_main->p_playlist->b_stopped )
        {
            if( p_main->p_playlist->i_size )
            {
                playlistUnlock();
                intf_PlaylistJumpto( p_main->p_playlist,
                                     p_main->p_playlist->i_index - 1 );
				p_main->p_intf->p_sys->b_mute = 0;
            }
            else
                playlistUnlock();
        }
        else
            playlistUnlock();
    }

    return( true );
    
}

void Intf_VLCWrapper::playlistPause()
{
    if ( p_input_bank->pp_input[0] != NULL )
    {
        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_PAUSE );

        playlistLock();
        p_main->p_playlist->b_stopped = 0;
        playlistUnlock();
    }
}

void Intf_VLCWrapper::playlistStop()
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        /* end playing item */
        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_END );

        /* update playlist */
        playlistLock();
        p_main->p_playlist->b_stopped = 1;
        playlistUnlock();
    }

}

void Intf_VLCWrapper::playlistNext()
{
	playlistJumpTo( playlistCurrentPos() + 1 );
}

void Intf_VLCWrapper::playlistPrev()
{
	playlistJumpTo( playlistCurrentPos() - 1 );
}

void Intf_VLCWrapper::playlistJumpTo( int pos )
{
	// sanity checks
	if ( pos < 0 )
		pos = 0;
	int size = playlistSize();
	if (pos >= size)
		pos = size - 1;
	// weird hack
    if( p_input_bank->pp_input[0] != NULL )
		pos--;
	// stop current stream
	playlistStop();
	// modify current position in playlist
	playlistLock();
	p_main->p_playlist->i_index = pos;
	playlistUnlock();
	// start playing
	playlistPlay();
}

int Intf_VLCWrapper::playlistCurrentPos()
{
	playlistLock();
	int pos = p_main->p_playlist->i_index;
	playlistUnlock();
	return pos;
}

int Intf_VLCWrapper::playlistSize()
{
	playlistLock();
	int size = p_main->p_playlist->i_size;
	playlistUnlock();
	return size;
}

void Intf_VLCWrapper::playlistLock()
{
	vlc_mutex_lock( &p_main->p_playlist->change_lock );
}

void Intf_VLCWrapper::playlistUnlock()
{
	vlc_mutex_unlock( &p_main->p_playlist->change_lock );
}

void Intf_VLCWrapper::getNavCapabilities( bool* canSkipPrev,
										  bool* canSkipNext )
{
	if ( canSkipPrev && canSkipNext )
	{
		// init the parameters
		*canSkipPrev = false;
		*canSkipNext = false;
		// get playlist info
		playlistLock();
		int pos = p_main->p_playlist->i_index;
		int size = p_main->p_playlist->i_size;
		playlistUnlock();

		input_thread_s* input = p_input_bank->pp_input[0];
		// see if we have got a stream going		
		if ( input )
		{
			vlc_mutex_lock( &input->stream.stream_lock );

			bool hasTitles = input->stream.i_area_nb > 1;
			int numChapters = input->stream.p_selected_area->i_part_nb;
			bool hasChapters = numChapters > 1;
			// first, look for chapters
			if ( hasChapters )
			{
				*canSkipPrev = input->stream.p_selected_area->i_part > 1;
				*canSkipNext = input->stream.p_selected_area->i_part <
									 input->stream.p_selected_area->i_part_nb - 1;
			}
			// if one of the skip capabilities is false,
			// make it depend on titles instead
			if ( !*canSkipPrev && hasTitles )
				*canSkipPrev = input->stream.p_selected_area->i_id > 1;
			if ( !*canSkipNext && hasTitles )
				*canSkipNext = input->stream.p_selected_area->i_id < input->stream.i_area_nb - 1;

			vlc_mutex_unlock( &input->stream.stream_lock );
		}
		// last but not least, make capabilities depend on playlist
		if ( !*canSkipPrev )
			*canSkipPrev = pos > 0;
		if ( !*canSkipNext )
			*canSkipNext = pos < size - 1;
	}
}

void Intf_VLCWrapper::navigatePrev()
{
	bool hasSkiped = false;

	input_thread_s* input = p_input_bank->pp_input[0];
	// see if we have got a stream going		
	if ( input )
	{
		// get information from stream (lock it while looking at it)
		vlc_mutex_lock( &input->stream.stream_lock );

		int currentTitle = input->stream.p_selected_area->i_id;
		int currentChapter = input->stream.p_selected_area->i_part;
		int numTitles = input->stream.i_area_nb;
		bool hasTitles = numTitles > 1;
		int numChapters = input->stream.p_selected_area->i_part_nb;
		bool hasChapters = numChapters > 1;

		vlc_mutex_unlock( &input->stream.stream_lock );

		// first, look for chapters
		if ( hasChapters )
		{
			// skip to the previous chapter
			currentChapter--;

			if ( currentChapter >= 1 )
			{
				toggleChapter( currentChapter );
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
				toggleTitle(currentTitle);
				hasSkiped = true;
			}
		}

	}
	// last but not least, skip to previous file
	if ( !hasSkiped )
		playlistPrev();
}

void Intf_VLCWrapper::navigateNext()
{
	bool hasSkiped = false;

	input_thread_s* input = p_input_bank->pp_input[0];
	// see if we have got a stream going		
	if ( input )
	{
		// get information from stream (lock it while looking at it)
		vlc_mutex_lock( &input->stream.stream_lock );

		int currentTitle = input->stream.p_selected_area->i_id;
		int currentChapter = input->stream.p_selected_area->i_part;
		int numTitles = input->stream.i_area_nb;
		bool hasTitles = numTitles > 1;
		int numChapters = input->stream.p_selected_area->i_part_nb;
		bool hasChapters = numChapters > 1;

		vlc_mutex_unlock( &input->stream.stream_lock );

		// first, look for chapters
		if ( hasChapters )
		{
			// skip to the next chapter
			currentChapter++;
			if ( currentChapter < numChapters )
			{
				toggleChapter( currentChapter );
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
				toggleTitle(currentTitle);
				hasSkiped = true;
			}
		}

	}
	// last but not least, skip to next file
	if ( !hasSkiped )
		playlistNext();
}


//void Intf_VLCWrapper::channelNext()
//{
//    intf_thread_t * p_intf = p_main->p_intf;
//
//    p_intf->p_sys->i_channel++;
//
//    intf_WarnMsg( 3, "intf info: joining channel %d", p_intf->p_sys->i_channel );
//
//    vlc_mutex_lock( &p_intf->change_lock );
//
//    network_ChannelJoin( p_intf->p_sys->i_channel );
//    p_intf->pf_manage( p_intf );
//
//    vlc_mutex_unlock( &p_intf->change_lock );
//}
//
//void Intf_VLCWrapper::channelPrev()
//{
//    intf_thread_t * p_intf = p_main->p_intf;
//
//    if ( p_intf->p_sys->i_channel )
//    {
//        p_intf->p_sys->i_channel--;
//    }
//
//    intf_WarnMsg( 3, "intf info: joining channel %d", p_intf->p_sys->i_channel );
//
//    vlc_mutex_lock( &p_intf->change_lock );
//
//    network_ChannelJoin( p_intf->p_sys->i_channel );
//    p_intf->pf_manage( p_intf );
//
//    vlc_mutex_unlock( &p_intf->change_lock );
//
//}

void Intf_VLCWrapper::loop()
{
    intf_thread_t * p_intf = p_main->p_intf;

    if ( p_intf->p_sys->b_loop )
    {
        intf_PlaylistDelete( p_main->p_playlist,
                             p_main->p_playlist->i_size - 1 );
    }
    else
    {
        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, 
                          "vlc:loop" );
    }
    p_intf->p_sys->b_loop = !p_intf->p_sys->b_loop;
}


    /* playback control */
void Intf_VLCWrapper::playSlower()
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_SLOWER );

        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        p_main->p_playlist->b_stopped = 0;
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }
}

void Intf_VLCWrapper::playFaster()
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_FASTER );

        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        p_main->p_playlist->b_stopped = 0;
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }
}

void Intf_VLCWrapper::volume_mute()
{
    if( p_aout_bank->i_count > 0
    	&& p_aout_bank->pp_aout[0] != NULL )
    {
	    if( !p_main->p_intf->p_sys->b_mute )
		{
		    p_main->p_intf->p_sys->i_saved_volume = 
		                        p_aout_bank->pp_aout[0]->i_volume;
		    p_aout_bank->pp_aout[0]->i_volume = 0;
		    p_main->p_intf->p_sys->b_mute = 1;
		}
    }
}

void Intf_VLCWrapper::volume_restore()
{
    if( p_aout_bank->i_count > 0
    	&& p_aout_bank->pp_aout[0] != NULL )
    {
	    p_aout_bank->pp_aout[0]->i_volume = 
	                      p_main->p_intf->p_sys->i_saved_volume;
		p_main->p_intf->p_sys->i_saved_volume = 0;
	    p_main->p_intf->p_sys->b_mute = 0;
    }
}

void Intf_VLCWrapper::set_volume(int value)
{
    if( p_aout_bank->i_count > 0
    	&& p_aout_bank->pp_aout[0] != NULL )
    {
		// make sure value is within bounds
		if (value < 0)
			value = 0;
		if (value > VOLUME_MAX)
			value = VOLUME_MAX;
		vlc_mutex_lock( &p_aout_bank->lock );
			// unmute volume if muted
			if ( p_main->p_intf->p_sys->b_mute )
				p_main->p_intf->p_sys->b_mute = 0;
			// set every stream to the given value
			for ( int i = 0 ; i < p_aout_bank->i_count ; i++ )
			{
				if ( p_aout_bank->pp_aout[i] )
					p_aout_bank->pp_aout[i]->i_volume = value;
			}
		vlc_mutex_unlock( &p_aout_bank->lock );
    }
}

void Intf_VLCWrapper::toggle_mute()
{
    if( p_aout_bank->i_count > 0
    	&& p_aout_bank->pp_aout[0] != NULL )
   	{
	    if ( p_main->p_intf->p_sys->b_mute )
	    {
	        Intf_VLCWrapper::volume_restore();
	    }
	    else
	    {
	        Intf_VLCWrapper::volume_mute();
	    }
	}
}

bool Intf_VLCWrapper::is_muted()
{
	bool muted = true;
	if ( p_aout_bank->i_count > 0 )
	{
		vlc_mutex_lock( &p_aout_bank->lock );
			for ( int i = 0 ; i < p_aout_bank->i_count ; i++ )
			{
				if ( p_aout_bank->pp_aout[i]
					 && p_aout_bank->pp_aout[i]->i_volume > 0 )
				{
					muted = false;
					break;
				}
			}
		vlc_mutex_unlock( &p_aout_bank->lock );
// unfortunately, this is not reliable!
//		return p_main->p_intf->p_sys->b_mute;
	}
	return muted;
}

bool Intf_VLCWrapper::is_playing()
{
	bool playing = false;
	if ( p_input_bank->pp_input[0] )
	{
		switch ( p_input_bank->pp_input[0]->stream.control.i_status )
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

void Intf_VLCWrapper::maxvolume()
{
    if( p_aout_bank->i_count > 0
    	&& p_aout_bank->pp_aout[0] != NULL )
    {
	    if( p_main->p_intf->p_sys->b_mute )
	    {
	        p_main->p_intf->p_sys->i_saved_volume = VOLUME_MAX;
	    }
	    else
	    {
	        p_aout_bank->pp_aout[0]->i_volume = VOLUME_MAX;
	    }
    }
}

bool Intf_VLCWrapper::has_audio()
{
	return (p_aout_bank->i_count > 0);
}

//void Intf_VLCWrapper::fullscreen()
//{
//    if( p_vout_bank->pp_vout[0] != NULL )
//    {
//        p_vout_bank->pp_vout[0]->i_changes |= VOUT_FULLSCREEN_CHANGE;
//    }
//}

void Intf_VLCWrapper::eject(){}

    /* playback info */

const char*
Intf_VLCWrapper::getTimeAsString()
{
	static char psz_currenttime[ OFFSETTOTIME_MAX_SIZE ];
        
	if( p_input_bank->pp_input[0] == NULL )
		return ("-:--:--");

	input_OffsetToTime( p_input_bank->pp_input[0],
						psz_currenttime,
						p_input_bank->pp_input[0]->stream.p_selected_area->i_tell );

	return(psz_currenttime);
}

float  Intf_VLCWrapper::getTimeAsFloat()
{
    float f_time = 0.0;

    if( p_input_bank->pp_input[0] != NULL )
    {
        f_time = (float)p_input_bank->pp_input[0]->stream.p_selected_area->i_tell / 
                 (float)p_input_bank->pp_input[0]->stream.p_selected_area->i_size;
    }    

    return( f_time );
}

void   Intf_VLCWrapper::setTimeAsFloat(float f_position)
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        input_Seek( p_input_bank->pp_input[0], 
                    (long long int)(p_input_bank->pp_input[0]->stream.p_selected_area->i_size * f_position) );
    }
}

bool   Intf_VLCWrapper::playlistPlaying()
{ 
    return( !p_main->p_playlist->b_stopped );
}

BList  *Intf_VLCWrapper::playlistAsArray()
{ 
    int i;
    BList* p_list = new BList(p_main->p_playlist->i_size);
    
    vlc_mutex_lock( &p_main->p_playlist->change_lock );

    for( i = 0; i < p_main->p_playlist->i_size; i++ )
    {
        p_list->AddItem(new BString(p_main->p_playlist->p_item[i].psz_name));
    }

    vlc_mutex_unlock( &p_main->p_playlist->change_lock );
        
    return( p_list );
}

// getPlaylistInfo
void
Intf_VLCWrapper::getPlaylistInfo( int32& currentIndex, int32& maxIndex )
{
	currentIndex = -1;
	maxIndex = -1;
	if ( playlist_t* list = (playlist_t*)p_main->p_playlist )
	{
	    vlc_mutex_lock( &list->change_lock );

		maxIndex = list->i_size;
		if ( maxIndex > 0 )
			currentIndex = list->i_index + 1;
		else
			maxIndex = -1;

	    vlc_mutex_unlock( &list->change_lock );
	}
}

// getTitleInfo
void  
Intf_VLCWrapper::getTitleInfo( int32& currentIndex, int32& maxIndex )
{
	currentIndex = -1;
	maxIndex = -1;
	if ( input_thread_s* input = p_input_bank->pp_input[0] )
	{
		vlc_mutex_lock( &input->stream.stream_lock );

		maxIndex = input->stream.i_area_nb - 1;
		if ( maxIndex > 0)
			currentIndex = input->stream.p_selected_area->i_id;
		else
			maxIndex = -1;

		vlc_mutex_unlock( &input->stream.stream_lock );
	}
}

// getChapterInfo
void
Intf_VLCWrapper::getChapterInfo( int32& currentIndex, int32& maxIndex )
{
	currentIndex = -1;
	maxIndex = -1;
	if ( input_thread_s* input = p_input_bank->pp_input[0] )
	{
		vlc_mutex_lock( &input->stream.stream_lock );

		maxIndex = input->stream.p_selected_area->i_part_nb - 1;
		if ( maxIndex > 0)
			currentIndex = input->stream.p_selected_area->i_part;
		else
			maxIndex = -1;

		vlc_mutex_unlock( &input->stream.stream_lock );
	}
}


    /* open file/disc/network */
void Intf_VLCWrapper::openFiles( BList* o_files, bool replace )
{
	BString *o_file;
	int i_end = p_main->p_playlist->i_size;
	intf_thread_t * p_intf = p_main->p_intf;

	// make sure we remove the "loop" item from the end before mucking arround
    if ( p_intf->p_sys->b_loop )
    {
		intf_PlaylistDelete( p_main->p_playlist, i_end - 1 );
		i_end--;
    }

	// add the new files to the playlist
	while ( ( o_file = (BString *)o_files->LastItem() ) )
	{
		o_files->RemoveItem(o_files->CountItems() - 1);
		intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, 
						  o_file->String() );
		delete o_file;
    }

	if ( replace || i_end < 1 )
	{
		// end current item
		if ( p_input_bank->pp_input[0] != NULL )
			p_input_bank->pp_input[0]->b_eof = 1;
		// remove everything that was in playlist before
		for ( int i = 0; i < i_end; i++ )
			intf_PlaylistDelete( p_main->p_playlist, 0 );
		// jump to beginning and start playing
		intf_PlaylistJumpto( p_main->p_playlist, -1 );
	}

	// if we were looping, add special "loop" item back to list
	if ( p_intf->p_sys->b_loop )
		intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, "vlc:loop" );
}

void Intf_VLCWrapper::openDisc(BString o_type, BString o_device, int i_title, int i_chapter)
{
    BString o_source("");
    int i_end = p_main->p_playlist->i_size;
    intf_thread_t * p_intf = p_main->p_intf;

    o_source << o_type << ":" << o_device ;

    if ( p_intf->p_sys->b_loop )
    {
        intf_PlaylistDelete( p_main->p_playlist,
                             p_main->p_playlist->i_size - 1 );
    }

    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END,
                      o_source.String() );

    /* stop current item, select added item */
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );

    if ( p_intf->p_sys->b_loop )
    {
        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, 
                          "vlc:loop" );
    }
}

void Intf_VLCWrapper::openNet(BString o_addr, int i_port)
{
}

void Intf_VLCWrapper::openNetChannel(BString o_addr, int i_port)
{
}

void Intf_VLCWrapper::openNetHTTP(BString o_addr)
{
}


    /* menus management */
    void Intf_VLCWrapper::toggleProgram(int i_program){}

void Intf_VLCWrapper::toggleTitle(int i_title)
{
    input_thread_t * p_input = p_input_bank->pp_input[0];

    input_ChangeArea( p_input,
                      p_input->stream.pp_areas[i_title] );

    vlc_mutex_lock( &p_input->stream.stream_lock );
    setupMenus();

    vlc_mutex_unlock( &p_input->stream.stream_lock );
    input_SetStatus( p_input, INPUT_STATUS_PLAY );
}

void Intf_VLCWrapper::toggleChapter(int i_chapter)
{
    input_thread_t * p_input = p_input_bank->pp_input[0];
    p_input->stream.p_selected_area->i_part = i_chapter;
    input_ChangeArea( p_input,
                      p_input->stream.p_selected_area );

    vlc_mutex_lock( &p_input->stream.stream_lock );
    setupMenus();
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    input_SetStatus( p_input, INPUT_STATUS_PLAY );
}

void Intf_VLCWrapper::toggleLanguage( int i_language )
{
	if ( input_thread_t * p_input = p_input_bank->pp_input[0] )
	{
		if ( i_language == -1 )
		{
			input_ChangeES( p_input, NULL, AUDIO_ES );
		}
		else if ( i_language >= 0
				  && i_language < p_input->stream.i_es_number )
		{
			input_ChangeES( p_input, p_input->stream.pp_es[i_language], AUDIO_ES );
		}
	}
}

void Intf_VLCWrapper::toggleSubtitle(int i_subtitle)
{
	if ( input_thread_t * p_input = p_input_bank->pp_input[0] )
	{
		if ( i_subtitle == -1 )
		{
			input_ChangeES( p_input, NULL, SPU_ES );
		}
		else if ( i_subtitle >= 0
				  && i_subtitle < p_input->stream.i_es_number )
		{
			input_ChangeES( p_input, p_input->stream.pp_es[i_subtitle], SPU_ES );
		}
	}
}


    void Intf_VLCWrapper::setupMenus(){}
    
