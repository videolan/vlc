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

QHash<QByteArray, vlc_ml_sorting_criteria_t> MLAlbumModel::M_names_to_criteria = {
    {"id", VLC_ML_SORTING_DEFAULT},
    {"title", VLC_ML_SORTING_ALBUM},
    {"release_year", VLC_ML_SORTING_RELEASEDATE},
    {"main_artist", VLC_ML_SORTING_ARTIST},
    //{"nb_tracks"},
    {"duration", VLC_ML_SORTING_DURATION}
};

MLAlbumModel::MLAlbumModel(QObject *parent)
    : MLSlidingWindowModel<MLAlbum>(parent)
{
}

QVariant MLAlbumModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0)
        return QVariant();

    const MLAlbum* ml_item = item(static_cast<unsigned int>(index.row()));
    if ( ml_item == NULL )
        return QVariant();

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
    case ALBUM_DURATION_SHORT:
        return QVariant::fromValue( ml_item->getDurationShort() );
    case ALBUM_TITLE_FIRST_SYMBOL:
        return QVariant::fromValue( getFirstSymbol( ml_item->getTitle() ) );
    case ALBUM_MAIN_ARTIST_FIRST_SYMBOL:
        return QVariant::fromValue( getFirstSymbol( ml_item->getArtist() ) );
    default:
        return QVariant();
    }
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
        {ALBUM_DURATION_SHORT, "durationShort"},
        {ALBUM_TITLE_FIRST_SYMBOL, "title_first_symbol"},
        {ALBUM_MAIN_ARTIST_FIRST_SYMBOL, "main_artist_first_symbol"}
    };
}

std::vector<std::unique_ptr<MLAlbum>> MLAlbumModel::fetch( )
{
    ml_unique_ptr<vlc_ml_album_list_t> album_list;
    if ( m_parent.id <= 0 )
        album_list.reset( vlc_ml_list_albums(m_ml, &m_query_param ) );
    else
        album_list.reset( vlc_ml_list_albums_of(m_ml, &m_query_param, m_parent.type, m_parent.id ) );
    if ( album_list == nullptr )
        return {};
    std::vector<std::unique_ptr<MLAlbum>> res;
    for( const vlc_ml_album_t& album: ml_range_iterate<vlc_ml_album_t>( album_list ) )
        res.emplace_back( std::make_unique<MLAlbum>( m_ml, &album ) );
    return res;
}

vlc_ml_sorting_criteria_t MLAlbumModel::nameToCriteria(QByteArray name) const
{
    return M_names_to_criteria.value(name, VLC_ML_SORTING_DEFAULT);
}

QByteArray MLAlbumModel::criteriaToName(vlc_ml_sorting_criteria_t criteria) const
{
    return M_names_to_criteria.key(criteria, "");
}

void MLAlbumModel::onVlcMlEvent(const vlc_ml_event_t* event)
{
    switch( event->i_type )
    {
        case VLC_ML_EVENT_ALBUM_ADDED:
        case VLC_ML_EVENT_ALBUM_DELETED:
        case VLC_ML_EVENT_ALBUM_UPDATED:
            m_need_reset = true;
            break;
        case VLC_ML_EVENT_ARTIST_DELETED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_ARTIST &&
                 event->deletion.i_entity_id == m_parent.id )
                    m_need_reset = true;
            break;
        case VLC_ML_EVENT_GENRE_DELETED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_GENRE &&
                 event->deletion.i_entity_id == m_parent.id )
                    m_need_reset = true;
            break;
        default:
            break;
    }
    MLSlidingWindowModel::onVlcMlEvent( event );
}

void MLAlbumModel::thumbnailUpdated(int idx)
{
    emit dataChanged(index(idx), index(idx), {ALBUM_COVER});
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
    case ALBUM_DURATION_SHORT:
        return VLC_ML_SORTING_DURATION;
    default:
        return VLC_ML_SORTING_DEFAULT;
    }
}

size_t MLAlbumModel::countTotalElements() const
{
    auto queryParams = m_query_param;
    queryParams.i_offset = 0;
    queryParams.i_nbResults = 0;
    if ( m_parent.id <= 0 )
        return vlc_ml_count_albums(m_ml, &queryParams);
    return vlc_ml_count_albums_of(m_ml, &queryParams, m_parent.type, m_parent.id);
}
