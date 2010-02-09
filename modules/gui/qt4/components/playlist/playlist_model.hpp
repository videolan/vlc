/*****************************************************************************
 * playlist_model.hpp : Model for a playlist tree
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jakob Leben <jleben@videolan.org>
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

#ifndef _PLAYLIST_MODEL_H_
#define _PLAYLIST_MODEL_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"

#include <vlc_input.h>
#include <vlc_playlist.h>

#include "playlist_item.hpp"

#include <QModelIndex>
#include <QObject>
#include <QEvent>
#include <QMimeData>
#include <QSignalMapper>
#include <QAbstractItemModel>
#include <QVariant>

class QSignalMapper;
class PLItem;

class PLModel : public QAbstractItemModel
{
    Q_OBJECT

friend class PLItem;

public:
    enum {
      IsCurrentRole = Qt::UserRole
    };

    PLModel( playlist_t *, intf_thread_t *,
             playlist_item_t *, QObject *parent = 0 );
    ~PLModel();

    /*** QModel subclassing ***/

    /* Data structure */
    QVariant data( const QModelIndex &index, int role ) const;
    QVariant headerData( int section, Qt::Orientation orientation,
                         int role = Qt::DisplayRole ) const;
    int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    int columnCount( const QModelIndex &parent = QModelIndex() ) const;
    Qt::ItemFlags flags( const QModelIndex &index ) const;
    QModelIndex index( int r, int c, const QModelIndex &parent ) const;
    QModelIndex parent( const QModelIndex &index ) const;

    /* Drag and Drop */
    Qt::DropActions supportedDropActions() const;
    QMimeData* mimeData( const QModelIndexList &indexes ) const;
    bool dropMimeData( const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &target );
    QStringList mimeTypes() const;

    /**** Custom ****/

    /* Lookups */
    QStringList selectedURIs();
    QModelIndex index( PLItem *, int c ) const;
    QModelIndex index( int i_id, int c );
    QModelIndex currentIndex();
    bool isCurrent( const QModelIndex &index ) const;
    int itemId( const QModelIndex &index ) const;

    /* Actions */
    void popup( const QModelIndex & index, const QPoint &point, const QModelIndexList &list );
    void doDelete( QModelIndexList selected );
    void search( const QString& search_text );
    void sort( int column, Qt::SortOrder order );
    void sort( int i_root_id, int column, Qt::SortOrder order );
    void removeItem( int );
    void rebuild(); void rebuild( playlist_item_t *, bool b_first = false );

    inline PLItem *getItem( QModelIndex index ) const
    {
        if( index.isValid() )
            return static_cast<PLItem*>( index.internalPointer() );
        else return rootItem;
    }

private:

    /* General */
    PLItem *rootItem;

    playlist_t *p_playlist;
    intf_thread_t *p_intf;
    int i_depth;

    static QIcon icons[ITEM_TYPE_NUMBER];

    /* Actions */
    void recurseDelete( QList<PLItem*> children, QModelIndexList *fullList );
    void doDeleteItem( PLItem *item, QModelIndexList *fullList );
    void updateTreeItem( PLItem * );
    void removeItem ( PLItem * );
    void takeItem( PLItem * ); //will not delete item
    void insertChildren( PLItem *node, QList<PLItem*>& items, int i_pos );
    void dropAppendCopy( QByteArray& data, PLItem *target );
    void dropMove( QByteArray& data, PLItem *target, int new_pos );
    /* The following actions will not signal the view! */
    void updateChildren( PLItem * );
    void updateChildren( playlist_item_t *, PLItem * );

    /* Popup */
    int i_popup_item, i_popup_parent, i_popup_column;
    QModelIndexList current_selection;

    /* Lookups */
    PLItem *findById( PLItem *, int );
    PLItem *findByInput( PLItem *, int );
    PLItem *findInner( PLItem *, int , bool );

    int columnFromMeta( int meta_column ) const;
    int columnToMeta( int column ) const;
    bool canEdit() const;
    PLItem *p_cached_item;
    PLItem *p_cached_item_bi;
    int i_cached_id;
    int i_cached_input_id;

signals:
    void currentChanged( const QModelIndex& );

public slots:
    void activateItem( const QModelIndex &index );
    void activateItem( playlist_item_t *p_item );

private slots:
    void popupPlay();
    void popupDel();
    void popupInfo();
    void popupStream();
    void popupSave();
    void popupExplore();
    void popupAddNode();
    void popupSortAsc();
    void popupSortDesc();
    void processInputItemUpdate( input_item_t *);
    void processInputItemUpdate( input_thread_t* p_input );
    void processItemRemoval( int i_id );
    void processItemAppend( int item, int parent );
};

#endif
