/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef MLPLAYLISTMODEL_HPP
#define MLPLAYLISTMODEL_HPP

// MediaLibrary includes
#include "mlbasemodel.hpp"

class MLPlaylistModel : public MLBaseModel
{
    Q_OBJECT

public:
    enum Role
    {
        MEDIA_ID = Qt::UserRole + 1,
        MEDIA_TITLE,
        MEDIA_THUMBNAIL,
        MEDIA_DURATION,
        MEDIA_DURATION_SHORT,
        MEDIA_PROGRESS,
        MEDIA_PLAYCOUNT,
        MEDIA_RESOLUTION,
        MEDIA_CHANNEL,
        MEDIA_MRL,
        MEDIA_DISPLAY_MRL,
        MEDIA_VIDEO_TRACK,
        MEDIA_AUDIO_TRACK,
        MEDIA_TITLE_FIRST_SYMBOL,
    };

public:
    explicit MLPlaylistModel(QObject * parent = nullptr);

public: // QAbstractItemModel implementation
    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

protected: // MLBaseModel implementation    
    vlc_ml_sorting_criteria_t roleToCriteria(int role) const override;

    vlc_ml_sorting_criteria_t nameToCriteria(QByteArray name) const override;

    QByteArray criteriaToName(vlc_ml_sorting_criteria_t criteria) const override;

    ListCacheLoader<std::unique_ptr<MLItem>> * createLoader() const override;

private: // MLBaseModel implementation
    void onVlcMlEvent(const MLEvent & event) override;

private:
    struct Loader : public MLBaseModel::BaseLoader
    {
        Loader(const MLPlaylistModel & model);

        size_t count() const override;

        std::vector<std::unique_ptr<MLItem>> load(size_t index, size_t count) const override;
    };
};

#endif // MLPLAYLISTMODEL_HPP
