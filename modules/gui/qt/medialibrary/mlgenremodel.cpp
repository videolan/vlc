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
#include "mlartistmodel.hpp"

//-------------------------------------------------------------------------------------------------
// Static variables

// NOTE: We multiply by 2 to cover most dpi settings.
static const int MLGENREMODEL_COVER_WIDTH  = 260 * 2;
static const int MLGENREMODEL_COVER_HEIGHT = 130 * 2;

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

QVariant MLGenreModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();

    if (!index.isValid() || row < 0)
        return QVariant();

    MLGenre* ml_genre = static_cast<MLGenre *>(item(row));
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
        return getCover(ml_genre, row);
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

void MLGenreModel::onVlcMlEvent(const MLEvent &event)
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_GENRE_ADDED:
        case VLC_ML_EVENT_GENRE_UPDATED:
        case VLC_ML_EVENT_GENRE_DELETED:
            m_need_reset = true;
            break;
    }
    MLBaseModel::onVlcMlEvent(event);
}

void MLGenreModel::thumbnailUpdated(int idx)
{
    emit dataChanged(index(idx), index(idx), {GENRE_COVER});
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

QString MLGenreModel::getCover(MLGenre * genre, int index) const
{
    QString cover = genre->getCover();

    // NOTE: Making sure we're not already generating a cover.
    if (cover.isNull() == false || genre->hasGenerator())
        return cover;

    CoverGenerator * generator = new CoverGenerator(m_ml, genre->getId(), index);

    generator->setSize(QSize(MLGENREMODEL_COVER_WIDTH,
                             MLGENREMODEL_COVER_HEIGHT));

    generator->setCountX(MLGENREMODEL_COVER_COUNTX);
    generator->setCountY(MLGENREMODEL_COVER_COUNTY);

    generator->setSplit(CoverGenerator::Duplicate);

    generator->setBlur(MLGENREMODEL_COVER_BLUR);

    generator->setDefaultThumbnail(":/noart_album.svg");

    // NOTE: We'll apply the new cover once it's loaded.
    connect(generator, &CoverGenerator::result, this, &MLGenreModel::onCover);

    generator->start(*QThreadPool::globalInstance());

    genre->setGenerator(generator);

    return cover;
}

//-------------------------------------------------------------------------------------------------
// Private slots
//-------------------------------------------------------------------------------------------------

void MLGenreModel::onCover()
{
    CoverGenerator * generator = static_cast<CoverGenerator *> (sender());

    int index = generator->getIndex();

    // NOTE: We want to avoid calling 'MLBaseModel::item' for performance issues.
    MLItem * item = this->itemCache(index);

    // NOTE: When the item is no longer cached or has been moved we return right away.
    if (item == nullptr || item->getId() != generator->getId())
    {
        generator->deleteLater();

        return;
    }

    MLGenre * genre = static_cast<MLGenre *> (item);

    QString fileName = QUrl::fromLocalFile(generator->takeResult()).toString();

    genre->setCover(fileName);

    genre->setGenerator(nullptr);

    thumbnailUpdated(index);
}

//-------------------------------------------------------------------------------------------------

ListCacheLoader<std::unique_ptr<MLItem>> *
MLGenreModel::createLoader() const
{
    return new Loader(*this);
}

size_t MLGenreModel::Loader::count() const
{
    MLQueryParams params = getParams();
    auto queryParams = params.toCQueryParams();

    return vlc_ml_count_genres( m_ml, &queryParams );
}

std::vector<std::unique_ptr<MLItem>>
MLGenreModel::Loader::load(size_t index, size_t count) const
{
    MLQueryParams params = getParams(index, count);
    auto queryParams = params.toCQueryParams();

    ml_unique_ptr<vlc_ml_genre_list_t> genre_list(
        vlc_ml_list_genres(m_ml, &queryParams)
    );
    if ( genre_list == nullptr )
        return {};
    std::vector<std::unique_ptr<MLItem>> res;
    for( const vlc_ml_genre_t& genre: ml_range_iterate<vlc_ml_genre_t>( genre_list ) )
        res.emplace_back( std::make_unique<MLGenre>( m_ml, &genre ) );
    return res;

}
