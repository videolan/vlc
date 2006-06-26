/*****************************************************************************
 * standardpanel.cpp : The "standard" playlist panel : just a treeview
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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

#include "playlist_model.hpp"
#include "components/playlist/panels.hpp"
#include <QTreeView>

StandardPLPanel::StandardPLPanel( QWidget *_parent, intf_thread_t *_p_intf,
                                  playlist_item_t *p_root ):
                                  PLPanel( _parent, _p_intf )
{
   
    PLModel *model = new PLModel( p_root, -1, this );
    QTreeView *view = new QTreeView( this );
    view->setModel(model);
}

StandardPLPanel::~StandardPLPanel()
{}
