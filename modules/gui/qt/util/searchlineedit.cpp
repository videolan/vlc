/*****************************************************************************
 * searchlineedit.cpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * Copyright (C) 2004 Daniel Molkentin <molkentin@kde.org>
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "searchlineedit.hpp"
#include "customwidgets.hpp"

#include "qt4.hpp" /*needed for qtr and CONNECT, but not necessary */

#include <QPainter>
#include <QRect>
#include <QStyle>
#include <QStyleOption>

#include <vlc_intf_strings.h>

#if !HAS_QT47

ClickLineEdit::ClickLineEdit( const QString &msg, QWidget *parent) : QLineEdit( parent )
{
    mDrawClickMsg = true;
    setPlaceholderText( msg );
}

void ClickLineEdit::setPlaceholderText( const QString &msg )
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
    QLineEdit::paintEvent( pe );
    if ( mDrawClickMsg && !hasFocus() ) {
        QPainter p( this );
        QPen tmp = p.pen();
        p.setPen( palette().color( QPalette::Disabled, QPalette::Text ) );
        QRect cr = contentsRect();
        // Add two pixel margin on the left side
        cr.setLeft( cr.left() + 3 );
        p.drawText( cr, Qt::AlignLeft | Qt::AlignVCenter, mClickMessage );
        p.setPen( tmp );
        p.end();
    }
}

void ClickLineEdit::dropEvent( QDropEvent *ev )
{
    mDrawClickMsg = false;
    QLineEdit::dropEvent( ev );
}

void ClickLineEdit::focusInEvent( QFocusEvent *ev )
{
    if ( mDrawClickMsg ) {
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
#endif

#ifndef Q_OS_MAC
SearchLineEdit::SearchLineEdit( QWidget *parent ) : QLineEdit( parent )
{
    clearButton = new QFramelessButton( this );
    clearButton->setIcon( QIcon( ":/search_clear" ) );
    clearButton->setIconSize( QSize( 16, 16 ) );
    clearButton->setCursor( Qt::ArrowCursor );
    clearButton->setToolTip( qfu(vlc_pgettext("Tooltip|Clear", "Clear")) );
    clearButton->hide();

    CONNECT( clearButton, clicked(), this, clear() );

    int frameWidth = style()->pixelMetric( QStyle::PM_DefaultFrameWidth, 0, this );

    QFontMetrics metrics( font() );
    QString styleSheet = QString( "min-height: %1px; "
                                  "padding-top: 1px; "
                                  "padding-bottom: 1px; "
                                  "padding-right: %2px;" )
                                  .arg( metrics.height() + ( 2 * frameWidth ) )
                                  .arg( clearButton->sizeHint().width() + 6 );
    setStyleSheet( styleSheet );

    setMessageVisible( true );

    CONNECT( this, textEdited( const QString& ),
             this, updateText( const QString& ) );

    CONNECT( this, editingFinished(),
             this, searchEditingFinished() );

}

void SearchLineEdit::clear()
{
    setText( QString() );
    clearButton->hide();
    setMessageVisible( true );
}

void SearchLineEdit::setMessageVisible( bool on )
{
    message = on;
    repaint();
    return;
}

void SearchLineEdit::updateText( const QString& text )
{
    /* if reset() won't be focused out */
    if ( !text.isEmpty() ) setMessageVisible( false );
    clearButton->setVisible( !text.isEmpty() );
}

void SearchLineEdit::resizeEvent ( QResizeEvent * event )
{
    QLineEdit::resizeEvent( event );
    int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth,0,this);
    clearButton->resize( clearButton->sizeHint().width(), height() );
    clearButton->move( width() - clearButton->width() - frameWidth - 3,
                      ( height() - clearButton->height() + 2 ) / 2 );
}

void SearchLineEdit::focusInEvent( QFocusEvent *event )
{
    if( message )
    {
        setMessageVisible( false );
    }
    QLineEdit::focusInEvent( event );
}

void SearchLineEdit::focusOutEvent( QFocusEvent *event )
{
    if( text().isEmpty() )
    {
        setMessageVisible( true );
    }
    QLineEdit::focusOutEvent( event );
}

void SearchLineEdit::paintEvent( QPaintEvent *event )
{
    QLineEdit::paintEvent( event );
    if( !message ) return;
    QStyleOption option;
    option.initFrom( this );
    QRect rect = style()->subElementRect( QStyle::SE_LineEditContents, &option, this )
        .adjusted( 3, 0, clearButton->width() + 1, 0 );
    QPainter painter( this );
    painter.setPen( palette().color( QPalette::Disabled, QPalette::Text ) );
    painter.drawText( rect, Qt::AlignLeft | Qt::AlignVCenter, qtr( I_PL_SEARCH ) );
}

void SearchLineEdit::searchEditingFinished()
{
    emit searchDelayedChanged( text() );
}

#endif
