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

#include <assert.h>
#include <QIcon>
#include <QFont>
#include <QMenu>
#include <QApplication>

#include "qt4.hpp"
#include "components/playlist/playlist_model.hpp"
#include "dialogs/mediainfo.hpp"
#include <vlc_intf_strings.h>

#include "pixmaps/type_unknown.xpm"

#define DEPTH_PL -1
#define DEPTH_SEL 1
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
 * Playlist item implementation
 *************************************************************************/

/*
   Playlist item is just a wrapper, an abstraction of the playlist_item
   in order to be managed by PLModel

   PLItem have a parent, and id and a input Id
*/


void PLItem::init( int _i_id, int _i_input_id, PLItem *parent, PLModel *m )
{
    parentItem = parent;          /* Can be NULL, but only for the rootItem */
    i_id       = _i_id;           /* Playlist item specific id */
    i_input_id = _i_input_id;     /* Identifier of the input */
    model      = m;               /* PLModel (QAbsmodel) */
    i_type     = -1;              /* Item type - Avoid segfault */
    b_current  = false;

    assert( model );

    /* No parent, should be the main one */
    if( parentItem == NULL )
    {
        i_showflags = config_GetInt( model->p_intf, "qt-pl-showflags" );
        updateview();
    }
    else
    {
        i_showflags = parentItem->i_showflags;
        //Add empty string and update() handles data appending
        item_col_strings.append( "" );
    }
    msg_Dbg( model->p_intf, "PLItem created of type: %i", model->i_depth );
}

/*
   Constructors
   Call the above function init
   So far the first constructor isn't used...
   */
PLItem::PLItem( int _i_id, int _i_input_id, PLItem *parent, PLModel *m )
{
    init( _i_id, _i_input_id, parent, m );
}

PLItem::PLItem( playlist_item_t * p_item, PLItem *parent, PLModel *m )
{
    init( p_item->i_id, p_item->p_input->i_id, parent, m );
}

PLItem::~PLItem()
{
    qDeleteAll( children );
    children.clear();
}

/* Column manager */
void PLItem::updateview()
{
    item_col_strings.clear();

    if( model->i_depth == 1 )  /* Selector Panel */
    {
        item_col_strings.append( "" );
        return;
    }

    for( int i_index=1; i_index <= VLC_META_ENGINE_ART_URL; i_index *= 2 )
    {
        if( i_showflags & i_index )
        {
            switch( i_index )
            {
            case VLC_META_ENGINE_ARTIST:
                item_col_strings.append( qtr( VLC_META_ARTIST ) );
                break;
            case VLC_META_ENGINE_TITLE:
                item_col_strings.append( qtr( VLC_META_TITLE ) );
                break;
            case VLC_META_ENGINE_DESCRIPTION:
                item_col_strings.append( qtr( VLC_META_DESCRIPTION ) );
                break;
            case VLC_META_ENGINE_DURATION:
                item_col_strings.append( qtr( "Duration" ) );
                break;
            case VLC_META_ENGINE_GENRE:
                item_col_strings.append( qtr( VLC_META_GENRE ) );
                break;
            case VLC_META_ENGINE_COLLECTION:
                item_col_strings.append( qtr( VLC_META_COLLECTION ) );
                break;
            case VLC_META_ENGINE_SEQ_NUM:
                item_col_strings.append( qtr( VLC_META_SEQ_NUM ) );
                break;
            case VLC_META_ENGINE_RATING:
                item_col_strings.append( qtr( VLC_META_RATING ) );
                break;
            default:
                break;
            }
        }
    }
}

/* So far signal is always true.
   Using signal false would not call PLModel... Why ?
 */
void PLItem::insertChild( PLItem *item, int i_pos, bool signal )
{
    if( signal )
        model->beginInsertRows( model->index( this , 0 ), i_pos, i_pos );
    children.insert( i_pos, item );
    if( signal )
        model->endInsertRows();
}

void PLItem::remove( PLItem *removed )
{
    if( model->i_depth == DEPTH_SEL || parentItem )
    {
        int i_index = parentItem->children.indexOf( removed );
        model->beginRemoveRows( model->index( parentItem, 0 ),
                                i_index, i_index );
        parentItem->children.removeAt( i_index );
        model->endRemoveRows();
    }
}

/* This function is used to get one's parent's row number in the model */
int PLItem::row() const
{
    if( parentItem )
        return parentItem->children.indexOf( const_cast<PLItem*>(this) ); // Why this ? I don't think we ever inherit PLItem
    return 0;
}

/* update the PL Item, get the good names and so on */
void PLItem::update( playlist_item_t *p_item, bool iscurrent )
{
    char psz_duration[MSTRTIME_MAX_SIZE];
    char *psz_meta;

    assert( p_item->p_input->i_id == i_input_id );

    i_type = p_item->p_input->i_type;
    b_current = iscurrent;

    item_col_strings.clear();

    if( model->i_depth == 1 )  /* Selector Panel */
    {
        item_col_strings.append( qfu( p_item->p_input->psz_name ) );
        return;
    }

#define ADD_META( item, meta ) \
    psz_meta = input_item_Get ## meta ( item->p_input ); \
    item_col_strings.append( qfu( psz_meta ) ); \
    free( psz_meta );

    for( int i_index=1; i_index <= VLC_META_ENGINE_ART_URL; i_index *= 2 )
    {
        if( parentItem->i_showflags & i_index )
        {
            switch( i_index )
            {
            case VLC_META_ENGINE_ARTIST:
                ADD_META( p_item, Artist );
                break;
            case VLC_META_ENGINE_TITLE:
                char *psz_title, *psz_name;
                psz_title = input_item_GetTitle( p_item->p_input );
                psz_name = input_item_GetName( p_item->p_input );
                if( psz_title )
                {
                    ADD_META( p_item, Title );
                }
                else if( psz_name )
                {
                    item_col_strings.append( qfu( psz_name ) );
                }
                free( psz_title );
                free( psz_name );
                break;
            case VLC_META_ENGINE_DESCRIPTION:
                ADD_META( p_item, Description );
                break;
            case VLC_META_ENGINE_DURATION:
                secstotimestr( psz_duration,
                    input_item_GetDuration( p_item->p_input ) / 1000000 );
                item_col_strings.append( QString( psz_duration ) );
                break;
            case VLC_META_ENGINE_GENRE:
                ADD_META( p_item, Genre );
                break;
            case VLC_META_ENGINE_COLLECTION:
                ADD_META( p_item, Album );
                break;
            case VLC_META_ENGINE_SEQ_NUM:
                ADD_META( p_item, TrackNum );
                break;
            case VLC_META_ENGINE_RATING:
                ADD_META( p_item, Rating );
            default:
                break;
            }
        }

    }
#undef ADD_META
}

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
    i_items_to_append = 0;
    b_need_update     = false;
    i_cached_id       = -1;
    i_cached_input_id = -1;
    i_popup_item      = i_popup_parent = -1;

    rootItem          = NULL; /* PLItem rootItem, will be set in rebuild( ) */

    /* Icons initialization */
#define ADD_ICON(type, x) icons[ITEM_TYPE_##type] = QIcon( QPixmap( x ) )
    ADD_ICON( UNKNOWN , type_unknown_xpm );
    ADD_ICON( FILE, ":/pixmaps/type_file.png" );
    ADD_ICON( DIRECTORY, ":/pixmaps/type_directory.png" );
    ADD_ICON( DISC, ":/pixmaps/disc_16px.png" );
    ADD_ICON( CDDA, ":/pixmaps/cdda_16px.png" );
    ADD_ICON( CARD, ":/pixmaps/capture-card_16px.png" );
    ADD_ICON( NET, ":/pixmaps/type_net.png" );
    ADD_ICON( PLAYLIST, ":/pixmaps/type_playlist.png" );
    ADD_ICON( NODE, ":/pixmaps/type_node.png" );
#undef ADD_ICON

    addCallbacks();
    rebuild( p_root );
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

    foreach( QModelIndex index, indexes ) {
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

        PLItem *targetItem;
        if( target.isValid() )
            targetItem = static_cast<PLItem*>( target.internalPointer() );
        else
            targetItem = rootItem;

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
                        playlist_ItemGetById( p_playlist, targetItem->i_id,
                                              VLC_TRUE );
            playlist_item_t *p_src = playlist_ItemGetById( p_playlist, srcId,
                                                           VLC_TRUE );

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
                         playlist_ItemGetById( p_playlist, parentItem->i_id,
                                               VLC_TRUE );
                if( !p_parent )
                {
                    PL_UNLOCK;
                    return false;
                }
                for( i = 0 ; i< p_parent->i_children ; i++ )
                    if( p_parent->pp_children[i] == p_target ) break;
                playlist_TreeMove( p_playlist, p_src, p_parent, i );
                newParentItem = parentItem;
            }
            else
            {
                /* \todo: if we drop on a top-level node, use copy instead ? */
                playlist_TreeMove( p_playlist, p_src, p_target, 0 );
                i = 0;
                newParentItem = targetItem;
            }
            /* Remove from source */
            PLItem *srcItem = FindById( rootItem, p_src->i_id );
            // We dropped on the source selector. Ask the dialog to forward
            // to the main view
            if( !srcItem )
            {
                emit shouldRemove( p_src->i_id );
            }
            else
                srcItem->remove( srcItem );

            /* Display at new destination */
            PLItem *newItem = new PLItem( p_src, newParentItem, this );
            newParentItem->insertChild( newItem, i, true );
            UpdateTreeItem( p_src, newItem, true );
            if( p_src->i_children != -1 )
                UpdateNodeChildren( newItem );
            PL_UNLOCK;
        }
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

void PLModel::activateItem( const QModelIndex &index )
{
    assert( index.isValid() );
    PLItem *item = static_cast<PLItem*>(index.internalPointer());
    assert( item );
    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item->i_id,
                                                    VLC_TRUE);
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
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, VLC_TRUE,
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
    var_SetBool( p_playlist, "loop", on ? VLC_TRUE:VLC_FALSE );
    config_PutInt( p_playlist, "loop", on ? 1: 0 );
}
void PLModel::setRepeat( bool on )
{
    var_SetBool( p_playlist, "repeat", on ? VLC_TRUE:VLC_FALSE );
    config_PutInt( p_playlist, "repeat", on ? 1: 0 );
}
void PLModel::setRandom( bool on )
{
    var_SetBool( p_playlist, "random", on ? VLC_TRUE:VLC_FALSE );
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
    if( type != ItemUpdate_Type && type != ItemAppend_Type &&
        type != ItemDelete_Type && type != PLUpdate_Type )
        return;

    PLEvent *ple = static_cast<PLEvent *>(event);

    if( type == ItemUpdate_Type )
        ProcessInputItemUpdate( ple->i_id );
    else if( type == ItemAppend_Type )
        ProcessItemAppend( ple->p_add );
    else if( type == ItemDelete_Type )
        ProcessItemRemoval( ple->i_id );
    else
        rebuild();
}

/**** Events processing ****/
void PLModel::ProcessInputItemUpdate( int i_input_id )
{
    if( i_input_id <= 0 ) return;
    PLItem *item = FindByInput( rootItem, i_input_id );
    if( item )
        UpdateTreeItem( item, true );
}

void PLModel::ProcessItemRemoval( int i_id )
{
    if( i_id <= 0 ) return;
    if( i_id == i_cached_id ) i_cached_id = -1;
    i_cached_input_id = -1;

    removeItem( i_id );
}

void PLModel::ProcessItemAppend( playlist_add_t *p_add )
{
    playlist_item_t *p_item = NULL;
    PLItem *newItem = NULL;
    i_items_to_append--;
    if( b_need_update ) return;

    PLItem *nodeItem = FindById( rootItem, p_add->i_node );
    PL_LOCK;
    if( !nodeItem ) goto end;

    p_item = playlist_ItemGetById( p_playlist, p_add->i_item, VLC_TRUE );
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
        //if( rootItem ) delete rootItem;
        rootItem = new PLItem( p_root, NULL, this );
    }
    assert( rootItem );
    /* Recreate from root */
    UpdateNodeChildren( rootItem );
    if( p_playlist->status.p_item )
    {
        PLItem *currentItem = FindByInput( rootItem,
                                     p_playlist->status.p_item->p_input->i_id );
        if( currentItem )
        {
            UpdateTreeItem( p_playlist->status.p_item, currentItem,
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
    playlist_item_t *p_node = playlist_ItemGetById( p_playlist, root->i_id,
                                                    VLC_TRUE );
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
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item->i_id,
                                                    VLC_TRUE );
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
    item->update( p_item, p_item == p_playlist->status.p_item );
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
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, item->i_id,
                                                    VLC_TRUE );
    if( !p_item )
    {
        PL_UNLOCK; return;
    }
    if( p_item->i_children == -1 )
        playlist_DeleteFromInput( p_playlist, item->i_input_id, VLC_TRUE );
    else
        playlist_NodeDelete( p_playlist, p_item, VLC_TRUE, VLC_FALSE );
    /* And finally, remove it from the tree */
    item->remove( item );
    PL_UNLOCK;
}

/******* Volume III: Sorting and searching ********/
void PLModel::sort( int column, Qt::SortOrder order )
{
    PL_LOCK;
    {
        playlist_item_t *p_root = playlist_ItemGetById( p_playlist,
                                                        rootItem->i_id,
                                                        VLC_TRUE );
        int i_mode;
        switch( column )
        {
        case 0: i_mode = SORT_TITLE_NODES_FIRST;break;
        case 1: i_mode = SORT_ARTIST;break;
        case 2: i_mode = SORT_DURATION; break;
        default: i_mode = SORT_TITLE_NODES_FIRST; break;
        }
        if( p_root )
            playlist_RecursiveNodeSort( p_playlist, p_root, i_mode,
                                        order == Qt::AscendingOrder ?
                                            ORDER_NORMAL : ORDER_REVERSE );
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
                                                        rootItem->i_id,
                                                        VLC_TRUE );
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
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist,
                                                    itemId( index ), VLC_TRUE );
    if( p_item )
    {
        i_popup_item = p_item->i_id;
        i_popup_parent = p_item->p_parent ? p_item->p_parent->i_id : -1;
        PL_UNLOCK;
        current_selection = list;
        QMenu *menu = new QMenu;
        menu->addAction( qfu(I_POP_PLAY), this, SLOT( popupPlay() ) );
        menu->addAction( qfu(I_POP_DEL), this, SLOT( popupDel() ) );
        menu->addSeparator();
        menu->addAction( qfu(I_POP_STREAM), this, SLOT( popupStream() ) );
        menu->addAction( qfu(I_POP_SAVE), this, SLOT( popupSave() ) );
        menu->addSeparator();
        menu->addAction( qfu(I_POP_INFO), this, SLOT( popupInfo() ) );
        if( p_item->i_children > -1 )
        {
            menu->addSeparator();
            menu->addAction( qfu(I_POP_SORT), this, SLOT( popupSort() ) );
            menu->addAction( qfu(I_POP_ADD), this, SLOT( popupAdd() ) );
        }
#ifdef WIN32
        menu->addSeparator();
        menu->addAction( qfu( I_POP_EXPLORE ), this, SLOT( popupExplore() ) );
#endif
        menu->popup( point );
    }
    else
        PL_UNLOCK;
}


void PLModel::viewchanged( int meta )
{
   if( rootItem )
   {
       int index=0;
       switch( meta )
       {
       case VLC_META_ENGINE_TITLE:
           index=0; break;
       case VLC_META_ENGINE_DURATION:
           index=1; break;
       case VLC_META_ENGINE_ARTIST:
           index=2; break;
       case VLC_META_ENGINE_GENRE:
           index=3; break;
       case VLC_META_ENGINE_COPYRIGHT:
           index=4; break;
       case VLC_META_ENGINE_COLLECTION:
           index=5; break;
       case VLC_META_ENGINE_SEQ_NUM:
           index=6; break;
       case VLC_META_ENGINE_DESCRIPTION:
           index=7; break;
       default:
           break;
       }
       /* UNUSED        emit layoutAboutToBeChanged(); */
       index = __MIN( index , rootItem->item_col_strings.count() );
       QModelIndex parent = createIndex( 0, 0, rootItem );

       if( rootItem->i_showflags & meta )
           /* Removing columns */
       {
           beginRemoveColumns( parent, index, index+1 );
           rootItem->i_showflags &= ~( meta );
           rootItem->updateview();
           endRemoveColumns();
       }
       else
       {
           /* Adding columns */
           beginInsertColumns( createIndex( 0, 0, rootItem), index, index+1 );
           rootItem->i_showflags |= meta;
           rootItem->updateview();
           endInsertColumns();
       }
       rebuild();
       config_PutInt( p_intf, "qt-pl-showflags", rootItem->i_showflags );
       config_SaveConfigFile( p_intf, NULL );
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
                                                        i_popup_item,VLC_TRUE );
        activateItem( p_item );
    }
    PL_UNLOCK;
}

void PLModel::popupInfo()
{
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist,
                                                    i_popup_item,
                                                    VLC_TRUE );
    if( p_item )
    {
        MediaInfoDialog *mid = new MediaInfoDialog( p_intf, p_item->p_input );
        mid->show();
    }
}

void PLModel::popupStream()
{
     msg_Err( p_playlist, "Stream not implemented" );
}

void PLModel::popupSave()
{
     msg_Err( p_playlist, "Save not implemented" );
}

#ifdef WIN32
#include <shellapi.h>
void PLModel::popupExplore()
{
    ShellExecuteW( NULL, L"explore", L"C:\\", NULL, NULL, SW_SHOWNORMAL );
}
#endif

/**********************************************************************
 * Playlist callbacks
 **********************************************************************/
static int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                            vlc_value_t oval, vlc_value_t nval, void *param )
{
    PLModel *p_model = (PLModel *) param;
    PLEvent *event = new PLEvent( PLUpdate_Type, 0 );
    QApplication::postEvent( p_model, static_cast<QEvent*>(event) );
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
//        p_model->b_need_update = VLC_TRUE;
//        return VLC_SUCCESS;
    }
    PLEvent *event = new PLEvent(  p_add );
    QApplication::postEvent( p_model, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

