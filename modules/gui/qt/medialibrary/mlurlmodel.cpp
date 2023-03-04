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

#include "mlurlmodel.hpp"

#include <QDateTime>

MLUrlModel::MLUrlModel(QObject *parent)
    : MLBaseModel(parent)
{
}

QVariant MLUrlModel::itemRoleData(MLItem *item, int role) const
{
    const MLUrl* ml_url = static_cast<MLUrl *>(item);
    if ( !ml_url )
        return QVariant();

    switch (role)
    {
    case URL_ID:
        return QVariant::fromValue( ml_url->getId() );
    case URL_URL:
        return QVariant::fromValue( ml_url->getUrl() );
    case URL_LAST_PLAYED_DATE :
        return QVariant::fromValue( ml_url->getLastPlayedDate() );
    default :
        return QVariant();
    }
}

QHash<int, QByteArray> MLUrlModel::roleNames() const
{
    return {
        { URL_ID, "id" },
        { URL_URL, "url" },
        { URL_LAST_PLAYED_DATE, "last_played_date" }
    };
}

void MLUrlModel::addAndPlay( const QString &url )
{
    struct Ctx{
        bool succeed = false;
        MLItemId itemId;
    };
    m_mediaLib->runOnMLThread<Ctx>(this,
    //ML thread
    [url](vlc_medialibrary_t* ml, Ctx& ctx){

        ml_unique_ptr<vlc_ml_media_t> s{vlc_ml_get_media_by_mrl( ml, qtu( url ))};
        if (!s)
            s.reset(vlc_ml_new_stream( ml, qtu( url ) ));
        if (!s)
            return;
        ctx.succeed = true;
        ctx.itemId = MLItemId( s->i_id, VLC_ML_PARENT_UNKNOWN );
    },
    //UI Thread
    [this](quint64, Ctx& ctx){
        if (!ctx.succeed)
            return;

        m_mediaLib->addAndPlay(ctx.itemId);
        emit resetRequested();
    });
}

vlc_ml_sorting_criteria_t MLUrlModel::roleToCriteria(int role) const
{
    switch (role) {
    case URL_URL :
        return VLC_ML_SORTING_DEFAULT;
    case URL_LAST_PLAYED_DATE :
        return VLC_ML_SORTING_LASTMODIFICATIONDATE;
    default:
        return VLC_ML_SORTING_DEFAULT;
    }
}

void MLUrlModel::onVlcMlEvent(const MLEvent &event)
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_MEDIA_UPDATED:
        case VLC_ML_EVENT_HISTORY_CHANGED:
            emit resetRequested();
            return;
    }
    MLBaseModel::onVlcMlEvent( event );
}

MLUrl::MLUrl(const vlc_ml_media_t *_data)
    : MLItem( MLItemId( _data->i_id, VLC_ML_PARENT_UNKNOWN ) )
    , m_url( _data->p_files->i_nb_items > 0 ? _data->p_files->p_items[0].psz_mrl : "" )
    , m_lastPlayedDate(
          QDateTime::fromSecsSinceEpoch( _data->i_last_played_date ).toString( QLocale::system().dateFormat( QLocale::ShortFormat ) )
          )
{
}

MLUrl::MLUrl(const MLUrl &url)
    : MLItem( url.getId() )
    , m_url( url.m_url )
    , m_lastPlayedDate( url.m_lastPlayedDate )
{
}

QString MLUrl::getUrl() const
{
    return m_url;
}

QString MLUrl::getLastPlayedDate() const
{
    return m_lastPlayedDate;
}

std::unique_ptr<MLBaseModel::BaseLoader>
MLUrlModel::createLoader() const
{
    return std::make_unique<Loader>(*this);
}

size_t MLUrlModel::Loader::count(vlc_medialibrary_t* ml) const
{
    MLQueryParams params = getParams();
    auto queryParams = params.toCQueryParams();

    return vlc_ml_count_stream_history( ml, &queryParams );
}

std::vector<std::unique_ptr<MLItem>>
MLUrlModel::Loader::load(vlc_medialibrary_t* ml, size_t index, size_t count) const
{
    MLQueryParams params = getParams(index, count);
    auto queryParams = params.toCQueryParams();

    ml_unique_ptr<vlc_ml_media_list_t> media_list;
    media_list.reset( vlc_ml_list_stream_history(ml, &queryParams) );
    if ( media_list == nullptr )
        return {};

    std::vector<std::unique_ptr<MLItem>> res;
    for( const vlc_ml_media_t& media: ml_range_iterate<vlc_ml_media_t>( media_list ) )
        res.emplace_back( std::make_unique<MLUrl>( &media ) );
    return res;
}

std::unique_ptr<MLItem>
MLUrlModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLUrl>(media.get());
}
