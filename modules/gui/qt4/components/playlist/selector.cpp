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
#include "playlist_item.hpp"
#include "qt4.hpp"
#include "../../dialogs_provider.hpp"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QMimeData>
#include <QPushButton>
#include <QLabel>

#include <vlc_playlist.h>
#include <vlc_services_discovery.h>

PLSelector::PLSelector( QWidget *p, intf_thread_t *_p_intf )
           : QTreeWidget( p ), p_intf(_p_intf)
{
    setItemDelegate( new PLSelectorDelegate() );
    setFrameStyle( QFrame::StyledPanel | QFrame::Plain );
    setIconSize( QSize( 24,24 ) );
    setIndentation( 15 );
    header()->hide();
    setRootIsDecorated( false );
    setAlternatingRowColors( true );

    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
    invisibleRootItem()->setFlags( invisibleRootItem()->flags() & ~Qt::ItemIsDropEnabled );

    createItems();
    CONNECT( this, itemActivated( QTreeWidgetItem *, int ),
             this, setSource( QTreeWidgetItem *) );
    /* I believe this is unnecessary, seeing
       QStyle::SH_ItemView_ActivateItemOnSingleClick
        CONNECT( view, itemClicked( QTreeWidgetItem *, int ),
             this, setSource( QTreeWidgetItem *) ); */

    /* select the first item */
//  view->setCurrentIndex( model->index( 0, 0, QModelIndex() ) );
}

void PLSelector::setSource( QTreeWidgetItem *item )
{
    if( !item )
        return;

    bool b_ok;
    int i_type = item->data( 0, TYPE_ROLE ).toInt( &b_ok );
    if( !b_ok )
        return;

    assert( ( i_type == PL_TYPE || i_type == ML_TYPE || i_type == SD_TYPE ) );
    if( i_type == SD_TYPE )
    {
        QString qs = item->data( 0, NAME_ROLE ).toString();
        if( !playlist_IsServicesDiscoveryLoaded( THEPL, qtu( qs ) ) )
        {
            playlist_ServicesDiscoveryAdd( THEPL, qtu( qs ) );
        }
    }

    if( i_type == SD_TYPE )
        msg_Dbg( p_intf, "SD already loaded, reloading" );

    playlist_Lock( THEPL );
    playlist_item_t *pl_item = NULL;
    if( i_type == SD_TYPE )
       pl_item = playlist_ChildSearchName( THEPL->p_root_category, qtu( item->data(0, LONGNAME_ROLE ).toString() ) );
    else if ( i_type == PL_TYPE )
       pl_item = THEPL->p_local_category;
    else if ( i_type == ML_TYPE )
       pl_item = THEPL->p_ml_category;
    playlist_Unlock( THEPL );

    if( pl_item )
       emit activated( pl_item );
}

void PLSelector::makeStandardItem( QTreeWidgetItem* item, const QString& str )
{
  item->setText( 0,str );
}

void PLSelector::createItems()
{
    QTreeWidgetItem *pl = new QTreeWidgetItem( this );
    makeStandardItem( pl, qtr( "Playlist" ) );
    pl->setData( 0, TYPE_ROLE, PL_TYPE );
    pl->setData( 0, PPL_ITEM_ROLE, QVariant::fromValue( THEPL->p_local_category ) );

    QTreeWidgetItem *lib = new QTreeWidgetItem( this );
    makeStandardItem( lib, qtr( "Media Library" ) );
    lib->setData( 0, TYPE_ROLE, ML_TYPE );
    lib->setData( 0, PPL_ITEM_ROLE, QVariant::fromValue( THEPL->p_ml_category ) );

    QTreeWidgetItem *sds = new QTreeWidgetItem( this );
    makeStandardItem( sds, qtr( "Services" ) );
    sds->setExpanded( true );
    sds->setFlags( sds->flags() & ~Qt::ItemIsDropEnabled );

    QTreeWidgetItem *mfldrs = new QTreeWidgetItem( sds );
    makeStandardItem( mfldrs, qtr( "Media Folders" ) );
    mfldrs->setFlags( mfldrs->flags() & ~Qt::ItemIsDropEnabled );

    QTreeWidgetItem *shouts = new QTreeWidgetItem( sds );
    makeStandardItem( shouts, qtr( "Shoutcast" ) );
    shouts->setFlags( shouts->flags() & ~Qt::ItemIsDropEnabled );

    char **ppsz_longnames;
    char **ppsz_names = vlc_sd_GetNames( &ppsz_longnames );
    if( !ppsz_names )
        return;

    char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
    QTreeWidgetItem *sd_item;
    for( ; *ppsz_name; ppsz_name++, ppsz_longname++ )
    {
        sd_item = new QTreeWidgetItem(  );

#define SD_IS( name ) ( !strcmp( *ppsz_name, name ) )

        if( SD_IS("shoutcast") || SD_IS("shoutcasttv") ||
            SD_IS("frenchtv") || SD_IS("freebox") )
        {
            shouts->addChild( sd_item );
            makeStandardItem( sd_item, qfu(*ppsz_longname) );
        }
        else if( SD_IS("video_dir") || SD_IS("audio_dir") || SD_IS("picture_dir") )
        {
            mfldrs->addChild( sd_item );
            makeStandardItem( sd_item, qfu(*ppsz_longname) );
        }
        else if( SD_IS("podcast") )
        {
            sds->addChild( sd_item );

            QLabel *lbl = new QLabel("Podcasts");
            lbl->setMargin(3);
            QPushButton *btn = new QPushButton();
            btn->setMaximumWidth(30);
            btn->setIcon( QIcon( ":/menu/preferences" ) );
            QHBoxLayout *hbox = new QHBoxLayout();
            hbox->setContentsMargins(0,0,0,0);
            hbox->addWidget(lbl);
            hbox->addWidget(btn);
            QWidget *w = new QWidget();
            w->setLayout(hbox);

            CONNECT( btn, clicked(), THEDP, podcastConfigureDialog() );
            setItemWidget( sd_item, 0, w );
        }
        else
        {
            sds->addChild( sd_item );
            makeStandardItem( sd_item, qfu(*ppsz_longname) );
        }

#undef SD_IS

        sd_item->setData( 0, TYPE_ROLE, SD_TYPE );
        sd_item->setData( 0, NAME_ROLE, qfu( *ppsz_name ) );
        sd_item->setData( 0, LONGNAME_ROLE, qfu( *ppsz_longname ) );
        sd_item->setFlags( sd_item->flags() & ~Qt::ItemIsDropEnabled );
        free( *ppsz_name );
        free( *ppsz_longname );
    }
    free( ppsz_names );
    free( ppsz_longnames );
}

QStringList PLSelector::mimeTypes() const
{
    QStringList types;
    types << "vlc/qt-playlist-item";
    return types;
}

bool PLSelector::dropMimeData ( QTreeWidgetItem * parent, int index,
  const QMimeData * data, Qt::DropAction action )
{
    if( !parent ) return false;

    QVariant type = parent->data( 0, TYPE_ROLE );
    if( type == QVariant() ) return false;
    int i_type = type.toInt();
    if( i_type != PL_TYPE && i_type != ML_TYPE ) return false;
    bool to_pl = i_type == PL_TYPE;

    if( data->hasFormat( "vlc/qt-playlist-item" ) )
    {
        QByteArray encodedData = data->data( "vlc/qt-playlist-item" );
        QDataStream stream( &encodedData, QIODevice::ReadOnly );
        playlist_Lock( THEPL );
        while( !stream.atEnd() )
        {
            PLItem *item;
            stream.readRawData( (char*)&item, sizeof(PLItem*) );
            input_item_t *pl_input =item->inputItem();
            playlist_AddExt ( THEPL,
                pl_input->psz_uri, pl_input->psz_name,
                PLAYLIST_APPEND | PLAYLIST_SPREPARSE, PLAYLIST_END,
                pl_input->i_duration,
                pl_input->i_options, pl_input->ppsz_options, pl_input->optflagc,
                to_pl, true );
        }
        playlist_Unlock( THEPL );
    }
    return true;
}

PLSelector::~PLSelector()
{
}
