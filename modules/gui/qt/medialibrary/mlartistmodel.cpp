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

#include "mlartistmodel.hpp"

namespace {
    enum Roles
    {
        ARTIST_ID = Qt::UserRole + 1,
        ARTIST_NAME,
        ARTIST_SHORT_BIO,
        ARTIST_COVER,
        ARTIST_NB_ALBUMS
    };
}

QHash<QByteArray, vlc_ml_sorting_criteria_t> MLArtistModel::M_names_to_criteria = {
    {"title", VLC_ML_SORTING_ALPHA},
};

MLArtistModel::MLArtistModel(QObject *parent)
    : MLSlidingWindowModel(parent)
{
}

QVariant MLArtistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0)
        return QVariant();

    const MLArtist* ml_artist = item(static_cast<unsigned int>(index.row()));
    if ( !ml_artist )
        return QVariant();

    switch (role)
    {
    case ARTIST_ID :
        return QVariant::fromValue( ml_artist->getId() );
    case ARTIST_NAME :
        return QVariant::fromValue( ml_artist->getName() );
    case ARTIST_SHORT_BIO :
        return QVariant::fromValue( ml_artist->getShortBio() );
    case ARTIST_COVER :
        return QVariant::fromValue( ml_artist->getCover() );
    case ARTIST_NB_ALBUMS :
        return QVariant::fromValue( ml_artist->getNbAlbums() );
    default :
        return QVariant();
    }
}

QHash<int, QByteArray> MLArtistModel::roleNames() const
{
    return {
        { ARTIST_ID, "id" },
        { ARTIST_NAME, "name" },
        { ARTIST_SHORT_BIO, "short_bio" },
        { ARTIST_COVER, "cover" },
        { ARTIST_NB_ALBUMS, "nb_albums" },
    };
}

std::vector<std::unique_ptr<MLArtist>> MLArtistModel::fetch()
{
    ml_unique_ptr<vlc_ml_artist_list_t> artist_list;
    if ( m_parent.id <= 0 )
        artist_list.reset( vlc_ml_list_artists(m_ml, &m_query_param, false) );
    else
        artist_list.reset( vlc_ml_list_artist_of(m_ml, &m_query_param, m_parent.type, m_parent.id) );
    if ( artist_list == nullptr )
        return {};
    std::vector<std::unique_ptr<MLArtist>> res;
    for( const vlc_ml_artist_t& artist: ml_range_iterate<vlc_ml_artist_t>( artist_list ) )
        res.emplace_back( std::make_unique<MLArtist>( &artist ) );
    return res;
}

size_t MLArtistModel::countTotalElements() const
{
    auto queryParams = m_query_param;
    queryParams.i_offset = 0;
    queryParams.i_nbResults = 0;

    if ( m_parent.id <= 0 )
        return vlc_ml_count_artists(m_ml, &queryParams, false);
    return vlc_ml_count_artists_of(m_ml, &queryParams, m_parent.type, m_parent.id );
}

vlc_ml_sorting_criteria_t MLArtistModel::roleToCriteria(int role) const
{
    switch (role)
    {
    case ARTIST_NAME :
        return VLC_ML_SORTING_ALPHA;
    default :
        return VLC_ML_SORTING_DEFAULT;
    }
}

vlc_ml_sorting_criteria_t MLArtistModel::nameToCriteria(QByteArray name) const
{
    return M_names_to_criteria.value(name, VLC_ML_SORTING_DEFAULT);
}

QByteArray MLArtistModel::criteriaToName(vlc_ml_sorting_criteria_t criteria) const
{
    return M_names_to_criteria.key(criteria, "");
}

void MLArtistModel::onVlcMlEvent(const vlc_ml_event_t* event)
{
    switch (event->i_type)
    {
        case VLC_ML_EVENT_ARTIST_ADDED:
        case VLC_ML_EVENT_ARTIST_UPDATED:
        case VLC_ML_EVENT_ARTIST_DELETED:
            m_need_reset = true;
            break;
        case VLC_ML_EVENT_GENRE_DELETED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_GENRE &&
                 m_parent.id == event->deletion.i_entity_id )
                m_need_reset = true;
            break;
    }
    MLSlidingWindowModel::onVlcMlEvent(event);
}

void MLArtistModel::thumbnailUpdated(int idx)
{
    emit dataChanged(index(idx), index(idx), {ARTIST_COVER});
}
