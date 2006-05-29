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

#include "util/input_slider.hpp"

void InputSlider::init()
{
    setMinimum( 0 );
    setMaximum( 1000 );
    setSingleStep( 2 );
    setPageStep( 100 );
    setTracking( true );
    QObject::connect( this, SIGNAL( valueChanged(int) ), this,
    		      SLOT( userDrag( int ) ) );
}

void InputSlider::setPosition( float pos, int a, int b )
{
    setValue( (int)(pos * 1000.0 ) );
}

void InputSlider::userDrag( int new_value )
{
    float f_pos = (float)(new_value)/1000.0;
    emit positionUpdated( f_pos );
}
