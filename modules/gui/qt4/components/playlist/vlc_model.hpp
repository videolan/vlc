/*****************************************************************************
 * vlc_model.hpp : base for playlist and ml model
 ****************************************************************************
 * Copyright (C) 2010 the VideoLAN team and AUTHORS
 * $Id$
 *
 * Authors: Srikanth Raju <srikiraju#gmail#com>
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

#ifndef _VLC_MODEL_H_
#define _VLC_MODEL_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"
#include "sorting.h"

#include <vlc_input.h>

#include <QModelIndex>
#include <QPixmapCache>
#include <QSize>
#include <QAbstractItemModel>
class QAction;

class VLCModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum {
      IsCurrentRole = Qt::UserRole,
      IsLeafNodeRole,
      IsCurrentsParentNodeRole
    };

    VLCModel( intf_thread_t *_p_intf, QObject *parent = 0 );
    /*** QAbstractItemModel subclassing ***/
    virtual int columnCount( const QModelIndex &parent = QModelIndex() ) const;

    virtual int itemId( const QModelIndex & ) const = 0;
    virtual input_item_t *getInputItem( const QModelIndex & ) const = 0;
    virtual QModelIndex currentIndex() const = 0;
    virtual void doDelete( QModelIndexList ) = 0;
    virtual ~VLCModel();
    static QString getMeta( const QModelIndex & index, int meta );
    static QPixmap getArtPixmap( const QModelIndex & index, const QSize & size );
    static QString getArtUrl( const QModelIndex & index );
    virtual QString getURI( const QModelIndex &index ) const = 0;
    virtual QModelIndex rootIndex() const = 0;
    virtual bool isTree() const = 0;
    virtual bool canEdit() const = 0;
    enum playLocation
    {
        IN_PLAYLIST,
        IN_MEDIALIBRARY
    };
    virtual bool isCurrentItem( const QModelIndex &index, playLocation where ) const = 0;

    struct actionsContainerType
    {
        enum
        {
            ACTION_PLAY = 1,
            ACTION_ADDTOPLAYLIST,
            ACTION_REMOVE,
            ACTION_SORT
        } action;
        QModelIndexList indexes; /* for passing selection or caller index(es) */
        int column; /* for sorting */
    };

    static int columnToMeta( int _column )
    {
        int meta = 1, column = 0;

        while( column != _column && meta != COLUMN_END )
        {
            meta <<= 1;
            column++;
        }

        return meta;
    }

    static int columnFromMeta( int meta_col )
    {
        int meta = 1, column = 0;

        while( meta != meta_col && meta != COLUMN_END )
        {
            meta <<= 1;
            column++;
        }

        return column;
    }

    virtual void createNode( QModelIndex, QString ) {};

public slots:
    virtual void activateItem( const QModelIndex &index ) = 0;
    virtual void actionSlot( QAction *action ) = 0;

protected:
    intf_thread_t *p_intf;
};

Q_DECLARE_METATYPE(VLCModel::actionsContainerType)

#endif
