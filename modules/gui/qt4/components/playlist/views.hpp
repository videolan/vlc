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
#include <QTreeView>

class QPainter;
class PLModel;

class AbstractPlViewItemDelegate : public QStyledItemDelegate
{
public:
    AbstractPlViewItemDelegate( QWidget * parent = 0 ) : QStyledItemDelegate(parent) {}
    void paintBackground( QPainter *, const QStyleOptionViewItem &, const QModelIndex & ) const;
};

class PlIconViewItemDelegate : public AbstractPlViewItemDelegate
{
    Q_OBJECT

public:
    PlIconViewItemDelegate(QWidget *parent = 0) : AbstractPlViewItemDelegate( parent ) {}
    virtual void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    virtual QSize sizeHint ( const QStyleOptionViewItem & option = QStyleOptionViewItem(),
                     const QModelIndex & index = QModelIndex() ) const;
};

class PlListViewItemDelegate : public AbstractPlViewItemDelegate
{
    Q_OBJECT

public:
    PlListViewItemDelegate(QWidget *parent = 0) : AbstractPlViewItemDelegate(parent) {}

    virtual void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    virtual QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
};

class PlIconView : public QListView
{
    Q_OBJECT

public:
    PlIconView( PLModel *model, QWidget *parent = 0 );
protected:
    virtual void startDrag ( Qt::DropActions supportedActions );
    virtual void dragMoveEvent ( QDragMoveEvent * event );
};

class PlListView : public QListView
{
    Q_OBJECT

public:
    PlListView( PLModel *model, QWidget *parent = 0 );
protected:
    virtual void startDrag ( Qt::DropActions supportedActions );
    virtual void dragMoveEvent ( QDragMoveEvent * event );
    virtual void keyPressEvent( QKeyEvent *event );
};

class PlTreeView : public QTreeView
{
    Q_OBJECT

protected:
    virtual void startDrag ( Qt::DropActions supportedActions );
    virtual void dragMoveEvent ( QDragMoveEvent * event );
    virtual void keyPressEvent( QKeyEvent *event );
};

#endif

