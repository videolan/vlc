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

#ifndef MLVIDEOMODEL_H
#define MLVIDEOMODEL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mlmediamodel.hpp"

class MLVideoModel : public MLMediaModel
{
    Q_OBJECT

public:
    enum Roles
    {
        VIDEO_THUMBNAIL = MLMediaModel::MEDIA_ROLES_COUNT, // TODO: remove (a similar role already MEDIA_SMALL_COVER)
        VIDEO_IS_NEW,
        VIDEO_RESOLUTION,
        VIDEO_CHANNEL,
        VIDEO_DISPLAY_MRL,
        VIDEO_VIDEO_TRACK,
        VIDEO_AUDIO_TRACK,
        VIDEO_SUBTITLE_TRACK,
        VIDEO_ROLES_COUNT,
    };

public:
    explicit MLVideoModel(QObject* parent = nullptr);

    virtual ~MLVideoModel() = default;

public: // Interface
    // NOTE: This function is useful when we want to apply a change before the database event.
    Q_INVOKABLE void setItemPlayed(const QModelIndex & index, bool played);

protected:
    QHash<int, QByteArray> roleNames() const override;

    QVariant itemRoleData(const MLItem *item, int role) const override;

    void thumbnailUpdated(const QModelIndex& , MLItem* , const QString& , vlc_ml_thumbnail_status_t ) override;

    std::unique_ptr<MLListCacheLoader> createMLLoader() const override;

protected: // MLBaseModel reimplementation
    void onVlcMlEvent( const MLEvent &event ) override;

private:
    void generateThumbnail(uint64_t id) const;

    vlc_ml_sorting_criteria_t nameToCriteria(QByteArray name) const override;

    struct Loader : public MLListCacheLoader::MLOp
    {
        using MLListCacheLoader::MLOp::MLOp;

        size_t count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override;
        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override;
        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;
    };
};

#endif // MLVIDEOMODEL_H
