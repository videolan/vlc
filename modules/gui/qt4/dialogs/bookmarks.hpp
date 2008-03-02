/*****************************************************************************
 * bookmarks.hpp : bookmarks
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@via.ecp.fr>
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


#ifndef _BOOKMARKS_H_
#define _BOOKMARKS_H

#include "util/qvlcframe.hpp"
#include <QStandardItemModel>
#include <QTreeView>
#include <QTreeWidget>

class BookmarksDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static BookmarksDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance )
            instance = new BookmarksDialog( p_intf );
        return instance;
    }
    static void killInstance()
    {
        if( instance ) delete instance;
        instance = NULL;
    }
    virtual ~BookmarksDialog();
private:
    BookmarksDialog( intf_thread_t * );
    static BookmarksDialog *instance;
    void update();
    QTreeWidget *bookmarksList;
private slots:
    void add();
    void del();
    void clear();
    void edit( QTreeWidgetItem *item, int column );
    void extract();
    void activateItem( QModelIndex index );
};

#endif

