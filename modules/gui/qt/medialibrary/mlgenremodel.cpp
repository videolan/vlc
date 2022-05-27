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

#include "mlgenremodel.hpp"

// Util includes
#include "util/covergenerator.hpp"

// MediaLibrary includes
#include "mlcustomcover.hpp"
#include "mlartistmodel.hpp"

//-------------------------------------------------------------------------------------------------
// Static variables

// NOTE: We multiply by 3 to cover most dpi settings.
static const int MLGENREMODEL_COVER_WIDTH  = 260 * 3;
static const int MLGENREMODEL_COVER_HEIGHT = 130 * 3;

static const int MLGENREMODEL_COVER_COUNTX = 4;
static const int MLGENREMODEL_COVER_COUNTY = 2;

static const int MLGENREMODEL_COVER_BLUR = 4;

//-------------------------------------------------------------------------------------------------

QHash<QByteArray, vlc_ml_sorting_criteria_t> MLGenreModel::M_names_to_criteria = {
    {"title", VLC_ML_SORTING_ALPHA}
};

MLGenreModel::MLGenreModel(QObject *parent)
    : MLBaseModel(parent)
{
}

QVariant MLGenreModel::itemRoleData(MLItem *item, const int role) const
{
    MLGenre* ml_genre = static_cast<MLGenre *>(item);
    if (!ml_genre)
        return QVariant();

    switch (role)
    {
        // Genres
    case GENRE_ID:
        return QVariant::fromValue( ml_genre->getId() );
    case GENRE_NAME:
        return QVariant::fromValue( ml_genre->getName() );
    case GENRE_NB_TRACKS:
        return QVariant::fromValue( ml_genre->getNbTracks() );
    case GENRE_COVER:
        return getCover(ml_genre);
    default :
        return QVariant();
    }
}

QHash<int, QByteArray> MLGenreModel::roleNames() const
{
    return {
        { GENRE_ID, "id" },
        { GENRE_NAME, "name" },
        { GENRE_NB_TRACKS, "nb_tracks" },
        { GENRE_ARTISTS, "artists" },
        { GENRE_TRACKS, "tracks" },
        { GENRE_ALBUMS, "albums" },
        { GENRE_COVER, "cover" }
    };
}


QString MLGenreModel::getCoverDefault() const
{
    return m_coverDefault;
}

void MLGenreModel::setCoverDefault(const QString& defaultCover)
{
    if (m_coverDefault == defaultCover)
        return;
    m_coverDefault = defaultCover;
    emit coverDefaultChanged();
}

void MLGenreModel::onVlcMlEvent(const MLEvent &event)
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_GENRE_ADDED:
            emit resetRequested();
            return;
        case VLC_ML_EVENT_GENRE_UPDATED:
        {
            MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            updateItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_GENRE_DELETED:
        {
            MLItemId itemId(event.deletion.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            deleteItemInCache(itemId);
            return;
        }
    }

    MLBaseModel::onVlcMlEvent(event);
}

vlc_ml_sorting_criteria_t MLGenreModel::roleToCriteria(int role) const
{
    switch (role)
    {
    case GENRE_NAME:
        return VLC_ML_SORTING_ALPHA;
    default :
        return VLC_ML_SORTING_DEFAULT;
    }
}

vlc_ml_sorting_criteria_t MLGenreModel::nameToCriteria(QByteArray name) const
{
    return M_names_to_criteria.value(name, VLC_ML_SORTING_DEFAULT);
}

QString MLGenreModel::getCover(MLGenre * genre) const
{
    return ml()->customCover()->get(genre->getId()
                                    , QSize(MLGENREMODEL_COVER_WIDTH, MLGENREMODEL_COVER_HEIGHT)
                                    , m_coverDefault
                                    , MLGENREMODEL_COVER_COUNTX
                                    , MLGENREMODEL_COVER_COUNTY
                                    , MLGENREMODEL_COVER_BLUR
                                    , true);
}

//-------------------------------------------------------------------------------------------------

std::unique_ptr<MLBaseModel::BaseLoader>
MLGenreModel::createLoader() const
{
    return std::make_unique<Loader>(*this);
}

size_t MLGenreModel::Loader::count(vlc_medialibrary_t* ml) const
{
    MLQueryParams params = getParams();
    auto queryParams = params.toCQueryParams();

    return vlc_ml_count_genres( ml, &queryParams );
}

std::vector<std::unique_ptr<MLItem>>
MLGenreModel::Loader::load(vlc_medialibrary_t* ml, size_t index, size_t count) const
{
    MLQueryParams params = getParams(index, count);
    auto queryParams = params.toCQueryParams();

    ml_unique_ptr<vlc_ml_genre_list_t> genre_list(
        vlc_ml_list_genres(ml, &queryParams)
    );
    if ( genre_list == nullptr )
        return {};
    std::vector<std::unique_ptr<MLItem>> res;
    for( const vlc_ml_genre_t& genre: ml_range_iterate<vlc_ml_genre_t>( genre_list ) )
        res.emplace_back( std::make_unique<MLGenre>( &genre ) );
    return res;

}

std::unique_ptr<MLItem>
MLGenreModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_GENRE);
    ml_unique_ptr<vlc_ml_genre_t> genre(vlc_ml_get_genre(ml, itemId.id));
    if (!genre)
        return nullptr;
    return std::make_unique<MLGenre>(genre.get());
}
