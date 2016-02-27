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
    input_title_t *p_title = NULL;
    input_thread_t *p_input_thread = playlist_CurrentInput( THEPL );
    int i_title_id = -1;
    if( !p_input_thread ) { pointsList.clear(); return; }

    if ( input_Control( p_input_thread, INPUT_GET_TITLE_INFO, &p_title, &i_title_id )
        != VLC_SUCCESS )
    {
        vlc_object_release( p_input_thread );
        pointsList.clear();
        return;
    }

    vlc_object_release( p_input_thread );

    if( !p_title )
        return;

    /* lock here too, as update event is triggered by an external thread */
    if ( !access() ) return;
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
    vlc_input_title_Delete( p_title );
    release();
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
