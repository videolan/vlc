/*****************************************************************************
 * ml_model.hpp ML model
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

#ifdef MEDIA_LIBRARY
#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_media_library.h>

#include "components/playlist/vlc_model.hpp"
#include "ml_item.hpp"
#include "qt4.hpp"

class MLItem;

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

    virtual int itemId( const QModelIndex & ) const;
    virtual input_item_t *getInputItem( const QModelIndex &index ) const;

    QVariant data( const QModelIndex &idx, const int role = Qt::DisplayRole ) const;
    bool setData( const QModelIndex &idx, const QVariant &value,
                  int role = Qt::EditRole );
    ml_select_e columnType( int column ) const;

    QModelIndex index( int row, int column,
                       const QModelIndex & parent = QModelIndex() ) const;
    virtual QModelIndex currentIndex() const;
    int rowCount( const QModelIndex & parent = QModelIndex() ) const;

    QModelIndex parent( const QModelIndex& ) const;
    QVariant headerData( int, Qt::Orientation, int ) const;
    Qt::ItemFlags flags( const QModelIndex& ) const;
    bool isEditable( const QModelIndex& ) const;

    // Drag and drop: MIME data
    QMimeData* mimeData( const QModelIndexList & indexes ) const;

    // Custom functions
    int insertMedia( ml_media_t *p_media, int row = -1,
                     bool bSignal = true );
    int appendMedia( ml_media_t *p_media );
    int insertMediaArray( vlc_array_t *p_media_array, int row = -1,
                          bool bSignal = true );

    int insertResult( const ml_result_t *p_result, int row = -1,
                      bool bSignal = true );
    inline int appendResult( const ml_result_t *p_result );
    int insertResultArray( vlc_array_t *p_result_array, int row = -1,
                           bool bSignal = true );

    virtual void doDelete( QModelIndexList list );
    void remove( QModelIndex idx );

    void clear();
    void play( const QModelIndex &idx );
    virtual QString getURI( const QModelIndex &index ) const;
    virtual QModelIndex rootIndex() const;
    virtual bool isTree() const;
    virtual bool canEdit() const;
    virtual bool isCurrentItem( const QModelIndex &index, playLocation where ) const;
    QModelIndex getIndexByMLID( int id ) const;

public slots:
    void activateItem( const QModelIndex &index );
    virtual void actionSlot( QAction *action );

protected:
    void remove( MLItem *item );
    inline MLItem *getItem( QModelIndex index ) const
    {
        if( index.isValid() )
            return static_cast<MLItem*>( index.internalPointer() );
        else return NULL;
    }

private:
    QList< MLItem* > items;
    media_library_t* p_ml;

};

#endif
#endif
