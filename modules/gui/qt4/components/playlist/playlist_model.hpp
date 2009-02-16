/*****************************************************************************
 * playlist_model.hpp : Model for a playlist tree
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#define DEPTH_PL -1
#define DEPTH_SEL 1

enum {
    ItemUpdate_Type = QEvent::User + PLEventType + 2,
    ItemDelete_Type = QEvent::User + PLEventType + 3,
    ItemAppend_Type = QEvent::User + PLEventType + 4,
    PLUpdate_Type   = QEvent::User + PLEventType + 5,
};

class PLEvent : public QEvent
{
public:
    PLEvent( int type, int id ) : QEvent( (QEvent::Type)(type) )
    {
        i_id = id;
        add.i_node = -1;
        add.i_item = -1;
    };

    PLEvent( const playlist_add_t  *a ) : QEvent( (QEvent::Type)(ItemAppend_Type) )
    {
        add = *a;
    };

    virtual ~PLEvent() { };

    int i_id;
    playlist_add_t add;
};


class PLModel : public QAbstractItemModel
{
    Q_OBJECT

friend class PLItem;

public:
    PLModel( playlist_t *, intf_thread_t *,
             playlist_item_t *, int, QObject *parent = 0 );
    ~PLModel();

    /* All types of lookups / QModel stuff */
    QVariant data( const QModelIndex &index, int role ) const;
    Qt::ItemFlags flags( const QModelIndex &index ) const;
    QVariant headerData( int section, Qt::Orientation orientation,
                         int role = Qt::DisplayRole ) const;
    QModelIndex index( int r, int c, const QModelIndex &parent ) const;
    QModelIndex index( PLItem *, int c ) const;
    int itemId( const QModelIndex &index ) const;
    bool isCurrent( const QModelIndex &index );
    QModelIndex parent( const QModelIndex &index ) const;
    int childrenCount( const QModelIndex &parent = QModelIndex() ) const;
    int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    int columnCount( const QModelIndex &parent = QModelIndex() ) const;

    /* Get current selection */
    QStringList selectedURIs();

    void rebuild(); void rebuild( playlist_item_t * );
    bool hasRandom(); bool hasLoop(); bool hasRepeat();

    /* Actions made by the views */
    void popup( QModelIndex & index, QPoint &point, QModelIndexList list );
    void doDelete( QModelIndexList selected );
    void search( QString search );
    void sort( int column, Qt::SortOrder order );
    void removeItem( int );

    /* DnD handling */
    Qt::DropActions supportedDropActions() const;
    QMimeData* mimeData( const QModelIndexList &indexes ) const;
    bool dropMimeData( const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &target );
    QStringList mimeTypes() const;

    int shownFlags() { return rootItem->i_showflags;  }

private:
    void addCallbacks();
    void delCallbacks();
    void customEvent( QEvent * );

    PLItem *rootItem;

    playlist_t *p_playlist;
    intf_thread_t *p_intf;
    int i_depth;

    static QIcon icons[ITEM_TYPE_NUMBER];

    /* Update processing */
    void ProcessItemRemoval( int i_id );
    void ProcessItemAppend( const playlist_add_t *p_add );

    void UpdateTreeItem( PLItem *, bool, bool force = false );
    void UpdateTreeItem( playlist_item_t *, PLItem *, bool, bool forc = false );
    void UpdateNodeChildren( PLItem * );
    void UpdateNodeChildren( playlist_item_t *, PLItem * );

    /* Actions */
    void recurseDelete( QList<PLItem*> children, QModelIndexList *fullList );
    void doDeleteItem( PLItem *item, QModelIndexList *fullList );

    /* Popup */
    int i_popup_item, i_popup_parent;
    QModelIndexList current_selection;
    QSignalMapper *ContextUpdateMapper;

    /* Lookups */
    PLItem *FindById( PLItem *, int );
    PLItem *FindByInput( PLItem *, int );
    PLItem *FindInner( PLItem *, int , bool );
    PLItem *p_cached_item;
    PLItem *p_cached_item_bi;
    int i_cached_id;
    int i_cached_input_id;
signals:
    void shouldRemove( int );

public slots:
    void activateItem( const QModelIndex &index );
    void activateItem( playlist_item_t *p_item );
    void setRandom( bool );
    void setLoop( bool );
    void setRepeat( bool );

private slots:
    void popupPlay();
    void popupDel();
    void popupInfo();
    void popupStream();
    void popupSave();
    void popupExplore();
    void viewchanged( int );
    void ProcessInputItemUpdate( int i_input_id );
    void ProcessInputItemUpdate( input_thread_t* p_input );
};

#endif
