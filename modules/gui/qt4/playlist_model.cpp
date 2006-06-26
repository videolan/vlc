/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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

#include <QApplication>
#include "playlist_model.hpp"
#include <assert.h>


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
 * Playlist item implementation
 *************************************************************************/

PLItem::PLItem( int _i_id, int _i_input_id, PLItem *parent, PLModel *m)
{
    parentItem = parent;
    i_id = _i_id; i_input_id = _i_input_id;
    model = m;
}

PLItem::PLItem( playlist_item_t * p_item, PLItem *parent, PLModel *m )
{
    i_id = p_item->i_id;
    i_input_id = p_item->p_input->i_id;
    parentItem = parent;
    model = m;
}

PLItem::~PLItem()
{
    qDeleteAll(children);
}

void PLItem::insertChild( PLItem *item, int i_pos, bool signal )
{
    assert( model );
    if( signal )
        model->beginInsertRows( model->index( this , 0 ), i_pos, i_pos );
    children.append( item );
    if( signal )
        model->endInsertRows();
}

int PLItem::row() const
{
    if (parentItem)
        return parentItem->children.indexOf(const_cast<PLItem*>(this));
    return 0;
}


/*************************************************************************
 * Playlist model implementation
 *************************************************************************/

PLModel::PLModel( playlist_item_t * p_root, int i_depth, QObject *parent)
                                    : QAbstractItemModel(parent)
{
    rootItem = new PLItem( p_root, NULL, this );

    i_items_to_append = 0;
    b_need_update     = false;
    i_cached_id       = -1;
    i_cached_input_id = -1;

    addCallbacks();
}

PLModel::~PLModel()
{
    delCallbacks();
    delete rootItem;
}

void PLModel::addCallbacks()
{
    /* Some global changes happened -> Rebuild all */
    var_AddCallback( p_playlist, "intf-change", PlaylistChanged, this );
    /* We went to the next item */
    var_AddCallback( p_playlist, "playlist-current", PlaylistNext, this );
    /* One item has been updated */
    var_AddCallback( p_playlist, "item-change", ItemChanged, this );
    var_AddCallback( p_playlist, "item-append", ItemAppended, this );
    var_AddCallback( p_playlist, "item-deleted", ItemDeleted, this );
}

void PLModel::delCallbacks()
{
    var_DelCallback( p_playlist, "item-change", ItemChanged, this );
    var_DelCallback( p_playlist, "playlist-current", PlaylistNext, this );
    var_DelCallback( p_playlist, "intf-change", PlaylistChanged, this );
    var_DelCallback( p_playlist, "item-append", ItemAppended, this );
    var_DelCallback( p_playlist, "item-deleted", ItemDeleted, this );
}

/****************** Base model mandatory implementations *****************/

QVariant PLModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (role != Qt::DisplayRole)
        return QVariant();

    PLItem *item = static_cast<PLItem*>(index.internalPointer());
    return QVariant( item->columnString( index.column() ) );
}

Qt::ItemFlags PLModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant PLModel::headerData( int section, Qt::Orientation orientation,
                              int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
            return QVariant( rootItem->columnString( section ) );
    return QVariant();
}

QModelIndex PLModel::index(int row, int column, const QModelIndex &parent)
                  const
{
    PLItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<PLItem*>(parent.internalPointer());

    PLItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

/* Return the index of a given item */
QModelIndex PLModel::index( PLItem *item, int column ) const 
{
    if( !item ) return QModelIndex();
    const PLItem *parent = item->parent();
    if( parent )
        return createIndex( parent->children.lastIndexOf( item ), column, item );
    return QModelIndex();
}

QModelIndex PLModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    PLItem *childItem = static_cast<PLItem*>(index.internalPointer());
    PLItem *parentItem = childItem->parent();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int PLModel::columnCount( const QModelIndex &i) const 
{
    return 1;
}

int PLModel::childrenCount(const QModelIndex &parent) const
{
    return rowCount( parent );
}

int PLModel::rowCount(const QModelIndex &parent) const
{
    PLItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<PLItem*>(parent.internalPointer());

    return parentItem->childCount();
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

#define CACHE( i, p ) i_cached_id = i; p_cached_item = p;
#define ICACHE( i, p ) i_cached_input_id = i; p_cached_item_bi = p;

PLItem * PLModel::FindInner( PLItem *root, int i_id, bool b_input )
{
    if( ( !b_input && i_cached_id == i_id) || 
        ( b_input && i_cached_input_id ==i_id ) )
        return b_input ? p_cached_item_bi : p_cached_item;

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
        }
        if( (*it)->children.size() )
        {
            PLItem *childFound = FindInner( (*it), i_id, b_input );
            if( childFound ) 
            {
                if( b_input )
                {
                    ICACHE( i_id, childFound );
                }
                else
                {
                    CACHE( i_id, childFound );
                }
                return childFound;
            } 
        }
    }
    return NULL;
}
#undef CACHE
#undef ICACHE


/************************* Updates handling *****************************/
void PLModel::customEvent( QEvent *event )
{
    PLEvent *ple = dynamic_cast<PLEvent *>(event);

    if( event->type() == ItemUpdate_Type )
        ProcessInputItemUpdate( ple->i_id );
    else if( event->type() == ItemAppend_Type )
        ProcessItemAppend( ple->p_add );
    else
        ProcessItemRemoval( ple->i_id );
}

/**** Events processing ****/
void PLModel::ProcessInputItemUpdate( int i_input_id )
{
    assert( i_input_id >= 0 );
    UpdateTreeItem( FindByInput( rootItem, i_input_id ), true );
}

void PLModel::ProcessItemRemoval( int i_id )
{
    assert( i_id >= 0 );
    if( i_id == i_cached_id ) i_cached_id = -1;
    i_cached_input_id = -1;

    /// \todo
}

void PLModel::ProcessItemAppend( playlist_add_t *p_add )
{
    playlist_item_t *p_item = NULL;
    i_items_to_append--;
    if( b_need_update ) return;

    PLItem *nodeItem = FindById( rootItem, p_add->i_node );
    if( !nodeItem ) goto end;

    p_item = playlist_ItemGetById( p_playlist, p_add->i_item );
    if( !p_item || p_item->i_flags & PLAYLIST_DBL_FLAG ) goto end;

    nodeItem->appendChild( new PLItem( p_item, nodeItem, this ) );

end:
    return;
}

void PLModel::Rebuild()
{
    /* Remove callbacks before locking to avoid deadlocks */
    delCallbacks();
    PL_LOCK;

    /* Invalidate cache */
    i_cached_id = i_cached_input_id = -1;

    /* Clear the tree */
    qDeleteAll( rootItem->children );

    /* Recreate from root */
    UpdateNodeChildren( rootItem );

    /* And signal the view */
    emit layoutChanged();

    addCallbacks();
    PL_UNLOCK;
}

void PLModel::UpdateNodeChildren( PLItem *root )
{
    playlist_item_t *p_node = playlist_ItemGetById( p_playlist, root->i_id );
    UpdateNodeChildren( p_node, root );
}

void PLModel::UpdateNodeChildren( playlist_item_t *p_node, PLItem *root )
{
    for( int i = 0; i < p_node->i_children ; i++ )
    {
        PLItem *newItem =  new PLItem( p_node->pp_children[i], root, this );
        root->appendChild( newItem, false );
        UpdateTreeItem( newItem, false );
        if( p_node->pp_children[i]->i_children != -1 )
            UpdateNodeChildren( p_node->pp_children[i], newItem );
    }
}

void PLModel::UpdateTreeItem( PLItem *item, bool signal )
{
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, rootItem->i_id );
    UpdateTreeItem( p_item, item, signal );
}

void PLModel::UpdateTreeItem( playlist_item_t *p_item, PLItem *item, bool signal ) 
{
    /// \todo
    if( signal )
    {    // emit 
    }
}
        
/**********************************************************************
 * Playlist callbacks 
 **********************************************************************/
static int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                            vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    p_model->b_need_update = VLC_TRUE;
    return VLC_SUCCESS;
}

static int PlaylistNext( vlc_object_t *p_this, const char *psz_variable,
                         vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    PLEvent *event = new PLEvent( ItemUpdate_Type, oval.i_int );
    QApplication::postEvent( p_model, static_cast<QEvent*>(event) );
    event = new PLEvent( ItemUpdate_Type, nval.i_int );
    QApplication::postEvent( p_model, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemChanged( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    PLEvent *event = new PLEvent( ItemUpdate_Type, nval.i_int ); 
    QApplication::postEvent( p_model, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemDeleted( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    PLEvent *event = new PLEvent( ItemDelete_Type, nval.i_int );
    QApplication::postEvent( p_model, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

static int ItemAppended( vlc_object_t *p_this, const char *psz_variable,
                         vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    playlist_add_t *p_add = (playlist_add_t *)malloc( sizeof( playlist_add_t));
    memcpy( p_add, nval.p_address, sizeof( playlist_add_t ) );

    if( ++p_model->i_items_to_append >= 50 )
    {
        p_model->b_need_update = VLC_TRUE;
        return VLC_SUCCESS;
    }
    PLEvent *event = new PLEvent(  p_add );
    QApplication::postEvent( p_model, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}
