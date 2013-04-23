/*****************************************************************************
 * playlist_item.hpp : Item for a playlist tree
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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

#ifndef _PLAYLIST_ITEM_H_
#define _PLAYLIST_ITEM_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QList>

class AbstractPLItem
{
    friend class PLItem; /* super ugly glue stuff */
    friend class MLItem;
    friend class PLModel;
    friend class MLModel;

public:
    virtual ~AbstractPLItem() {}

protected:
    virtual int id() const = 0;
    int childCount() const { return children.count(); }
    int indexOf( AbstractPLItem *item ) const { return children.indexOf( item ); };
    int lastIndexOf( AbstractPLItem *item ) const { return children.lastIndexOf( item ); };
    AbstractPLItem *parent() { return parentItem; }
    virtual input_item_t *inputItem() = 0;
    void insertChild( AbstractPLItem *item, int pos = -1 ) { children.insert( pos, item ); }
    void appendChild( AbstractPLItem *item ) { insertChild( item, children.count() ); } ;
    virtual AbstractPLItem *child( int id ) const = 0;
    void clearChildren();

    QList<AbstractPLItem *> children;
    AbstractPLItem *parentItem;
};

class PLItem : public AbstractPLItem
{
    friend class PLModel;

public:
    virtual ~PLItem();
    bool hasSameParent( PLItem *other ) { return parent() == other->parent(); }
    bool operator< ( PLItem& );

private:
    /* AbstractPLItem */
    int id() const { return i_id; };
    input_item_t *inputItem() { return p_input; }
    AbstractPLItem *child( int id ) const { return children.value( id ); };

    /* Local */
    PLItem( playlist_item_t *, PLItem *parent );
    int row();
    void removeChild( PLItem * );
    void takeChildAt( int );

    PLItem( playlist_item_t * );
    void init( playlist_item_t *, PLItem * );
    int i_id;
    input_item_t *p_input;
};

#endif

