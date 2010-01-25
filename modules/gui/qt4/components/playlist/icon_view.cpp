/*****************************************************************************
 * icon_view.cpp : Icon view for the Playlist
 ****************************************************************************
 * Copyright © 2010 the VideoLAN team
 * $Id$
 *
 * Authors:         Jean-Baptiste Kempf <jb@videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "components/playlist/icon_view.hpp"
#include "components/playlist/playlist_model.hpp"
#include "input_manager.hpp"

#include <QPainter>
#include <QRect>
#include <QStyleOptionViewItem>

#include "assert.h"

#define RECT_SIZE 100
#define ART_SIZE  64
#define OFFSET    (100-64)/2

void PlListViewItemDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    painter->setRenderHint( QPainter::Antialiasing );

    if( option.state & QStyle::State_Selected )
         painter->fillRect(option.rect, option.palette.highlight());

    PLItem *currentItem = static_cast<PLItem*>( index.internalPointer() );
    assert( currentItem );

    QPixmap pix;
    QString url = InputManager::decodeArtURL( currentItem->inputItem() );

    if( !url.isEmpty() && pix.load( url ) )
    {
        pix = pix.scaled( ART_SIZE, ART_SIZE, Qt::KeepAspectRatioByExpanding );
    }
    else
    {
        pix = QPixmap( ":/noart64" );
    }

    QRect art_rect = option.rect.adjusted( OFFSET - 1, 0, - OFFSET, - OFFSET *2 );

    painter->drawPixmap( art_rect, pix );

    painter->setFont( QFont( "Verdana", 7 ) );

    QRect textRect = option.rect.adjusted( 1, ART_SIZE + 2, -1, -1 );
    painter->drawText( textRect, qfu( input_item_GetTitle( currentItem->inputItem() ) ),
                       QTextOption( Qt::AlignCenter ) );

}

QSize PlListViewItemDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    return QSize( RECT_SIZE, RECT_SIZE);
}


PlIconView::PlIconView( PLModel *model, QWidget *parent ) : QListView( parent )
{
    setModel( model );
    setViewMode( QListView::IconMode );
    setMovement( QListView::Snap );

    PlListViewItemDelegate *pl = new PlListViewItemDelegate();
    setItemDelegate( pl );
}
