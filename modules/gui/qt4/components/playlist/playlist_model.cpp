/*****************************************************************************
 * playlist_model.cpp : Manage playlist model
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Ilkka Ollakkka <ileoo (at) videolan dot org>
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

#include "qt4.hpp"
#include "dialogs_provider.hpp"
#include "components/playlist/playlist_model.hpp"
#include "dialogs/mediainfo.hpp"
#include "dialogs/playlist.hpp"
#include <vlc_intf_strings.h>

#include "pixmaps/types/type_unknown.xpm"

#include <assert.h>
#include <QIcon>
#include <QFont>
#include <QMenu>
#include <QApplication>
#include <QSettings>

#include "sorting.h"

QIcon PLModel::icons[ITEM_TYPE_NUMBER];

static int PlaylistChanged( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void * );
static int PlaylistNext( vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void * );
static int ItemAppended( vlc_object_t *p_this, const char *psz_variable,
                         vlc_value_t oval, vlc_value_t nval, void *param );
static int ItemDeleted( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t oval, vlc_value_t nval, void *param );

/*************************************************************************
 * Playlist model implementation
 *************************************************************************/

/*
  This model is called two times, for the selector and the standard panel
*/
PLModel::PLModel( playlist_t *_p_playlist,  /* THEPL */
                  intf_thread_t *_p_intf,   /* main Qt p_intf */
                  playlist_item_t * p_root,
                  /*playlist_GetPreferredNode( THEPL, THEPL->p_local_category );
                    and THEPL->p_root_category for SelectPL */
                  int _i_depth,             /* -1 for StandPL, 1 for SelectPL */
                  QObject *parent )         /* Basic Qt parent */
                  : QAbstractItemModel( parent )
{
    i_depth = _i_depth;
    assert( i_depth == DEPTH_SEL || i_depth == DEPTH_PL );
    p_intf            = _p_intf;
    p_playlist        = _p_playlist;
    i_cached_id       = -1;
    i_cached_input_id = -1;
    i_popup_item      = i_popup_parent = -1;
    currentItem       = NULL;

    rootItem          = NULL; /* PLItem rootItem, will be set in rebuild( ) */

    if( i_depth == DEPTH_SEL )
        i_showflags = 0;
    else
    {
        i_showflags = getSettings()->value( "qt-pl-showflags", COLUMN_DEFAULT ).toInt();
        if( i_showflags < 1)
            i_showflags = COLUMN_DEFAULT; /* reasonable default to show something */
        else if ( i_showflags >= COLUMN_END )
            i_showflags = COLUMN_END - 1; /* show everything */
    }

    /* Icons initialization */
#define ADD_ICON(type, x) icons[ITEM_TYPE_##type] = QIcon( QPixmap( x ) )
    ADD_ICON( UNKNOWN , type_unknown_xpm );
    ADD_ICON( FILE, ":/type/file" );
    ADD_ICON( DIRECTORY, ":/type/directory" );
    ADD_ICON( DISC, ":/type/disc" );
    ADD_ICON( CDDA, ":/type/cdda" );
    ADD_ICON( CARD, ":/type/capture-card" );
    ADD_ICON( NET, ":/type/net" );
    ADD_ICON( PLAYLIST, ":/type/playlist" );
    ADD_ICON( NODE, ":/type/node" );
#undef ADD_ICON

    rebuild( p_root );
    CONNECT( THEMIM->getIM(), metaChanged( input_item_t *),
            this, ProcessInputItemUpdate( input_item_t *) );
    CONNECT( THEMIM, inputChanged( input_thread_t * ),
            this, ProcessInputItemUpdate( input_thread_t* ) );
    PL_LOCK;
    playlist_item_t *p_item;
    /* Check if there's allready some item playing when playlist
     * model is created, if so, tell model that it's currentone
     */
    if( (p_item = playlist_CurrentPlayingItem(p_playlist)) )
    {
        currentItem = FindByInput( rootItem,
                                           p_item->p_input->i_id );
        emit currentChanged( index( currentItem, 0 ) );
    }
    PL_UNLOCK;
}

PLModel::~PLModel()
{
    if(i_depth == -1)
        getSettings()->setValue( "qt-pl-showflags", i_showflags );
    delCallbacks();
    delete rootItem;
}

Qt::DropActions PLModel::supportedDropActions() const
{
    return Qt::CopyAction; /* Why not Qt::MoveAction */
}

Qt::ItemFlags PLModel::flags( const QModelIndex &index ) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags( index );

    PLItem *item = index.isValid() ?
        static_cast<PLItem*>( index.internalPointer() ) :
        rootItem;

    input_item_t *pl_input = p_playlist->p_local_category->p_input;
    input_item_t *ml_input = p_playlist->p_ml_category->p_input;

    if( rootItem->i_id == p_playlist->p_root_onelevel->i_id
          || rootItem->i_id == p_playlist->p_root_category->i_id )
    {
        if( item->p_input == pl_input
            || item->p_input == ml_input)
                flags |= Qt::ItemIsDropEnabled;
    }
    else
    {
        PL_LOCK;
        playlist_item_t *plItem =
            playlist_ItemGetById( p_playlist, item->i_id );
        if ( plItem && ( plItem->i_children > -1 ) &&
            ( rootItem->p_input == pl_input ||
            rootItem->p_input == ml_input ) )
                flags |= Qt::ItemIsDropEnabled;
        PL_UNLOCK;
        flags |= Qt::ItemIsDragEnabled;
    }

    return flags;
}

/* A list of model indexes are a playlist */
QStringList PLModel::mimeTypes() const
{
    QStringList types;
    types << "vlc/playlist-item-id";
    return types;
}

QMimeData *PLModel::mimeData( const QModelIndexList &indexes ) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;
    QDataStream stream( &encodedData, QIODevice::WriteOnly );
    QModelIndexList list;

    foreach( const QModelIndex &index, indexes ) {
        if( index.isValid() && index.column() == 0 )
            list.append(index);
    }

    qSort(list);

    foreach( const QModelIndex &index, list ) {
        stream << itemId( index );
    }
    mimeData->setData( "vlc/playlist-item-id", encodedData );
    return mimeData;
}

/* Drop operation */
bool PLModel::dropMimeData( const QMimeData *data, Qt::DropAction action,
                           int row, int column, const QModelIndex &parent )
{
    if( data->hasFormat( "vlc/playlist-item-id" ) )
    {
        if( action == Qt::IgnoreAction )
            return true;

        PL_LOCK;

        playlist_item_t *p_parent;

        if( !parent.isValid())
        {
            if( row > -1)
                p_parent = playlist_ItemGetById( p_playlist, rootItem->i_id );
            else
            {
                PL_UNLOCK;
                return true;
            }
        }
        else
            p_parent = playlist_ItemGetById( p_playlist, itemId ( parent ) );

        if( !p_parent || p_parent->i_children == -1 )
        {
            PL_UNLOCK;
            return false;
        }

        bool copy = false;
        if( row == -1 &&
            ( p_parent->p_input == p_playlist->p_local_category->p_input
            || p_parent->p_input == p_playlist->p_ml_category->p_input ) )
                copy = true;

        QByteArray encodedData = data->data( "vlc/playlist-item-id" );
        QDataStream stream( &encodedData, QIODevice::ReadOnly );

        if( copy )
        {
            while( !stream.atEnd() )
            {
                int i_id;
                stream >> i_id;
                playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_id );
                if( !p_item )
                {
                    PL_UNLOCK;
                    return false;
                }
                input_item_t *p_input = p_item->p_input;
                playlist_AddExt ( p_playlist,
                    p_input->psz_uri, p_input->psz_name,
                    PLAYLIST_APPEND | PLAYLIST_SPREPARSE, PLAYLIST_END,
                    p_input->i_duration,
                    p_input->i_options, p_input->ppsz_options, p_input->optflagc,
                    p_parent == p_playlist->p_local_category, true );
            }
        }
        else
        {
            QList<int> ids;
            while( !stream.atEnd() )
            {
                int id;
                stream >> id;
                ids.append(id);
            }
            int count = ids.size();
            playlist_item_t *items[count];
            for( int i = 0; i < count; i++ )
            {
                playlist_item_t *item = playlist_ItemGetById( p_playlist, ids[i] );
                if( !item )
                {
                    PL_UNLOCK;
                    return false;
                }
                items[i] = item;
            }
            playlist_TreeMoveMany( p_playlist, count, items, p_parent,
                (row == -1 ? p_parent->i_children : row) );
        }

        PL_UNLOCK;
        /*TODO: That's not a good idea to rebuild the playlist */
        rebuild();
    }
    return true;
}

/* remove item with its id */
void PLModel::removeItem( int i_id )
{
    PLItem *item = FindById( rootItem, i_id );
    if( currentItem && item && currentItem->p_input == item->p_input ) currentItem = NULL;
    if( item ) item->remove( item, i_depth );
}

/* callbacks and slots */
void PLModel::addCallbacks()
{
    /* Some global changes happened -> Rebuild all */
    var_AddCallback( p_playlist, "intf-change", PlaylistChanged, this );
    /* We went to the next item
    var_AddCallback( p_playlist, "item-current", PlaylistNext, this );
    */
    /* One item has been updated */
    var_AddCallback( p_playlist, "playlist-item-append", ItemAppended, this );
    var_AddCallback( p_playlist, "playlist-item-deleted", ItemDeleted, this );
}

void PLModel::delCallbacks()
{
    /*
    var_DelCallback( p_playlist, "item-current", PlaylistNext, this );
    */
    var_DelCallback( p_playlist, "intf-change", PlaylistChanged, this );
    var_DelCallback( p_playlist, "playlist-item-append", ItemAppended, this );
    var_DelCallback( p_playlist, "playlist-item-deleted", ItemDeleted, this );
}

void PLModel::activateItem( const QModelIndex &index )
{
    assert( index.isValid() );
    PLItem *item = static_cast<PLItem*>(index.internalPointer());
    assert( item );
    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item->i_id );
    activateItem( p_item );
    PL_UNLOCK;
}

/* Must be entered with lock */
void PLModel::activateItem( playlist_item_t *p_item )
{
    if( !p_item ) return;
    playlist_item_t *p_parent = p_item;
    while( p_parent )
    {
        if( p_parent->i_id == rootItem->i_id ) break;
        p_parent = p_parent->p_parent;
    }
    if( p_parent )
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, pl_Locked,
                          p_parent, p_item );
}

/****************** Base model mandatory implementations *****************/
QVariant PLModel::data( const QModelIndex &index, int role ) const
{
    if( !index.isValid() ) return QVariant();
    PLItem *item = static_cast<PLItem*>(index.internalPointer());
    if( role == Qt::DisplayRole )
    {
        int running_index = -1;
        int columncount = 0;
        int metadata = 1;

        if( i_depth == DEPTH_SEL )
        {
            vlc_mutex_lock( &item->p_input->lock );
            QString returninfo = QString( qfu( item->p_input->psz_name ) );
            vlc_mutex_unlock( &item->p_input->lock );
            return QVariant(returninfo);
        }

        while( metadata < COLUMN_END )
        {
            if( i_showflags & metadata )
                running_index++;
            if( running_index == index.column() )
                break;
            metadata <<= 1;
        }

        if( running_index != index.column() ) return QVariant();

        QString returninfo;
        if( metadata == COLUMN_NUMBER )
            returninfo = QString::number( index.row() + 1 );
        else
        {
            char *psz = psz_column_meta( item->p_input, metadata );
            returninfo = QString( qfu( psz ) );
            free( psz );
        }
        return QVariant( returninfo );
    }
    else if( role == Qt::DecorationRole && index.column() == 0  )
    {
        /* Use to segfault here because i_type wasn't always initialized */
        if( item->p_input->i_type >= 0 )
            return QVariant( PLModel::icons[item->p_input->i_type] );
    }
    else if( role == Qt::FontRole )
    {
        if( isCurrent( index ) )
        {
            QFont f; f.setBold( true ); return QVariant( f );
        }
    }
    return QVariant();
}

bool PLModel::isCurrent( const QModelIndex &index ) const
{
    assert( index.isValid() );
    if( !currentItem ) return false;
    return static_cast<PLItem*>(index.internalPointer())->p_input == currentItem->p_input;
}

int PLModel::itemId( const QModelIndex &index ) const
{
    assert( index.isValid() );
    return static_cast<PLItem*>(index.internalPointer())->i_id;
}

QVariant PLModel::headerData( int section, Qt::Orientation orientation,
                              int role ) const
{
    int metadata=1;
    int running_index=-1;
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    if( i_depth == DEPTH_SEL ) return QVariant( QString("") );

    while( metadata < COLUMN_END )
    {
        if( metadata & i_showflags )
            running_index++;
        if( running_index == section )
            break;
        metadata <<= 1;
    }

    if( running_index != section ) return QVariant();

    return QVariant( qfu( psz_column_title( metadata ) ) );
}

QModelIndex PLModel::index( int row, int column, const QModelIndex &parent )
                  const
{
    PLItem *parentItem;
    if( !parent.isValid() )
        parentItem = rootItem;
    else
        parentItem = static_cast<PLItem*>(parent.internalPointer());

    PLItem *childItem = parentItem->child( row );
    if( childItem )
        return createIndex( row, column, childItem );
    else
        return QModelIndex();
}

/* Return the index of a given item */
QModelIndex PLModel::index( PLItem *item, int column ) const
{
    if( !item ) return QModelIndex();
    const PLItem *parent = item->parent();
    if( parent )
        return createIndex( parent->children.lastIndexOf( item ),
                            column, item );
    return QModelIndex();
}

QModelIndex PLModel::parent( const QModelIndex &index ) const
{
    if( !index.isValid() ) return QModelIndex();

    PLItem *childItem = static_cast<PLItem*>(index.internalPointer());
    if( !childItem )
    {
        msg_Err( p_playlist, "NULL CHILD" );
        return QModelIndex();
    }

    PLItem *parentItem = childItem->parent();
    if( !parentItem || parentItem == rootItem ) return QModelIndex();
    if( !parentItem->parentItem )
    {
        msg_Err( p_playlist, "No parent parent, trying row 0 " );
        msg_Err( p_playlist, "----- PLEASE REPORT THIS ------" );
        return createIndex( 0, 0, parentItem );
    }
    QModelIndex ind = createIndex(parentItem->row(), 0, parentItem);
    return ind;
}

int PLModel::columnCount( const QModelIndex &i) const
{
    int columnCount=0;
    int metadata=1;
    if( i_depth == DEPTH_SEL ) return 1;

    while( metadata < COLUMN_END )
    {
        if( metadata & i_showflags )
            columnCount++;
        metadata <<= 1;
    }
    return columnCount;
}

int PLModel::childrenCount( const QModelIndex &parent ) const
{
    return rowCount( parent );
}

int PLModel::rowCount( const QModelIndex &parent ) const
{
    PLItem *parentItem;

    if( !parent.isValid() )
        parentItem = rootItem;
    else
        parentItem = static_cast<PLItem*>(parent.internalPointer());

    return parentItem->childCount();
}

QStringList PLModel::selectedURIs()
{
    QStringList lst;
    for( int i = 0; i < current_selection.size(); i++ )
    {
        PLItem *item = static_cast<PLItem*>
                    (current_selection[i].internalPointer());
        if( item )
        {
            PL_LOCK;
            playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item->i_id );
            if( p_item )
            {
                char *psz = input_item_GetURI( p_item->p_input );
                if( psz )
                {
                    lst.append( psz );
                    free( psz );
                }
            }
            PL_UNLOCK;
        }
    }
    return lst;
}

/************************* General playlist status ***********************/

bool PLModel::hasRandom()
{
    return var_GetBool( p_playlist, "random" );
}
bool PLModel::hasRepeat()
{
    return var_GetBool( p_playlist, "repeat" );
}
bool PLModel::hasLoop()
{
    return var_GetBool( p_playlist, "loop" );
}
void PLModel::setLoop( bool on )
{
    var_SetBool( p_playlist, "loop", on ? true:false );
    config_PutInt( p_playlist, "loop", on ? 1: 0 );
}
void PLModel::setRepeat( bool on )
{
    var_SetBool( p_playlist, "repeat", on ? true:false );
    config_PutInt( p_playlist, "repeat", on ? 1: 0 );
}
void PLModel::setRandom( bool on )
{
    var_SetBool( p_playlist, "random", on ? true:false );
    config_PutInt( p_playlist, "random", on ? 1: 0 );
}

/************************* Lookups *****************************/

PLItem *PLModel::FindById( PLItem *root, int i_id )
{
    return FindInner( root, i_id, false );
}

PLItem *PLModel::FindByInput( PLItem *root, int i_id )
{
    PLItem *result = FindInner( root, i_id, true );
    return result;
}

#define CACHE( i, p ) { i_cached_id = i; p_cached_item = p; }
#define ICACHE( i, p ) { i_cached_input_id = i; p_cached_item_bi = p; }

PLItem * PLModel::FindInner( PLItem *root, int i_id, bool b_input )
{
    if( ( !b_input && i_cached_id == i_id) ||
        ( b_input && i_cached_input_id ==i_id ) )
    {
        return b_input ? p_cached_item_bi : p_cached_item;
    }

    if( !b_input && root->i_id == i_id )
    {
        CACHE( i_id, root );
        return root;
    }
    else if( b_input && root->p_input->i_id == i_id )
    {
        ICACHE( i_id, root );
        return root;
    }

    QList<PLItem *>::iterator it = root->children.begin();
    while ( it != root->children.end() )
    {
        if( !b_input && (*it)->i_id == i_id )
        {
            CACHE( i_id, (*it) );
            return p_cached_item;
        }
        else if( b_input && (*it)->p_input->i_id == i_id )
        {
            ICACHE( i_id, (*it) );
            return p_cached_item_bi;
        }
        if( (*it)->children.size() )
        {
            PLItem *childFound = FindInner( (*it), i_id, b_input );
            if( childFound )
            {
                if( b_input )
                    ICACHE( i_id, childFound )
                else
                    CACHE( i_id, childFound )
                return childFound;
            }
        }
        it++;
    }
    return NULL;
}
#undef CACHE
#undef ICACHE


/************************* Updates handling *****************************/
void PLModel::customEvent( QEvent *event )
{
    int type = event->type();
    if( type != ItemAppend_Type &&
        type != ItemDelete_Type && type != PLUpdate_Type )
        return;

    PLEvent *ple = static_cast<PLEvent *>(event);

    if( type == ItemAppend_Type )
        ProcessItemAppend( &ple->add );
    else if( type == ItemDelete_Type )
        ProcessItemRemoval( ple->i_id );
    else
        rebuild();
}

/**** Events processing ****/
void PLModel::ProcessInputItemUpdate( input_thread_t *p_input )
{
    if( !p_input ) return;
    ProcessInputItemUpdate( input_GetItem( p_input ) );
    if( p_input && !( p_input->b_dead || !vlc_object_alive( p_input ) ) )
    {
        PLItem *item = FindByInput( rootItem, input_GetItem( p_input )->i_id );
        currentItem = item;
        emit currentChanged( index( item, 0 ) );
    }
    else
    {
        currentItem = NULL;
    }
}
void PLModel::ProcessInputItemUpdate( input_item_t *p_item )
{
    if( !p_item ||  p_item->i_id <= 0 ) return;
    PLItem *item = FindByInput( rootItem, p_item->i_id );
    if( item )
        UpdateTreeItem( item, true, true);
}

void PLModel::ProcessItemRemoval( int i_id )
{
    if( i_id <= 0 ) return;
    if( i_id == i_cached_id ) i_cached_id = -1;
    i_cached_input_id = -1;

    removeItem( i_id );
}

void PLModel::ProcessItemAppend( const playlist_add_t *p_add )
{
    playlist_item_t *p_item = NULL;
    PLItem *newItem = NULL;

    PLItem *nodeItem = FindById( rootItem, p_add->i_node );
    if( !nodeItem ) return;

    PL_LOCK;
    p_item = playlist_ItemGetById( p_playlist, p_add->i_item );
    if( !p_item || p_item->i_flags & PLAYLIST_DBL_FLAG ) goto end;
    if( i_depth == DEPTH_SEL && p_item->p_parent &&
                        p_item->p_parent->i_id != rootItem->i_id )
        goto end;

    newItem = new PLItem( p_item, nodeItem );
    PL_UNLOCK;

    emit layoutAboutToBeChanged();
    emit beginInsertRows( index( newItem, 0 ), nodeItem->childCount(), nodeItem->childCount()+1 );
    nodeItem->appendChild( newItem );
    emit endInsertRows();
    emit layoutChanged();
    UpdateTreeItem( newItem, true );
    return;
end:
    PL_UNLOCK;
    return;
}


void PLModel::rebuild()
{
    rebuild( NULL );
}

void PLModel::rebuild( playlist_item_t *p_root )
{
    playlist_item_t* p_item;
    /* Remove callbacks before locking to avoid deadlocks */
    delCallbacks();
    /* Invalidate cache */
    i_cached_id = i_cached_input_id = -1;

    emit layoutAboutToBeChanged();

    /* Clear the tree */
    if( rootItem )
    {
        if( rootItem->children.size() )
        {
            emit beginRemoveRows( index( rootItem, 0 ), 0,
                    rootItem->children.size() -1 );
            qDeleteAll( rootItem->children );
            rootItem->children.clear();
            emit endRemoveRows();
        }
    }
    PL_LOCK;
    if( p_root )
    {
        delete rootItem;
        rootItem = new PLItem( p_root );
    }
    assert( rootItem );
    /* Recreate from root */
    UpdateNodeChildren( rootItem );
    if( (p_item = playlist_CurrentPlayingItem(p_playlist)) )
    {
        currentItem = FindByInput( rootItem,
                                           p_item->p_input->i_id );
        if( currentItem )
        {
            UpdateTreeItem( currentItem, true, false );
        }
    }
    else
    {
        currentItem = NULL;
    }
    PL_UNLOCK;

    /* And signal the view */
    emit layoutChanged();
    addCallbacks();
}

/* This function must be entered WITH the playlist lock */
void PLModel::UpdateNodeChildren( PLItem *root )
{
    emit layoutAboutToBeChanged();
    playlist_item_t *p_node = playlist_ItemGetById( p_playlist, root->i_id );
    UpdateNodeChildren( p_node, root );
    emit layoutChanged();
}

/* This function must be entered WITH the playlist lock */
void PLModel::UpdateNodeChildren( playlist_item_t *p_node, PLItem *root )
{
    for( int i = 0; i < p_node->i_children ; i++ )
    {
        if( p_node->pp_children[i]->i_flags & PLAYLIST_DBL_FLAG ) continue;
        PLItem *newItem =  new PLItem( p_node->pp_children[i], root );
        emit beginInsertRows( index( newItem, 0 ), root->childCount(), root->childCount()+1 );
        root->appendChild( newItem );
        emit endInsertRows();
        UpdateTreeItem( newItem, true, true );
        if( i_depth == DEPTH_PL && p_node->pp_children[i]->i_children != -1 )
            UpdateNodeChildren( p_node->pp_children[i], newItem );
    }
}

/* Function doesn't need playlist-lock, as we don't touch playlist_item_t stuff here*/
void PLModel::UpdateTreeItem( PLItem *item, bool signal, bool force )
{
    if ( !item || !item->p_input )
        return;
    if( !force && i_depth == DEPTH_SEL && item->parentItem &&
                                 item->parentItem->p_input != rootItem->p_input )
        return;
    if( signal )
        emit dataChanged( index( item, 0 ) , index( item, columnCount( QModelIndex() ) ) );
}

/************************* Actions ******************************/

/**
 * Deletion, here we have to do a ugly slow hack as we retrieve the full
 * list of indexes to delete at once: when we delete a node and all of
 * its children, we need to update the list.
 * Todo: investigate whethere we can use ranges to be sure to delete all items?
 */
void PLModel::doDelete( QModelIndexList selected )
{
    for( int i = selected.size() -1 ; i >= 0; i-- )
    {
        QModelIndex index = selected[i];
        if( index.column() != 0 ) continue;
        PLItem *item = static_cast<PLItem*>(index.internalPointer());
        if( item )
        {
            if( item->children.size() )
                recurseDelete( item->children, &selected );
            doDeleteItem( item, &selected );
        }
        if( i > selected.size() ) i = selected.size();
    }
}

void PLModel::recurseDelete( QList<PLItem*> children, QModelIndexList *fullList )
{
    for( int i = children.size() - 1; i >= 0 ; i-- )
    {
        PLItem *item = children[i];
        if( item->children.size() )
            recurseDelete( item->children, fullList );
        doDeleteItem( item, fullList );
    }
}

void PLModel::doDeleteItem( PLItem *item, QModelIndexList *fullList )
{
    QModelIndex deleteIndex = index( item, 0 );
    fullList->removeAll( deleteIndex );

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item->i_id );
    if( !p_item )
    {
        PL_UNLOCK;
        return;
    }
    if( p_item->i_children == -1 )
        playlist_DeleteFromInput( p_playlist, p_item->p_input, pl_Locked );
    else
        playlist_NodeDelete( p_playlist, p_item, true, false );
    PL_UNLOCK;
    /* And finally, remove it from the tree */
    emit beginRemoveRows( index( item->parentItem, 0), item->parentItem->children.indexOf( item ),
            item->parentItem->children.indexOf( item )+1 );
    item->remove( item, i_depth );
    emit endRemoveRows();
}

/******* Volume III: Sorting and searching ********/
void PLModel::sort( int column, Qt::SortOrder order )
{
    int i_index = -1;
    int i_flag = 0;

    int i_column = 1;
    for( i_column = 1; i_column != COLUMN_END; i_column<<=1 )
    {
        if( ( shownFlags() & i_column ) )
            i_index++;
        if( column == i_index )
        {
            i_flag = i_column;
            goto next;
        }
    }


next:
    PL_LOCK;
    {
        playlist_item_t *p_root = playlist_ItemGetById( p_playlist,
                                                        rootItem->i_id );
        if( p_root && i_flag )
        {
            playlist_RecursiveNodeSort( p_playlist, p_root,
                                        i_column_sorting( i_flag ),
                                        order == Qt::AscendingOrder ?
                                            ORDER_NORMAL : ORDER_REVERSE );
        }
    }
    PL_UNLOCK;
    rebuild();
}

void PLModel::search( const QString& search_text )
{
    /** \todo Fire the search with a small delay ? */
    PL_LOCK;
    {
        playlist_item_t *p_root = playlist_ItemGetById( p_playlist,
                                                        rootItem->i_id );
        assert( p_root );
        const char *psz_name = search_text.toUtf8().data();
        playlist_LiveSearchUpdate( p_playlist , p_root, psz_name );
    }
    PL_UNLOCK;
    rebuild();
}

/*********** Popup *********/
void PLModel::popup( QModelIndex & index, QPoint &point, QModelIndexList list )
{
    PL_LOCK;
    int i_id;
    if( index.isValid() ) i_id = itemId( index );
    else i_id = rootItem->i_id;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_id );
    if( p_item )
    {
        i_popup_item = p_item->i_id;
        i_popup_parent = p_item->p_parent ? p_item->p_parent->i_id : -1;
        bool node = p_item->i_children > -1;
        bool tree = false;
        if( node )
        {
            /* check whether we are in tree view */
            playlist_item_t *p_up = p_item;
            while( p_up )
            {
                if ( p_up == p_playlist->p_root_category ) tree = true;
                p_up = p_up->p_parent;
            }
        }
        PL_UNLOCK;

        current_selection = list;
        QMenu *menu = new QMenu;
        if( index.isValid() )
        {
            menu->addAction( qtr(I_POP_PLAY), this, SLOT( popupPlay() ) );
            menu->addAction( qtr(I_POP_DEL), this, SLOT( popupDel() ) );
            menu->addSeparator();
            menu->addAction( qtr(I_POP_STREAM), this, SLOT( popupStream() ) );
            menu->addAction( qtr(I_POP_SAVE), this, SLOT( popupSave() ) );
            menu->addSeparator();
            menu->addAction( qtr(I_POP_INFO), this, SLOT( popupInfo() ) );
        }
        if( node )
        {
            if( index.isValid() ) menu->addSeparator();
            menu->addAction( qtr(I_POP_SORT), this, SLOT( popupSort() ) );
            if( tree )
                menu->addAction( qtr(I_POP_ADD), this, SLOT( popupAddNode() ) );
        }
        if( index.isValid() )
        {
            menu->addSeparator();
            menu->addAction( qtr( I_POP_EXPLORE ), this, SLOT( popupExplore() ) );
        }
        menu->popup( point );
    }
    else
        PL_UNLOCK;
}


void PLModel::viewchanged( int meta )
{
    assert( meta );
    int _meta = meta;
    if( rootItem )
    {
        int index=-1;
        while( _meta )
        {
            index++;
            _meta >>= 1;
        }

        /* UNUSED        emit layoutAboutToBeChanged(); */
        index = __MIN( index, columnCount() );
        QModelIndex parent = createIndex( 0, 0, rootItem );

        if( i_showflags & meta )
            /* Removing columns */
        {
            emit beginRemoveColumns( parent, index, index+1 );
            i_showflags &= ~( meta );
            getSettings()->setValue( "qt-pl-showflags", i_showflags );
            emit endRemoveColumns();
        }
        else
        {
            /* Adding columns */
            emit beginInsertColumns( parent, index, index+1 );
            i_showflags |= meta;
            getSettings()->setValue( "qt-pl-showflags", i_showflags );
            emit endInsertColumns();
        }
        emit columnsChanged( meta );
        rebuild();
    }
}

void PLModel::popupDel()
{
    doDelete( current_selection );
}

void PLModel::popupPlay()
{
    PL_LOCK;
    {
        playlist_item_t *p_item = playlist_ItemGetById( p_playlist,
                                                        i_popup_item );
        activateItem( p_item );
    }
    PL_UNLOCK;
}

void PLModel::popupInfo()
{
    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist,
                                                    i_popup_item );
    if( p_item )
    {
        input_item_t* p_input = p_item->p_input;
        vlc_gc_incref( p_input );
        PL_UNLOCK;
        MediaInfoDialog *mid = new MediaInfoDialog( p_intf, p_input );
        vlc_gc_decref( p_input );
        mid->setParent( PlaylistDialog::getInstance( p_intf ),
                        Qt::Dialog );
        mid->show();
    } else
        PL_UNLOCK;
}

void PLModel::popupStream()
{
    QStringList mrls = selectedURIs();
    if( !mrls.isEmpty() )
        THEDP->streamingDialog( NULL, mrls[0], false );

}

void PLModel::popupSave()
{
    QStringList mrls = selectedURIs();
    if( !mrls.isEmpty() )
        THEDP->streamingDialog( NULL, mrls[0] );
}

#include <QUrl>
#include <QFileInfo>
#include <QDesktopServices>
void PLModel::popupExplore()
{
    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist,
                                                    i_popup_item );
    if( p_item )
    {
       input_item_t *p_input = p_item->p_input;
       char *psz_meta = input_item_GetURI( p_input );
       PL_UNLOCK;
       if( psz_meta )
       {
           const char *psz_access;
           const char *psz_demux;
           char  *psz_path;
           input_SplitMRL( &psz_access, &psz_demux, &psz_path, psz_meta );

           if( EMPTY_STR( psz_access ) ||
               !strncasecmp( psz_access, "file", 4 ) ||
               !strncasecmp( psz_access, "dire", 4 ) )
           {
               QFileInfo info( qfu( psz_meta ) );
               QDesktopServices::openUrl(
                               QUrl::fromLocalFile( info.absolutePath() ) );
           }
           free( psz_meta );
       }
    }
    else
        PL_UNLOCK;
}

#include <QInputDialog>
void PLModel::popupAddNode()
{
    bool ok;
    QString name = QInputDialog::getText( PlaylistDialog::getInstance( p_intf ),
        qtr( I_POP_ADD ), qtr( "Enter name for new node:" ),
        QLineEdit::Normal, QString(), &ok);
    if( !ok || name.isEmpty() ) return;
    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist,
                                                    i_popup_item );
    if( p_item )
    {
        playlist_NodeCreate( p_playlist, qtu( name ), p_item, 0, NULL );
    }
    PL_UNLOCK;
}
/**********************************************************************
 * Playlist callbacks
 **********************************************************************/
static int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                            vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    PLEvent *event = new PLEvent( PLUpdate_Type, 0 );
    QApplication::postEvent( p_model, event );
    return VLC_SUCCESS;
}

static int PlaylistNext( vlc_object_t *p_this, const char *psz_variable,
                         vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    PLEvent *event = new PLEvent( ItemUpdate_Type, oval.i_int );
    QApplication::postEvent( p_model, event );
    event = new PLEvent( ItemUpdate_Type, nval.i_int );
    QApplication::postEvent( p_model, event );
    return VLC_SUCCESS;
}

static int ItemDeleted( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    PLEvent *event = new PLEvent( ItemDelete_Type, nval.i_int );
    QApplication::postEvent( p_model, event );
    return VLC_SUCCESS;
}

static int ItemAppended( vlc_object_t *p_this, const char *psz_variable,
                         vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    const playlist_add_t *p_add = (playlist_add_t *)nval.p_address;
    PLEvent *event = new PLEvent( p_add );
    QApplication::postEvent( p_model, event );
    return VLC_SUCCESS;
}

