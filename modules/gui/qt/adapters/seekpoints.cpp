/*****************************************************************************
 * seekpoints.cpp : Chapters & Bookmarks (menu)
 *****************************************************************************
 * Copyright © 2011 the VideoLAN team
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


#include "recents.hpp"
#include "dialogs_provider.hpp"
#include "menus.hpp"

#include "seekpoints.hpp"

#include "qt.hpp"
#include "input_manager.hpp"

SeekPoints::SeekPoints( QObject *parent, intf_thread_t *p_intf_ ) :
    QObject( parent ), p_intf( p_intf_ )
{}

void SeekPoints::update()
{
    input_thread_t *p_input_thread = playlist_CurrentInput( THEPL );
    if( !p_input_thread ) { pointsList.clear(); return; }

    input_title_t **pp_title = NULL, *p_title = NULL;
    int i_title_count = 0;
    int i_title_id = var_GetInteger( p_input_thread, "title" );
    if ( input_Control( p_input_thread, INPUT_GET_FULL_TITLE_INFO, &pp_title,
                        &i_title_count ) != VLC_SUCCESS )
    {
        vlc_object_release( p_input_thread );
        pointsList.clear();
        return;
    }

    vlc_object_release( p_input_thread );

    if( i_title_id < i_title_count )
        p_title = pp_title[i_title_id];

    /* lock here too, as update event is triggered by an external thread */
    if( p_title && access() )
    {
        pointsList.clear();
        if ( p_title->i_seekpoint > 0 )
        {
            /* first check the last point to see if we have filled time offsets (> 0) */
            if ( p_title->seekpoint[p_title->i_seekpoint - 1]->i_time_offset > 0 )
            {
                for ( int i=0; i<p_title->i_seekpoint ; i++ )
                    pointsList << SeekPoint( p_title->seekpoint[i] );
            }
        }
        release();
    }

    for( int i = 0; i < i_title_count; i++ )
        vlc_input_title_Delete( pp_title[i] );
    free( pp_title ) ;
}

QList<SeekPoint> const SeekPoints::getPoints()
{
    QList<SeekPoint> copy;
    if ( access() )
    {
        copy = pointsList;
        release();
    }
    return copy;
}

bool SeekPoints::jumpTo( int i_chapterindex )
{
    vlc_value_t val;
    val.i_int = i_chapterindex;
    input_thread_t *p_input_thread = playlist_CurrentInput( THEPL );
    if( !p_input_thread ) return false;
    bool b_succ = var_Set( p_input_thread, "chapter", val );
    vlc_object_release( p_input_thread );
    return ( b_succ == VLC_SUCCESS );
}
