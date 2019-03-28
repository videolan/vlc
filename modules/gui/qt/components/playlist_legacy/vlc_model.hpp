/*****************************************************************************
 * vlc_model.hpp : base for playlist and ml model
 ****************************************************************************
 * Copyright (C) 2010 the VideoLAN team and AUTHORS
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

#ifndef VLC_QT_VMCMODEL_LEGACY_HPP_
#define VLC_QT_VMCMODEL_LEGACY_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "sorting.h"

#include "playlist_item.hpp"

#include <vlc_input.h>

#include <QModelIndex>
#include <QPixmapCache>
#include <QSize>
#include <QObject>
#include <QAbstractItemModel>
#include <QIcon>

class QAction;

/* Provides non Q_Object interface for Models.
   This allows multiple inheritance on already QAbstractModel based
   Qobjects like Q*ProxyModel.
   Signals being a Q_Object property, they need to be redirected
   using a QObject based class member.
*/
class VLCModelSubInterface
{
public:
    VLCModelSubInterface();
    virtual ~VLCModelSubInterface();
    enum nodeRole
    {
      CURRENT_ITEM_ROLE = Qt::UserRole,
      CURRENT_ITEM_CHILD_ROLE,
      LEAF_NODE_ROLE, /* FIXME: same as index().child() ? */
    };
    virtual void rebuild( playlist_item_t * p = NULL ) = 0;
    virtual void doDelete( QModelIndexList ) = 0;
    virtual void createNode( QModelIndex, QString ) = 0;
    virtual void renameNode( QModelIndex, QString ) = 0;
    virtual void removeAll() = 0;

    virtual QModelIndex rootIndex() const = 0;
    virtual void filter( const QString& search_text, const QModelIndex & root, bool b_recursive ) = 0;
    virtual QModelIndex currentIndex() const = 0;
    virtual QModelIndex indexByPLID( const int i_plid, const int c ) const = 0;
    virtual QModelIndex indexByInputItem( const input_item_t *, const int c ) const = 0;
    virtual int itemId( const QModelIndex & ) const = 0;
    virtual bool isTree() const = 0;
    virtual bool canEdit() const = 0;
    virtual QString getURI( const QModelIndex &index ) const = 0;
    virtual input_item_t *getInputItem( const QModelIndex & ) const = 0;
    virtual QString getTitle( const QModelIndex &index ) const = 0;
    enum actions
    {
        ACTION_PLAY = 1,
        ACTION_PAUSE,
        ACTION_STREAM,
        ACTION_SAVE,
        ACTION_INFO,
        ACTION_ADDTOPLAYLIST,
        ACTION_REMOVE,
        ACTION_SORT,
        ACTION_EXPLORE,
        ACTION_CREATENODE,
        ACTION_RENAMENODE,
        ACTION_CLEAR,
        ACTION_ENQUEUEFILE,
        ACTION_ENQUEUEDIR,
        ACTION_ENQUEUEGENERIC,
        ACTION_SAVETOPLAYLIST
    };
    struct actionsContainerType
    {
        actions action;
        int column; /* for sorting */
        QStringList uris; /* for enqueuing */
        QString options;
    };
    virtual bool action( QAction *, const QModelIndexList & ) = 0;
    virtual bool isSupportedAction( actions action, const QModelIndex & ) const = 0;
    static int columnFromMeta( int meta_col );

    virtual void activateItem( const QModelIndex &index ) = 0;
    virtual void ensureArtRequested( const QModelIndex &index ) = 0;
};

/* Abstract VLC Model ; Base for custom models.
   Only implements methods sharing the same code that would be
   implemented in subclasses.
   Any custom method here must be only used in implemented methods.
*/
class VLCModel : public QAbstractItemModel, public VLCModelSubInterface
{
    Q_OBJECT
public:
    VLCModel( intf_thread_t *_p_intf, QObject *parent = 0 );
    virtual ~VLCModel();

    /*** QAbstractItemModel subclassing ***/
    int columnCount( const QModelIndex &parent = QModelIndex() ) const Q_DECL_OVERRIDE;
    QVariant headerData( int, Qt::Orientation, int ) const Q_DECL_OVERRIDE;

    /*** VLCModelSubInterface subclassing ***/
    int itemId( const QModelIndex & ) const Q_DECL_OVERRIDE;
    QString getURI( const QModelIndex &index ) const Q_DECL_OVERRIDE;
    input_item_t *getInputItem( const QModelIndex & ) const Q_DECL_OVERRIDE;
    QString getTitle( const QModelIndex &index ) const Q_DECL_OVERRIDE;

    /* Custom */
    static int columnToMeta( int _column );
    static int metaToColumn( int meta );
    static QString getMeta( const QModelIndex & index, int meta );
    static QPixmap getArtPixmap( const QModelIndex & index, const QSize & size );

public slots:
    /* slots handlers */
    void ensureArtRequested( const QModelIndex &index ) Q_DECL_OVERRIDE;

signals:
    void currentIndexChanged( const QModelIndex& );
    void rootIndexChanged();

protected:
    /* Custom methods / helpers */
    virtual bool isCurrent( const QModelIndex &index ) const;
    virtual bool isParent( const QModelIndex &index, const QModelIndex &current ) const = 0;
    virtual bool isLeaf( const QModelIndex &index ) const = 0;
    virtual AbstractPLItem *getItem( const QModelIndex & index ) const;

    QIcon icons[ITEM_TYPE_NUMBER];

    intf_thread_t *p_intf;
};

Q_DECLARE_METATYPE(VLCModelSubInterface::actionsContainerType)

#endif
