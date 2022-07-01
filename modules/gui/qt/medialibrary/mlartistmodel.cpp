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

QHash<QByteArray, vlc_ml_sorting_criteria_t> MLArtistModel::M_names_to_criteria = {
    {"title", VLC_ML_SORTING_ALPHA},
};

MLArtistModel::MLArtistModel(QObject *parent)
    : MLBaseModel(parent)
{
}

QVariant MLArtistModel::itemRoleData(MLItem *item, const int role) const
{
    const MLArtist* ml_artist = static_cast<MLArtist *>(item);
    assert( ml_artist );

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
    case ARTIST_NB_TRACKS :
        return QVariant::fromValue( ml_artist->getNbTracks() );
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
        { ARTIST_NB_TRACKS, "nb_tracks" }
    };
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

void MLArtistModel::onVlcMlEvent(const MLEvent &event)
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_ARTIST_ADDED:
            emit resetRequested();
            return;
        case VLC_ML_EVENT_ARTIST_UPDATED:
        {
            MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            updateItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_ARTIST_DELETED:
        {
            MLItemId itemId(event.deletion.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            deleteItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_GENRE_DELETED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_GENRE &&
                 m_parent.id == event.deletion.i_entity_id )
                emit resetRequested();
            return;
    }

    MLBaseModel::onVlcMlEvent(event);
}

std::unique_ptr<MLBaseModel::BaseLoader>
MLArtistModel::createLoader() const
{
    return std::make_unique<Loader>(*this);
}

size_t MLArtistModel::Loader::count(vlc_medialibrary_t* ml) const
{
    MLQueryParams params = getParams();
    auto queryParams = params.toCQueryParams();

    if ( m_parent.id <= 0 )
        return vlc_ml_count_artists(ml, &queryParams, false);
    return vlc_ml_count_artists_of(ml, &queryParams, m_parent.type, m_parent.id );
}

std::vector<std::unique_ptr<MLItem>>
MLArtistModel::Loader::load(vlc_medialibrary_t* ml, size_t index, size_t count) const
{
    MLQueryParams params = getParams(index, count);
    auto queryParams = params.toCQueryParams();

    ml_unique_ptr<vlc_ml_artist_list_t> artist_list;
    if ( m_parent.id <= 0 )
        artist_list.reset( vlc_ml_list_artists(ml, &queryParams, false) );
    else
        artist_list.reset( vlc_ml_list_artist_of(ml, &queryParams, m_parent.type, m_parent.id) );
    if ( artist_list == nullptr )
        return {};
    std::vector<std::unique_ptr<MLItem>> res;
    for( const vlc_ml_artist_t& artist: ml_range_iterate<vlc_ml_artist_t>( artist_list ) )
        res.emplace_back( std::make_unique<MLArtist>( &artist ) );
    return res;
}

std::unique_ptr<MLItem>
MLArtistModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_ARTIST);
    ml_unique_ptr<vlc_ml_artist_t> artist(vlc_ml_get_artist(ml, itemId.id));
    if (!artist)
        return nullptr;
    return std::make_unique<MLArtist>(artist.get());
}

