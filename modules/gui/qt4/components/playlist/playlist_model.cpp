/*****************************************************************************
 * playlist_model.cpp : Manage playlist model
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Ilkka Ollakkka <ileoo (at) videolan dot org>
 *          Jakob Leben <jleben@videolan.org>
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
                  QObject *parent )         /* Basic Qt parent */
                  : QAbstractItemModel( parent )
{
    p_intf            = _p_intf;
    p_playlist        = _p_playlist;
    i_cached_id       = -1;
    i_cached_input_id = -1;
    i_popup_item      = i_popup_parent = -1;
    currentItem       = NULL;

    rootItem          = NULL; /* PLItem rootItem, will be set in rebuild( ) */

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

    rebuild( p_root, true );
    CONNECT( THEMIM->getIM(), metaChanged( input_item_t *),
            this, processInputItemUpdate( input_item_t *) );
    CONNECT( THEMIM, inputChanged( input_thread_t * ),
            this, processInputItemUpdate( input_thread_t* ) );
}

PLModel::~PLModel()
{
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

    PLItem *item = index.isValid() ? getItem( index ) : rootItem;

    input_item_t *pl_input =
        p_playlist->p_local_category ?
        p_playlist->p_local_category->p_input : NULL;
    input_item_t *ml_input =
        p_playlist->p_ml_category ?
        p_playlist->p_ml_category->p_input : NULL;

    if( ( pl_input && rootItem->p_input == pl_input ) ||
              ( ml_input && rootItem->p_input == ml_input ) )
    {
        PL_LOCK;
        playlist_item_t *plItem =
            playlist_ItemGetById( p_playlist, item->i_id );

        if ( plItem && ( plItem->i_children > -1 ) )
            flags |= Qt::ItemIsDropEnabled;

        PL_UNLOCK;

    }
    flags |= Qt::ItemIsDragEnabled;

    return flags;
}

QStringList PLModel::mimeTypes() const
{
    QStringList types;
    types << "vlc/qt-playlist-item";
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
        PLItem *item = getItem( index );
        stream.writeRawData( (char*) &item, sizeof( PLItem* ) );
    }
    mimeData->setData( "vlc/qt-playlist-item", encodedData );
    return mimeData;
}

/* Drop operation */
bool PLModel::dropMimeData( const QMimeData *data, Qt::DropAction action,
                           int row, int column, const QModelIndex &parent )
{
    if( data->hasFormat( "vlc/qt-playlist-item" ) )
    {
        if( action == Qt::IgnoreAction )
            return true;

        PLItem *parentItem = parent.isValid() ? getItem( parent ) : rootItem;

        PL_LOCK;
        playlist_item_t *p_parent =
            playlist_ItemGetById( p_playlist, parentItem->i_id );
        if( !p_parent || p_parent->i_children == -1 )
        {
            PL_UNLOCK;
            return false;
        }

        bool copy = false;
        playlist_item_t *p_pl = p_playlist->p_local_category;
        playlist_item_t *p_ml = p_playlist->p_ml_category;
        if
        (
            row == -1 && (
            ( p_pl && p_parent->p_input == p_pl->p_input ) ||
            ( p_ml && p_parent->p_input == p_ml->p_input ) )
        )
            copy = true;
        PL_UNLOCK;

        QByteArray encodedData = data->data( "vlc/qt-playlist-item" );
        if( copy )
            dropAppendCopy( encodedData, parentItem );
        else
            dropMove( encodedData, parentItem, row );
    }
    return true;
}

void PLModel::dropAppendCopy( QByteArray& data, PLItem *target )
{
    QDataStream stream( &data, QIODevice::ReadOnly );

    PL_LOCK;
    playlist_item_t *p_parent =
            playlist_ItemGetById( p_playlist, target->i_id );
    while( !stream.atEnd() )
    {
        PLItem *item;
        stream.readRawData( (char*)&item, sizeof(PLItem*) );
        playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item->i_id );
        if( !p_item ) continue;
        input_item_t *p_input = p_item->p_input;
        playlist_AddExt ( p_playlist,
            p_input->psz_uri, p_input->psz_name,
            PLAYLIST_APPEND | PLAYLIST_SPREPARSE, PLAYLIST_END,
            p_input->i_duration,
            p_input->i_options, p_input->ppsz_options, p_input->optflagc,
            p_parent == p_playlist->p_local_category, true );
    }
    PL_UNLOCK;
}

void PLModel::dropMove( QByteArray& data, PLItem *target, int row )
{
    QDataStream stream( &data, QIODevice::ReadOnly );
    QList<PLItem*> model_items;
    QList<int> ids;
    int new_pos = row == -1 ? target->children.size() : row;
    int model_pos = new_pos;
    while( !stream.atEnd() )
    {
        PLItem *item;
        stream.readRawData( (char*)&item, sizeof(PLItem*) );

        /* better not try to move a node into itself: */
        PLItem *climber = target;
        while( climber )
        {
            if( climber == item ) break;
            climber = climber->parentItem;
        }
        if( climber ) continue;

        if( item->parentItem == target &&
            target->children.indexOf( item ) < model_pos )
                model_pos--;

        ids.append( item->i_id );
        model_items.append( item );

        takeItem( item );
    }
    int count = ids.size();
    if( count )
    {
        playlist_item_t *pp_items[count];

        PL_LOCK;
        for( int i = 0; i < count; i++ )
        {
            playlist_item_t *p_item = playlist_ItemGetById( p_playlist, ids[i] );
            if( !p_item )
            {
                PL_UNLOCK;
                return;
            }
            pp_items[i] = p_item;
        }
        playlist_item_t *p_parent =
            playlist_ItemGetById( p_playlist, target->i_id );
        playlist_TreeMoveMany( p_playlist, count, pp_items, p_parent,
            new_pos );
        PL_UNLOCK;

        insertChildren( target, model_items, model_pos );
    }
}

/* remove item with its id */
void PLModel::removeItem( int i_id )
{
    PLItem *item = findById( rootItem, i_id );
    removeItem( item );
}

/* callbacks and slots */
void PLModel::addCallbacks()
{
    /* One item has been updated */
    var_AddCallback( p_playlist, "playlist-item-append", ItemAppended, this );
    var_AddCallback( p_playlist, "playlist-item-deleted", ItemDeleted, this );
}

void PLModel::delCallbacks()
{
    var_DelCallback( p_playlist, "playlist-item-append", ItemAppended, this );
    var_DelCallback( p_playlist, "playlist-item-deleted", ItemDeleted, this );
}

void PLModel::activateItem( const QModelIndex &index )
{
    assert( index.isValid() );
    PLItem *item = getItem( index );
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
    PLItem *item = getItem( index );
    if( role == Qt::DisplayRole )
    {
        int metadata = columnToMeta( index.column() );
        if( metadata == COLUMN_END ) return QVariant();

        QString returninfo;
        if( metadata == COLUMN_NUMBER )
            returninfo = QString::number( index.row() + 1 );
        else
        {
            char *psz = psz_column_meta( item->p_input, metadata );
            returninfo = qfu( psz );
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
    if( !currentItem ) return false;
    return getItem( index )->p_input == currentItem->p_input;
}

int PLModel::itemId( const QModelIndex &index ) const
{
    return getItem( index )->i_id;
}

QVariant PLModel::headerData( int section, Qt::Orientation orientation,
                              int role ) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    int meta_col = columnToMeta( section );

    if( meta_col == COLUMN_END ) return QVariant();

    return QVariant( qfu( psz_column_title( meta_col ) ) );
}

QModelIndex PLModel::index( int row, int column, const QModelIndex &parent )
                  const
{
    PLItem *parentItem = parent.isValid() ? getItem( parent ) : rootItem;

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

    PLItem *childItem = getItem( index );
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
    return columnFromMeta( COLUMN_END );
}

int PLModel::rowCount( const QModelIndex &parent ) const
{
    PLItem *parentItem = parent.isValid() ? getItem( parent ) : rootItem;
    return parentItem->childCount();
}

QStringList PLModel::selectedURIs()
{
    QStringList lst;
    for( int i = 0; i < current_selection.size(); i++ )
    {
        PLItem *item = getItem( current_selection[i] );
        if( item )
        {
            PL_LOCK;
            playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item->i_id );
            if( p_item )
            {
                char *psz = input_item_GetURI( p_item->p_input );
                if( psz )
                {
                    lst.append( qfu(psz) );
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

PLItem *PLModel::findById( PLItem *root, int i_id )
{
    return findInner( root, i_id, false );
}

PLItem *PLModel::findByInput( PLItem *root, int i_id )
{
    PLItem *result = findInner( root, i_id, true );
    return result;
}

#define CACHE( i, p ) { i_cached_id = i; p_cached_item = p; }
#define ICACHE( i, p ) { i_cached_input_id = i; p_cached_item_bi = p; }

PLItem * PLModel::findInner( PLItem *root, int i_id, bool b_input )
{
    if( !root ) return NULL;
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
            PLItem *childFound = findInner( (*it), i_id, b_input );
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

PLItem *PLModel::getItem( QModelIndex index )
{
    assert( index.isValid() );
    return static_cast<PLItem*>( index.internalPointer() );
}

int PLModel::columnToMeta( int _column ) const
{
    int meta = 1;
    int column = 0;

    while( column != _column && meta != COLUMN_END )
    {
        meta <<= 1;
        column++;
    }

    return meta;
}

int PLModel::columnFromMeta( int meta_col ) const
{
    int meta = 1;
    int column = 0;

    while( meta != meta_col && meta != COLUMN_END )
    {
        meta <<= 1;
        column++;
    }

    return column;
}

/************************* Updates handling *****************************/
void PLModel::customEvent( QEvent *event )
{
    int type = event->type();
    if( type != ItemAppend_Type &&
        type != ItemDelete_Type )
        return;

    PLEvent *ple = static_cast<PLEvent *>(event);

    if( type == ItemAppend_Type )
        processItemAppend( &ple->add );
    else if( type == ItemDelete_Type )
        processItemRemoval( ple->i_id );
}

/**** Events processing ****/
void PLModel::processInputItemUpdate( input_thread_t *p_input )
{
    if( !p_input ) return;
    processInputItemUpdate( input_GetItem( p_input ) );
    if( p_input && !( p_input->b_dead || !vlc_object_alive( p_input ) ) )
    {
        PLItem *item = findByInput( rootItem, input_GetItem( p_input )->i_id );
        currentItem = item;
        emit currentChanged( index( item, 0 ) );
    }
    else
    {
        currentItem = NULL;
    }
}
void PLModel::processInputItemUpdate( input_item_t *p_item )
{
    if( !p_item ||  p_item->i_id <= 0 ) return;
    PLItem *item = findByInput( rootItem, p_item->i_id );
    if( item )
        updateTreeItem( item );
}

void PLModel::processItemRemoval( int i_id )
{
    if( i_id <= 0 ) return;
    if( i_id == i_cached_id ) i_cached_id = -1;
    i_cached_input_id = -1;

    removeItem( i_id );
}

void PLModel::processItemAppend( const playlist_add_t *p_add )
{
    playlist_item_t *p_item = NULL;
    PLItem *newItem = NULL;

    PLItem *nodeItem = findById( rootItem, p_add->i_node );
    if( !nodeItem ) return;

    PL_LOCK;
    p_item = playlist_ItemGetById( p_playlist, p_add->i_item );
    if( !p_item || p_item->i_flags & PLAYLIST_DBL_FLAG ) goto end;

    newItem = new PLItem( p_item, nodeItem );
    PL_UNLOCK;

    beginInsertRows( index( nodeItem, 0 ), nodeItem->childCount(), nodeItem->childCount() );
    nodeItem->appendChild( newItem );
    endInsertRows();
    updateTreeItem( newItem );
    return;
end:
    PL_UNLOCK;
    return;
}


void PLModel::rebuild()
{
    rebuild( NULL, false );
}

void PLModel::rebuild( playlist_item_t *p_root, bool b_first )
{
    playlist_item_t* p_item;
    /* Remove callbacks before locking to avoid deadlocks
       The first time the callbacks are not present so
       don't try to delete them */
    if( !b_first )
        delCallbacks();

    /* Invalidate cache */
    i_cached_id = i_cached_input_id = -1;

    if( rootItem ) rootItem->removeChildren();

    PL_LOCK;
    if( p_root )
    {
        delete rootItem;
        rootItem = new PLItem( p_root );
    }
    assert( rootItem );
    /* Recreate from root */
    updateChildren( rootItem );
    if( (p_item = playlist_CurrentPlayingItem(p_playlist)) )
        currentItem = findByInput( rootItem, p_item->p_input->i_id );
    else
        currentItem = NULL;
    PL_UNLOCK;

    /* And signal the view */
    reset();

    emit currentChanged( index( currentItem, 0 ) );

    addCallbacks();
}

void PLModel::takeItem( PLItem *item )
{
    assert( item );
    PLItem *parent = item->parentItem;
    assert( parent );
    int i_index = parent->children.indexOf( item );

    beginRemoveRows( index( parent, 0 ), i_index, i_index );
    parent->takeChildAt( i_index );
    endRemoveRows();
}

void PLModel::insertChildren( PLItem *node, QList<PLItem*>& items, int i_pos )
{
    assert( node );
    int count = items.size();
    if( !count ) return;
    beginInsertRows( index( node, 0 ), i_pos, i_pos + count - 1 );
    for( int i = 0; i < count; i++ )
    {
        node->children.insert( i_pos + i, items[i] );
        items[i]->parentItem = node;
    }
    endInsertRows();
}

void PLModel::removeItem( PLItem *item )
{
    if( !item ) return;

    if( currentItem == item )
    {
        currentItem = NULL;
        emit currentChanged( QModelIndex() );
    }

    if( item->parentItem ) item->parentItem->removeChild( item );
    else delete item;

    if(item == rootItem)
    {
        rootItem = NULL;
        reset();
    }
}

/* This function must be entered WITH the playlist lock */
void PLModel::updateChildren( PLItem *root )
{
    playlist_item_t *p_node = playlist_ItemGetById( p_playlist, root->i_id );
    updateChildren( p_node, root );
}

/* This function must be entered WITH the playlist lock */
void PLModel::updateChildren( playlist_item_t *p_node, PLItem *root )
{
    playlist_item_t *p_item = playlist_CurrentPlayingItem(p_playlist);
    for( int i = 0; i < p_node->i_children ; i++ )
    {
        if( p_node->pp_children[i]->i_flags & PLAYLIST_DBL_FLAG ) continue;
        PLItem *newItem =  new PLItem( p_node->pp_children[i], root );
        root->appendChild( newItem );
        if( p_item && newItem->p_input == p_item->p_input )
        {
            currentItem = newItem;
            emit currentChanged( index( currentItem, 0 ) );
        }
        if( p_node->pp_children[i]->i_children != -1 )
            updateChildren( p_node->pp_children[i], newItem );
    }
}

/* Function doesn't need playlist-lock, as we don't touch playlist_item_t stuff here*/
void PLModel::updateTreeItem( PLItem *item )
{
    if( !item ) return;
    rebuild();
    //emit dataChanged( index( item, 0 ) , index( item, columnCount( QModelIndex() ) ) );
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
        PLItem *item = getItem( index );
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
    int itemIndex = item->parentItem->children.indexOf( item );
    beginRemoveRows( index( item->parentItem, 0), itemIndex, itemIndex );
    removeItem( item );
    endRemoveRows();
}

/******* Volume III: Sorting and searching ********/
void PLModel::sort( int column, Qt::SortOrder order )
{
    sort( rootItem->i_id, column, order );
}

void PLModel::sort( int i_root_id, int column, Qt::SortOrder order )
{
    int meta = columnToMeta( column );
    if( meta == COLUMN_END ) return;

    PLItem *item = findById( rootItem, i_root_id );
    if( !item ) return;
    QModelIndex qIndex = index( item, 0 );
    int count = item->children.size();
    if( count )
    {
        beginRemoveRows( qIndex, 0, count - 1 );
        item->removeChildren();
        endRemoveRows( );
    }

    PL_LOCK;
    {
        playlist_item_t *p_root = playlist_ItemGetById( p_playlist,
                                                        i_root_id );
        if( p_root )
        {
            playlist_RecursiveNodeSort( p_playlist, p_root,
                                        i_column_sorting( meta ),
                                        order == Qt::AscendingOrder ?
                                            ORDER_NORMAL : ORDER_REVERSE );
        }
    }
    if( count )
    {
        beginInsertRows( qIndex, 0, count - 1 );
        updateChildren( item );
        endInsertRows( );
    }
    PL_UNLOCK;
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
    int i_id = index.isValid() ? itemId( index ) : rootItem->i_id;

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_id );
    if( !p_item )
    {
        PL_UNLOCK; return;
    }
    i_popup_item = index.isValid() ? p_item->i_id : -1;
    i_popup_parent = index.isValid() ?
        ( p_item->p_parent ? p_item->p_parent->i_id : -1 ) :
        ( p_item->i_id );
    i_popup_column = index.column();
    /* check whether we are in tree view */
    bool tree = false;
    playlist_item_t *p_up = p_item;
    while( p_up )
    {
        if ( p_up == p_playlist->p_root_category ) tree = true;
        p_up = p_up->p_parent;
    }
    PL_UNLOCK;

    current_selection = list;
    QMenu *menu = new QMenu;
    if( i_popup_item > -1 )
    {
        menu->addAction( qtr(I_POP_PLAY), this, SLOT( popupPlay() ) );
        menu->addAction( qtr(I_POP_DEL), this, SLOT( popupDel() ) );
        menu->addSeparator();
        menu->addAction( qtr(I_POP_STREAM), this, SLOT( popupStream() ) );
        menu->addAction( qtr(I_POP_SAVE), this, SLOT( popupSave() ) );
        menu->addSeparator();
        menu->addAction( qtr(I_POP_INFO), this, SLOT( popupInfo() ) );
        menu->addSeparator();
        QMenu *sort_menu = menu->addMenu( qtr( "Sort by ") +
            qfu( psz_column_title( columnToMeta( index.column() ) ) ) );
        sort_menu->addAction( qtr( "Ascending" ),
            this, SLOT( popupSortAsc() ) );
        sort_menu->addAction( qtr( "Descending" ),
            this, SLOT( popupSortDesc() ) );
    }
    if( tree )
        menu->addAction( qtr(I_POP_ADD), this, SLOT( popupAddNode() ) );
    if( i_popup_item > -1 )
    {
        menu->addSeparator();
        menu->addAction( qtr( I_POP_EXPLORE ), this, SLOT( popupExplore() ) );
    }
    menu->popup( point );
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
                                                    i_popup_parent );
    if( p_item )
    {
        playlist_NodeCreate( p_playlist, qtu( name ), p_item, 0, NULL );
    }
    PL_UNLOCK;
}

void PLModel::popupSortAsc()
{
    sort( i_popup_parent, i_popup_column, Qt::AscendingOrder );
}

void PLModel::popupSortDesc()
{
    sort( i_popup_parent, i_popup_column, Qt::DescendingOrder );
}
/**********************************************************************
 * Playlist callbacks
 **********************************************************************/

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

