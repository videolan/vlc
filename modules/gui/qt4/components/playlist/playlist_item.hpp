/*****************************************************************************
 * playlist_item.hpp : Item for a playlist tree
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
    PLItem( playlist_item_t *, PLItem *parent );
    PLItem( playlist_item_t * );
    ~PLItem();

    int row() const;

    void insertChild( PLItem *, int p, bool signal = true );
    void appendChild( PLItem *item, bool signal = true )
    {
        children.insert( children.count(), item );
    };
    void removeChild( PLItem * );
    void removeChildren();
    void takeChildAt( int );

    PLItem *child( int row ) { return children.value( row ); }
    int childCount() const { return children.count(); }

    PLItem *parent() { return parentItem; }
    input_item_t *inputItem() { return p_input; }

protected:
    QList<PLItem*> children;
    int i_id;
    input_item_t *p_input;

private:
    void init( playlist_item_t *, PLItem * );
    PLItem *parentItem;
};

#endif

