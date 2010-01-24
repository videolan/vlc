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

#include <QPainter>

void PlListViewItemDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
}

QSize PlListViewItemDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    return QSize(100, 100);
}


PlIconView::PlIconView( PLModel *model, QWidget *parent ) : QListView( parent )
{
    setModel( model );
    setViewMode( QListView::IconMode );
    setMovement( QListView::Snap );

    PlListViewItemDelegate *pl = new PlListViewItemDelegate();
    setItemDelegate( pl );
}

