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

#include "input_manager.hpp"
#include "dialogs_provider.hpp"
#include "qt4.hpp"

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
