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

#ifndef MLAUDIOMODEL_HPP
#define MLAUDIOMODEL_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mlmediamodel.hpp"

class MLAudioModel : public MLMediaModel
{
    Q_OBJECT

public:
    enum Roles
    {
        AUDIO_COVER = MLMediaModel::MEDIA_ROLES_COUNT, // TODO: remove (a similar role already MEDIA_SMALL_COVER)
        AUDIO_TRACK_NUMBER,
        AUDIO_DISC_NUMBER,
        AUDIO_ARTIST,
        AUDIO_ARTIST_FIRST_SYMBOL,
        AUDIO_ALBUM,
        AUDIO_ALBUM_FIRST_SYMBOL,
    };

public:
    explicit MLAudioModel(QObject *parent = nullptr);

    virtual ~MLAudioModel() = default;

protected:
    QHash<int, QByteArray> roleNames() const override;

    QVariant itemRoleData(const MLItem *item, int role) const override;

    std::unique_ptr<MLListCacheLoader> createMLLoader() const override;

private:
    vlc_ml_sorting_criteria_t nameToCriteria(QByteArray name) const override;
    void onVlcMlEvent( const MLEvent &event ) override;

    struct Loader : public MLListCacheLoader::MLOp
    {
        using MLListCacheLoader::MLOp::MLOp;
        size_t count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParam) const override;
        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParam) const override;
        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;
    };
};

#endif // MLAUDIOMODEL_HPP
