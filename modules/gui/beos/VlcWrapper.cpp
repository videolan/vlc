/*****************************************************************************
 * intf_vlc_wrapper.h: BeOS plugin for vlc (derived from MacOS X port )
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: VlcWrapper.cpp,v 1.6 2002/10/14 20:09:17 titer Exp $
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

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <audio_output.h>
#include <aout_internal.h>

#include "VlcWrapper.h"

Intf_VLCWrapper *Intf_VLCWrapper::getVLCWrapper(intf_thread_t *p_interface)
{
    static Intf_VLCWrapper *one_wrapper;
    if (one_wrapper == NULL )
    {
       one_wrapper = new Intf_VLCWrapper(p_interface);
    }
    return one_wrapper;
}

Intf_VLCWrapper::Intf_VLCWrapper(intf_thread_t *p_interface)
{
    p_intf = p_interface;
}

Intf_VLCWrapper::~Intf_VLCWrapper()
{
}

#if 0
bool Intf_VLCWrapper::manage()
{
   /* p_main->p_intf->pf_manage( p_intf ); */
   
   if ( p_intf->b_die )
   {
       // exit the lot
       return( 1 );
   }
   
   if ( p_intf->p_sys->p_input != NULL )
   {
       vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        if( !p_intf->p_sys->p_input->b_die )
        {
            /* New input or stream map change */
            if( p_intf->p_sys->p_input->stream.b_changed ||
                p_intf->p_sys->i_part !=
                    p_intf->p_sys->p_input->stream.p_selected_area->i_part )
            {
                setupMenus();
                p_intf->p_sys->b_disabled_menus = 0;
            }
        }
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
    else if ( !p_intf->p_sys->b_disabled_menus )
    {
        setupMenus();
        p_intf->p_sys->b_disabled_menus = 1;
    }
    return( 0 );
}
#endif

void Intf_VLCWrapper::quit()
{
    p_intf->b_die = 1;
}
    
/* playlist control */
bool Intf_VLCWrapper::playlistPlay()
{
    playlist_t *p_playlist = 
                (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Play( p_playlist );
        vlc_object_release( p_playlist );
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
    }

    return( true );
}

void Intf_VLCWrapper::playlistPause()
{
    toggle_mute(  );
    playlist_t *p_playlist = 
                (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    playlist_Pause( p_playlist );
    vlc_object_release( p_playlist );
}

void Intf_VLCWrapper::playlistStop()
{
    playlist_t *p_playlist = 
                (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );
}

void Intf_VLCWrapper::playlistNext()
{
    playlist_t *p_playlist = 
                (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );
}

void Intf_VLCWrapper::playlistPrev()
{
    playlist_t *p_playlist = 
                (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );
}

void Intf_VLCWrapper::playlistSkip(int i)
{
    playlist_t *p_playlist = 
                (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    playlist_Skip( p_playlist, i );
    vlc_object_release( p_playlist );
}

void Intf_VLCWrapper::playlistGoto(int i)
{
    playlist_t *p_playlist = 
                (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    playlist_Goto( p_playlist, i );
    vlc_object_release( p_playlist );
}

void Intf_VLCWrapper::playlistJumpTo( int pos )
{
#if 0
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
#endif
}

int Intf_VLCWrapper::playlistCurrentPos()
{
	playlistLock();
	int pos = p_intf->p_sys->p_playlist->i_index;
	playlistUnlock();
	return pos;
}

int Intf_VLCWrapper::playlistSize()
{
	playlistLock();
	int size = p_intf->p_sys->p_playlist->i_size;
	playlistUnlock();
	return size;
}

void Intf_VLCWrapper::playlistLock()
{
	vlc_mutex_lock( &p_intf->p_sys->p_playlist->object_lock );
}

void Intf_VLCWrapper::playlistUnlock()
{
	vlc_mutex_unlock( &p_intf->p_sys->p_playlist->object_lock );
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
		int pos = p_intf->p_sys->p_playlist->i_index;
		int size = p_intf->p_sys->p_playlist->i_size;
		playlistUnlock();

		/* input_thread_t* input = p_input_bank->pp_input[0]; */
		input_thread_t* input = p_intf->p_sys->p_input;
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
				*canSkipPrev = input->stream.p_selected_area->i_part > 0;
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
#if 0
	bool hasSkiped = false;

	input_thread_t* input = p_input_bank->pp_input[0];
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

			if ( currentChapter >= 0 )
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
#endif
}

void Intf_VLCWrapper::navigateNext()
{
#if 0
	bool hasSkiped = false;

	input_thread_t* input = p_input_bank->pp_input[0];
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
#endif
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
    if ( p_intf->p_sys->b_loop )
    {
        playlist_Delete( p_intf->p_sys->p_playlist,
                         p_intf->p_sys->p_playlist->i_size - 1 );
    }
    else
    {
        playlist_Add( p_intf->p_sys->p_playlist, "vlc:loop",
                      PLAYLIST_APPEND | PLAYLIST_GO,
                      PLAYLIST_END );
    }
    p_intf->p_sys->b_loop = !p_intf->p_sys->b_loop;
}


    /* playback control */
void Intf_VLCWrapper::playSlower()
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
    }
    if (p_intf->p_sys->p_input->stream.control.i_rate == DEFAULT_RATE)
    {
        toggle_mute(  );
    }
    else
    {
        toggle_mute ( );
    }
}

void Intf_VLCWrapper::playFaster()
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_FASTER );
    }
    if (p_intf->p_sys->p_input->stream.control.i_rate == DEFAULT_RATE)
    {
        toggle_mute(  );
    }
    else
    {
        toggle_mute ( );
    }
}

void Intf_VLCWrapper::volume_mute()
{
    p_intf->p_sys->p_aout =
        (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                           FIND_ANYWHERE );
    if( p_intf->p_sys->p_aout != NULL )
    {
	    if( !p_intf->p_sys->b_mute )
		{
		    p_intf->p_sys->i_saved_volume = p_intf->p_sys->p_aout->output.i_volume;
		    p_intf->p_sys->p_aout->output.i_volume = 0;
		    p_intf->p_sys->b_mute = 1;
		}
    }

}

void Intf_VLCWrapper::volume_restore()
{
    p_intf->p_sys->p_aout =
        (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                            FIND_ANYWHERE );
    if( p_intf->p_sys->p_aout != NULL )
    {
	    p_intf->p_sys->p_aout->output.i_volume = p_intf->p_sys->i_saved_volume;
		p_intf->p_sys->i_saved_volume = 0;
	    p_intf->p_sys->b_mute = 0;
    }

}

void Intf_VLCWrapper::set_volume(int value)
{
    p_intf->p_sys->p_aout =
        (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                            FIND_ANYWHERE );
    if( p_intf->p_sys->p_aout != NULL )
    {
		// make sure value is within bounds
		if (value < 0)
			value = 0;
		if (value > AOUT_VOLUME_MAX)
			value = AOUT_VOLUME_MAX;
		vlc_mutex_lock( &p_intf->p_sys->p_aout->mixer_lock );
		// unmute volume if muted
		if ( p_intf->p_sys->b_mute )
		{
			p_intf->p_sys->b_mute = 0;
            p_intf->p_sys->p_aout->output.i_volume = value;
		}
		vlc_mutex_unlock( &p_intf->p_sys->p_aout->mixer_lock );
    }
}

void Intf_VLCWrapper::toggle_mute()
{
    p_intf->p_sys->p_aout =
        (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                            FIND_ANYWHERE );
    if( p_intf->p_sys->p_aout != NULL )
   	{
	    if ( p_intf->p_sys->b_mute )
	    {
	        volume_restore();
	    }
	    else
	    {
	        volume_mute();
	    }
	}
}

bool Intf_VLCWrapper::is_muted()
{
	bool muted = true;
	
    p_intf->p_sys->p_aout =
        (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                            FIND_ANYWHERE );
    if( p_intf->p_sys->p_aout != NULL )
	{
		vlc_mutex_lock( &p_intf->p_sys->p_aout->mixer_lock );
		if( p_intf->p_sys->p_aout->output.i_volume > 0 )
		{
			muted = false;
		}
		vlc_mutex_unlock( &p_intf->p_sys->p_aout->mixer_lock );
// unfortunately, this is not reliable!
//		return p_main->p_intf->p_sys->b_mute;
	}
	return muted;
}

bool Intf_VLCWrapper::is_playing()
{

	bool playing = false;
	if ( p_intf->p_sys->p_input )
	{
		switch ( p_intf->p_sys->p_input->stream.control.i_status )
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
    p_intf->p_sys->p_aout =
        (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                            FIND_ANYWHERE );
    if( p_intf->p_sys->p_aout != NULL )
    {
	    if( p_intf->p_sys->b_mute )
	    {
	        p_intf->p_sys->i_saved_volume = AOUT_VOLUME_MAX;
	    }
	    else
	    {
	        p_intf->p_sys->p_aout->output.i_volume = AOUT_VOLUME_MAX;
	    }
    }
}

bool Intf_VLCWrapper::has_audio()
{
    p_intf->p_sys->p_aout =
        (aout_instance_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                            FIND_ANYWHERE );
    return( p_intf->p_sys->p_aout != NULL );
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

BString*  Intf_VLCWrapper::getTimeAsString()
{
    static char psz_currenttime[ OFFSETTOTIME_MAX_SIZE ];
        
    if( p_intf->p_sys->p_input == NULL )
    {
        return (new BString("00:00:00"));
    }     
   
    input_OffsetToTime( p_intf->p_sys->p_input, 
                        psz_currenttime, 
                        p_intf->p_sys->p_input->stream.p_selected_area->i_tell );        

    return(new BString(psz_currenttime));
}

float  Intf_VLCWrapper::getTimeAsFloat()
{
    float f_time = 0.0;

    if( p_intf->p_sys->p_input != NULL )
    {
        f_time = (float)p_intf->p_sys->p_input->stream.p_selected_area->i_tell / 
                 (float)p_intf->p_sys->p_input->stream.p_selected_area->i_size;
    }    
    else
    {
        f_time = 0.0;
    }
    return( f_time );
}

void   Intf_VLCWrapper::setTimeAsFloat(float f_position)
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_Seek( p_intf->p_sys->p_input, 
                   (long long int)(p_intf->p_sys->p_input->stream.p_selected_area->i_size
                       * f_position / SEEKSLIDER_RANGE ), 
                   INPUT_SEEK_SET);
    }
}

/* bool   Intf_VLCWrapper::playlistPlaying()
{ 
    return( !p_intf->p_sys->p_playlist->b_stopped );
} */

BList  *Intf_VLCWrapper::playlistAsArray()
{ 
    int i;
    playlist_t *p_playlist = 
                (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    BList* p_list = new BList(p_playlist->i_size);
    
    vlc_mutex_lock( &p_playlist->object_lock );

    for( i = 0; i < p_playlist->i_size; i++ )
    {
        p_list->AddItem(new BString(p_playlist->pp_items[i]->psz_name));
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );
    return( p_list );
}

    /* open file/disc/network */
void Intf_VLCWrapper::openFiles( BList* o_files, bool replace )
{
    BString *o_file;
    playlist_t *p_playlist = p_intf->p_sys->p_playlist;

    while( ( o_file = (BString *)o_files->LastItem() ) )
    {
        o_files->RemoveItem(o_files->CountItems() - 1);
        playlist_Add( p_playlist, o_file->String(),
                  PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
        delete o_file;
    }

    vlc_object_release( p_playlist );
}

void Intf_VLCWrapper::openDisc(BString o_type, BString o_device, int i_title, int i_chapter)
{
    BString o_source("");
    o_source << o_type << ":" << o_device ;

    playlist_t *p_playlist = 
               (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                      FIND_ANYWHERE );
    playlist_Add( p_playlist, o_source.String(),
                  PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
    vlc_object_release( p_playlist );
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
    if( p_intf->p_sys->p_input != NULL )
    {
        input_ChangeArea( p_intf->p_sys->p_input,
                          p_intf->p_sys->p_input->stream.pp_areas[i_title] );

        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
        //setupMenus();

        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}

void Intf_VLCWrapper::toggleChapter(int i_chapter)
{
    if( p_intf->p_sys->p_input != NULL )
    {
        p_intf->p_sys->p_input->stream.p_selected_area->i_part = i_chapter;
        input_ChangeArea( p_intf->p_sys->p_input,
                          p_intf->p_sys->p_input->stream.p_selected_area );

        vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
//        setupMenus();
        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    }
}

void Intf_VLCWrapper::toggleLanguage(int i_language)
{

    int32 i_old = -1;
    int i_cat = AUDIO_ES;

    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
    for( int i = 0; i < p_intf->p_sys->p_input->stream.i_selected_es_number ; i++ )
    {
        if( p_intf->p_sys->p_input->stream.pp_selected_es[i]->i_cat == i_cat )
        {
            i_old = i;
            break;
        }
    }
    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );

    msg_Info( p_intf, "Old: %d,  New: %d", i_old, i_language);
    if( i_language != -1 )
    {
        input_ToggleES( p_intf->p_sys->p_input, 
                        p_intf->p_sys->p_input->stream.pp_selected_es[i_language],
                        VLC_TRUE );
    }

    if( (i_old != -1) && (i_old != i_language) )
    {
        input_ToggleES( p_intf->p_sys->p_input, 
                        p_intf->p_sys->p_input->stream.pp_selected_es[i_old],
                        VLC_FALSE );
    }
}

void Intf_VLCWrapper::toggleSubtitle(int i_subtitle)
{
    int32 i_old = -1;
    int i_cat = SPU_ES;

    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
    for( int i = 0; i < p_intf->p_sys->p_input->stream.i_selected_es_number ; i++ )
    {
        if( p_intf->p_sys->p_input->stream.pp_selected_es[i]->i_cat == i_cat )
        {
            i_old = i;
            break;
        }
    }
    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
    
    msg_Info( p_intf, "Old: %d,  New: %d", i_old, i_subtitle);
    if( i_subtitle != -1 )
    {
        input_ToggleES( p_intf->p_sys->p_input, 
                        p_intf->p_sys->p_input->stream.pp_selected_es[i_subtitle],
                        VLC_TRUE );
    }

    if( (i_old != -1) && (i_old != i_subtitle) )
    {
        input_ToggleES( p_intf->p_sys->p_input, 
                        p_intf->p_sys->p_input->stream.pp_selected_es[i_old],
                        VLC_FALSE );
    }
}


void Intf_VLCWrapper::setupMenus(){}
int  Intf_VLCWrapper::inputGetStatus() {}
