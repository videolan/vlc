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
#include <QPushButton>
#include <QHBoxLayout>
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
    view = new QTreeView( 0 );
    view->setModel(model);
    view->header()->resizeSection( 0, 300 );

    connect( view, SIGNAL( activated( const QModelIndex& ) ), model,
             SLOT( activateItem( const QModelIndex& ) ) );

    connect( model,
             SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
             this, SLOT( handleExpansion( const QModelIndex& ) ) );

    model->Rebuild();

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing( 0 ); layout->setMargin( 0 );

    QHBoxLayout *buttons = new QHBoxLayout();
    repeatButton = new QPushButton( this );
    buttons->addWidget( repeatButton );
    randomButton = new QPushButton( this );
    buttons->addWidget( randomButton );

    if( model->hasRandom() ) randomButton->setText( qtr( "Random" ) );
    else randomButton->setText( qtr( "No random" ) );

    if( model->hasRepeat() ) repeatButton->setText( qtr( "Repeat One" ) );
    else if( model->hasLoop() ) repeatButton->setText( qtr( "Repeat All" ) );
    else repeatButton->setText( qtr( "No Repeat" ) );

    connect( repeatButton, SIGNAL( clicked() ), this, SLOT( toggleRepeat() ));
    connect( randomButton, SIGNAL( clicked() ), this, SLOT( toggleRandom() ));

    layout->addWidget( view );
    layout->addLayout( buttons );
    setLayout( layout );
}

void StandardPLPanel::toggleRepeat()
{
    if( model->hasRepeat() )
    {
        model->setRepeat( false ); model->setLoop( true );
        repeatButton->setText( qtr( "Repeat All" ) );
    }
    else if( model->hasLoop() )
    {
        model->setRepeat( false ) ; model->setLoop( false );
        repeatButton->setText( qtr( "No Repeat" ) );
    }
    else
    {
        model->setRepeat( true );
        repeatButton->setText( qtr( "Repeat One" ) );
    }
}

void StandardPLPanel::toggleRandom()
{
    bool prev = model->hasRandom();
    model->setRandom( !prev );
    randomButton->setText( prev ? qtr( "No Random" ) : qtr( "Random" ) );
}

void StandardPLPanel::handleExpansion( const QModelIndex &index )
{
    QModelIndex parent;
    if( model->isCurrent( index ) )
    {
        parent = index;
        while( parent.isValid() )
        {
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
