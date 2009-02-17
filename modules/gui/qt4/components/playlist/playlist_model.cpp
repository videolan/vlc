/*****************************************************************************
 * playlist_model.cpp : Manage playlist model
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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
static int ItemChanged( vlc_object_t *, const char *,
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

    rootItem          = NULL; /* PLItem rootItem, will be set in rebuild( ) */

    /* Icons initialization */
#define ADD_ICON(type, x) icons[ITEM_TYPE_##type] = QIcon( QPixmap( x ) )
    ADD_ICON( UNKNOWN , type_unknown_xpm );
    ADD_ICON( FILE, ":/type_file" );
    ADD_ICON( DIRECTORY, ":/type_directory" );
    ADD_ICON( DISC, ":/disc" );
    ADD_ICON( CDDA, ":/cdda" );
    ADD_ICON( CARD, ":/capture-card" );
    ADD_ICON( NET, ":/type_net" );
    ADD_ICON( PLAYLIST, ":/type_playlist" );
    ADD_ICON( NODE, ":/type_node" );
#undef ADD_ICON

    rebuild( p_root );
    CONNECT( THEMIM->getIM(), metaChanged( int ),
            this, ProcessInputItemUpdate( int ) );
    CONNECT( THEMIM, inputChanged( input_thread_t * ),
            this, ProcessInputItemUpdate( input_thread_t* ) );
}

PLModel::~PLModel()
{
    getSettings()->setValue( "qt-pl-showflags", rootItem->i_showflags );
    delCallbacks();
    delete rootItem;
}

Qt::DropActions PLModel::supportedDropActions() const
{
    return Qt::CopyAction; /* Why not Qt::MoveAction */
}

Qt::ItemFlags PLModel::flags( const QModelIndex &index ) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags( index );
    if( index.isValid() )
        return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
    else
        return Qt::ItemIsDropEnabled | defaultFlags;
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

    foreach( const QModelIndex &index, indexes ) {
        if( index.isValid() && index.column() == 0 )
            stream << itemId( index );
    }
    mimeData->setData( "vlc/playlist-item-id", encodedData );
    return mimeData;
}

/* Drop operation */
bool PLModel::dropMimeData( const QMimeData *data, Qt::DropAction action,
                           int row, int column, const QModelIndex &target )
{
    if( data->hasFormat( "vlc/playlist-item-id" ) )
    {
        if( action == Qt::IgnoreAction )
            return true;

        if( !target.isValid() )
            /* We don't want to move on an invalid position */
            return true;

        PLItem *targetItem = static_cast<PLItem*>( target.internalPointer() );

        QByteArray encodedData = data->data( "vlc/playlist-item-id" );
        QDataStream stream( &encodedData, QIODevice::ReadOnly );

        PLItem *newParentItem;
        while( !stream.atEnd() )
        {
            int i;
            int srcId;
            stream >> srcId;

            PL_LOCK;
            playlist_item_t *p_target =
                        playlist_ItemGetById( p_playlist, targetItem->i_id );
            playlist_item_t *p_src = playlist_ItemGetById( p_playlist, srcId );

            if( !p_target || !p_src )
            {
                PL_UNLOCK;
                return false;
            }
            if( p_target->i_children == -1 ) /* A leaf */
            {
                PLItem *parentItem = targetItem->parent();
                assert( parentItem );
                playlist_item_t *p_parent =
                         playlist_ItemGetById( p_playlist, parentItem->i_id );
                if( !p_parent )
                {
                    PL_UNLOCK;
                    return false;
                }
                for( i = 0 ; i< p_parent->i_children ; i++ )
                    if( p_parent->pp_children[i] == p_target ) break;
                // Move the item to the element after i
                playlist_TreeMove( p_playlist, p_src, p_parent, i + 1 );
                newParentItem = parentItem;
            }
            else
            {
                /* \todo: if we drop on a top-level node, use copy instead ? */
                playlist_TreeMove( p_playlist, p_src, p_target, 0 );
                i = 0;
                newParentItem = targetItem;
            }
            PL_UNLOCK;
        }
        /*TODO: That's not a good idea to rebuild the playlist */
        rebuild();
    }
    return true;
}

/* remove item with its id */
void PLModel::removeItem( int i_id )
{
    PLItem *item = FindById( rootItem, i_id );
    if( item ) item->remove( item );
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
    var_DelCallback( p_playlist, "item-change", ItemChanged, this );
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
        return QVariant( item->columnString( index.column() ) );
    }
    else if( role == Qt::DecorationRole && index.column() == 0  )
    {
        /* Use to segfault here because i_type wasn't always initialized */
        if( item->i_type >= 0 )
            return QVariant( PLModel::icons[item->i_type] );
    }
    else if( role == Qt::FontRole )
    {
        if( item->b_current == true )
        {
            QFont f; f.setBold( true ); return QVariant( f );
        }
    }
    return QVariant();
}

bool PLModel::isCurrent( const QModelIndex &index )
{
    assert( index.isValid() );
    return static_cast<PLItem*>(index.internalPointer())->b_current;
}

int PLModel::itemId( const QModelIndex &index ) const
{
    assert( index.isValid() );
    return static_cast<PLItem*>(index.internalPointer())->i_id;
}

QVariant PLModel::headerData( int section, Qt::Orientation orientation,
                              int role ) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
            return QVariant( rootItem->columnString( section ) );
    return QVariant();
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
    return rootItem->item_col_strings.count();
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
        PL_LOCK;
        PLItem *item = static_cast<PLItem*>
                    (current_selection[i].internalPointer());
        if( !item )
            continue;

        input_item_t *p_item = NULL;
        if( !p_item )
            continue;

        char *psz = input_item_GetURI( p_item );
        if( !psz )
            continue;
        else
        {
            lst.append( QString( psz ) );
            free( psz );
        }
        PL_UNLOCK;
    }
    return lst;
}

/************************* General playlist status ***********************/

bool PLModel::hasRandom()
{
    if( var_GetBool( p_playlist, "random" ) ) return true;
    return false;
}
bool PLModel::hasRepeat()
{
    if( var_GetBool( p_playlist, "repeat" ) ) return true;
    return false;
}
bool PLModel::hasLoop()
{
    if( var_GetBool( p_playlist, "loop" ) ) return true;
    return false;
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
    return FindInner( root, i_id, true );
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
    else if( b_input && root->i_input_id == i_id )
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
        else if( b_input && (*it)->i_input_id == i_id )
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
    ProcessInputItemUpdate( input_GetItem( p_input )->i_id );
}
void PLModel::ProcessInputItemUpdate( int i_input_id )
{
    if( i_input_id <= 0 ) return;
    PLItem *item = FindByInput( rootItem, i_input_id );
    if( item )
    {
        QPL_LOCK;
        UpdateTreeItem( item, true );
        QPL_UNLOCK;
    }
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
    PL_LOCK;
    if( !nodeItem ) goto end;

    p_item = playlist_ItemGetById( p_playlist, p_add->i_item );
    if( !p_item || p_item->i_flags & PLAYLIST_DBL_FLAG ) goto end;
    if( i_depth == DEPTH_SEL && p_item->p_parent &&
                        p_item->p_parent->i_id != rootItem->i_id )
        goto end;

    newItem = new PLItem( p_item, nodeItem, this );
    nodeItem->appendChild( newItem );
    UpdateTreeItem( p_item, newItem, true );
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

    PL_LOCK;
    /* Clear the tree */
    if( rootItem )
    {
        if( rootItem->children.size() )
        {
            beginRemoveRows( index( rootItem, 0 ), 0,
                    rootItem->children.size() -1 );
            qDeleteAll( rootItem->children );
            rootItem->children.clear();
            endRemoveRows();
        }
    }
    if( p_root )
    {
        delete rootItem;
        rootItem = new PLItem( p_root, getSettings(), this );
    }
    assert( rootItem );
    /* Recreate from root */
    UpdateNodeChildren( rootItem );
    if( (p_item = playlist_CurrentPlayingItem(p_playlist)) )
    {
        PLItem *currentItem = FindByInput( rootItem,
                                           p_item->p_input->i_id );
        if( currentItem )
        {
            UpdateTreeItem( p_item, currentItem,
                            true, false );
        }
    }
    PL_UNLOCK;

    /* And signal the view */
    emit layoutChanged();
    addCallbacks();
}

/* This function must be entered WITH the playlist lock */
void PLModel::UpdateNodeChildren( PLItem *root )
{
    playlist_item_t *p_node = playlist_ItemGetById( p_playlist, root->i_id );
    UpdateNodeChildren( p_node, root );
}

/* This function must be entered WITH the playlist lock */
void PLModel::UpdateNodeChildren( playlist_item_t *p_node, PLItem *root )
{
    for( int i = 0; i < p_node->i_children ; i++ )
    {
        if( p_node->pp_children[i]->i_flags & PLAYLIST_DBL_FLAG ) continue;
        PLItem *newItem =  new PLItem( p_node->pp_children[i], root, this );
        root->appendChild( newItem, false );
        UpdateTreeItem( newItem, false, true );
        if( i_depth == DEPTH_PL && p_node->pp_children[i]->i_children != -1 )
            UpdateNodeChildren( p_node->pp_children[i], newItem );
    }
}

/* This function must be entered WITH the playlist lock */
void PLModel::UpdateTreeItem( PLItem *item, bool signal, bool force )
{
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item->i_id );
    UpdateTreeItem( p_item, item, signal, force );
}

/* This function must be entered WITH the playlist lock */
void PLModel::UpdateTreeItem( playlist_item_t *p_item, PLItem *item,
                              bool signal, bool force )
{
    if ( !p_item )
        return;
    if( !force && i_depth == DEPTH_SEL && p_item->p_parent &&
                                 p_item->p_parent->i_id != rootItem->i_id )
        return;
    item->update( p_item, p_item == playlist_CurrentPlayingItem( p_playlist ) );
    if( signal )
        emit dataChanged( index( item, 0 ) , index( item, 1 ) );
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
        playlist_DeleteFromInput( p_playlist, item->i_input_id, pl_Locked );
    else
        playlist_NodeDelete( p_playlist, p_item, true, false );
    /* And finally, remove it from the tree */
    item->remove( item );
    PL_UNLOCK;
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
        if( p_root )
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

void PLModel::search( QString search_text )
{
    /** \todo Fire the search with a small delay ? */
    PL_LOCK;
    {
        playlist_item_t *p_root = playlist_ItemGetById( p_playlist,
                                                        rootItem->i_id );
        assert( p_root );
        char *psz_name = search_text.toUtf8().data();
        playlist_LiveSearchUpdate( p_playlist , p_root, psz_name );
    }
    PL_UNLOCK;
    rebuild();
}

/*********** Popup *********/
void PLModel::popup( QModelIndex & index, QPoint &point, QModelIndexList list )
{
    assert( index.isValid() );
    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, itemId( index ) );
    if( p_item )
    {
        i_popup_item = p_item->i_id;
        i_popup_parent = p_item->p_parent ? p_item->p_parent->i_id : -1;
        PL_UNLOCK;
        current_selection = list;
        QMenu *menu = new QMenu;
        menu->addAction( qtr(I_POP_PLAY), this, SLOT( popupPlay() ) );
        menu->addAction( qtr(I_POP_DEL), this, SLOT( popupDel() ) );
        menu->addSeparator();
        menu->addAction( qtr(I_POP_STREAM), this, SLOT( popupStream() ) );
        menu->addAction( qtr(I_POP_SAVE), this, SLOT( popupSave() ) );
        menu->addSeparator();
        menu->addAction( qtr(I_POP_INFO), this, SLOT( popupInfo() ) );
        if( p_item->i_children > -1 )
        {
            menu->addSeparator();
            menu->addAction( qtr(I_POP_SORT), this, SLOT( popupSort() ) );
            menu->addAction( qtr(I_POP_ADD), this, SLOT( popupAdd() ) );
        }
        menu->addSeparator();
        menu->addAction( qtr( I_POP_EXPLORE ), this, SLOT( popupExplore() ) );
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
        index = __MIN( index, rootItem->item_col_strings.count() );
        QModelIndex parent = createIndex( 0, 0, rootItem );

        if( rootItem->i_showflags & meta )
            /* Removing columns */
        {
            beginRemoveColumns( parent, index, index+1 );
            rootItem->i_showflags &= ~( meta );
            getSettings()->setValue( "qt-pl-showflags", rootItem->i_showflags );
            rootItem->updateColumnHeaders();
            endRemoveColumns();
        }
        else
        {
            /* Adding columns */
            beginInsertColumns( parent, index, index+1 );
            rootItem->i_showflags |= meta;
            getSettings()->setValue( "qt-pl-showflags", rootItem->i_showflags );
            rootItem->updateColumnHeaders();
            endInsertColumns();
        }
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
    }
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
        THEDP->streamingDialog( NULL, mrls[0], true );
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

static int ItemChanged( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    PLEvent *event = new PLEvent( ItemUpdate_Type, nval.i_int );
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

