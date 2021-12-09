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
struct vlc_medialibrary_t;
class MLGroup;

class MLGroupListModel : public MLBaseModel
{
    Q_OBJECT

public:
    enum Roles
    {
        GROUP_ID = Qt::UserRole + 1,
        GROUP_TITLE,
        GROUP_THUMBNAIL,
        GROUP_DURATION,
        GROUP_DATE,
        GROUP_COUNT,
        // NOTE: Media specific.
        GROUP_IS_NEW,
        GROUP_FILENAME,
        GROUP_PROGRESS,
        GROUP_PLAYCOUNT,
        GROUP_RESOLUTION,
        GROUP_CHANNEL,
        GROUP_MRL,
        GROUP_MRL_DISPLAY,
        GROUP_VIDEO_TRACK,
        GROUP_AUDIO_TRACK,

        GROUP_TITLE_FIRST_SYMBOL
    };

public:
    explicit MLGroupListModel(QObject * parent = nullptr);

public: // QAbstractItemModel implementation
    QHash<int, QByteArray> roleNames() const override;

protected: // MLBaseModel implementation
    QVariant itemRoleData(MLItem *item, int role = Qt::DisplayRole) const override;

    vlc_ml_sorting_criteria_t roleToCriteria(int role) const override;

    vlc_ml_sorting_criteria_t nameToCriteria(QByteArray name) const override;

    QByteArray criteriaToName(vlc_ml_sorting_criteria_t criteria) const override;

    ListCacheLoader<std::unique_ptr<MLItem>> * createLoader() const override;

    void thumbnailUpdated(const QModelIndex& idx, MLItem* mlitem, const QString& mrl, vlc_ml_thumbnail_status_t status) override;

private: // Functions
    QString getCover(MLGroup * group) const;

private: // MLBaseModel implementation
    void onVlcMlEvent(const MLEvent & event) override;

    void generateVideoThumbnail(uint64_t id) const;

private:
    struct Loader : public MLBaseModel::BaseLoader
    {
        Loader(const MLGroupListModel & model);

        size_t count(vlc_medialibrary_t* ml) const override;

        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, size_t index, size_t count) const override;
    };
};

#endif // MLGROUPLISTMODEL_HPP
