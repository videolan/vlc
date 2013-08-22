/*****************************************************************************
 * ml_model.hpp SQL ML model
 *****************************************************************************
 * Copyright (C) 2008-2011 the VideoLAN Team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
 *          Srikanth Raju <srikiraju#gmail#com>
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

#ifndef _MEDIA_LIBRARY_MLMODEL_H
#define _MEDIA_LIBRARY_MLMODEL_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef SQL_MEDIA_LIBRARY
#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_media_library.h>

#include "components/playlist/vlc_model.hpp"
#include "ml_item.hpp"
#include "qt4.hpp"

#include <QMutex>
#include <QEvent>
class MLItem;

/** *************************************************************************
 * \brief ML Events class because we don't want direct callbacks
 ****************************************************************************/
class MLEvent : public QEvent
{
public:
    static const QEvent::Type MediaAdded_Type;
    static const QEvent::Type MediaRemoved_Type;
    static const QEvent::Type MediaUpdated_Type;
    MLEvent( QEvent::Type type, media_library_t *_p_ml, int32_t _ml_media_id ) :
        QEvent( type ), ml_media_id( _ml_media_id ), p_ml( _p_ml ) {};
    int32_t ml_media_id;
    media_library_t * p_ml; /* store instance */
};

/** *************************************************************************
 * \brief Tree model for the result list
 ****************************************************************************/
class MLModel : public VLCModel
{
    Q_OBJECT;

public:
    // Basic QAbstractItemModel implementation
    MLModel( intf_thread_t *_p_intf, QObject *parent = NULL );
    virtual ~MLModel();

    QVariant data( const QModelIndex &idx, const int role = Qt::DisplayRole ) const;
    bool setData( const QModelIndex &idx, const QVariant &value,
                  int role = Qt::EditRole );
    QModelIndex index( int row, int column,
                       const QModelIndex & parent = QModelIndex() ) const;
    int rowCount( const QModelIndex & parent = QModelIndex() ) const;
    QModelIndex parent( const QModelIndex& ) const;
    Qt::ItemFlags flags( const QModelIndex& ) const;
    QMimeData* mimeData( const QModelIndexList & indexes ) const;
    virtual bool removeRows( int row, int count, const QModelIndex & parent = QModelIndex() );
    virtual void sort( const int column, Qt::SortOrder order = Qt::AscendingOrder );

    // Custom functions
    bool isEditable( const QModelIndex& ) const;
    ml_select_e columnType( int column ) const;
    virtual bool event( QEvent * e );

    QModelIndex getIndexByMLID( int id ) const;

    /* VLCModelSubInterface */
    virtual void rebuild( playlist_item_t * p = NULL );
    virtual void doDelete( QModelIndexList selected );
    virtual void createNode( QModelIndex, QString ) {};
    virtual void removeAll();

    virtual QModelIndex rootIndex() const;
    virtual void filter( const QString& search_text, const QModelIndex & root, bool b_recursive );
    virtual QModelIndex currentIndex() const;
    virtual QModelIndex indexByPLID( const int i_plid, const int c ) const;
    virtual QModelIndex indexByInputItemID( const int i_inputitem_id, const int c ) const;
    virtual bool isTree() const;
    virtual bool canEdit() const;

    virtual bool action( QAction *action, const QModelIndexList &indexes );
    virtual bool isSupportedAction( actions action, const QModelIndex & ) const;

    /* VLCModelSubInterface virtual slots */
    virtual void activateItem( const QModelIndex &index );

protected:

    /* VLCModel subclassing */
    bool isParent( const QModelIndex &index, const QModelIndex &current) const;
    bool isLeaf( const QModelIndex &index ) const;

private:
    /* custom */
    int insertMedia( ml_media_t *p_media, int row = -1 );
    int insertResultArray( vlc_array_t *p_result_array, int row = -1 );
    QList< MLItem* > items;
    media_library_t* p_ml;
};

#endif
#endif
