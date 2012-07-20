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
#include <QTimer>
#include <QMutex>

class PLItem;
class PLSelector;
class PlMimeData;

class PLModel : public VLCModel
{
    Q_OBJECT

public:
    PLModel( playlist_t *, intf_thread_t *,
             playlist_item_t *, QObject *parent = 0 );
    virtual ~PLModel();

    /* Qt4 main PLModel */
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

    /*** QAbstractItemModel subclassing ***/

    /* Data structure */
    virtual QVariant data( const QModelIndex &index, const int role ) const;
    virtual QVariant headerData( int section, Qt::Orientation orientation,
                         int role = Qt::DisplayRole ) const;
    virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;
    virtual QModelIndex index( const int r, const int c, const QModelIndex &parent ) const;
    virtual QModelIndex parent( const QModelIndex &index ) const;

    /* Drag and Drop */
    virtual Qt::DropActions supportedDropActions() const;
    virtual QMimeData* mimeData( const QModelIndexList &indexes ) const;
    virtual bool dropMimeData( const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &target );
    virtual QStringList mimeTypes() const;

    /* Sort */
    virtual void sort( const int column, Qt::SortOrder order = Qt::AscendingOrder );

    /**** Custom ****/

    /* Lookups */
    QModelIndex index( const int i_id, const int c );
    virtual QModelIndex rootIndex() const;
    virtual bool isTree() const;
    virtual bool canEdit() const;
    virtual QModelIndex currentIndex() const;
    int itemId( const QModelIndex &index ) const;
    virtual input_item_t *getInputItem( const QModelIndex & ) const;
    virtual QString getURI( const QModelIndex &index ) const;
    QString getTitle( const QModelIndex &index ) const;
    virtual bool isCurrentItem( const QModelIndex &index, playLocation where ) const;

    /* */
    void search( const QString& search_text, const QModelIndex & root, bool b_recursive );
    void rebuild( playlist_item_t * p = NULL );

    virtual void doDelete( QModelIndexList selected );
    virtual void createNode( QModelIndex index, QString name );

signals:
    void currentIndexChanged( const QModelIndex& );
    void rootIndexChanged();

public slots:
    virtual void activateItem( const QModelIndex &index );
    void clearPlaylist();
    void ensureArtRequested( const QModelIndex &index );
    virtual void actionSlot( QAction *action );

private:
    /* General */
    PLItem *rootItem;

    playlist_t *p_playlist;

    static QIcon icons[ITEM_TYPE_NUMBER];

    /* single row linear inserts agregation */
    void bufferedRowInsert( PLItem *item, PLItem *parent, int pos );
    bool isBufferedForInsert( PLItem *parent, int i_item );
    PLItem *insertBufferRoot;
    int insertbuffer_firstrow;
    int insertbuffer_lastrow;
    QTimer insertBufferCommitTimer;
    QList<PLItem *> insertBuffer;
    QMutex insertBufferMutex;

    /* Custom model private methods */
    /* Lookups */
    PLItem *getItem( const QModelIndex & index ) const
    {
        if( index.isValid() )
            return static_cast<PLItem*>( index.internalPointer() );
        else return rootItem;
    }
    QModelIndex index( PLItem *, const int c ) const;
    bool isCurrent( const QModelIndex &index ) const;
    bool isParent( const QModelIndex &index, const QModelIndex &current) const;

    /* Shallow actions (do not affect core playlist) */
    void updateTreeItem( PLItem * );
    void removeItem ( PLItem * );
    void removeItem( int );
    void recurseDelete( QList<AbstractPLItem*> children, QModelIndexList *fullList );
    void takeItem( PLItem * ); //will not delete item
    void insertChildren( PLItem *node, QList<PLItem*>& items, int i_pos );
    /* ...of which  the following will not update the views */
    void updateChildren( PLItem * );
    void updateChildren( playlist_item_t *, PLItem * );

    /* Deep actions (affect core playlist) */
    void dropAppendCopy( const PlMimeData * data, PLItem *target, int pos );
    void dropMove( const PlMimeData * data, PLItem *target, int new_pos );

    /* */
    void sort( QModelIndex caller, QModelIndex rootIndex, const int column, Qt::SortOrder order );

    /* Lookups */
    PLItem *findById( PLItem *, int ) const;
    PLItem *findByInput( PLItem *, int ) const;
    PLItem *findInner(PLItem *, int , bool ) const;

    PLItem *p_cached_item;
    PLItem *p_cached_item_bi;
    int i_cached_id;
    int i_cached_input_id;

    /* */
    QString latestSearch;

private slots:
    void processInputItemUpdate( input_item_t *);
    void processInputItemUpdate( input_thread_t* p_input );
    void processItemRemoval( int i_id );
    void processItemAppend( int item, int parent );
    void commitBufferedRowInserts();
    void activateItem( playlist_item_t *p_item );
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
