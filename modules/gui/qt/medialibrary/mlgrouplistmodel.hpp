/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

#ifndef MLGROUPLISTMODEL_HPP
#define MLGROUPLISTMODEL_HPP

// MediaLibrary includes
#include "mlbasemodel.hpp"

// Forward declarations
class vlc_medialibrary_t;

class MLGroupListModel : public MLBaseModel
{
    Q_OBJECT

public:
    enum Roles
    {
        GROUP_ID = Qt::UserRole + 1,
        GROUP_NAME,
        GROUP_THUMBNAIL,
        GROUP_DURATION,
        GROUP_DATE,
        GROUP_COUNT,
        // NOTE: Media specific.
        GROUP_TITLE,
        GROUP_RESOLUTION,
        GROUP_CHANNEL,
        GROUP_MRL,
        GROUP_MRL_DISPLAY,
        GROUP_PROGRESS,
        GROUP_PLAYCOUNT,
        GROUP_VIDEO_TRACK,
        GROUP_AUDIO_TRACK,
        GROUP_TITLE_FIRST_SYMBOL
    };

public:
    explicit MLGroupListModel(vlc_medialibrary_t * ml, QObject * parent = nullptr);

    explicit MLGroupListModel(QObject * parent = nullptr);

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

    void thumbnailUpdated(int idx) override;

private:
    struct Loader : public MLBaseModel::BaseLoader
    {
        Loader(const MLGroupListModel & model);

        size_t count() const override;

        std::vector<std::unique_ptr<MLItem>> load(size_t index, size_t count) const override;
    };
};

#endif // MLGROUPLISTMODEL_HPP
