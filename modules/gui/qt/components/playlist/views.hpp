/*****************************************************************************
 * views.hpp : Icon view for the Playlist
 ****************************************************************************
 * Copyright © 2010 the VideoLAN team
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

#ifndef VLC_QT_VIEWS_HPP_
#define VLC_QT_VIEWS_HPP_

#include <QStyledItemDelegate>
#include <QListView>
#include <QTreeView>
#include <QAbstractItemView>
#include "util/pictureflow.hpp"

class QPainter;

class AbstractPlViewItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    AbstractPlViewItemDelegate( QWidget * parent = 0 ) : QStyledItemDelegate(parent) {}
    void paintBackground( QPainter *, const QStyleOptionViewItem &, const QModelIndex & ) const;
};

class PlIconViewItemDelegate : public AbstractPlViewItemDelegate
{
    Q_OBJECT

public:
    PlIconViewItemDelegate(QWidget *parent = 0) : AbstractPlViewItemDelegate( parent ) {}
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const Q_DECL_OVERRIDE;
    QSize sizeHint ( const QStyleOptionViewItem & option = QStyleOptionViewItem(),
                     const QModelIndex & index = QModelIndex() ) const Q_DECL_OVERRIDE;
};

class PlListViewItemDelegate : public AbstractPlViewItemDelegate
{
    Q_OBJECT

public:
    PlListViewItemDelegate(QWidget *parent = 0) : AbstractPlViewItemDelegate(parent) {}

    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const Q_DECL_OVERRIDE;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const Q_DECL_OVERRIDE;
};

class PlTreeViewItemDelegate : public AbstractPlViewItemDelegate
{
    Q_OBJECT

public:
    PlTreeViewItemDelegate(QWidget *parent = 0) : AbstractPlViewItemDelegate(parent) {}

    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const Q_DECL_OVERRIDE;
};

class CellPixmapDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    CellPixmapDelegate(QWidget *parent = 0) : QStyledItemDelegate(parent) {}
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const Q_DECL_OVERRIDE;
};

class PlIconView : public QListView
{
    Q_OBJECT

public:
    PlIconView( QAbstractItemModel *model, QWidget *parent = 0 );
protected:
    void startDrag ( Qt::DropActions supportedActions ) Q_DECL_OVERRIDE;
    void dragMoveEvent ( QDragMoveEvent * event ) Q_DECL_OVERRIDE;
    bool viewportEvent ( QEvent * ) Q_DECL_OVERRIDE;
};

class PlListView : public QListView
{
    Q_OBJECT

public:
    PlListView( QAbstractItemModel *model, QWidget *parent = 0 );
protected:
    void startDrag ( Qt::DropActions supportedActions ) Q_DECL_OVERRIDE;
    void dragMoveEvent ( QDragMoveEvent * event ) Q_DECL_OVERRIDE;
    void keyPressEvent( QKeyEvent *event ) Q_DECL_OVERRIDE;
    bool viewportEvent ( QEvent * ) Q_DECL_OVERRIDE;
};

class PlTreeView : public QTreeView
{
    Q_OBJECT

public:
    PlTreeView( QAbstractItemModel *, QWidget *parent = 0 );
protected:
    void startDrag ( Qt::DropActions supportedActions ) Q_DECL_OVERRIDE;
    void dragMoveEvent ( QDragMoveEvent * event ) Q_DECL_OVERRIDE;
    void keyPressEvent( QKeyEvent *event ) Q_DECL_OVERRIDE;
    void setModel( QAbstractItemModel * ) Q_DECL_OVERRIDE;
};

class PicFlowView : public QAbstractItemView
{
    Q_OBJECT
public:
    PicFlowView( QAbstractItemModel *model, QWidget *parent = 0 );

    QRect visualRect(const QModelIndex&) const Q_DECL_OVERRIDE;
    void scrollTo(const QModelIndex&, QAbstractItemView::ScrollHint) Q_DECL_OVERRIDE;
    QModelIndex indexAt(const QPoint&) const Q_DECL_OVERRIDE;
    void setModel(QAbstractItemModel *model) Q_DECL_OVERRIDE;

protected:
    int horizontalOffset() const Q_DECL_OVERRIDE;
    int verticalOffset() const Q_DECL_OVERRIDE;
    QModelIndex moveCursor(QAbstractItemView::CursorAction, Qt::KeyboardModifiers) Q_DECL_OVERRIDE;
    bool isIndexHidden(const QModelIndex&) const Q_DECL_OVERRIDE;
    QRegion visualRegionForSelection(const QItemSelection&) const Q_DECL_OVERRIDE;
    void setSelection(const QRect&, QFlags<QItemSelectionModel::SelectionFlag>) Q_DECL_OVERRIDE;
    bool viewportEvent ( QEvent * ) Q_DECL_OVERRIDE;

private:
    PictureFlow *picFlow;

public slots:
    void dataChanged( const QModelIndex &, const QModelIndex &);
private slots:
    void playItem( int );
};

#endif
