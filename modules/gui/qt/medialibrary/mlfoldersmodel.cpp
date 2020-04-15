/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "mlfoldersmodel.hpp"
#include <cassert>

MlFoldersModel::MlFoldersModel( QObject *parent )
    : QAbstractListModel( parent )
    , m_ml_event_handle( nullptr , [this](vlc_ml_event_callback_t* cb ) {
        if ( m_ml )
            vlc_ml_event_unregister_callback( m_ml , cb );
    })
{
    connect( this , &MlFoldersModel::onMLEntryPointModified , this , &MlFoldersModel::update );
}

MlFoldersModel::EntryPoint::EntryPoint( const vlc_ml_entry_point_t& entryPoint)
    : mrl(entryPoint.psz_mrl)
    , banned(entryPoint.b_banned)
{
}

void MlFoldersModel::setCtx(QmlMainContext *ctx)
{
    if (ctx)
    {
        m_ctx = ctx;
        setMl(vlc_ml_instance_get( m_ctx->getIntf() ));
    }
    else
    {
        m_ctx = nullptr;
        setMl(nullptr);
    }
    emit ctxChanged();
}

void MlFoldersModel::setMl(vlc_medialibrary_t *ml)
{
    if (ml)
        m_ml_event_handle.reset( vlc_ml_event_register_callback( ml , onMlEvent , this ) );
    else
        m_ml_event_handle.reset( nullptr );
    m_ml = ml;
    update();
}

int MlFoldersModel::rowCount( QModelIndex const & ) const
{
    return static_cast<int>(m_mrls.size());
}

int MlFoldersModel::columnCount( QModelIndex const & ) const
{
    return 3;
}

QVariant MlFoldersModel::data( const QModelIndex &index ,
                              int role) const {
    if ( index.isValid() )
    {
        switch ( role )
        {
        case Qt::DisplayRole :
        {
            if ( index.column() != 1 )
                return {};
            QUrl url = QUrl::fromUserInput(m_mrls[index.row()].mrl);
            if (!url.isValid())
                return {};
            return QVariant::fromValue( url.toDisplayString( QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::NormalizePathSegments ) );
        }
        case DisplayUrl:
        {
            QUrl url = QUrl::fromUserInput(m_mrls[index.row()].mrl);
            if (!url.isValid())
                return {};
            return QVariant::fromValue( url.toDisplayString( QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::NormalizePathSegments ) );
        }
        case Banned:
            return m_mrls[index.row()].banned;
        default :
            return {};
        }
    }
    return {};
}

void MlFoldersModel::removeAt( int index )
{
    assert(index  < static_cast<int>(m_mrls.size()));
    vlc_ml_remove_folder( m_ml , qtu( m_mrls[index].mrl ) );
}

void MlFoldersModel::add( QUrl mrl )
{
    vlc_ml_add_folder( m_ml , qtu( mrl.toString( QUrl::None ) ) );
}

void MlFoldersModel::update()
{
    beginResetModel();

    m_mrls.clear();

    vlc_ml_entry_point_list_t * entrypoints;
    vlc_ml_list_folder( m_ml , &entrypoints ); //TODO: get list of banned folders as well

    for ( unsigned int i=0 ; i<entrypoints->i_nb_items ; i++ )
        m_mrls.emplace_back( entrypoints->p_items[i] );

    vlc_ml_release(entrypoints);

    endResetModel();
}

Qt::ItemFlags MlFoldersModel::flags ( const QModelIndex & index ) const {
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags( index );
    if ( index.isValid() ){
        return defaultFlags;
    }
    return defaultFlags;
}

QHash<int, QByteArray> MlFoldersModel::roleNames() const
{
    return {
        {DisplayUrl, "display_url"},
        {Banned, "banned"},
    };
}

bool MlFoldersModel::setData( const QModelIndex &index ,
                                const QVariant &value , int role){
    if( !index.isValid() )
        return false;

    else if( role == Banned ){
        if( !value.toBool() ){
            vlc_ml_unban_folder(m_ml, qtu( m_mrls[index.row()].mrl ) );
        }
        else{
            vlc_ml_ban_folder( m_ml , qtu( m_mrls[index.row()].mrl ) );
        }
    }

    return true;
}
void MlFoldersModel::onMlEvent( void* data , const vlc_ml_event_t* event )
{
    auto self = static_cast<MlFoldersModel*>( data );
    if ( event->i_type == VLC_ML_EVENT_ENTRY_POINT_ADDED || event->i_type == VLC_ML_EVENT_ENTRY_POINT_REMOVED ||
         event->i_type == VLC_ML_EVENT_ENTRY_POINT_UNBANNED || event->i_type == VLC_ML_EVENT_ENTRY_POINT_BANNED  )
    {
        emit self->onMLEntryPointModified( QPrivateSignal() );
    }
}

 QVariant MlFoldersModel::headerData( int section , Qt::Orientation orientation , int /*role*/) const
 {
     if ( orientation == Qt::Horizontal ) {
         switch ( section ) {
             case 0:
                 return qtr("Banned");

             case 1:
                 return qtr("Path");

             case 2:
                 return qtr("Remove");

             default:
                 return qtr("Unknown");
         }
     }
     return QVariant();
 }

