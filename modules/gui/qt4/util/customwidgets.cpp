/*****************************************************************************
 * customwidgets.cpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * Copyright (C) 2004 Daniel Molkentin <molkentin@kde.org>
 * $Id: qvlcframe.hpp 16283 2006-08-17 18:16:09Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 * The "ClickLineEdit" control is based on code by  Daniel Molkentin
 * <molkentin@kde.org> for libkdepim
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#include "customwidgets.hpp"
#include <QPainter>
#include <QLineEdit>
#include <QPainter>
#include <QColorGroup>
#include <QRect>

ClickLineEdit::ClickLineEdit( const QString &msg, QWidget *parent) : QLineEdit( parent )
{
    mDrawClickMsg = true;
    setClickMessage( msg );
}

void ClickLineEdit::setClickMessage( const QString &msg )
{
    mClickMessage = msg;
    repaint();
}


void ClickLineEdit::setText( const QString &txt )
{
    mDrawClickMsg = txt.isEmpty();
    repaint();
    QLineEdit::setText( txt );
}

void ClickLineEdit::paintEvent( QPaintEvent *pe )
{
    QPainter p( this );
    QLineEdit::paintEvent( pe );
    if ( mDrawClickMsg == true && !hasFocus() ) {
        QPen tmp = p.pen();
        p.setPen( palette().color( QPalette::Disabled, QPalette::Text ) );
        QRect cr = contentsRect();
        // Add two pixel margin on the left side
        cr.setLeft( cr.left() + 3 );
        p.drawText( cr, Qt::AlignLeft | Qt::AlignVCenter, mClickMessage );
        p.setPen( tmp );
    }
}

void ClickLineEdit::dropEvent( QDropEvent *ev )
{
    mDrawClickMsg = false;
    QLineEdit::dropEvent( ev );
}


void ClickLineEdit::focusInEvent( QFocusEvent *ev )
{
    if ( mDrawClickMsg == true ) {
        mDrawClickMsg = false;
        repaint();
    }
    QLineEdit::focusInEvent( ev );
}


void ClickLineEdit::focusOutEvent( QFocusEvent *ev )
{
    if ( text().isEmpty() ) {
        mDrawClickMsg = true;
        repaint();
    }
    QLineEdit::focusOutEvent( ev );
}
