/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#include "mlrecentsmodel.hpp"
#include <QDateTime>
#include "mlhelper.hpp"


MLRecentMedia::MLRecentMedia( const vlc_ml_media_t *media )
    : MLItem( MLItemId( media->i_id, VLC_ML_PARENT_UNKNOWN ) )
    , m_url ( media->p_files->i_nb_items > 0 ? media->p_files->p_items[0].psz_mrl : "" )
    , m_lastPlayedDate(QDateTime::fromSecsSinceEpoch( media->i_last_played_date ))
{
}

MLRecentMedia::MLRecentMedia( const MLRecentMedia& media )
    : MLItem(media.getId())
    , m_url(media.m_url)
    , m_lastPlayedDate(media.m_lastPlayedDate)
{
}

MLRecentsModel::MLRecentsModel( QObject* parent )
    : MLBaseModel( parent )
{
}

QVariant MLRecentsModel::itemRoleData(MLItem *item , int role ) const
{
    const MLRecentMedia* media = static_cast<MLRecentMedia *>(item);
    if ( !media )
        return QVariant();

    switch (role)
    {
    case RECENT_MEDIA_ID:
        return QVariant::fromValue( media->getId() );
    case RECENT_MEDIA_URL:
        return QVariant::fromValue( media->getUrl().toString(QUrl::PreferLocalFile | QUrl::RemovePassword));
    case RECENT_MEDIA_LAST_PLAYED_DATE:
        return QVariant::fromValue( media->getLastPlayedDate().toString( QLocale::system().dateFormat( QLocale::ShortFormat )));
    default :
        return QVariant();
    }
}

QHash<int, QByteArray> MLRecentsModel::roleNames() const
{
    return {
        { RECENT_MEDIA_ID, "id" },
        { RECENT_MEDIA_URL, "url" },
        { RECENT_MEDIA_LAST_PLAYED_DATE, "last_played_date" }
    };
}

void MLRecentsModel::clearHistory()
{
    m_mediaLib->runOnMLThread(this,
    //ML thread
    [](vlc_medialibrary_t* ml){
        vlc_ml_clear_history(ml);
    });
}

void MLRecentsModel::onVlcMlEvent( const MLEvent &event )
{
    switch ( event.i_type )
    {
        case VLC_ML_EVENT_HISTORY_CHANGED:
        {
            emit resetRequested();
            return;
        }
        default:
            break;
    }
    MLBaseModel::onVlcMlEvent( event );
}
void MLRecentsModel::setNumberOfItemsToShow( int n ){
    m_numberOfItemsToShow = n;
    invalidateCache();
}
int MLRecentsModel::getNumberOfItemsToShow() const {
    return m_numberOfItemsToShow;
}

std::unique_ptr<MLBaseModel::BaseLoader>
MLRecentsModel::createLoader() const
{
    return std::make_unique<Loader>(*this, m_numberOfItemsToShow);
}

MLRecentsModel::Loader::Loader(const MLRecentsModel &model, int numberOfItemsToShow)
    : BaseLoader(model)
    , m_numberOfItemsToShow(numberOfItemsToShow)
{
}

size_t MLRecentsModel::Loader::count(vlc_medialibrary_t* ml) const
{
    MLQueryParams params = getParams();
    auto queryParams = params.toCQueryParams();

    size_t realCount = vlc_ml_count_history( ml, &queryParams );
    if (m_numberOfItemsToShow >= 0)
        return std::min( realCount, static_cast<size_t>(m_numberOfItemsToShow) );
    return realCount;
}

std::vector<std::unique_ptr<MLItem>>
MLRecentsModel::Loader::load(vlc_medialibrary_t* ml, size_t index, size_t count) const
{
    MLQueryParams params = getParams(index, count);
    auto queryParams = params.toCQueryParams();

    std::vector<std::unique_ptr<MLItem>> res;
    if (m_numberOfItemsToShow >= 0)
    {
        if (queryParams.i_offset <= static_cast<uint32_t>(m_numberOfItemsToShow))
           queryParams.i_nbResults = static_cast<uint32_t>(m_numberOfItemsToShow) - queryParams.i_offset;
        else
            return res;
    }

    ml_unique_ptr<vlc_ml_media_list_t> media_list{ vlc_ml_list_history(
                ml, &queryParams ) };
    if ( media_list == nullptr )
        return {};
    for( vlc_ml_media_t &media: ml_range_iterate<vlc_ml_media_t>( media_list ) )
        res.emplace_back( std::make_unique<MLRecentMedia>( &media ) );
    return res;
}

std::unique_ptr<MLItem>
MLRecentsModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLRecentMedia>(media.get());
}
