/*****************************************************************************
 * intf_vlc_wrapper.h: BeOS plugin for vlc (derived from MacOS X port )
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.cpp,v 1.2 2002/07/23 12:42:17 tcastley Exp $
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

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "intf_vlc_wrapper.h"

Intf_VLCWrapper *Intf_VLCWrapper::getVLCWrapper(intf_thread_t *p_if)
{
    static Intf_VLCWrapper *one_wrapper;
    if (one_wrapper == NULL )
    {
       one_wrapper = new Intf_VLCWrapper(p_if);
    }
    return one_wrapper;
}

Intf_VLCWrapper::Intf_VLCWrapper(intf_thread_t *p_if)
{
    p_intf = p_if;
}

Intf_VLCWrapper::~Intf_VLCWrapper()
{
}

//bool Intf_VLCWrapper::manage()
//{
//
//   p_intf->pf_manage( p_intf );
//   
//   if ( p_intf->b_die )
//   {
//       // exit the lot
//       return( 1 );
//   }
    /* Update the input */
//    if( p_intf->p_sys->p_input != NULL )
//    {
//        if( p_intf->p_sys->p_input->b_dead )
//        {
//            vlc_object_release( p_intf->p_sys->p_input );
//            p_intf->p_sys->p_input = NULL;
//        }
//    }
//   
//    p_intf->p_sys->p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
//                                                      FIND_ANYWHERE );

//   if ( p_intf->p_sys->p_input != NULL )
//   {
//       vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );
//        if( !p_intf->p_sys->p_input->b_die )
//        {
//            /* New input or stream map change */
//            if( p_intf->p_sys->p_input->stream.b_changed ||
//                p_intf->p_sys->i_part !=
//                p_intf->p_sys->p_input->stream.p_selected_area->i_part )
//            {
//                setupMenus();
//                p_intf->p_sys->b_disabled_menus = 0;
//            }
//        }
//        vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
//    }
//    else if ( !p_intf->p_sys->b_disabled_menus )
//    {
//        setupMenus();
//        p_intf->p_sys->b_disabled_menus = 1;
//    }
//    return( 0 );
//}

void Intf_VLCWrapper::quit()
{
    p_intf->b_die = 1;
}
    
/* playlist control */

int Intf_VLCWrapper::inputGetStatus()
{
    if( p_intf->p_sys->p_input != NULL )
    {
        return( p_intf->p_sys->p_input->stream.control.i_status );
    }
    else
    {
        return( UNDEF_S );
    }
}

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
    volumeMute( true );
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

    /* playback control */
void Intf_VLCWrapper::playSlower()
{
    if( p_intf->p_sys->p_input != NULL )
    {
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
    }
    if (p_intf->p_sys->p_input->stream.control.i_rate == DEFAULT_RATE)
    {
        volumeMute( false );
    }
    else
    {
        volumeMute (true );
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
        volumeMute( false );
    }
    else
    {
        volumeMute (true );
    }
}

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

    if( i_language != -1 )
    {
        input_ToggleES( p_intf->p_sys->p_input, 
                        p_intf->p_sys->p_input->stream.pp_selected_es[i_language],
                        VLC_TRUE );
    }

    if( i_old != -1 )
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

    if( i_subtitle != -1 )
    {
        input_ToggleES( p_intf->p_sys->p_input, 
                        p_intf->p_sys->p_input->stream.pp_selected_es[i_subtitle],
                        VLC_TRUE );
    }

    if( i_old != -1 )
    {
        input_ToggleES( p_intf->p_sys->p_input, 
                        p_intf->p_sys->p_input->stream.pp_selected_es[i_old],
                        VLC_FALSE );
    }
}


void Intf_VLCWrapper::channelNext()
{
}

void Intf_VLCWrapper::channelPrev()
{
}

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
                   (long long int)(p_intf->p_sys->p_input->stream.p_selected_area->i_size * f_position / 100), 
                   INPUT_SEEK_SET);
    }
}

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
void Intf_VLCWrapper::openFiles(BList *o_files)
{
    BString *o_file;
    playlist_t *p_playlist = 
               (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                      FIND_ANYWHERE );

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

void Intf_VLCWrapper::volumeMute( bool mute )
{
    if ( mute )
    {
    }
    else
    {
    }
}

/* menus management */
    
