/*****************************************************************************
 * intf_vlc_wrapper.h: BeOS plugin for vlc (derived from MacOS X port )
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.cpp,v 1.1.2.1 2002/07/13 11:33:11 tcastley Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Tony Casltey <tony@castley.net>
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
        p_main->p_playlist->b_stopped = 0;
    }
    else
    {
        vlc_mutex_lock( &p_main->p_playlist->change_lock );

        if( p_main->p_playlist->b_stopped )
        {
            if( p_main->p_playlist->i_size )
            {
                vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                intf_PlaylistJumpto( p_main->p_playlist,
                                     p_main->p_playlist->i_index );
            }
            else
            {
                vlc_mutex_unlock( &p_main->p_playlist->change_lock );
            }
        }
        else
        {
            vlc_mutex_unlock( &p_main->p_playlist->change_lock );
        }
    }

    return( true );
    
}

void Intf_VLCWrapper::playlistPause()
{
    if ( p_input_bank->pp_input[0] != NULL )
    {
        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_PAUSE );

        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        p_main->p_playlist->b_stopped = 0;
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }
}

void Intf_VLCWrapper::playlistStop()
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        /* end playing item */
        p_input_bank->pp_input[0]->b_eof = 1;

        /* update playlist */
        vlc_mutex_lock( &p_main->p_playlist->change_lock );

        p_main->p_playlist->i_index--;
        p_main->p_playlist->b_stopped = 1;

        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }

}

void Intf_VLCWrapper::playlistNext()
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }
}

void Intf_VLCWrapper::playlistPrev()
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        /* FIXME: temporary hack */
        intf_PlaylistPrev( p_main->p_playlist );
        intf_PlaylistPrev( p_main->p_playlist );
        p_input_bank->pp_input[0]->b_eof = 1;
    }
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
    if( p_aout_bank->pp_aout[0] == NULL ) return;
    if( p_main->p_intf->p_sys->b_mute ) return;
    p_main->p_intf->p_sys->i_saved_volume = 
                        p_aout_bank->pp_aout[0]->i_volume;
    p_aout_bank->pp_aout[0]->i_volume = 0;
    p_main->p_intf->p_sys->b_mute = 1;
}

void Intf_VLCWrapper::volume_restore()
{
    if( p_aout_bank->pp_aout[0] == NULL ) return;

    p_aout_bank->pp_aout[0]->i_volume = 
                      p_main->p_intf->p_sys->i_saved_volume;
    p_main->p_intf->p_sys->b_mute = 0;
}

void Intf_VLCWrapper::toggle_mute()
{
    if( p_aout_bank->pp_aout[0] == NULL ) return;
    if ( p_main->p_intf->p_sys->b_mute )
    {
        Intf_VLCWrapper::volume_restore();
    }
    else
    {
        Intf_VLCWrapper::volume_mute();
    }
    p_main->p_intf->p_sys->b_mute = !p_main->p_intf->p_sys->b_mute;
}

void Intf_VLCWrapper::maxvolume()
{
    if( p_aout_bank->pp_aout[0] == NULL ) return;

    if( p_main->p_intf->p_sys->b_mute )
    {
        p_main->p_intf->p_sys->i_saved_volume = VOLUME_MAX;
    }
    else
    {
        p_aout_bank->pp_aout[0]->i_volume = VOLUME_MAX;
    }
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
        
    if( p_input_bank->pp_input[0] == NULL )
    {
        return (new BString("00:00:00"));
    }     
   
    input_OffsetToTime( p_input_bank->pp_input[0], 
                        psz_currenttime, 
                        p_input_bank->pp_input[0]->stream.p_selected_area->i_tell );        

    return(new BString(psz_currenttime));
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

    /* open file/disc/network */
void Intf_VLCWrapper::openFiles(BList *o_files)
{
    BString *o_file;
    int i_end = p_main->p_playlist->i_size;
    intf_thread_t * p_intf = p_main->p_intf;

    if ( p_intf->p_sys->b_loop )
    {
        intf_PlaylistDelete( p_main->p_playlist,
                             p_main->p_playlist->i_size - 1 );
    }

    while( ( o_file = (BString *)o_files->LastItem() ) )
    {
        o_files->RemoveItem(o_files->CountItems() - 1);
        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, 
                          o_file->String() );
        delete o_file;
    }

    /* end current item, select first added item */
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

void Intf_VLCWrapper::openDisc(BString o_type, BString o_device, int i_title, int i_chapter)
{
    BString o_source("");
    int i_end = p_main->p_playlist->i_size;
    intf_thread_t * p_intf = p_main->p_intf;

    o_source << o_type << ":" << o_device ;
    //i_title, i_chapter;

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

    void Intf_VLCWrapper::toggleLanguage(int i_language){}
    void Intf_VLCWrapper::toggleSubtitle(int i_subtitle){}
    void Intf_VLCWrapper::setupMenus(){}
    
