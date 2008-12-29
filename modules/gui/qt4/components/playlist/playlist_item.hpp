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

#include "components/playlist/playlist_model.hpp"

#include <QString>
#include <QList>

class QSettings;
class PLModel;

class PLItem
{
    friend class PLModel;
public:
    PLItem( int, int, PLItem *parent , PLModel * );
    PLItem( playlist_item_t *, PLItem *parent, PLModel * );
    PLItem( playlist_item_t *, QSettings *, PLModel * );
    ~PLItem();

    int row() const;

    void insertChild( PLItem *, int p, bool signal = true );
    void appendChild( PLItem *item, bool signal = true )
    {
        insertChild( item, children.count(), signal );
    };

    void remove( PLItem *removed );

    PLItem *child( int row ) { return children.value( row ); };
    int childCount() const { return children.count(); };

    PLItem *parent() { return parentItem; };

    QString columnString( int col ) { return item_col_strings.value( col ); };

    void update( playlist_item_t *, bool );

protected:
    QList<PLItem*> children;
    QList<QString> item_col_strings;
    bool b_current;
    int i_type;
    int i_id;
    int i_input_id;
    int i_showflags;

private:
    void init( int, int, PLItem *, PLModel *, QSettings * );
    void updateColumnHeaders();
    PLItem *parentItem;
    PLModel *model;
};

#endif

