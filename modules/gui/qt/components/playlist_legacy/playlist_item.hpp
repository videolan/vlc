/*****************************************************************************
 * playlist_item.hpp : Item for a playlist tree
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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

#ifndef VLC_QT_PLAYLIST_LEGACY_ITEM_HPP_
#define VLC_QT_PLAYLIST_LEGACY_ITEM_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include <QList>
#include <QString>

class AbstractPLItem
{
    friend class PLItem; /* super ugly glue stuff */
    friend class MLItem;
    friend class VLCModel;
    friend class PLModel;
    friend class MLModel;

public:
    virtual ~AbstractPLItem() {}

protected:
    virtual int id( ) const = 0;
    int childCount() const { return children.count(); }
    int indexOf( AbstractPLItem *item ) const { return children.indexOf( item ); };
    int lastIndexOf( AbstractPLItem *item ) const { return children.lastIndexOf( item ); };
    AbstractPLItem *parent() { return parentItem; }
    virtual input_item_t *inputItem() = 0;
    void insertChild( AbstractPLItem *item, int pos = -1 ) { children.insert( pos, item ); }
    void appendChild( AbstractPLItem *item ) { insertChild( item, children.count() ); } ;
    virtual AbstractPLItem *child( int id ) const = 0;
    void removeChild( AbstractPLItem *item );
    void clearChildren();
    virtual QString getURI() const = 0;
    virtual QString getTitle() const = 0;
    virtual bool readOnly() const = 0;

    QList<AbstractPLItem *> children;
    AbstractPLItem *parentItem;
};

class PLItem : public AbstractPLItem
{
    friend class PLModel;

public:
    virtual ~PLItem();
    bool hasSameParent( PLItem *other ) { return parent() == other->parent(); }
    bool operator< ( AbstractPLItem& );

private:
    /* AbstractPLItem */
    int id() const Q_DECL_OVERRIDE;
    input_item_t *inputItem() Q_DECL_OVERRIDE { return p_input; }
    AbstractPLItem *child( int id ) const Q_DECL_OVERRIDE { return children.value( id ); };
    virtual QString getURI() const Q_DECL_OVERRIDE;
    virtual QString getTitle() const Q_DECL_OVERRIDE;
    virtual bool readOnly() const Q_DECL_OVERRIDE;

    /* Local */
    PLItem( playlist_item_t *, PLItem *parent );
    int row();
    void takeChildAt( int );

    PLItem( playlist_item_t * );
    void init( playlist_item_t *, PLItem * );
    int i_playlist_id;
    int i_flags;
    input_item_t *p_input;
};

#endif

