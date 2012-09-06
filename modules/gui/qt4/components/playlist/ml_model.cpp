/*****************************************************************************
 * ml_model.cpp: the media library's model
 *****************************************************************************
 * Copyright (C) 2008-2011 the VideoLAN Team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
 *          Srikanth Raju <srikiraju#gmail#com>
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
# include <config.h>
#endif

#ifdef MEDIA_LIBRARY

#include <QUrl>
#include <QMenu>
#include <QMimeData>
#include "ml_item.hpp"
#include "ml_model.hpp"
#include "dialogs/playlist.hpp"
#include "components/playlist/sorting.h"
#include "dialogs_provider.hpp"
#include "input_manager.hpp"                            /* THEMIM */

#include <assert.h>
#include <vlc_intf_strings.h>

static int mediaAdded( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data );
static int mediaDeleted( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data );
static int mediaUpdated( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data );

/**
 * @brief Definition of the result item model for the result tree
 * @param parent the parent Qt object
 */
MLModel::MLModel( intf_thread_t* _p_intf, QObject *parent )
        :VLCModel( _p_intf, parent )
{
    p_ml = ml_Get( p_intf );
    if ( !p_ml ) return;

    vlc_array_t *p_result_array = vlc_array_new();
    if ( p_result_array )
    {
        ml_Find( p_ml, p_result_array, ML_MEDIA );
        insertResultArray( p_result_array );
        ml_DestroyResultArray( p_result_array );
        vlc_array_destroy( p_result_array );
    }

    var_AddCallback( p_ml, "media-added", mediaAdded, this );
    var_AddCallback( p_ml, "media-deleted", mediaDeleted, this );
    var_AddCallback( p_ml, "media-meta-change", mediaUpdated, this );
}

/**
 * @brief Simple destructor for the model
 */
MLModel::~MLModel()
{
    if ( !p_ml ) return;
    var_DelCallback( p_ml, "media-meta-change", mediaUpdated, this );
    var_DelCallback( p_ml, "media-deleted", mediaDeleted, this );
    var_DelCallback( p_ml, "media-added", mediaAdded, this );
}

void MLModel::clear()
{
    int rows = rowCount();
    if( rows > 0 )
    {
        beginRemoveRows( createIndex( 0, 0 ), 0, rows-1 );
        items.clear();
        endRemoveRows();
        emit layoutChanged();
    }
}

QModelIndex MLModel::index( int row, int column,
                                  const QModelIndex &parent ) const
{
    if( parent.isValid() || row >= items.count() )
        return QModelIndex();
    else
    {
        QModelIndex idx = createIndex( row, column, items.value( row ) );
        return idx;
    }
}

QModelIndex MLModel::parent(const QModelIndex & ) const
{
    return QModelIndex();
}

/**
 * @brief Return the index of currently playing item
 */
QModelIndex MLModel::currentIndex() const
{
    input_thread_t *p_input_thread = THEMIM->getInput();
    if( !p_input_thread ) return QModelIndex();

    /*TODO: O(n) not good */
    input_item_t* p_iitem = input_GetItem( p_input_thread );
    foreach( MLItem* item, items )
    {
        if( !QString::compare( item->getUri().toString(),
                    QString::fromAscii( p_iitem->psz_uri ) ) )
            return index( items.indexOf( item ), 0 );
    }
    return QModelIndex();
}
/**
 * @brief This returns the type of data shown in the specified column
 * @param column must be valid
 * @return The type, or ML_END in case of error
 */
ml_select_e MLModel::columnType( int logicalindex ) const
{
    if( logicalindex < 0 || logicalindex >= columnCount() ) return ML_END;
    return meta_to_mlmeta( columnToMeta( logicalindex ) );
}

QVariant MLModel::headerData( int section, Qt::Orientation orientation,
                                    int role ) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return QVariant( qfu( psz_column_title( columnToMeta( section ) ) ) );
    else
        return QVariant();
}

Qt::ItemFlags MLModel::flags(const QModelIndex &index) const
{
    if( !index.isValid() )
        return 0;

    if( isEditable( index ) )
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
                | Qt::ItemIsEditable;
    else
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

bool MLModel::isEditable( const QModelIndex &index ) const
{
    if( !index.isValid() )
        return false;

    ml_select_e type = columnType( index.column() );
    switch( type )
    {
    // Read-only members: not editable
    case ML_ALBUM_ID:
    case ML_ARTIST_ID:
    case ML_DURATION:
    case ML_ID:
    case ML_LAST_PLAYED:
    case ML_PLAYED_COUNT:
    case ML_TYPE:
        return false;
    // Read-write members: editable
    case ML_ALBUM:
    case ML_ARTIST:
    case ML_COVER:
    case ML_EXTRA:
    case ML_GENRE:
    case ML_ORIGINAL_TITLE:
    // case ML_ROLE:
    case ML_SCORE:
    case ML_TITLE:
    case ML_TRACK_NUMBER:
    case ML_URI:
    case ML_VOTE:
    case ML_YEAR:
        return true;
    default:
        return false;
    }
}

QMimeData* MLModel::mimeData( const QModelIndexList &indexes ) const
{
    QList< QUrl > urls;
    QList< int > rows;
    foreach( QModelIndex idx, indexes )
    {
        if( rows.contains( idx.row() ) )
            continue;
        rows.append( idx.row() );
        MLItem* item = static_cast<MLItem*>( idx.internalPointer() );
        urls.append( item->getUri() );
    }
    QMimeData *data = new QMimeData;
    data->setUrls( urls );
    return data;
}

int MLModel::rowCount( const QModelIndex & parent ) const
{
    if( !parent.isValid() )
        return items.count();
    return 0;
}

void MLModel::remove( MLItem *item )
{
    int row = items.indexOf( item );
    remove( createIndex( row, 0 ) );
}

void MLModel::doDelete( QModelIndexList list )
{
    for (int i = 0; i < list.count(); ++i)
    {
        int id = itemId( list.at(i) );
        ml_DeleteSimple( p_ml, id );
    }
}

void MLModel::remove( QModelIndex idx )
{
    if( !idx.isValid() )
        return;
    else
    {
        beginRemoveRows( createIndex( 0, 0 ), idx.row(), idx.row() );
        items.removeAt( idx.row() );
        endRemoveRows();
    }
}

int MLModel::itemId( const QModelIndex &index ) const
{
    return getItem( index )->id();
}

input_item_t * MLModel::getInputItem( const QModelIndex &index ) const
{
    return getItem( index )->inputItem();
}

QVariant MLModel::data( const QModelIndex &index, const int role ) const
{
    if( index.isValid() )
    {
        if( role == Qt::DisplayRole || role == Qt::EditRole )
        {
            MLItem *it = static_cast<MLItem*>( index.internalPointer() );
            if( !it ) return QVariant();
            QVariant tmp = it->data( index.column() );
            return tmp;
        }
        else if( role == IsLeafNodeRole )
            return QVariant( true );
        else if( role == IsCurrentsParentNodeRole )
            return QVariant( false );
    }
    return QVariant();
}

bool MLModel::setData( const QModelIndex &idx, const QVariant &value,
                             int role )
{
    if( role != Qt::EditRole || !idx.isValid() ) return false;
    MLItem *media = static_cast<MLItem*>( idx.internalPointer() );
    media->setData( columnType( idx.column() ), value );
    emit dataChanged( idx, idx );
    return true;
}

/**
 * @brief Insert a media to the model in a given row
 * @param p_media the media to append
 * @param row the future row for this media, -1 to append at the end
 * @param bSignal signal Qt that the model has been modified,
 *                should NOT be used by the user
 * @return a VLC error code
 */
int MLModel::insertMedia( ml_media_t *p_media, int row,
                                bool bSignal )
{
    // Some checks
    if( !p_media || row < -1 || row > rowCount() )
        return VLC_EGENERIC;

    if( row == -1 )
        row = rowCount();

    if( bSignal )
        beginInsertRows( createIndex( -1, -1 ), row, row );

    // Create and insert the item
    MLItem *item = new MLItem( this, p_intf, p_media, NULL );
    items.append( item );

    if( bSignal )
        endInsertRows();

    return VLC_SUCCESS;
}

/**
 * @brief Append a media to the model
 * @param p_media the media to append
 * @return see insertMedia
 * @note Always signals. Do not use in a loop.
 */
int MLModel::appendMedia( ml_media_t *p_media )
{
    return insertMedia( p_media, -1, true );
}

/**
 * @brief Insert all medias from an array to the model
 * @param p_media_array the medias to append
 * @return see insertMedia
 * @note if bSignal==true, then it signals only once
 */
int MLModel::insertMediaArray( vlc_array_t *p_media_array,
                                     int row, bool bSignal )
{
    int i_ok = VLC_SUCCESS;
    int count = vlc_array_count( p_media_array );

    if( !count )
        return i_ok;

    if( row == -1 )
        row = rowCount();

    // Signal Qt that we will insert rows
    if( bSignal )
        beginInsertRows( createIndex( -1, -1 ), row, row + count-1 );

    // Loop
    for( int i = 0; i < count; ++i )
    {
        i_ok = insertMedia( (ml_media_t*)
            vlc_array_item_at_index( p_media_array, i ), row + i, false );
        if( i_ok != VLC_SUCCESS )
            break;
    }

    if( bSignal )
        endInsertRows();

    return i_ok;
}

/**
 * @brief Insert the media contained in a result to the model
 * @param p_result the media to append is p_result->value.p_media
 * @param row the future row for this media
 * @param bSignal signal Qt that the model has been modified,
 *                should NOT be used by the user
 * @return a VLC error code
 */
int MLModel::insertResult( const ml_result_t *p_result, int row,
                                 bool bSignal )
{
    if( !p_result || p_result->type != ML_TYPE_MEDIA )
        return VLC_EGENERIC;
    else
        return insertMedia( p_result->value.p_media, row, bSignal );
}

/**
 * @brief Append the media contained in a result to the model
 * @param p_result the media to append is p_result->value.p_media
 * @param row the future row for this media
 * @return a VLC error code
 * @note Always signals. Do not use in a loop.
 */
inline int MLModel::appendResult( const ml_result_t *p_result )
{
    return insertResult( p_result, -1, true );
}

/**
 * @brief Insert all medias from a result array to the model
 * @param p_result_array the medias to append
 * @return see insertMedia
 * @note if bSignal==true, then it signals only once
 *       not media or NULL items are skipped
 */
int MLModel::insertResultArray( vlc_array_t *p_result_array,
                                      int row, bool bSignal )
{
    int i_ok = VLC_SUCCESS;
    int count = vlc_array_count( p_result_array );

    if( !count )
        return i_ok;

    if( row == -1 )
        row = rowCount();

    // Signal Qt that we will insert rows
    if( bSignal )
        beginInsertRows( createIndex( -1, -1 ), row, row + count-1 );

    // Loop and insert
    for( int i = 0; i < count; ++i )
    {
        ml_result_t *p_result = (ml_result_t*)
                        vlc_array_item_at_index( p_result_array, i );
        if( !p_result || p_result->type != ML_TYPE_MEDIA )
            continue;
        i_ok = insertMedia( p_result->value.p_media, row + i, false );
        if( i_ok != VLC_SUCCESS )
            break;
    }
    // Signal we're done
    if( bSignal )
        endInsertRows();

    return i_ok;
}

/** **************************************************************************
 * \brief Add a media to the playlist
 *
 * \param id the item id
 * @todo this code must definitely be done by the ML core
 *****************************************************************************/
static void AddItemToPlaylist( int i_media_id, bool bPlay, media_library_t* p_ml,
        bool bRenew )
{

    input_item_t *p_item = ml_CreateInputItem( p_ml, i_media_id );
    if( !p_item )
    {
        msg_Dbg( p_ml, "unable to create input item for media %d",
                 i_media_id );
        return;
    }
    playlist_t *p_playlist = pl_Get( p_ml );
    playlist_item_t *p_playlist_item = NULL;

    playlist_Lock( p_playlist );
    if( !bRenew )
    {
        p_playlist_item = playlist_ItemGetByInput( p_playlist, p_item );
    }

    if( !p_playlist_item || p_playlist_item->i_id == 1 )
    {
        playlist_AddInput( p_playlist, p_item,
                           PLAYLIST_APPEND,
                           PLAYLIST_END, true, true );

        p_playlist_item = playlist_ItemGetByInput( p_playlist, p_item );
    }
    playlist_Unlock( p_playlist );

    if( !p_playlist_item || p_playlist_item->i_id == 1 )
    {
        msg_Dbg( p_ml, "could not find playlist item %s (%s:%d)",
                 p_item->psz_name, __FILE__, __LINE__ );
        return;
    }

    /* Auto play item */
    if( bPlay ) // || p_playlist->status.i_status == PLAYLIST_STOPPED )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, false,
                          NULL, p_playlist_item );
    }
    vlc_gc_decref( p_item );
}

void MLModel::activateItem( const QModelIndex &index )
{
    play( index );
}

void MLModel::play( const QModelIndex &idx )
{
    if( !idx.isValid() )
        return;
    MLItem *item = static_cast< MLItem* >( idx.internalPointer() );
    if( !item )
        return;
    AddItemToPlaylist( item->id(), true, p_ml, true );
}

QString MLModel::getURI( const QModelIndex &index ) const
{
    return QString();
}

void MLModel::actionSlot( QAction *action )
{
    QString name;
    QStringList mrls;
    QModelIndex index;
    playlist_item_t *p_item;

    actionsContainerType a = action->data().value<actionsContainerType>();
    switch ( a.action )
    {

    case actionsContainerType::ACTION_PLAY:
        play( a.indexes.first() );
        break;

    case actionsContainerType::ACTION_ADDTOPLAYLIST:
        break;

    case actionsContainerType::ACTION_REMOVE:
        doDelete( a.indexes );
        break;

    case actionsContainerType::ACTION_SORT:
        break;
    }
}

QModelIndex MLModel::rootIndex() const
{
    // FIXME
    return QModelIndex();
}

bool MLModel::isTree() const
{
    // FIXME ?
    return false;
}

bool MLModel::canEdit() const
{
    /* can always insert */
    return true;
}

bool MLModel::isCurrentItem( const QModelIndex &index, playLocation where ) const
{
    Q_UNUSED( index );
    if ( where == IN_MEDIALIBRARY )
        return true;
    return false;
}

QModelIndex MLModel::getIndexByMLID( int id ) const
{
    for( int i = 0; i < rowCount( ); i++ )
    {
        QModelIndex idx = index( i, 0 );
        MLItem *item = static_cast< MLItem* >( idx.internalPointer() );
        if( item->id() == id )
            return idx;
    }
    return QModelIndex();
}

static int mediaAdded( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data )
{
    VLC_UNUSED( psz_var ); VLC_UNUSED( oldval );

    int ret = VLC_SUCCESS;
    media_library_t *p_ml = ( media_library_t* )p_this;
    MLModel* p_model = ( MLModel* )data;
    vlc_array_t* p_result = vlc_array_new();
    ret = ml_FindMedia( p_ml, p_result, ML_ID, newval.i_int );
    if( ret != VLC_SUCCESS )
    {
        vlc_array_destroy( p_result );
        return VLC_EGENERIC;
    }
    p_model->insertResultArray( p_result );
    ml_DestroyResultArray( p_result );
    vlc_array_destroy( p_result );
    return VLC_SUCCESS;
}

static int mediaDeleted( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_var ); VLC_UNUSED( oldval );

    MLModel* p_model = ( MLModel* )data;
    QModelIndex remove_idx = p_model->getIndexByMLID( newval.i_int );
    if( remove_idx.isValid() )
        p_model->remove( remove_idx );
    return VLC_SUCCESS;
}

static int mediaUpdated( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_var ); VLC_UNUSED( oldval );
    VLC_UNUSED( newval ); VLC_UNUSED( data );

    return VLC_SUCCESS;
}

#endif
