/*****************************************************************************
 * Copyright © 2011 VideoLAN
 * $Id$
 *
 * Authors: Filipe Azevedo, aka PasNox
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "DeckButtonsLayout.hpp"

DeckButtonsLayout::DeckButtonsLayout( QWidget* parent )
    : QLayout( parent )
{
    backwardItem = 0;
    goItem = 0;
    forwardItem = 0;

    setContentsMargins( 0, 0, 0, 0 );
    setSpacing( 0 );

    setBackwardButton( 0 );
    setRoundButton( 0 );
    setForwardButton( 0 );
}

DeckButtonsLayout::~DeckButtonsLayout()
{
    delete backwardItem;
    delete goItem;
    delete forwardItem;
}

QSize DeckButtonsLayout::sizeHint() const
{
    const int bbw = backwardButton ? backwardButton->sizeHint().width() : 0;
    const int fbw = forwardButton ? forwardButton->sizeHint().width() : 0;
    const int gbw = bbw +fbw == 0 ? ( RoundButton ? RoundButton->sizeHint().width() : 0 ) : bbw +fbw;
    int left; int top; int right; int bottom;
    QSize sh = QSize( gbw, 0 );

    getContentsMargins( &left, &top, &right, &bottom );

    sh.setHeight( qMax( sh.height(), backwardButton ? backwardButton->sizeHint().height() : 0 ) );
    sh.setHeight( qMax( sh.height(), RoundButton ? RoundButton->sizeHint().height() : 0 ) );
    sh.setHeight( qMax( sh.height(), forwardButton ? forwardButton->sizeHint().height() : 0 ) );

    sh.rwidth() += left +right;
    sh.rheight() += top +bottom;

    return sh;
}

int DeckButtonsLayout::count() const
{
    return 3;
}

void DeckButtonsLayout::setGeometry( const QRect& _r )
{
    QLayout::setGeometry( _r );

    int left; int top; int right; int bottom;
    getContentsMargins( &left, &top, &right, &bottom );

    const QRect r = _r.adjusted( left, top, right, bottom );
    const QAbstractButton* button = backwardButton ? backwardButton : forwardButton;
    qreal factor = 1;

    if ( !button ) {
        if ( RoundButton ) {
            const int min = qMin( r.height(), r.width() );
            QRect rect = QRect( QPoint(), QSize( min, min ) );

            rect.moveCenter( r.center() );
            RoundButton->setGeometry( rect );
        }

        return;
    }
    else if ( backwardButton && forwardButton ) {
        factor = (qreal)r.width() /(qreal)( button->sizeHint().width() *2 );
    }
    else if ( RoundButton ) {
        factor = (qreal)r.width() /(qreal)( button->sizeHint().width() +( RoundButton->sizeHint().width() /2 ) );
    }
    else {
        factor = (qreal)r.width() /(qreal)( button->sizeHint().width() );
    }

    if ( RoundButton ) {
        int height = (qreal)RoundButton->sizeHint().height() *factor;

        while ( height > r.height() ) {
            factor -= 0.1;
            height = (qreal)RoundButton->sizeHint().height() *factor;
        }

        QRect rect( QPoint(), QSize( height, height ) );
        rect.moveCenter( r.center() );

        if ( backwardButton && forwardButton ) {
            // nothing to do
        }
        else if ( backwardButton ) {
            rect.moveRight( r.right() );
        }
        else if ( forwardButton ) {
            rect.moveLeft( r.left() );
        }

        RoundButton->setGeometry( rect );
    }
    else {
        int height = (qreal)button->sizeHint().height() *factor;

        while ( height > r.height() ) {
            factor -= 0.1;
            height = (qreal)button->sizeHint().height() *factor;
        }
    }

    const QSize bs = QSize( (qreal)button->sizeHint().width() *factor, (qreal)button->sizeHint().height() *factor );

    if ( backwardButton ) {
        QRect gr = RoundButton ? QRect( QPoint(), RoundButton->size() ) : r;
        QRect rect = QRect( QPoint(), bs );

        if ( RoundButton ) {
            gr.moveTopLeft( RoundButton->pos() );
        }

        rect.moveCenter( gr.center() );
        rect.moveRight( gr.center().x() +1 );

        backwardButton->setGeometry( rect );
    }

    if ( forwardButton ) {
        QRect gr = RoundButton ? QRect( QPoint(), RoundButton->size() ) : r;
        QRect rect = QRect( QPoint(), bs );

        if ( RoundButton ) {
            gr.moveTopLeft( RoundButton->pos() );
        }

        rect.moveCenter( gr.center() );
        rect.moveLeft( gr.center().x() );

        forwardButton->setGeometry( rect );
    }

    if ( RoundButton ) {
        RoundButton->raise();
    }
}

void DeckButtonsLayout::addItem( QLayoutItem* item )
{
    Q_UNUSED( item );
}

QLayoutItem* DeckButtonsLayout::itemAt( int index ) const
{
    switch ( index ) {
        case 0:
            return backwardItem;
        case 1:
            return goItem;
        case 2:
            return forwardItem;
    }

    return 0;
}

QLayoutItem* DeckButtonsLayout::takeAt( int index )
{
    QLayoutItem* item = itemAt( index );

    switch ( index ) {
        case 0: {
            backwardItem = 0;

            if ( backwardButton ) {
                backwardButton->setParent( 0 );
            }

            backwardButton = 0;
            break;
        }
        case 1: {
            goItem = 0;

            if ( RoundButton ) {
                RoundButton->setParent( 0 );
            }

            RoundButton = 0;
            break;
        }
        case 2: {
            forwardItem = 0;

            if ( forwardButton ) {
                forwardButton->setParent( 0 );
            }

            forwardButton = 0;
            break;
        }
    }

    update();

    return item;
}

void DeckButtonsLayout::setBackwardButton( QAbstractButton* button )
{
    if ( backwardButton && button == backwardButton ) {
        return;
    }

    if ( backwardItem ) {
        delete takeAt( 0 );
    }

    if ( button ) {
        addChildWidget( button );
    }

    backwardItem = new QWidgetItem( button );
    backwardButton = button;

    update();
}

void DeckButtonsLayout::setRoundButton( QAbstractButton* button )
{
    if ( RoundButton && button == RoundButton ) {
        return;
    }

    if ( goItem ) {
        delete takeAt( 1 );
    }

    if ( button ) {
        addChildWidget( button );
    }

    goItem = new QWidgetItem( button );
    RoundButton = button;

    update();
}

void DeckButtonsLayout::setForwardButton( QAbstractButton* button )
{
    if ( forwardButton && button == forwardButton ) {
        return;
    }

    if ( forwardItem ) {
        delete takeAt( 2 );
    }

    if ( button ) {
        addChildWidget( button );
    }

    forwardItem = new QWidgetItem( button );
    forwardButton = button;

    update();
}
