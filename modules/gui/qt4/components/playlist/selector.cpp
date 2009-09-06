/*****************************************************************************
 * selector.cpp : Playlist source selector
 ****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include "components/playlist/selector.hpp"
#include "qt4.hpp"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QTreeWidget>

#include <vlc_playlist.h>
#include <vlc_services_discovery.h>

PLSelector::PLSelector( QWidget *p, intf_thread_t *_p_intf )
           : QWidget( p ), p_intf(_p_intf)
{
    view = new QTreeWidget;

    view->setIconSize( QSize( 24,24 ) );
//    view->setAlternatingRowColors( true );
    view->setIndentation( 10 );
    view->header()->hide();
    view->setRootIsDecorated( false );
//    model = new PLModel( THEPL, p_intf, THEPL->p_root_category, 1, this );
//    view->setModel( model );
//    view->setAcceptDrops(true);
//    view->setDropIndicatorShown(true);

    createItems();
    CONNECT( view, itemActivated( QTreeWidgetItem *, int ),
             this, setSource( QTreeWidgetItem *) );
    /* I believe this is unnecessary, seeing
       QStyle::SH_ItemView_ActivateItemOnSingleClick
        CONNECT( view, itemClicked( QTreeWidgetItem *, int ),
             this, setSource( QTreeWidgetItem *) ); */

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setSpacing( 0 ); layout->setMargin( 0 );
    layout->addWidget( view );
    setLayout( layout );

    /* select the first item */
//  view->setCurrentIndex( model->index( 0, 0, QModelIndex() ) );
}

void PLSelector::setSource( QTreeWidgetItem *item )
{
    if( !item )
        return;

    int i_type = item->data( 0, Qt::UserRole ).toInt();

    assert( ( i_type == PL_TYPE || i_type == ML_TYPE || i_type == SD_TYPE ) );
    if( i_type == SD_TYPE )
    {
        QString qs = item->data( 0, Qt::UserRole + 2 ).toString();
        if( !playlist_IsServicesDiscoveryLoaded( THEPL, qtu( qs ) ) )
        {
            playlist_ServicesDiscoveryAdd( THEPL, qtu( qs ) );
#warning FIXME
            playlist_item_t *pl_item =
                    THEPL->p_root_category->pp_children[THEPL->p_root_category->i_children-1];
            item->setData( 0, Qt::UserRole + 1, QVariant::fromValue( pl_item ) );

            emit activated( pl_item );
            return;
        }
    }

    if( i_type == SD_TYPE )
        msg_Dbg( p_intf, "SD already loaded, reloading" );

    playlist_item_t *pl_item =
            item->data( 0, Qt::UserRole + 1 ).value<playlist_item_t *>();
    if( pl_item )
            emit activated( pl_item );
}

void PLSelector::createItems()
{
    assert( view );
    QTreeWidgetItem *pl = new QTreeWidgetItem( view );
    pl->setText( 0, qtr( "Playlist" ) );
    pl->setData( 0, Qt::UserRole, PL_TYPE );
    pl->setData( 0, Qt::UserRole + 1, QVariant::fromValue( THEPL->p_local_category ) );

/*  QTreeWidgetItem *empty = new QTreeWidgetItem( view );
    empty->setFlags(Qt::NoItemFlags); */

    QTreeWidgetItem *lib = new QTreeWidgetItem( view );
    lib->setText( 0, qtr( "Library" ) );
    lib->setData( 0, Qt::UserRole, ML_TYPE );
    lib->setData( 0, Qt::UserRole + 1, QVariant::fromValue( THEPL->p_ml_category ) );

/*  QTreeWidgetItem *empty2 = new QTreeWidgetItem( view );
    empty2->setFlags(Qt::NoItemFlags);*/

    QTreeWidgetItem *sds = new QTreeWidgetItem( view );
    sds->setExpanded( true );
    sds->setText( 0, qtr( "Libraries" ) );

    char **ppsz_longnames;
    char **ppsz_names = vlc_sd_GetNames( &ppsz_longnames );
    if( !ppsz_names )
        return;

    char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
    QTreeWidgetItem *sd_item;
    for( ; *ppsz_name; ppsz_name++, ppsz_longname++ )
    {
        sd_item = new QTreeWidgetItem( QStringList( *ppsz_longname ) );
        sd_item->setData( 0, Qt::UserRole, SD_TYPE );
        sd_item->setData( 0, Qt::UserRole + 2, qfu( *ppsz_name ) );
        sds->addChild( sd_item );
    }
}

PLSelector::~PLSelector()
{
}
