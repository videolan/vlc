/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <assert.h>

#include "input_manager.hpp"
#include "dialogs_provider.hpp"
#include "qt4.hpp"

/**********************************************************************
 * InputManager implementation
 **********************************************************************/

InputManager::InputManager( QObject *parent, intf_thread_t *_p_intf) :
                           QObject( parent ), p_intf( _p_intf )
{
    p_input = NULL;
    /* Subscribe to updates */
    connect( DialogsProvider::getInstance( p_intf )->fixed_timer,
             SIGNAL( timeout() ), this, SLOT( update() ) );
}

InputManager::~InputManager()
{
}

void InputManager::setInput( input_thread_t *_p_input )
{
    p_input = _p_input;
    emit positionUpdated( 0.0,0,0 );
}

void InputManager::update()
{
    /// \todo Emit the signals only if it changed
    if( !p_input || p_input->b_die ) return;

    if( p_input->b_dead )
    {
        emit positionUpdated( 0.0, 0, 0 );
        emit navigationChanged( 0 );
        emit statusChanged( 0 ); // 0 = STOPPED, 1 = PAUSE, 2 = PLAY
    }

    /* Update position */
    mtime_t i_length, i_time;
    float f_pos;
    i_length = var_GetTime( p_input, "length" ) / 1000000;
    i_time = var_GetTime( p_input, "time") / 1000000;
    f_pos = var_GetFloat( p_input, "position" );
    emit positionUpdated( f_pos, i_time, i_length );

    /* Update disc status */
    vlc_value_t val;
    var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int > 0 )
    {
        vlc_value_t val;
        var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int > 0 )
        emit navigationChanged( 1 ); // 1 = chapter, 2 = title, 3 = NO
    else
        emit navigationChanged( 2 );
    }
    else
    {
        emit navigationChanged( 0 );
    }

    /* Update text */
    QString text;
    if( p_input->input.p_item->p_meta &&
        p_input->input.p_item->p_meta->psz_nowplaying &&
        *p_input->input.p_item->p_meta->psz_nowplaying )
    {
        text.sprintf( "%s - %s",
                  p_input->input.p_item->p_meta->psz_nowplaying,
                  p_input->input.p_item->psz_name );
    }
    else
    {
        text.sprintf( "%s", p_input->input.p_item->psz_name );
    }
    emit nameChanged( text );

}

void InputManager::sliderUpdate( float new_pos )
{
   if( p_input && !p_input->b_die && !p_input->b_dead )
        var_SetFloat( p_input, "position", new_pos );
}

/**********************************************************************
 * MainInputManager implementation. Wrap an input manager and
 * take care of updating the main playlist input
 **********************************************************************/
MainInputManager * MainInputManager::instance = NULL;

MainInputManager::MainInputManager( intf_thread_t *_p_intf ) : QObject(NULL),
                                                p_intf( _p_intf )
{
    p_input = NULL;
    im = new InputManager( this, p_intf );
    /* Get timer updates */
    connect( DialogsProvider::getInstance(p_intf)->fixed_timer,
             SIGNAL(timeout() ), this, SLOT( updateInput() ) );
    /* Warn our embedded IM about input changes */
    connect( this, SIGNAL( inputChanged( input_thread_t * ) ),
             im, SLOT( setInput( input_thread_t * ) ) );
}

void MainInputManager::updateInput()
{
    vlc_mutex_lock( &p_intf->change_lock );
    if( p_input && p_input->b_dead )
    {
        vlc_object_release( p_input );
        p_input = NULL;
        emit inputChanged( NULL );
    }

    if( !p_input )
    {
        playlist_t *p_playlist = (playlist_t *) vlc_object_find( p_intf,
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        assert( p_playlist );
        PL_LOCK;
        p_input = p_playlist->p_input;
        if( p_input )
        {
            vlc_object_yield( p_input );
            emit inputChanged( p_input );
        }
        PL_UNLOCK;
        vlc_object_release( p_playlist );
    }
    vlc_mutex_unlock( &p_intf->change_lock );
}
