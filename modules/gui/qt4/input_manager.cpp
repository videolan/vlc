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
    QObject::connect( DialogsProvider::getInstance( p_intf )->fixed_timer,
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
    if( !p_input || p_input->b_die ) return;

    if( p_input->b_dead )
    {
        emit positionUpdated( 0.0, 0, 0 );
    }
    mtime_t i_length, i_time;
    float f_pos;
    i_length = var_GetTime( p_input, "length" ) / 1000000;
    i_time = var_GetTime( p_input, "time") / 1000000;
    f_pos = var_GetFloat( p_input, "position" );

    emit positionUpdated( f_pos, i_time, i_length );
}

void InputManager::sliderUpdate( float new_pos )
{
   if( p_input && !p_input->b_die && !p_input->b_dead )
        var_SetFloat( p_input, "position", new_pos );
}
