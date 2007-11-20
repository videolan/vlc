/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
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

#include "qt4.hpp"
#include "util/input_slider.hpp"

InputSlider::InputSlider( QWidget *_parent ) : DirectSlider( _parent )
{
    InputSlider::InputSlider( Qt::Horizontal, _parent );
}

InputSlider::InputSlider( Qt::Orientation q,QWidget *_parent ) :
                                 DirectSlider( q, _parent )
{
    mymove = false;
    setMinimum( 0 );
    setMouseTracking(true);
    setMaximum( 1000 );
    setSingleStep( 2 );
    setPageStep( 10 );
    setTracking( true );
    CONNECT( this, valueChanged(int), this, userDrag( int ) );
}

void InputSlider::setPosition( float pos, int a, int b )
{
    if( pos == 0.0 )
        setEnabled( false );
    else
        setEnabled( true );
    mymove = true;
    setValue( (int)(pos * 1000.0 ) );
    mymove = false;
    inputLength = b;
}

void InputSlider::userDrag( int new_value )
{
    float f_pos = (float)(new_value)/1000.0;
    if( !mymove )
    {
        emit sliderDragged( f_pos );
    }
}

void InputSlider::mouseMoveEvent(QMouseEvent *event)
{
    char psz_length[MSTRTIME_MAX_SIZE];
    secstotimestr( psz_length,  (int)((float)event->x()/size().width()*inputLength) );
    setToolTip( psz_length );
}

