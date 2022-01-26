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
        MEDIA_IS_NEW,
        MEDIA_TITLE,
        MEDIA_THUMBNAIL,
        MEDIA_DURATION,
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

public: // Interface
    Q_INVOKABLE void insert(const QVariantList & items, int at);

    Q_INVOKABLE void move(const QModelIndexList & indexes, int to);

    Q_INVOKABLE void remove(const QModelIndexList & indexes);

public: // QAbstractItemModel implementation
    QHash<int, QByteArray> roleNames() const override;

protected: // MLBaseModel implementation
    QVariant itemRoleData(MLItem *item, int role = Qt::DisplayRole) const override;

    vlc_ml_sorting_criteria_t roleToCriteria(int role) const override;

    vlc_ml_sorting_criteria_t nameToCriteria(QByteArray name) const override;

    QByteArray criteriaToName(vlc_ml_sorting_criteria_t criteria) const override;

    std::unique_ptr<MLBaseModel::BaseLoader> createLoader() const override;

protected: // MLBaseModel reimplementation
    void onVlcMlEvent(const MLEvent & event) override;

    void thumbnailUpdated(const QModelIndex& idx, MLItem* item, const QString& mrl, vlc_ml_thumbnail_status_t status) override;

private: // Functions
    struct HighLowRanges {
        int lowTo;
        int highTo;
        std::vector<std::pair<int, int>> lowRanges;
        size_t lowRangeIt;
        std::vector<std::pair<int, int>> highRanges;
        size_t highRangeIt;
    };

    /**
     * returns list of row indexes in decreasing order
     */
    std::vector<std::pair<int, int>> getSortedRowsRanges(const QModelIndexList & indexes, bool asc) const;

    void removeImpl(int64_t playlistId, const std::vector<std::pair<int, int> >&& rangeList, size_t index);

    void moveImpl(int64_t playlistId, HighLowRanges&& ranges);

    void endTransaction();

    void generateThumbnail(const MLItemId& itemid) const;

    bool m_transactionPending = false;
    bool m_resetAfterTransaction = false;

private:
    struct Loader : public MLBaseModel::BaseLoader
    {
        Loader(const MLPlaylistModel & model);

        size_t count(vlc_medialibrary_t* ml) const override;

        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, size_t index, size_t count) const override;

        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;
    };
};

#endif // MLPLAYLISTMODEL_HPP
