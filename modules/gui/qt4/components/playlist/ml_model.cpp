/*****************************************************************************
 * ml_model.cpp: the SQL media library's model
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

#ifdef SQL_MEDIA_LIBRARY

#include <QUrl>
#include <QMenu>
#include <QMimeData>
#include <QApplication>
#include "ml_item.hpp"
#include "ml_model.hpp"
#include "dialogs/playlist.hpp"
#include "components/playlist/sorting.h"
#include "dialogs_provider.hpp"
#include "input_manager.hpp"                            /* THEMIM */
#include "util/qt_dirs.hpp"

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

/* Register ML Events */
const QEvent::Type MLEvent::MediaAdded_Type =
        (QEvent::Type)QEvent::registerEventType();
const QEvent::Type MLEvent::MediaRemoved_Type =
        (QEvent::Type)QEvent::registerEventType();
const QEvent::Type MLEvent::MediaUpdated_Type =
        (QEvent::Type)QEvent::registerEventType();

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

void MLModel::removeAll()
{
    vlc_array_t* p_where = vlc_array_new();
    if ( !p_where ) return;

    int rows = rowCount();
    if( rows > 0 )
    {
        beginRemoveRows( createIndex( 0, 0 ), 0, rows-1 );

        QList< MLItem* >::iterator it = items.begin();
        ml_element_t p_find[ items.count() ];
        int i = 0;
        while( it != items.end() )
        {
            p_find[i].criteria = ML_ID;
            p_find[i].value.i = (*it)->id( MLMEDIA_ID );
            vlc_array_append( p_where, & p_find[i++] );
            delete *it;
            it++;
        }

        ml_Delete( p_ml, p_where );
        items.clear();
        endRemoveRows();
    }

    vlc_array_destroy( p_where );
    reset();
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

void MLModel::filter( const QString& search_text, const QModelIndex & root, bool b_recursive )
{
    Q_UNUSED( search_text ); Q_UNUSED( root ); Q_UNUSED( b_recursive );
    /* FIXME */
}

void MLModel::sort( const int column, Qt::SortOrder order )
{
    Q_UNUSED( column ); Q_UNUSED( order );
}

/**
 * @brief Return the index of currently playing item
 */
QModelIndex MLModel::currentIndex() const
{
    input_thread_t *p_input_thread = THEMIM->getInput();
    if( !p_input_thread ) return QModelIndex();

    input_item_t* p_iitem = input_GetItem( p_input_thread );
    int i = 0;
    foreach( MLItem* item, items )
    {
        if ( item->inputItem() == p_iitem )
            return index( i, 0 );
        i++;
    }
    return QModelIndex();
}

QModelIndex MLModel::indexByPLID( const int i_plid, const int c ) const
{
    Q_UNUSED( i_plid ); Q_UNUSED( c );
    return QModelIndex(); /* FIXME ? */
}

QModelIndex MLModel::indexByInputItemID( const int i_inputitem_id, const int c ) const
{
    Q_UNUSED( c );
    foreach( MLItem* item, items )
        if ( item->id( INPUTITEM_ID ) == i_inputitem_id )
        {
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
        AbstractPLItem* item = static_cast<AbstractPLItem*>( idx.internalPointer() );
        urls.append( item->getURI() );
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

void MLModel::rebuild( playlist_item_t *p )
{
    Q_UNUSED( p );
    emit layoutChanged();
}

void MLModel::doDelete( QModelIndexList list )
{
    for (int i = 0; i < list.count(); ++i)
    {
        const QModelIndex &index = list.at( i );
        if ( !index.isValid() ) break;
        int id = itemId( list.at(i), MLMEDIA_ID );
        ml_DeleteSimple( p_ml, id );
        /* row will be removed by the lib callback */
    }
}

bool MLModel::removeRows( int row, int count, const QModelIndex &parent )
{
    /* !warn probably not a good idea to expose this as public method */
    if ( row < 0 || count < 0 ) return false;
    beginRemoveRows( parent, row, row );
    MLItem *item = items.takeAt( row );
    assert( item );
    if ( likely( item ) ) delete item;
    endRemoveRows();
    return true;
}

QVariant MLModel::data( const QModelIndex &index, const int role ) const
{
    if( index.isValid() )
    {
        if( role == Qt::DisplayRole || role == Qt::EditRole )
        {
            MLItem *it = static_cast<MLItem*>( index.internalPointer() );
            if( !it ) return QVariant();
            QVariant tmp = it->data( columnType( index.column() ) );
            return tmp;
        }
        else if( role == Qt::DecorationRole && index.column() == 0  )
        {
            /*
                FIXME: (see ml_type_e)
                media->type uses flags for media type information
            */
            return QVariant( icons[ getInputItem(index)->i_type ] );
        }
        else if( role == IsLeafNodeRole )
            return isLeaf( index );
        else if( role == IsCurrentsParentNodeRole )
            return isParent( index, currentIndex() );
        else if( role == IsCurrentRole )
        {
            return QVariant( isCurrent( index ) );
        }
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
 * @return a VLC error code
 */
int MLModel::insertMedia( ml_media_t *p_media, int row )
{
    // Some checks
    if( !p_media || row < -1 || row > rowCount() )
        return VLC_EGENERIC;

    if( row == -1 )
        row = rowCount();

    // Create and insert the item
    MLItem *item = new MLItem( p_intf, p_media, NULL );
    items.append( item );

    return VLC_SUCCESS;
}

/**
 * @brief Insert all medias from a result array to the model
 * @param p_result_array the medias to append
 * @return see insertMedia
 */
int MLModel::insertResultArray( vlc_array_t *p_result_array, int row )
{
    int i_ok = VLC_SUCCESS;
    int count = vlc_array_count( p_result_array );

    if( !count )
        return i_ok;

    emit layoutAboutToBeChanged();
    if( row == -1 )
        row = rowCount();

    // Signal Qt that we will insert rows
    beginInsertRows( createIndex( -1, -1 ), row, row + count-1 );

    // Loop and insert
    for( int i = 0; i < count; ++i )
    {
        ml_result_t *p_result = (ml_result_t*)
                        vlc_array_item_at_index( p_result_array, i );
        if( !p_result || p_result->type != ML_TYPE_MEDIA )
            continue;
        i_ok = insertMedia( p_result->value.p_media, row + i );
        if( i_ok != VLC_SUCCESS )
            break;
    }
    // Signal we're done
    endInsertRows();

    emit layoutChanged();

    return i_ok;
}

bool MLModel::event( QEvent *event )
{
    if ( event->type() == MLEvent::MediaAdded_Type )
    {
        event->accept();
        MLEvent *e = static_cast<MLEvent *>(event);
        vlc_array_t* p_result = vlc_array_new();
        if ( ml_FindMedia( e->p_ml, p_result, ML_ID, e->ml_media_id ) == VLC_SUCCESS )
        {
            insertResultArray( p_result );
            ml_DestroyResultArray( p_result );
        }
        vlc_array_destroy( p_result );
        return true;
    }
    else if( event->type() == MLEvent::MediaRemoved_Type )
    {
        event->accept();
        MLEvent *e = static_cast<MLEvent *>(event);
        removeRow( getIndexByMLID( e->ml_media_id ).row() );
        return true;
    }
    else if( event->type() == MLEvent::MediaUpdated_Type )
    {
        event->accept();
        /* Never implemented... */
        return true;
    }

    return VLCModel::event( event );
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
    playlist_t *p_playlist = THEPL;
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

void MLModel::activateItem( const QModelIndex &idx )
{
    if( !idx.isValid() ) return;
    AddItemToPlaylist( itemId( idx, MLMEDIA_ID ), true, p_ml, true );
}

bool MLModel::action( QAction *action, const QModelIndexList &indexes )
{
    actionsContainerType a = action->data().value<actionsContainerType>();
    input_item_t *p_input;

    switch ( a.action )
    {

    case ACTION_PLAY:
        if ( ! indexes.empty() && indexes.first().isValid() )
        {
            activateItem( indexes.first() );
            return true;
        }
        break;

    case ACTION_ADDTOPLAYLIST:
        foreach( const QModelIndex &index, indexes )
        {
            if( !index.isValid() ) return false;
            AddItemToPlaylist( itemId( index, MLMEDIA_ID ), false, p_ml, true );
        }
        return true;

    case ACTION_REMOVE:
        doDelete( indexes );
        return true;

    case ACTION_SORT:
        break;

    case ACTION_CLEAR:
        removeAll();
        return true;

    case ACTION_ENQUEUEFILE:
        foreach( const QString &uri, a.uris )
            playlist_Add( THEPL, uri.toAscii().constData(),
                          NULL, PLAYLIST_APPEND | PLAYLIST_PREPARSE,
                          PLAYLIST_END, false, pl_Unlocked );
        return true;

    case ACTION_ENQUEUEDIR:
        if( a.uris.isEmpty() ) return false;
        p_input = input_item_New( a.uris.first().toAscii().constData(), NULL );
        if( unlikely( p_input == NULL ) ) return false;

        /* FIXME: playlist_AddInput() can fail */
        playlist_AddInput( THEPL, p_input,
                           PLAYLIST_APPEND,
                           PLAYLIST_END, true, pl_Unlocked );
        vlc_gc_decref( p_input );
        return true;

    case ACTION_ENQUEUEGENERIC:
        foreach( const QString &uri, a.uris )
        {
            p_input = input_item_New( qtu( uri ), NULL );
            /* Insert options */
            foreach( const QString &option, a.options.split( " :" ) )
            {
                QString temp = colon_unescape( option );
                if( !temp.isEmpty() )
                    input_item_AddOption( p_input, qtu( temp ),
                                          VLC_INPUT_OPTION_TRUSTED );
            }

            /* FIXME: playlist_AddInput() can fail */
            playlist_AddInput( THEPL, p_input,
                    PLAYLIST_APPEND | PLAYLIST_PREPARSE,
                    PLAYLIST_END, false, pl_Unlocked );
            vlc_gc_decref( p_input );
        }
        return true;

    default:
        break;
    }
    return false;
}

bool MLModel::isSupportedAction( actions action, const QModelIndex &index ) const
{
    switch ( action )
    {
    case ACTION_ADDTOPLAYLIST:
        return index.isValid();
    case ACTION_SORT:
        return false;
    case ACTION_PLAY:
    case ACTION_STREAM:
    case ACTION_SAVE:
    case ACTION_INFO:
    case ACTION_REMOVE:
        return index.isValid();
    case ACTION_EXPLORE:
        if( index.isValid() )
            return getURI( index ).startsWith( "file://" );
    case ACTION_CREATENODE:
        return false;
    case ACTION_CLEAR:
        return rowCount() && canEdit();
    case ACTION_ENQUEUEFILE:
    case ACTION_ENQUEUEDIR:
    case ACTION_ENQUEUEGENERIC:
        return canEdit();
    default:
        return false;
    }
    return false;
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

QModelIndex MLModel::getIndexByMLID( int id ) const
{
    for( int i = 0; i < rowCount( ); i++ )
    {
        QModelIndex idx = index( i, 0 );
        MLItem *item = static_cast< MLItem* >( idx.internalPointer() );
        if( item->id( MLMEDIA_ID ) == id ) return idx;
    }

    return QModelIndex();
}

bool MLModel::isParent( const QModelIndex &index, const QModelIndex &current) const
{
    Q_UNUSED( index ); Q_UNUSED( current );
    return false;
}

bool MLModel::isLeaf( const QModelIndex &index ) const
{
    Q_UNUSED( index );
    return true;
}

/* ML Callbacks handling */

inline void postMLEvent( vlc_object_t *p_this, QEvent::Type type, void *data, int32_t i )
{
    MLModel* p_model = static_cast<MLModel*>(data);
    media_library_t *p_ml = (media_library_t *) p_this;
    QApplication::postEvent( p_model, new MLEvent( type, p_ml, i ) );
}

static int mediaAdded( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data )
{
    VLC_UNUSED( psz_var ); VLC_UNUSED( oldval );
    postMLEvent( p_this, MLEvent::MediaAdded_Type, data, newval.i_int );
    return VLC_SUCCESS;
}

static int mediaDeleted( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data )
{
    VLC_UNUSED( psz_var ); VLC_UNUSED( oldval );
    postMLEvent( p_this, MLEvent::MediaRemoved_Type, data, newval.i_int );
    return VLC_SUCCESS;
}

static int mediaUpdated( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data )
{
    VLC_UNUSED( psz_var ); VLC_UNUSED( oldval );
    postMLEvent( p_this, MLEvent::MediaUpdated_Type, data, newval.i_int );
    return VLC_SUCCESS;
}

#endif
