/*****************************************************************************
 * playlist_model.hpp : Model for a playlist tree
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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

#include <vlc_input.h>
#include <vlc_playlist.h>
#include "vlc_model.hpp"
#include "playlist_item.hpp"

#include <QObject>
#include <QEvent>
#include <QSignalMapper>
#include <QMimeData>
#include <QAbstractItemModel>
#include <QVariant>
#include <QModelIndex>

class PLItem;
class PLSelector;
class PlMimeData;
class QSignalMapper;

class PLModel : public VLCModel
{
    Q_OBJECT

public:
    PLModel( playlist_t *, intf_thread_t *,
             playlist_item_t *, QObject *parent = 0 );
    virtual ~PLModel();

    static PLModel* getPLModel( intf_thread_t *p_intf )
    {
        if(!p_intf->p_sys->pl_model )
        {
            playlist_Lock( THEPL );
            playlist_item_t *p_root = THEPL->p_playing;
            playlist_Unlock( THEPL );
            p_intf->p_sys->pl_model = new PLModel( THEPL, p_intf, p_root, NULL );
        }

        return p_intf->p_sys->pl_model;
    }

    /*** QModel subclassing ***/

    /* Data structure */
    virtual QVariant data( const QModelIndex &index, const int role ) const;
    virtual QVariant headerData( int section, Qt::Orientation orientation,
                         int role = Qt::DisplayRole ) const;
    virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual int columnCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;
    virtual QModelIndex index( const int r, const int c, const QModelIndex &parent ) const;
    virtual QModelIndex parent( const QModelIndex &index ) const;

    /* Drag and Drop */
    virtual Qt::DropActions supportedDropActions() const;
    virtual QMimeData* mimeData( const QModelIndexList &indexes ) const;
    virtual bool dropMimeData( const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &target );
    virtual QStringList mimeTypes() const;

    /**** Custom ****/

    /* Lookups */
    QStringList selectedURIs();
    QModelIndex index( PLItem *, const int c ) const;
    QModelIndex index( const int i_id, const int c );
    virtual QModelIndex currentIndex() const;
    bool isParent( const QModelIndex &index, const QModelIndex &current) const;
    bool isCurrent( const QModelIndex &index ) const;
    int itemId( const QModelIndex &index ) const;

    /* Actions */
    virtual bool popup( const QModelIndex & index, const QPoint &point, const QModelIndexList &list );
    virtual void doDelete( QModelIndexList selected );
    void search( const QString& search_text, const QModelIndex & root, bool b_recursive );
    void sort( const int column, Qt::SortOrder order );
    void sort( const int i_root_id, const int column, Qt::SortOrder order );
    void rebuild( playlist_item_t * p = NULL );

    inline PLItem *getItem( QModelIndex index ) const
    {
        if( index.isValid() )
            return static_cast<PLItem*>( index.internalPointer() );
        else return rootItem;
    }
    virtual int getId( QModelIndex index ) const
    {
        return getItem( index )->id();
    }
    inline int getZoom() const
    {
        return i_zoom;
    }

signals:
    void currentChanged( const QModelIndex& );
    void rootChanged();

public slots:
    virtual void activateItem( const QModelIndex &index );
    void activateItem( playlist_item_t *p_item );
    inline void changeZoom( const int zoom )
    {
        i_zoom = zoom;
        emit layoutChanged();
    }

private:
    /* General */
    PLItem *rootItem;

    playlist_t *p_playlist;

    static QIcon icons[ITEM_TYPE_NUMBER];

    /* Shallow actions (do not affect core playlist) */
    void updateTreeItem( PLItem * );
    void removeItem ( PLItem * );
    void removeItem( int );
    void recurseDelete( QList<PLItem*> children, QModelIndexList *fullList );
    void takeItem( PLItem * ); //will not delete item
    void insertChildren( PLItem *node, QList<PLItem*>& items, int i_pos );
    /* ...of which  the following will not update the views */
    void updateChildren( PLItem * );
    void updateChildren( playlist_item_t *, PLItem * );

    /* Deep actions (affect core playlist) */
    void dropAppendCopy( const PlMimeData * data, PLItem *target, int pos );
    void dropMove( const PlMimeData * data, PLItem *target, int new_pos );

    /* Popup */
    int i_popup_item, i_popup_parent, i_popup_column;
    QModelIndexList current_selection;
    QMenu *sortingMenu;
    QSignalMapper *sortingMapper;

    /* Lookups */
    PLItem *findById( PLItem *, int ) const;
    PLItem *findByInput( PLItem *, int ) const;
    PLItem *findInner(PLItem *, int , bool ) const;
    bool canEdit() const;

    PLItem *p_cached_item;
    PLItem *p_cached_item_bi;
    int i_cached_id;
    int i_cached_input_id;

    /* Zoom factor for font-size */
    int i_zoom;

private slots:
    void popupPlay();
    void popupDel();
    void popupInfo();
    void popupStream();
    void popupSave();
    void popupExplore();
    void popupAddNode();
    void popupAddToPlaylist();
    void popupSort( int column );
    void processInputItemUpdate( input_item_t *);
    void processInputItemUpdate( input_thread_t* p_input );
    void processItemRemoval( int i_id );
    void processItemAppend( int item, int parent );
};

class PlMimeData : public QMimeData
{
    Q_OBJECT

public:
    PlMimeData() {}
    ~PlMimeData();
    void appendItem( input_item_t *p_item );
    QList<input_item_t*> inputItems() const;
    QStringList formats () const;

private:
    QList<input_item_t*> _inputItems;
    QMimeData *_mimeData;
};

#endif
