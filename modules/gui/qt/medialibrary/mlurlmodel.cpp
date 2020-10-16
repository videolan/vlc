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
    : MLSlidingWindowModel<MLUrl>(parent)
{
}

QVariant MLUrlModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0)
        return QVariant();

    const MLUrl* ml_url = item(static_cast<unsigned int>(index.row()));
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
    QMetaObject::invokeMethod(  this
                              , [this, url]() {
        ml_unique_ptr<vlc_ml_media_t> s{vlc_ml_get_media_by_mrl( m_ml, qtu( url ))};
        if (!s) {
            s.reset(vlc_ml_new_stream( m_ml, qtu( url ) ));
        }
        if (!s)
            return;
        MLParentId itemId( s->i_id, VLC_ML_PARENT_UNKNOWN );
        m_mediaLib->addAndPlay(itemId);
        emit resetRequested();
    });
}

size_t MLUrlModel::countTotalElements() const
{
    auto queryParams = m_query_param;
    queryParams.i_offset = 0;
    queryParams.i_nbResults = 0;
    auto s = vlc_ml_count_stream_history( m_ml, &queryParams );
    return s;
}

std::vector<std::unique_ptr<MLUrl>> MLUrlModel::fetch() const
{
    ml_unique_ptr<vlc_ml_media_list_t> media_list;
    media_list.reset( vlc_ml_list_stream_history(m_ml, &m_query_param) );
    if ( media_list == nullptr )
        return {};

    std::vector<std::unique_ptr<MLUrl>> res;
    for( const vlc_ml_media_t& media: ml_range_iterate<vlc_ml_media_t>( media_list ) )
        res.emplace_back( std::make_unique<MLUrl>( &media ) );
    return res;
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
            m_need_reset = true;
            break;
    }
    MLBaseModel::onVlcMlEvent( event );
}

MLUrl::MLUrl(const vlc_ml_media_t *_data)
    : m_id( _data->i_id, VLC_ML_PARENT_UNKNOWN )
    , m_url( _data->p_files->i_nb_items > 0 ? _data->p_files->p_items[0].psz_mrl : "" )
    , m_lastPlayedDate(
          QDateTime::fromTime_t( _data->i_last_played_date ).toString( QLocale::system().dateFormat( QLocale::ShortFormat ) )
          )
{
}

MLUrl::MLUrl(const MLUrl &url)
    : m_id( url.m_id )
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

MLUrl *MLUrl::clone() const {
    return new MLUrl( *this );
}
