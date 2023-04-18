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

#include "mlalbummodel.hpp"

#include "util/vlctick.hpp"

QHash<QByteArray, vlc_ml_sorting_criteria_t> MLAlbumModel::M_names_to_criteria = {
    {"id", VLC_ML_SORTING_DEFAULT},
    {"title", VLC_ML_SORTING_ALBUM},
    {"release_year", VLC_ML_SORTING_RELEASEDATE},
    {"main_artist", VLC_ML_SORTING_ARTIST},
    //{"nb_tracks"},
    {"duration", VLC_ML_SORTING_DURATION}
};

MLAlbumModel::MLAlbumModel(QObject *parent)
    : MLBaseModel(parent)
{
}

QHash<int, QByteArray> MLAlbumModel::roleNames() const
{
    return {
        {ALBUM_ID,"id"},
        {ALBUM_TITLE, "title"},
        {ALBUM_RELEASE_YEAR, "release_year"},
        {ALBUM_SHORT_SUMMARY, "shortsummary"},
        {ALBUM_COVER, "cover"},
        {ALBUM_MAIN_ARTIST, "main_artist"},
        {ALBUM_NB_TRACKS, "nb_tracks"},
        {ALBUM_DURATION, "duration"},
        {ALBUM_TITLE_FIRST_SYMBOL, "title_first_symbol"},
        {ALBUM_MAIN_ARTIST_FIRST_SYMBOL, "main_artist_first_symbol"}
    };
}

vlc_ml_sorting_criteria_t MLAlbumModel::nameToCriteria(QByteArray name) const
{
    return M_names_to_criteria.value(name, VLC_ML_SORTING_DEFAULT);
}

QByteArray MLAlbumModel::criteriaToName(vlc_ml_sorting_criteria_t criteria) const
{
    return M_names_to_criteria.key(criteria, "");
}

void MLAlbumModel::onVlcMlEvent(const MLEvent &event)
{
    switch( event.i_type )
    {
        case VLC_ML_EVENT_ALBUM_ADDED:
        {
            emit resetRequested();
            return;
        }
        case VLC_ML_EVENT_ALBUM_DELETED:
        {
            MLItemId itemId(event.deletion.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            deleteItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_ALBUM_UPDATED:
        {
            MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            updateItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_ARTIST_DELETED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_ARTIST &&
                 event.deletion.i_entity_id == m_parent.id )
                    emit resetRequested();
            return;
        case VLC_ML_EVENT_GENRE_DELETED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_GENRE &&
                 event.deletion.i_entity_id == m_parent.id )
                    emit resetRequested();;
            return;
        default:
            break;
    }

    MLBaseModel::onVlcMlEvent( event );
}

vlc_ml_sorting_criteria_t MLAlbumModel::roleToCriteria(int role) const
{
    switch (role)
    {
    case ALBUM_TITLE :
        return VLC_ML_SORTING_ALPHA;
    case ALBUM_RELEASE_YEAR :
        return VLC_ML_SORTING_RELEASEDATE;
    case ALBUM_MAIN_ARTIST :
        return VLC_ML_SORTING_ARTIST;
    case ALBUM_DURATION:
        return VLC_ML_SORTING_DURATION;
    default:
        return VLC_ML_SORTING_DEFAULT;
    }
}

QVariant MLAlbumModel::itemRoleData(MLItem *item, const int role) const
{
    auto ml_item = static_cast<MLAlbum *>(item);
    assert(ml_item);

    switch (role)
    {
    case ALBUM_ID :
        return QVariant::fromValue( ml_item->getId() );
    case ALBUM_TITLE :
        return QVariant::fromValue( ml_item->getTitle() );
    case ALBUM_RELEASE_YEAR :
        return QVariant::fromValue( ml_item->getReleaseYear() );
    case ALBUM_SHORT_SUMMARY :
        return QVariant::fromValue( ml_item->getShortSummary() );
    case ALBUM_COVER :
        return QVariant::fromValue( ml_item->getCover() );
    case ALBUM_MAIN_ARTIST :
        return QVariant::fromValue( ml_item->getArtist() );
    case ALBUM_NB_TRACKS :
        return QVariant::fromValue( ml_item->getNbTracks() );
    case ALBUM_DURATION:
        return QVariant::fromValue( ml_item->getDuration() );
    case ALBUM_TITLE_FIRST_SYMBOL:
        return QVariant::fromValue( getFirstSymbol( ml_item->getTitle() ) );
    case ALBUM_MAIN_ARTIST_FIRST_SYMBOL:
        return QVariant::fromValue( getFirstSymbol( ml_item->getArtist() ) );
    default:
        return QVariant();
    }
}

std::unique_ptr<MLBaseModel::BaseLoader>
MLAlbumModel::createLoader() const
{
    return std::make_unique<Loader>(*this);
}

size_t MLAlbumModel::Loader::count(vlc_medialibrary_t* ml) const
{
    MLQueryParams params = getParams();
    auto queryParams = params.toCQueryParams();

    if ( m_parent.id <= 0 )
        return vlc_ml_count_albums(ml, &queryParams);
    return vlc_ml_count_albums_of(ml, &queryParams, m_parent.type, m_parent.id);
}

std::vector<std::unique_ptr<MLItem>>
MLAlbumModel::Loader::load(vlc_medialibrary_t* ml, size_t index, size_t count) const
{
    MLQueryParams params = getParams(index, count);
    auto queryParams = params.toCQueryParams();

    ml_unique_ptr<vlc_ml_album_list_t> album_list;
    if ( m_parent.id <= 0 )
        album_list.reset( vlc_ml_list_albums(ml, &queryParams) );
    else
        album_list.reset( vlc_ml_list_albums_of(ml, &queryParams, m_parent.type, m_parent.id ) );
    if ( album_list == nullptr )
        return {};
    std::vector<std::unique_ptr<MLItem>> res;
    for( const vlc_ml_album_t& album: ml_range_iterate<vlc_ml_album_t>( album_list ) )
        res.emplace_back( std::make_unique<MLAlbum>( &album ) );
    return res;
}

std::unique_ptr<MLItem>
MLAlbumModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_ALBUM);
    ml_unique_ptr<vlc_ml_album_t> album(vlc_ml_get_album(ml, itemId.id));
    if (!album)
        return nullptr;
    return std::make_unique<MLAlbum>(album.get());
}
