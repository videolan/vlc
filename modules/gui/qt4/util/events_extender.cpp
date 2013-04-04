/*****************************************************************************
 * events_extender.cpp: Events Extenders / Modifiers
 ****************************************************************************
 * Copyright (C) 2013 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "events_extender.hpp"

#include <QTimer>
#include <QApplication>
#include <QMouseEvent>

const QEvent::Type MouseEventExtenderFilter::MouseButtonLongPress =
        (QEvent::Type)QEvent::registerEventType();
const QEvent::Type MouseEventExtenderFilter::MouseButtonShortClick =
        (QEvent::Type)QEvent::registerEventType();
const QEvent::Type MouseEventExtenderFilter::MouseButtonLongClick =
        (QEvent::Type)QEvent::registerEventType();

MouseEventExtenderFilter::ButtonState::ButtonState( QObject *parent, Qt::MouseButton b )
{
    state = RELEASED;
    button = b;
    timer = new QTimer( parent );
    timer->setInterval( 2 * QApplication::doubleClickInterval() );
    timer->setSingleShot( true );
    timer->installEventFilter( parent );
}

MouseEventExtenderFilter::ButtonState::~ButtonState()
{
    delete timer;
}

//! MouseEventExtenderFilter constructor
/*!
\param The QObject we're extending events.
*/
MouseEventExtenderFilter::MouseEventExtenderFilter( QObject *parent ) : QObject( parent )
{}

MouseEventExtenderFilter::~MouseEventExtenderFilter()
{
    qDeleteAll( buttonsStates.begin(), buttonsStates.end() );
}

MouseEventExtenderFilter::ButtonState * MouseEventExtenderFilter::getState( Qt::MouseButton button )
{
    foreach( ButtonState *bs, buttonsStates )
        if ( bs->button == button )
            return bs;

    ButtonState *bs = new ButtonState( this, button );
    buttonsStates << bs;
    return bs;
}

MouseEventExtenderFilter::ButtonState * MouseEventExtenderFilter::getStateByTimer( int id )
{
    foreach( ButtonState *bs, buttonsStates )
        if ( bs->timer->timerId() == id )
            return bs;
    return NULL;
}

void MouseEventExtenderFilter::postEvent( QObject *obj, QEvent::Type type, ButtonState *bs ) const
{
    QApplication::postEvent( obj,
        new QMouseEvent( type, bs->localPos, bs->button, bs->buttons, bs->modifiers ) );
}

bool MouseEventExtenderFilter::eventFilter( QObject *obj, QEvent *e )
{
    ButtonState *bs;
    QMouseEvent *mouseevent;
    QTimerEvent *timerevent;

    switch ( e->type() )
    {
        case QEvent::Timer:
            timerevent = static_cast<QTimerEvent *>(e);
            bs = getStateByTimer( timerevent->timerId() );
            if ( bs )
            {
                killTimer( timerevent->timerId() );
                bs->state = ButtonState::LONGPRESSED;
                postEvent( obj, MouseButtonLongPress, bs );
            }
            break;

        case QEvent::MouseButtonPress:
            mouseevent = static_cast<QMouseEvent *>(e);
            bs = getState( mouseevent->button() );
            bs->state = ButtonState::PRESSED;
            bs->localPos = mouseevent->pos();
            bs->buttons = mouseevent->buttons();
            bs->modifiers = mouseevent->modifiers();
            bs->timer->start();
            break;

        case QEvent::MouseButtonRelease:
            mouseevent = static_cast<QMouseEvent *>(e);
            bs = getState( mouseevent->button() );
            if ( bs->state == ButtonState::LONGPRESSED )
                postEvent( obj, MouseButtonLongClick, bs );
            else if ( bs->state == ButtonState::PRESSED )
                postEvent( obj, MouseButtonShortClick, bs );
            bs->state = ButtonState::RELEASED;
            bs->timer->stop();
            break;

        case QEvent::Leave:
        case QEvent::MouseButtonDblClick:
            mouseevent = static_cast<QMouseEvent *>(e);
            bs = getState( mouseevent->button() );
            bs->state = ButtonState::RELEASED;
            bs->timer->stop();
            // ff

        default:
            break;
    }

    /* Pass requests to original handler */
    return parent()->eventFilter( obj, e );
}
