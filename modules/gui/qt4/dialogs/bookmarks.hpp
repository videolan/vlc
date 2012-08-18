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


#ifndef QVLC_BOOKMARKS_H_
#define QVLC_BOOKMARKS_H_ 1

#include "util/qvlcframe.hpp"
#include <QStandardItemModel>
#include <QTreeView>
#include <QTreeWidget>
#include "util/singleton.hpp"
class QPushButton;

class BookmarksDialog : public QVLCFrame, public Singleton<BookmarksDialog>
{
    Q_OBJECT
private:
    BookmarksDialog( intf_thread_t * );
    virtual ~BookmarksDialog();

    QTreeWidget *bookmarksList;
    QPushButton *clearButton;
    QPushButton *delButton;
    bool b_ignore_updates;

private slots:
    void update();
    void add();
    void del();
    void clear();
    void edit( QTreeWidgetItem *item, int column );
    void extract();
    void activateItem( QModelIndex index );
    void updateButtons();

    friend class    Singleton<BookmarksDialog>;
};

#endif

