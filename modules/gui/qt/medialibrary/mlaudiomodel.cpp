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

#include "mlaudiomodel.hpp"

MLAudioModel::MLAudioModel(QObject *parent)
    : MLMediaModel(parent)
{
}

QVariant MLAudioModel::itemRoleData(const MLItem *item, const int role) const
{
    const MLAudio* audio = static_cast<const MLAudio *>(item);
    assert( audio );

    switch (role)
    {
    case AUDIO_COVER: // TODO: remove (a similar role already MEDIA_SMALL_COVER)
        return MLMediaModel::itemRoleData(item, MLMediaModel::MEDIA_SMALL_COVER);
    case AUDIO_TRACK_NUMBER:
        return QVariant::fromValue(audio->getTrackNumber());
    case AUDIO_DISC_NUMBER:
        return QVariant::fromValue(audio->getDiscNumber());
    case AUDIO_ARTIST:
        return QVariant::fromValue(audio->getArtist());
    case AUDIO_ARTIST_FIRST_SYMBOL:
        return QVariant::fromValue(getFirstSymbol(audio->getArtist()));
    case AUDIO_ALBUM:
        return QVariant::fromValue(audio->getAlbumTitle());
    case AUDIO_ALBUM_FIRST_SYMBOL:
        return QVariant::fromValue(getFirstSymbol(audio->getAlbumTitle()));
    default:
        return MLMediaModel::itemRoleData(item, role);
    }

    return {};
}

QHash<int, QByteArray> MLAudioModel::roleNames() const
{
    QHash<int, QByteArray> hash = MLMediaModel::roleNames();

    hash.insert({
        {AUDIO_COVER, "cover"}, // TODO: remove (a similar roleName already "small_cover")
        {AUDIO_TRACK_NUMBER, "track_number"},
        {AUDIO_DISC_NUMBER, "disc_number"},
        {AUDIO_ARTIST, "main_artist"},
        {AUDIO_ARTIST_FIRST_SYMBOL, "main_artist_first_symbol"},
        {AUDIO_ALBUM, "album_title"},
        {AUDIO_ALBUM_FIRST_SYMBOL, "album_title_first_symbol"},
    });

    return hash;
}

vlc_ml_sorting_criteria_t MLAudioModel::nameToCriteria(QByteArray name) const
{
    return QHash<QByteArray, vlc_ml_sorting_criteria_t> {
        {"id", VLC_ML_SORTING_DEFAULT},
        {"title", VLC_ML_SORTING_ALPHA},
        {"album_title", VLC_ML_SORTING_ALBUM},
        {"track_number", VLC_ML_SORTING_TRACKNUMBER},
        {"release_year", VLC_ML_SORTING_RELEASEDATE},
        {"main_artist", VLC_ML_SORTING_ARTIST},
        {"duration", VLC_ML_SORTING_DURATION},
    }.value(name, VLC_ML_SORTING_DEFAULT);
}

void MLAudioModel::onVlcMlEvent(const MLEvent &event)
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_MEDIA_ADDED:
            if ( event.creation.media.i_subtype == VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK )
                emit resetRequested();
            return;
        case VLC_ML_EVENT_MEDIA_UPDATED:
        {
            MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            updateItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_MEDIA_DELETED:
        {
            MLItemId itemId(event.deletion.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            deleteItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_ALBUM_UPDATED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_ALBUM &&
                 m_parent.id == event.modification.i_entity_id )
                emit resetRequested();
            return;
        case VLC_ML_EVENT_ALBUM_DELETED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_ALBUM &&
                 m_parent.id == event.deletion.i_entity_id )
                emit resetRequested();
            return;
        case VLC_ML_EVENT_GENRE_DELETED:
            if ( m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_GENRE &&
                 m_parent.id == event.deletion.i_entity_id )
                emit resetRequested();
            return;
    }

    MLBaseModel::onVlcMlEvent( event );
}

std::unique_ptr<MLListCacheLoader>
MLAudioModel::createMLLoader() const
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLAudioModel::Loader>(*this));
}

size_t MLAudioModel::Loader::count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const
{
    if ( m_parent.id <= 0 )
        return vlc_ml_count_audio_media(ml, queryParams);
    return vlc_ml_count_media_of(ml, queryParams, m_parent.type, m_parent.id );
}

std::vector<std::unique_ptr<MLItem>>
MLAudioModel::Loader::load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const
{
    ml_unique_ptr<vlc_ml_media_list_t> media_list;

    if ( m_parent.id <= 0 )
        media_list.reset( vlc_ml_list_audio_media(ml, queryParams) );
    else
        media_list.reset( vlc_ml_list_media_of(ml, queryParams, m_parent.type, m_parent.id ) );
    if ( media_list == nullptr )
        return {};
    std::vector<std::unique_ptr<MLItem>> res;
    for( const vlc_ml_media_t& media: ml_range_iterate<vlc_ml_media_t>( media_list ) )
        res.emplace_back( std::make_unique<MLAudio>( ml, &media ) );
    return res;
}

std::unique_ptr<MLItem>
MLAudioModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLAudio>(ml, media.get());
}
