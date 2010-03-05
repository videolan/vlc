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
#include <QUrl>
#include <QFileInfo>
#include <QDesktopServices>
#include <QInputDialog>

#include "sorting.h"

#define I_NEW_DIR \
    I_DIR_OR_FOLDER( N_("Create Directory"), N_( "Create Folder" ) )
#define I_NEW_DIR_NAME \
    I_DIR_OR_FOLDER( N_( "Enter name for new directory:" ), \
                     N_( "Enter name for new folder:" ) )

QIcon PLModel::icons[ITEM_TYPE_NUMBER];

/*************************************************************************
 * Playlist model implementation
 *************************************************************************/

PLModel::PLModel( playlist_t *_p_playlist,  /* THEPL */
                  intf_thread_t *_p_intf,   /* main Qt p_intf */
                  playlist_item_t * p_root,
                  QObject *parent )         /* Basic Qt parent */
                  : QAbstractItemModel( parent )
{
    p_intf            = _p_intf;
    p_playlist        = _p_playlist;
    i_cached_id       = -1;
    i_cached_input_id = -1;
    i_popup_item      = i_popup_parent = -1;
    sortingMenu       = NULL;

    rootItem          = NULL; /* PLItem rootItem, will be set in rebuild( ) */

    /* Icons initialization */
#define ADD_ICON(type, x) icons[ITEM_TYPE_##type] = QIcon( x )
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
    DCONNECT( THEMIM->getIM(), metaChanged( input_item_t *),
             this, processInputItemUpdate( input_item_t *) );
    DCONNECT( THEMIM, inputChanged( input_thread_t * ),
             this, processInputItemUpdate( input_thread_t* ) );
    CONNECT( THEMIM, playlistItemAppended( int, int ),
             this, processItemAppend( int, int ) );
    CONNECT( THEMIM, playlistItemRemoved( int ),
             this, processItemRemoval( int ) );
}

PLModel::~PLModel()
{
    delete rootItem;
    delete sortingMenu;
}

Qt::DropActions PLModel::supportedDropActions() const
{
    return Qt::CopyAction; /* Why not Qt::MoveAction */
}

Qt::ItemFlags PLModel::flags( const QModelIndex &index ) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags( index );

    PLItem *item = index.isValid() ? getItem( index ) : rootItem;

    if( canEdit() )
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
        playlist_item_t *p_pl = p_playlist->p_playing;
        playlist_item_t *p_ml = p_playlist->p_media_library;
        if
        (
            row == -1 && (
            ( p_pl && p_parent == p_pl ) ||
            ( p_ml && p_parent == p_ml ) )
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
            p_parent == p_playlist->p_playing,
            true );
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
        /* Used to segfault here because i_type wasn't always initialized */
        return QVariant( PLModel::icons[item->p_input->i_type] );
    }
    else if( role == Qt::FontRole )
    {
        if( isCurrent( index ) )
        {
            QFont f; f.setBold( true ); return QVariant( f );
        }
    }
    else if( role == Qt::BackgroundRole && isCurrent( index ) )
    {
        return QVariant( QBrush( Qt::gray ) );
    }
    else if( role == IsCurrentRole ) return QVariant( isCurrent( index ) );
    else if( role == IsLeafNodeRole )
    {
        QVariant isLeaf;
        PL_LOCK;
        playlist_item_t *plItem =
            playlist_ItemGetById( p_playlist, item->i_id );

        if( plItem )
            isLeaf = plItem->i_children == -1;

        PL_UNLOCK;
        return isLeaf;
    }
    return QVariant();
}

bool PLModel::isCurrent( const QModelIndex &index ) const
{
    return getItem( index )->p_input == THEMIM->currentInputItem();
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

QModelIndex PLModel::index( int i_id, int c )
{
  return index( findById( rootItem, i_id ), c );
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

QModelIndex PLModel::currentIndex()
{
    input_thread_t *p_input_thread = THEMIM->getInput();
    if( !p_input_thread ) return QModelIndex();
    PLItem *item = findByInput( rootItem, input_GetItem( p_input_thread )->i_id );
    return index( item, 0 );
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

int PLModel::columnToMeta( int _column )
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

int PLModel::columnFromMeta( int meta_col )
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

bool PLModel::canEdit() const
{
  return (
    rootItem != NULL &&
    (
      rootItem->p_input == p_playlist->p_playing->p_input ||
      (
        p_playlist->p_media_library &&
        rootItem->p_input == p_playlist->p_media_library->p_input
      )
    )
  );
}
/************************* Updates handling *****************************/

/**** Events processing ****/
void PLModel::processInputItemUpdate( input_thread_t *p_input )
{
    if( !p_input ) return;
    if( p_input && !( p_input->b_dead || !vlc_object_alive( p_input ) ) )
    {
        PLItem *item = findByInput( rootItem, input_GetItem( p_input )->i_id );
        if( item ) emit currentChanged( index( item, 0 ) );
    }
    processInputItemUpdate( input_GetItem( p_input ) );
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
    removeItem( i_id );
}

void PLModel::processItemAppend( int i_item, int i_parent )
{
    playlist_item_t *p_item = NULL;
    PLItem *newItem = NULL;
    input_thread_t *currentInputThread;
    int pos;

    PLItem *nodeItem = findById( rootItem, i_parent );
    if( !nodeItem ) return;

    foreach( PLItem *existing, nodeItem->children )
      if( existing->i_id == i_item ) return;

    PL_LOCK;
    p_item = playlist_ItemGetById( p_playlist, i_item );
    if( !p_item || p_item->i_flags & PLAYLIST_DBL_FLAG )
    {
        PL_UNLOCK; return;
    }

    for( pos = 0; pos < p_item->p_parent->i_children; pos++ )
        if( p_item->p_parent->pp_children[pos] == p_item ) break;

    newItem = new PLItem( p_item, nodeItem );
    PL_UNLOCK;

    beginInsertRows( index( nodeItem, 0 ), pos, pos );
    nodeItem->insertChild( newItem, pos );
    endInsertRows();

    if( newItem->p_input == THEMIM->currentInputItem() )
        emit currentChanged( index( newItem, 0 ) );
}


void PLModel::rebuild()
{
    rebuild( NULL );
}

void PLModel::rebuild( playlist_item_t *p_root )
{
    playlist_item_t* p_item;

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
    PL_UNLOCK;

    /* And signal the view */
    reset();

    if( p_root ) emit rootChanged();
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

    i_cached_id = -1;
    i_cached_input_id = -1;

    if( item->parentItem ) {
        int i = item->parentItem->children.indexOf( item );
        beginRemoveRows( index( item->parentItem, 0), i, i );
        item->parentItem->children.removeAt(i);
        delete item;
        endRemoveRows();
    }
    else delete item;

    if(item == rootItem)
    {
        rootItem = NULL;
        rebuild( p_playlist->p_playing );
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
    for( int i = 0; i < p_node->i_children ; i++ )
    {
        if( p_node->pp_children[i]->i_flags & PLAYLIST_DBL_FLAG ) continue;
        PLItem *newItem =  new PLItem( p_node->pp_children[i], root );
        root->appendChild( newItem );
        if( p_node->pp_children[i]->i_children != -1 )
            updateChildren( p_node->pp_children[i], newItem );
    }
}

/* Function doesn't need playlist-lock, as we don't touch playlist_item_t stuff here*/
void PLModel::updateTreeItem( PLItem *item )
{
    if( !item ) return;
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
    if( !canEdit() ) return;

    while( !selected.isEmpty() )
    {
        QModelIndex index = selected[0];
        selected.removeAt( 0 );

        if( index.column() != 0 ) continue;

        PLItem *item = getItem( index );
        if( item->children.size() )
            recurseDelete( item->children, &selected );

        PL_LOCK;
        playlist_DeleteFromInput( p_playlist, item->p_input, pl_Locked );
        PL_UNLOCK;

        removeItem( item );
    }
}

void PLModel::recurseDelete( QList<PLItem*> children, QModelIndexList *fullList )
{
    for( int i = children.size() - 1; i >= 0 ; i-- )
    {
        PLItem *item = children[i];
        if( item->children.size() )
            recurseDelete( item->children, fullList );
        fullList->removeAll( index( item, 0 ) );
    }
}

/******* Volume III: Sorting and searching ********/
void PLModel::sort( int column, Qt::SortOrder order )
{
    sort( rootItem->i_id, column, order );
}

void PLModel::sort( int i_root_id, int column, Qt::SortOrder order )
{
    msg_Dbg( p_intf, "Sorting by column %i, order %i", column, order );

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

    i_cached_id = i_cached_input_id = -1;

    if( count )
    {
        beginInsertRows( qIndex, 0, count - 1 );
        updateChildren( item );
        endInsertRows( );
    }
    PL_UNLOCK;
}

void PLModel::search( const QString& search_text, const QModelIndex & idx, bool b_recursive )
{
    /** \todo Fire the search with a small delay ? */
    PL_LOCK;
    {
        playlist_item_t *p_root = playlist_ItemGetById( p_playlist,
                                                        itemId( idx ) );
        assert( p_root );
        const char *psz_name = qtu( search_text );
        playlist_LiveSearchUpdate( p_playlist , p_root, psz_name, b_recursive );

        if( idx.isValid() )
        {
            PLItem *searchRoot = getItem( idx );

            beginRemoveRows( idx, 0, searchRoot->children.size() - 1 );
            searchRoot->removeChildren();
            endRemoveRows( );

            beginInsertRows( idx, 0, searchRoot->children.size() - 1 );
            updateChildren( searchRoot );
            endInsertRows();

            PL_UNLOCK;
            return;
        }
    }
    PL_UNLOCK;
    rebuild();
}

/*********** Popup *********/
bool PLModel::popup( const QModelIndex & index, const QPoint &point, const QModelIndexList &list )
{
    int i_id = index.isValid() ? itemId( index ) : rootItem->i_id;

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_id );
    if( !p_item )
    {
        PL_UNLOCK;
        return false;
    }

    i_popup_item = index.isValid() ? p_item->i_id : -1;
    i_popup_parent = index.isValid() ?
        ( p_item->p_parent ? p_item->p_parent->i_id : -1 ) :
        ( rootItem->i_id );
    i_popup_column = index.column();

    bool tree = ( rootItem && rootItem->i_id != p_playlist->p_playing->i_id ) ||
                var_InheritBool( p_intf, "playlist-tree" );

    PL_UNLOCK;

    current_selection = list;

    QMenu menu;
    if( i_popup_item > -1 )
    {
        menu.addAction( QIcon( ":/menu/play" ), qtr(I_POP_PLAY), this, SLOT( popupPlay() ) );
        menu.addAction( QIcon( ":/menu/stream" ),
                        qtr(I_POP_STREAM), this, SLOT( popupStream() ) );
        menu.addAction( qtr(I_POP_SAVE), this, SLOT( popupSave() ) );
        menu.addAction( QIcon( ":/menu/info" ), qtr(I_POP_INFO), this, SLOT( popupInfo() ) );
        menu.addAction( QIcon( ":/type/folder-grey" ),
                        qtr( I_POP_EXPLORE ), this, SLOT( popupExplore() ) );
        menu.addSeparator();
    }
    if( canEdit() )
    {
        QIcon addIcon( ":/buttons/playlist/playlist_add" );
        menu.addSeparator();
        if( tree ) menu.addAction( addIcon, qtr(I_POP_NEWFOLDER), this, SLOT( popupAddNode() ) );
        if( rootItem->i_id == THEPL->p_playing->i_id )
        {
            menu.addAction( addIcon, qtr(I_PL_ADDF), THEDP, SLOT( simplePLAppendDialog()) );
            menu.addAction( addIcon, qtr(I_PL_ADDDIR), THEDP, SLOT( PLAppendDir()) );
            menu.addAction( addIcon, qtr(I_OP_ADVOP), THEDP, SLOT( PLAppendDialog()) );
        }
        else if( THEPL->p_media_library &&
                    rootItem->i_id == THEPL->p_media_library->i_id )
        {
            menu.addAction( addIcon, qtr(I_PL_ADDF), THEDP, SLOT( simpleMLAppendDialog()) );
            menu.addAction( addIcon, qtr(I_PL_ADDDIR), THEDP, SLOT( MLAppendDir() ) );
            menu.addAction( addIcon, qtr(I_OP_ADVOP), THEDP, SLOT( MLAppendDialog() ) );
        }
    }
    if( i_popup_item > -1 )
    {
        menu.addAction( QIcon( ":/buttons/playlist/playlist_remove" ),
                        qtr(I_POP_DEL), this, SLOT( popupDel() ) );
        menu.addSeparator();
        if( !sortingMenu )
        {
            sortingMenu = new QMenu( qtr( "Sort by" ) );
            sortingMapper = new QSignalMapper( this );
            int i, j;
            for( i = 1, j = 1; i < COLUMN_END; i <<= 1, j++ )
            {
                if( i == COLUMN_NUMBER ) continue;
                QMenu *m = sortingMenu->addMenu( qfu( psz_column_title( i ) ) );
                QAction *asc = m->addAction( qtr("Ascending") );
                QAction *desc = m->addAction( qtr("Descending") );
                sortingMapper->setMapping( asc, j );
                sortingMapper->setMapping( desc, -j );
                CONNECT( asc, triggered(), sortingMapper, map() );
                CONNECT( desc, triggered(), sortingMapper, map() );
            }
            CONNECT( sortingMapper, mapped( int ), this, popupSort( int ) );
        }
        menu.addMenu( sortingMenu );
    }
    if( !menu.isEmpty() )
    {
        menu.exec( point ); return true;
    }
    else return false;
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

           if( !EMPTY_STR( psz_access ) && (
                   !strncasecmp( psz_access, "file", 4 ) ||
                   !strncasecmp( psz_access, "dire", 4 ) ))
           {
               QFileInfo info( qfu( psz_path ) );
               QDesktopServices::openUrl(
                               QUrl::fromLocalFile( info.absolutePath() ) );
           }
           free( psz_meta );
       }
    }
    else
        PL_UNLOCK;
}

void PLModel::popupAddNode()
{
    bool ok;
    QString name = QInputDialog::getText( PlaylistDialog::getInstance( p_intf ),
        qtr( I_NEW_DIR ), qtr( I_NEW_DIR_NAME ),
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

void PLModel::popupSort( int column )
{
    sort( i_popup_parent,
          column > 0 ? column - 1 : -column - 1,
          column > 0 ? Qt::AscendingOrder : Qt::DescendingOrder );
}
