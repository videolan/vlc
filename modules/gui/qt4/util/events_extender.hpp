/*****************************************************************************
 * events_extender.hpp: Events Extenders / Modifiers
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

#ifndef EVENTS_EXTENDER_HPP
#define EVENTS_EXTENDER_HPP

#include <QObject>
#include <QEvent>
#include <QList>
#include <QPoint>

class QMouseEvent;
class QTimer;

//! MouseEventExtenderFilter
/*!
Adds special QObject mouse events per button: LongPress, ShortClick
and LongClick.
Use by registering the extender as Object's event Filter. Will pass
back every original event to Object's filter.
*/

/* Note: The Extender being a QObject too, you can chain multiple extenders */

class MouseEventExtenderFilter : public QObject
{
    Q_OBJECT

public:
    //! MouseButtonLongPress Event.
    /*!
    Fired when the mouse has been pressed for longer than a single
    click time.
    */
    static const QEvent::Type MouseButtonLongPress;
    //! MouseButtonShortClick Event.
    /*!
    Single mouse click event. Sent by extender to differenciate
    from the Object's unmodified clicked() signal.
    */
    static const QEvent::Type MouseButtonShortClick;
    //! MouseButtonLongClick Event.
    /*!
    Single mouse long click event. Preceded by a LongPress Event.
    */
    static const QEvent::Type MouseButtonLongClick;
    MouseEventExtenderFilter( QObject *parent );
    ~MouseEventExtenderFilter();

protected:
     bool eventFilter( QObject *, QEvent * );

private:
     class ButtonState
     {
     public:
         ButtonState( QObject *, Qt::MouseButton );
         ~ButtonState();
         enum
         {
             RELEASED,
             PRESSED,
             LONGPRESSED
         } state;
         QPoint localPos;
         Qt::MouseButton button;
         Qt::MouseButtons buttons;
         Qt::KeyboardModifiers modifiers;
         QTimer *timer;
     };
     ButtonState * getState( Qt::MouseButton );
     ButtonState * getStateByTimer( int );
     void postEvent( QObject *, QEvent::Type, ButtonState * ) const;
     QList<ButtonState *> buttonsStates;
};

#endif // EVENTS_EXTENDER_HPP
