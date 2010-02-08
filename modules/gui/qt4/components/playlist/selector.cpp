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
#include "playlist.hpp"
#include "util/customwidgets.hpp"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QMimeData>
#include <QInputDialog>
#include <QMessageBox>

#include <vlc_playlist.h>
#include <vlc_services_discovery.h>

PLSelItem::PLSelItem ( QTreeWidgetItem *i, const QString& text )
    : qitem(i), lblAction( NULL)
{
    layout = new QHBoxLayout();
    layout->setContentsMargins(0,0,0,0);
    layout->addSpacing( 3 );

    lbl = new QLabel( text );

    layout->addWidget(lbl, 1);

    setLayout( layout );

    setMinimumHeight( 22 ); //Action icon height plus 6
}

void PLSelItem::addAction( ItemAction act, const QString& tooltip )
{
    if( lblAction ) return; //might change later

    QIcon icon;

    switch( act )
    {
    case ADD_ACTION:
        icon = QIcon( ":/buttons/playlist/playlist_add" ); break;
    case RM_ACTION:
        icon = QIcon( ":/buttons/playlist/playlist_remove" ); break;
    }

    lblAction = new QVLCFramelessButton();
    lblAction->setIcon( icon );

    if( !tooltip.isEmpty() ) lblAction->setToolTip( tooltip );

    layout->addWidget( lblAction, 0 );
    lblAction->hide();
    layout->addSpacing( 3 );

    CONNECT( lblAction, clicked(), this, triggerAction() );
}

void PLSelItem::showAction() { if( lblAction ) lblAction->show(); }

void PLSelItem::hideAction() { if( lblAction ) lblAction->hide(); }

void PLSelItem::setText( const QString& text ) { lbl->setText( text ); }

void PLSelItem::enterEvent( QEvent *ev ){ showAction(); }

void PLSelItem::leaveEvent( QEvent *ev ){ hideAction(); }

PLSelector::PLSelector( QWidget *p, intf_thread_t *_p_intf )
           : QTreeWidget( p ), p_intf(_p_intf)
{
    setFrameStyle( QFrame::NoFrame );
    viewport()->setAutoFillBackground( false );
    setIconSize( QSize( 24,24 ) );
    setIndentation( 14 );
    header()->hide();
    setRootIsDecorated( true );
    setAlternatingRowColors( false );
    podcastsParent = NULL;
    podcastsParentId = -1;

    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
    invisibleRootItem()->setFlags( invisibleRootItem()->flags() & ~Qt::ItemIsDropEnabled );

    CONNECT( THEMIM, playlistItemAppended( int, int ),
             this, plItemAdded( int, int ) );
    CONNECT( THEMIM, playlistItemRemoved( int ),
             this, plItemRemoved( int ) );
    CONNECT( THEMIM->getIM(), metaChanged( input_item_t *),
            this, inputItemUpdate( input_item_t * ) );

    createItems();
    CONNECT( this, itemActivated( QTreeWidgetItem *, int ),
             this, setSource( QTreeWidgetItem *) );
    CONNECT( this, itemClicked( QTreeWidgetItem *, int ),
             this, setSource( QTreeWidgetItem *) );

    /* I believe this is unnecessary, seeing
       QStyle::SH_ItemView_ActivateItemOnSingleClick
        CONNECT( view, itemClicked( QTreeWidgetItem *, int ),
             this, setSource( QTreeWidgetItem *) ); */

    /* select the first item */
//  view->setCurrentIndex( model->index( 0, 0, QModelIndex() ) );
}

PLSelector::~PLSelector()
{
    if( podcastsParent )
    {
        int c = podcastsParent->childCount();
        for( int i = 0; i < c; i++ )
        {
            QTreeWidgetItem *item = podcastsParent->child(i);
            input_item_t *p_input = item->data( 0, IN_ITEM_ROLE ).value<input_item_t*>();
            vlc_gc_decref( p_input );
        }
    }
}

void PLSelector::setSource( QTreeWidgetItem *item )
{
    if( !item )
        return;

    bool b_ok;
    int i_type = item->data( 0, TYPE_ROLE ).toInt( &b_ok );
    if( !b_ok || i_type == CATEGORY_TYPE )
        return;

    bool sd_loaded;
    if( i_type == SD_TYPE )
    {
        QString qs = item->data( 0, NAME_ROLE ).toString();
        sd_loaded = playlist_IsServicesDiscoveryLoaded( THEPL, qtu( qs ) );
        if( !sd_loaded )
            playlist_ServicesDiscoveryAdd( THEPL, qtu( qs ) );
    }

    playlist_Lock( THEPL );

    playlist_item_t *pl_item = NULL;

    if( i_type == SD_TYPE )
    {
        pl_item = playlist_ChildSearchName( THEPL->p_root, qtu( item->data(0, LONGNAME_ROLE ).toString() ) );
        if( item->data( 0, SPECIAL_ROLE ).toInt() == IS_PODCAST )
        {
            if( pl_item && !sd_loaded )
            {
                podcastsParentId = pl_item->i_id;
                for( int i=0; i < pl_item->i_children; i++ )
                    addPodcastItem( pl_item->pp_children[i] );
            }
            pl_item = NULL; //to prevent activating it
        }
    }
    else
        pl_item = item->data( 0, PL_ITEM_ROLE ).value<playlist_item_t*>();

    playlist_Unlock( THEPL );

    if( pl_item )
       emit activated( pl_item );
}

PLSelItem * PLSelector::addItem (
  SelectorItemType type, const QString& str, bool drop,
  QTreeWidgetItem* parentItem )
{
  QTreeWidgetItem *item = parentItem ?
      new QTreeWidgetItem( parentItem ) : new QTreeWidgetItem( this );
  PLSelItem *selItem = new PLSelItem( item, str );
  setItemWidget( item, 0, selItem );
  item->setData( 0, TYPE_ROLE, (int)type );
  if( !drop ) item->setFlags( item->flags() & ~Qt::ItemIsDropEnabled );

  return selItem;
}

PLSelItem * putSDData( PLSelItem* item, const char* name, const char* longname )
{
    item->treeItem()->setData( 0, NAME_ROLE, qfu( name ) );
    item->treeItem()->setData( 0, LONGNAME_ROLE, qfu( longname ) );
    return item;
}

PLSelItem * putPLData( PLSelItem* item, playlist_item_t* plItem )
{
    item->treeItem()->setData( 0, PL_ITEM_ROLE, QVariant::fromValue( plItem ) );
/*    item->setData( 0, PL_ITEM_ID_ROLE, plItem->i_id );
    item->setData( 0, IN_ITEM_ROLE, QVariant::fromValue( (void*) plItem->p_input ) ); );*/
    return item;
}

PLSelItem *PLSelector::addPodcastItem( playlist_item_t *p_item )
{
  vlc_gc_incref( p_item->p_input );
  char *psz_name = input_item_GetName( p_item->p_input );
  PLSelItem *item = addItem(
      PL_ITEM_TYPE, qfu( psz_name ), false, podcastsParent );
  item->addAction( RM_ACTION, qtr( "Remove this podcast subscription" ) );
  item->treeItem()->setData( 0, PL_ITEM_ROLE, QVariant::fromValue( p_item ) );
  item->treeItem()->setData( 0, PL_ITEM_ID_ROLE, QVariant(p_item->i_id) );
  item->treeItem()->setData( 0, IN_ITEM_ROLE, QVariant::fromValue( p_item->p_input ) );
  CONNECT( item, action( PLSelItem* ), this, podcastRemove( PLSelItem* ) );
  free( psz_name );
  return item;
}

void PLSelector::createItems()
{
    PLSelItem *pl = putPLData( addItem( PL_ITEM_TYPE, qtr( "Playlist" ), true ),
                              THEPL->p_playing );
    pl->treeItem()->setData( 0, SPECIAL_ROLE, QVariant( IS_PL ) );

    PLSelItem *ml = putPLData( addItem( PL_ITEM_TYPE, qtr( "Media Library" ), true ),
                              THEPL->p_media_library );
    ml->treeItem()->setData( 0, SPECIAL_ROLE, QVariant( IS_ML ) );

    QTreeWidgetItem *mfldrs = NULL;

    QTreeWidgetItem *shouts = NULL;

    char **ppsz_longnames;
    char **ppsz_names = vlc_sd_GetNames( THEPL, &ppsz_longnames );
    if( !ppsz_names )
        return;

    char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
    for( ; *ppsz_name; ppsz_name++, ppsz_longname++ )
    {
        //msg_Dbg( p_intf, "Adding a SD item: %s", *ppsz_longname );
#define SD_IS( name ) ( !strcmp( *ppsz_name, name ) )

        if( SD_IS("shoutcast") || SD_IS("shoutcasttv") ||
            SD_IS("frenchtv") || SD_IS("freebox") )
        {
            if( !shouts ) shouts = addItem( CATEGORY_TYPE, qtr( "Shoutcast" ),
                                            false )->treeItem();
            putSDData( addItem( SD_TYPE, *ppsz_longname, false, shouts ),
                       *ppsz_name, *ppsz_longname );
        }
        else if( SD_IS("video_dir") || SD_IS("audio_dir") || SD_IS("picture_dir") )
        {
            if( !mfldrs ) mfldrs = addItem( CATEGORY_TYPE, qtr( "Media Folders" ),
                                            false )->treeItem();
            putSDData( addItem( SD_TYPE, *ppsz_longname, false, mfldrs ),
                       *ppsz_name, *ppsz_longname );
        }
        else if( !strncmp( *ppsz_name, "podcast", 7 ) )
        {

            PLSelItem *podItem = addItem( SD_TYPE, qtr( "Podcasts" ), false );
            putSDData( podItem, *ppsz_name, *ppsz_longname );
            podItem->treeItem()->setData( 0, SPECIAL_ROLE, QVariant( IS_PODCAST ) );
            podItem->addAction( ADD_ACTION, qtr( "Subscribe to a podcast" ) );
            CONNECT( podItem, action( PLSelItem* ), this, podcastAdd( PLSelItem* ) );

            podcastsParent = podItem->treeItem();
        }
        else
        {
            putSDData( addItem( SD_TYPE, qtr( *ppsz_longname ), false ),
                       *ppsz_name, *ppsz_longname );
        }

#undef SD_IS

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
    int i_truth = parent->data( 0, SPECIAL_ROLE ).toInt();

    if( i_truth != IS_PL && i_truth != IS_ML ) return false;
    bool to_pl = ( i_truth == IS_PL );

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

void PLSelector::plItemAdded( int item, int parent )
{
    if( parent != podcastsParentId ) return;

    playlist_Lock( THEPL );

    playlist_item_t *p_item = playlist_ItemGetById( THEPL, item );
    if( !p_item ) {
        playlist_Unlock( THEPL );
        return;
    }

    int c = podcastsParent->childCount();
    for( int i = 0; i < c; i++ )
    {
        QTreeWidgetItem *podItem = podcastsParent->child(i);
        if( podItem->data( 0, PL_ITEM_ID_ROLE ).toInt() == item )
        {
          //msg_Dbg( p_intf, "Podcast already in: (%d) %s", item, p_item->p_input->psz_uri);
          playlist_Unlock( THEPL );
          return;
        }
    }

    //msg_Dbg( p_intf, "Adding podcast: (%d) %s", item, p_item->p_input->psz_uri );
    addPodcastItem( p_item );

    playlist_Unlock( THEPL );

    podcastsParent->setExpanded( true );
}

void PLSelector::plItemRemoved( int id )
{
    int c = podcastsParent->childCount();
    for( int i = 0; i < c; i++ )
    {
        QTreeWidgetItem *item = podcastsParent->child(i);
        if( item->data( 0, PL_ITEM_ID_ROLE ).toInt() == id )
        {
            input_item_t *p_input = item->data( 0, IN_ITEM_ROLE ).value<input_item_t*>();
            //msg_Dbg( p_intf, "Removing podcast: (%d) %s", id, p_input->psz_uri );
            vlc_gc_decref( p_input );
            delete item;
            return;
        }
    }
}

void PLSelector::inputItemUpdate( input_item_t *arg )
{
    int c = podcastsParent->childCount();
    for( int i = 0; i < c; i++ )
    {
        QTreeWidgetItem *item = podcastsParent->child(i);
        input_item_t *p_input = item->data( 0, IN_ITEM_ROLE ).value<input_item_t*>();
        if( p_input == arg )
        {
            PLSelItem *si = itemWidget( item );
            char *psz_name = input_item_GetName( p_input );
            si->setText( qfu( psz_name ) );
            free( psz_name );
            return;
        }
    }
}

void PLSelector::podcastAdd( PLSelItem* item )
{
    bool ok;
    QString url = QInputDialog::getText( this, qtr( "Subscribe" ),
                                         qtr( "Enter URL of the podcast to subscribe to:" ),
                                         QLineEdit::Normal, QString(), &ok );
    if( !ok || url.isEmpty() ) return;

    setSource( podcastsParent ); //to load the SD in case it's not loaded

    vlc_object_t *p_obj = (vlc_object_t*) vlc_object_find_name(
        p_intf->p_libvlc, "podcast", FIND_CHILD );
    if( !p_obj ) return;

    QString request("ADD:");
    request += url;
    var_SetString( p_obj, "podcast-request", qtu( request ) );
    vlc_object_release( p_obj );
}

void PLSelector::podcastRemove( PLSelItem* item )
{
    //FIXME will translators know to leave that %1 somewhere inside?
    QString question ( qtr( "Do you really want to unsubscribe from %1?" ) );
    question = question.arg( item->text() );
    QMessageBox::StandardButton res =
        QMessageBox::question( this, qtr( "Unsubscribe" ), question,
                               QMessageBox::Ok | QMessageBox::Cancel,
                               QMessageBox::Cancel );
    if( res == QMessageBox::Cancel ) return;

    input_item_t *input = item->treeItem()->data( 0, IN_ITEM_ROLE ).value<input_item_t*>();
    if( !input ) return;

    vlc_object_t *p_obj = (vlc_object_t*) vlc_object_find_name(
        p_intf->p_libvlc, "podcast", FIND_CHILD );
    if( !p_obj ) return;

    QString request("RM:");
    char *psz_uri = input_item_GetURI( input );
    request += qfu( psz_uri );
    var_SetString( p_obj, "podcast-request", qtu( request ) );
    vlc_object_release( p_obj );
    free( psz_uri );
}

PLSelItem * PLSelector::itemWidget( QTreeWidgetItem *item )
{
    return ( static_cast<PLSelItem*>( QTreeWidget::itemWidget( item, 0 ) ) );
}

void PLSelector::drawBranches ( QPainter * painter, const QRect & rect, const QModelIndex & index ) const
{
    if( !model()->hasChildren( index ) ) return;
    QStyleOption option;
    option.initFrom( this );
    option.rect = rect;
    /*option.state = QStyle::State_Children;
    if( isExpanded( index ) ) option.state |=  QStyle::State_Open;*/
    style()->drawPrimitive( isExpanded( index ) ?
                            QStyle::PE_IndicatorArrowDown :
                            QStyle::PE_IndicatorArrowRight, &option, painter );
}
