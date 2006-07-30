/*****************************************************************************
 * selector.cpp : Playlist source selector
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: standardpanel.cpp 16024 2006-07-13 13:51:05Z xtophe $
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

#include "components/playlist/selector.hpp"
#include "qt4.hpp"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTreeView>

PLSelector::PLSelector( QWidget *p, intf_thread_t *_p_intf,
                        playlist_t *p_playlist ) : QWidget( p ), p_intf(_p_intf)
{
    model = new PLModel( THEPL, THEPL->p_root_category, 1, this );
    model->Rebuild();
    view = new QTreeView( 0 );
    view->setIndentation( 0 );
    view->header()->hide();
    view->setModel( model );

    connect( view, SIGNAL( activated( const QModelIndex& ) ),
             this, SLOT( setSource( const QModelIndex& ) ) );
    connect( view, SIGNAL( clicked( const QModelIndex& ) ),
             this, SLOT( setSource( const QModelIndex& ) ) );

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing( 0 ); layout->setMargin( 0 );
    layout->addWidget( view );
    setLayout( layout );
}

void PLSelector::setSource( const QModelIndex &index )
{
    if( model )
        emit activated( model->itemId( index ) );
}

PLSelector::~PLSelector()
{
}
