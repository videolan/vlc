/*****************************************************************************
 * standardpanel.cpp : The "standard" playlist panel : just a treeview
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
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

#include "playlist_model.hpp"
#include "components/playlist/panels.hpp"
#include <QTreeView>
#include <QVBoxLayout>
#include <QHeaderView>
#include "qt4.hpp"
#include <assert.h>

StandardPLPanel::StandardPLPanel( QWidget *_parent, intf_thread_t *_p_intf,
                                  playlist_t *p_playlist,
                                  playlist_item_t *p_root ):
                                  PLPanel( _parent, _p_intf )
{
    model = new PLModel( p_playlist, p_root, -1, this );
    model->Rebuild();
    view = new QTreeView( 0 );
    view->setModel(model);
    view->header()->resizeSection( 0, 300 );

    connect( view, SIGNAL( activated( const QModelIndex& ) ), model,
             SLOT( activateItem( const QModelIndex& ) ) );

    connect( model,
             SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
             this, SLOT( handleExpansion( const QModelIndex& ) ) );

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing( 0 ); layout->setMargin( 0 );
    layout->addWidget( view );
    setLayout( layout );
}

void StandardPLPanel::handleExpansion( const QModelIndex &index )
{
    fprintf( stderr, "Checking expansion\n" );
    QModelIndex parent;
    if( model->isCurrent( index ) )
    {
        fprintf( stderr, "It is the current one\n" ) ;
        parent = index;
        while( parent.isValid() )
        {
            fprintf( stderr, "Expanding %s\n",
         (model->data( parent, Qt::DisplayRole )).toString().toUtf8().data() );
            view->setExpanded( parent, true );
            parent = model->parent( parent );
        }
    }
}

void StandardPLPanel::setRoot( int i_root_id )
{
    playlist_item_t *p_item = playlist_ItemGetById( THEPL, i_root_id );
    assert( p_item );
    model->rebuildRoot( p_item );
    model->Rebuild();
}

StandardPLPanel::~StandardPLPanel()
{}
