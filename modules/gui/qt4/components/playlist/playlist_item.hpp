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

class PLItem
{
    friend class PLModel;
public:
    ~PLItem();
    bool hasSameParent( PLItem *other ) { return parent() == other->parent(); }
    bool operator< ( PLItem& );
protected:
    PLItem( playlist_item_t *, PLItem *parent );

    int row() const;

    void insertChild( PLItem *, int pos );
    void appendChild( PLItem *item );
    void removeChild( PLItem * );
    void removeChildren();
    void takeChildAt( int );

    PLItem *child( int row ) const { return children.value( row ); }
    int childCount() const { return children.count(); }

    PLItem *parent() { return parentItem; }
    input_item_t *inputItem() const { return p_input; }
    int id() { return i_id; }

    QList<PLItem*> children;
    PLItem *parentItem;
    int i_id;
    input_item_t *p_input;

private:
    PLItem( playlist_item_t * );
    void init( playlist_item_t *, PLItem * );
};

#endif

