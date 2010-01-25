/*****************************************************************************
 * icon_view.hpp : Icon view for the Playlist
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

#ifndef _ICON_VIEW_H_
#define _ICON_VIEW_H_

#include <QStyledItemDelegate>
#include <QListView>

class QPainter;
class PLModel;

class PlListViewItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    PlListViewItemDelegate(QWidget *parent = 0) : QStyledItemDelegate(parent) {}

    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
};

class PlIconView : public QListView
{
    Q_OBJECT

public:
    PlIconView( PLModel *model, QWidget *parent = 0 );
public slots:
    void activate( const QModelIndex & index );
};

#endif

