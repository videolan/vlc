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

#include "BrowseButton.hpp"

#include <QPainter>
#include <QStyleOptionToolButton>

BrowseButton::BrowseButton( QWidget* parent, BrowseButton::Type type )
    : RoundButton( parent )
{
    setIconSize( QSize( 16, 16 ) );
    setType( type );
}

BrowseButton::Type BrowseButton::type() const
{
    return mType;
}

void BrowseButton::setType( BrowseButton::Type type )
{
    //FIXME
    switch ( type ) {
        case BrowseButton::Backward:
            setIcon( QIcon::fromTheme( "media-seek-backward" ) );
            break;
        case BrowseButton::Forward:
            setIcon( QIcon::fromTheme( "media-seek-forward" ) );
            break;
    }

    mType = type;
}

QSize BrowseButton::sizeHint() const
{
    return QSize( 50, 26 );
}

void BrowseButton::paintEvent( QPaintEvent* event )
{
    /*RoundButton::paintEvent( event );
    return;*/

    Q_UNUSED( event );

    const int corner = 5;
    const int margin = 5;
    QPainter painter( this );
    QStyleOptionToolButton option;

    initStyleOption( &option );
    painter.setRenderHint( QPainter::Antialiasing );

    painter.setPen( QPen( pen( &option ), 1 ) );
    painter.setBrush( brush( &option ) );
    painter.drawRoundedRect( rect().adjusted( 1, 1, -1, -1 ), corner, corner );

    switch ( mType ) {
        case BrowseButton::Backward:
            option.rect = option.rect.adjusted( 0, 0, -height() +margin, 0 );
            break;
        case BrowseButton::Forward:
            option.rect = option.rect.adjusted( height() -margin, 0, 0, 0 );
            break;
    }

    style()->drawControl( QStyle::CE_ToolButtonLabel, &option, &painter, this );
}

